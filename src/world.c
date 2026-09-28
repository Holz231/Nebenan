// SPDX-License-Identifier: MIT

#include "world.h"

#include "fracture.h"
#include "hull_builder.h"
#include "scheduler.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

static nbWorld nb_worlds[NB_MAX_WORLDS];

nbWorld* nbGetWorld( int index )
{
	NB_ASSERT( 0 <= index && index < NB_MAX_WORLDS );
	return nb_worlds + index;
}

nbWorld* nbGetWorldFromId( nbWorldId id )
{
	if ( id.index1 < 1 || NB_MAX_WORLDS < id.index1 )
	{
		return NULL;
	}

	nbWorld* world = nb_worlds + ( id.index1 - 1 );
	if ( world->inUse == false || world->generation != id.generation )
	{
		return NULL;
	}

	return world;
}

nbChunkId nbMakeChunkId( const nbWorld* world, int chunkIndex )
{
	nbChunkId id = { chunkIndex + 1, world->worldIndex, world->chunks.data[chunkIndex].generation };
	return id;
}

nbChunk* nbGetChunkFromId( nbChunkId id, nbWorld** worldOut )
{
	if ( id.world0 >= NB_MAX_WORLDS || id.index1 < 1 )
	{
		return NULL;
	}

	nbWorld* world = nb_worlds + id.world0;
	if ( world->inUse == false || id.index1 > world->chunks.count )
	{
		return NULL;
	}

	nbChunk* chunk = world->chunks.data + ( id.index1 - 1 );
	if ( chunk->shape == NULL || chunk->generation != id.generation )
	{
		return NULL;
	}

	if ( worldOut != NULL )
	{
		*worldOut = world;
	}
	return chunk;
}

void nbPushEvent( nbChunkIdArray* events, nbChunkId id )
{
	nbArray_Push( *events, id );
}

void nbBeginOperation( nbWorld* world )
{
	nbArena_Reset( &world->arena );
	for ( int i = 0; i < world->workerCount; ++i )
	{
		nbArena_Reset( world->workerArenas + i );
	}
	world->touchedChunks.count = 0;
	world->touchedActors.count = 0;
	world->splitSeeds.count = 0;
}

void nbTouchChunk( nbWorld* world, int chunkIndex )
{
	nbChunk* chunk = world->chunks.data + chunkIndex;
	if ( ( chunk->flags & nb_chunkTouched ) == 0 )
	{
		chunk->flags |= nb_chunkTouched;
		nbArray_Push( world->touchedChunks, chunkIndex );
	}
}

void nbTouchActor( nbWorld* world, int actorIndex )
{
	for ( int i = 0; i < world->touchedActors.count; ++i )
	{
		if ( world->touchedActors.data[i] == actorIndex )
		{
			return;
		}
	}
	nbArray_Push( world->touchedActors, actorIndex );
}

static void nbMapShape( nbWorld* world, b3ShapeId shapeId, int chunkIndex )
{
	int index = shapeId.index1 - 1;
	if ( index >= world->shapeToChunk.count )
	{
		int oldCount = world->shapeToChunk.count;
		nbArray_Reserve( world->shapeToChunk, index + 1 );
		for ( int i = oldCount; i <= index; ++i )
		{
			world->shapeToChunk.data[i] = NB_NULL_INDEX;
		}
		world->shapeToChunk.count = index + 1;
	}
	world->shapeToChunk.data[index] = chunkIndex;
}

static void nbUnmapShape( nbWorld* world, b3ShapeId shapeId )
{
	int index = shapeId.index1 - 1;
	if ( 0 <= index && index < world->shapeToChunk.count )
	{
		world->shapeToChunk.data[index] = NB_NULL_INDEX;
	}
}

int nbFindChunkFromShape( const nbWorld* world, b3ShapeId shapeId )
{
	int index = shapeId.index1 - 1;
	if ( index < 0 || index >= world->shapeToChunk.count )
	{
		return NB_NULL_INDEX;
	}

	int chunkIndex = world->shapeToChunk.data[index];
	if ( chunkIndex == NB_NULL_INDEX )
	{
		return NB_NULL_INDEX;
	}

	// Box3D reuses shape slots, so compare the full id
	const nbChunk* chunk = world->chunks.data + chunkIndex;
	if ( B3_ID_EQUALS( chunk->shapeId, shapeId ) == false )
	{
		return NB_NULL_INDEX;
	}

	return chunkIndex;
}

static void nbMapBody( nbWorld* world, b3BodyId bodyId, int actorIndex )
{
	int index = bodyId.index1 - 1;
	if ( index >= world->bodyToActor.count )
	{
		int oldCount = world->bodyToActor.count;
		nbArray_Reserve( world->bodyToActor, index + 1 );
		for ( int i = oldCount; i <= index; ++i )
		{
			world->bodyToActor.data[i] = NB_NULL_INDEX;
		}
		world->bodyToActor.count = index + 1;
	}
	world->bodyToActor.data[index] = actorIndex;
}

static int nbFindActorFromBody( const nbWorld* world, b3BodyId bodyId )
{
	int index = bodyId.index1 - 1;
	if ( index < 0 || index >= world->bodyToActor.count )
	{
		return NB_NULL_INDEX;
	}

	int actorIndex = world->bodyToActor.data[index];
	if ( actorIndex == NB_NULL_INDEX )
	{
		return NB_NULL_INDEX;
	}

	const nbActor* actor = world->actors.data + actorIndex;
	if ( actor->isFree || B3_ID_EQUALS( actor->bodyId, bodyId ) == false )
	{
		return NB_NULL_INDEX;
	}
	return actorIndex;
}

b3WorldTransform nbActor_GetTransform( const nbWorld* world, const nbActor* actor )
{
	if ( actor->isStatic )
	{
		return world->destructibles.data[actor->destructibleIndex].transform;
	}
	return b3Body_GetTransform( actor->bodyId );
}

void nbCreateActorBody( nbWorld* world, int actorIndex, b3WorldTransform transform )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	bodyDef.position = transform.p;
	bodyDef.rotation = transform.q;
	bodyDef.sleepThreshold = world->def.debrisSleepThreshold;
	b3BodyId bodyId = b3CreateBody( world->physicsWorld, &bodyDef );
	world->actors.data[actorIndex].bodyId = bodyId;
	nbMapBody( world, bodyId, actorIndex );
}

static int nbAllocChunk( nbWorld* world )
{
	int index;
	if ( world->freeChunks.count > 0 )
	{
		index = world->freeChunks.data[--world->freeChunks.count];
	}
	else
	{
		nbChunk empty = { 0 };
		nbArray_Push( world->chunks, empty );
		index = world->chunks.count - 1;
	}

	nbChunk* chunk = world->chunks.data + index;
	uint16_t generation = chunk->generation;
	*chunk = (nbChunk){ 0 };
	chunk->generation = generation;
	chunk->bodyId = b3_nullBodyId;
	chunk->shapeId = b3_nullShapeId;
	chunk->destructibleIndex = NB_NULL_INDEX;
	chunk->actorIndex = NB_NULL_INDEX;
	chunk->prevChunk = NB_NULL_INDEX;
	chunk->nextChunk = NB_NULL_INDEX;
	chunk->headBondKey = NB_NULL_INDEX;
	chunk->scratch = NB_NULL_INDEX;
	world->chunkCount += 1;
	return index;
}

static void nbFreeChunk( nbWorld* world, int chunkIndex )
{
	nbChunk* chunk = world->chunks.data + chunkIndex;
	NB_ASSERT( chunk->headBondKey == NB_NULL_INDEX );
	NB_ASSERT( chunk->actorIndex == NB_NULL_INDEX );
	chunk->shape = NULL;
	chunk->destructibleIndex = NB_NULL_INDEX;
	chunk->generation += 1;
	nbArray_Push( world->freeChunks, chunkIndex );
	world->chunkCount -= 1;
}

int nbCreateChunk( nbWorld* world, int destructibleIndex, int actorIndex, nbShape* shape, int depth, uint8_t interiorMaterial,
				   int materialIndex )
{
	// The hull lives in the scratch arena until the chunk gets its shape, Box3D clones it into its hull database
	b3HullData* hull = nbCreateHullInArena( shape, &world->arena, &world->stats.hullFallbackCount );
	return nbCreateChunkWithHull( world, destructibleIndex, actorIndex, shape, hull, depth, interiorMaterial, materialIndex );
}

int nbCreateChunkWithHull( nbWorld* world, int destructibleIndex, int actorIndex, nbShape* shape, b3HullData* hull, int depth,
						   uint8_t interiorMaterial, int materialIndex )
{
	if ( hull == NULL )
	{
		// Degenerate sliver. It cannot be simulated, so it is dropped.
		nbShape_Destroy( shape );
		return NB_NULL_INDEX;
	}

	int chunkIndex = nbAllocChunk( world );
	nbChunk* chunk = world->chunks.data + chunkIndex;
	nbDestructible* destructible = world->destructibles.data + destructibleIndex;

	chunk->shape = shape;
	chunk->pendingHull = hull;
	chunk->destructibleIndex = destructibleIndex;
	chunk->depth = (uint8_t)( depth < 255 ? depth : 255 );
	chunk->interiorMaterial = interiorMaterial;
	NB_ASSERT( 0 <= materialIndex && materialIndex < destructible->materialCount );
	chunk->materialIndex = (uint8_t)materialIndex;
	chunk->flags = nb_chunkNew;
	if ( nbIsAnchored( destructible, shape ) )
	{
		chunk->flags |= nb_chunkAnchored;
	}

	destructible->chunkCount += 1;
	nbActor_AddChunk( world, actorIndex, chunkIndex );
	nbTouchChunk( world, chunkIndex );
	nbTouchActor( world, actorIndex );
	nbArray_Push( world->splitSeeds, chunkIndex );
	nbPushEvent( world->createdEvents + world->eventBuffer, nbMakeChunkId( world, chunkIndex ) );
	world->stats.createdChunkCount += 1;
	return chunkIndex;
}

const nbMaterial* nbGetChunkMaterial( const nbWorld* world, const nbChunk* chunk )
{
	return world->destructibles.data[chunk->destructibleIndex].materials + chunk->materialIndex;
}

float nbGetBondStrength( const nbWorld* world, const nbBond* bond )
{
	const nbMaterial* a = nbGetChunkMaterial( world, world->chunks.data + bond->chunk[0] );
	const nbMaterial* b = nbGetChunkMaterial( world, world->chunks.data + bond->chunk[1] );
	return b3MinFloat( a->strength, b->strength );
}

int nbCreateBond( nbWorld* world, int chunkA, int chunkB, const nbBondGeometry* geometry, float health )
{
	NB_ASSERT( chunkA != chunkB );

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

	nbBond* bond = world->bonds.data + index;
	bond->chunk[0] = chunkA;
	bond->chunk[1] = chunkB;
	bond->centroid = geometry->centroid;
	bond->area = geometry->area;
	bond->normal = geometry->normal;
	bond->moments = geometry->moments;
	bond->crossMoments = geometry->crossMoments;
	bond->health = health;
	bond->stamp = 0;
	bond->cohesive = false;
	bond->sibling = false;

	for ( int side = 0; side < 2; ++side )
	{
		nbChunk* chunk = world->chunks.data + bond->chunk[side];
		int key = ( index << 1 ) | side;
		bond->prevKey[side] = NB_NULL_INDEX;
		bond->nextKey[side] = chunk->headBondKey;
		if ( chunk->headBondKey != NB_NULL_INDEX )
		{
			nbBond* head = world->bonds.data + ( chunk->headBondKey >> 1 );
			head->prevKey[chunk->headBondKey & 1] = key;
		}
		chunk->headBondKey = key;
		chunk->bondCount += 1;
	}

	world->bondCount += 1;
	return index;
}

void nbDestroyBond( nbWorld* world, int bondIndex )
{
	nbBond* bond = world->bonds.data + bondIndex;
	NB_ASSERT( bond->chunk[0] != NB_NULL_INDEX );

	for ( int side = 0; side < 2; ++side )
	{
		int chunkIndex = bond->chunk[side];
		nbChunk* chunk = world->chunks.data + chunkIndex;
		int prevKey = bond->prevKey[side];
		int nextKey = bond->nextKey[side];

		if ( prevKey != NB_NULL_INDEX )
		{
			world->bonds.data[prevKey >> 1].nextKey[prevKey & 1] = nextKey;
		}
		else
		{
			chunk->headBondKey = nextKey;
		}

		if ( nextKey != NB_NULL_INDEX )
		{
			world->bonds.data[nextKey >> 1].prevKey[nextKey & 1] = prevKey;
		}

		chunk->bondCount -= 1;

		// A face the bond covered may be visible now
		if ( ( chunk->flags & nb_chunkExposed ) == 0 )
		{
			chunk->flags |= nb_chunkExposed;
			nbPushEvent( world->exposedEvents + world->eventBuffer, nbMakeChunkId( world, chunkIndex ) );
		}

		// Both ends may have lost their path to an anchor
		nbArray_Push( world->splitSeeds, chunkIndex );
		if ( chunk->actorIndex != NB_NULL_INDEX )
		{
			nbTouchActor( world, chunk->actorIndex );
		}
	}

	bond->chunk[0] = NB_NULL_INDEX;
	bond->chunk[1] = NB_NULL_INDEX;
	nbArray_Push( world->freeBonds, bondIndex );
	world->bondCount -= 1;
}

int nbAllocActor( nbWorld* world, int destructibleIndex, bool isStatic )
{
	int index;
	if ( world->freeActors.count > 0 )
	{
		index = world->freeActors.data[--world->freeActors.count];
	}
	else
	{
		nbActor empty = { 0 };
		nbArray_Push( world->actors, empty );
		index = world->actors.count - 1;
	}

	nbActor* actor = world->actors.data + index;
	uint16_t generation = actor->generation;
	*actor = (nbActor){ 0 };
	actor->generation = generation;
	actor->bodyId = b3_nullBodyId;
	actor->destructibleIndex = destructibleIndex;
	actor->headChunk = NB_NULL_INDEX;
	actor->debrisIndex = NB_NULL_INDEX;
	actor->isStatic = isStatic;
	actor->isNew = true;
	actor->holdSource = NB_NULL_INDEX;

	nbDestructible* destructible = world->destructibles.data + destructibleIndex;
	actor->prevActor = NB_NULL_INDEX;
	actor->nextActor = destructible->headActor;
	if ( destructible->headActor != NB_NULL_INDEX )
	{
		world->actors.data[destructible->headActor].prevActor = index;
	}
	destructible->headActor = index;
	destructible->actorCount += 1;

	if ( isStatic )
	{
		world->staticActorCount += 1;
	}
	else
	{
		world->dynamicActorCount += 1;
	}

	return index;
}

static void nbRemoveDebris( nbWorld* world, int actorIndex )
{
	nbActor* actor = world->actors.data + actorIndex;
	int debrisIndex = actor->debrisIndex;
	if ( debrisIndex == NB_NULL_INDEX )
	{
		return;
	}

	int last = world->debris.count - 1;
	int movedActor = world->debris.data[last];
	world->debris.data[debrisIndex] = movedActor;
	world->actors.data[movedActor].debrisIndex = debrisIndex;
	world->debris.count -= 1;
	actor->debrisIndex = NB_NULL_INDEX;
}

void nbFreeActor( nbWorld* world, int actorIndex )
{
	nbActor* actor = world->actors.data + actorIndex;
	NB_ASSERT( actor->isFree == false );
	NB_ASSERT( actor->chunkCount == 0 );

	if ( actor->isRubble )
	{
		actor->isRubble = false;
		world->rubbleCount -= 1;
	}

	if ( B3_IS_NON_NULL( actor->bodyId ) )
	{
		int index = actor->bodyId.index1 - 1;
		if ( index < world->bodyToActor.count && world->bodyToActor.data[index] == actorIndex )
		{
			world->bodyToActor.data[index] = NB_NULL_INDEX;
		}

		if ( b3Body_IsValid( actor->bodyId ) )
		{
			b3DestroyBody( actor->bodyId );
		}
		actor->bodyId = b3_nullBodyId;
	}

	nbRemoveDebris( world, actorIndex );

	nbDestructible* destructible = world->destructibles.data + actor->destructibleIndex;
	if ( actor->prevActor != NB_NULL_INDEX )
	{
		world->actors.data[actor->prevActor].nextActor = actor->nextActor;
	}
	else
	{
		destructible->headActor = actor->nextActor;
	}

	if ( actor->nextActor != NB_NULL_INDEX )
	{
		world->actors.data[actor->nextActor].prevActor = actor->prevActor;
	}
	destructible->actorCount -= 1;

	if ( actor->isStatic )
	{
		world->staticActorCount -= 1;
	}
	else
	{
		world->dynamicActorCount -= 1;
	}

	actor->isFree = true;
	actor->generation += 1;
	nbArray_Push( world->freeActors, actorIndex );
}

void nbActor_AddChunk( nbWorld* world, int actorIndex, int chunkIndex )
{
	nbActor* actor = world->actors.data + actorIndex;
	nbChunk* chunk = world->chunks.data + chunkIndex;
	NB_ASSERT( chunk->actorIndex == NB_NULL_INDEX );

	chunk->actorIndex = actorIndex;
	chunk->prevChunk = NB_NULL_INDEX;
	chunk->nextChunk = actor->headChunk;
	if ( actor->headChunk != NB_NULL_INDEX )
	{
		world->chunks.data[actor->headChunk].prevChunk = chunkIndex;
	}
	actor->headChunk = chunkIndex;
	actor->chunkCount += 1;
	actor->volume += chunk->shape->volume;
	actor->massDirty = true;
}

void nbActor_RemoveChunk( nbWorld* world, int actorIndex, int chunkIndex )
{
	nbActor* actor = world->actors.data + actorIndex;
	nbChunk* chunk = world->chunks.data + chunkIndex;
	NB_ASSERT( chunk->actorIndex == actorIndex );

	if ( chunk->prevChunk != NB_NULL_INDEX )
	{
		world->chunks.data[chunk->prevChunk].nextChunk = chunk->nextChunk;
	}
	else
	{
		actor->headChunk = chunk->nextChunk;
	}

	if ( chunk->nextChunk != NB_NULL_INDEX )
	{
		world->chunks.data[chunk->nextChunk].prevChunk = chunk->prevChunk;
	}

	chunk->actorIndex = NB_NULL_INDEX;
	chunk->prevChunk = NB_NULL_INDEX;
	chunk->nextChunk = NB_NULL_INDEX;
	actor->chunkCount -= 1;
	actor->volume -= chunk->shape->volume;
	actor->massDirty = true;
}

// Remove the physics of a chunk. With destroyBodies false the caller destroys the shared body.
static void nbRemoveChunkPhysics( nbWorld* world, nbChunk* chunk, bool destroyBodies )
{
	if ( B3_IS_NON_NULL( chunk->shapeId ) )
	{
		nbUnmapShape( world, chunk->shapeId );
		if ( chunk->flags & nb_chunkOwnsBody )
		{
			if ( b3Body_IsValid( chunk->bodyId ) )
			{
				b3DestroyBody( chunk->bodyId );
			}
		}
		else if ( destroyBodies && b3Shape_IsValid( chunk->shapeId ) )
		{
			b3DestroyShape( chunk->shapeId, false );
		}
	}

	chunk->shapeId = b3_nullShapeId;
	chunk->bodyId = b3_nullBodyId;
	chunk->flags &= ~nb_chunkOwnsBody;
}

// Release everything of a chunk except its physics
static void nbReleaseChunk( nbWorld* world, int chunkIndex )
{
	nbChunk* chunk = world->chunks.data + chunkIndex;

	// A destroyed chunk is not reported as exposed
	chunk->flags |= nb_chunkExposed;
	while ( chunk->headBondKey != NB_NULL_INDEX )
	{
		nbDestroyBond( world, chunk->headBondKey >> 1 );
	}

	if ( chunk->actorIndex != NB_NULL_INDEX )
	{
		nbTouchActor( world, chunk->actorIndex );
		nbActor_RemoveChunk( world, chunk->actorIndex, chunkIndex );
	}

	nbDestructible* destructible = world->destructibles.data + chunk->destructibleIndex;
	destructible->chunkCount -= 1;

	// Chunks created and destroyed in the same event window are reported as both
	nbPushEvent( world->destroyedEvents + world->eventBuffer, nbMakeChunkId( world, chunkIndex ) );

	// Scratch memory, released with the arena
	chunk->pendingHull = NULL;

	nbShape_Destroy( chunk->shape );
	chunk->shape = NULL;
	nbFreeChunk( world, chunkIndex );
}

void nbDestroyChunk( nbWorld* world, int chunkIndex )
{
	nbRemoveChunkPhysics( world, world->chunks.data + chunkIndex, true );
	nbReleaseChunk( world, chunkIndex );
}

void nbDestroyActor( nbWorld* world, int actorIndex )
{
	// Destroying a dynamic body removes all of its shapes at once, which is much cheaper than one by one
	while ( world->actors.data[actorIndex].headChunk != NB_NULL_INDEX )
	{
		int chunkIndex = world->actors.data[actorIndex].headChunk;
		nbRemoveChunkPhysics( world, world->chunks.data + chunkIndex, false );
		nbReleaseChunk( world, chunkIndex );
	}

	nbFreeActor( world, actorIndex );
}

bool nbIsAnchored( const nbDestructible* destructible, const nbShape* shape )
{
	if ( destructible->isStatic == false )
	{
		return false;
	}

	for ( int i = 0; i < destructible->anchorCount; ++i )
	{
		const nbAnchorPlane* anchor = destructible->anchors + i;
		float limit = anchor->offset + 1.0e-3f;
		for ( int v = 0; v < shape->vertexCount; ++v )
		{
			if ( b3Dot( anchor->normal, shape->vertices[v] ) <= limit )
			{
				return true;
			}
		}
	}

	return false;
}

// World bounds of the shapes of an actor, a little larger so they reach what rests on them
static b3AABB nbGetActorBounds( const nbWorld* world, const nbActor* actor )
{
	b3AABB box = { { FLT_MAX, FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX, -FLT_MAX } };
	for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
	{
		b3ShapeId shapeId = world->chunks.data[c].shapeId;
		if ( b3Shape_IsValid( shapeId ) )
		{
			box = b3AABB_Union( box, b3Shape_GetAABB( shapeId ) );
		}
	}

	b3Vec3 margin = { 0.05f, 0.05f, 0.05f };
	box.lowerBound = b3Sub( box.lowerBound, margin );
	box.upperBound = b3Add( box.upperBound, margin );
	return box;
}

// Rest detection and settling follow the reference engine (src/rubble_rest.h and BuildingScene::settle on the Referenz
// branch). A piece is quiet once it stayed within NB_REST_DISTANCE of one pose for NB_REST_TIME seconds, counting
// rotation as the way its farthest corner moves.
#define NB_REST_DISTANCE 0.02f
#define NB_REST_TIME 0.2f

// A body the solver keeps rocking in place is quiet too, once the means of NB_JITTER_WINDOW seconds stayed within
// NB_JITTER_MATCH of each other NB_JITTER_MATCHES times in a row. It may not wander off its first pose, though.
#define NB_JITTER_WINDOW 0.5f
#define NB_JITTER_MATCH 0.003f
#define NB_JITTER_MATCHES 3

// A contact carries a piece when its normal points down out of the piece by more than this, and its closest point is
// within NB_TOUCH_GAP.
#define NB_SUPPORT_NORMAL 0.1f
#define NB_TOUCH_GAP 0.005f

// Rubble resting on a piece comes back to life once that piece moved this far from where it lay
#define NB_HOLD_DISTANCE 0.05f

// Rubble bodies that pieces moving away release per update at most. The others follow in the next updates.
#define NB_MAX_RELEASES 32

