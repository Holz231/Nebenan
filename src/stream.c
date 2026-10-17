// SPDX-License-Identifier: MIT

#include "world.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

// Where a site is: out of the world, prepared by the workers, in the world, or out of it with its rubble
enum nbSiteState
{
	nb_siteOut = 0,
	nb_siteLoading,
	nb_siteIn,
	nb_siteSaved,
};

typedef struct nbSite
{
	// The bounds as built, and the bounds in the tree of the stream, grown to the rubble while the site is saved
	b3AABB bounds;
	b3AABB reach;
	nbDestructibleId id;

	// When a moving body last came near, and when to ask again whether the site can go, in seconds of the world
	double activeTime;
	double nextCheck;

	// The bytes of a saved site, null when the store of the program keeps them
	uint8_t* saved;
	size_t savedSize;

	int proxyId;

	// The update that last took it as a candidate, so several viewers take it once
	uint32_t pickStamp;

	uint8_t state;

	// A moving body asked for it since the last update
	bool urgent;
} nbSite;

typedef struct nbCandidate
{
	// Squared distance to the nearest viewer, -1 for a site a moving body asked for
	float distance;
	int site;
} nbCandidate;

NB_ARRAY_DECLARE( nbSite, nbSiteArray );
NB_ARRAY_DECLARE( nbCandidate, nbCandidateArray );

// A cell of the bodies that moved, with the first and the last of its bodies in the order they moved, see nbMarkStream
typedef struct nbMarkCell
{
	int64_t key;
	int first;
	int last;
} nbMarkCell;

// A site that the bodies of a cell may reach, with its bounds as the tree holds them
typedef struct nbReachedSite
{
	b3AABB reach;
	int site;
} nbReachedSite;

NB_ARRAY_DECLARE( nbMarkCell, nbMarkCellArray );
NB_ARRAY_DECLARE( nbReachedSite, nbReachedSiteArray );
NB_ARRAY_DECLARE( nbDestructibleDef, nbDestructibleDefArray );
NB_ARRAY_DECLARE( const nbPieceDef*, nbPieceListArray );
NB_ARRAY_DECLARE( nbDestructibleId, nbDestructibleIdArray );

// Sites the workers prepare together, with what nbStartCreating needs until they are built in
typedef struct nbStreamBatch
{
	nbCreationId creation;
	nbIntArray sites;
	nbDestructibleDefArray defs;
	nbPieceListArray pieceLists;
	nbIntArray pieceCounts;
	nbDestructibleIdArray ids;
} nbStreamBatch;

typedef struct nbStream
{
	nbStreamDef def;
	nbWorld* world;
	nbStreamId id;

	nbSiteArray sites;

	// The reach of every site, to find the sites near a moving body or a viewer
	b3DynamicTree tree;

	// Sites in the world, and sites a moving body asked for since the last update
	nbIntArray loaded;
	nbIntArray urgent;

	// One batch per update of the load delay, the next one to build in and start again
	nbStreamBatch* batches;
	int batchIndex;

	// The sites out of the world near the viewers at the last search, and where the viewers stood, see nbPickSites
	nbIntArray nearby;
	nbVec3Array nearViewers;
	b3Vec3 nearUp;
	bool nearValid;

	// The marking by cells of moving bodies: a hash table of the cells, the cells in the order they came, the next body
	// in the same cell, and the sites near the bodies of a cell, see nbMarkStream
	nbMarkCellArray markCells;
	nbIntArray markOrder;
	nbIntArray markNext;
	nbReachedSiteArray reached;

	nbCandidateArray candidates;
	nbChunkIdArray chunks;
	uint32_t pickStamp;

	nbStreamStats stats;
} nbStream;

// An update of nbStream_Update with its own copy of the viewers, run after the steps
typedef struct nbStreamJob
{
	nbStream* stream;
	size_t size;
	int viewerCount;
	b3Vec3 viewers[];
} nbStreamJob;

nbStreamDef nbDefaultStreamDef( void )
{
	nbStreamDef def = { 0 };
	def.loadRadius = 100.0f;
	def.unloadMargin = 20.0f;
	def.activeLoadRadius = 10.0f;
	def.activeHoldRadius = 20.0f;
	def.holdTime = 5.0f;
	def.retryTime = 0.5f;
	def.margin = 2.0f;
	def.loadBudget = 2;
	def.unloadBudget = 4;
	def.saveBudget = 1;
	def.loadDelay = 1;
	def.internalValue = NB_SECRET_COOKIE;
	return def;
}

static nbWorldId nbStreamWorldId( const nbStream* stream )
{
	return (nbWorldId){ (uint16_t)( stream->world->worldIndex + 1 ), stream->world->generation };
}

