// SPDX-License-Identifier: MIT

#include "world.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

// Destructibles at rest saved to a buffer and loaded back, see nbSaveDestructibles. The structures go into the buffer as
// they are, with every reference to a chunk, bond or actor turned into its place in the buffer, and every value that only
// means something in one world, such as indices, Box3D ids, generations and visit stamps, set to the value a loaded copy
// gets. So saving what was loaded gives the same bytes again. A buffer only loads into the build that wrote it, the header
// holds the sizes of the structures to tell.

#define NB_SAVE_MAGIC 0x5642454Eu
#define NB_SAVE_VERSION 1u

// Every part of the buffer starts on this many bytes from its start
#define NB_SAVE_ALIGNMENT 16

// A carrier or a hold source that refers to nothing any more, a chunk or an actor that went
#define NB_SAVE_STALE ( -2 )

typedef struct nbSaveHeader
{
	uint32_t magic;
	uint32_t version;
	uint32_t layout;
	int32_t destructibleCount;
	int32_t actorCount;
	int32_t chunkCount;
	int32_t bondCount;
	int32_t orderCount;
	uint64_t shapeBytes;
	uint64_t byteCount;
} nbSaveHeader;

typedef struct nbSavedDestructible
{
	nbDestructible destructible;

	// Its actors, consecutive in the buffer, in the order of its list
	int32_t firstActor;
	int32_t actorCount;

	// The chunks on its static body in the order of the shapes of that body, see nbSavedActor
	int32_t firstOrder;
	int32_t orderCount;
} nbSavedDestructible;

typedef struct nbSavedActor
{
	nbActor actor;

	// The body of rubble
	b3WorldTransform transform;
	float safetyFactor;

	// Its chunks, consecutive in the buffer, in the order of its list
	int32_t firstChunk;
	int32_t chunkCount;

	// Its chunks in the order of the shapes on its body. Box3D puts a new shape at the front and sums the mass in that
	// order, so they go back in reverse.
	int32_t firstOrder;
	int32_t orderCount;
} nbSavedActor;

typedef struct nbSavedChunk
{
	nbChunk chunk;

	// Its shape in the shape data, padded to NB_SAVE_ALIGNMENT and followed by a hull of its own from quickhull if it has
	// one
	uint64_t shapeOffset;
	uint32_t shapeBytes;
	uint32_t hullBytes;
} nbSavedChunk;

typedef struct nbSavedBond
{
	nbBond bond;
	nbBondMoments moments;
} nbSavedBond;

static size_t nbAlignSave( size_t size )
{
	return ( size + NB_SAVE_ALIGNMENT - 1 ) & ~(size_t)( NB_SAVE_ALIGNMENT - 1 );
}

// The sizes of the structures, so a buffer of another build or platform is refused
static uint32_t nbSaveLayout( void )
{
	size_t sizes[] = {
		sizeof( nbDestructible ), sizeof( nbActor ),		 sizeof( nbChunk ),		  sizeof( nbBond ),
		sizeof( nbBondMoments ),  sizeof( nbShape ),		 sizeof( b3HullData ),	  sizeof( nbSavedDestructible ),
		sizeof( nbSavedActor ),	  sizeof( nbSavedChunk ),	 sizeof( nbSavedBond ),	  sizeof( void* ),
	};

	uint32_t hash = 2166136261u;
	for ( int i = 0; i < NB_ARRAY_COUNT( sizes ); ++i )
	{
		hash = ( hash ^ (uint32_t)sizes[i] ) * 16777619u;
	}
	return hash;
}

// The places in the buffer of indices in the world, in a table with open addressing that is at most half full
typedef struct nbPlaceMap
{
	int* indices;
	int* places;
	uint32_t capacity;
} nbPlaceMap;

static uint32_t nbHashIndex( int index )
{
	uint32_t x = (uint32_t)index;
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	return x ^ ( x >> 16 );
}

static void nbCreatePlaceMap( nbPlaceMap* map, const int* indices, int count )
{
	map->capacity = 16;
	while ( map->capacity < 2u * (uint32_t)count )
	{
		map->capacity <<= 1;
	}

	map->indices = nbAlloc( sizeof( int ) * map->capacity );
	map->places = nbAlloc( sizeof( int ) * map->capacity );
	for ( uint32_t i = 0; i < map->capacity; ++i )
	{
		map->indices[i] = NB_NULL_INDEX;
	}

	uint32_t mask = map->capacity - 1;
	for ( int place = 0; place < count; ++place )
	{
		uint32_t slot = nbHashIndex( indices[place] ) & mask;
		while ( map->indices[slot] != NB_NULL_INDEX )
		{
			slot = ( slot + 1 ) & mask;
		}
		map->indices[slot] = indices[place];
		map->places[slot] = place;
	}
}

static void nbDestroyPlaceMap( nbPlaceMap* map )
{
	nbFree( map->indices, sizeof( int ) * map->capacity );
	nbFree( map->places, sizeof( int ) * map->capacity );
}

static int nbFindPlace( const nbPlaceMap* map, int index )
{
	uint32_t mask = map->capacity - 1;
	uint32_t slot = nbHashIndex( index ) & mask;
	while ( map->indices[slot] != NB_NULL_INDEX )
	{
		if ( map->indices[slot] == index )
		{
			return map->places[slot];
		}
		slot = ( slot + 1 ) & mask;
	}
	return NB_NULL_INDEX;
}

// The destructibles to save, by index in their world, in the order given
typedef struct nbSaveSet
{
	nbWorld* world;
	int* indices;
	int count;
} nbSaveSet;

static bool nbSetHas( const nbSaveSet* set, int destructibleIndex )
{
	for ( int i = 0; i < set->count; ++i )
	{
		if ( set->indices[i] == destructibleIndex )
		{
			return true;
		}
	}
	return false;
}

static void nbFreeSet( nbSaveSet* set )
{
	nbFree( set->indices, sizeof( int ) * (size_t)set->count );
	set->indices = NULL;
}

// Valid destructibles of one world, each once
static bool nbResolveSet( const nbDestructibleId* ids, int count, nbSaveSet* set )
{
	*set = (nbSaveSet){ 0 };
	if ( ids == NULL || count <= 0 )
	{
		return false;
	}

	set->indices = nbAlloc( sizeof( int ) * (size_t)count );
	set->count = count;
	for ( int i = 0; i < count; ++i )
	{
		nbWorld* world;
		if ( nbGetDestructibleFromId( ids[i], &world ) == NULL || ( set->world != NULL && world != set->world ) ||
			 nbSetHas( &(nbSaveSet){ world, set->indices, i }, ids[i].index1 - 1 ) )
		{
			nbFreeSet( set );
			return false;
		}
		set->world = world;
		set->indices[i] = ids[i].index1 - 1;
	}
	return true;
}