// Rubble bodies the search for what carries rubble visits at most, see nbIsSupportedWithout
#define NB_SUPPORT_SEARCH 64

// A chunk still carries rubble while it is within this distance of the point where the rubble lay on it
#define NB_CARRIER_SLACK 0.05f

// Pieces of at least this volume in cubic meters freeze only on rubble that something else carries, see
// nbIsSupportedWithout. Smaller ones freeze on whatever they lie on.
#define NB_CHECKED_VOLUME 0.1f

// A support point this close to the center of mass, seen from above, balances a piece on its own, see nbIsBalanced
#define NB_BALANCE_RADIUS 0.05f

// A body that is not debris brings the rubble it hits this fast back to life. An impact brings back the rubble it would
// push at least this fast, see nbImpactMoves.
#define NB_WAKE_SPEED 1.0f

// Rubble bodies that hits bring back to life per update at most
#define NB_MAX_RUBBLE_HITS 16

// Debris that touches nothing freezes to keep within the budget only when it flies at least this fast, see
// nbEnforceDebrisBudget
#define NB_FLIGHT_SPEED 1.0f

// A larger moving body that presses this deep into a piece keeps it from freezing, unless the body freezes with it.
// Frozen alone, the piece would push that body out of it at once and throw it: a part of a building that came down onto
// the small pieces it squeezes.
#define NB_SQUEEZE_DEPTH 0.02f

// Debris younger than this in seconds does not freeze to keep within the budget, and neither does the debris around a
// storey that gives way for as long, see nbThawContext::collapse
#define NB_FREEZE_GRACE 0.25f

// Rubble that comes back to life and stays slower than this for NB_PROBE_TIME is still carried and freezes again. A body
// that lost its support is past this after one step of free fall.
#define NB_PROBE_SPEED 0.05f
#define NB_PROBE_TIME 0.05f

// How far a body moved from a pose: the translation plus the way its farthest corner moved with the rotation
static float nbPoseDistance( b3WorldTransform from, b3WorldTransform to, float radius )
{
	b3Quat a = from.q;
	b3Quat b = to.q;
	float cosine = b3ClampFloat( b3AbsFloat( a.v.x * b.v.x + a.v.y * b.v.y + a.v.z * b.v.z + a.s * b.s ), 0.0f, 1.0f );
	return b3Length( b3SubPos( to.p, from.p ) ) + 2.0f * radius * sqrtf( b3MaxFloat( 1.0f - cosine * cosine, 0.0f ) );
}

static void nbResetRest( nbActor* actor )
{
	actor->restTime = 0.0f;
	actor->jitterTime = 0.0f;
	actor->jitterSum = b3Vec3_zero;
	actor->jitterMatches = 0;
	actor->jitterSampled = false;
}

// The mean position over half second windows. Position correction can make a wedged piece alternate between poses long
// after the solver removed its motion, which must not keep it awake. Steady drift or turning is not rocking.
static bool nbIsRocking( nbActor* actor, b3WorldTransform transform, float timeStep )
{
	if ( actor->jitterTime == 0.0f && actor->jitterSampled == false )
	{
		actor->jitterPose = transform;
	}

	b3Vec3 offset = b3SubPos( transform.p, actor->jitterPose.p );
	float envelope = b3ClampFloat( 0.25f * actor->radius, 0.02f, 0.1f );
	float turn = nbPoseDistance( actor->jitterPose, (b3WorldTransform){ actor->jitterPose.p, transform.q }, actor->radius );
	if ( b3Length( offset ) > envelope || turn > 0.04f )
	{
		nbResetRest( actor );
		actor->jitterPose = transform;
		offset = b3Vec3_zero;
	}

	actor->jitterSum = b3MulAdd( actor->jitterSum, timeStep, offset );
	actor->jitterTime += timeStep;
	if ( actor->jitterTime >= NB_JITTER_WINDOW )
	{
		b3Vec3 mean = b3MulSV( 1.0f / actor->jitterTime, actor->jitterSum );
		bool match = actor->jitterSampled && b3Distance( mean, actor->jitterMean ) <= NB_JITTER_MATCH;
		actor->jitterMatches = match ? actor->jitterMatches + 1 : 0;
		actor->jitterMean = mean;
		actor->jitterSampled = true;
		actor->jitterTime = 0.0f;
		actor->jitterSum = b3Vec3_zero;
	}
	return actor->jitterMatches >= NB_JITTER_MATCHES;
}

// A piece is quiet when it is slow and stayed in place, or rocks in place. Box3D islands that fell asleep are quiet.
static bool nbIsQuiet( const nbWorld* world, nbActor* actor, float timeStep )
{
	if ( b3Body_IsAwake( actor->bodyId ) == false )
	{
		return true;
	}

	b3WorldTransform transform = b3Body_GetTransform( actor->bodyId );
	float slow = 4.0f * world->def.debrisSleepThreshold;
	float speed = b3Length( b3Body_GetLinearVelocity( actor->bodyId ) );
	float spin = b3Length( b3Body_GetAngularVelocity( actor->bodyId ) ) * actor->radius;

	// Rubble that came back to life, when a piece under it moved away or an impact went off nearby, and did not start to
	// move is still carried. It need not wait the whole rest time, a house on its stubs would cost hundreds of contacts
	// meanwhile.
	bool probed = false;
	if ( actor->probing )
	{
		actor->probing = speed <= NB_PROBE_SPEED && spin <= NB_PROBE_SPEED;
		actor->probeTime += timeStep;
		probed = actor->probing && actor->probeTime >= NB_PROBE_TIME;
	}

	if ( speed > 2.0f * slow || spin > 2.0f * slow )
	{
		nbResetRest( actor );
		return false;
	}

	// A brief solver spike keeps the rocking history, only a fast body loses it
	bool rocking = nbIsRocking( actor, transform, timeStep );
	if ( speed > slow || spin > slow )
	{
		actor->restTime = 0.0f;
		return false;
	}

	if ( actor->restTime == 0.0f || nbPoseDistance( actor->restPose, transform, actor->radius ) > NB_REST_DISTANCE )
	{
		actor->restPose = transform;
		actor->restTime = 0.0f;
	}
	actor->restTime = b3MinFloat( actor->restTime + timeStep, 1.0f );
	return actor->restTime >= NB_REST_TIME || rocking || probed;
}

// Mass, center of mass and reach of a dynamic actor from its shapes
static void nbUpdateActorMass( nbWorld* world, nbActor* actor )
{
	b3Body_ApplyMassFromShapes( actor->bodyId );
	actor->localCenter = b3Body_GetLocalCenter( actor->bodyId );
	actor->radius = 0.0f;
	for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
	{
		const nbShape* shape = world->chunks.data[c].shape;
		actor->radius = b3MaxFloat( actor->radius, b3Distance( shape->centroid, actor->localCenter ) + shape->radius );
	}
	actor->massDirty = false;
}

// The rubble resting on this actor comes back to life once it moves away from where it is now
static void nbHoldRubble( nbWorld* world, nbActor* actor )
{
	actor->holdsRubble = true;
	actor->holdPose = b3Body_GetTransform( actor->bodyId );
	actor->holdBounds = nbGetActorBounds( world, actor );
}

// Debris at rest becomes static and leaves the island it was part of. Box3D wakes that island once when the first of its
// bodies changes type, the others freeze in the same update.
static void nbFreezeActor( nbWorld* world, int actorIndex )
{
	nbActor* actor = world->actors.data + actorIndex;
	b3Body_SetType( actor->bodyId, b3_staticBody );
	actor->isRubble = true;
	actor->holdsRubble = false;
	actor->probing = false;
	actor->carrierCount = 0;
	nbResetRest( actor );
	world->rubbleCount += 1;
}

// Bring rubble back to life. The rubble resting on it stays until it moves, then follows. Box3D gives the body its mass
// back, the center of mass moved if impacts broke pieces off the rubble.
void nbThawActor( nbWorld* world, int actorIndex )
{
	nbActor* actor = world->actors.data + actorIndex;
	NB_ASSERT( actor->isRubble );
	actor->isRubble = false;
	world->rubbleCount -= 1;
	b3Body_SetType( actor->bodyId, b3_dynamicBody );
	if ( actor->massDirty )
	{
		nbUpdateActorMass( world, actor );
	}
	b3Body_SetAwake( actor->bodyId, true );
	nbResetRest( actor );
	actor->probing = true;
	actor->probeTime = 0.0f;
	nbHoldRubble( world, actor );
}

typedef struct nbThawContext
{
	nbWorld* world;
	nbIntArray* actors;

	// With boxes, only rubble that overlaps one of them
	const b3AABB* boxes;
	int boxCount;

	// With a direction, only rubble whose lowest point lies higher along it than the height
	b3Vec3 up;
	float height;

	// For an impact, only rubble it moves, see nbImpactMoves
	bool impact;
	float ejectSpeed;

	// Rubble to leave alone
	int skipActor;
	uint16_t skipGeneration;

	// Rubble that froze on this actor follows it wherever it lies, see nbRestsOn
	int holder;

	// The rubble that comes back to life gets the grace of new debris, see NB_FREEZE_GRACE. What lay on a piece that
	// moved away would freeze again where it lay.
	bool grace;

	// A storey gives way: all the rubble comes back to life, also past NB_MAX_THAW, and all the debris there gets the
	// grace, what lay on the storey too
	bool collapse;
} nbThawContext;

// Whether rubble froze on one of the chunks of an actor
static bool nbRestsOn( const nbWorld* world, const nbActor* rubble, int actorIndex )
{
	for ( int k = 0; k < rubble->carrierCount; ++k )
	{
		const nbCarrier* carrier = rubble->carriers + k;
		if ( carrier->chunkIndex != NB_NULL_INDEX && world->chunks.data[carrier->chunkIndex].generation == carrier->generation &&
			 world->chunks.data[carrier->chunkIndex].actorIndex == actorIndex )
		{
			return true;
		}
	}
	return false;
}

// Whether a chunk is still where rubble lay on it, within NB_CARRIER_SLACK of the point. A chunk that moved away since, a
// piece that broke off below and fell, carries nothing.
static bool nbStillCarries( const nbChunk* chunk, b3Vec3 point )
{
	b3AABB box = b3Shape_GetAABB( chunk->shapeId );
	float slack = NB_CARRIER_SLACK;
	return box.lowerBound.x - slack <= point.x && point.x <= box.upperBound.x + slack && box.lowerBound.y - slack <= point.y &&
		   point.y <= box.upperBound.y + slack && box.lowerBound.z - slack <= point.z && point.z <= box.upperBound.z + slack;
}

// Whether rubble lies on something that holds without an actor, following the chunks each rubble body froze on. Chunks
// that are gone, moved away or part of the actor carry nothing, and so do moving bodies that rest on the actor, see
// nbFindSupport, and rubble frozen in flight without a record. Everything else counts as carried: the ground, a structure,
// other moving bodies, and rubble whose search runs past the limit.
static bool nbIsSupportedWithout( nbWorld* world, int rubbleIndex, int actorIndex, uint32_t restStamp )
{
	int stack[NB_SUPPORT_SEARCH];
	int count = 0;
	int visits = 0;
	world->supportStamp += 1;
	uint32_t stamp = world->supportStamp;
	world->actors.data[rubbleIndex].supportStamp = stamp;
	stack[count++] = rubbleIndex;
	while ( count > 0 )
	{
		const nbActor* rubble = world->actors.data + stack[--count];
		if ( ++visits > NB_SUPPORT_SEARCH )
		{
			return true;
		}

		for ( int k = 0; k < rubble->carrierCount; ++k )
		{
			const nbCarrier* record = rubble->carriers + k;
			if ( record->chunkIndex == NB_NULL_INDEX )
			{
				return true;
			}

			const nbChunk* chunk = world->chunks.data + record->chunkIndex;
			if ( chunk->generation != record->generation || chunk->actorIndex == actorIndex ||
				 nbStillCarries( chunk, record->point ) == false )
			{
				continue;
			}

			nbActor* carrier = world->actors.data + chunk->actorIndex;
			if ( carrier->isRubble == false )
			{
				if ( carrier->isStatic || carrier->restStamp != restStamp )
				{
					return true;
				}
				continue;
			}

			if ( carrier->supportStamp != stamp )
			{
				if ( count == NB_SUPPORT_SEARCH )
				{
					return true;
				}
				carrier->supportStamp = stamp;
				stack[count++] = chunk->actorIndex;
			}
		}
	}
	return false;
}

static bool nbThawCallback( b3ShapeId shapeId, void* context )
{
	nbThawContext* thawContext = context;
	nbWorld* world = thawContext->world;
	int chunkIndex = nbFindChunkFromShape( world, shapeId );
	if ( chunkIndex == NB_NULL_INDEX )
	{
		return true;
	}

	int actorIndex = world->chunks.data[chunkIndex].actorIndex;
	nbActor* actor = world->actors.data + actorIndex;
	if ( thawContext->collapse && actor->isStatic == false )
	{
		actor->budgetAge = 0.0f;
	}

	if ( actor->isRubble == false || actor->settleStamp == world->settleStamp ||
		 ( actorIndex == thawContext->skipActor && actor->generation == thawContext->skipGeneration ) )
	{
		return true;
	}

	if ( thawContext->impact && nbImpactMoves( world, actor, thawContext->ejectSpeed ) == false )
	{
		return true;
	}

	if ( thawContext->boxCount > 0 )
	{
		b3AABB shapeBox = b3Shape_GetAABB( shapeId );
		bool inside = false;
		for ( int i = 0; i < thawContext->boxCount && inside == false; ++i )
		{
			inside = b3AABB_Overlaps( shapeBox, thawContext->boxes[i] );
		}
		if ( inside == false )
		{
			return true;
		}
	}

	if ( b3LengthSquared( thawContext->up ) > 0.0f )
	{
		// Only rubble lying on top: the lowest point of the shape box lies above the height
		b3AABB shapeBox = b3Shape_GetAABB( shapeId );
		b3Vec3 center = b3MulSV( 0.5f, b3Add( shapeBox.lowerBound, shapeBox.upperBound ) );
		b3Vec3 extents = b3MulSV( 0.5f, b3Sub( shapeBox.upperBound, shapeBox.lowerBound ) );
		b3Vec3 up = thawContext->up;
		float bottom = b3Dot( up, center ) - b3Dot( b3Abs( up ), extents );
		if ( bottom <= thawContext->height &&
			 ( thawContext->holder == NB_NULL_INDEX || nbRestsOn( world, actor, thawContext->holder ) == false ) )
		{
			return true;
		}
	}

	// Collect each actor once
	actor->settleStamp = world->settleStamp;
	nbArray_Push( *thawContext->actors, actorIndex );
	return true;
}

// Rubble in the boxes, in the order Box3D finds it. Box3D's trees are deterministic, so is the order. Many boxes are
// searched with one query over all of them. With a direction, only the rubble above the height.
static void nbThawRubbleInBoxes( nbWorld* world, const b3AABB* boxes, int boxCount, b3Vec3 up, float height,
								 const nbThawContext* filter )
{
	if ( world->rubbleCount == 0 || boxCount == 0 )
	{
		return;
	}

	b3AABB total = boxes[0];
	for ( int i = 1; i < boxCount; ++i )
	{
		total = b3AABB_Union( total, boxes[i] );
	}

	world->settleStamp += 1;
	nbIntArray* thawed = &world->actorList;
	thawed->count = 0;
	nbThawContext context = {
		.world = world,
		.actors = thawed,
		.boxes = boxCount > 1 ? boxes : NULL,
		.boxCount = boxCount > 1 ? boxCount : 0,
		.up = up,
		.height = height,
		.impact = filter != NULL && filter->impact,
		.ejectSpeed = filter != NULL ? filter->ejectSpeed : 0.0f,
		.skipActor = filter != NULL ? filter->skipActor : NB_NULL_INDEX,
		.skipGeneration = filter != NULL ? filter->skipGeneration : 0,
		.holder = filter != NULL ? filter->holder : NB_NULL_INDEX,
		.collapse = filter != NULL && filter->collapse,
	};
	b3World_OverlapAABB( world->physicsWorld, total, b3DefaultQueryFilter(), nbThawCallback, &context );

	// Past the limit the rest stays rubble
	int limit = filter != NULL && filter->collapse ? thawed->count : NB_MAX_THAW;
	for ( int i = 0; i < thawed->count && i < limit; ++i )
	{
		nbThawActor( world, thawed->data[i] );
		if ( filter != NULL && filter->grace )
		{
			world->actors.data[thawed->data[i]].budgetAge = 0.0f;
		}
	}
	thawed->count = 0;
}

float nbGetImpactPush( const nbWorld* world, const nbActor* actor, float ejectSpeed )
{
	float fragmentSize = nbGetFragmentSize( world, nbGetChunkMaterial( world, world->chunks.data + actor->headChunk ) );
	float fragmentVolume = fragmentSize * fragmentSize * fragmentSize;
	return 0.5f * ejectSpeed * b3MinFloat( 1.0f, 4.0f * fragmentVolume / b3MaxFloat( actor->volume, 1.0e-9f ) );
}

// Rubble an impact cannot move stays rubble, only the chunks it breaks off fly. Otherwise a house that came off its
// anchors and stands on its stubs would come back to life with its hundreds of contacts for every grenade that hits it.
bool nbImpactMoves( const nbWorld* world, const nbActor* actor, float ejectSpeed )
{
	return nbGetImpactPush( world, actor, ejectSpeed ) >= NB_WAKE_SPEED;
}

void nbThawRubble( nbWorld* world, b3AABB box, float ejectSpeed )
{
	nbThawContext filter = { .impact = true, .ejectSpeed = ejectSpeed, .skipActor = NB_NULL_INDEX, .holder = NB_NULL_INDEX };
	nbThawRubbleInBoxes( world, &box, 1, b3Vec3_zero, 0.0f, &filter );
}

void nbUpdateDebris( nbWorld* world, int actorIndex )
{
	nbActor* actor = world->actors.data + actorIndex;
	bool isDebris = actor->isStatic == false && actor->isFree == false && actor->chunkCount > 0;
	if ( isDebris && actor->debrisIndex == NB_NULL_INDEX )
	{
		actor->debrisIndex = world->debris.count;
		nbArray_Push( world->debris, actorIndex );
	}
	else if ( isDebris == false && actor->debrisIndex != NB_NULL_INDEX )
	{
		nbRemoveDebris( world, actorIndex );
	}
}

// Queue a static actor, or a part of a building that came down, for a check in the next update
static void nbMarkSupport( nbWorld* world, int actorIndex )
{
	nbActor* actor = world->actors.data + actorIndex;
	if ( actor->supportDirty == false )
	{
		actor->supportDirty = true;
		nbArray_Push( world->supportChecks, actorIndex );
	}
}

static b3ShapeDef nbMakeShapeDef( const nbDestructible* destructible, const nbMaterial* material, bool isStatic )
{
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.density = material->density;
	shapeDef.baseMaterial.friction = material->friction;
	shapeDef.baseMaterial.restitution = material->restitution;
	shapeDef.baseMaterial.userMaterialId = material->userMaterialId;
	shapeDef.filter = destructible->filter;
	shapeDef.enableHitEvents = destructible->enableCollisionDamage && isStatic == false;
	shapeDef.updateBodyMass = false;
	shapeDef.invokeContactCreation = true;
	return shapeDef;
}

// Box3D walks all contacts of a body for every shape the body loses. A body that loses at least NB_BULK_SHAPES shapes at
// once while it has at least NB_BULK_CONTACTS contacts, a large part that breaks in two, leaves the simulation meanwhile:
// that drops its contacts in one go, and Box3D finds them again in the next step. Box3D forgets the velocity of a body
// that leaves, so it is kept here.
#define NB_BULK_SHAPES 16
#define NB_BULK_CONTACTS 256
#define NB_MAX_BULK_BODIES 8

typedef struct nbBulkBody
{
	b3BodyId bodyId;
	int shapeCount;
	b3Vec3 linearVelocity;
	b3Vec3 angularVelocity;
	bool disabled;
} nbBulkBody;

void nbCommitPhysics( nbWorld* world )
{
	nbBulkBody bulk[NB_MAX_BULK_BODIES];
	int bulkCount = 0;
	for ( int i = 0; i < world->touchedChunks.count; ++i )
	{
		const nbChunk* chunk = world->chunks.data + world->touchedChunks.data[i];
		if ( chunk->shape == NULL || ( chunk->flags & nb_chunkMoved ) == 0 || ( chunk->flags & nb_chunkOwnsBody ) != 0 ||
			 B3_IS_NULL( chunk->shapeId ) )
		{
			continue;
		}

		int k = 0;
		while ( k < bulkCount && B3_ID_EQUALS( bulk[k].bodyId, chunk->bodyId ) == false )
		{
			k += 1;
		}

		if ( k == bulkCount )
		{
			if ( bulkCount == NB_MAX_BULK_BODIES )
			{
				continue;
			}
			bulk[bulkCount++] = (nbBulkBody){ .bodyId = chunk->bodyId };
		}
		bulk[k].shapeCount += 1;
	}

	for ( int k = 0; k < bulkCount; ++k )
	{
		nbBulkBody* body = bulk + k;
		if ( body->shapeCount >= NB_BULK_SHAPES && b3Body_GetContactCapacity( body->bodyId ) >= NB_BULK_CONTACTS )
		{
			body->linearVelocity = b3Body_GetLinearVelocity( body->bodyId );
			body->angularVelocity = b3Body_GetAngularVelocity( body->bodyId );
			b3Body_Disable( body->bodyId );
			body->disabled = true;
		}
	}

	for ( int i = 0; i < world->touchedChunks.count; ++i )
	{
		int chunkIndex = world->touchedChunks.data[i];
		nbChunk* chunk = world->chunks.data + chunkIndex;
		uint8_t flags = chunk->flags;
		chunk->flags &= ~( nb_chunkTouched | nb_chunkNew | nb_chunkMoved );

		if ( chunk->shape == NULL || ( flags & ( nb_chunkNew | nb_chunkMoved ) ) == 0 )
		{
			continue;
		}

		nbActor* actor = world->actors.data + chunk->actorIndex;
		nbDestructible* destructible = world->destructibles.data + chunk->destructibleIndex;
		b3ShapeDef shapeDef = nbMakeShapeDef( destructible, destructible->materials + chunk->materialIndex, actor->isStatic );

		// The hull to use: the fresh one, or the one the chunk already has in the Box3D hull database
		const b3HullData* hull = chunk->pendingHull;
		b3ShapeId oldShapeId = chunk->shapeId;
		b3BodyId oldBodyId = chunk->bodyId;
		bool ownedOldBody = ( chunk->flags & nb_chunkOwnsBody ) != 0;
		if ( hull == NULL )
		{
			NB_ASSERT( B3_IS_NON_NULL( oldShapeId ) );
			hull = b3Shape_GetHull( oldShapeId );
		}

		b3BodyId bodyId;
		if ( actor->isStatic )
		{
			// Static chunks get a body each. Static-static pairs never collide, so this costs nothing
			// in the solver, and destroying a chunk only touches its own contacts.
			b3BodyDef bodyDef = b3DefaultBodyDef();
			bodyDef.type = b3_staticBody;
			bodyDef.position = destructible->transform.p;
			bodyDef.rotation = destructible->transform.q;
			bodyId = b3CreateBody( world->physicsWorld, &bodyDef );
			chunk->flags |= nb_chunkOwnsBody;
		}
		else
		{
			bodyId = actor->bodyId;
			chunk->flags &= ~nb_chunkOwnsBody;
		}

		// Create the new shape before destroying the old one so the shared hull stays alive
		chunk->shapeId = b3CreateHullShape( bodyId, &shapeDef, hull );
		chunk->bodyId = bodyId;
		chunk->pendingHull = NULL;
		nbMapShape( world, chunk->shapeId, chunkIndex );
		actor->massDirty = true;

		if ( B3_IS_NON_NULL( oldShapeId ) )
		{
			nbUnmapShape( world, oldShapeId );
			if ( ownedOldBody )
			{
				b3DestroyBody( oldBodyId );
			}
			else
			{
				b3DestroyShape( oldShapeId, false );
			}
		}

		if ( ( flags & nb_chunkNew ) == 0 )
		{
			nbPushEvent( world->movedEvents + world->eventBuffer, nbMakeChunkId( world, chunkIndex ) );
		}
	}
	world->touchedChunks.count = 0;

	// A body left without shapes goes away with its actor below
	for ( int k = 0; k < bulkCount; ++k )
	{
		nbBulkBody* body = bulk + k;
		if ( body->disabled && b3Body_GetShapeCount( body->bodyId ) > 0 )
		{
			b3Body_Enable( body->bodyId );
			if ( b3Body_GetType( body->bodyId ) == b3_dynamicBody )
			{
				b3Body_SetLinearVelocity( body->bodyId, body->linearVelocity );
				b3Body_SetAngularVelocity( body->bodyId, body->angularVelocity );
			}
		}
	}

	for ( int i = 0; i < world->touchedActors.count; ++i )
	{
		int actorIndex = world->touchedActors.data[i];
		nbActor* actor = world->actors.data + actorIndex;
		if ( actor->isFree )
		{
			continue;
		}

		if ( actor->chunkCount == 0 )
		{
			nbFreeActor( world, actorIndex );
			continue;
		}

		// Rubble is a static body without mass, it gets its mass back when it comes back to life
		if ( actor->isStatic == false && actor->isRubble == false && actor->massDirty )
		{
			nbUpdateActorMass( world, actor );
		}

		nbUpdateDebris( world, actorIndex );
		if ( actor->isStatic || actor->isBuildingPart )
		{
			nbMarkSupport( world, actorIndex );
		}

		// Rubble that rested on a part falling off the structure or off rubble falls with it
		if ( actor->fromStructure )
		{
			actor->fromStructure = false;
			nbHoldRubble( world, actor );
		}
	}
}