// The stream of an id, waiting for the steps if the calling thread started them like every lookup of the world, or
// without waiting
static nbStream* nbFindStream( nbStreamId id, bool wait )
{
	if ( id.index1 < 1 || id.world0 >= NB_MAX_WORLDS )
	{
		return NULL;
	}

	nbWorld* world = wait ? nbGetWorld( id.world0 ) : nbPeekWorld( id.world0 );
	if ( world->inUse == false || id.index1 > world->streams.count )
	{
		return NULL;
	}

	nbStreamSlot* slot = world->streams.data + ( id.index1 - 1 );
	return slot->stream != NULL && slot->generation == id.generation ? slot->stream : NULL;
}

static float nbSquaredBoxDistance( b3AABB box, b3Vec3 point )
{
	float dx = b3MaxFloat( b3MaxFloat( box.lowerBound.x - point.x, point.x - box.upperBound.x ), 0.0f );
	float dy = b3MaxFloat( b3MaxFloat( box.lowerBound.y - point.y, point.y - box.upperBound.y ), 0.0f );
	float dz = b3MaxFloat( b3MaxFloat( box.lowerBound.z - point.z, point.z - box.upperBound.z ), 0.0f );
	return dx * dx + dy * dy + dz * dz;
}

static b3Vec3 nbBoxCenter( b3AABB box )
{
	return b3MulSV( 0.5f, b3Add( box.lowerBound, box.upperBound ) );
}

static b3AABB nbBoxAround( b3Vec3 point, float radius )
{
	b3Vec3 r = { radius, radius, radius };
	return (b3AABB){ b3Sub( point, r ), b3Add( point, r ) };
}

// Up against gravity, zero without gravity. The viewers measure along the ground, so a camera high above a city loads
// the same houses as one in the street.
static b3Vec3 nbStreamUp( const nbStream* stream )
{
	b3Vec3 gravity = b3World_GetGravity( stream->world->physicsWorld );
	float length = b3Length( gravity );
	return length > 0.0f ? b3MulSV( -1.0f / length, gravity ) : b3Vec3_zero;
}

// The viewers measure along the ground, far up or down along up counts nothing
#define NB_STREAM_HEIGHT 1.0e5f

static b3AABB nbViewerBox( b3Vec3 viewer, float radius, b3Vec3 up )
{
	b3Vec3 r = { radius + NB_STREAM_HEIGHT * b3AbsFloat( up.x ), radius + NB_STREAM_HEIGHT * b3AbsFloat( up.y ),
				 radius + NB_STREAM_HEIGHT * b3AbsFloat( up.z ) };
	return (b3AABB){ b3Sub( viewer, r ), b3Add( viewer, r ) };
}

// Squared distance along the ground from the center of a site as built to the nearest viewer
static float nbViewerDistance( const nbSite* site, const b3Vec3* viewers, int viewerCount, b3Vec3 up )
{
	b3Vec3 center = nbBoxCenter( site->bounds );
	float best = FLT_MAX;
	for ( int i = 0; i < viewerCount; ++i )
	{
		b3Vec3 offset = b3Sub( center, viewers[i] );
		offset = b3MulSub( offset, b3Dot( offset, up ), up );
		best = b3MinFloat( best, b3LengthSquared( offset ) );
	}
	return best;
}

nbStreamId nbCreateStream( nbWorldId worldId, const nbStreamDef* def )
{
	NB_ASSERT( def->internalValue == NB_SECRET_COOKIE );
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL || def->prepareFcn == NULL || ( def->storeFcn == NULL ) != ( def->fetchFcn == NULL ) )
	{
		return nb_nullStreamId;
	}

	int index = NB_NULL_INDEX;
	for ( int i = 0; i < world->streams.count; ++i )
	{
		if ( world->streams.data[i].stream == NULL )
		{
			index = i;
			break;
		}
	}

	if ( index == NB_NULL_INDEX )
	{
		index = world->streams.count;
		nbArray_Push( world->streams, ( (nbStreamSlot){ NULL, 0 } ) );
	}

	nbStream* stream = nbAlloc( sizeof( nbStream ) );
	memset( stream, 0, sizeof( nbStream ) );
	stream->def = *def;
	stream->def.loadDelay = def->loadDelay < 1 ? 1 : def->loadDelay;
	stream->world = world;
	stream->tree = b3DynamicTree_Create( 1024 );
	stream->batches = nbAlloc( sizeof( nbStreamBatch ) * (size_t)stream->def.loadDelay );
	memset( stream->batches, 0, sizeof( nbStreamBatch ) * (size_t)stream->def.loadDelay );

	nbStreamSlot* slot = world->streams.data + index;
	slot->stream = stream;
	slot->generation += 1;
	stream->id = (nbStreamId){ index + 1, world->worldIndex, slot->generation };
	return stream->id;
}

static void nbReleaseSite( nbStream* stream, int site )
{
	if ( stream->def.releaseFcn != NULL )
	{
		stream->def.releaseFcn( site, stream->def.userContext );
	}
}

