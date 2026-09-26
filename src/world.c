// SPDX-License-Identifier: MIT

#include "world.h"

#include "fracture.h"
#include "hull_builder.h"
#include "scheduler.h"

#include <float.h>
#include <stdlib.h>

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

// A body that is not debris brings the rubble it hits this fast back to life
#define NB_WAKE_SPEED 1.0f

// Rubble bodies that hits bring back to life per update at most
#define NB_MAX_RUBBLE_HITS 16

// Debris younger than this in seconds does not freeze to keep within the budget
#define NB_FREEZE_GRACE 0.25f

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
	return actor->restTime >= NB_REST_TIME || rocking;
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
	nbResetRest( actor );
	world->rubbleCount += 1;
}

// Bring rubble back to life. The rubble resting on it stays until it moves, then follows.
void nbThawActor( nbWorld* world, int actorIndex )
{
	nbActor* actor = world->actors.data + actorIndex;
	NB_ASSERT( actor->isRubble );
	actor->isRubble = false;
	world->rubbleCount -= 1;
	b3Body_SetType( actor->bodyId, b3_dynamicBody );
	b3Body_ApplyMassFromShapes( actor->bodyId );
	b3Body_SetAwake( actor->bodyId, true );
	nbResetRest( actor );
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
} nbThawContext;

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
	if ( actor->isRubble == false || actor->settleStamp == world->settleStamp )
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
		if ( bottom <= thawContext->height )
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
// searched with one query over all of them, the neighborhoods of the debris of one impact overlap a lot. With a
// direction, only the rubble above the height.
static void nbThawRubbleInBoxes( nbWorld* world, const b3AABB* boxes, int boxCount, b3Vec3 up, float height )
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
	nbThawContext context = { world, thawed, boxCount > 1 ? boxes : NULL, boxCount > 1 ? boxCount : 0, up, height };
	b3World_OverlapAABB( world->physicsWorld, total, b3DefaultQueryFilter(), nbThawCallback, &context );

	// Past the limit the rest stays rubble
	for ( int i = 0; i < thawed->count && i < NB_MAX_THAW; ++i )
	{
		nbThawActor( world, thawed->data[i] );
	}
	thawed->count = 0;
}