// Move a set of chunks from an actor to a new dynamic actor that inherits the source motion.
static int nbDetachChunks( nbWorld* world, int sourceIndex, const int* chunks, int count, b3WorldTransform transform,
						   b3Vec3 linearVelocity, b3Vec3 angularVelocity, b3Pos center )
{
	nbActor* source = world->actors.data + sourceIndex;
	int destructibleIndex = source->destructibleIndex;

	int actorIndex = nbAllocActor( world, destructibleIndex, false );
	nbCreateActorBody( world, actorIndex, transform );

	// The allocation may have moved the actors
	source = world->actors.data + sourceIndex;
	nbActor* actor = world->actors.data + actorIndex;
	actor->sourceLinearVelocity = linearVelocity;
	actor->sourceAngularVelocity = angularVelocity;
	actor->sourceCenter = center;
	actor->fromStructure = source->isStatic || source->isRubble;
	actor->fromCollapse = source->fromCollapse || world->collapsing;
	actor->holdSource = source->isRubble ? sourceIndex : NB_NULL_INDEX;
	actor->holdSourceGeneration = source->generation;
	nbTouchActor( world, actorIndex );

	// Rubble can only lose what carries it below its center of mass
	b3WorldTransform sourceTransform = nbActor_GetTransform( world, source );
	b3Vec3 up = b3Neg( b3Normalize( b3World_GetGravity( world->physicsWorld ) ) );
	float centerHeight = b3Dot( up, b3ToVec3( b3TransformWorldPoint( sourceTransform, source->localCenter ) ) );

	for ( int i = 0; i < count; ++i )
	{
		int chunkIndex = chunks[i];
		if ( source->isRubble )
		{
			b3Pos centroid = b3TransformWorldPoint( sourceTransform, world->chunks.data[chunkIndex].shape->centroid );
			source->lostPieces = source->lostPieces || b3Dot( up, b3ToVec3( centroid ) ) < centerHeight;
		}
		nbActor_RemoveChunk( world, sourceIndex, chunkIndex );
		nbActor_AddChunk( world, actorIndex, chunkIndex );
		world->chunks.data[chunkIndex].flags |= nb_chunkMoved;
		nbTouchChunk( world, chunkIndex );
	}

	return actorIndex;
}

static float nbAnchorDistance( const nbDestructible* destructible, const nbChunk* chunk )
{
	float distance = FLT_MAX;
	b3Vec3 centroid = chunk->shape->centroid;
	for ( int i = 0; i < destructible->anchorCount; ++i )
	{
		const nbAnchorPlane* anchor = destructible->anchors + i;
		distance = b3MinFloat( distance, b3Dot( anchor->normal, centroid ) - anchor->offset );
	}
	return distance > 0.0f ? distance : 0.0f;
}

static uint32_t nbFloatKey( float value )
{
	uint32_t bits;
	memcpy( &bits, &value, sizeof( bits ) );
	return bits;
}

static void nbSiftDown( uint64_t* heap, int count, int index )
{
	uint64_t key = heap[index];
	for ( ;; )
	{
		int child = 2 * index + 1;
		if ( child >= count )
		{
			break;
		}
		if ( child + 1 < count && heap[child + 1] < heap[child] )
		{
			child += 1;
		}
		if ( key <= heap[child] )
		{
			break;
		}
		heap[index] = heap[child];
		index = child;
	}
	heap[index] = key;
}

static void nbSiftUp( uint64_t* heap, int index )
{
	uint64_t key = heap[index];
	while ( index > 0 )
	{
		int parent = ( index - 1 ) / 2;
		if ( heap[parent] <= key )
		{
			break;
		}
		heap[index] = heap[parent];
		index = parent;
	}
	heap[index] = key;
}

// The static actor only has to check the chunks near broken bonds. From each seed a best-first
// search walks toward the anchor planes and stops at the first anchored or known supported chunk.
// Only an exhausted search, which has then visited its entire island, detaches anything.
static void nbSplitStaticActor( nbWorld* world, int actorIndex, nbImpactResult* result )
{
	nbActor* actor = world->actors.data + actorIndex;
	int destructibleIndex = actor->destructibleIndex;
	const nbDestructible* destructible = world->destructibles.data + destructibleIndex;
	b3WorldTransform transform = destructible->transform;

	int capacity = actor->chunkCount;
	uint64_t* heap = nbArena_AllocArray( &world->arena, uint64_t, capacity );
	int* visited = nbArena_AllocArray( &world->arena, int, capacity );

	world->searchStamp += 1;
	uint32_t stamp = world->searchStamp;

	for ( int s = 0; s < world->splitSeeds.count; ++s )
	{
		int seed = world->splitSeeds.data[s];
		nbChunk* seedChunk = world->chunks.data + seed;
		if ( seedChunk->shape == NULL || seedChunk->actorIndex != actorIndex || seedChunk->searchStamp == stamp )
		{
			continue;
		}

		int heapCount = 0;
		int visitedCount = 0;
		bool supported = false;

		seedChunk->searchStamp = stamp;
		seedChunk->scratch = -1;
		heap[heapCount++] = ( (uint64_t)nbFloatKey( nbAnchorDistance( destructible, seedChunk ) ) << 32 ) | (uint32_t)seed;

		while ( heapCount > 0 && supported == false )
		{
			uint64_t top = heap[0];
			heap[0] = heap[--heapCount];
			nbSiftDown( heap, heapCount, 0 );

			int chunkIndex = (int)( top & 0xFFFFFFFFu );
			nbChunk* chunk = world->chunks.data + chunkIndex;
			visited[visitedCount++] = chunkIndex;

			if ( chunk->flags & nb_chunkAnchored )
			{
				supported = true;
				break;
			}

			for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
			{
				const nbBond* bond = world->bonds.data + ( key >> 1 );
				int side = key & 1;
				int other = bond->chunk[side ^ 1];
				key = bond->nextKey[side];

				nbChunk* otherChunk = world->chunks.data + other;
				if ( otherChunk->searchStamp == stamp )
				{
					if ( otherChunk->scratch == 1 )
					{
						supported = true;
						break;
					}
					continue;
				}

				otherChunk->searchStamp = stamp;
				otherChunk->scratch = -1;
				heap[heapCount] = ( (uint64_t)nbFloatKey( nbAnchorDistance( destructible, otherChunk ) ) << 32 ) | (uint32_t)other;
				nbSiftUp( heap, heapCount );
				heapCount += 1;
			}
		}

		int label = supported ? 1 : 0;
		for ( int i = 0; i < visitedCount; ++i )
		{
			world->chunks.data[visited[i]].scratch = label;
		}
		for ( int i = 0; i < heapCount; ++i )
		{
			world->chunks.data[heap[i] & 0xFFFFFFFFu].scratch = label;
		}

		if ( supported == false )
		{
			// The search ran dry, so the visited chunks are the whole unsupported island
			nbDetachChunks( world, actorIndex, visited, visitedCount, transform, b3Vec3_zero, b3Vec3_zero, transform.p );
			result->detachedChunkCount += visitedCount;
			result->createdBodyCount += 1;
		}
	}
}

// Dynamic actors are small. Label all islands, the heaviest keeps the body.
static void nbSplitDynamicActor( nbWorld* world, int actorIndex, nbImpactResult* result )
{
	nbActor* actor = world->actors.data + actorIndex;
	int chunkCount = actor->chunkCount;
	int* chunkList = nbArena_AllocArray( &world->arena, int, chunkCount );
	int* stack = nbArena_AllocArray( &world->arena, int, chunkCount );
	int* order = nbArena_AllocArray( &world->arena, int, chunkCount );

	int count = 0;
	for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
	{
		world->chunks.data[c].scratch = NB_NULL_INDEX;
		chunkList[count++] = c;
	}
	NB_ASSERT( count == chunkCount );

	float* componentVolume = nbArena_AllocArray( &world->arena, float, chunkCount );
	int* componentStart = nbArena_AllocArray( &world->arena, int, chunkCount + 1 );
	int componentCount = 0;
	int orderCount = 0;

	for ( int i = 0; i < count; ++i )
	{
		int seed = chunkList[i];
		if ( world->chunks.data[seed].scratch != NB_NULL_INDEX )
		{
			continue;
		}

		int component = componentCount++;
		componentVolume[component] = 0.0f;
		componentStart[component] = orderCount;

		int stackCount = 0;
		stack[stackCount++] = seed;
		world->chunks.data[seed].scratch = component;

		while ( stackCount > 0 )
		{
			int chunkIndex = stack[--stackCount];
			nbChunk* chunk = world->chunks.data + chunkIndex;
			componentVolume[component] += chunk->shape->volume;
			order[orderCount++] = chunkIndex;

			for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
			{
				const nbBond* bond = world->bonds.data + ( key >> 1 );
				int side = key & 1;
				int other = bond->chunk[side ^ 1];
				key = bond->nextKey[side];

				nbChunk* otherChunk = world->chunks.data + other;
				if ( otherChunk->scratch == NB_NULL_INDEX )
				{
					otherChunk->scratch = component;
					stack[stackCount++] = other;
				}
			}
		}
	}
	componentStart[componentCount] = orderCount;

	if ( componentCount < 2 )
	{
		return;
	}

	int heaviest = 0;
	for ( int c = 1; c < componentCount; ++c )
	{
		if ( componentVolume[c] > componentVolume[heaviest] )
		{
			heaviest = c;
		}
	}

	b3WorldTransform transform = b3Body_GetTransform( actor->bodyId );
	b3Vec3 linearVelocity = b3Body_GetLinearVelocity( actor->bodyId );
	b3Vec3 angularVelocity = b3Body_GetAngularVelocity( actor->bodyId );
	b3Pos center = b3Body_GetWorldCenter( actor->bodyId );

	for ( int c = 0; c < componentCount; ++c )
	{
		if ( c == heaviest )
		{
			continue;
		}

		int first = componentStart[c];
		int islandCount = componentStart[c + 1] - first;
		nbDetachChunks( world, actorIndex, order + first, islandCount, transform, linearVelocity, angularVelocity, center );
		result->createdBodyCount += 1;
	}
}

void nbSplitActors( nbWorld* world, nbImpactResult* result )
{
	int touchedCount = world->touchedActors.count;
	for ( int t = 0; t < touchedCount; ++t )
	{
		int actorIndex = world->touchedActors.data[t];
		nbActor* actor = world->actors.data + actorIndex;
		if ( actor->isFree || actor->chunkCount == 0 )
		{
			continue;
		}

		if ( actor->isStatic )
		{
			nbSplitStaticActor( world, actorIndex, result );
		}
		else if ( actor->chunkCount > 1 )
		{
			nbSplitDynamicActor( world, actorIndex, result );
		}
	}
}

// A cohesive bond holds its cells together as one block in the load check while it keeps at least this part of its
// strength, see nbCheckSupport
#define NB_BLOCK_INTEGRITY 0.999f

// The load check follows the structural solver of the reference engine (src/structure/structural_loads.cpp on the
// Referenz branch). Bonds carry the full strength of their material in compression, NB_SHEAR_STRENGTH of it sideways,
// and bend like a beam of NB_BENDING_STRENGTH times it.
#define NB_SHEAR_STRENGTH 0.5f
#define NB_BENDING_STRENGTH 1.0f

// Path cost of a bond per inverse square meter of its area, see nbPathCost. Small against the height costs of whole wall
// pieces, so the load still goes down story by story, large for the tiny faces of fragments.
#define NB_FACE_COST 0.001f

// A block rests on a real face when the faces it rests on cover at least this part of its footprint, its volume over
// its height
#define NB_BEARING_FACE 0.25f

// A bond fails above this utilization, a little over one so float noise cannot break a structure that just holds
#define NB_FAILURE_UTILIZATION 1.02f

// Chunks crushed in one structure per update at most. The next update carries on with what is left.
#define NB_MAX_CRUSHES 4

// A crushed chunk bursts into this many fragments that fly apart at this speed in meters per second. A few are enough
// to take the load off, more only cost. On top of that they are pushed out of the wall at NB_CRUSH_PUSH, otherwise they
// would stay wedged between the stones around them and carry the load again.
#define NB_CRUSH_FRAGMENTS 6
#define NB_CRUSH_SPEED 2.0f
#define NB_CRUSH_PUSH 4.0f

// A block that gives way collapses, see nbCheckSupport. The chunks it rests on fly out of the wall whole at this speed,
// no matter how many, so what comes down does not stay up on the stumps.
#define NB_COLLAPSE_PUSH 6.0f

// Only a sound bond crushes a chunk when it fails. A bond that damage already weakened below this fraction of its
// strength just lets go.
#define NB_CRUSH_INTEGRITY 0.5f

// Chunks the load checks of one update visit at most. Structures beyond wait for the next update.
#define NB_SUPPORT_BUDGET 65536

// A bond through which a chunk passes load to a neighbor closer to the anchors
typedef struct nbLoadPath
{
	int bondIndex;
	int parent;

	// Share of the load, then the bending the bond carries in Newton meters
	float weight;
	float bending;
	float integrity;

	// Where the share acts, the bond centroid moved by the pressure gradient
	b3Vec3 point;

	// The chunk of the carried block the bond starts at, as an index into the chunks of the structure
	int member;

	// The block rests on the parent through this bond
	bool bearing;
} nbLoadPath;

// A block that gives way, and whether it rests on something below
typedef struct nbCollapse
{
	int block;
	bool vertical;
	float utilization;
} nbCollapse;

// An overloaded bond, and the chunk it crushes or NB_NULL_INDEX. Or a chunk a collapse throws out, without a bond.
typedef struct nbOverload
{
	int bondIndex;
	int crushChunk;
	float utilization;

	// Throw the chunk out whole instead of bursting it
	bool eject;
} nbOverload;

// What comes down over a storey that gave way starts to turn at up to this many radians per second toward the side where
// the walls are missing, see nbCheckStoreys. It only turns, a push on top would throw it.
#define NB_TILT_SPEED 0.5f

// A part of a building that came down keeps its storeys while its local Y axis stays within 45 degrees of up
#define NB_UPRIGHT 0.7071f

// Rubble this close around the remains of a storey that gives way comes back to life, so they can fly out
#define NB_CLEARANCE 1.0f

// A storey that gave way does not give way again for remains of less than this part of its walls
#define NB_STOREY_REMAINS 0.1f

// How a chunk comes down over a storey that gave way
typedef struct nbTilt
{
	int chunkIndex;
	b3Vec3 angularVelocity;
} nbTilt;

// Remaining health of a bond over the health of an undamaged bond of the same area and materials
static float nbBondIntegrity( const nbWorld* world, const nbBond* bond )
{
	float full = nbGetBondStrength( world, bond ) * bond->area;
	return full > 0.0f ? b3ClampFloat( bond->health / full, 0.01f, 1.0f ) : 0.01f;
}

// a^T M b with the second moments M of a bond
static float nbMomentForm( const nbBond* bond, b3Vec3 a, b3Vec3 b )
{
	b3Vec3 m = bond->moments;
	b3Vec3 c = bond->crossMoments;
	b3Vec3 mb = {
		m.x * b.x + c.x * b.y + c.y * b.z,
		c.x * b.x + m.y * b.y + c.z * b.z,
		c.y * b.x + c.z * b.y + m.z * b.z,
	};
	return b3Dot( a, mb );
}

// Section modulus of a bond for bending about an axis through its centroid, I / c. I is the second moment of the face
// about the axis, c the distance of the farthest fiber, estimated as sqrt(3 I / A). For a rectangle that is exactly
// area * depth / 6.
static float nbSectionModulus( const nbBond* bond, b3Vec3 axis )
{
	float trace = bond->moments.x + bond->moments.y + bond->moments.z;
	float spread = b3MaxFloat( trace - nbMomentForm( bond, axis, axis ), 0.0f );
	return bond->area * sqrtf( spread / 3.0f );
}

// Cost of carrying a chunk through a bond from a chunk that is already supported. Resting on something below is cheap,
// reaching sideways costs the distance, and a damaged bond costs more. The normal points from the supporting chunk to
// the carried one. Unlike the equal cells of the reference engine, chunks here range from whole walls to the fragments
// around a bullet hole, so a small face costs extra: the load of a wall arches around the fragments of a hole instead of
// crushing them one by one.
static float nbPathCost( b3Vec3 from, b3Vec3 to, b3Vec3 normal, float area, float integrity, b3Vec3 up )
{
	b3Vec3 d = b3Sub( to, from );
	float rise = b3Dot( d, up );
	float horizontal = b3Length( b3MulSub( d, rise, up ) );
	bool bearing = b3Dot( normal, up ) > 0.1f && rise > 0.0f;
	float cost = bearing ? 0.04f * rise : horizontal + 0.02f + 0.04f * b3MaxFloat( -rise, 0.0f );
	cost += NB_FACE_COST / b3MaxFloat( area, 1.0e-6f );
	return cost / integrity;
}

static int nbComparePoints( const void* a, const void* b )
{
	const b3Vec2* p = a;
	const b3Vec2* q = b;
	if ( p->x != q->x )
	{
		return p->x < q->x ? -1 : 1;
	}
	return p->y < q->y ? -1 : ( p->y > q->y ? 1 : 0 );
}

static float nbCross2( b3Vec2 o, b3Vec2 a, b3Vec2 b )
{
	return ( a.x - o.x ) * ( b.y - o.y ) - ( a.y - o.y ) * ( b.x - o.x );
}

static float nbDot2( b3Vec2 a, b3Vec2 b )
{
	return a.x * b.x + a.y * b.y;
}

// The point of a segment nearest to p
static b3Vec2 nbNearestOnSegment( b3Vec2 a, b3Vec2 b, b3Vec2 p )
{
	b3Vec2 ab = { b.x - a.x, b.y - a.y };
	b3Vec2 ap = { p.x - a.x, p.y - a.y };
	float length = nbDot2( ab, ab );
	float t = length > 0.0f ? b3ClampFloat( nbDot2( ap, ab ) / length, 0.0f, 1.0f ) : 0.0f;
	return (b3Vec2){ a.x + t * ab.x, a.y + t * ab.y };
}

// The point of the convex hull of the given points nearest to p, p itself inside. Sorts the points, the scratch holds
// twice as many.
static b3Vec2 nbNearestOnHull( b3Vec2* points, int count, b3Vec2* scratch, b3Vec2 p )
{
	qsort( points, (size_t)count, sizeof( b3Vec2 ), nbComparePoints );

	// Monotone chain, counter clockwise
	int n = 0;
	for ( int i = 0; i < count; ++i )
	{
		while ( n >= 2 && nbCross2( scratch[n - 2], scratch[n - 1], points[i] ) <= 0.0f )
		{
			n -= 1;
		}
		scratch[n++] = points[i];
	}
	for ( int i = count - 2, lower = n + 1; i >= 0; --i )
	{
		while ( n >= lower && nbCross2( scratch[n - 2], scratch[n - 1], points[i] ) <= 0.0f )
		{
			n -= 1;
		}
		scratch[n++] = points[i];
	}
	n = count > 1 ? n - 1 : n;

	if ( n == 1 )
	{
		return scratch[0];
	}

	bool inside = n >= 3;
	float best = FLT_MAX;
	b3Vec2 nearest = p;
	for ( int i = 0; i < n; ++i )
	{
		b3Vec2 a = scratch[i];
		b3Vec2 b = scratch[i + 1 < n ? i + 1 : 0];
		inside = inside && nbCross2( a, b, p ) >= 0.0f;
		b3Vec2 q = nbNearestOnSegment( a, b, p );
		b3Vec2 d = { p.x - q.x, p.y - q.y };
		float distance = nbDot2( d, d );
		if ( distance < best )
		{
			best = distance;
			nearest = q;
		}
	}
	return inside ? p : nearest;
}

// Two boxes overlap seen from above, along both horizontal axes
static bool nbOverlapsAcross( b3AABB a, b3AABB b, b3Vec3 side1, b3Vec3 side2 )
{
	b3Vec3 axes[2] = { side1, side2 };
	for ( int k = 0; k < 2; ++k )
	{
		b3Vec3 axis = b3Abs( axes[k] );
		float centerA = b3Dot( axes[k], b3MulSV( 0.5f, b3Add( a.lowerBound, a.upperBound ) ) );
		float centerB = b3Dot( axes[k], b3MulSV( 0.5f, b3Add( b.lowerBound, b.upperBound ) ) );
		float extentA = b3Dot( axis, b3MulSV( 0.5f, b3Sub( a.upperBound, a.lowerBound ) ) );
		float extentB = b3Dot( axis, b3MulSV( 0.5f, b3Sub( b.upperBound, b.lowerBound ) ) );
		if ( b3AbsFloat( centerA - centerB ) >= extentA + extentB - 0.01f )
		{
			return false;
		}
	}
	return true;
}

// A cell of a pre-fracture that can be part of a block: not a fragment and not anchored
static bool nbIsBlockCell( const nbChunk* chunk )
{
	return chunk->depth == 0 && ( chunk->flags & nb_chunkAnchored ) == 0;
}

static int nbFindBlock( int* blocks, int i )
{
	while ( blocks[i] != i )
	{
		blocks[i] = blocks[blocks[i]];
		i = blocks[i];
	}
	return i;
}