// Free a stream. A batch in the background is built in first, unless its world goes.
static void nbFreeStream( nbStream* stream, bool worldGoes )
{
	for ( int b = 0; b < stream->def.loadDelay; ++b )
	{
		nbStreamBatch* batch = stream->batches + b;
		if ( batch->sites.count > 0 && worldGoes == false )
		{
			nbFinishCreating( batch->creation, batch->ids.data );
			for ( int k = 0; k < batch->sites.count; ++k )
			{
				nbSite* site = stream->sites.data + batch->sites.data[k];
				site->id = batch->ids.data[k];
				site->state = nb_siteIn;
			}
		}

		for ( int k = 0; k < batch->sites.count; ++k )
		{
			nbReleaseSite( stream, batch->sites.data[k] );
		}

		nbArray_Free( batch->sites );
		nbArray_Free( batch->defs );
		nbArray_Free( batch->pieceLists );
		nbArray_Free( batch->pieceCounts );
		nbArray_Free( batch->ids );
	}

	for ( int i = 0; i < stream->sites.count; ++i )
	{
		nbSite* site = stream->sites.data + i;
		if ( site->saved != NULL )
		{
			nbFree( site->saved, site->savedSize );
		}
	}

	b3DynamicTree_Destroy( &stream->tree );
	nbFree( stream->batches, sizeof( nbStreamBatch ) * (size_t)stream->def.loadDelay );
	nbArray_Free( stream->sites );
	nbArray_Free( stream->loaded );
	nbArray_Free( stream->urgent );
	nbArray_Free( stream->nearby );
	nbArray_Free( stream->nearViewers );
	nbArray_Free( stream->markCells );
	nbArray_Free( stream->markOrder );
	nbArray_Free( stream->markNext );
	nbArray_Free( stream->reached );
	nbArray_Free( stream->candidates );
	nbArray_Free( stream->chunks );
	nbFree( stream, sizeof( nbStream ) );
}

void nbDestroyStream( nbStreamId streamId )
{
	nbStream* stream = nbFindStream( streamId, true );
	if ( stream == NULL )
	{
		return;
	}

	nbStreamSlot* slot = stream->world->streams.data + ( streamId.index1 - 1 );
	nbFreeStream( stream, false );
	slot->stream = NULL;
}

void nbDestroyStreams( nbWorld* world )
{
	for ( int i = 0; i < world->streams.count; ++i )
	{
		if ( world->streams.data[i].stream != NULL )
		{
			nbFreeStream( world->streams.data[i].stream, true );
			world->streams.data[i].stream = NULL;
		}
	}
}

int nbStream_AddSite( nbStreamId streamId, b3AABB bounds )
{
	nbStream* stream = nbFindStream( streamId, true );
	if ( stream == NULL )
	{
		return NB_NULL_INDEX;
	}

	int index = stream->sites.count;
	nbSite site = { 0 };
	site.bounds = bounds;
	site.reach = bounds;
	site.activeTime = -1.0e30;
	site.proxyId = b3DynamicTree_CreateProxy( &stream->tree, bounds, B3_DEFAULT_CATEGORY_BITS, (uint64_t)index );
	nbArray_Push( stream->sites, site );
	stream->stats.siteCount += 1;
	stream->nearValid = false;
	return index;
}

void nbStream_SetLoadRadius( nbStreamId streamId, float radius )
{
	nbStream* stream = nbFindStream( streamId, true );
	if ( stream != NULL )
	{
		stream->def.loadRadius = b3MaxFloat( radius, 0.0f );
		stream->nearValid = false;
	}
}

nbDestructibleId nbStream_GetDestructible( nbStreamId streamId, int site )
{
	nbStream* stream = nbFindStream( streamId, true );
	if ( stream == NULL || site < 0 || site >= stream->sites.count || stream->sites.data[site].state != nb_siteIn )
	{
		return nb_nullDestructibleId;
	}
	return stream->sites.data[site].id;
}

nbStreamStats nbStream_GetStats( nbStreamId streamId )
{
	nbStream* stream = nbFindStream( streamId, true );
	if ( stream == NULL )
	{
		return (nbStreamStats){ 0 };
	}

	nbStreamStats stats = stream->stats;
	stats.loadedCount = stream->loaded.count;
	stats.damagedCount = 0;
	for ( int k = 0; k < stream->loaded.count; ++k )
	{
		stats.damagedCount += nbDestructible_IsIntact( stream->sites.data[stream->loaded.data[k]].id ) ? 0 : 1;
	}

	stats.loadingCount = 0;
	for ( int b = 0; b < stream->def.loadDelay; ++b )
	{
		stats.loadingCount += stream->batches[b].sites.count;
	}
	return stats;
}

// The cell of a point for the marking, in cells of the given size with 21 bits per axis. Points far out may share a
// cell, which only makes the search of that cell larger.
static int64_t nbMarkKey( b3Vec3 point, float cellSize )
{
	const float limit = 1048575.0f;
	int64_t x = (int64_t)b3ClampFloat( floorf( point.x / cellSize ), -limit, limit ) & 0x1FFFFF;
	int64_t y = (int64_t)b3ClampFloat( floorf( point.y / cellSize ), -limit, limit ) & 0x1FFFFF;
	int64_t z = (int64_t)b3ClampFloat( floorf( point.z / cellSize ), -limit, limit ) & 0x1FFFFF;
	return ( x << 42 ) | ( y << 21 ) | z;
}