void nbThawRubble( nbWorld* world, b3AABB box )
{
	nbThawRubbleInBoxes( world, &box, 1, b3Vec3_zero, 0.0f );
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

// Queue a static actor for a load check in the next update
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

void nbCommitPhysics( nbWorld* world )
{
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

		if ( actor->isStatic == false && actor->massDirty )
		{
			b3Body_ApplyMassFromShapes( actor->bodyId );
			actor->localCenter = b3Body_GetLocalCenter( actor->bodyId );
			actor->radius = 0.0f;
			for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
			{
				const nbShape* shape = world->chunks.data[c].shape;
				actor->radius = b3MaxFloat( actor->radius, b3Distance( shape->centroid, actor->localCenter ) + shape->radius );
			}
		}
		actor->massDirty = false;

		nbUpdateDebris( world, actorIndex );
		if ( actor->isStatic )
		{
			nbMarkSupport( world, actorIndex );
		}

		// Rubble that rested on a part falling off the structure falls with it
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

	nbActor* actor = world->actors.data + actorIndex;
	actor->sourceLinearVelocity = linearVelocity;
	actor->sourceAngularVelocity = angularVelocity;
	actor->sourceCenter = center;
	actor->fromStructure = world->actors.data[sourceIndex].isStatic;
	nbTouchActor( world, actorIndex );

	for ( int i = 0; i < count; ++i )
	{
		int chunkIndex = chunks[i];
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

// The load check follows the structural solver of the reference engine (src/structure/structural_loads.cpp on the
// Referenz branch). Bonds carry the full strength of their material in compression, NB_SHEAR_STRENGTH of it sideways,
// and bend like a beam of NB_BENDING_STRENGTH times it.
#define NB_SHEAR_STRENGTH 0.5f
#define NB_BENDING_STRENGTH 1.0f

// Path cost of a bond per inverse square meter of its area, see nbPathCost. Small against the height costs of whole wall
// pieces, so the load still goes down story by story, large for the tiny faces of fragments.
#define NB_FACE_COST 0.001f

// A chunk rests on a real face when the faces it rests on cover at least this part of the square of its size, the
// volume to the power of 2/3
#define NB_BEARING_FACE 0.25f

// A bond fails above this utilization, a little over one so float noise cannot break a structure that just holds
#define NB_FAILURE_UTILIZATION 1.02f

// Chunks crushed in one structure per update at most. The next update carries on with what is left.
#define NB_MAX_CRUSHES 4

// A crushed chunk bursts into this many fragments that fly apart at this speed in meters per second. A few are enough
// to take the load off, more only cost.
#define NB_CRUSH_FRAGMENTS 6
#define NB_CRUSH_SPEED 2.0f

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

	// The chunk rests on the parent through this bond
	bool bearing;
} nbLoadPath;

// An overloaded bond, and the chunk it crushes or NB_NULL_INDEX
typedef struct nbOverload
{
	int bondIndex;
	int crushChunk;
	float utilization;
} nbOverload;

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

// Load check of one structure, after the structural solver of the reference engine. A shortest path search out of the
// anchored chunks decides who carries whom. Then the load flows from the farthest chunks inward: every chunk passes its
// weight, and the weight resting on it, to its neighbors closer to the anchors, shared by bond area and four times as
// much through bonds it rests on. The shares lean toward the center of mass of the load, like the pressure under a
// footing, so the load passes through it where the bonds reach around it. What reaches beyond, like an overhang, bends
// the bonds. Bonds a chunk rests on work as hinges: the load goes straight down through them and only the chunk's own
// weight bends them, so a roof does not twist a whole wall. A bond fails when the force through it over what its area
// carries, plus the bending over what its shape carries, exceeds one. Returns the overloaded bonds in arena memory.
static int nbCheckSupport( nbWorld* world, int actorIndex, float gravity, b3Vec3 up, nbOverload** overloadsOut )
{
	const nbActor* actor = world->actors.data + actorIndex;
	const nbDestructible* destructible = world->destructibles.data + actor->destructibleIndex;
	float scale = world->def.supportScale;
	b3Vec3 localUp = b3InvRotateVector( destructible->transform.q, up );
	b3Vec3 side1 = b3Perp( localUp );
	b3Vec3 side2 = b3Cross( localUp, side1 );

	int count = actor->chunkCount;
	int* chunks = nbArena_AllocArray( &world->arena, int, count );
	int* order = nbArena_AllocArray( &world->arena, int, count );
	int* rank = nbArena_AllocArray( &world->arena, int, count );
	int* previous = nbArena_AllocArray( &world->arena, int, count );
	float* distance = nbArena_AllocArray( &world->arena, float, count );
	float* mass = nbArena_AllocArray( &world->arena, float, count );
	float* load = nbArena_AllocArray( &world->arena, float, count );
	b3Vec3* moment = nbArena_AllocArray( &world->arena, b3Vec3, count );

	world->searchStamp += 1;
	uint32_t stamp = world->searchStamp;
	int keyCount = 0;
	int maxBonds = 0;
	int n = 0;
	for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
	{
		nbChunk* chunk = world->chunks.data + c;
		chunk->searchStamp = stamp;
		chunk->scratch = n;
		chunks[n] = c;
		mass[n] = chunk->shape->volume * destructible->materials[chunk->materialIndex].density;
		load[n] = mass[n];
		moment[n] = b3MulSV( mass[n], chunk->shape->centroid );
		distance[n] = ( chunk->flags & nb_chunkAnchored ) ? 0.0f : FLT_MAX;
		rank[n] = -1;
		previous[n] = -1;
		keyCount += chunk->bondCount;
		maxBonds = chunk->bondCount > maxBonds ? chunk->bondCount : maxBonds;
		n += 1;
	}

	// Shortest paths out of the anchored chunks, with a heap that skips stale entries. Every directed bond pushes once
	// at most.
	uint64_t* heap = nbArena_AllocArray( &world->arena, uint64_t, n + keyCount );
	int heapCount = 0;
	for ( int i = 0; i < n; ++i )
	{
		if ( distance[i] == 0.0f )
		{
			heap[heapCount] = (uint32_t)i;
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

		const nbChunk* chunk = world->chunks.data + chunks[i];
		for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
		{
			const nbBond* bond = world->bonds.data + ( key >> 1 );
			int side = key & 1;
			key = bond->nextKey[side];

			const nbChunk* other = world->chunks.data + bond->chunk[side ^ 1];
			int j = other->scratch;
			if ( other->searchStamp != stamp || rank[j] >= 0 )
			{
				continue;
			}

			b3Vec3 normal = side == 0 ? bond->normal : b3Neg( bond->normal );
			float cost = nbPathCost( chunk->shape->centroid, other->shape->centroid, normal, bond->area, nbBondIntegrity( world, bond ),
									 localUp );
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

	// The flow from the farthest chunks inward
	nbLoadPath* paths = nbArena_AllocArray( &world->arena, nbLoadPath, maxBonds + 1 );
	nbOverload* overloads = nbArena_AllocArray( &world->arena, nbOverload, keyCount / 2 + 1 );
	int overloadCount = 0;
	for ( int o = orderCount - 1; o >= 0; --o )
	{
		int i = order[o];
		const nbChunk* chunk = world->chunks.data + chunks[i];
		if ( chunk->flags & nb_chunkAnchored )
		{
			continue;
		}

		// The bonds to chunks closer to the anchors. Ties only count for the chunk the search came from.
		int pathCount = 0;
		float total = 0.0f;
		b3Vec3 center = b3Vec3_zero;
		bool vertical = false;
		float bearingArea = 0.0f;
		for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
		{
			int bondIndex = key >> 1;
			const nbBond* bond = world->bonds.data + bondIndex;
			int side = key & 1;
			key = bond->nextKey[side];

			const nbChunk* other = world->chunks.data + bond->chunk[side ^ 1];
			int j = other->scratch;
			if ( other->searchStamp != stamp || rank[j] < 0 || rank[j] > rank[i] || ( distance[j] >= distance[i] && j != previous[i] ) )
			{
				continue;
			}

			// The normal points out of this chunk, down into a chunk it rests on
			b3Vec3 normal = side == 0 ? bond->normal : b3Neg( bond->normal );
			bool bearing = b3Dot( normal, localUp ) < -0.1f;
			float integrity = nbBondIntegrity( world, bond );
			float weight = bond->area * ( bearing ? 4.0f : 1.0f ) * integrity;
			paths[pathCount++] = (nbLoadPath){ bondIndex, j, weight, 0.0f, integrity, bond->centroid, bearing };
			vertical = vertical || bearing;
			bearingArea += bearing ? bond->area : 0.0f;
		}

		// A chunk that rests on a real face passes its load down, not sideways into its neighbors, like the box cells of
		// the reference. Otherwise the piers beside a window would unload into the sill below it. A chunk that only
		// touches what is below with a small tip shares through its side faces as well, like the irregular cells there.
		if ( bearingArea >= NB_BEARING_FACE * nbCbrt( chunk->shape->volume * chunk->shape->volume ) )
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

		// The load the bonds do not reach around bends them, shared by what each bond carries in bending about the axis
		float bendingLoad = vertical ? mass[i] : load[i];
		b3Vec3 lever = b3Sub( vertical ? chunk->shape->centroid : massCenter, resultant );
		lever = b3MulSub( lever, b3Dot( lever, localUp ), localUp );
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

		for ( int p = 0; p < pathCount; ++p )
		{
			const nbLoadPath* path = paths + p;
			if ( path->weight <= 0.0f )
			{
				continue;
			}

			const nbBond* bond = world->bonds.data + path->bondIndex;
			float weight = load[i] * path->weight / adjusted;
			float strength = scale * nbGetBondStrength( world, bond );
			float upright = b3AbsFloat( b3Dot( bond->normal, localUp ) );
			float carried = strength * ( NB_SHEAR_STRENGTH + ( 1.0f - NB_SHEAR_STRENGTH ) * upright ) * bond->area * path->integrity;
			float forceRatio = gravity * weight / b3MaxFloat( carried, FLT_MIN );
			float utilization = forceRatio + bendingRatio;

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
				// A sound bond that gives way under the weight resting on it crushes the smaller of its two chunks
				int crushChunk = NB_NULL_INDEX;
				if ( path->bearing && forceRatio >= bendingRatio && path->integrity >= NB_CRUSH_INTEGRITY )
				{
					const nbChunk* parent = world->chunks.data + chunks[j];
					crushChunk = parent->shape->volume <= chunk->shape->volume ? chunks[j] : chunks[i];
				}
				overloads[overloadCount++] = (nbOverload){ path->bondIndex, crushChunk, utilization };
			}
		}
	}

	*overloadsOut = overloads;
	return overloadCount;
}

// Break a chunk out of whatever holds it and burst it into fragments that scatter, the way an overloaded column
// crumbles. Anchored chunks come off their anchors too.
static void nbCrushChunk( nbWorld* world, int chunkIndex )
{
	nbBeginOperation( world );
	nbChunk* chunk = world->chunks.data + chunkIndex;
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
	world->touchedActors.count = 0;

	chunk = world->chunks.data + chunkIndex;
	b3WorldTransform transform = nbActor_GetTransform( world, world->actors.data + chunk->actorIndex );
	nbImpactDef def = { 0 };
	def.point = b3TransformWorldPoint( transform, chunk->shape->centroid );
	def.radius = chunk->shape->radius;
	def.damage = 1.0e30f;
	def.ejectSpeed = NB_CRUSH_SPEED;
	def.fragmentCount = NB_CRUSH_FRAGMENTS;
	nbApplyImpact( world, &def, chunk->actorIndex );
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
	if ( world->def.supportScale <= 0.0f || gravity <= 0.0f )
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
		if ( actor->isFree || actor->isStatic == false || actor->supportDirty == false || actor->chunkCount == 0 )
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

		nbBeginOperation( world );
		nbOverload* overloads = NULL;
		int overloadCount = nbCheckSupport( world, actorIndex, gravity, up, &overloads );
		if ( overloadCount == 0 )
		{
			continue;
		}
		world->stats.overloadedBondCount += overloadCount;

		// The most overloaded bonds crush first
		int crushes[NB_MAX_CRUSHES];
		uint16_t crushGenerations[NB_MAX_CRUSHES];
		int crushCount = 0;
		for ( ; crushCount < NB_MAX_CRUSHES; ++crushCount )
		{
			int best = NB_NULL_INDEX;
			for ( int k = 0; k < overloadCount; ++k )
			{
				int chunkIndex = overloads[k].crushChunk;
				bool taken = chunkIndex == NB_NULL_INDEX;
				for ( int c = 0; c < crushCount && taken == false; ++c )
				{
					taken = crushes[c] == chunkIndex;
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
			if ( world->bonds.data[overloads[k].bondIndex].chunk[0] != NB_NULL_INDEX )
			{
				nbDestroyBond( world, overloads[k].bondIndex );
			}
		}

		nbImpactResult result = { 0 };
		nbSplitActors( world, &result );
		nbCommitPhysics( world );
		world->touchedActors.count = 0;

		for ( int c = 0; c < crushCount; ++c )
		{
			const nbChunk* chunk = world->chunks.data + crushes[c];
			if ( chunk->shape != NULL && chunk->generation == crushGenerations[c] )
			{
				nbCrushChunk( world, crushes[c] );
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

nbWorldDef nbDefaultWorldDef( void )
{
	nbWorldDef def = { 0 };
	def.physicsWorld = b3_nullWorldId;
	def.maxDebrisBodies = 1500;
	def.enableRubble = true;
	def.debrisSleepThreshold = 0.12f;
	def.killDepth = -100.0f;
	def.supportScale = 1.0f;
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

			const nbChunk* chunk = world->chunks.data + chunkIndex;
			const nbDestructible* destructible = world->destructibles.data + chunk->destructibleIndex;
			if ( destructible->enableCollisionDamage == false )
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
			// The rubble that rested on it lies higher than its center of mass did
			b3Pos center = b3TransformWorldPoint( actor->holdPose, actor->localCenter );
			nbThawRubbleInBoxes( world, &actor->holdBounds, 1, up, b3Dot( up, b3ToVec3( center ) ) );
		}
	}
}

// Quiet debris freezes into rubble when something that does not move carries it: the ground, a structure, rubble, or
// quiet debris that is carried itself. The carrying contacts form a graph from the ground up, and a search from the
// grounded pieces freezes whole piles at once, from the bottom up. Nothing freezes while it lies on a moving piece.
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
	bool* stable = nbArena_AllocArray( &world->arena, bool, quietCount );
	int* edgeCounts = nbArena_AllocArray( &world->arena, int, quietCount + 1 );
	for ( int slot = 0; slot < quietCount; ++slot )
	{
		stable[slot] = false;
		edgeCounts[slot + 1] = 0;
	}
	edgeCounts[0] = 0;
	nbIntArray* edges = &world->scratchList;
	edges->count = 0;
	int queueCount = 0;

	for ( int slot = 0; slot < quietCount; ++slot )
	{
		const nbActor* actor = world->actors.data + quiet[slot];
		int capacity = b3Body_GetContactCapacity( actor->bodyId );
		if ( capacity == 0 )
		{
			continue;
		}

		b3ContactData* contacts = nbArena_AllocArray( &world->arena, b3ContactData, capacity );
		int contactCount = b3Body_GetContactData( actor->bodyId, contacts, capacity );
		bool grounded = false;
		for ( int c = 0; c < contactCount && grounded == false; ++c )
		{
			bool isA = B3_ID_EQUALS( b3Shape_GetBody( contacts[c].shapeIdA ), actor->bodyId );
			b3BodyId otherBodyId = b3Shape_GetBody( isA ? contacts[c].shapeIdB : contacts[c].shapeIdA );
			for ( int m = 0; m < contacts[c].manifoldCount; ++m )
			{
				const b3Manifold* manifold = contacts[c].manifolds + m;
				b3Vec3 down = isA ? manifold->normal : b3Neg( manifold->normal );
				bool touching = false;
				for ( int p = 0; p < manifold->pointCount; ++p )
				{
					touching = touching || manifold->points[p].separation <= NB_TOUCH_GAP;
				}

				if ( touching == false || b3Dot( down, up ) > -NB_SUPPORT_NORMAL )
				{
					continue;
				}

				b3BodyType type = b3Body_GetType( otherBodyId );
				if ( type == b3_staticBody )
				{
					grounded = true;
					break;
				}

				int other = type == b3_dynamicBody ? nbFindActorFromBody( world, otherBodyId ) : NB_NULL_INDEX;
				if ( other != NB_NULL_INDEX && world->actors.data[other].settleStamp == stamp )
				{
					// An edge from the carrying piece to this one
					int below = world->actors.data[other].settleSlot;
					nbArray_Push( *edges, below );
					nbArray_Push( *edges, slot );
					edgeCounts[below + 1] += 1;
				}
				break;
			}
		}

		if ( grounded )
		{
			stable[slot] = true;
			queue[queueCount++] = slot;
		}
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

	for ( int head = 0; head < queueCount; ++head )
	{
		int slot = queue[head];
		for ( int e = edgeCounts[slot]; e < edgeCounts[slot + 1]; ++e )
		{
			int above = carried[e];
			if ( stable[above] == false )
			{
				stable[above] = true;
				queue[queueCount++] = above;
			}
		}
	}

	for ( int k = 0; k < queueCount; ++k )
	{
		nbFreezeActor( world, quiet[queue[k]] );
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
// ranking only runs every so often.
static void nbEnforceDebrisBudget( nbWorld* world )
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
		if ( actor->isRubble == false && actor->age >= NB_FREEZE_GRACE )
		{
			ranks[rankCount++] = (nbDebrisRank){ b3Length( b3Body_GetLinearVelocity( actor->bodyId ) ), actorIndex };
		}
	}

	qsort( ranks, (size_t)rankCount, sizeof( nbDebrisRank ), nbCompareDebris );
	int excess = count - budget + budget / 10;
	for ( int i = 0; i < excess && i < rankCount; ++i )
	{
		nbFreezeActor( world, ranks[i].actorIndex );
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
		world->actors.data[world->debris.data[i]].age += timeStep;
	}
	if ( hasGravity )
	{
		nbReleaseHeldRubble( world, up );
		nbSettleDebris( world, up, timeStep );
	}
	nbEnforceDebrisBudget( world );

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