// Load check of one structure, after the structural solver of the reference engine. It works on blocks: the undamaged
// cells of a pre-fractured piece carry as one, every other chunk is a block of its own. A shortest path search out of
// the anchored blocks decides who carries whom. Then the load flows from the farthest blocks inward: every block passes
// its weight, and the weight resting on it, to its neighbors closer to the anchors, shared by bond area and four times as
// much through bonds it rests on. The shares lean toward the center of mass of the load, like the pressure under a
// footing, so the load passes through it where the bonds reach around it. What reaches beyond, like an overhang, bends
// the bonds. Bonds a chunk rests on work as hinges: the load goes straight down through them and only the chunk's own
// weight bends them, so a roof does not twist a whole wall. A bond fails when the force through it over what its area
// carries, plus the bending over what its shape carries, exceeds one. Returns the overloaded bonds in arena memory.
static int nbCheckSupport( nbWorld* world, int actorIndex, float gravity, b3Vec3 up, nbOverload** overloadsOut,
						   bool* collapsedOut )
{
	const nbActor* actor = world->actors.data + actorIndex;
	const nbDestructible* destructible = world->destructibles.data + actor->destructibleIndex;
	float scale = world->def.supportScale;
	b3Vec3 localUp = b3InvRotateVector( destructible->transform.q, up );
	b3Vec3 side1 = b3Perp( localUp );
	b3Vec3 side2 = b3Cross( localUp, side1 );

	int count = actor->chunkCount;
	int* chunks = nbArena_AllocArray( &world->arena, int, count );
	int* blocks = nbArena_AllocArray( &world->arena, int, count );
	int* nodes = nbArena_AllocArray( &world->arena, int, count );
	int* nextMember = nbArena_AllocArray( &world->arena, int, count );

	world->searchStamp += 1;
	uint32_t stamp = world->searchStamp;
	int keyCount = 0;
	int n = 0;
	for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
	{
		nbChunk* chunk = world->chunks.data + c;
		chunk->searchStamp = stamp;
		chunk->scratch = n;
		chunks[n] = c;
		blocks[n] = n;
		keyCount += chunk->bondCount;
		n += 1;
	}

	// The cells of a piece that no damage reached yet hold together through their cohesive bonds and carry as one
	// block, like the piece before its pre-fracture. The irregular cells would otherwise make up weak spots, small faces
	// that bear the load of a whole wall or cells that reach over a window, that the piece does not have. Fragments and
	// the cells on anchors stay blocks of their own, so the load on the ground and around damage is checked in detail.
	for ( int i = 0; i < n; ++i )
	{
		const nbChunk* chunk = world->chunks.data + chunks[i];
		if ( nbIsBlockCell( chunk ) == false )
		{
			continue;
		}

		for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
		{
			const nbBond* bond = world->bonds.data + ( key >> 1 );
			int side = key & 1;
			key = bond->nextKey[side];

			const nbChunk* other = world->chunks.data + bond->chunk[side ^ 1];
			if ( bond->cohesive == false || other->searchStamp != stamp || nbIsBlockCell( other ) == false ||
				 nbBondIntegrity( world, bond ) < NB_BLOCK_INTEGRITY )
			{
				continue;
			}

			// Joined under the smaller index, so a block is numbered where its first chunk is
			int a = nbFindBlock( blocks, i );
			int b = nbFindBlock( blocks, other->scratch );
			blocks[a > b ? a : b] = a < b ? a : b;
		}
	}

	int nodeCount = 0;
	for ( int i = 0; i < n; ++i )
	{
		int root = nbFindBlock( blocks, i );
		nodes[i] = root == i ? nodeCount++ : nodes[root];
	}

	int* order = nbArena_AllocArray( &world->arena, int, nodeCount );
	int* rank = nbArena_AllocArray( &world->arena, int, nodeCount );
	int* previous = nbArena_AllocArray( &world->arena, int, nodeCount );
	int* firstMember = nbArena_AllocArray( &world->arena, int, nodeCount );
	float* distance = nbArena_AllocArray( &world->arena, float, nodeCount );
	float* mass = nbArena_AllocArray( &world->arena, float, nodeCount );
	float* volume = nbArena_AllocArray( &world->arena, float, nodeCount );
	b3AABB* bounds = nbArena_AllocArray( &world->arena, b3AABB, nodeCount );
	float* load = nbArena_AllocArray( &world->arena, float, nodeCount );
	b3Vec3* moment = nbArena_AllocArray( &world->arena, b3Vec3, nodeCount );
	b3Vec3* centroid = nbArena_AllocArray( &world->arena, b3Vec3, nodeCount );
	for ( int k = 0; k < nodeCount; ++k )
	{
		firstMember[k] = NB_NULL_INDEX;
		distance[k] = FLT_MAX;
		mass[k] = 0.0f;
		volume[k] = 0.0f;
		moment[k] = b3Vec3_zero;
		rank[k] = -1;
		previous[k] = -1;
	}

	for ( int i = n - 1; i >= 0; --i )
	{
		nbChunk* chunk = world->chunks.data + chunks[i];
		int k = nodes[i];
		nextMember[i] = firstMember[k];
		firstMember[k] = i;

		float chunkMass = chunk->shape->volume * destructible->materials[chunk->materialIndex].density;
		mass[k] += chunkMass;
		volume[k] += chunk->shape->volume;
		bounds[k] = nextMember[i] == NB_NULL_INDEX ? chunk->shape->bounds : b3AABB_Union( bounds[k], chunk->shape->bounds );
		moment[k] = b3MulAdd( moment[k], chunkMass, chunk->shape->centroid );
		distance[k] = ( chunk->flags & nb_chunkAnchored ) ? 0.0f : distance[k];
	}

	// The lowest point of every block
	float* bottom = nbArena_AllocArray( &world->arena, float, nodeCount );
	for ( int k = 0; k < nodeCount; ++k )
	{
		b3Vec3 center = b3MulSV( 0.5f, b3Add( bounds[k].lowerBound, bounds[k].upperBound ) );
		b3Vec3 half = b3MulSV( 0.5f, b3Sub( bounds[k].upperBound, bounds[k].lowerBound ) );
		bottom[k] = b3Dot( center, localUp ) - b3Dot( b3Abs( localUp ), half );
	}

	for ( int k = 0; k < nodeCount; ++k )
	{
		int first = firstMember[k];
		load[k] = mass[k];
		centroid[k] = nextMember[first] == NB_NULL_INDEX ? world->chunks.data[chunks[first]].shape->centroid
														: b3MulSV( 1.0f / mass[k], moment[k] );
	}

	// Shortest paths out of the anchored blocks, with a heap that skips stale entries. Every directed bond pushes once
	// at most.
	uint64_t* heap = nbArena_AllocArray( &world->arena, uint64_t, nodeCount + keyCount );
	int heapCount = 0;
	for ( int k = 0; k < nodeCount; ++k )
	{
		if ( distance[k] == 0.0f )
		{
			heap[heapCount] = (uint32_t)k;
			nbSiftUp( heap, heapCount );
			heapCount += 1;
		}
	}

	int orderCount = 0;
	while ( heapCount > 0 )
	{
		uint64_t top = heap[0];
		heap[0] = heap[--heapCount];
		nbSiftDown( heap, heapCount, 0 );

		int i = (int)( top & 0xFFFFFFFFu );
		if ( rank[i] >= 0 )
		{
			continue;
		}
		rank[i] = orderCount;
		order[orderCount++] = i;

		for ( int m = firstMember[i]; m != NB_NULL_INDEX; m = nextMember[m] )
		{
			const nbChunk* chunk = world->chunks.data + chunks[m];
			for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
			{
				const nbBond* bond = world->bonds.data + ( key >> 1 );
				int side = key & 1;
				key = bond->nextKey[side];

				const nbChunk* other = world->chunks.data + bond->chunk[side ^ 1];
				if ( other->searchStamp != stamp )
				{
					continue;
				}

				int j = nodes[other->scratch];
				if ( j == i || rank[j] >= 0 )
				{
					continue;
				}

				// A block takes the load where the bond meets it, not at its center, which lies meters away in a wall
				b3Vec3 normal = side == 0 ? bond->normal : b3Neg( bond->normal );
				b3Vec3 from = nextMember[firstMember[i]] != NB_NULL_INDEX ? bond->centroid : centroid[i];
				b3Vec3 to = nextMember[firstMember[j]] != NB_NULL_INDEX ? bond->centroid : centroid[j];
				float cost = nbPathCost( from, to, normal, bond->area, nbBondIntegrity( world, bond ), localUp );
				float next = distance[i] + cost;
				if ( next < distance[j] )
				{
					distance[j] = next;
					previous[j] = i;
					heap[heapCount] = ( (uint64_t)nbFloatKey( next ) << 32 ) | (uint32_t)j;
					nbSiftUp( heap, heapCount );
					heapCount += 1;
				}
			}
		}
	}

	// The flow from the farthest blocks inward
	nbLoadPath* paths = nbArena_AllocArray( &world->arena, nbLoadPath, keyCount + 1 );
	b3Vec2* footprint = nbArena_AllocArray( &world->arena, b3Vec2, 4 * keyCount + 4 );
	b3Vec2* hull = nbArena_AllocArray( &world->arena, b3Vec2, 8 * keyCount + 8 );
	nbOverload* overloads = nbArena_AllocArray( &world->arena, nbOverload, 2 * keyCount + n + 1 );
	nbCollapse* collapses = nbArena_AllocArray( &world->arena, nbCollapse, nodeCount );
	int collapseCount = 0;
	bool* thrown = nbArena_AllocArray( &world->arena, bool, n );
	bool* rests = nbArena_AllocArray( &world->arena, bool, n );
	for ( int c = 0; c < n; ++c )
	{
		thrown[c] = false;
		rests[c] = false;
	}
	int overloadCount = 0;
	for ( int o = orderCount - 1; o >= 0; --o )
	{
		int i = order[o];
		if ( distance[i] == 0.0f )
		{
			continue;
		}

		// The bonds to blocks closer to the anchors. Ties only count for the block the search came from.
		int pathCount = 0;
		float total = 0.0f;
		b3Vec3 center = b3Vec3_zero;
		bool vertical = false;
		float bearingArea = 0.0f;
		for ( int m = firstMember[i]; m != NB_NULL_INDEX; m = nextMember[m] )
		{
			const nbChunk* chunk = world->chunks.data + chunks[m];
			for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
			{
				int bondIndex = key >> 1;
				const nbBond* bond = world->bonds.data + bondIndex;
				int side = key & 1;
				key = bond->nextKey[side];

				const nbChunk* other = world->chunks.data + bond->chunk[side ^ 1];
				if ( other->searchStamp != stamp )
				{
					continue;
				}

				int j = nodes[other->scratch];
				if ( j == i || rank[j] < 0 || rank[j] > rank[i] || ( distance[j] >= distance[i] && j != previous[i] ) )
				{
					continue;
				}

				// The normal points out of this block, down into a block it rests on
				b3Vec3 normal = side == 0 ? bond->normal : b3Neg( bond->normal );
				bool bearing = b3Dot( normal, localUp ) < -0.1f;
				float integrity = nbBondIntegrity( world, bond );
				float weight = bond->area * ( bearing ? 4.0f : 1.0f ) * integrity;
				paths[pathCount++] = (nbLoadPath){ bondIndex, j, weight, 0.0f, integrity, bond->centroid, m, bearing };
				vertical = vertical || bearing;
				bearingArea += bearing ? bond->area : 0.0f;
			}
		}

		// A block that rests on a real face passes its load down, not sideways into its neighbors, like the box cells of
		// the reference. Otherwise the piers beside a window would unload into the sill below it. A block that only
		// touches what is below with a small tip shares through its side faces as well, like the irregular cells there.
		// A block of cells that stands on something passes its load down only, so a wall whose base gives way does not
		// hang on the walls it meets at the corners.
		b3Vec3 extent = b3Sub( bounds[i].upperBound, bounds[i].lowerBound );
		float height = b3Dot( b3Abs( localUp ), extent );
		bool block = nextMember[firstMember[i]] != NB_NULL_INDEX;
		if ( ( block && bearingArea > 0.0f ) || bearingArea >= NB_BEARING_FACE * volume[i] / b3MaxFloat( height, 1.0e-3f ) )
		{
			int bearingCount = 0;
			for ( int p = 0; p < pathCount; ++p )
			{
				if ( paths[p].bearing )
				{
					paths[bearingCount++] = paths[p];
				}
			}
			pathCount = bearingCount;
		}

		for ( int p = 0; p < pathCount; ++p )
		{
			total += paths[p].weight;
			center = b3MulAdd( center, paths[p].weight, paths[p].point );
		}

		if ( total <= 0.0f )
		{
			continue;
		}
		center = b3MulSV( 1.0f / total, center );

		// A pressure that grows linearly across the footprint moves the resultant onto the center of mass of the load
		float xx = 0.0f, xz = 0.0f, zz = 0.0f;
		for ( int p = 0; p < pathCount; ++p )
		{
			const nbBond* bond = world->bonds.data + paths[p].bondIndex;
			float f = paths[p].weight / total;
			b3Vec3 d = b3Sub( paths[p].point, center );
			float dx = b3Dot( d, side1 );
			float dz = b3Dot( d, side2 );
			xx += f * ( dx * dx + nbMomentForm( bond, side1, side1 ) );
			xz += f * ( dx * dz + nbMomentForm( bond, side1, side2 ) );
			zz += f * ( dz * dz + nbMomentForm( bond, side2, side2 ) );
		}

		b3Vec3 massCenter = b3MulSV( 1.0f / load[i], moment[i] );
		float tx = b3Dot( b3Sub( massCenter, center ), side1 );
		float tz = b3Dot( b3Sub( massCenter, center ), side2 );
		float lx = 0.0f, lz = 0.0f;
		float det = xx * zz - xz * xz;
		if ( det > 1.0e-10f )
		{
			lx = ( tx * zz - tz * xz ) / det;
			lz = ( tz * xx - tx * xz ) / det;
		}
		else if ( xx > zz && xx > 1.0e-8f )
		{
			lx = tx / xx;
		}
		else if ( zz > 1.0e-8f )
		{
			lz = tz / zz;
		}

		// Pressure is never negative. Within each face the same gradient moves the point the share acts at.
		float adjusted = 0.0f;
		b3Vec3 resultant = b3Vec3_zero;
		for ( int p = 0; p < pathCount; ++p )
		{
			nbLoadPath* path = paths + p;
			const nbBond* bond = world->bonds.data + path->bondIndex;
			b3Vec3 d = b3Sub( path->point, center );
			float factor = 1.0f + lx * b3Dot( d, side1 ) + lz * b3Dot( d, side2 );
			path->weight *= b3MaxFloat( factor, 0.0f );
			if ( factor > 0.0f )
			{
				float m11 = nbMomentForm( bond, side1, side1 );
				float m12 = nbMomentForm( bond, side1, side2 );
				float m22 = nbMomentForm( bond, side2, side2 );
				float range = b3AbsFloat( lx ) * sqrtf( 12.0f * m11 ) + b3AbsFloat( lz ) * sqrtf( 12.0f * m22 );
				float s = b3MinFloat( 1.0f, factor / b3MaxFloat( range, 1.0e-12f ) );
				b3Vec3 shift = b3MulAdd( b3MulSV( m11 * lx + m12 * lz, side1 ), m12 * lx + m22 * lz, side2 );
				path->point = b3MulAdd( path->point, s / factor, shift );
			}
			adjusted += path->weight;
			resultant = b3MulAdd( resultant, path->weight, path->point );
		}

		if ( adjusted <= 0.0f )
		{
			continue;
		}
		resultant = b3MulSV( 1.0f / adjusted, resultant );

		// The load the bonds do not reach around bends them, shared by what each bond carries in bending about the axis.
		// Through hinges the loads resting on a block go straight down, and only its own weight bends the bonds, as far as
		// its center of mass lies beyond the faces it rests on, seen from above. A big block rests on many faces, and the
		// pressure gradient above would leave a few centimeters that its whole weight bends through.
		float bendingLoad = vertical ? mass[i] : load[i];
		b3Vec3 lever;
		if ( vertical )
		{
			int pointCount = 0;
			for ( int p = 0; p < pathCount; ++p )
			{
				const nbBond* bond = world->bonds.data + paths[p].bondIndex;
				b3Vec2 c = { b3Dot( bond->centroid, side1 ), b3Dot( bond->centroid, side2 ) };
				float e1 = sqrtf( 3.0f * nbMomentForm( bond, side1, side1 ) );
				float e2 = sqrtf( 3.0f * nbMomentForm( bond, side2, side2 ) );
				footprint[pointCount++] = (b3Vec2){ c.x - e1, c.y };
				footprint[pointCount++] = (b3Vec2){ c.x + e1, c.y };
				footprint[pointCount++] = (b3Vec2){ c.x, c.y - e2 };
				footprint[pointCount++] = (b3Vec2){ c.x, c.y + e2 };
			}

			b3Vec2 own = { b3Dot( centroid[i], side1 ), b3Dot( centroid[i], side2 ) };
			b3Vec2 nearest = nbNearestOnHull( footprint, pointCount, hull, own );
			lever = b3MulAdd( b3MulSV( own.x - nearest.x, side1 ), own.y - nearest.y, side2 );
		}
		else
		{
			lever = b3Sub( massCenter, resultant );
			lever = b3MulSub( lever, b3Dot( lever, localUp ), localUp );
		}
		float leverLength = b3Length( lever );
		b3Vec3 axis = leverLength > 1.0e-6f ? b3MulSV( 1.0f / leverLength, b3Cross( localUp, lever ) ) : side1;
		float bending = gravity * bendingLoad * leverLength;
		float capacity = 0.0f;
		for ( int p = 0; p < pathCount; ++p )
		{
			nbLoadPath* path = paths + p;
			if ( path->weight > 0.0f )
			{
				const nbBond* bond = world->bonds.data + path->bondIndex;
				float strength = scale * nbGetBondStrength( world, bond );
				path->bending = NB_BENDING_STRENGTH * strength * nbSectionModulus( bond, axis ) * path->integrity;
				capacity += path->bending;
			}
		}
		float bendingRatio = bending > 0.0f ? bending / b3MaxFloat( capacity, FLT_MIN ) : 0.0f;

		// The force against what the bonds carry together. A block settles on its bonds until they all carry what they
		// can before one gives way, so an irregular face of a pre-fracture that takes a large share does not decide.
		float carried = 0.0f;
		for ( int p = 0; p < pathCount; ++p )
		{
			const nbLoadPath* path = paths + p;
			if ( path->weight > 0.0f )
			{
				const nbBond* bond = world->bonds.data + path->bondIndex;
				float strength = scale * nbGetBondStrength( world, bond );
				float upright = b3AbsFloat( b3Dot( bond->normal, localUp ) );
				carried += strength * ( NB_SHEAR_STRENGTH + ( 1.0f - NB_SHEAR_STRENGTH ) * upright ) * bond->area * path->integrity;
			}
		}
		float forceRatio = gravity * load[i] / b3MaxFloat( carried, FLT_MIN );
		float utilization = forceRatio + bendingRatio;

		if ( block && utilization > NB_FAILURE_UTILIZATION )
		{
			collapses[collapseCount++] = (nbCollapse){ i, vertical, utilization };
		}

		for ( int p = 0; p < pathCount; ++p )
		{
			const nbLoadPath* path = paths + p;
			if ( path->weight <= 0.0f )
			{
				continue;
			}

			const nbBond* bond = world->bonds.data + path->bondIndex;
			float weight = load[i] * path->weight / adjusted;

			// Sideways the load keeps its center of mass, so the lever of an overhang grows toward the support. Through
			// a hinge it acts where it rests.
			int j = path->parent;
			load[j] += weight;
			moment[j] = b3MulAdd( moment[j], weight, path->point );
			if ( vertical == false && capacity > 0.0f )
			{
				moment[j] = b3MulAdd( moment[j], bendingLoad * path->bending / capacity, lever );
			}

			if ( utilization > NB_FAILURE_UTILIZATION )
			{
				// A sound bond that gives way under the weight resting on it crushes the smaller of its two chunks. What
				// a collapsing block rests on goes below.
				int crushChunk = NB_NULL_INDEX;
				if ( block )
				{
					rests[path->member] = rests[path->member] || path->bearing;
				}
				else if ( path->bearing && forceRatio >= bendingRatio && path->integrity >= NB_CRUSH_INTEGRITY )
				{
					int above = chunks[path->member];
					int below = bond->chunk[0] == above ? bond->chunk[1] : bond->chunk[0];
					crushChunk = world->chunks.data[below].shape->volume <= world->chunks.data[above].shape->volume ? below : above;
				}
				overloads[overloadCount++] = (nbOverload){ path->bondIndex, crushChunk, utilization, false };
			}
		}
	}

	// A collapsing block comes down in one piece with everything that hangs on it, like the floors and upper stories of a
	// house: all that is farther from the anchors and held through it. That piece lets go of everything closer to the
	// anchors. A block that stood on something breaks along a crack under the lowest floor that hangs on it, a sound
	// part of another piece too large to throw, or above its foot without one. The cells below the crack fly out of it,
	// with the other parts of their cells, and so does what it could come down on: the stumps that reach up to it and the
	// small chunks of their own it rests or leans on, however high they reach. Otherwise it would only sink a little and
	// get wedged between them. Blocks nearer the anchors go first and take the blocks that hang on them along.
	int* cells = nbArena_AllocArray( &world->arena, int, n );
	float* cellVolumes = nbArena_AllocArray( &world->arena, float, n );
	float* cellHeights = nbArena_AllocArray( &world->arena, float, n );
	int* falls = nbArena_AllocArray( &world->arena, int, nodeCount );
	int* queue = nbArena_AllocArray( &world->arena, int, nodeCount );
	for ( int k = 0; k < nodeCount; ++k )
	{
		falls[k] = -1;
	}

	for ( int c = collapseCount - 1; c >= 0; --c )
	{
		int i = collapses[c].block;
		if ( falls[i] >= 0 )
		{
			continue;
		}

		// What falls with it
		int queueCount = 0;
		falls[i] = c;
		queue[queueCount++] = i;
		for ( int q = 0; q < queueCount; ++q )
		{
			for ( int m = firstMember[queue[q]]; m != NB_NULL_INDEX; m = nextMember[m] )
			{
				const nbChunk* member = world->chunks.data + chunks[m];
				for ( int key = member->headBondKey; key != NB_NULL_INDEX; )
				{
					const nbBond* bond = world->bonds.data + ( key >> 1 );
					int side = key & 1;
					key = bond->nextKey[side];

					const nbChunk* other = world->chunks.data + bond->chunk[side ^ 1];
					int j = other->searchStamp == stamp ? nodes[other->scratch] : i;
					if ( falls[j] < 0 && rank[j] > rank[i] )
					{
						falls[j] = c;
						queue[queueCount++] = j;
					}
				}
			}
		}

		// The parts of a cell go together, see nbBond::sibling, and the lowest floor sets the crack
		bool vertical = collapses[c].vertical;
		float utilization = collapses[c].utilization;
		float crack = -FLT_MAX;
		for ( int m = firstMember[i]; m != NB_NULL_INDEX; m = nextMember[m] )
		{
			cells[m] = m;
			cellVolumes[m] = 0.0f;
			cellHeights[m] = 0.0f;
		}

		for ( int m = firstMember[i]; m != NB_NULL_INDEX; m = nextMember[m] )
		{
			const nbChunk* member = world->chunks.data + chunks[m];
			for ( int key = member->headBondKey; key != NB_NULL_INDEX; )
			{
				const nbBond* bond = world->bonds.data + ( key >> 1 );
				int side = key & 1;
				key = bond->nextKey[side];

				const nbChunk* other = world->chunks.data + bond->chunk[side ^ 1];
				if ( other->searchStamp != stamp )
				{
					continue;
				}

				int j = nodes[other->scratch];
				if ( bond->sibling && j == i )
				{
					int a = nbFindBlock( cells, m );
					int b = nbFindBlock( cells, other->scratch );
					cells[a > b ? a : b] = a < b ? a : b;
				}
				else if ( vertical && j != i && falls[j] == c && bond->cohesive == false && other->depth == 0 &&
						  volume[j] > 2.0f * member->shape->volume )
				{
					crack = crack == -FLT_MAX ? bottom[j] : b3MinFloat( crack, bottom[j] );
				}
			}
		}

		int memberCount = 0;
		for ( int m = firstMember[i]; m != NB_NULL_INDEX; m = nextMember[m] )
		{
			const nbShape* shape = world->chunks.data[chunks[m]].shape;
			int root = nbFindBlock( cells, m );
			rests[root] = rests[root] || rests[m];
			cellVolumes[root] += shape->volume;
			cellHeights[root] += shape->volume * b3Dot( shape->centroid, localUp );
			memberCount += 1;
		}

		// Chunks of their own that hang on the block below the crack, damaged cells of the story below, stay out of the
		// piece that comes down. It would land on them. They fly out like the cells there.
		float cellVolume = volume[i] / (float)memberCount;
		for ( int q = 1; q < queueCount; ++q )
		{
			int j = queue[q];
			int k = firstMember[j];
			const nbShape* shape = world->chunks.data[chunks[k]].shape;
			if ( nextMember[k] != NB_NULL_INDEX || b3Dot( shape->centroid, localUp ) >= crack )
			{
				continue;
			}

			falls[j] = -2;
			if ( thrown[k] == false && shape->volume <= 2.0f * cellVolume )
			{
				thrown[k] = true;
				overloads[overloadCount++] = (nbOverload){ NB_NULL_INDEX, chunks[k], utilization, true };
			}
		}

		for ( int m = firstMember[i]; m != NB_NULL_INDEX && vertical; m = nextMember[m] )
		{
			const nbChunk* member = world->chunks.data + chunks[m];
			int root = nbFindBlock( cells, m );
			if ( ( rests[root] || cellHeights[root] < crack * cellVolumes[root] ) && thrown[m] == false )
			{
				thrown[m] = true;
				overloads[overloadCount++] = (nbOverload){ NB_NULL_INDEX, chunks[m], utilization, true };
			}

			for ( int key = member->headBondKey; key != NB_NULL_INDEX; )
			{
				const nbBond* bond = world->bonds.data + ( key >> 1 );
				int side = key & 1;
				key = bond->nextKey[side];

				const nbChunk* other = world->chunks.data + bond->chunk[side ^ 1];
				int k = other->scratch;
				int j = other->searchStamp == stamp ? nodes[k] : i;
				if ( j != i && rank[j] < rank[i] && thrown[k] == false && nextMember[firstMember[j]] == NB_NULL_INDEX &&
					 other->shape->volume <= 2.0f * member->shape->volume )
				{
					thrown[k] = true;
					overloads[overloadCount++] = (nbOverload){ NB_NULL_INDEX, chunks[k], utilization, true };
				}
			}
		}

		// What falls lets go of everything else
		for ( int q = 0; q < queueCount; ++q )
		{
			for ( int m = falls[queue[q]] == c ? firstMember[queue[q]] : NB_NULL_INDEX; m != NB_NULL_INDEX; m = nextMember[m] )
			{
				const nbChunk* member = world->chunks.data + chunks[m];
				for ( int key = member->headBondKey; key != NB_NULL_INDEX; )
				{
					int bondIndex = key >> 1;
					const nbBond* bond = world->bonds.data + bondIndex;
					int side = key & 1;
					key = bond->nextKey[side];

					const nbChunk* other = world->chunks.data + bond->chunk[side ^ 1];
					if ( other->searchStamp == stamp && falls[nodes[other->scratch]] != c )
					{
						overloads[overloadCount++] = (nbOverload){ bondIndex, NB_NULL_INDEX, utilization, false };
					}
				}
			}
		}

		if ( vertical == false )
		{
			continue;
		}

		for ( int k = 0; k < n; ++k )
		{
			const nbChunk* other = world->chunks.data + chunks[k];
			int j = nodes[k];
			b3Vec3 center = b3MulSV( 0.5f, b3Add( other->shape->bounds.lowerBound, other->shape->bounds.upperBound ) );
			b3Vec3 half = b3MulSV( 0.5f, b3Sub( other->shape->bounds.upperBound, other->shape->bounds.lowerBound ) );
			float top = b3Dot( center, localUp ) + b3Dot( b3Abs( localUp ), half );
			if ( thrown[k] || nextMember[firstMember[j]] != NB_NULL_INDEX ||
				 b3Dot( other->shape->centroid, localUp ) >= bottom[i] || top < bottom[i] - 0.01f )
			{
				continue;
			}

			for ( int m = firstMember[i]; m != NB_NULL_INDEX; m = nextMember[m] )
			{
				const nbShape* member = world->chunks.data[chunks[m]].shape;
				if ( nbOverlapsAcross( member->bounds, other->shape->bounds, side1, side2 ) &&
					 other->shape->volume <= 2.0f * member->volume )
				{
					thrown[k] = true;
					overloads[overloadCount++] = (nbOverload){ NB_NULL_INDEX, chunks[k], utilization, true };
					break;
				}
			}
		}
	}

	*overloadsOut = overloads;
	*collapsedOut = collapseCount > 0;
	return overloadCount;
}