static bool nbReachSite( int proxyId, uint64_t userData, void* context )
{
	nbStream* stream = context;

	// The box as the tree holds it, so the site itself stays out of the cache unless a body comes near
	nbReachedSite reached = { b3DynamicTree_GetAABB( &stream->tree, proxyId ), (int)userData };
	nbArray_Push( stream->reached, reached );
	return true;
}

#if !defined( NDEBUG )
typedef struct nbCheckContext
{
	const nbStream* stream;
	b3Vec3 point;
	double time;
} nbCheckContext;

// A site within the hold radius of a body is marked, and asked for if it is out of the world and near enough
static bool nbCheckMark( int proxyId, uint64_t userData, void* context )
{
	nbCheckContext* check = context;
	const nbStream* stream = check->stream;
	const nbSite* site = stream->sites.data + (int)userData;
	float distance = nbSquaredBoxDistance( b3DynamicTree_GetAABB( &stream->tree, proxyId ), check->point );
	float holdRadius = stream->def.activeHoldRadius;
	float loadRadius = stream->def.activeLoadRadius;
	bool held = distance <= holdRadius * holdRadius;
	bool asked = held && distance <= loadRadius * loadRadius && ( site->state == nb_siteOut || site->state == nb_siteSaved );
	NB_ASSERT( held == false || site->activeTime == check->time );
	NB_ASSERT( asked == false || site->urgent );
	return true;
}
#endif

// Every body that moved keeps the sites within the hold radius from going, and asks for the sites out of the world
// within the active load radius before it can reach them. Debris flies in groups, so the bodies go by cells as large as
// the hold radius, and each cell searches the tree once for all of its bodies. The nearest body of a cell decides for a
// site, so a site is marked just as if every body searched on its own.
static void nbMarkStream( nbStream* stream, const b3Vec3* points, int pointCount, double time )
{
	float holdRadius = stream->def.activeHoldRadius;
	float loadRadius = stream->def.activeLoadRadius;
	float cellSize = b3MaxFloat( holdRadius, 1.0f );

	// The cells in a hash table with room for twice as many, each with its bodies in a list in the order they moved
	int capacity = 16;
	while ( capacity < 2 * pointCount )
	{
		capacity *= 2;
	}
	nbArray_Reserve( stream->markCells, capacity );
	nbArray_Reserve( stream->markNext, pointCount );
	nbMarkCell* cells = stream->markCells.data;
	int* next = stream->markNext.data;
	for ( int i = 0; i < capacity; ++i )
	{
		cells[i].first = NB_NULL_INDEX;
	}

	stream->markOrder.count = 0;
	for ( int i = 0; i < pointCount; ++i )
	{
		int64_t key = nbMarkKey( points[i], cellSize );
		int slot = (int)( nbHashSeed( (uint64_t)key, 0 ) & (uint64_t)( capacity - 1 ) );
		while ( cells[slot].first != NB_NULL_INDEX && cells[slot].key != key )
		{
			slot = ( slot + 1 ) & ( capacity - 1 );
		}

		next[i] = NB_NULL_INDEX;
		if ( cells[slot].first == NB_NULL_INDEX )
		{
			cells[slot] = (nbMarkCell){ key, i, i };
			nbArray_Push( stream->markOrder, slot );
		}
		else
		{
			next[cells[slot].last] = i;
			cells[slot].last = i;
		}
	}

	for ( int c = 0; c < stream->markOrder.count; ++c )
	{
		const nbMarkCell* cell = cells + stream->markOrder.data[c];
		b3AABB box = nbBoxAround( points[cell->first], holdRadius );
		for ( int i = next[cell->first]; i != NB_NULL_INDEX; i = next[i] )
		{
			box = b3AABB_Union( box, nbBoxAround( points[i], holdRadius ) );
		}

		stream->reached.count = 0;
		b3DynamicTree_Query( &stream->tree, box, B3_DEFAULT_MASK_BITS, false, nbReachSite, stream );
		for ( int k = 0; k < stream->reached.count; ++k )
		{
			nbReachedSite reached = stream->reached.data[k];
			float distance = FLT_MAX;
			for ( int i = cell->first; i != NB_NULL_INDEX && distance > 0.0f; i = next[i] )
			{
				distance = b3MinFloat( distance, nbSquaredBoxDistance( reached.reach, points[i] ) );
			}

			if ( distance > holdRadius * holdRadius )
			{
				continue;
			}

			nbSite* site = stream->sites.data + reached.site;
			site->activeTime = time;
			if ( distance <= loadRadius * loadRadius && ( site->state == nb_siteOut || site->state == nb_siteSaved ) &&
				 site->urgent == false )
			{
				site->urgent = true;
				nbArray_Push( stream->urgent, reached.site );
			}
		}
	}

#if !defined( NDEBUG )
	// The same marks as a search of its own for every body
	for ( int i = 0; i < pointCount; ++i )
	{
		nbCheckContext check = { stream, points[i], time };
		b3DynamicTree_Query( &stream->tree, nbBoxAround( points[i], holdRadius ), B3_DEFAULT_MASK_BITS, false, nbCheckMark,
							 &check );
	}
#endif
}