// The chunk a carrier record names, if it is still there
static const nbChunk* nbGetCarrierChunk( const nbWorld* world, const nbCarrier* carrier )
{
	if ( carrier->chunkIndex == NB_NULL_INDEX )
	{
		return NULL;
	}

	const nbChunk* chunk = world->chunks.data + carrier->chunkIndex;
	return chunk->generation == carrier->generation && chunk->shape != NULL ? chunk : NULL;
}

// The actor a hold source names, if it is still there
static const nbActor* nbGetHoldSource( const nbWorld* world, const nbActor* actor )
{
	if ( actor->holdSource == NB_NULL_INDEX )
	{
		return NULL;
	}

	const nbActor* source = world->actors.data + actor->holdSource;
	return source->isFree == false && source->generation == actor->holdSourceGeneration ? source : NULL;
}

// Everything that keeps destructibles from being saved, apart from what lies near them: debris that moves or sleeps, a
// load check still to run, rubble that lies on chunks of other destructibles or rubble of other destructibles on theirs,
// and a piece that holds rubble of theirs
static bool nbCanSaveSet( const nbSaveSet* set )
{
	const nbWorld* world = set->world;
	for ( int i = 0; i < set->count; ++i )
	{
		const nbDestructible* destructible = world->destructibles.data + set->indices[i];
		for ( int a = destructible->headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
		{
			const nbActor* actor = world->actors.data + a;
			if ( ( actor->isStatic == false && actor->isRubble == false ) || actor->supportDirty )
			{
				return false;
			}
		}
	}

	// The rubble and the pieces of all destructibles, those of the set included
	for ( int i = 0; i < world->debris.count; ++i )
	{
		const nbActor* actor = world->actors.data + world->debris.data[i];
		bool inside = nbSetHas( set, actor->destructibleIndex );
		for ( int k = 0; actor->isRubble && k < actor->carrierCount; ++k )
		{
			const nbChunk* chunk = nbGetCarrierChunk( world, actor->carriers + k );
			if ( chunk != NULL && nbSetHas( set, chunk->destructibleIndex ) != inside )
			{
				return false;
			}
		}

		const nbActor* source = actor->holdsRubble ? nbGetHoldSource( world, actor ) : NULL;
		if ( source != NULL && inside == false && nbSetHas( set, source->destructibleIndex ) )
		{
			return false;
		}
	}
	return true;
}

typedef struct nbSaveQuery
{
	const nbSaveSet* set;
	bool blocked;
} nbSaveQuery;

// Whatever lies near the destructibles keeps them from being saved, unless it stands for itself: their own chunks, the
// standing parts of other destructibles and the static bodies of the application, see nbDestructible_CanUnload
static bool nbSaveQueryCallback( b3ShapeId shapeId, void* context )
{
	nbSaveQuery* query = context;
	if ( b3Shape_IsSensor( shapeId ) )
	{
		return true;
	}

	const nbWorld* world = query->set->world;
	int chunkIndex = nbFindChunkFromShape( world, shapeId );
	if ( chunkIndex != NB_NULL_INDEX )
	{
		const nbChunk* chunk = world->chunks.data + chunkIndex;
		if ( nbSetHas( query->set, chunk->destructibleIndex ) || world->actors.data[chunk->actorIndex].isStatic )
		{
			return true;
		}
	}
	else if ( b3Body_GetType( b3Shape_GetBody( shapeId ) ) == b3_staticBody )
	{
		return true;
	}

	query->blocked = true;
	return false;
}

static void nbQueryNear( const nbSaveSet* set, const nbDestructible* destructible, b3AABB bounds, float margin,
						 nbSaveQuery* query )
{
	b3Vec3 extent = { margin, margin, margin };
	bounds.lowerBound = b3Sub( bounds.lowerBound, extent );
	bounds.upperBound = b3Add( bounds.upperBound, extent );

	// Only what collides with the chunks counts
	b3QueryFilter filter = b3DefaultQueryFilter();
	filter.categoryBits = destructible->filter.categoryBits;
	filter.maskBits = destructible->filter.maskBits;
	b3World_OverlapAABB( set->world->physicsWorld, bounds, filter, nbSaveQueryCallback, query );
}

// Nothing near the standing parts and the rubble of the destructibles that moves or can be moved
static bool nbIsQuietAround( const nbSaveSet* set, float margin )
{
	const nbWorld* world = set->world;
	margin = b3MaxFloat( margin, 0.0f );
	nbSaveQuery query = { set, false };
	for ( int i = 0; i < set->count && query.blocked == false; ++i )
	{
		const nbDestructible* destructible = world->destructibles.data + set->indices[i];
		b3AABB standing = { { FLT_MAX, FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX, -FLT_MAX } };
		bool hasStanding = false;
		for ( int a = destructible->headActor; a != NB_NULL_INDEX && query.blocked == false; a = world->actors.data[a].nextActor )
		{
			const nbActor* actor = world->actors.data + a;
			if ( actor->isStatic )
			{
				for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
				{
					standing = b3AABB_Union( standing, b3Shape_GetAABB( world->chunks.data[c].shapeId ) );
					hasStanding = true;
				}
				continue;
			}

			nbQueryNear( set, destructible, b3Body_ComputeAABB( actor->bodyId ), margin, &query );
		}

		if ( hasStanding && query.blocked == false )
		{
			nbQueryNear( set, destructible, standing, margin, &query );
		}
	}
	return query.blocked == false;
}

bool nbCanSaveDestructibles( const nbDestructibleId* ids, int count, float margin )
{
	nbSaveSet set;
	if ( nbResolveSet( ids, count, &set ) == false )
	{
		return false;
	}

	bool canSave = nbCanSaveSet( &set ) && nbIsQuietAround( &set, margin );
	nbFreeSet( &set );
	return canSave;
}

// The bytes of a shape in the buffer, and of a hull of its own from quickhull if it has one, else zero
static size_t nbGetShapeBytes( const nbShape* shape, size_t* hullBytes )
{
	nbShapeLayout layout = nbGetShapeLayout( shape->vertexCount, shape->faceCount, shape->indexCount );
	bool inside = layout.hullOffset > 0 && (const uint8_t*)shape->hull == (const uint8_t*)shape + layout.hullOffset;
	*hullBytes = inside ? 0 : (size_t)shape->hull->byteCount;
	return layout.byteCount;
}

// The places of the actors, chunks and bonds of a set: the actors of each destructible in the order of its list, the
// chunks of each actor in the order of its list, and every bond once, from its first chunk, in the order of the chunks and
// of their bond lists. Both chunks of a bond belong to one actor. Only what goes into the buffer is counted until it is
// written.
typedef struct nbSavePlan
{
	int actorCount;
	int chunkCount;
	int bondCount;
	int orderCount;
	uint64_t shapeBytes;

	int* actors;
	int* chunks;
	int* bonds;
	nbPlaceMap actorPlaces;
	nbPlaceMap chunkPlaces;
	nbPlaceMap bondPlaces;
	bool listed;
} nbSavePlan;

static void nbCountPlan( const nbSaveSet* set, nbSavePlan* plan )
{
	const nbWorld* world = set->world;
	*plan = (nbSavePlan){ 0 };
	int bondEnds = 0;
	for ( int i = 0; i < set->count; ++i )
	{
		const nbDestructible* destructible = world->destructibles.data + set->indices[i];
		plan->actorCount += destructible->actorCount;
		plan->chunkCount += destructible->chunkCount;
		for ( int a = destructible->headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
		{
			const nbActor* actor = world->actors.data + a;
			for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
			{
				const nbChunk* chunk = world->chunks.data + c;
				bondEnds += chunk->bondCount;
				size_t hullBytes;
				plan->shapeBytes += nbAlignSave( nbGetShapeBytes( chunk->shape, &hullBytes ) );
				plan->shapeBytes += nbAlignSave( hullBytes );
			}
			plan->orderCount += B3_IS_NON_NULL( actor->bodyId ) ? b3Body_GetShapeCount( actor->bodyId ) : 0;
		}

#if defined( NB_SHARED_STATIC_BODY )
		plan->orderCount += B3_IS_NON_NULL( destructible->staticBody ) ? b3Body_GetShapeCount( destructible->staticBody ) : 0;
#endif
	}

	// Every bond is in the lists of both its chunks
	NB_ASSERT( bondEnds % 2 == 0 );
	plan->bondCount = bondEnds / 2;
}

static void nbListPlan( const nbSaveSet* set, nbSavePlan* plan )
{
	const nbWorld* world = set->world;
	plan->actors = nbAlloc( sizeof( int ) * (size_t)( plan->actorCount + 1 ) );
	plan->chunks = nbAlloc( sizeof( int ) * (size_t)( plan->chunkCount + 1 ) );
	plan->bonds = nbAlloc( sizeof( int ) * (size_t)( plan->bondCount + 1 ) );
	int actorCount = 0;
	int chunkCount = 0;
	int bondCount = 0;
	for ( int i = 0; i < set->count; ++i )
	{
		const nbDestructible* destructible = world->destructibles.data + set->indices[i];
		for ( int a = destructible->headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
		{
			plan->actors[actorCount++] = a;
			for ( int c = world->actors.data[a].headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
			{
				plan->chunks[chunkCount++] = c;
				for ( int key = world->chunks.data[c].headBondKey; key != NB_NULL_INDEX;
					  key = world->bonds.data[key >> 1].nextKey[key & 1] )
				{
					if ( ( key & 1 ) == 0 )
					{
						plan->bonds[bondCount++] = key >> 1;
					}
				}
			}
		}
	}
	NB_ASSERT( actorCount == plan->actorCount && chunkCount == plan->chunkCount && bondCount == plan->bondCount );

	nbCreatePlaceMap( &plan->actorPlaces, plan->actors, plan->actorCount );
	nbCreatePlaceMap( &plan->chunkPlaces, plan->chunks, plan->chunkCount );
	nbCreatePlaceMap( &plan->bondPlaces, plan->bonds, plan->bondCount );
	plan->listed = true;
}

static void nbFreePlan( nbSavePlan* plan )
{
	if ( plan->listed )
	{
		nbFree( plan->actors, sizeof( int ) * (size_t)( plan->actorCount + 1 ) );
		nbFree( plan->chunks, sizeof( int ) * (size_t)( plan->chunkCount + 1 ) );
		nbFree( plan->bonds, sizeof( int ) * (size_t)( plan->bondCount + 1 ) );
		nbDestroyPlaceMap( &plan->actorPlaces );
		nbDestroyPlaceMap( &plan->chunkPlaces );
		nbDestroyPlaceMap( &plan->bondPlaces );
	}
}

// A bond key with the place of the bond instead of its index
static int nbSaveKey( const nbSavePlan* plan, int key )
{
	if ( key == NB_NULL_INDEX )
	{
		return NB_NULL_INDEX;
	}
	return ( nbFindPlace( &plan->bondPlaces, key >> 1 ) << 1 ) | ( key & 1 );
}

// The parts of a buffer, each on NB_SAVE_ALIGNMENT bytes from its start
typedef struct nbSaveParts
{
	size_t destructibles;
	size_t actors;
	size_t chunks;
	size_t bonds;
	size_t orders;
	size_t shapes;
	size_t byteCount;
} nbSaveParts;

static nbSaveParts nbGetSaveParts( int destructibleCount, int actorCount, int chunkCount, int bondCount, int orderCount,
								   uint64_t shapeBytes )
{
	nbSaveParts parts;
	parts.destructibles = nbAlignSave( sizeof( nbSaveHeader ) );
	parts.actors = parts.destructibles + nbAlignSave( sizeof( nbSavedDestructible ) * (size_t)destructibleCount );
	parts.chunks = parts.actors + nbAlignSave( sizeof( nbSavedActor ) * (size_t)actorCount );
	parts.bonds = parts.chunks + nbAlignSave( sizeof( nbSavedChunk ) * (size_t)chunkCount );
	parts.orders = parts.bonds + nbAlignSave( sizeof( nbSavedBond ) * (size_t)bondCount );
	parts.shapes = parts.orders + nbAlignSave( sizeof( int32_t ) * (size_t)orderCount );
	parts.byteCount = parts.shapes + (size_t)shapeBytes;
	return parts;
}

// The places of the chunks on a body in the order of its shapes
static int nbWriteOrder( const nbWorld* world, const nbSavePlan* plan, b3BodyId bodyId, int32_t* orders )
{
	int count = b3Body_GetShapeCount( bodyId );
	b3ShapeId* shapes = nbAlloc( sizeof( b3ShapeId ) * (size_t)( count + 1 ) );
	count = b3Body_GetShapes( bodyId, shapes, count );
	for ( int i = 0; i < count; ++i )
	{
		int chunkIndex = nbFindChunkFromShape( world, shapes[i] );
		orders[i] = nbFindPlace( &plan->chunkPlaces, chunkIndex );
		NB_ASSERT( orders[i] != NB_NULL_INDEX );
	}
	nbFree( shapes, sizeof( b3ShapeId ) * (size_t)( count + 1 ) );
	return count;
}

size_t nbSaveDestructibles( const nbDestructibleId* ids, int count, void* buffer, size_t capacity )
{
	nbSaveSet set;
	if ( nbResolveSet( ids, count, &set ) == false )
	{
		return 0;
	}

	if ( nbCanSaveSet( &set ) == false )
	{
		nbFreeSet( &set );
		return 0;
	}

	const nbWorld* world = set.world;
	nbSavePlan plan;
	nbCountPlan( &set, &plan );
	nbSaveParts parts =
		nbGetSaveParts( set.count, plan.actorCount, plan.chunkCount, plan.bondCount, plan.orderCount, plan.shapeBytes );
	if ( buffer == NULL || capacity < parts.byteCount )
	{
		nbFreeSet( &set );
		return parts.byteCount;
	}
	nbListPlan( &set, &plan );

	uint8_t* bytes = buffer;
	memset( bytes, 0, parts.shapes );
	nbSaveHeader* header = (nbSaveHeader*)bytes;
	*header = (nbSaveHeader){
		.magic = NB_SAVE_MAGIC,
		.version = NB_SAVE_VERSION,
		.layout = nbSaveLayout(),
		.destructibleCount = set.count,
		.actorCount = plan.actorCount,
		.chunkCount = plan.chunkCount,
		.bondCount = plan.bondCount,
		.orderCount = plan.orderCount,
		.shapeBytes = plan.shapeBytes,
		.byteCount = parts.byteCount,
	};

	nbSavedDestructible* destructibles = (nbSavedDestructible*)( bytes + parts.destructibles );
	nbSavedActor* actors = (nbSavedActor*)( bytes + parts.actors );
	nbSavedChunk* chunks = (nbSavedChunk*)( bytes + parts.chunks );
	nbSavedBond* bonds = (nbSavedBond*)( bytes + parts.bonds );
	int32_t* orders = (int32_t*)( bytes + parts.orders );
	uint8_t* shapes = bytes + parts.shapes;

	int actorPlace = 0;
	int chunkPlace = 0;
	int orderPlace = 0;
	uint64_t shapeOffset = 0;
	for ( int i = 0; i < set.count; ++i )
	{
		const nbDestructible* source = world->destructibles.data + set.indices[i];
		nbSavedDestructible* saved = destructibles + i;
		saved->destructible = *source;
		saved->destructible.staticBody = b3_nullBodyId;
		saved->destructible.headActor = NB_NULL_INDEX;
		saved->destructible.shapeBlock = NULL;
		saved->destructible.shapeBlockSize = 0;
		saved->destructible.shapeBlockCount = 0;
		saved->destructible.generation = 0;
		saved->firstActor = actorPlace;
		saved->actorCount = source->actorCount;

		saved->firstOrder = orderPlace;
#if defined( NB_SHARED_STATIC_BODY )
		if ( B3_IS_NON_NULL( source->staticBody ) )
		{
			orderPlace += nbWriteOrder( world, &plan, source->staticBody, orders + orderPlace );
		}
#endif
		saved->orderCount = orderPlace - saved->firstOrder;

		for ( int a = source->headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
		{
			const nbActor* actor = world->actors.data + a;
			nbSavedActor* savedActor = actors + actorPlace++;
			nbActor* copy = &savedActor->actor;
			*copy = *actor;
			copy->bodyId = b3_nullBodyId;
			copy->destructibleIndex = NB_NULL_INDEX;
			copy->headChunk = NB_NULL_INDEX;
			copy->prevActor = NB_NULL_INDEX;
			copy->nextActor = NB_NULL_INDEX;
			copy->debrisIndex = NB_NULL_INDEX;
			copy->generation = 0;
			copy->settleSlot = 0;
			copy->settleStamp = 0;
			copy->supportStamp = 0;
			copy->answerStamp = 0;
			copy->restStamp = 0;

			const nbActor* holdSource = nbGetHoldSource( world, actor );
			copy->holdSource = actor->holdSource == NB_NULL_INDEX ? NB_NULL_INDEX
							   : holdSource != NULL ? nbFindPlace( &plan.actorPlaces, actor->holdSource )
													: NB_SAVE_STALE;
			copy->holdSourceGeneration = 0;
			for ( int k = 0; k < actor->carrierCount; ++k )
			{
				nbCarrier* carrier = copy->carriers + k;
				const nbChunk* chunk = nbGetCarrierChunk( world, actor->carriers + k );
				carrier->chunkIndex = carrier->chunkIndex == NB_NULL_INDEX ? NB_NULL_INDEX
									  : chunk != NULL ? nbFindPlace( &plan.chunkPlaces, carrier->chunkIndex )
													  : NB_SAVE_STALE;
				carrier->generation = 0;
			}

			savedActor->transform = nbActor_GetTransform( world, actor );
			savedActor->safetyFactor = B3_IS_NON_NULL( actor->bodyId ) ? b3Body_GetSafetyFactor( actor->bodyId ) : 0.0f;
			savedActor->firstChunk = chunkPlace;
			savedActor->chunkCount = actor->chunkCount;
			savedActor->firstOrder = orderPlace;
			if ( B3_IS_NON_NULL( actor->bodyId ) )
			{
				orderPlace += nbWriteOrder( world, &plan, actor->bodyId, orders + orderPlace );
			}
			savedActor->orderCount = orderPlace - savedActor->firstOrder;

			for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
			{
				const nbChunk* chunk = world->chunks.data + c;
				nbSavedChunk* savedChunk = chunks + chunkPlace++;
				nbChunk* chunkCopy = &savedChunk->chunk;
				*chunkCopy = *chunk;
				chunkCopy->shape = NULL;
				chunkCopy->bodyId = b3_nullBodyId;
				chunkCopy->shapeId = b3_nullShapeId;
				chunkCopy->destructibleIndex = NB_NULL_INDEX;
				chunkCopy->actorIndex = NB_NULL_INDEX;
				chunkCopy->prevChunk = NB_NULL_INDEX;
				chunkCopy->nextChunk = NB_NULL_INDEX;
				chunkCopy->headBondKey = nbSaveKey( &plan, chunk->headBondKey );
				chunkCopy->scratch = NB_NULL_INDEX;
				chunkCopy->searchStamp = 0;
				chunkCopy->generation = 0;
				chunkCopy->flags = (uint8_t)( chunk->flags & ( nb_chunkAnchored | nb_chunkStaticBody ) );

				// The shape with its pointers cleared, they point into the allocation it gets when it is loaded
				const nbShape* shape = chunk->shape;
				size_t hullBytes;
				size_t shapeBytes = nbGetShapeBytes( shape, &hullBytes );
				nbShape* shapeCopy = (nbShape*)( shapes + shapeOffset );
				memcpy( shapeCopy, shape, shapeBytes );
				memset( (uint8_t*)shapeCopy + shapeBytes, 0, nbAlignSave( shapeBytes ) - shapeBytes );
				shapeCopy->hull = NULL;
				shapeCopy->vertices = NULL;
				shapeCopy->planes = NULL;
				shapeCopy->faces = NULL;
				shapeCopy->indices = NULL;
				savedChunk->shapeOffset = shapeOffset;
				savedChunk->shapeBytes = (uint32_t)shapeBytes;
				shapeOffset += nbAlignSave( shapeBytes );

				if ( hullBytes > 0 )
				{
					memcpy( shapes + shapeOffset, shape->hull, hullBytes );
					memset( shapes + shapeOffset + hullBytes, 0, nbAlignSave( hullBytes ) - hullBytes );
					savedChunk->hullBytes = (uint32_t)hullBytes;
					shapeOffset += nbAlignSave( hullBytes );
				}
			}
		}
	}
	NB_ASSERT( actorPlace == plan.actorCount && chunkPlace == plan.chunkCount && orderPlace == plan.orderCount );
	NB_ASSERT( shapeOffset == plan.shapeBytes );

	for ( int i = 0; i < plan.bondCount; ++i )
	{
		const nbBond* bond = world->bonds.data + plan.bonds[i];
		nbSavedBond* saved = bonds + i;
		saved->bond = *bond;
		for ( int side = 0; side < 2; ++side )
		{
			saved->bond.chunk[side] = nbFindPlace( &plan.chunkPlaces, bond->chunk[side] );
			saved->bond.prevKey[side] = nbSaveKey( &plan, bond->prevKey[side] );
			saved->bond.nextKey[side] = nbSaveKey( &plan, bond->nextKey[side] );
		}
		saved->bond.stamp = 0;

		const nbDestructible* destructible = world->destructibles.data + world->chunks.data[bond->chunk[0]].destructibleIndex;
		if ( destructible->bondMoments && plan.bonds[i] < world->bondMoments.count )
		{
			saved->moments = world->bondMoments.data[plan.bonds[i]];
		}
	}

	nbFreePlan( &plan );
	nbFreeSet( &set );
	return parts.byteCount;
}

// Read a structure from the buffer, which need not be aligned
#define NB_READ( type, bytes, offset, index )                                                                              \
	( *(const type*)nbReadAt( &( type ){ 0 }, sizeof( type ), ( bytes ), ( offset ), ( index ) ) )

static const void* nbReadAt( void* target, size_t size, const uint8_t* bytes, size_t offset, int index )
{
	memcpy( target, bytes + offset + size * (size_t)index, size );
	return target;
}

// Whether a reference to a place is one of the places or one of the given values
static bool nbIsPlace( int place, int count, bool allowNull, bool allowStale )
{
	return ( 0 <= place && place < count ) || ( allowNull && place == NB_NULL_INDEX ) || ( allowStale && place == NB_SAVE_STALE );
}

// Check what loading relies on, so a buffer that is not what nbSaveDestructibles wrote is refused instead of breaking
// the world: the parts fit, every chunk belongs to one actor and every actor to one destructible, each chunk of a body
// is on its list of shapes once, and every reference is a place of the buffer
static bool nbCheckSaved( const uint8_t* bytes, size_t size, const nbSaveHeader* header, const nbSaveParts* parts )
{
	if ( parts->byteCount > size || header->byteCount != parts->byteCount )
	{
		return false;
	}

	// Per chunk: 0 not seen, 1 on a static actor, 2 on rubble, 3 on a body's list
	uint8_t* kinds = nbAlloc( (size_t)header->chunkCount + 1 );
	memset( kinds, 0, (size_t)header->chunkCount + 1 );
	bool valid = true;
	int actorEnd = 0;
	int chunkEnd = 0;
	for ( int i = 0; valid && i < header->destructibleCount; ++i )
	{
		nbSavedDestructible saved = NB_READ( nbSavedDestructible, bytes, parts->destructibles, i );
		valid = saved.firstActor == actorEnd && saved.actorCount >= 0 && saved.firstActor + saved.actorCount <= header->actorCount &&
				saved.firstOrder >= 0 && saved.orderCount >= 0 && saved.firstOrder + saved.orderCount <= header->orderCount &&
				saved.destructible.materialCount >= 1 && saved.destructible.materialCount <= NB_MAX_MATERIALS &&
				saved.destructible.anchorCount >= 0 && saved.destructible.anchorCount <= NB_MAX_ANCHORS &&
				saved.destructible.storeyCount >= 0 && saved.destructible.storeyCount <= NB_MAX_STOREYS;
		actorEnd += saved.actorCount;
		int firstChunk = chunkEnd;
		int staticCount = 0;

		for ( int a = saved.firstActor; valid && a < saved.firstActor + saved.actorCount; ++a )
		{
			nbSavedActor actor = NB_READ( nbSavedActor, bytes, parts->actors, a );
			bool rubble = actor.actor.isStatic == false;
			valid = actor.firstChunk == chunkEnd && actor.chunkCount >= 0 && actor.firstChunk + actor.chunkCount <= header->chunkCount &&
					actor.firstOrder >= 0 && actor.orderCount >= 0 && actor.firstOrder + actor.orderCount <= header->orderCount &&
					actor.actor.carrierCount >= 0 && actor.actor.carrierCount <= NB_MAX_CARRIERS &&
					nbIsPlace( actor.actor.holdSource, header->actorCount, true, true ) &&
					( rubble == false || ( actor.actor.isRubble && actor.chunkCount > 0 && actor.orderCount == actor.chunkCount ) ) &&
					( rubble || actor.orderCount == 0 );
			chunkEnd += actor.chunkCount;

			for ( int k = 0; valid && k < actor.actor.carrierCount; ++k )
			{
				valid = nbIsPlace( actor.actor.carriers[k].chunkIndex, header->chunkCount, true, true );
			}

			for ( int c = actor.firstChunk; valid && c < actor.firstChunk + actor.chunkCount; ++c )
			{
				nbSavedChunk chunk = NB_READ( nbSavedChunk, bytes, parts->chunks, c );
				if ( chunk.shapeOffset + sizeof( nbShape ) > header->shapeBytes )
				{
					valid = false;
					break;
				}

				nbShape shape;
				memcpy( &shape, bytes + parts->shapes + chunk.shapeOffset, sizeof( nbShape ) );
				nbShapeLayout layout = nbGetShapeLayout( shape.vertexCount, shape.faceCount, shape.indexCount );
				size_t hullBytes = chunk.hullBytes == 0 ? 0 : nbAlignSave( chunk.hullBytes );
				valid = shape.vertexCount >= 4 && shape.faceCount >= 4 && shape.indexCount >= 0 &&
						layout.byteCount == chunk.shapeBytes && ( layout.hullOffset > 0 || chunk.hullBytes > 0 ) &&
						( chunk.hullBytes == 0 || chunk.hullBytes >= sizeof( b3HullData ) ) &&
						chunk.shapeOffset + nbAlignSave( chunk.shapeBytes ) + hullBytes <= header->shapeBytes &&
						chunk.chunk.materialIndex < saved.destructible.materialCount &&
						nbIsPlace( chunk.chunk.headBondKey >> 1, header->bondCount, chunk.chunk.headBondKey == NB_NULL_INDEX, false );
				kinds[c] = rubble ? 2 : 1;
				staticCount += rubble ? 0 : 1;
			}

			// The shapes on the body of rubble are its chunks, each once
			for ( int k = actor.firstOrder; valid && k < actor.firstOrder + actor.orderCount; ++k )
			{
				int c = NB_READ( int32_t, bytes, parts->orders, k );
				valid = actor.firstChunk <= c && c < actor.firstChunk + actor.chunkCount && kinds[c] == 2;
				kinds[valid ? c : 0] = 3;
			}
		}

		valid = valid && chunkEnd - firstChunk == saved.destructible.chunkCount;

		// The shapes on its static body are the chunks of its static actors, each once
#if defined( NB_SHARED_STATIC_BODY )
		valid = valid && saved.orderCount == staticCount;
		for ( int k = saved.firstOrder; valid && k < saved.firstOrder + saved.orderCount; ++k )
		{
			int c = NB_READ( int32_t, bytes, parts->orders, k );
			valid = firstChunk <= c && c < chunkEnd && kinds[c] == 1;
			kinds[valid ? c : 0] = 3;
		}
#else
		valid = valid && saved.orderCount == 0;
		NB_UNUSED( staticCount );
		NB_UNUSED( firstChunk );
#endif
	}
	nbFree( kinds, (size_t)header->chunkCount + 1 );

	if ( valid == false || actorEnd != header->actorCount || chunkEnd != header->chunkCount )
	{
		return false;
	}

	for ( int i = 0; i < header->bondCount; ++i )
	{
		nbSavedBond bond = NB_READ( nbSavedBond, bytes, parts->bonds, i );
		for ( int side = 0; side < 2; ++side )
		{
			int prevKey = bond.bond.prevKey[side];
			int nextKey = bond.bond.nextKey[side];
			if ( nbIsPlace( bond.bond.chunk[side], header->chunkCount, false, false ) == false ||
				 nbIsPlace( prevKey >> 1, header->bondCount, prevKey == NB_NULL_INDEX, false ) == false ||
				 nbIsPlace( nextKey >> 1, header->bondCount, nextKey == NB_NULL_INDEX, false ) == false )
			{
				return false;
			}
		}
	}
	return true;
}

// A bond key with the index of the bond instead of its place
static int nbLoadKey( const int* bondIndices, int key )
{
	return key == NB_NULL_INDEX ? NB_NULL_INDEX : ( bondIndices[key >> 1] << 1 ) | ( key & 1 );
}

bool nbLoadDestructibles( nbWorldId worldId, const void* buffer, size_t size, nbDestructibleId* ids )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL || buffer == NULL || size < sizeof( nbSaveHeader ) )
	{
		return false;
	}

	const uint8_t* bytes = buffer;
	nbSaveHeader header = NB_READ( nbSaveHeader, bytes, 0, 0 );
	if ( header.magic != NB_SAVE_MAGIC || header.version != NB_SAVE_VERSION || header.layout != nbSaveLayout() ||
		 header.destructibleCount <= 0 || header.actorCount < 0 || header.chunkCount < 0 || header.bondCount < 0 ||
		 header.orderCount < 0 || header.shapeBytes > size )
	{
		return false;
	}

	nbSaveParts parts = nbGetSaveParts( header.destructibleCount, header.actorCount, header.chunkCount, header.bondCount,
										header.orderCount, header.shapeBytes );
	if ( nbCheckSaved( bytes, size, &header, &parts ) == false )
	{
		return false;
	}

	nbBeginOperation( world );
	int* destructibleIndices = nbArena_AllocArray( &world->arena, int, header.destructibleCount );
	int* actorIndices = nbArena_AllocArray( &world->arena, int, header.actorCount + 1 );
	int* chunkIndices = nbArena_AllocArray( &world->arena, int, header.chunkCount + 1 );
	int* bondIndices = nbArena_AllocArray( &world->arena, int, header.bondCount + 1 );
	const uint8_t* shapeData = bytes + parts.shapes;

	// The chunks first, their slots give the references of everything else
	for ( int i = 0; i < header.chunkCount; ++i )
	{
		chunkIndices[i] = nbAllocChunk( world );
	}

	for ( int i = 0; i < header.bondCount; ++i )
	{
		int index;
		if ( world->freeBonds.count > 0 )
		{
			index = world->freeBonds.data[--world->freeBonds.count];
		}
		else
		{
			nbBond empty = { 0 };
			nbArray_Push( world->bonds, empty );
			index = world->bonds.count - 1;
		}
		bondIndices[i] = index;
	}

	int firstChunk = 0;
	for ( int d = 0; d < header.destructibleCount; ++d )
	{
		nbSavedDestructible saved = NB_READ( nbSavedDestructible, bytes, parts.destructibles, d );

		int index;
		if ( world->freeDestructibles.count > 0 )
		{
			index = world->freeDestructibles.data[--world->freeDestructibles.count];
		}
		else
		{
			nbDestructible empty = { 0 };
			nbArray_Push( world->destructibles, empty );
			index = world->destructibles.count - 1;
		}
		destructibleIndices[d] = index;

		nbDestructible* destructible = world->destructibles.data + index;
		uint16_t generation = destructible->generation;
		*destructible = saved.destructible;
		destructible->generation = generation;
		destructible->isFree = false;
		destructible->headActor = NB_NULL_INDEX;
		destructible->actorCount = 0;
		destructible->staticBody = b3_nullBodyId;
		world->destructibleCount += 1;

		// The shapes with their hull inside in one block, like those the workers prepare, see nbPackLoadShapes. Its chunks
		// follow on each other in the buffer.
		size_t blockSize = 0;
		int blockCount = 0;
		int chunkEnd = firstChunk + saved.destructible.chunkCount;
		for ( int c = firstChunk; c < chunkEnd; ++c )
		{
			nbSavedChunk savedChunk = NB_READ( nbSavedChunk, bytes, parts.chunks, c );
			if ( savedChunk.hullBytes == 0 )
			{
				blockSize += ( ( (size_t)savedChunk.shapeBytes - 1 ) | ( NB_ALIGNMENT - 1 ) ) + 1;
				blockCount += 1;
			}
		}

		uint8_t* block = blockCount > 0 ? nbAlloc( blockSize ) : NULL;
		destructible->shapeBlock = block;
		destructible->shapeBlockSize = blockSize;
		destructible->shapeBlockCount = blockCount;
		size_t blockOffset = 0;
		int chunkCount = 0;

		// Actors go in at the front of the list of their destructible, so in reverse
		for ( int a = saved.firstActor + saved.actorCount - 1; a >= saved.firstActor; --a )
		{
			nbSavedActor savedActor = NB_READ( nbSavedActor, bytes, parts.actors, a );
			int actorIndex = nbAllocActor( world, index, savedActor.actor.isStatic );
			actorIndices[a] = actorIndex;
		}

		for ( int a = saved.firstActor; a < saved.firstActor + saved.actorCount; ++a )
		{
			nbSavedActor savedActor = NB_READ( nbSavedActor, bytes, parts.actors, a );
			nbActor* actor = world->actors.data + actorIndices[a];
			nbActor kept = *actor;
			*actor = savedActor.actor;
			world->maxCenterOffset = b3MaxFloat( world->maxCenterOffset, b3Length( actor->localCenter ) );
			actor->bodyId = b3_nullBodyId;
			actor->destructibleIndex = index;
			actor->headChunk = NB_NULL_INDEX;
			actor->chunkCount = 0;
			actor->prevActor = kept.prevActor;
			actor->nextActor = kept.nextActor;
			actor->debrisIndex = NB_NULL_INDEX;
			actor->generation = kept.generation;
			actor->isFree = false;

			// The chunks go in at the front of the list of their actor, so in reverse. The volume stays the one it summed up
			// to, in the order the chunks came and went.
			for ( int c = savedActor.firstChunk + savedActor.chunkCount - 1; c >= savedActor.firstChunk; --c )
			{
				nbSavedChunk savedChunk = NB_READ( nbSavedChunk, bytes, parts.chunks, c );
				int chunkIndex = chunkIndices[c];
				nbChunk* chunk = world->chunks.data + chunkIndex;
				uint16_t chunkGeneration = chunk->generation;
				*chunk = savedChunk.chunk;
				chunk->generation = chunkGeneration;
				chunk->bodyId = b3_nullBodyId;
				chunk->shapeId = b3_nullShapeId;
				chunk->destructibleIndex = index;
				chunk->actorIndex = actorIndices[a];
				chunk->prevChunk = NB_NULL_INDEX;
				chunk->nextChunk = actor->headChunk;
				chunk->headBondKey = nbLoadKey( bondIndices, savedChunk.chunk.headBondKey );
				chunk->flags = (uint8_t)( savedChunk.chunk.flags & nb_chunkAnchored );
				if ( actor->headChunk != NB_NULL_INDEX )
				{
					world->chunks.data[actor->headChunk].prevChunk = chunkIndex;
				}
				actor->headChunk = chunkIndex;
				actor->chunkCount += 1;
				chunkCount += 1;

				const uint8_t* source = shapeData + savedChunk.shapeOffset;
				nbShape* shape;
				if ( savedChunk.hullBytes == 0 )
				{
					shape = (nbShape*)( block + blockOffset );
					memcpy( shape, source, savedChunk.shapeBytes );
					nbShape_Rebase( shape, true );
					blockOffset += ( ( (size_t)savedChunk.shapeBytes - 1 ) | ( NB_ALIGNMENT - 1 ) ) + 1;
				}
				else
				{
					shape = nbAlloc( savedChunk.shapeBytes );
					memcpy( shape, source, savedChunk.shapeBytes );
					nbShape_Rebase( shape, false );

					// Box3D clones the hull into an allocation of its own, from a copy on its alignment
					b3HullData* hull = nbArena_Alloc( &world->arena, savedChunk.hullBytes );
					memcpy( hull, source + nbAlignSave( savedChunk.shapeBytes ), savedChunk.hullBytes );
					shape->hull = b3CloneHull( hull );
				}
				chunk->shape = shape;
			}
			NB_ASSERT( actor->chunkCount == savedActor.chunkCount );
		}
		NB_ASSERT( blockOffset == blockSize );
		NB_ASSERT( chunkCount == saved.destructible.chunkCount );
		destructible->chunkCount = chunkCount;
		firstChunk = chunkEnd;
	}

	// Bonds, with their places turned into indices
	for ( int i = 0; i < header.bondCount; ++i )
	{
		nbSavedBond saved = NB_READ( nbSavedBond, bytes, parts.bonds, i );
		nbBond* bond = world->bonds.data + bondIndices[i];
		*bond = saved.bond;
		for ( int side = 0; side < 2; ++side )
		{
			bond->chunk[side] = chunkIndices[saved.bond.chunk[side]];
			bond->prevKey[side] = nbLoadKey( bondIndices, saved.bond.prevKey[side] );
			bond->nextKey[side] = nbLoadKey( bondIndices, saved.bond.nextKey[side] );
		}
		world->bondCount += 1;

		if ( world->destructibles.data[world->chunks.data[bond->chunk[0]].destructibleIndex].bondMoments )
		{
			nbKeepBondMoments( world, bondIndices[i], saved.moments.moments, saved.moments.crossMoments );
		}
	}

	// Carriers and hold sources, now that all chunks and actors have their slots. A reference to what went still refers
	// to nothing: a slot of the set with a generation it no longer has.
	for ( int a = 0; a < header.actorCount; ++a )
	{
		nbSavedActor saved = NB_READ( nbSavedActor, bytes, parts.actors, a );
		nbActor* actor = world->actors.data + actorIndices[a];
		int holdSource = saved.actor.holdSource;
		if ( holdSource == NB_SAVE_STALE )
		{
			actor->holdSource = actorIndices[a];
			actor->holdSourceGeneration = (uint16_t)( actor->generation - 1 );
		}
		else if ( holdSource != NB_NULL_INDEX )
		{
			actor->holdSource = actorIndices[holdSource];
			actor->holdSourceGeneration = world->actors.data[actorIndices[holdSource]].generation;
		}

		for ( int k = 0; k < actor->carrierCount; ++k )
		{
			nbCarrier* carrier = actor->carriers + k;
			int place = saved.actor.carriers[k].chunkIndex;
			if ( place == NB_SAVE_STALE )
			{
				carrier->chunkIndex = chunkIndices[saved.firstChunk];
				carrier->generation = (uint16_t)( world->chunks.data[carrier->chunkIndex].generation - 1 );
			}
			else if ( place != NB_NULL_INDEX )
			{
				carrier->chunkIndex = chunkIndices[place];
				carrier->generation = world->chunks.data[chunkIndices[place]].generation;
			}
		}
	}

	// Box3D bodies and shapes, which go into the tree together, see nbCommitPhysics
#if defined( B3_HAS_SHAPE_BATCH )
	b3World_BeginShapeBatch( world->physicsWorld );
#endif

	for ( int d = 0; d < header.destructibleCount; ++d )
	{
		nbSavedDestructible saved = NB_READ( nbSavedDestructible, bytes, parts.destructibles, d );
		nbDestructible* destructible = world->destructibles.data + destructibleIndices[d];

#if defined( NB_SHARED_STATIC_BODY )
		for ( int k = saved.firstOrder + saved.orderCount - 1; k >= saved.firstOrder; --k )
		{
			int chunkIndex = chunkIndices[NB_READ( int32_t, bytes, parts.orders, k )];
			nbChunk* chunk = world->chunks.data + chunkIndex;
			NB_ASSERT( world->actors.data[chunk->actorIndex].isStatic );
			b3ShapeDef shapeDef = nbMakeShapeDef( destructible, destructible->materials + chunk->materialIndex, true );
			chunk->bodyId = nbGetStaticBody( world, destructible );
			chunk->shapeId = b3CreateHullShape( chunk->bodyId, &shapeDef, chunk->shape->hull );
			chunk->flags |= nb_chunkStaticBody;
			nbMapShape( world, chunk->shapeId, chunkIndex );
		}
#endif

		for ( int a = saved.firstActor; a < saved.firstActor + saved.actorCount; ++a )
		{
			nbSavedActor savedActor = NB_READ( nbSavedActor, bytes, parts.actors, a );
			nbActor* actor = world->actors.data + actorIndices[a];
			if ( actor->isStatic )
			{
#if !defined( NB_SHARED_STATIC_BODY )
				for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
				{
					nbChunk* chunk = world->chunks.data + c;
					b3ShapeDef shapeDef = nbMakeShapeDef( destructible, destructible->materials + chunk->materialIndex, true );
					chunk->bodyId = nbGetStaticBody( world, destructible );
					chunk->shapeId = b3CreateHullShape( chunk->bodyId, &shapeDef, chunk->shape->hull );
					chunk->flags |= nb_chunkStaticBody;
					nbMapShape( world, chunk->shapeId, c );
				}
#endif
				continue;
			}

			// Rubble, a static body that is the body of a dynamic actor in all else, with the sweep it had
			b3BodyDef bodyDef = nbMakeActorBodyDef( world, savedActor.transform );
			bodyDef.type = b3_staticBody;
			actor->bodyId = b3CreateBody( world->physicsWorld, &bodyDef );
			b3Body_SetSafetyFactor( actor->bodyId, savedActor.safetyFactor );
			nbMapBody( world, actor->bodyId, actorIndices[a] );
			for ( int k = savedActor.firstOrder + savedActor.orderCount - 1; k >= savedActor.firstOrder; --k )
			{
				int chunkIndex = chunkIndices[NB_READ( int32_t, bytes, parts.orders, k )];
				nbChunk* chunk = world->chunks.data + chunkIndex;
				NB_ASSERT( chunk->actorIndex == actorIndices[a] );
				b3ShapeDef shapeDef = nbMakeShapeDef( destructible, destructible->materials + chunk->materialIndex, false );
				chunk->bodyId = actor->bodyId;
				chunk->shapeId = b3CreateHullShape( chunk->bodyId, &shapeDef, chunk->shape->hull );
				nbMapShape( world, chunk->shapeId, chunkIndex );
			}
		}
	}

#if defined( B3_HAS_SHAPE_BATCH )
	b3World_EndShapeBatch( world->physicsWorld );
#endif

	// Rubble is debris that does not move. The chunks are reported as created, in the order of the buffer.
	for ( int a = 0; a < header.actorCount; ++a )
	{
		int actorIndex = actorIndices[a];
		nbActor* actor = world->actors.data + actorIndex;
		if ( actor->isRubble )
		{
			world->rubbleCount += 1;
		}
		nbUpdateDebris( world, actorIndex );
	}

	for ( int i = 0; i < header.chunkCount; ++i )
	{
		nbPushEvent( world->createdEvents + world->eventBuffer, nbMakeChunkId( world, chunkIndices[i] ) );
	}
	world->stats.createdChunkCount += header.chunkCount;

	if ( ids != NULL )
	{
		for ( int d = 0; d < header.destructibleCount; ++d )
		{
			int index = destructibleIndices[d];
			ids[d] = (nbDestructibleId){ index + 1, world->worldIndex, world->destructibles.data[index].generation };
		}
	}
	return true;
}