// The storey a height along the local Y axis of a destructible lies in, or -1 on a floor or outside
static int nbFindStorey( const nbDestructible* destructible, float height )
{
	for ( int s = 0; s < destructible->storeyCount && height >= destructible->storeys[s].low; ++s )
	{
		if ( height < destructible->storeys[s].high )
		{
			return s;
		}
	}
	return -1;
}

// A storey of a building gives way when less than nbWorldDef::storeySupport of the walls it had is left, on any floor
// and whatever they could still carry. The chunks in it fly out like the stones under a collapsing block, see
// nbCheckSupports, and everything above comes down in one piece, so the rooms up there stay whole. It tilts toward the
// side where the walls are missing, the faster the farther its center of mass lies beside the middle of the walls that
// are left. A cell goes where its center lies, so the cracks follow the faces of the cells. The storeys count the walls
// of all static parts of the destructible, and a part lets go of what it holds itself. Returns the chunks to throw and
// the bonds to break in arena memory, and the spin of the chunks that come down.
static int nbCheckStoreys( nbWorld* world, int actorIndex, b3Vec3 up, nbOverload** overloadsOut, nbTilt** tiltsOut,
						   int* tiltCountOut )
{
	*tiltCountOut = 0;
	const nbActor* actor = world->actors.data + actorIndex;
	const nbDestructible* destructible = world->destructibles.data + actor->destructibleIndex;
	if ( world->def.storeySupport <= 0.0f || destructible->storeyCount == 0 )
	{
		return 0;
	}

	// A part that came down only while it still stands more or less upright, a building lying on its side has no storeys
	b3WorldTransform transform = nbActor_GetTransform( world, actor );
	if ( actor->isStatic == false && b3Dot( b3RotateVector( transform.q, b3Vec3_axisY ), up ) < NB_UPRIGHT )
	{
		return 0;
	}

	// What is left of every storey, in the parts of the building that stand and those that came down
	float volumes[NB_MAX_STOREYS];
	for ( int s = 0; s < destructible->storeyCount; ++s )
	{
		volumes[s] = 0.0f;
	}

	for ( int a = destructible->headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
	{
		const nbActor* part = world->actors.data + a;
		bool building = part->isStatic || part->isBuildingPart;
		for ( int c = building ? part->headChunk : NB_NULL_INDEX; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
		{
			const nbShape* shape = world->chunks.data[c].shape;
			int s = nbFindStorey( destructible, shape->centroid.y );
			if ( s >= 0 )
			{
				volumes[s] += shape->volume;
			}
		}
	}

	int failing[NB_MAX_STOREYS];
	int failingCount = 0;
	for ( int s = 0; s < destructible->storeyCount; ++s )
	{
		if ( volumes[s] < world->def.storeySupport * destructible->storeys[s].volume )
		{
			failing[failingCount++] = s;
		}
	}

	if ( failingCount == 0 )
	{
		return 0;
	}

	// The height of the center of the cell of every chunk, see nbBond::sibling
	int n = actor->chunkCount;
	int* chunks = nbArena_AllocArray( &world->arena, int, n );
	int* cells = nbArena_AllocArray( &world->arena, int, n );
	float* cellVolumes = nbArena_AllocArray( &world->arena, float, n );
	float* heights = nbArena_AllocArray( &world->arena, float, n );
	world->searchStamp += 1;
	uint32_t stamp = world->searchStamp;
	int bondKeys = 0;
	int count = 0;
	for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
	{
		nbChunk* chunk = world->chunks.data + c;
		chunk->searchStamp = stamp;
		chunk->scratch = count;
		chunks[count] = c;
		cells[count] = count;
		cellVolumes[count] = 0.0f;
		heights[count] = 0.0f;
		bondKeys += chunk->bondCount;
		count += 1;
	}

	for ( int k = 0; k < n; ++k )
	{
		const nbChunk* chunk = world->chunks.data + chunks[k];
		for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
		{
			const nbBond* bond = world->bonds.data + ( key >> 1 );
			int side = key & 1;
			key = bond->nextKey[side];

			const nbChunk* other = world->chunks.data + bond->chunk[side ^ 1];
			if ( bond->sibling && other->searchStamp == stamp )
			{
				int a = nbFindBlock( cells, k );
				int b = nbFindBlock( cells, other->scratch );
				cells[a > b ? a : b] = a < b ? a : b;
			}
		}
	}

	for ( int k = 0; k < n; ++k )
	{
		const nbShape* shape = world->chunks.data[chunks[k]].shape;
		int root = nbFindBlock( cells, k );
		cellVolumes[root] += shape->volume;
		heights[root] += shape->volume * shape->centroid.y;
	}

	// A cell's root is its first part, so it is done before the other parts
	for ( int k = 0; k < n; ++k )
	{
		int root = nbFindBlock( cells, k );
		heights[k] = k == root ? heights[k] / cellVolumes[k] : heights[root];
	}

	// What an impact cut off the building since the last update, a part as large as a tenth of a storey or more
	int* fresh = nbArena_AllocArray( &world->arena, int, destructible->actorCount );
	float* freshLows = nbArena_AllocArray( &world->arena, float, destructible->actorCount );
	float* freshHighs = nbArena_AllocArray( &world->arena, float, destructible->actorCount );
	int freshCount = 0;
	for ( int a = destructible->headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
	{
		const nbActor* part = world->actors.data + a;
		if ( part->isStatic || part->isRubble || part->age > 0.0f )
		{
			continue;
		}

		float low = FLT_MAX, high = -FLT_MAX;
		for ( int c = part->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
		{
			low = b3MinFloat( low, world->chunks.data[c].shape->bounds.lowerBound.y );
			high = b3MaxFloat( high, world->chunks.data[c].shape->bounds.upperBound.y );
		}
		fresh[freshCount] = a;
		freshLows[freshCount] = low;
		freshHighs[freshCount] = high;
		freshCount += 1;
	}

	// A storey gives way in this part where the part has walls in it and holds something above it, or something just cut
	// off above it is coming down onto them. That falls onto what is left of the storey, which flies out from under it,
	// and it lands whole like the rest of a collapse.
	int storeys[NB_MAX_STOREYS];
	int storeyCount = 0;
	for ( int f = 0; f < failingCount; ++f )
	{
		const nbStorey* storey = destructible->storeys + failing[f];
		bool holds = false;
		float walls = 0.0f;
		for ( int k = 0; k < n; ++k )
		{
			bool inside = heights[k] > storey->low && heights[k] < storey->high;
			walls += inside ? world->chunks.data[chunks[k]].shape->volume : 0.0f;
			holds = holds || heights[k] >= storey->high;
		}

		// Of a storey that gave way before, a few fragments left under a part that came down do not count
		bool before = ( destructible->collapsedStoreys & ( 1u << failing[f] ) ) != 0;
		if ( walls == 0.0f || ( before && walls < NB_STOREY_REMAINS * storey->volume ) )
		{
			continue;
		}

		for ( int i = 0; i < freshCount; ++i )
		{
			nbActor* part = world->actors.data + fresh[i];
			if ( freshLows[i] > storey->low && freshHighs[i] > storey->high && part->volume >= 0.1f * storey->volume )
			{
				part->fromCollapse = true;
				part->isBuildingPart = true;
				holds = true;
			}
		}

		if ( holds )
		{
			storeys[storeyCount++] = failing[f];
		}
	}

	if ( storeyCount == 0 )
	{
		return 0;
	}

	// The other parts of the building check their storeys in the next update
	for ( int a = destructible->headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
	{
		if ( a != actorIndex && ( world->actors.data[a].isStatic || world->actors.data[a].isBuildingPart ) )
		{
			nbMarkSupport( world, a );
		}
	}

	// Every chunk flies out of a storey that gives way, -1, or stays with the stretch of the building between two of them
	int* stretches = nbArena_AllocArray( &world->arena, int, n );
	for ( int k = 0; k < n; ++k )
	{
		stretches[k] = 0;
		for ( int t = 0; t < storeyCount && heights[k] > destructible->storeys[storeys[t]].low; ++t )
		{
			stretches[k] = heights[k] >= destructible->storeys[storeys[t]].high ? t + 1 : -1;
			if ( stretches[k] < 0 )
			{
				break;
			}
		}
	}

	nbOverload* overloads = nbArena_AllocArray( &world->arena, nbOverload, n + bondKeys );
	int overloadCount = 0;
	for ( int k = 0; k < n; ++k )
	{
		if ( stretches[k] < 0 )
		{
			overloads[overloadCount++] = (nbOverload){ NB_NULL_INDEX, chunks[k], 1.0f, true };
			continue;
		}

		const nbChunk* chunk = world->chunks.data + chunks[k];
		for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
		{
			int bondIndex = key >> 1;
			const nbBond* bond = world->bonds.data + bondIndex;
			int side = key & 1;
			key = bond->nextKey[side];

			int other = world->chunks.data[bond->chunk[side ^ 1]].scratch;
			if ( other > k && stretches[other] >= 0 && stretches[other] != stretches[k] )
			{
				overloads[overloadCount++] = (nbOverload){ bondIndex, NB_NULL_INDEX, 1.0f, false };
			}
		}
	}

	// What comes down over a storey turns toward the side it lost, away from the middle of what was left of the storey
	b3Vec3 spins[NB_MAX_STOREYS + 1];
	for ( int t = 0; t <= storeyCount; ++t )
	{
		spins[t] = b3Vec3_zero;
	}

	for ( int t = 0; t < storeyCount; ++t )
	{
		const nbStorey* storey = destructible->storeys + storeys[t];
		float mass = 0.0f, left = 0.0f;
		b3Vec3 moment = b3Vec3_zero, middle = b3Vec3_zero;
		b3AABB box = { { FLT_MAX, FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX, -FLT_MAX } };
		for ( int k = 0; k < n; ++k )
		{
			const nbChunk* chunk = world->chunks.data + chunks[k];
			if ( stretches[k] == t + 1 )
			{
				float chunkMass = chunk->shape->volume * nbGetChunkMaterial( world, chunk )->density;
				mass += chunkMass;
				moment = b3MulAdd( moment, chunkMass, chunk->shape->centroid );
				box = b3AABB_Union( box, chunk->shape->bounds );
			}
			else if ( stretches[k] < 0 && heights[k] > storey->low && heights[k] < storey->high )
			{
				left += chunk->shape->volume;
				middle = b3MulAdd( middle, chunk->shape->volume, chunk->shape->centroid );
			}
		}

		if ( mass <= 0.0f || left <= 0.0f )
		{
			continue;
		}

		middle = b3MulSV( 1.0f / left, middle );
		b3Vec3 offset = b3Sub( b3MulSV( 1.0f / mass, moment ), middle );
		offset.y = 0.0f;
		float distance = b3Length( offset );
		if ( distance < 1.0e-3f )
		{
			continue;
		}

		b3Vec3 direction = b3MulSV( 1.0f / distance, offset );
		b3Vec3 size = b3Sub( box.upperBound, box.lowerBound );
		float reach = 0.5f * ( b3AbsFloat( direction.x ) * size.x + b3AbsFloat( direction.z ) * size.z );
		float speed = NB_TILT_SPEED * b3MinFloat( distance / b3MaxFloat( reach, 1.0e-3f ), 1.0f );
		spins[t + 1] = b3RotateVector( transform.q, b3MulSV( speed, b3Cross( b3Vec3_axisY, direction ) ) );
	}

	// Every stretch that comes down stays a building, and so does what is left of a part that came down before
	nbTilt* tilts = nbArena_AllocArray( &world->arena, nbTilt, n );
	int tiltCount = 0;
	for ( int k = 0; k < n; ++k )
	{
		if ( stretches[k] > 0 || ( stretches[k] == 0 && actor->isStatic == false ) )
		{
			tilts[tiltCount++] = (nbTilt){ chunks[k], spins[stretches[k]] };
		}
	}

	// Each storey counts once, also when its remains fly out from under a part that came down before
	nbDestructible* building = world->destructibles.data + actor->destructibleIndex;
	for ( int t = 0; t < storeyCount; ++t )
	{
		uint32_t bit = 1u << storeys[t];
		world->stats.collapsedStoreyCount += ( building->collapsedStoreys & bit ) == 0 ? 1 : 0;
		building->collapsedStoreys |= bit;
	}
	*overloadsOut = overloads;
	*tiltsOut = tilts;
	*tiltCountOut = tiltCount;
	return overloadCount;
}

// Parts that fell off a structure outside an impact start at rest, like the structure. Only an impact hands new actors
// their velocity, see nbApplyVelocities, and would otherwise stop them when it hits them later.
static void nbStartAtRest( nbWorld* world, int exceptActor )
{
	for ( int i = 0; i < world->touchedActors.count; ++i )
	{
		int actorIndex = world->touchedActors.data[i];
		if ( actorIndex != exceptActor )
		{
			world->actors.data[actorIndex].isNew = false;
		}
	}
	world->touchedActors.count = 0;
}

// Where a chunk is free to leave its wall, in the frame of its destructible: the horizontal axis along which most of its
// surface is bonded to nothing, the faces of the wall it sits in. The surface of its faces minus what its bonds cover
// tells, and the vector sum of that free surface tells whether one face is free or both. With one, the axis points
// through it. With both, each fragment leaves through the nearer one, and the axis points away from the origin of the
// destructible, out of a house. A chunk without a free side gets a zero axis.
typedef struct nbFreeSide
{
	b3Vec3 axis;
	bool bothSides;
} nbFreeSide;

static nbFreeSide nbFindFreeSide( const nbWorld* world, const nbChunk* chunk, b3Vec3 localUp )
{
	b3Vec3 side1 = b3Perp( localUp );
	b3Vec3 side2 = b3Cross( localUp, side1 );
	const nbShape* shape = chunk->shape;

	// Horizontal second moments of the free surface, [a b; b c], and its vector area
	float a = 0.0f, b = 0.0f, c = 0.0f;
	for ( int f = 0; f < shape->faceCount; ++f )
	{
		const nbFace* face = shape->faces + f;
		b3Vec3 origin = shape->vertices[shape->indices[face->firstIndex]];
		b3Vec3 sum = b3Vec3_zero;
		for ( int k = 1; k + 1 < face->indexCount; ++k )
		{
			b3Vec3 p = b3Sub( shape->vertices[shape->indices[face->firstIndex + k]], origin );
			b3Vec3 q = b3Sub( shape->vertices[shape->indices[face->firstIndex + k + 1]], origin );
			sum = b3Add( sum, b3Cross( p, q ) );
		}

		float area = 0.5f * b3Length( sum );
		float x = b3Dot( face->plane.normal, side1 );
		float z = b3Dot( face->plane.normal, side2 );
		a += area * x * x;
		b += area * x * z;
		c += area * z * z;
	}
	float total = a + c;

	float freeX = 0.0f, freeZ = 0.0f;
	for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
	{
		const nbBond* bond = world->bonds.data + ( key >> 1 );
		int side = key & 1;
		key = bond->nextKey[side];
		b3Vec3 normal = side == 0 ? bond->normal : b3Neg( bond->normal );
		float x = b3Dot( normal, side1 );
		float z = b3Dot( normal, side2 );
		a -= bond->area * x * x;
		b -= bond->area * x * z;
		c -= bond->area * z * z;
		freeX -= bond->area * x;
		freeZ -= bond->area * z;
	}

	// The eigenvector of the larger eigenvalue, from whichever row of the shifted matrix is longer
	nbFreeSide result = { b3Vec3_zero, false };
	float mean = 0.5f * ( a + c );
	float spread = sqrtf( 0.25f * ( a - c ) * ( a - c ) + b * b );
	float lambda = mean + spread;
	if ( lambda <= 0.1f * total )
	{
		return result;
	}

	float u1 = b, v1 = lambda - a, u2 = lambda - c, v2 = b;
	b3Vec3 axis = u1 * u1 + v1 * v1 >= u2 * u2 + v2 * v2 ? b3MulAdd( b3MulSV( u1, side1 ), v1, side2 )
														 : b3MulAdd( b3MulSV( u2, side1 ), v2, side2 );
	axis = b3Normalize( axis );
	float net = freeX * b3Dot( axis, side1 ) + freeZ * b3Dot( axis, side2 );
	result.bothSides = b3AbsFloat( net ) <= 0.5f * lambda;
	float sign = result.bothSides ? b3Dot( axis, chunk->shape->centroid ) : net;
	result.axis = sign < 0.0f ? b3Neg( axis ) : axis;
	return result;
}

// Break a chunk out of whatever holds it and burst it into fragments that scatter, the way an overloaded column
// crumbles, and that are pushed out of the wall. Anchored chunks come off their anchors too.
static void nbCrushChunk( nbWorld* world, int chunkIndex, b3Vec3 up, float push )
{
	nbBeginOperation( world );
	nbChunk* chunk = world->chunks.data + chunkIndex;
	b3Quat rotation = nbActor_GetTransform( world, world->actors.data + chunk->actorIndex ).q;
	nbFreeSide freeSide = nbFindFreeSide( world, chunk, b3InvRotateVector( rotation, up ) );
	while ( chunk->headBondKey != NB_NULL_INDEX )
	{
		nbDestroyBond( world, chunk->headBondKey >> 1 );
	}

	int actorIndex = chunk->actorIndex;
	if ( world->actors.data[actorIndex].isStatic )
	{
		chunk->flags &= ~nb_chunkAnchored;
		b3WorldTransform transform = world->destructibles.data[chunk->destructibleIndex].transform;
		nbDetachChunks( world, actorIndex, &chunkIndex, 1, transform, b3Vec3_zero, b3Vec3_zero, transform.p );
	}

	nbImpactResult result = { 0 };
	nbSplitActors( world, &result );
	nbCommitPhysics( world );
	nbStartAtRest( world, world->chunks.data[chunkIndex].actorIndex );

	chunk = world->chunks.data + chunkIndex;
	b3WorldTransform transform = nbActor_GetTransform( world, world->actors.data + chunk->actorIndex );
	nbImpactDef def = { 0 };
	def.point = b3TransformWorldPoint( transform, chunk->shape->centroid );
	def.radius = chunk->shape->radius;
	def.damage = 1.0e30f;
	def.ejectSpeed = NB_CRUSH_SPEED;
	def.fragmentCount = NB_CRUSH_FRAGMENTS;
	world->pushAxis = b3RotateVector( rotation, freeSide.axis );
	world->pushSpeed = b3LengthSquared( freeSide.axis ) > 0.0f ? push : 0.0f;
	world->pushBothSides = freeSide.bothSides;

	// All fragments fly, also the one that keeps the body. The chunk may lie in a part that just fell off and already
	// started at rest.
	nbActor* actor = world->actors.data + chunk->actorIndex;
	if ( actor->isNew == false )
	{
		actor->isNew = true;
		actor->sourceLinearVelocity = b3Body_GetLinearVelocity( actor->bodyId );
		actor->sourceAngularVelocity = b3Body_GetAngularVelocity( actor->bodyId );
		actor->sourceCenter = b3Body_GetWorldCenter( actor->bodyId );
	}

	uint16_t generation = chunk->generation;
	nbApplyImpact( world, &def, chunk->actorIndex );

	// A chunk too small to burst leaves the wall whole
	chunk = world->chunks.data + chunkIndex;
	if ( chunk->shape != NULL && chunk->generation == generation )
	{
		actor = world->actors.data + chunk->actorIndex;
		actor->isNew = false;
		b3Body_SetLinearVelocity( actor->bodyId,
								  b3MulAdd( b3Body_GetLinearVelocity( actor->bodyId ), world->pushSpeed, world->pushAxis ) );
	}
	world->pushSpeed = 0.0f;
}

// Check the structures that changed. Overloaded bonds break all at once, parts that lost their way to the anchors fall
// as one piece, and chunks crushed under their load burst. Each change marks the structure again, so it gives way
// update after update until what is left carries itself.
static void nbCheckSupports( nbWorld* world )
{
	if ( world->supportChecks.count == 0 )
	{
		return;
	}

	b3Vec3 gravityVector = b3World_GetGravity( world->physicsWorld );
	float gravity = b3Length( gravityVector );
	if ( ( world->def.supportScale <= 0.0f && world->def.storeySupport <= 0.0f ) || gravity <= 0.0f )
	{
		for ( int i = 0; i < world->supportChecks.count; ++i )
		{
			world->actors.data[world->supportChecks.data[i]].supportDirty = false;
		}
		world->supportChecks.count = 0;
		return;
	}
	b3Vec3 up = b3MulSV( -1.0f / gravity, gravityVector );

	// Structures that change during the checks wait for the next update
	nbIntArray queue = world->supportChecks;
	world->supportChecks = world->supportQueue;
	world->supportChecks.count = 0;
	world->supportQueue = queue;

	int visited = 0;
	for ( int q = 0; q < queue.count; ++q )
	{
		int actorIndex = queue.data[q];
		nbActor* actor = world->actors.data + actorIndex;
		if ( actor->isFree || ( actor->isStatic == false && actor->isBuildingPart == false ) || actor->supportDirty == false ||
			 actor->chunkCount == 0 )
		{
			actor->supportDirty = false;
			continue;
		}

		if ( visited > 0 && visited + actor->chunkCount > NB_SUPPORT_BUDGET )
		{
			nbArray_Push( world->supportChecks, actorIndex );
			continue;
		}
		visited += actor->chunkCount;
		actor->supportDirty = false;

		// A building gives way storey by storey and never above a hole, only other structures take the load check
		nbBeginOperation( world );
		nbOverload* overloads = NULL;
		nbTilt* tilts = NULL;
		int tiltCount = 0;
		bool collapsed = false;
		int overloadCount = nbCheckStoreys( world, actorIndex, up, &overloads, &tilts, &tiltCount );
		bool storey = overloadCount > 0;
		if ( storey )
		{
			collapsed = true;
		}
		else if ( world->def.supportScale > 0.0f && actor->isStatic &&
				  world->destructibles.data[actor->destructibleIndex].storeyCount == 0 )
		{
			overloadCount = nbCheckSupport( world, actorIndex, gravity, up, &overloads, &collapsed );
			for ( int k = 0; k < overloadCount; ++k )
			{
				world->stats.overloadedBondCount += overloads[k].bondIndex != NB_NULL_INDEX ? 1 : 0;
			}
		}

		if ( overloadCount == 0 )
		{
			continue;
		}

		// A part that came down and froze comes back to life to give way
		if ( actor->isRubble )
		{
			nbThawActor( world, actorIndex );
		}

		// A collapse throws chunks out whole through the side of the wall they are free on. The side needs the bonds, so
		// it is found before any bond breaks. Where no side is free the chunk leaves away from the middle of the
		// structure.
		b3WorldTransform transform = nbActor_GetTransform( world, actor );
		b3Vec3 localUp = b3InvRotateVector( transform.q, up );
		int* ejects = nbArena_AllocArray( &world->arena, int, overloadCount );
		b3Vec3* ejectAxes = nbArena_AllocArray( &world->arena, b3Vec3, overloadCount );
		int ejectCount = 0;
		for ( int k = 0; k < overloadCount; ++k )
		{
			if ( overloads[k].eject == false )
			{
				continue;
			}

			const nbChunk* chunk = world->chunks.data + overloads[k].crushChunk;
			b3Vec3 axis = nbFindFreeSide( world, chunk, localUp ).axis;
			if ( b3LengthSquared( axis ) == 0.0f )
			{
				axis = b3MulSub( chunk->shape->centroid, b3Dot( chunk->shape->centroid, localUp ), localUp );
				axis = b3LengthSquared( axis ) > 0.0f ? b3Normalize( axis ) : b3Perp( localUp );
			}
			ejects[ejectCount] = overloads[k].crushChunk;
			ejectAxes[ejectCount] = b3RotateVector( transform.q, axis );
			ejectCount += 1;
		}

		// Of the rest the most overloaded bonds crush first
		int crushes[NB_MAX_CRUSHES];
		uint16_t crushGenerations[NB_MAX_CRUSHES];
		int crushCount = 0;
		for ( ; crushCount < NB_MAX_CRUSHES; ++crushCount )
		{
			int best = NB_NULL_INDEX;
			for ( int k = 0; k < overloadCount; ++k )
			{
				int chunkIndex = overloads[k].crushChunk;
				bool taken = chunkIndex == NB_NULL_INDEX || overloads[k].eject;
				for ( int c = 0; c < crushCount && taken == false; ++c )
				{
					taken = crushes[c] == chunkIndex;
				}
				for ( int e = 0; e < ejectCount && taken == false; ++e )
				{
					taken = ejects[e] == chunkIndex;
				}
				if ( taken == false && ( best == NB_NULL_INDEX || overloads[k].utilization > overloads[best].utilization ) )
				{
					best = k;
				}
			}
			if ( best == NB_NULL_INDEX )
			{
				break;
			}
			crushes[crushCount] = overloads[best].crushChunk;
			crushGenerations[crushCount] = world->chunks.data[crushes[crushCount]].generation;
		}

		for ( int k = 0; k < overloadCount; ++k )
		{
			if ( overloads[k].bondIndex != NB_NULL_INDEX && world->bonds.data[overloads[k].bondIndex].chunk[0] != NB_NULL_INDEX )
			{
				nbDestroyBond( world, overloads[k].bondIndex );
			}
		}

		// The remains of a storey fly out through the rubble lying around it, all of it comes back to life. Otherwise they
		// would get stuck against the frozen pieces of an earlier collapse, and what stands on them would stay up. The
		// budget leaves the debris there alone for a moment, or it would freeze where the storey held it.
		if ( storey && ejectCount > 0 )
		{
			b3AABB zone = b3Shape_GetAABB( world->chunks.data[ejects[0]].shapeId );
			for ( int e = 1; e < ejectCount; ++e )
			{
				zone = b3AABB_Union( zone, b3Shape_GetAABB( world->chunks.data[ejects[e]].shapeId ) );
			}
			b3Vec3 clearance = b3MulAdd( (b3Vec3){ NB_CLEARANCE, NB_CLEARANCE, NB_CLEARANCE }, -NB_CLEARANCE, b3Abs( up ) );
			zone.lowerBound = b3Sub( zone.lowerBound, clearance );
			zone.upperBound = b3Add( zone.upperBound, clearance );
			nbThawContext filter = { .skipActor = NB_NULL_INDEX, .holder = NB_NULL_INDEX, .collapse = true };
			nbThawRubbleInBoxes( world, &zone, 1, b3Vec3_zero, 0.0f, &filter );
		}

		// What a collapse drops comes from it
		world->collapsing = collapsed || ejectCount > 0;

		// The parts of a cell leave as one body, see nbBond::sibling, along the mean of the free sides of the parts
		world->searchStamp += 1;
		uint32_t ejectStamp = world->searchStamp;
		int* ejectSets = nbArena_AllocArray( &world->arena, int, ejectCount );
		for ( int e = 0; e < ejectCount; ++e )
		{
			nbChunk* chunk = world->chunks.data + ejects[e];
			chunk->searchStamp = ejectStamp;
			chunk->scratch = e;
			ejectSets[e] = NB_NULL_INDEX;
		}

		int* setChunks = nbArena_AllocArray( &world->arena, int, ejectCount );
		int* setHeads = nbArena_AllocArray( &world->arena, int, ejectCount );
		b3Vec3* setVelocities = nbArena_AllocArray( &world->arena, b3Vec3, ejectCount );
		int setCount = 0;
		for ( int e = 0; e < ejectCount; ++e )
		{
			if ( ejectSets[e] != NB_NULL_INDEX )
			{
				continue;
			}

			int count = 0;
			ejectSets[e] = setCount;
			setChunks[count++] = ejects[e];
			b3Vec3 axis = b3Vec3_zero;
			for ( int q = 0; q < count; ++q )
			{
				const nbChunk* chunk = world->chunks.data + setChunks[q];
				axis = b3Add( axis, ejectAxes[chunk->scratch] );
				for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
				{
					const nbBond* bond = world->bonds.data + ( key >> 1 );
					int side = key & 1;
					key = bond->nextKey[side];

					const nbChunk* other = world->chunks.data + bond->chunk[side ^ 1];
					if ( bond->sibling && other->searchStamp == ejectStamp && ejectSets[other->scratch] == NB_NULL_INDEX )
					{
						ejectSets[other->scratch] = setCount;
						setChunks[count++] = bond->chunk[side ^ 1];
					}
				}
			}

			for ( int q = 0; q < count; ++q )
			{
				nbChunk* chunk = world->chunks.data + setChunks[q];
				for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
				{
					int bondIndex = key >> 1;
					const nbBond* bond = world->bonds.data + bondIndex;
					int side = key & 1;
					key = bond->nextKey[side];

					const nbChunk* other = world->chunks.data + bond->chunk[side ^ 1];
					if ( bond->sibling == false || other->searchStamp != ejectStamp || ejectSets[other->scratch] != setCount )
					{
						nbDestroyBond( world, bondIndex );
					}
				}
				chunk->flags &= ~nb_chunkAnchored;
			}

			nbDetachChunks( world, actorIndex, setChunks, count, transform, b3Vec3_zero, b3Vec3_zero, transform.p );
			axis = b3LengthSquared( axis ) > 1.0e-4f ? b3Normalize( axis ) : ejectAxes[e];
			setHeads[setCount] = setChunks[0];
			setVelocities[setCount] = b3MulSV( NB_COLLAPSE_PUSH, axis );
			setCount += 1;
		}

		nbImpactResult result = { 0 };
		nbSplitActors( world, &result );
		nbCommitPhysics( world );
		nbStartAtRest( world, NB_NULL_INDEX );
		world->collapsing = false;

		for ( int s = 0; s < setCount; ++s )
		{
			const nbActor* ejected = world->actors.data + world->chunks.data[setHeads[s]].actorIndex;
			b3Body_SetLinearVelocity( ejected->bodyId, setVelocities[s] );
		}

		// What comes down over a storey stays a building and starts to turn toward the side it lost
		for ( int t = 0; t < tiltCount; ++t )
		{
			nbActor* part = world->actors.data + world->chunks.data[tilts[t].chunkIndex].actorIndex;
			if ( part->isStatic == false )
			{
				part->isBuildingPart = true;
				if ( b3LengthSquared( tilts[t].angularVelocity ) > 0.0f )
				{
					b3Body_SetAngularVelocity( part->bodyId, tilts[t].angularVelocity );
				}
			}
		}

		for ( int c = 0; c < crushCount; ++c )
		{
			const nbChunk* chunk = world->chunks.data + crushes[c];
			if ( chunk->shape != NULL && chunk->generation == crushGenerations[c] )
			{
				nbCrushChunk( world, crushes[c], up, NB_CRUSH_PUSH );
			}
		}
	}
	queue.count = 0;
	world->supportQueue = queue;
}

void nbWorld_SetSupportScale( nbWorldId worldId, float scale )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return;
	}

	world->def.supportScale = scale > 0.0f ? scale : 0.0f;
	for ( int i = 0; i < world->actors.count; ++i )
	{
		const nbActor* actor = world->actors.data + i;
		if ( actor->isFree == false && actor->isStatic && actor->chunkCount > 0 )
		{
			nbMarkSupport( world, i );
		}
	}
}

void nbWorld_SetStoreySupport( nbWorldId worldId, float fraction )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return;
	}

	world->def.storeySupport = b3ClampFloat( fraction, 0.0f, 1.0f );
	for ( int i = 0; i < world->actors.count; ++i )
	{
		const nbActor* actor = world->actors.data + i;
		if ( actor->isFree == false && ( actor->isStatic || actor->isBuildingPart ) && actor->chunkCount > 0 )
		{
			nbMarkSupport( world, i );
		}
	}
}

nbWorldDef nbDefaultWorldDef( void )
{
	nbWorldDef def = { 0 };
	def.physicsWorld = b3_nullWorldId;
	def.maxDebrisBodies = 1500;
	def.enableRubble = true;
	def.debrisSleepThreshold = 0.12f;
	def.killDepth = -100.0f;
	def.supportScale = 0.0f;
	def.storeySupport = 0.5f;
	def.collisionSpeedThreshold = 4.0f;
	def.collisionDamageScale = 12.0f;
	def.collisionRadiusScale = 0.035f;
	def.maxCollisionImpactsPerUpdate = 4;
	def.maxFragmentsPerImpact = 160;
	def.fragmentScale = 1.0f;
	def.collisionPassThrough = 0.6f;
	def.workerCount = 1;
	def.internalValue = NB_SECRET_COOKIE;
	return def;
}

// Use the application's task system if it has one, otherwise start threads when more than one worker is wanted
static void nbStartWorkers( nbWorld* world, int workerCount )
{
	workerCount = workerCount < 1 ? 1 : ( workerCount > NB_MAX_WORKERS ? NB_MAX_WORKERS : workerCount );
	world->workerCount = workerCount;
	world->def.workerCount = workerCount;

	if ( world->def.enqueueTask != NULL && world->def.finishTask != NULL )
	{
		world->enqueueTask = world->def.enqueueTask;
		world->finishTask = world->def.finishTask;
		world->userTaskContext = world->def.userTaskContext;
	}
	else if ( workerCount > 1 )
	{
		world->scheduler = nbCreateScheduler( workerCount - 1 );
		world->enqueueTask = nbSchedulerEnqueueTask;
		world->finishTask = nbSchedulerFinishTask;
		world->userTaskContext = world->scheduler;
	}

	for ( int i = 0; i < workerCount; ++i )
	{
		nbArena_Create( world->workerArenas + i, 64 * 1024 );
	}
}

static void nbStopWorkers( nbWorld* world )
{
	nbDestroyScheduler( world->scheduler );
	world->scheduler = NULL;
	world->enqueueTask = NULL;
	world->finishTask = NULL;
	world->userTaskContext = NULL;

	for ( int i = 0; i < world->workerCount; ++i )
	{
		nbArena_Destroy( world->workerArenas + i );
	}
	world->workerCount = 0;
}

typedef struct nbFractureTask
{
	nbFractureJob* jobs;
	const int* itemJobs;
	const int* itemCells;
	int itemCount;
	int siteCapacity;

	// Shared counter that hands out the work items
	int* nextItem;

	nbArena* arena;
	nbFractureCounters counters;
} nbFractureTask;

static void nbFractureTaskMain( void* context )
{
	nbFractureTask* task = context;
	nbCellScratch scratch;
	nbCellScratch_Create( &scratch, task->arena, task->siteCapacity );

	for ( ;; )
	{
		int item = nbAtomicFetchAddInt( task->nextItem, 1 );
		if ( item >= task->itemCount )
		{
			break;
		}

		nbComputeCell( task->jobs + task->itemJobs[item], task->itemCells[item], task->arena, &scratch, &task->counters );
	}
}

void nbRunFractureJobs( nbWorld* world, nbFractureJob* jobs, int jobCount )
{
	int itemCount = 0;
	int siteCapacity = 0;
	for ( int i = 0; i < jobCount; ++i )
	{
		jobs[i].cells = nbArena_AllocArray( &world->arena, nbCell, jobs[i].siteCount );
		itemCount += jobs[i].siteCount;
		siteCapacity = jobs[i].siteCount > siteCapacity ? jobs[i].siteCount : siteCapacity;
	}

	if ( itemCount == 0 )
	{
		return;
	}

	// Every cell is a work item. Workers grab the next item when they finish one, which balances
	// cheap cells at the impact against the larger cells further out.
	int* itemJobs = nbArena_AllocArray( &world->arena, int, itemCount );
	int* itemCells = nbArena_AllocArray( &world->arena, int, itemCount );
	int item = 0;
	for ( int i = 0; i < jobCount; ++i )
	{
		for ( int k = 0; k < jobs[i].siteCount; ++k )
		{
			itemJobs[item] = i;
			itemCells[item] = k;
			item += 1;
		}
	}

	// Waking threads costs more than a handful of cells
	int taskCount = world->enqueueTask != NULL && itemCount >= 16 ? world->workerCount : 1;
	taskCount = taskCount < itemCount ? taskCount : itemCount;

	int nextItem = 0;
	nbFractureTask tasks[NB_MAX_WORKERS];
	void* userTasks[NB_MAX_WORKERS];
	for ( int i = 0; i < taskCount; ++i )
	{
		tasks[i] = (nbFractureTask){
			.jobs = jobs,
			.itemJobs = itemJobs,
			.itemCells = itemCells,
			.itemCount = itemCount,
			.siteCapacity = siteCapacity,
			.nextItem = &nextItem,
			.arena = world->workerArenas + i,
		};
	}

	for ( int i = 1; i < taskCount; ++i )
	{
		userTasks[i] = world->enqueueTask( nbFractureTaskMain, tasks + i, world->userTaskContext, "nebenan fracture" );
	}

	// The calling thread works as well
	nbFractureTaskMain( tasks + 0 );

	for ( int i = 1; i < taskCount; ++i )
	{
		if ( userTasks[i] != NULL )
		{
			world->finishTask( userTasks[i], world->userTaskContext );
		}
	}

	for ( int i = 0; i < taskCount; ++i )
	{
		world->stats.hullFallbackCount += tasks[i].counters.hullFallbackCount;
	}
}

void nbWorld_SetWorkerCount( nbWorldId worldId, int count )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return;
	}

	nbStopWorkers( world );
	nbStartWorkers( world, count );
}