void nbMarkStreams( nbWorld* world )
{
	uint64_t ticks = b3GetTicks();
	b3BodyEvents events = b3World_GetBodyEvents( world->physicsWorld );
	nbArray_Reserve( world->movedCenters, events.moveCount );
	world->movedCenters.count = 0;
	for ( int e = 0; e < events.moveCount; ++e )
	{
		// The program may destroy bodies of its own between b3World_Step and nbWorld_Update
		b3BodyId bodyId = events.moveEvents[e].bodyId;
		if ( b3Body_IsValid( bodyId ) == false )
		{
			continue;
		}

		// The chunks of a body lie in the frame of their destructible, its origin can be meters away
		world->movedCenters.data[world->movedCenters.count++] = b3ToVec3( b3Body_GetWorldCenter( bodyId ) );
	}

	for ( int s = 0; s < world->streams.count; ++s )
	{
		nbStream* stream = world->streams.data[s].stream;
		if ( stream != NULL && stream->sites.count > 0 && world->movedCenters.count > 0 )
		{
			nbMarkStream( stream, world->movedCenters.data, world->movedCenters.count, world->time );
		}
	}

	float markTime = b3GetMilliseconds( ticks );
	for ( int s = 0; s < world->streams.count; ++s )
	{
		if ( world->streams.data[s].stream != NULL )
		{
			world->streams.data[s].stream->stats.markTime = markTime;
		}
	}
}

// The bounds of the parts of a destructible, its rubble included
static b3AABB nbDestructibleBounds( nbStream* stream, nbDestructibleId id, b3AABB bounds )
{
	int count = nbDestructible_GetChunkCount( id );
	nbArray_Reserve( stream->chunks, count );
	nbChunkId* chunks = stream->chunks.data;
	count = nbDestructible_GetChunks( id, chunks, count );
	for ( int i = 0; i < count; ++i )
	{
		bounds = b3AABB_Union( bounds, b3Shape_GetAABB( nbChunk_GetShape( chunks[i] ) ) );
	}
	return bounds;
}

// Save a damaged site at rest with its rubble and take it out of the world, see nbSaveDestructibles. False if the store
// of the program could not keep it.
static bool nbSaveSite( nbStream* stream, int index )
{
	nbSite* site = stream->sites.data + index;
	size_t size = nbSaveDestructibles( &site->id, 1, NULL, 0 );
	uint8_t* data = nbAlloc( size );
	nbSaveDestructibles( &site->id, 1, data, size );

	if ( stream->def.storeFcn != NULL )
	{
		bool kept = stream->def.storeFcn( index, data, size, stream->def.userContext );
		nbFree( data, size );
		if ( kept == false )
		{
			return false;
		}
	}
	else
	{
		site->saved = data;
		stream->stats.savedBytes += size;
	}

	// A body that comes near its rubble brings it back
	site->savedSize = size;
	site->reach = nbDestructibleBounds( stream, site->id, site->bounds );
	b3DynamicTree_MoveProxy( &stream->tree, site->proxyId, site->reach );

	nbDestroyDestructible( site->id );
	site->id = nb_nullDestructibleId;
	site->state = nb_siteSaved;
	stream->stats.saves += 1;
	stream->stats.savedCount += 1;
	return true;
}

// Bring a saved site back as it was. False if the store of the program lost it.
static bool nbRestoreSite( nbStream* stream, int index )
{
	nbSite* site = stream->sites.data + index;
	size_t size = site->savedSize;
	uint8_t* data = site->saved;
	if ( stream->def.fetchFcn != NULL )
	{
		data = nbAlloc( size );
		if ( stream->def.fetchFcn( index, data, size, stream->def.userContext ) == false )
		{
			nbFree( data, size );
			return false;
		}
	}

	// Written by this build from a site that was there, so it loads
	bool loaded = nbLoadDestructibles( nbStreamWorldId( stream ), data, size, &site->id );
	NB_ASSERT( loaded );
	if ( stream->def.fetchFcn != NULL )
	{
		nbFree( data, size );
	}
	if ( loaded == false )
	{
		return false;
	}

	if ( site->saved != NULL )
	{
		nbFree( site->saved, size );
		site->saved = NULL;
		stream->stats.savedBytes -= size;
	}
	site->savedSize = 0;
	site->reach = site->bounds;
	b3DynamicTree_MoveProxy( &stream->tree, site->proxyId, site->reach );
	site->state = nb_siteIn;
	nbArray_Push( stream->loaded, index );
	stream->stats.restores += 1;
	stream->stats.savedCount -= 1;
	return true;
}