void nbWorld_SetFragmentScale( nbWorldId worldId, float scale )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return;
	}

	world->def.fragmentScale = scale > 0.0f ? scale : 1.0f;
}

void nbWorld_SetDebrisBudget( nbWorldId worldId, int maxDebrisBodies )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return;
	}

	world->def.maxDebrisBodies = b3MaxInt( maxDebrisBodies, 0 );
}

nbWorldId nbCreateWorld( const nbWorldDef* def )
{
	NB_ASSERT( def->internalValue == NB_SECRET_COOKIE );
	if ( b3World_IsValid( def->physicsWorld ) == false )
	{
		return nb_nullWorldId;
	}

	int index = NB_NULL_INDEX;
	for ( int i = 0; i < NB_MAX_WORLDS; ++i )
	{
		if ( nb_worlds[i].inUse == false )
		{
			index = i;
			break;
		}
	}

	if ( index == NB_NULL_INDEX )
	{
		return nb_nullWorldId;
	}

	nbWorld* world = nb_worlds + index;
	uint16_t generation = world->generation;
	memset( world, 0, sizeof( nbWorld ) );
	world->generation = generation + 1;
	world->worldIndex = (uint16_t)index;
	world->inUse = true;
	world->def = *def;
	world->def.fragmentScale = def->fragmentScale > 0.0f ? def->fragmentScale : 1.0f;
	world->physicsWorld = def->physicsWorld;
	nbArena_Create( &world->arena, 256 * 1024 );
	nbStartWorkers( world, def->workerCount );

	return (nbWorldId){ (uint16_t)( index + 1 ), world->generation };
}

void nbDestroyWorld( nbWorldId worldId )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return;
	}

	if ( b3World_IsValid( world->physicsWorld ) )
	{
		for ( int i = 0; i < world->chunks.count; ++i )
		{
			nbChunk* chunk = world->chunks.data + i;
			if ( chunk->shape != NULL && ( chunk->flags & nb_chunkOwnsBody ) && b3Body_IsValid( chunk->bodyId ) )
			{
				b3DestroyBody( chunk->bodyId );
			}
		}

		for ( int i = 0; i < world->actors.count; ++i )
		{
			nbActor* actor = world->actors.data + i;
			if ( actor->isFree == false && B3_IS_NON_NULL( actor->bodyId ) && b3Body_IsValid( actor->bodyId ) )
			{
				b3DestroyBody( actor->bodyId );
			}
		}
	}

	for ( int i = 0; i < world->chunks.count; ++i )
	{
		nbShape_Destroy( world->chunks.data[i].shape );
	}

	nbArray_Free( world->chunks );
	nbArray_Free( world->bonds );
	nbArray_Free( world->actors );
	nbArray_Free( world->destructibles );
	nbArray_Free( world->freeChunks );
	nbArray_Free( world->freeBonds );
	nbArray_Free( world->freeActors );
	nbArray_Free( world->freeDestructibles );
	nbArray_Free( world->shapeToChunk );
	nbArray_Free( world->bodyToActor );
	nbArray_Free( world->debris );
	nbArray_Free( world->touchedChunks );
	nbArray_Free( world->touchedActors );
	nbArray_Free( world->splitSeeds );
	nbArray_Free( world->scratchList );
	nbArray_Free( world->actorList );
	nbArray_Free( world->supportChecks );
	nbArray_Free( world->supportQueue );
	for ( int i = 0; i < 2; ++i )
	{
		nbArray_Free( world->createdEvents[i] );
		nbArray_Free( world->destroyedEvents[i] );
		nbArray_Free( world->movedEvents[i] );
		nbArray_Free( world->exposedEvents[i] );
	}
	nbArray_Free( world->collisionImpacts );
	nbArena_Destroy( &world->arena );
	nbStopWorkers( world );

	uint16_t generation = world->generation;
	memset( world, 0, sizeof( nbWorld ) );
	world->generation = generation + 1;
}

bool nbWorld_IsValid( nbWorldId worldId )
{
	return nbGetWorldFromId( worldId ) != NULL;
}

nbEvents nbWorld_GetEvents( nbWorldId worldId )
{
	nbEvents events = { 0 };
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return events;
	}

	int readBuffer = world->eventBuffer;
	int writeBuffer = readBuffer ^ 1;

	// Clear the buffer that was handed out last time and start collecting into it
	world->createdEvents[writeBuffer].count = 0;
	world->destroyedEvents[writeBuffer].count = 0;
	world->movedEvents[writeBuffer].count = 0;
	world->exposedEvents[writeBuffer].count = 0;
	world->eventBuffer = writeBuffer;

	// The exposed chunks handed out now are reported again when they lose another bond
	for ( int i = 0; i < world->exposedEvents[readBuffer].count; ++i )
	{
		nbChunk* chunk = nbGetChunkFromId( world->exposedEvents[readBuffer].data[i], NULL );
		if ( chunk != NULL )
		{
			chunk->flags &= ~nb_chunkExposed;
		}
	}

	events.createdChunks = world->createdEvents[readBuffer].data;
	events.createdCount = world->createdEvents[readBuffer].count;
	events.destroyedChunks = world->destroyedEvents[readBuffer].data;
	events.destroyedCount = world->destroyedEvents[readBuffer].count;
	events.movedChunks = world->movedEvents[readBuffer].data;
	events.movedCount = world->movedEvents[readBuffer].count;
	events.exposedChunks = world->exposedEvents[readBuffer].data;
	events.exposedCount = world->exposedEvents[readBuffer].count;
	return events;
}

nbStats nbWorld_GetStats( nbWorldId worldId )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return (nbStats){ 0 };
	}

	nbStats stats = world->stats;
	stats.destructibleCount = world->destructibleCount;
	stats.chunkCount = world->chunkCount;
	stats.bondCount = world->bondCount;
	stats.staticBodyCount = world->staticActorCount;
	stats.dynamicBodyCount = world->dynamicActorCount;
	stats.debrisCount = world->debris.count;
	stats.rubbleCount = world->rubbleCount;
	stats.byteCount = nbGetByteCount();
	return stats;
}

nbChunkId nbWorld_GetChunkFromShape( nbWorldId worldId, b3ShapeId shapeId )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return nb_nullChunkId;
	}

	int chunkIndex = nbFindChunkFromShape( world, shapeId );
	if ( chunkIndex == NB_NULL_INDEX )
	{
		return nb_nullChunkId;
	}

	return nbMakeChunkId( world, chunkIndex );
}

void nbWorld_ClearDebris( nbWorldId worldId )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return;
	}

	while ( world->debris.count > 0 )
	{
		nbDestroyActor( world, world->debris.data[world->debris.count - 1] );
	}
}

// Collect Box3D hit events on chunks as collision impacts
static void nbCollectCollisionImpacts( nbWorld* world )
{
	world->collisionImpacts.count = 0;

	b3ContactEvents contactEvents = b3World_GetContactEvents( world->physicsWorld );
	for ( int i = 0; i < contactEvents.hitCount; ++i )
	{
		const b3ContactHitEvent* event = contactEvents.hitEvents + i;
		if ( event->approachSpeed < world->def.collisionSpeedThreshold )
		{
			continue;
		}

		int chunkA = nbFindChunkFromShape( world, event->shapeIdA );
		int chunkB = nbFindChunkFromShape( world, event->shapeIdB );
		if ( chunkA == NB_NULL_INDEX && chunkB == NB_NULL_INDEX )
		{
			continue;
		}

		// Reduced mass of the pair. Static bodies have zero mass and act as infinitely heavy.
		b3BodyId bodyA = b3Shape_GetBody( event->shapeIdA );
		b3BodyId bodyB = b3Shape_GetBody( event->shapeIdB );
		float massA = b3Body_GetType( bodyA ) == b3_dynamicBody ? b3Body_GetMass( bodyA ) : 0.0f;
		float massB = b3Body_GetType( bodyB ) == b3_dynamicBody ? b3Body_GetMass( bodyB ) : 0.0f;
		float mass;
		if ( massA > 0.0f && massB > 0.0f )
		{
			mass = massA * massB / ( massA + massB );
		}
		else
		{
			mass = massA > massB ? massA : massB;
		}

		float energy = 0.5f * mass * event->approachSpeed * event->approachSpeed;

		for ( int side = 0; side < 2; ++side )
		{
			int chunkIndex = side == 0 ? chunkA : chunkB;
			if ( chunkIndex == NB_NULL_INDEX )
			{
				continue;
			}

			// What came down in a collapse neither breaks nor breaks what it lands on
			const nbChunk* chunk = world->chunks.data + chunkIndex;
			const nbDestructible* destructible = world->destructibles.data + chunk->destructibleIndex;
			int otherIndex = side == 0 ? chunkB : chunkA;
			bool fromCollapse =
				world->actors.data[chunk->actorIndex].fromCollapse ||
				( otherIndex != NB_NULL_INDEX && world->actors.data[world->chunks.data[otherIndex].actorIndex].fromCollapse );
			if ( destructible->enableCollisionDamage == false || fromCollapse )
			{
				continue;
			}

			// Light knocks do nothing. The damage radius has to reach at least one fragment.
			float radius = world->def.collisionRadiusScale * nbCbrt( energy );
			if ( radius < nbGetFragmentSize( world, nbGetChunkMaterial( world, chunk ) ) )
			{
				continue;
			}

			// The hit normal points from shape A to shape B
			const nbActor* actor = world->actors.data + chunk->actorIndex;
			nbCollisionImpact impact = {
				.point = event->point,
				.normal = side == 0 ? event->normal : b3Neg( event->normal ),
				.energy = energy,
				.approachSpeed = event->approachSpeed,
				.otherBodyId = side == 0 ? bodyB : bodyA,
				.actorIndex = chunk->actorIndex,
				.actorGeneration = actor->generation,
			};
			nbArray_Push( world->collisionImpacts, impact );
		}
	}

	// Strongest impacts first, stable for determinism
	for ( int i = 1; i < world->collisionImpacts.count; ++i )
	{
		nbCollisionImpact key = world->collisionImpacts.data[i];
		int j = i - 1;
		while ( j >= 0 && world->collisionImpacts.data[j].energy < key.energy )
		{
			world->collisionImpacts.data[j + 1] = world->collisionImpacts.data[j];
			j -= 1;
		}
		world->collisionImpacts.data[j + 1] = key;
	}
}

// Rubble that a body other than debris hits comes back to life, a thrown boulder or a player. Debris landing on rubble
// leaves it alone: waking pieces in the middle of a pile gives each of them dozens of contacts, and in a collapse that
// costs Box3D more than all the rest. What rested on the rubble follows once it moves. Box3D already resolved the hit
// against a static body, so the body bounced off and the rubble got nothing. They share the momentum as if they had
// stuck together.
static void nbWakeHitRubble( nbWorld* world )
{
	if ( world->rubbleCount == 0 )
	{
		return;
	}

	int woken = 0;
	b3ContactEvents contactEvents = b3World_GetContactEvents( world->physicsWorld );
	for ( int i = 0; i < contactEvents.hitCount && woken < NB_MAX_RUBBLE_HITS; ++i )
	{
		const b3ContactHitEvent* event = contactEvents.hitEvents + i;
		if ( event->approachSpeed < NB_WAKE_SPEED )
		{
			continue;
		}

		for ( int side = 0; side < 2; ++side )
		{
			b3ShapeId shapeId = side == 0 ? event->shapeIdA : event->shapeIdB;
			b3ShapeId otherShapeId = side == 0 ? event->shapeIdB : event->shapeIdA;
			if ( b3Shape_IsValid( shapeId ) == false || b3Shape_IsValid( otherShapeId ) == false )
			{
				continue;
			}

			int chunkIndex = nbFindChunkFromShape( world, shapeId );
			if ( chunkIndex == NB_NULL_INDEX )
			{
				continue;
			}

			int actorIndex = world->chunks.data[chunkIndex].actorIndex;
			nbActor* actor = world->actors.data + actorIndex;
			b3BodyId otherBodyId = b3Shape_GetBody( otherShapeId );
			if ( actor->isRubble == false || b3Body_GetType( otherBodyId ) != b3_dynamicBody )
			{
				continue;
			}

			if ( nbFindActorFromBody( world, otherBodyId ) != NB_NULL_INDEX )
			{
				continue;
			}
			float otherMass = b3Body_GetMass( otherBodyId );

			nbThawActor( world, actorIndex );
			woken += 1;

			// The hit normal points from shape A to shape B, the body that hit pushes the rubble away from itself
			b3Vec3 push = side == 0 ? b3Neg( event->normal ) : event->normal;
			float speed = event->approachSpeed * otherMass / ( otherMass + b3Body_GetMass( actor->bodyId ) );
			b3Body_SetLinearVelocity( actor->bodyId, b3MulSV( speed, push ) );
			b3Vec3 velocity = b3Body_GetLinearVelocity( otherBodyId );
			b3Body_SetLinearVelocity( otherBodyId, b3MulAdd( velocity, speed - b3Dot( velocity, push ), push ) );
		}
	}
}

// Pieces that moved away from where they lay release the rubble resting on them
static void nbReleaseHeldRubble( nbWorld* world, b3Vec3 up )
{
	int rubbleCount = world->rubbleCount;
	for ( int i = 0; i < world->debris.count && rubbleCount - world->rubbleCount < NB_MAX_RELEASES; ++i )
	{
		int actorIndex = world->debris.data[i];
		nbActor* actor = world->actors.data + actorIndex;
		if ( actor->holdsRubble == false || actor->isRubble )
		{
			continue;
		}

		b3WorldTransform transform = b3Body_GetTransform( actor->bodyId );
		if ( nbPoseDistance( actor->holdPose, transform, actor->radius ) <= NB_HOLD_DISTANCE )
		{
			continue;
		}

		actor->holdsRubble = false;
		if ( world->rubbleCount > 0 )
		{
			// The rubble that rested on it lies higher than its center of mass did, or froze on it
			b3Pos center = b3TransformWorldPoint( actor->holdPose, actor->localCenter );
			nbThawContext filter = {
				.skipActor = actor->holdSource,
				.skipGeneration = actor->holdSourceGeneration,
				.holder = actorIndex,
				.grace = actor->isBuildingPart || actor->budgetAge < NB_FREEZE_GRACE,
			};
			nbThawRubbleInBoxes( world, &actor->holdBounds, 1, up, b3Dot( up, b3ToVec3( center ) ), &filter );
		}
	}
}

// A contact carries a body when one of its manifolds touches with the normal pointing down out of the body
static bool nbCarries( const b3ContactData* contact, bool isA, b3Vec3 up )
{
	for ( int m = 0; m < contact->manifoldCount; ++m )
	{
		const b3Manifold* manifold = contact->manifolds + m;
		b3Vec3 down = isA ? manifold->normal : b3Neg( manifold->normal );
		bool touching = false;
		for ( int p = 0; p < manifold->pointCount; ++p )
		{
			touching = touching || manifold->points[p].separation <= NB_TOUCH_GAP;
		}

		if ( touching && b3Dot( down, up ) < -NB_SUPPORT_NORMAL )
		{
			return true;
		}
	}
	return false;
}

// nbIsSupportedWithout for the carriers of one piece, each carrier searched once. The stamp changes with the piece.
static bool nbIsCarrierSupported( nbWorld* world, int carrierIndex, int actorIndex, uint32_t answerStamp )
{
	nbActor* carrier = world->actors.data + carrierIndex;
	if ( carrier->answerStamp != answerStamp )
	{
		carrier->answerStamp = answerStamp;
		carrier->supported = nbIsSupportedWithout( world, carrierIndex, actorIndex, answerStamp );
	}
	return carrier->supported;
}

// Remember a chunk a piece lies on, one per carrying body. NB_NULL_INDEX stands for a body that is no chunk.
static void nbAddCarrier( const nbWorld* world, nbCarrier* carriers, int* count, int chunkIndex, b3Vec3 point )
{
	int carrierIndex = chunkIndex != NB_NULL_INDEX ? world->chunks.data[chunkIndex].actorIndex : NB_NULL_INDEX;
	for ( int k = 0; k < *count; ++k )
	{
		int known = carriers[k].chunkIndex;
		if ( known == chunkIndex ||
			 ( known != NB_NULL_INDEX && chunkIndex != NB_NULL_INDEX && world->chunks.data[known].actorIndex == carrierIndex ) )
		{
			return;
		}
	}

	if ( *count < NB_MAX_CARRIERS )
	{
		nbCarrier* carrier = carriers + *count;
		carrier->point = point;
		carrier->chunkIndex = chunkIndex;
		carrier->generation = chunkIndex != NB_NULL_INDEX ? world->chunks.data[chunkIndex].generation : 0;
		*count += 1;
	}
}

// The closest point in world space of a contact, if it is within the gap. The center is the one of the body the contact
// belongs to.
static bool nbTouchPoint( const b3ContactData* contact, bool isA, b3Vec3 center, float gap, b3Vec3* touch )
{
	float closest = gap;
	bool found = false;
	for ( int m = 0; m < contact->manifoldCount; ++m )
	{
		const b3Manifold* manifold = contact->manifolds + m;
		for ( int p = 0; p < manifold->pointCount; ++p )
		{
			const b3ManifoldPoint* point = manifold->points + p;
			if ( point->separation <= closest )
			{
				closest = point->separation;
				*touch = b3Add( center, isA ? point->anchorA : point->anchorB );
				found = true;
			}
		}
	}
	return found;
}

// A larger moving body that presses into a piece deeper than NB_SQUEEZE_DEPTH, a null id if there is none
static b3BodyId nbFindSqueezer( const nbWorld* world, const nbActor* actor, const b3ContactData* contacts, int contactCount )
{
	for ( int c = 0; c < contactCount; ++c )
	{
		bool isA = B3_ID_EQUALS( b3Shape_GetBody( contacts[c].shapeIdA ), actor->bodyId );
		b3BodyId otherBodyId = b3Shape_GetBody( isA ? contacts[c].shapeIdB : contacts[c].shapeIdA );
		float depth = 0.0f;
		for ( int m = 0; m < contacts[c].manifoldCount; ++m )
		{
			const b3Manifold* manifold = contacts[c].manifolds + m;
			for ( int p = 0; p < manifold->pointCount; ++p )
			{
				depth = b3MaxFloat( depth, -manifold->points[p].separation );
			}
		}

		if ( depth <= NB_SQUEEZE_DEPTH || b3Body_GetType( otherBodyId ) != b3_dynamicBody )
		{
			continue;
		}

		int other = nbFindActorFromBody( world, otherBodyId );
		if ( other == NB_NULL_INDEX || world->actors.data[other].volume > actor->volume )
		{
			return otherBodyId;
		}
	}
	return b3_nullBodyId;
}

// Freeze a piece with the chunks it lies on
static void nbFreezeOnCarriers( nbWorld* world, int actorIndex, const nbCarrier* carriers, int count )
{
	nbFreezeActor( world, actorIndex );
	nbActor* actor = world->actors.data + actorIndex;
	actor->carrierCount = count;
	for ( int k = 0; k < count; ++k )
	{
		actor->carriers[k] = carriers[k];
	}
}

// Points seen from above where a piece touches what holds it, relative to its center of mass. When the buffer is full
// only the convex hull stays.
#define NB_FAN_POINTS 64

typedef struct nbSupportFan
{
	b3Vec3 u, v;
	b3Vec2 points[NB_FAN_POINTS];
	int count;
} nbSupportFan;

static nbSupportFan nbMakeSupportFan( b3Vec3 up )
{
	b3Vec3 side = b3AbsFloat( up.x ) < 0.9f ? (b3Vec3){ 1.0f, 0.0f, 0.0f } : (b3Vec3){ 0.0f, 1.0f, 0.0f };
	nbSupportFan fan;
	fan.u = b3Normalize( b3Cross( up, side ) );
	fan.v = b3Cross( up, fan.u );
	fan.count = 0;
	return fan;
}

// Convex hull by the monotone chain, counter clockwise, in place. Returns the vertex count.
static int nbConvexHull2( b3Vec2* points, int count )
{
	if ( count < 3 )
	{
		return count;
	}

	b3Vec2 sorted[NB_FAN_POINTS];
	memcpy( sorted, points, sizeof( b3Vec2 ) * (size_t)count );
	qsort( sorted, (size_t)count, sizeof( b3Vec2 ), nbComparePoints );

	b3Vec2 hull[2 * NB_FAN_POINTS];
	int n = 0;
	for ( int i = 0; i < count; ++i )
	{
		while ( n >= 2 && nbCross2( hull[n - 2], hull[n - 1], sorted[i] ) <= 0.0f )
		{
			n -= 1;
		}
		hull[n++] = sorted[i];
	}
	for ( int i = count - 2, lower = n + 1; i >= 0; --i )
	{
		while ( n >= lower && nbCross2( hull[n - 2], hull[n - 1], sorted[i] ) <= 0.0f )
		{
			n -= 1;
		}
		hull[n++] = sorted[i];
	}

	n = b3MaxInt( n - 1, 1 );
	memcpy( points, hull, sizeof( b3Vec2 ) * (size_t)n );
	return n;
}

// Add the touching points of a contact. The anchors are relative to the centers of mass.
static void nbAddSupportPoints( nbSupportFan* fan, const b3ContactData* contact, bool isA )
{
	for ( int m = 0; m < contact->manifoldCount; ++m )
	{
		const b3Manifold* manifold = contact->manifolds + m;
		for ( int p = 0; p < manifold->pointCount; ++p )
		{
			const b3ManifoldPoint* point = manifold->points + p;
			if ( point->separation > NB_TOUCH_GAP )
			{
				continue;
			}

			if ( fan->count == NB_FAN_POINTS )
			{
				fan->count = nbConvexHull2( fan->points, fan->count );
			}

			if ( fan->count < NB_FAN_POINTS )
			{
				b3Vec3 offset = isA ? point->anchorA : point->anchorB;
				fan->points[fan->count++] = (b3Vec2){ b3Dot( offset, fan->u ), b3Dot( offset, fan->v ) };
			}
		}
	}
}

// The center of mass lies over the support points, within NB_BALANCE_RADIUS of their convex hull seen from above
static bool nbIsBalanced( const nbSupportFan* fan )
{
	b3Vec2 points[NB_FAN_POINTS];
	memcpy( points, fan->points, sizeof( b3Vec2 ) * (size_t)fan->count );
	int count = nbConvexHull2( points, fan->count );
	float radius = NB_BALANCE_RADIUS;
	if ( count == 0 )
	{
		return false;
	}

	if ( count < 3 )
	{
		// A point or a segment: the distance of the center to it
		b3Vec2 a = points[0];
		b3Vec2 ab = { points[count - 1].x - a.x, points[count - 1].y - a.y };
		float length2 = nbDot2( ab, ab );
		float t = length2 > 0.0f ? b3ClampFloat( -nbDot2( a, ab ) / length2, 0.0f, 1.0f ) : 0.0f;
		b3Vec2 closest = { a.x + t * ab.x, a.y + t * ab.y };
		return nbDot2( closest, closest ) <= radius * radius;
	}

	b3Vec2 origin = { 0.0f, 0.0f };
	for ( int i = 0; i < count; ++i )
	{
		b3Vec2 a = points[i];
		b3Vec2 b = points[( i + 1 ) % count];
		b3Vec2 ab = { b.x - a.x, b.y - a.y };
		if ( nbCross2( a, b, origin ) < -radius * sqrtf( nbDot2( ab, ab ) ) )
		{
			return false;
		}
	}
	return true;
}

// What carries a piece, from its contacts
typedef struct nbSupport
{
	// Lies on the ground, a structure or rubble that holds without the piece
	bool grounded;

	// For a piece of at least NB_CHECKED_VOLUME: its center of mass lies over the points where it touches what holds
	// without it, or bodies that move and do not rest on it. Always true for smaller pieces.
	bool balanced;

	// For a piece of at least NB_CHECKED_VOLUME: the center of mass lies over the points of what holds without it alone,
	// lying on it or wedged in between
	bool braced;

	// The chunks it lies on, see nbActor::carriers
	nbCarrier carriers[NB_MAX_CARRIERS];
	int carrierCount;
} nbSupport;

// What carries a piece. A piece of at least NB_CHECKED_VOLUME rests on rubble only if something else holds that rubble,
// see nbIsSupportedWithout. Otherwise the two would hold each other up in the air: rubble that froze on a floor while it
// was part of the structure, and wedged between the floor and its lintels once the floor came down. Such rubble goes to
// the list, and the piece must stay balanced without it: a piece wedged under the floor that rests on the floor itself
// carries nothing either, however it touches. Smaller pieces lie on whatever they touch.
static void nbFindSupport( nbWorld* world, int actorIndex, const b3ContactData* contacts, int contactCount, b3Vec3 up,
						   const nbSupportFan* baseFan, nbSupport* support, int* unsupported, int* unsupportedCount,
						   int unsupportedCapacity )
{
	const nbActor* actor = world->actors.data + actorIndex;
	bool large = actor->volume >= NB_CHECKED_VOLUME;
	b3Vec3 center = b3ToVec3( b3Body_GetWorldCenter( actor->bodyId ) );
	world->answerStamp += 1;
	uint32_t stamp = world->answerStamp;
	support->grounded = false;
	support->carrierCount = 0;
	int firstUnsupported = *unsupportedCount;
	nbSupportFan fan = *baseFan;
	nbSupportFan fixedFan = *baseFan;

	// Moving bodies that rest on a large piece carry nothing
	for ( int c = 0; large && c < contactCount; ++c )
	{
		bool isA = B3_ID_EQUALS( b3Shape_GetBody( contacts[c].shapeIdA ), actor->bodyId );
		int otherIndex = nbFindActorFromBody( world, b3Shape_GetBody( isA ? contacts[c].shapeIdB : contacts[c].shapeIdA ) );
		if ( otherIndex != NB_NULL_INDEX && nbCarries( contacts + c, isA == false, up ) )
		{
			world->actors.data[otherIndex].restStamp = stamp;
		}
	}

	for ( int c = 0; c < contactCount; ++c )
	{
		// A small piece is done once it lies on something and knows enough of what
		if ( large == false && support->grounded && support->carrierCount == NB_MAX_CARRIERS )
		{
			break;
		}

		bool isA = B3_ID_EQUALS( b3Shape_GetBody( contacts[c].shapeIdA ), actor->bodyId );
		bool carries = nbCarries( contacts + c, isA, up );
		if ( carries == false && large == false )
		{
			continue;
		}

		b3ShapeId otherShapeId = isA ? contacts[c].shapeIdB : contacts[c].shapeIdA;
		b3BodyId otherBodyId = b3Shape_GetBody( otherShapeId );
		int chunkIndex = nbFindChunkFromShape( world, otherShapeId );
		int otherIndex = chunkIndex != NB_NULL_INDEX ? world->chunks.data[chunkIndex].actorIndex : NB_NULL_INDEX;
		if ( b3Body_GetType( otherBodyId ) == b3_staticBody )
		{
			if ( large && otherIndex != NB_NULL_INDEX && world->actors.data[otherIndex].isRubble &&
				 nbIsCarrierSupported( world, otherIndex, actorIndex, stamp ) == false )
			{
				bool listed = false;
				for ( int k = firstUnsupported; k < *unsupportedCount && listed == false; ++k )
				{
					listed = unsupported[k] == otherIndex;
				}
				if ( listed == false && *unsupportedCount < unsupportedCapacity )
				{
					unsupported[( *unsupportedCount )++] = otherIndex;
				}

				// It is still what the piece lies on, the search decides what holds
				b3Vec3 point;
				if ( carries && nbTouchPoint( contacts + c, isA, center, NB_TOUCH_GAP, &point ) )
				{
					nbAddCarrier( world, support->carriers, &support->carrierCount, chunkIndex, point );
				}
				continue;
			}

			support->grounded = support->grounded || carries;
			if ( large )
			{
				nbAddSupportPoints( &fixedFan, contacts + c, isA );
			}
		}
		else if ( otherIndex == NB_NULL_INDEX )
		{
			continue;
		}
		else if ( world->actors.data[otherIndex].restStamp == stamp )
		{
			continue;
		}

		if ( large )
		{
			nbAddSupportPoints( &fan, contacts + c, isA );
		}

		b3Vec3 point;
		if ( carries && nbTouchPoint( contacts + c, isA, center, NB_TOUCH_GAP, &point ) )
		{
			nbAddCarrier( world, support->carriers, &support->carrierCount, chunkIndex, point );
		}
	}

	// A piece that lies on nothing but is wedged in, or about to touch something, remembers what it is closest to, apart
	// from moving bodies that rest on it
	bool wedged = support->carrierCount == 0;
	for ( int c = 0; wedged && c < contactCount && support->carrierCount < NB_MAX_CARRIERS; ++c )
	{
		bool isA = B3_ID_EQUALS( b3Shape_GetBody( contacts[c].shapeIdA ), actor->bodyId );
		b3ShapeId otherShapeId = isA ? contacts[c].shapeIdB : contacts[c].shapeIdA;
		int chunkIndex = nbFindChunkFromShape( world, otherShapeId );
		bool fixed = b3Body_GetType( b3Shape_GetBody( otherShapeId ) ) == b3_staticBody;
		b3Vec3 point;
		if ( ( fixed || ( chunkIndex != NB_NULL_INDEX &&
						  world->actors.data[world->chunks.data[chunkIndex].actorIndex].restStamp != stamp ) ) &&
			 nbTouchPoint( contacts + c, isA, center, FLT_MAX, &point ) )
		{
			nbAddCarrier( world, support->carriers, &support->carrierCount, chunkIndex, point );
		}
	}

	support->balanced = large == false || nbIsBalanced( &fan );
	support->braced = large && nbIsBalanced( &fixedFan );
}