// Take out the sites beyond the load radius plus the margin of every viewer that no moving body came near for the hold
// time and that can go, at most the unload budget of them, and of those the damaged ones at most the save budget, which
// go with their rubble. A site that cannot go is asked again after the retry time.
static void nbUnloadSites( nbStream* stream, const b3Vec3* viewers, int viewerCount, b3Vec3 up )
{
	const nbStreamDef* def = &stream->def;
	double time = stream->world->time;
	float radius = def->loadRadius + def->unloadMargin;
	int count = 0;
	int saveCount = 0;
	for ( int k = 0; k < stream->loaded.count && count < def->unloadBudget; )
	{
		int index = stream->loaded.data[k];
		nbSite* site = stream->sites.data + index;
		if ( time < site->nextCheck || time < site->activeTime + def->holdTime ||
			 nbViewerDistance( site, viewers, viewerCount, up ) <= radius * radius )
		{
			k += 1;
			continue;
		}

		bool intact = nbDestructible_IsIntact( site->id );
		if ( intact == false && saveCount == def->saveBudget )
		{
			k += 1;
			continue;
		}

		if ( intact ? nbDestructible_CanUnload( site->id, def->margin ) == false
					: nbCanSaveDestructibles( &site->id, 1, def->margin ) == false )
		{
			site->nextCheck = time + def->retryTime;
			k += 1;
			continue;
		}

		if ( intact )
		{
			nbDestroyDestructible( site->id );
			site->id = nb_nullDestructibleId;
			site->state = nb_siteOut;
			stream->stats.unloads += 1;
		}
		else if ( nbSaveSite( stream, index ) )
		{
			saveCount += 1;
		}
		else
		{
			site->nextCheck = time + def->retryTime;
			k += 1;
			continue;
		}

		stream->loaded.data[k] = stream->loaded.data[--stream->loaded.count];
		count += 1;
	}
}

// Build in the sites of a batch, which the workers prepared in the background
static void nbFinishBatch( nbStream* stream, nbStreamBatch* batch )
{
	if ( batch->sites.count == 0 )
	{
		return;
	}

	nbFinishCreating( batch->creation, batch->ids.data );
	for ( int k = 0; k < batch->sites.count; ++k )
	{
		int index = batch->sites.data[k];
		nbSite* site = stream->sites.data + index;
		site->id = batch->ids.data[k];
		site->state = nb_siteIn;

		// One without pieces has nothing to stream
		if ( NB_IS_NON_NULL( site->id ) )
		{
			nbArray_Push( stream->loaded, index );
			stream->stats.loads += 1;
		}
		nbReleaseSite( stream, index );
	}
	batch->sites.count = 0;
}

typedef struct nbPickContext
{
	nbStream* stream;
	const b3Vec3* viewers;
	int viewerCount;
	b3Vec3 up;
	float radius;
	int count;
} nbPickContext;

// Keep a site out of the world within the radius of the viewers, once for several viewers
static bool nbPickSite( int proxyId, uint64_t userData, void* context )
{
	NB_UNUSED( proxyId );
	nbPickContext* pick = context;
	nbStream* stream = pick->stream;
	int index = (int)userData;
	nbSite* site = stream->sites.data + index;
	if ( site->pickStamp == stream->pickStamp || ( site->state != nb_siteOut && site->state != nb_siteSaved ) )
	{
		return true;
	}

	site->pickStamp = stream->pickStamp;
	if ( nbViewerDistance( site, pick->viewers, pick->viewerCount, pick->up ) <= pick->radius * pick->radius )
	{
		nbArray_Push( stream->nearby, index );
	}
	return true;
}

#if !defined( NDEBUG )
// Count a site a viewer can pick, once for several viewers
static bool nbCountSite( int proxyId, uint64_t userData, void* context )
{
	NB_UNUSED( proxyId );
	nbPickContext* pick = context;
	nbStream* stream = pick->stream;
	nbSite* site = stream->sites.data + (int)userData;
	if ( site->pickStamp == stream->pickStamp || site->urgent || ( site->state != nb_siteOut && site->state != nb_siteSaved ) )
	{
		return true;
	}

	site->pickStamp = stream->pickStamp;
	pick->count += nbViewerDistance( site, pick->viewers, pick->viewerCount, pick->up ) <= pick->radius * pick->radius ? 1 : 0;
	return true;
}
#endif

// Whether the viewers changed since the last search, or one of them moved farther along the ground than the tolerance
static bool nbNearbyStale( const nbStream* stream, const b3Vec3* viewers, int viewerCount, b3Vec3 up, float tolerance )
{
	if ( stream->nearValid == false || stream->nearViewers.count != viewerCount || up.x != stream->nearUp.x ||
		 up.y != stream->nearUp.y || up.z != stream->nearUp.z )
	{
		return true;
	}

	for ( int i = 0; i < viewerCount; ++i )
	{
		b3Vec3 offset = b3Sub( viewers[i], stream->nearViewers.data[i] );
		offset = b3MulSub( offset, b3Dot( offset, up ), up );
		if ( b3LengthSquared( offset ) > tolerance * tolerance )
		{
			return true;
		}
	}
	return false;
}