// Whether a piece lies on something fixed that holds without a large piece resting on it: the ground, a structure or
// rubble that holds without it. A piece that only lies on moving pieces or on the large piece's own rubble passes
// nothing on to it.
static bool nbCarriesWithout( nbWorld* world, const nbCarrier* carriers, int count, int actorIndex )
{
	world->answerStamp += 1;
	for ( int k = 0; k < count; ++k )
	{
		const nbCarrier* record = carriers + k;
		if ( record->chunkIndex == NB_NULL_INDEX )
		{
			return true;
		}

		int carrierIndex = world->chunks.data[record->chunkIndex].actorIndex;
		const nbActor* carrier = world->actors.data + carrierIndex;
		if ( carrierIndex != actorIndex &&
			 ( carrier->isStatic ||
			   ( carrier->isRubble && nbIsSupportedWithout( world, carrierIndex, actorIndex, world->answerStamp ) ) ) )
		{
			return true;
		}
	}
	return false;
}

// Quiet debris freezes into rubble when something that does not move carries it: the ground, a structure, rubble, or
// quiet debris that is carried itself. The carrying contacts form a graph from the ground up, and a search from the
// grounded pieces freezes whole piles at once, from the bottom up. Nothing freezes while it lies on a moving piece, and
// rubble only carries what does not carry it in turn, see nbFindSupport. Each piece remembers what it froze on.
static void nbSettleDebris( nbWorld* world, b3Vec3 up, float timeStep )
{
	if ( world->def.enableRubble == false || world->debris.count == 0 )
	{
		return;
	}

	nbBeginOperation( world );
	world->settleStamp += 1;
	uint32_t stamp = world->settleStamp;
	int* quiet = nbArena_AllocArray( &world->arena, int, world->debris.count );
	int quietCount = 0;
	for ( int i = 0; i < world->debris.count; ++i )
	{
		int actorIndex = world->debris.data[i];
		nbActor* actor = world->actors.data + actorIndex;
		if ( actor->isRubble || nbIsQuiet( world, actor, timeStep ) == false )
		{
			continue;
		}

		actor->settleSlot = quietCount;
		actor->settleStamp = stamp;
		quiet[quietCount++] = actorIndex;
	}

	if ( quietCount == 0 )
	{
		return;
	}

	// Carrying contacts between quiet pieces, and the pieces something fixed carries
	int* queue = nbArena_AllocArray( &world->arena, int, quietCount );
	bool* held = nbArena_AllocArray( &world->arena, bool, quietCount );
	bool* stable = nbArena_AllocArray( &world->arena, bool, quietCount );
	bool* blocked = nbArena_AllocArray( &world->arena, bool, quietCount );
	b3BodyId* squeezers = nbArena_AllocArray( &world->arena, b3BodyId, quietCount );
	int* edgeCounts = nbArena_AllocArray( &world->arena, int, quietCount + 1 );
	for ( int slot = 0; slot < quietCount; ++slot )
	{
		stable[slot] = false;
		edgeCounts[slot + 1] = 0;
	}
	edgeCounts[0] = 0;
	nbIntArray* edges = &world->scratchList;
	edges->count = 0;

	// The chunks each quiet piece lies on, one per carrying body, remembered by the pieces that freeze
	nbCarrier* carriers = nbArena_AllocArray( &world->arena, nbCarrier, quietCount * NB_MAX_CARRIERS );
	int* carrierCounts = nbArena_AllocArray( &world->arena, int, quietCount );

	// Rubble found to rest on the very piece it would carry
	int unsupported[NB_MAX_RELEASES];
	int unsupportedCount = 0;
	nbSupportFan fan = nbMakeSupportFan( up );

	for ( int slot = 0; slot < quietCount; ++slot )
	{
		int actorIndex = quiet[slot];
		const nbActor* actor = world->actors.data + actorIndex;
		carrierCounts[slot] = 0;
		held[slot] = false;
		blocked[slot] = false;
		squeezers[slot] = b3_nullBodyId;
		int capacity = b3Body_GetContactCapacity( actor->bodyId );
		if ( capacity == 0 )
		{
			continue;
		}

		b3ContactData* contacts = nbArena_AllocArray( &world->arena, b3ContactData, capacity );
		int contactCount = b3Body_GetContactData( actor->bodyId, contacts, capacity );
		nbSupport support;
		int firstUnsupported = unsupportedCount;
		nbFindSupport( world, actorIndex, contacts, contactCount, up, &fan, &support, unsupported, &unsupportedCount,
					   NB_MAX_RELEASES );
		carrierCounts[slot] = support.carrierCount;
		for ( int k = 0; k < support.carrierCount; ++k )
		{
			carriers[slot * NB_MAX_CARRIERS + k] = support.carriers[k];
		}

		// A large piece that would not stay balanced does not freeze, and the rubble that nothing else holds comes back to
		// life, the piece tips or falls without it. Otherwise that rubble stays where it is.
		if ( support.balanced == false )
		{
			blocked[slot] = true;
		}
		else if ( actor->volume >= NB_CHECKED_VOLUME )
		{
			unsupportedCount = firstUnsupported;
		}

		squeezers[slot] = nbFindSqueezer( world, actor, contacts, contactCount );

		// An edge from each quiet piece that carries this one
		for ( int c = 0; c < contactCount; ++c )
		{
			bool isA = B3_ID_EQUALS( b3Shape_GetBody( contacts[c].shapeIdA ), actor->bodyId );
			b3BodyId otherBodyId = b3Shape_GetBody( isA ? contacts[c].shapeIdB : contacts[c].shapeIdA );
			if ( b3Body_GetType( otherBodyId ) != b3_dynamicBody || nbCarries( contacts + c, isA, up ) == false )
			{
				continue;
			}

			int other = nbFindActorFromBody( world, otherBodyId );
			if ( other != NB_NULL_INDEX && world->actors.data[other].settleStamp == stamp )
			{
				int below = world->actors.data[other].settleSlot;
				nbArray_Push( *edges, below );
				nbArray_Push( *edges, slot );
				edgeCounts[below + 1] += 1;
			}
		}

		// A large piece must be held by what holds without it, balanced over it or wedged in
		held[slot] = actor->volume >= NB_CHECKED_VOLUME ? support.braced : support.grounded;
	}

	// The pieces each quiet piece carries, grouped by carrier
	for ( int slot = 0; slot < quietCount; ++slot )
	{
		edgeCounts[slot + 1] += edgeCounts[slot];
	}
	int edgeCount = edges->count / 2;
	int* carried = nbArena_AllocArray( &world->arena, int, edgeCount + 1 );
	int* cursor = nbArena_AllocArray( &world->arena, int, quietCount );
	for ( int slot = 0; slot < quietCount; ++slot )
	{
		cursor[slot] = edgeCounts[slot];
	}
	for ( int e = 0; e < edgeCount; ++e )
	{
		carried[cursor[edges->data[2 * e]]++] = edges->data[2 * e + 1];
	}
	edges->count = 0;

	// A piece that a larger moving body squeezes freezes only together with that body, see NB_SQUEEZE_DEPTH. Otherwise the
	// search runs again without it.
	int queueCount = 0;
	for ( bool again = true; again; )
	{
		queueCount = 0;
		for ( int slot = 0; slot < quietCount; ++slot )
		{
			stable[slot] = held[slot] && blocked[slot] == false;
			if ( stable[slot] )
			{
				queue[queueCount++] = slot;
			}
		}

		for ( int head = 0; head < queueCount; ++head )
		{
			int slot = queue[head];
			for ( int e = edgeCounts[slot]; e < edgeCounts[slot + 1]; ++e )
			{
				int above = carried[e];
				if ( stable[above] == false && blocked[above] == false &&
					 ( world->actors.data[quiet[above]].volume < NB_CHECKED_VOLUME ||
					   nbCarriesWithout( world, carriers + slot * NB_MAX_CARRIERS, carrierCounts[slot], quiet[above] ) ) )
				{
					stable[above] = true;
					queue[queueCount++] = above;
				}
			}
		}

		again = false;
		for ( int k = 0; k < queueCount; ++k )
		{
			int slot = queue[k];
			if ( B3_IS_NULL( squeezers[slot] ) )
			{
				continue;
			}

			int other = nbFindActorFromBody( world, squeezers[slot] );
			const nbActor* squeezer = other != NB_NULL_INDEX ? world->actors.data + other : NULL;
			if ( squeezer == NULL || squeezer->settleStamp != stamp || stable[squeezer->settleSlot] == false )
			{
				blocked[slot] = true;
				again = true;
			}
		}
	}

	for ( int k = 0; k < queueCount; ++k )
	{
		int slot = queue[k];
		nbFreezeOnCarriers( world, quiet[slot], carriers + slot * NB_MAX_CARRIERS, carrierCounts[slot] );
	}

	for ( int i = 0; i < unsupportedCount; ++i )
	{
		if ( world->actors.data[unsupported[i]].isRubble )
		{
			nbThawActor( world, unsupported[i] );
		}
	}
}

typedef struct nbDebrisRank
{
	float speed;
	int actorIndex;
} nbDebrisRank;

// Slowest first, then by index so the order is total and the same everywhere
static int nbCompareDebris( const void* a, const void* b )
{
	const nbDebrisRank* x = a;
	const nbDebrisRank* y = b;
	if ( x->speed != y->speed )
	{
		return x->speed < y->speed ? -1 : 1;
	}
	return x->actorIndex - y->actorIndex;
}

// Freeze the slowest debris over budget into rubble. Nothing is removed. Once over budget a tenth more freezes, so the
// ranking only runs every so often. What lies on something fixed freezes first: the ground, a structure or rubble.
// Debris on moving pieces would hang in the air once they move on, it only freezes when the rest is not enough, and so
// does debris in flight. Each piece remembers the chunks it lies on, none for a piece in flight, and a large piece only
// freezes where it would in nbSettleDebris. A squeezed piece never freezes here, see NB_SQUEEZE_DEPTH.
static void nbEnforceDebrisBudget( nbWorld* world, b3Vec3 up )
{
	int budget = world->def.maxDebrisBodies;
	int count = world->debris.count - world->rubbleCount;
	if ( world->def.enableRubble == false || count <= budget )
	{
		return;
	}

	nbBeginOperation( world );
	nbDebrisRank* ranks = nbArena_AllocArray( &world->arena, nbDebrisRank, count );
	int rankCount = 0;
	for ( int i = 0; i < world->debris.count; ++i )
	{
		int actorIndex = world->debris.data[i];
		const nbActor* actor = world->actors.data + actorIndex;
		if ( actor->isRubble == false && actor->budgetAge >= NB_FREEZE_GRACE )
		{
			ranks[rankCount++] = (nbDebrisRank){ b3Length( b3Body_GetLinearVelocity( actor->bodyId ) ), actorIndex };
		}
	}

	qsort( ranks, (size_t)rankCount, sizeof( nbDebrisRank ), nbCompareDebris );
	bool hasGravity = b3LengthSquared( up ) > 0.0f;
	nbSupportFan fan = nbMakeSupportFan( up );
	int unsupported[NB_MAX_RELEASES];
	int unsupportedCount = 0;
	int excess = count - budget + budget / 10;
	int frozen = 0;
	for ( int pass = 0; pass < 2 && frozen < excess; ++pass )
	{
		for ( int i = 0; i < rankCount && frozen < excess; ++i )
		{
			int actorIndex = ranks[i].actorIndex;
			const nbActor* actor = world->actors.data + actorIndex;
			if ( actor->isRubble )
			{
				continue;
			}

			nbSupport support = { .grounded = true, .balanced = true, .braced = true };
			int capacity = hasGravity ? b3Body_GetContactCapacity( actor->bodyId ) : 0;
			if ( capacity > 0 )
			{
				b3ContactData* contacts = nbArena_AllocArray( &world->arena, b3ContactData, capacity );
				int contactCount = b3Body_GetContactData( actor->bodyId, contacts, capacity );
				if ( B3_IS_NULL( nbFindSqueezer( world, actor, contacts, contactCount ) ) == false )
				{
					continue;
				}

				int firstUnsupported = unsupportedCount;
				nbFindSupport( world, actorIndex, contacts, contactCount, up, &fan, &support, unsupported, &unsupportedCount,
							   NB_MAX_RELEASES );
				if ( pass > 0 || actor->volume < NB_CHECKED_VOLUME || support.balanced )
				{
					unsupportedCount = firstUnsupported;
				}
			}
			else if ( hasGravity )
			{
				// Nothing touches it, a large piece in flight would hang in the air. So would a slow one, which just lost
				// what it lay on or came back to life: what breaks off gets a new body, rubble has no contacts with what
				// is fixed, and Box3D finds the new ones in the next step.
				support.grounded = false;
				support.balanced = actor->volume < NB_CHECKED_VOLUME && ranks[i].speed >= NB_FLIGHT_SPEED;
			}

			if ( support.balanced == false || ( pass == 0 && support.grounded == false ) )
			{
				continue;
			}

			nbFreezeOnCarriers( world, actorIndex, support.carriers, support.carrierCount );
			frozen += 1;
		}
	}

	for ( int i = 0; i < unsupportedCount; ++i )
	{
		if ( world->actors.data[unsupported[i]].isRubble )
		{
			nbThawActor( world, unsupported[i] );
		}
	}
}

void nbWorld_Update( nbWorldId worldId, float timeStep )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return;
	}

	uint64_t ticks = b3GetTicks();

	// Read the Box3D events of the last step before anything changes the world
	nbCollectCollisionImpacts( world );

	// Debris below the kill depth has left the world. Only bodies that moved can have crossed the depth.
	b3Vec3 gravity = b3World_GetGravity( world->physicsWorld );
	b3Vec3 up = b3Neg( b3Normalize( gravity ) );
	bool hasGravity = b3LengthSquared( up ) > 0.5f;
	world->scratchList.count = 0;
	b3BodyEvents bodyEvents = b3World_GetBodyEvents( world->physicsWorld );
	for ( int i = 0; hasGravity && i < bodyEvents.moveCount; ++i )
	{
		const b3BodyMoveEvent* event = bodyEvents.moveEvents + i;
		int actorIndex = nbFindActorFromBody( world, event->bodyId );
		if ( actorIndex == NB_NULL_INDEX )
		{
			continue;
		}

		const nbActor* actor = world->actors.data + actorIndex;
		b3Pos center = b3TransformWorldPoint( event->transform, actor->localCenter );
		float height = (float)( up.x * center.x + up.y * center.y + up.z * center.z );
		if ( height < world->def.killDepth )
		{
			nbArray_Push( world->scratchList, actorIndex );
		}
	}

	for ( int i = 0; i < world->scratchList.count; ++i )
	{
		int actorIndex = world->scratchList.data[i];
		if ( world->actors.data[actorIndex].isFree == false )
		{
			nbDestroyActor( world, actorIndex );
		}
	}
	world->scratchList.count = 0;

	// Rubble hit hard comes back to life
	nbWakeHitRubble( world );

	// Collision damage, strongest first, limited per update
	int impactCount = world->collisionImpacts.count;
	if ( impactCount > world->def.maxCollisionImpactsPerUpdate )
	{
		impactCount = world->def.maxCollisionImpactsPerUpdate;
	}

	for ( int i = 0; i < impactCount; ++i )
	{
		nbCollisionImpact impact = world->collisionImpacts.data[i];
		const nbActor* actor = world->actors.data + impact.actorIndex;
		if ( actor->isFree || actor->generation != impact.actorGeneration )
		{
			continue;
		}

		nbImpactDef def = { 0 };
		def.point = impact.point;
		def.direction = b3Neg( impact.normal );
		def.radius = world->def.collisionRadiusScale * nbCbrt( impact.energy );
		def.damage = world->def.collisionDamageScale * impact.energy;
		def.ejectSpeed = 0.4f * impact.approachSpeed;
		nbImpactResult result = nbApplyImpact( world, &def, impact.actorIndex );

		// Box3D resolved the contact as if the chunk were rigid, so the body that hit it bounced.
		// When material broke away, give the body back most of its speed so it punches through.
		if ( result.detachedChunkCount > 0 && b3Body_IsValid( impact.otherBodyId ) &&
			 b3Body_GetType( impact.otherBodyId ) == b3_dynamicBody )
		{
			b3Vec3 velocity = b3Body_GetLinearVelocity( impact.otherBodyId );
			float normalSpeed = b3Dot( velocity, impact.normal );
			float restored = world->def.collisionPassThrough * impact.approachSpeed;
			velocity = b3MulAdd( velocity, -normalSpeed - restored, impact.normal );
			b3Body_SetLinearVelocity( impact.otherBodyId, velocity );
		}
	}

	// Structures that cannot carry themselves give way
	nbCheckSupports( world );

	// Rubble follows what moved away from under it, then quiet debris freezes. Debris is never removed: past the budget
	// the slowest freezes.
	for ( int i = 0; i < world->debris.count; ++i )
	{
		nbActor* actor = world->actors.data + world->debris.data[i];
		actor->age += timeStep;
		actor->budgetAge += timeStep;
	}
	if ( hasGravity )
	{
		nbReleaseHeldRubble( world, up );
		nbSettleDebris( world, up, timeStep );
	}
	nbEnforceDebrisBudget( world, hasGravity ? up : b3Vec3_zero );

	world->stats.updateTime = b3GetMilliseconds( ticks );
}

bool nbChunk_IsValid( nbChunkId chunkId )
{
	return nbGetChunkFromId( chunkId, NULL ) != NULL;
}

b3BodyId nbChunk_GetBody( nbChunkId chunkId )
{
	nbChunk* chunk = nbGetChunkFromId( chunkId, NULL );
	return chunk != NULL ? chunk->bodyId : b3_nullBodyId;
}

b3ShapeId nbChunk_GetShape( nbChunkId chunkId )
{
	nbChunk* chunk = nbGetChunkFromId( chunkId, NULL );
	return chunk != NULL ? chunk->shapeId : b3_nullShapeId;
}

nbDestructibleId nbChunk_GetDestructible( nbChunkId chunkId )
{
	nbWorld* world;
	nbChunk* chunk = nbGetChunkFromId( chunkId, &world );
	if ( chunk == NULL )
	{
		return nb_nullDestructibleId;
	}

	const nbDestructible* destructible = world->destructibles.data + chunk->destructibleIndex;
	return (nbDestructibleId){ chunk->destructibleIndex + 1, world->worldIndex, destructible->generation };
}

float nbChunk_GetVolume( nbChunkId chunkId )
{
	nbChunk* chunk = nbGetChunkFromId( chunkId, NULL );
	return chunk != NULL ? chunk->shape->volume : 0.0f;
}

b3Vec3 nbChunk_GetCentroid( nbChunkId chunkId )
{
	nbChunk* chunk = nbGetChunkFromId( chunkId, NULL );
	return chunk != NULL ? chunk->shape->centroid : b3Vec3_zero;
}

int nbChunk_GetDepth( nbChunkId chunkId )
{
	nbChunk* chunk = nbGetChunkFromId( chunkId, NULL );
	return chunk != NULL ? chunk->depth : 0;
}

nbMaterial nbChunk_GetMaterial( nbChunkId chunkId )
{
	nbWorld* world;
	nbChunk* chunk = nbGetChunkFromId( chunkId, &world );
	return chunk != NULL ? *nbGetChunkMaterial( world, chunk ) : (nbMaterial){ 0 };
}

int nbChunk_GetBondCount( nbChunkId chunkId )
{
	nbChunk* chunk = nbGetChunkFromId( chunkId, NULL );
	return chunk != NULL ? chunk->bondCount : 0;
}

bool nbChunk_IsAnchored( nbChunkId chunkId )
{
	nbChunk* chunk = nbGetChunkFromId( chunkId, NULL );
	return chunk != NULL && ( chunk->flags & nb_chunkAnchored ) != 0;
}

bool nbChunk_IsDynamic( nbChunkId chunkId )
{
	nbWorld* world;
	nbChunk* chunk = nbGetChunkFromId( chunkId, &world );
	if ( chunk == NULL || chunk->actorIndex == NB_NULL_INDEX )
	{
		return false;
	}
	return world->actors.data[chunk->actorIndex].isStatic == false;
}

nbGeometry nbChunk_GetGeometry( nbChunkId chunkId )
{
	nbChunk* chunk = nbGetChunkFromId( chunkId, NULL );
	if ( chunk == NULL )
	{
		return (nbGeometry){ 0 };
	}
	return nbShape_GetGeometry( chunk->shape );
}

// A bond lies on a face when the normals agree within about one degree and the interface centroid is on the face
// plane within the tolerance of bonding. Interfaces that cover all but a thousandth of a face hide it.
#define NB_FACE_NORMAL_TOLERANCE 0.9998f
#define NB_FACE_PLANE_TOLERANCE 2.0e-3f
#define NB_FACE_COVERAGE 0.999f

// Two convex chunks touch in one plane, so every bond lies on one face of each chunk. The bond normal points out of
// its first chunk.
int nbChunk_GetVisibleFaces( nbChunkId chunkId, bool* visible, int capacity )
{
	nbWorld* world = NULL;
	nbChunk* chunk = nbGetChunkFromId( chunkId, &world );
	if ( chunk == NULL || visible == NULL )
	{
		return 0;
	}

	const nbShape* shape = chunk->shape;
	int faceCount = b3MinInt( shape->faceCount, capacity );
	float covered[NB_POLY_MAX_FACES] = { 0 };

	for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
	{
		const nbBond* bond = world->bonds.data + ( key >> 1 );
		int side = key & 1;
		b3Vec3 normal = side == 0 ? bond->normal : b3Neg( bond->normal );
		float height = b3Dot( normal, bond->centroid );

		int bestFace = -1;
		float bestDistance = NB_FACE_PLANE_TOLERANCE;
		for ( int f = 0; f < shape->faceCount; ++f )
		{
			b3Plane plane = shape->faces[f].plane;
			float distance = b3AbsFloat( plane.offset - height );
			if ( b3Dot( plane.normal, normal ) > NB_FACE_NORMAL_TOLERANCE && distance <= bestDistance )
			{
				bestFace = f;
				bestDistance = distance;
			}
		}

		if ( bestFace >= 0 )
		{
			covered[bestFace] += bond->area;
		}
		key = bond->nextKey[side];
	}

	for ( int f = 0; f < faceCount; ++f )
	{
		const nbFace* face = shape->faces + f;
		const uint8_t* loop = shape->indices + face->firstIndex;
		b3Vec3 origin = shape->vertices[loop[0]];
		float twiceArea = 0.0f;
		for ( int k = 1; k + 1 < face->indexCount; ++k )
		{
			b3Vec3 e1 = b3Sub( shape->vertices[loop[k]], origin );
			b3Vec3 e2 = b3Sub( shape->vertices[loop[k + 1]], origin );
			twiceArea += b3Dot( face->plane.normal, b3Cross( e1, e2 ) );
		}
		visible[f] = covered[f] < NB_FACE_COVERAGE * 0.5f * twiceArea;
	}
	return faceCount;
}