static int nbCompareCandidates( const void* a, const void* b )
{
	const nbCandidate* x = a;
	const nbCandidate* y = b;
	if ( x->distance != y->distance )
	{
		return x->distance < y->distance ? -1 : 1;
	}
	return ( x->site > y->site ) - ( x->site < y->site );
}

// The sites a moving body asked for, then the ones within the load radius of the viewers, the nearest first. The ones a
// moving body asked for that do not fit come back with the next step that moves something near them.
//
// The tree is searched again only once a viewer moved farther than half the unload margin. The search keeps the sites
// out of the world within the load radius plus that half, and the updates until the next one measure only those. No
// other site can come within the load radius meanwhile: The ones farther out stay beyond it, and the ones in the world
// that could reach it cannot go, the viewers stay within the load radius plus the unload margin of them. So the updates
// pick the same sites as with a search each time.
static void nbPickSites( nbStream* stream, const b3Vec3* viewers, int viewerCount, b3Vec3 up )
{
	stream->candidates.count = 0;
	for ( int k = 0; k < stream->urgent.count; ++k )
	{
		nbArray_Push( stream->candidates, ( (nbCandidate){ -1.0f, stream->urgent.data[k] } ) );
	}

	float tolerance = b3MaxFloat( 0.5f * stream->def.unloadMargin, 0.0f );
	if ( nbNearbyStale( stream, viewers, viewerCount, up, tolerance ) )
	{
		stream->nearby.count = 0;
		stream->pickStamp += 1;
		nbPickContext pick = { stream, viewers, viewerCount, up, stream->def.loadRadius + tolerance, 0 };
		for ( int i = 0; i < viewerCount; ++i )
		{
			b3DynamicTree_Query( &stream->tree, nbViewerBox( viewers[i], pick.radius, up ), B3_DEFAULT_MASK_BITS, false,
								 nbPickSite, &pick );
		}

		nbArray_Reserve( stream->nearViewers, viewerCount );
		for ( int i = 0; i < viewerCount; ++i )
		{
			stream->nearViewers.data[i] = viewers[i];
		}
		stream->nearViewers.count = viewerCount;
		stream->nearUp = up;
		stream->nearValid = true;
	}

	// The ones in the world or on their way leave the list. The urgent ones are candidates already, they keep their mark
	// until here.
	float radius = stream->def.loadRadius;
	int nearCount = 0;
	for ( int k = 0; k < stream->nearby.count; ++k )
	{
		int index = stream->nearby.data[k];
		const nbSite* site = stream->sites.data + index;
		if ( site->state != nb_siteOut && site->state != nb_siteSaved )
		{
			continue;
		}

		stream->nearby.data[nearCount++] = index;
		float distance = nbViewerDistance( site, viewers, viewerCount, up );
		if ( site->urgent == false && distance <= radius * radius )
		{
			nbArray_Push( stream->candidates, ( (nbCandidate){ distance, index } ) );
		}
	}
	stream->nearby.count = nearCount;

#if !defined( NDEBUG )
	// A search of the tree finds as many, and the list holds only sites it would find, so the same ones
	stream->pickStamp += 1;
	nbPickContext check = { stream, viewers, viewerCount, up, radius, 0 };
	for ( int i = 0; i < viewerCount; ++i )
	{
		b3DynamicTree_Query( &stream->tree, nbViewerBox( viewers[i], radius, up ), B3_DEFAULT_MASK_BITS, false, nbCountSite,
							 &check );
	}
	NB_ASSERT( check.count == stream->candidates.count - stream->urgent.count );
#endif

	for ( int k = 0; k < stream->urgent.count; ++k )
	{
		stream->sites.data[stream->urgent.data[k]].urgent = false;
	}
	stream->urgent.count = 0;

	qsort( stream->candidates.data, (size_t)stream->candidates.count, sizeof( nbCandidate ), nbCompareCandidates );
}

// Prepare the definitions of the sites of a batch and start the workers on them, see nbStartCreating
static void nbStartBatch( nbStream* stream, nbStreamBatch* batch )
{
	int count = batch->sites.count;
	nbArray_Reserve( batch->defs, count );
	nbArray_Reserve( batch->pieceLists, count );
	nbArray_Reserve( batch->pieceCounts, count );
	nbArray_Reserve( batch->ids, count );
	for ( int k = 0; k < count; ++k )
	{
		int index = batch->sites.data[k];
		nbSiteDef siteDef = { 0 };
		stream->def.prepareFcn( index, &siteDef, stream->def.userContext );
		batch->defs.data[k] = siteDef.def;
		batch->pieceLists.data[k] = siteDef.pieces;
		batch->pieceCounts.data[k] = siteDef.pieceCount;
		stream->sites.data[index].state = nb_siteLoading;
	}
	batch->creation = nbStartCreating( nbStreamWorldId( stream ), batch->defs.data, batch->pieceLists.data,
									   batch->pieceCounts.data, count );
}

static void nbUpdateStream( nbStream* stream, const b3Vec3* viewers, int viewerCount )
{
	uint64_t ticks = b3GetTicks();
	b3Vec3 up = nbStreamUp( stream );
	nbUnloadSites( stream, viewers, viewerCount, up );

	// The batch started the load delay ago, then the next one in its place. A saved site comes back right away.
	nbStreamBatch* batch = stream->batches + stream->batchIndex;
	stream->batchIndex = ( stream->batchIndex + 1 ) % stream->def.loadDelay;
	nbFinishBatch( stream, batch );

	nbPickSites( stream, viewers, viewerCount, up );
	for ( int k = 0; k < stream->candidates.count && k < stream->def.loadBudget; ++k )
	{
		nbCandidate candidate = stream->candidates.data[k];
		stream->stats.urgentLoads += candidate.distance < 0.0f ? 1 : 0;
		if ( stream->sites.data[candidate.site].state == nb_siteSaved )
		{
			nbRestoreSite( stream, candidate.site );
			continue;
		}
		nbArray_Push( batch->sites, candidate.site );
	}

	if ( batch->sites.count > 0 )
	{
		nbStartBatch( stream, batch );
	}
	stream->stats.updateTime = b3GetMilliseconds( ticks );
}

static void nbRunStreamJob( void* context )
{
	nbStreamJob* job = context;
	nbUpdateStream( job->stream, job->viewers, job->viewerCount );
	nbFree( job, job->size );
}

void nbStream_Update( nbStreamId streamId, const b3Vec3* viewers, int viewerCount )
{
	// Not through a lookup that waits, the update follows the steps the calling thread started
	nbStream* stream = nbFindStream( streamId, false );
	if ( stream == NULL )
	{
		return;
	}

	viewerCount = viewers != NULL && viewerCount > 0 ? viewerCount : 0;
	size_t size = sizeof( nbStreamJob ) + sizeof( b3Vec3 ) * (size_t)viewerCount;
	nbStreamJob* job = nbAlloc( size );
	job->stream = stream;
	job->size = size;
	job->viewerCount = viewerCount;
	if ( viewerCount > 0 )
	{
		memcpy( job->viewers, viewers, sizeof( b3Vec3 ) * (size_t)viewerCount );
	}
	nbWorld_RunAfterStep( nbStreamWorldId( stream ), nbRunStreamJob, job );
}

void nbStream_LoadAround( nbStreamId streamId, const b3Vec3* viewers, int viewerCount )
{
	nbStream* stream = nbFindStream( streamId, true );
	if ( stream == NULL || viewers == NULL || viewerCount < 1 )
	{
		return;
	}

	// All of them at once, like an update without a budget, built in right away
	nbPickSites( stream, viewers, viewerCount, nbStreamUp( stream ) );
	nbStreamBatch batch = { 0 };
	for ( int k = 0; k < stream->candidates.count; ++k )
	{
		int index = stream->candidates.data[k].site;
		if ( stream->sites.data[index].state == nb_siteSaved )
		{
			nbRestoreSite( stream, index );
			continue;
		}
		nbArray_Push( batch.sites, index );
	}

	int count = batch.sites.count;
	nbArray_Reserve( batch.defs, count );
	nbArray_Reserve( batch.pieceLists, count );
	nbArray_Reserve( batch.pieceCounts, count );
	nbArray_Reserve( batch.ids, count );
	for ( int k = 0; k < count; ++k )
	{
		nbSiteDef siteDef = { 0 };
		stream->def.prepareFcn( batch.sites.data[k], &siteDef, stream->def.userContext );
		batch.defs.data[k] = siteDef.def;
		batch.pieceLists.data[k] = siteDef.pieces;
		batch.pieceCounts.data[k] = siteDef.pieceCount;
	}

	if ( count > 0 )
	{
		nbCreateDestructibles( nbStreamWorldId( stream ), batch.defs.data, batch.pieceLists.data, batch.pieceCounts.data,
							   count, batch.ids.data );
	}

	for ( int k = 0; k < count; ++k )
	{
		int index = batch.sites.data[k];
		nbSite* site = stream->sites.data + index;
		site->id = batch.ids.data[k];
		site->state = nb_siteIn;
		if ( NB_IS_NON_NULL( site->id ) )
		{
			nbArray_Push( stream->loaded, index );
			stream->stats.loads += 1;
		}
		nbReleaseSite( stream, index );
	}

	nbArray_Free( batch.sites );
	nbArray_Free( batch.defs );
	nbArray_Free( batch.pieceLists );
	nbArray_Free( batch.pieceCounts );
	nbArray_Free( batch.ids );
}
