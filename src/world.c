// SPDX-License-Identifier: MIT

#include "world.h"

#include "fracture.h"
#include "hull_builder.h"
#include "scheduler.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

static nbWorld nb_worlds[NB_MAX_WORLDS];

static void nbWaitForSteps( nbWorld* world );

// The thread that handed steps to the step thread waits for them before it touches the world, see nbWorld_Step. Every
// other thread goes on, the step thread and the workers among them. The wait is out of line, so a lookup of a world
// without steps stays as cheap as before.
static NB_NOINLINE nbWorld* nbPassSteps( nbWorld* world )
{
	if ( nbAtomicLoadInt( &world->stepOwner ) == nbGetThreadToken() )
	{
		nbWaitForSteps( world );
	}
	return world;
}

static inline nbWorld* nbCheckSteps( nbWorld* world )
{
	return nbAtomicLoadInt( &world->stepOwner ) == 0 ? world : nbPassSteps( world );
}

nbWorld* nbGetWorld( int index )
{
	NB_ASSERT( 0 <= index && index < NB_MAX_WORLDS );
	return nbCheckSteps( nb_worlds + index );
}

// Without waiting for the steps
static nbWorld* nbFindWorld( nbWorldId id )
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

nbWorld* nbGetWorldFromId( nbWorldId id )
{
	nbWorld* world = nbFindWorld( id );
	return world != NULL ? nbCheckSteps( world ) : NULL;
}

nbWorld* nbPeekWorld( int index )
{
	NB_ASSERT( 0 <= index && index < NB_MAX_WORLDS );
	return nb_worlds + index;
}

nbChunkId nbMakeChunkId( const nbWorld* world, int chunkIndex )
{
	nbChunkId id = { chunkIndex + 1, world->worldIndex, world->chunks.data[chunkIndex].generation };
	return id;
}

// Without waiting for the steps
static inline nbChunk* nbFindChunk( nbWorld* world, nbChunkId id, nbWorld** worldOut )
{
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

static NB_NOINLINE nbChunk* nbFindChunkAfterSteps( nbChunkId id, nbWorld** worldOut )
{
	return nbFindChunk( nbPassSteps( nb_worlds + id.world0 ), id, worldOut );
}

nbChunk* nbGetChunkFromId( nbChunkId id, nbWorld** worldOut )
{
	if ( id.world0 >= NB_MAX_WORLDS || id.index1 < 1 )
	{
		return NULL;
	}

	// The wait for the steps out of line, see nbCheckSteps
	nbWorld* world = nb_worlds + id.world0;
	if ( nbAtomicLoadInt( &world->stepOwner ) != 0 )
	{
		return nbFindChunkAfterSteps( id, worldOut );
	}
	return nbFindChunk( world, id, worldOut );
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

void nbMapShape( nbWorld* world, b3ShapeId shapeId, int chunkIndex )
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

void nbMapBody( nbWorld* world, b3BodyId bodyId, int actorIndex )
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

b3BodyDef nbMakeActorBodyDef( const nbWorld* world, b3WorldTransform transform )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	bodyDef.position = transform.p;
	bodyDef.rotation = transform.q;
	bodyDef.sleepThreshold = world->def.debrisSleepThreshold;
	return bodyDef;
}

void nbCreateActorBody( nbWorld* world, int actorIndex, b3WorldTransform transform )
{
	b3BodyDef bodyDef = nbMakeActorBodyDef( world, transform );
	b3BodyId bodyId = b3CreateBody( world->physicsWorld, &bodyDef );
	world->actors.data[actorIndex].bodyId = bodyId;
	nbMapBody( world, bodyId, actorIndex );
}

int nbAllocChunk( nbWorld* world )
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
	nbShape_BuildHull( shape, &world->stats.hullFallbackCount );
	return nbCreateChunkWithHull( world, destructibleIndex, actorIndex, shape, depth, interiorMaterial, materialIndex );
}

int nbCreateChunkWithHull( nbWorld* world, int destructibleIndex, int actorIndex, nbShape* shape, int depth,
						   uint8_t interiorMaterial, int materialIndex )
{
	if ( shape->hull == NULL )
	{
		// Degenerate sliver. It cannot be simulated, so it is dropped.
		nbReleaseShape( world->destructibles.data + destructibleIndex, shape );
		return NB_NULL_INDEX;
	}

	int chunkIndex = nbAllocChunk( world );
	nbChunk* chunk = world->chunks.data + chunkIndex;
	nbDestructible* destructible = world->destructibles.data + destructibleIndex;

	chunk->shape = shape;
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

// Keep the second moments of a bond, see nbBondMoments
void nbKeepBondMoments( nbWorld* world, int bondIndex, b3Vec3 moments, b3Vec3 crossMoments )
{
	if ( world->bondMoments.count <= bondIndex )
	{
		nbArray_Reserve( world->bondMoments, bondIndex + 1 );
		memset( world->bondMoments.data + world->bondMoments.count, 0,
				sizeof( nbBondMoments ) * (size_t)( bondIndex + 1 - world->bondMoments.count ) );
		world->bondMoments.count = bondIndex + 1;
	}
	world->bondMoments.data[bondIndex] = (nbBondMoments){ moments, crossMoments };
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
	bond->health = health;
	bond->stamp = 0;
	bond->cohesive = false;
	bond->sibling = false;
	if ( world->destructibles.data[world->chunks.data[chunkA].destructibleIndex].bondMoments )
	{
		nbKeepBondMoments( world, index, geometry->moments, geometry->crossMoments );
	}

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
	world->destructibles.data[world->chunks.data[bond->chunk[0]].destructibleIndex].damaged = true;

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
	actor->freeTime = world->time;
	actor->budgetTime = world->time;
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
	else
	{
		destructible->tailActor = index;
	}
	destructible->headActor = index;
	destructible->actorCount += 1;
	destructible->buildingPartCount += isStatic ? 1 : 0;

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

// Keep the bit of the debris entry of an actor in step with whether it is rubble, see nbWorld::movingDebris
static void nbMarkMoving( nbWorld* world, const nbActor* actor )
{
	int index = actor->debrisIndex;
	if ( index != NB_NULL_INDEX )
	{
		uint64_t bit = (uint64_t)1 << ( index & 63 );
		uint64_t* word = world->movingDebris.data + ( index >> 6 );
		*word = actor->isRubble ? *word & ~bit : *word | bit;
	}
}

// Pieces of at least this volume in cubic meters freeze only on rubble that something else carries, see
// nbIsSupportedWithout. Smaller ones freeze on whatever they lie on.
#define NB_CHECKED_VOLUME 0.1f

// Keep the bit of the debris entry of an actor in step with its volume, see nbWorld::largeDebris
static void nbMarkLarge( nbWorld* world, const nbActor* actor )
{
	int index = actor->debrisIndex;
	if ( index != NB_NULL_INDEX )
	{
		uint64_t bit = (uint64_t)1 << ( index & 63 );
		uint64_t* word = world->largeDebris.data + ( index >> 6 );
		*word = actor->volume >= NB_CHECKED_VOLUME ? *word | bit : *word & ~bit;
	}
}

// The first entry of debris from the index on that moves, or the debris count. Rubble that comes back to life ahead of
// a loop over the moving debris is visited, as by a loop over all debris.
static int nbNextMoving( const nbWorld* world, int index )
{
	int wordIndex = index >> 6;
	if ( wordIndex >= world->movingDebris.count )
	{
		return world->debris.count;
	}

	uint64_t word = world->movingDebris.data[wordIndex] & ( ~(uint64_t)0 << ( index & 63 ) );
	while ( word == 0 )
	{
		wordIndex += 1;
		if ( wordIndex == world->movingDebris.count )
		{
			return world->debris.count;
		}
		word = world->movingDebris.data[wordIndex];
	}
	return 64 * wordIndex + nbCTZ64( word );
}

// The moving debris a number of places after an index, the debris count when fewer follow
static int nbMovingAhead( const nbWorld* world, int index, int places )
{
	for ( int k = 0; k < places && index < world->debris.count; ++k )
	{
		index = nbNextMoving( world, index + 1 );
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
	world->movingDebris.data[last >> 6] &= ~( (uint64_t)1 << ( last & 63 ) );
	world->largeDebris.data[last >> 6] &= ~( (uint64_t)1 << ( last & 63 ) );
	world->debris.count -= 1;
	world->movingDebris.count = ( world->debris.count + 63 ) >> 6;
	world->largeDebris.count = world->movingDebris.count;
	actor->debrisIndex = NB_NULL_INDEX;
	nbMarkMoving( world, world->actors.data + movedActor );
	nbMarkLarge( world, world->actors.data + movedActor );
}

// Destroy the Box3D body of an actor, with all shapes still on it
static void nbDestroyActorBody( nbWorld* world, int actorIndex )
{
	nbActor* actor = world->actors.data + actorIndex;
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

	nbDestroyActorBody( world, actorIndex );

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
	else
	{
		destructible->tailActor = actor->prevActor;
	}
	destructible->actorCount -= 1;
	destructible->buildingPartCount -= ( actor->isStatic || actor->isBuildingPart ) ? 1 : 0;

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

// A chunk comes to or leaves a part of a building that stands, see nbDestructible::standing
static void nbCountStanding( nbWorld* world, const nbActor* actor, const nbChunk* chunk, double sign )
{
	if ( actor->isStatic == false && actor->isBuildingPart == false )
	{
		return;
	}

	nbDestructible* destructible = world->destructibles.data + actor->destructibleIndex;
	int s = nbFindStorey( destructible, chunk->shape->centroid.y );
	if ( s >= 0 )
	{
		destructible->standing[s] += sign * chunk->shape->volume;
	}
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
	nbMarkLarge( world, actor );
	nbCountStanding( world, actor, chunk, 1.0 );
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
	nbMarkLarge( world, actor );
	nbCountStanding( world, actor, chunk, -1.0 );
}

void nbSumStanding( nbWorld* world, int destructibleIndex )
{
	nbDestructible* destructible = world->destructibles.data + destructibleIndex;
	for ( int s = 0; s < NB_MAX_STOREYS; ++s )
	{
		destructible->standing[s] = 0.0;
	}

	for ( int a = destructible->headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
	{
		const nbActor* actor = world->actors.data + a;
		for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
		{
			nbCountStanding( world, actor, world->chunks.data + c, 1.0 );
		}
	}
}

// A part that came down stays a building, and its chunks count for the storeys from now on, see nbCheckStoreys
static void nbMakeBuildingPart( nbWorld* world, nbActor* part )
{
	if ( part->isStatic || part->isBuildingPart )
	{
		return;
	}

	world->destructibles.data[part->destructibleIndex].buildingPartCount += 1;
	part->isBuildingPart = true;
	for ( int c = part->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
	{
		nbCountStanding( world, part, world->chunks.data + c, 1.0 );
	}
}

// The static body for a static chunk of the destructible: the one all its static chunks share, or a new one of its own,
// see NB_SHARED_STATIC_BODY
b3BodyId nbGetStaticBody( nbWorld* world, nbDestructible* destructible )
{
#if defined( NB_SHARED_STATIC_BODY )
	if ( B3_IS_NON_NULL( destructible->staticBody ) )
	{
		return destructible->staticBody;
	}
#endif

	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_staticBody;
	bodyDef.position = destructible->transform.p;
	bodyDef.rotation = destructible->transform.q;
	b3BodyId bodyId = b3CreateBody( world->physicsWorld, &bodyDef );

#if defined( NB_SHARED_STATIC_BODY )
	destructible->staticBody = bodyId;
#endif
	return bodyId;
}

// Destroy the shape of a chunk. A static body of its own goes with it, the shared one stays with the destructible.
static void nbDestroyChunkShape( b3ShapeId shapeId, b3BodyId bodyId, bool staticBody )
{
#if defined( NB_SHARED_STATIC_BODY )
	NB_UNUSED( bodyId );
	NB_UNUSED( staticBody );
#else
	if ( staticBody )
	{
		if ( b3Body_IsValid( bodyId ) )
		{
			b3DestroyBody( bodyId );
		}
		return;
	}
#endif

	if ( b3Shape_IsValid( shapeId ) )
	{
		b3DestroyShape( shapeId, false );
	}
}

// Remove the physics of a chunk. With destroyBodies false the caller destroys the body of the actor.
static void nbRemoveChunkPhysics( nbWorld* world, nbChunk* chunk, bool destroyBodies )
{
	if ( B3_IS_NON_NULL( chunk->shapeId ) )
	{
		nbUnmapShape( world, chunk->shapeId );
		bool staticBody = ( chunk->flags & nb_chunkStaticBody ) != 0;
		if ( staticBody || destroyBodies )
		{
			nbDestroyChunkShape( chunk->shapeId, chunk->bodyId, staticBody );
		}
	}

	chunk->shapeId = b3_nullShapeId;
	chunk->bodyId = b3_nullBodyId;
	chunk->flags &= ~nb_chunkStaticBody;
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
	destructible->damaged = true;

	// Chunks created and destroyed in the same event window are reported as both
	nbPushEvent( world->destroyedEvents + world->eventBuffer, nbMakeChunkId( world, chunkIndex ) );

	nbReleaseShape( destructible, chunk->shape );
	chunk->shape = NULL;
	nbFreeChunk( world, chunkIndex );
}

void nbReleaseShape( nbDestructible* destructible, nbShape* shape )
{
	if ( nbIsInShapeBlock( destructible->shapeBlock, destructible->shapeBlockSize, shape ) == false )
	{
		nbShape_Destroy( shape );
		return;
	}

	destructible->shapeBlockCount -= 1;
	if ( destructible->shapeBlockCount == 0 )
	{
		nbFree( destructible->shapeBlock, destructible->shapeBlockSize );
		destructible->shapeBlock = NULL;
		destructible->shapeBlockSize = 0;
	}
}

void nbDestroyChunk( nbWorld* world, int chunkIndex )
{
	nbRemoveChunkPhysics( world, world->chunks.data + chunkIndex, true );
	nbReleaseChunk( world, chunkIndex );
}

void nbDestroyActor( nbWorld* world, int actorIndex )
{
	// Destroying a dynamic body removes all of its shapes at once, which is much cheaper than one by one. The body goes
	// before the chunks, its shapes use the hulls in the shapes of the chunks.
	nbDestroyActorBody( world, actorIndex );
	while ( world->actors.data[actorIndex].headChunk != NB_NULL_INDEX )
	{
		int chunkIndex = world->actors.data[actorIndex].headChunk;
		nbRemoveChunkPhysics( world, world->chunks.data + chunkIndex, false );
		nbReleaseChunk( world, chunkIndex );
	}

	nbFreeActor( world, actorIndex );
}

void nbDestroyDestructibleParts( nbWorld* world, int destructibleIndex )
{
	// The bonds go with the chunks, all in one pass: without unlinking them from chunks that go anyway, reporting the faces
	// they covered or leaving seeds for a split. Every chunk frees the bonds it has left, in the order of its list, so
	// they come free in the same order as when the chunks go one by one.
	nbDestructible* destructible = world->destructibles.data + destructibleIndex;
	for ( int a = destructible->headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
	{
		for ( int c = world->actors.data[a].headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
		{
			nbChunk* chunk = world->chunks.data + c;
			for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
			{
				int bondIndex = key >> 1;
				nbBond* bond = world->bonds.data + bondIndex;
				key = bond->nextKey[key & 1];
				if ( bond->chunk[0] != NB_NULL_INDEX )
				{
					bond->chunk[0] = NB_NULL_INDEX;
					bond->chunk[1] = NB_NULL_INDEX;
					nbArray_Push( world->freeBonds, bondIndex );
					world->bondCount -= 1;
				}
			}
			chunk->headBondKey = NB_NULL_INDEX;
			chunk->bondCount = 0;
		}
	}

	while ( world->destructibles.data[destructibleIndex].headActor != NB_NULL_INDEX )
	{
		nbDestroyActor( world, world->destructibles.data[destructibleIndex].headActor );
	}
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
// branch). A piece is quiet once it stayed within NB_REST_SPEED times nbWorldDef::debrisRestTime of one pose for that
// time, counting rotation as the way its farthest corner moves.
#define NB_REST_SPEED 0.1f

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

// Rubble bodies that pieces moving away release per update at most, and so do large pieces that do not freeze, see
// nbSettleDebris. The others follow in the next updates.
#define NB_MAX_RELEASES 32

// A limit per update for the step of the current update. The limits hold for steps of 1/60 s and grow and shrink with the
// step, so the world takes as much per second at any step rate, see nbWorldDef::stepRate. At least one, and steps longer
// than a second count as one second.
static int nbLimitForStep( const nbWorld* world, int limit )
{
	return b3MaxInt( 1, (int)( (float)limit * world->stepScale + 0.5f ) );
}

// Rubble bodies the search for what carries rubble visits at most, see nbIsSupportedWithout
#define NB_SUPPORT_SEARCH 64

// A chunk still carries rubble while it is within this distance of the point where the rubble lay on it
#define NB_CARRIER_SLACK 0.05f

// A support point this close to the center of mass, seen from above, balances a piece on its own, see nbIsBalanced
#define NB_BALANCE_RADIUS 0.05f

// A body that is not debris brings the rubble it hits this fast back to life. An impact brings back the rubble it would
// push at least this fast, see nbImpactMoves.
#define NB_WAKE_SPEED 1.0f

// Rubble bodies that hits bring back to life per update at most
#define NB_MAX_RUBBLE_HITS 16

// Debris that lies on moving pieces freezes to keep within the budget only when it is slower than this, see
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

// A piece is quiet when it is slow and stayed in place, or rocks in place. Box3D islands that fell asleep are quiet, until
// a look found that they cannot freeze yet, see nbActor::checkedAsleep. A part of a building is quiet only asleep or
// rocking in place: it sinks onto the loose cells of a storey that gave way and tips over them at a few centimeters a
// second, which would pass for rest, and frozen there it would stand on them until the last of them is shot away. Box3D
// lets its island fall asleep only once the cells under it rest too.
static bool nbIsQuiet( const nbWorld* world, nbActor* actor, float timeStep )
{
	if ( b3Body_IsAwake( actor->bodyId ) == false )
	{
		return actor->checkedAsleep == false;
	}
	actor->checkedAsleep = false;

	b3WorldTransform transform = b3Body_GetTransform( actor->bodyId );
	float slow = 4.0f * world->def.debrisSleepThreshold;
	float speed = b3Length( b3Body_GetLinearVelocity( actor->bodyId ) );
	float spin = b3Length( b3Body_GetAngularVelocity( actor->bodyId ) ) * actor->radius;

	// Rubble that came back to life, when a piece under it moved away or an impact went off nearby, and did not start to
	// move is still carried. It need not wait the whole rest time, a large piece on its stubs would cost hundreds of
	// contacts meanwhile. A part of a building does not take this way: it comes back to life to sink further, the cells
	// under it give way slower than this, and it would freeze again after three steps, wake when the next cell moved and
	// never come down.
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
	if ( actor->isBuildingPart )
	{
		return rocking;
	}

	if ( speed > slow || spin > slow )
	{
		actor->restTime = 0.0f;
		return false;
	}

	float restTime = world->def.debrisRestTime;
	if ( actor->restTime == 0.0f || nbPoseDistance( actor->restPose, transform, actor->radius ) > NB_REST_SPEED * restTime )
	{
		actor->restPose = transform;
		actor->restTime = 0.0f;
	}
	actor->restTime = b3MinFloat( actor->restTime + timeStep, 1.0f );
	return actor->restTime >= restTime || rocking || probed;
}

// Box3D sweeps a body against static shapes once it moves more than its safety factor times the radius of the largest
// sphere in its thinnest shape within one step. Most debris is small, and Box3D's default of half that radius would sweep
// nearly every piece that flies. See nbWorldDef::debrisSweepDistance.
static void nbUpdateSweep( const nbWorld* world, const nbActor* actor )
{
	float distance = world->def.debrisSweepDistance;
	if ( distance > 0.0f )
	{
		float radius = b3Body_GetMinExtent( actor->bodyId );
		b3Body_SetSafetyFactor( actor->bodyId, b3ClampFloat( distance / b3MaxFloat( radius, 1.0e-3f ), 0.5f, 2.0f ) );
	}
}

// Mass, center of mass, reach and sweep of a dynamic actor from its shapes
static void nbUpdateActorMass( nbWorld* world, nbActor* actor )
{
	b3Body_ApplyMassFromShapes( actor->bodyId );
	nbUpdateSweep( world, actor );
	actor->localCenter = b3Body_GetLocalCenter( actor->bodyId );
	world->maxCenterOffset = b3MaxFloat( world->maxCenterOffset, b3Length( actor->localCenter ) );
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

#if defined( B3_HAS_SHAPE_BATCH )
// The shapes of the bodies that froze or came back to life go into Box3D's trees at the end of their batch, which is part
// of the type changes in the profile
static void nbEndTypeBatch( nbWorld* world )
{
	uint64_t ticks = b3GetTicks();
	b3World_EndShapeBatch( world->physicsWorld );
	world->profile.stages[nb_profileTypeChanges] += b3GetMilliseconds( ticks );
}
#endif

// Debris at rest becomes static and leaves the island it was part of. Box3D wakes that island once when the first of its
// bodies changes type, the others freeze in the same update.
static void nbFreezeActor( nbWorld* world, int actorIndex )
{
	uint64_t ticks = b3GetTicks();
	nbActor* actor = world->actors.data + actorIndex;
	b3Body_SetType( actor->bodyId, b3_staticBody );
	actor->isRubble = true;
	nbMarkMoving( world, actor );
	actor->holdsRubble = false;
	actor->probing = false;
	actor->carrierCount = 0;
	nbResetRest( actor );
	world->rubbleCount += 1;
	world->profile.stages[nb_profileTypeChanges] += b3GetMilliseconds( ticks );
	world->profile.freezeCount += 1.0;
}

// Bring rubble back to life. The rubble resting on it stays until it moves, then follows. Box3D gives the body its mass
// back, the center of mass moved if impacts broke pieces off the rubble.
void nbThawActor( nbWorld* world, int actorIndex )
{
	uint64_t ticks = b3GetTicks();
	nbActor* actor = world->actors.data + actorIndex;
	NB_ASSERT( actor->isRubble );
	actor->isRubble = false;
	nbMarkMoving( world, actor );
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
	world->profile.stages[nb_profileTypeChanges] += b3GetMilliseconds( ticks );
	world->profile.thawCount += 1.0;
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

	// Rubble to leave alone, unless it froze on the holder
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

// The moving bodies that rest on a large piece, see nbFindSupport, in a table with open addressing that stays at most half
// full. The search looks one up for almost every contact of the piece, and a storey on a heap of debris has a thousand.
typedef struct nbRestingSet
{
	int* places;
	int bits;
	int count;
} nbRestingSet;

// The bits of a place in the resting set of a piece with this many touching contacts. The set has 1 << bits places.
static int nbRestingBits( int touchCount )
{
	int bits = 4;
	while ( ( 1 << bits ) < 2 * touchCount )
	{
		bits += 1;
	}
	return bits;
}

static uint32_t nbRestingPlace( const nbRestingSet* set, int actorIndex )
{
	return ( (uint32_t)actorIndex * 2654435761u ) >> ( 32 - set->bits );
}

static void nbAddResting( nbRestingSet* set, int actorIndex )
{
	uint32_t mask = ( 1u << set->bits ) - 1;
	for ( uint32_t place = nbRestingPlace( set, actorIndex );; place = ( place + 1 ) & mask )
	{
		if ( set->places[place] == actorIndex )
		{
			return;
		}

		if ( set->places[place] == NB_NULL_INDEX )
		{
			set->places[place] = actorIndex;
			set->count += 1;
			return;
		}
	}
}

// Whether a moving body rests on the piece of the set. No set holds none.
static bool nbHoldsActor( const nbRestingSet* set, int actorIndex )
{
	if ( set == NULL || set->count == 0 )
	{
		return false;
	}

	uint32_t mask = ( 1u << set->bits ) - 1;
	for ( uint32_t place = nbRestingPlace( set, actorIndex );; place = ( place + 1 ) & mask )
	{
		if ( set->places[place] == actorIndex )
		{
			return true;
		}

		if ( set->places[place] == NB_NULL_INDEX )
		{
			return false;
		}
	}
}

// The rubble a search of nbIsSupportedWithout has seen: the first and every carrier it pushes, up to NB_MAX_CARRIERS for
// each of the NB_SUPPORT_SEARCH bodies it visits, in a table with open addressing that stays about half full. The search
// keeps it on its own stack instead of a stamp in the actors, so several threads can search at once.
#define NB_SEEN_PLACES 512
_Static_assert( NB_SEEN_PLACES >= 2 * NB_SUPPORT_SEARCH * NB_MAX_CARRIERS, "the table of the search stays about half full" );

// Whether the actor is new to the table, which then holds it
static bool nbSee( int* places, int actorIndex )
{
	uint32_t place = ( (uint32_t)actorIndex * 2654435761u ) >> 23;
	while ( places[place] != NB_NULL_INDEX )
	{
		if ( places[place] == actorIndex )
		{
			return false;
		}
		place = ( place + 1 ) & ( NB_SEEN_PLACES - 1 );
	}
	places[place] = actorIndex;
	return true;
}

// The moving actors a search read. While the budget freezes debris, only they can change what the search finds: rubble
// and structures stay as they are until it is done, see nbBudgetLook.
typedef struct nbMoverLog
{
	int* actors;
	int count;
	int capacity;

	// It read more than it has room for
	bool full;
} nbMoverLog;

static void nbLogMover( nbMoverLog* log, int actorIndex )
{
	if ( log->count < log->capacity )
	{
		log->actors[log->count++] = actorIndex;
	}
	else
	{
		log->full = true;
	}
}

// Whether rubble lies on something that holds without an actor, following the chunks each rubble body froze on. Chunks
// that are gone, moved away or part of the actor carry nothing, and so do moving bodies that rest on the actor, the set
// from nbFindSupport, and rubble frozen in flight without a record. Everything else counts as carried: the ground, a
// structure, other moving bodies, and rubble whose search runs past the limit. It only reads the world, and the moving
// bodies it reads go to the log if there is one.
static bool nbIsSupportedWithout( const nbWorld* world, int rubbleIndex, int actorIndex, const nbRestingSet* resting,
								  nbMoverLog* log )
{
	int stack[NB_SUPPORT_SEARCH];
	int count = 0;
	int visits = 0;
	int seen[NB_SEEN_PLACES];
	memset( seen, 0xff, sizeof( seen ) );
	nbSee( seen, rubbleIndex );
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
			if ( chunk->generation != record->generation || chunk->actorIndex == actorIndex )
			{
				continue;
			}

			// A moving carrier that freezes is rubble to the search from then on
			const nbActor* carrier = world->actors.data + chunk->actorIndex;
			if ( log != NULL && carrier->isRubble == false && carrier->isStatic == false )
			{
				nbLogMover( log, chunk->actorIndex );
			}

			if ( nbStillCarries( chunk, record->point ) == false )
			{
				continue;
			}

			if ( carrier->isRubble == false )
			{
				if ( carrier->isStatic || nbHoldsActor( resting, chunk->actorIndex ) == false )
				{
					return true;
				}
				continue;
			}

			if ( nbSee( seen, chunk->actorIndex ) )
			{
				if ( count == NB_SUPPORT_SEARCH )
				{
					return true;
				}
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
		actor->budgetTime = world->time;
	}

	// The rubble a piece broke off stays, unless it froze on that piece since
	if ( actor->isRubble == false || actor->settleStamp == world->settleStamp ||
		 ( actorIndex == thawContext->skipActor && actor->generation == thawContext->skipGeneration &&
		   ( thawContext->holder == NB_NULL_INDEX || nbRestsOn( world, actor, thawContext->holder ) == false ) ) )
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

static int nbCompareInts( const void* a, const void* b )
{
	int x = *(const int*)a;
	int y = *(const int*)b;
	return ( x > y ) - ( x < y );
}

// Rubble in the boxes, in the order of the actors. Many boxes are searched with one query over all of them. With a
// direction, only the rubble above the height. Returns how much rubble it left past its limit.
static int nbThawRubbleInBoxes( nbWorld* world, const b3AABB* boxes, int boxCount, b3Vec3 up, float height,
								const nbThawContext* filter )
{
	if ( world->rubbleCount == 0 || boxCount == 0 )
	{
		return 0;
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
#if defined( B3_HAS_STATIC_OVERLAP )
	// Rubble is static, so only Box3D's tree of static bodies holds it. A collapse also gives the moving debris in the boxes
	// the grace and needs the other trees, whose shapes the callback skips otherwise.
	if ( context.collapse == false )
	{
		b3World_OverlapStaticAABB( world->physicsWorld, total, b3DefaultQueryFilter(), nbThawCallback, &context );
	}
	else
	{
		b3World_OverlapAABB( world->physicsWorld, total, b3DefaultQueryFilter(), nbThawCallback, &context );
	}
#else
	b3World_OverlapAABB( world->physicsWorld, total, b3DefaultQueryFilter(), nbThawCallback, &context );
#endif

	// Box3D finds the rubble in the order of its static tree, which depends on how the shapes went in. The order of the
	// actors does not, so the same rubble comes back to life first.
	if ( thawed->count > 1 )
	{
		qsort( thawed->data, (size_t)thawed->count, sizeof( int ), nbCompareInts );
	}

	// Past the limit the rest stays rubble for now. The rubble goes into Box3D's tree of moving bodies together, see
	// b3World_BeginShapeBatch.
#if defined( B3_HAS_SHAPE_BATCH )
	b3World_BeginShapeBatch( world->physicsWorld );
#endif
	int limit = filter != NULL && filter->collapse ? thawed->count : NB_MAX_THAW;
	int left = b3MaxInt( thawed->count - limit, 0 );
	for ( int i = 0; i < thawed->count && i < limit; ++i )
	{
		nbThawActor( world, thawed->data[i] );
		if ( filter != NULL && filter->grace )
		{
			world->actors.data[thawed->data[i]].budgetTime = world->time;
		}
	}
#if defined( B3_HAS_SHAPE_BATCH )
	nbEndTypeBatch( world );
#endif
	thawed->count = 0;
	return left;
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
		if ( ( actor->debrisIndex >> 6 ) == world->movingDebris.count )
		{
			nbArray_Push( world->movingDebris, 0 );
			nbArray_Push( world->largeDebris, 0 );
		}
		nbMarkMoving( world, actor );
		nbMarkLarge( world, actor );
	}
	else if ( isDebris == false && actor->debrisIndex != NB_NULL_INDEX )
	{
		nbRemoveDebris( world, actorIndex );
	}
}

// Let a map from Box3D indices cover the first count of them
static void nbCoverIndexMap( nbIntArray* map, int count )
{
	if ( map->count < count )
	{
		nbArray_Reserve( *map, count );
		for ( int i = map->count; i < count; ++i )
		{
			map->data[i] = NB_NULL_INDEX;
		}
		map->count = count;
	}
}

// Every static chunk has a Box3D body, so the first debris body gets an index past all of them. The maps from body and
// shape indices cover them already, instead of filling hundreds of thousands of entries in the first impact.
void nbCoverIndices( nbWorld* world )
{
	b3Counters counters = b3World_GetCounters( world->physicsWorld );
	nbCoverIndexMap( &world->bodyToActor, counters.bodyCount );
	nbCoverIndexMap( &world->shapeToChunk, counters.shapeCount );
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

b3ShapeDef nbMakeShapeDef( const nbDestructible* destructible, const nbMaterial* material, bool isStatic )
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
	// Every chunk has a hull of its own, so Box3D's hull database would store each hull once anyway. It would also hash
	// them all into a table that rehashes all its hulls when it fills up, which stalls a large scene for many milliseconds.
	// So Box3D uses the hull built into the shape of the chunk in place, and the geometry is stored once, see nbShape.
	// Without that addition Box3D keeps a copy of its own.
#if defined( B3_HAS_EXTERNAL_HULLS )
	shapeDef.externalHull = true;
#elif defined( B3_HAS_UNIQUE_HULLS )
	shapeDef.uniqueHull = true;
#endif
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
		if ( chunk->shape == NULL || ( chunk->flags & nb_chunkMoved ) == 0 || ( chunk->flags & nb_chunkStaticBody ) != 0 ||
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

#if defined( B3_HAS_SHAPE_BATCH )
	// Box3D sorts every shape into its tree with a search from the root. The shapes of a commit, all chunks of a building
	// that is built or the fragments of an impact, go in together instead, those close together with one search. So do
	// the shapes of a body that left the simulation for the commit.
	b3World_BeginShapeBatch( world->physicsWorld );
#endif

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

		// The hull built into the shape of the chunk, Box3D uses it in place
		const b3HullData* hull = chunk->shape->hull;
		NB_ASSERT( hull != NULL );
		b3ShapeId oldShapeId = chunk->shapeId;
		b3BodyId oldBodyId = chunk->bodyId;
		bool oldStaticBody = ( chunk->flags & nb_chunkStaticBody ) != 0;

		b3BodyId bodyId;
		if ( actor->isStatic )
		{
			// Static-static pairs never collide, so static chunks cost nothing in the solver
			bodyId = nbGetStaticBody( world, destructible );
			chunk->flags |= nb_chunkStaticBody;
		}
		else
		{
			bodyId = actor->bodyId;
			chunk->flags &= ~nb_chunkStaticBody;
		}

		// Create the new shape before destroying the old one, so the new shape never takes over the id of the old one
		chunk->shapeId = b3CreateHullShape( bodyId, &shapeDef, hull );
		chunk->bodyId = bodyId;
		nbMapShape( world, chunk->shapeId, chunkIndex );
		actor->massDirty = true;

		if ( B3_IS_NON_NULL( oldShapeId ) )
		{
			nbUnmapShape( world, oldShapeId );
			nbDestroyChunkShape( oldShapeId, oldBodyId, oldStaticBody );
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

#if defined( B3_HAS_SHAPE_BATCH )
	b3World_EndShapeBatch( world->physicsWorld );
#endif

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
	actor->fromStructure = source->isStatic || source->isRubble || source->holdsRubble || source->fromStructure;
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

// Chunks crushed in one structure per update of 1/60 s at most, in proportion for other steps, see nbLimitForStep, but
// never more than NB_CRUSH_ROOM. The next update carries on with what is left.
#define NB_MAX_CRUSHES 4
#define NB_CRUSH_ROOM 32

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

// Chunks the load checks of one update of 1/60 s visit at most, in proportion for other steps, see nbLimitForStep.
// Structures beyond wait for the next update.
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

// A part of a building that came down keeps its storeys while its local Y axis stays within 45 degrees of up
#define NB_UPRIGHT 0.7071f

// Rubble this close around the remains of a storey that gives way comes back to life, so they can fly out
#define NB_CLEARANCE 1.0f

// A storey that gave way does not give way again for remains of less than this part of its walls, nor for what stayed of
// it, see nbStorey::kept
#define NB_STOREY_REMAINS 0.1f

// The walls a storey lost lie on one side when their center lies farther from the center of the walls it had than this
// part of the way to the edge of the storey, see nbFindRemains
#define NB_ONE_SIDED 0.35f

// The random numbers that pick what stays of a storey with damage all around, one stream for every storey
#define NB_REMAINS_STREAM 0x52454d41494e53ull

// Remaining health of a bond over the health of an undamaged bond of the same area and materials
static float nbBondIntegrity( const nbWorld* world, const nbBond* bond )
{
	float full = nbGetBondStrength( world, bond ) * bond->area;
	return full > 0.0f ? b3ClampFloat( bond->health / full, 0.01f, 1.0f ) : 0.01f;
}

// a^T M b with the second moments M of a bond
static float nbMomentForm( const nbBondMoments* moments, b3Vec3 a, b3Vec3 b )
{
	b3Vec3 m = moments->moments;
	b3Vec3 c = moments->crossMoments;
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
static float nbSectionModulus( const nbBond* bond, const nbBondMoments* moments, b3Vec3 axis )
{
	float trace = moments->moments.x + moments->moments.y + moments->moments.z;
	float spread = b3MaxFloat( trace - nbMomentForm( moments, axis, axis ), 0.0f );
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
	NB_ASSERT( destructible->bondMoments );
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
			const nbBondMoments* moments = world->bondMoments.data + paths[p].bondIndex;
			float f = paths[p].weight / total;
			b3Vec3 d = b3Sub( paths[p].point, center );
			float dx = b3Dot( d, side1 );
			float dz = b3Dot( d, side2 );
			xx += f * ( dx * dx + nbMomentForm( moments, side1, side1 ) );
			xz += f * ( dx * dz + nbMomentForm( moments, side1, side2 ) );
			zz += f * ( dz * dz + nbMomentForm( moments, side2, side2 ) );
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
			const nbBondMoments* moments = world->bondMoments.data + path->bondIndex;
			b3Vec3 d = b3Sub( path->point, center );
			float factor = 1.0f + lx * b3Dot( d, side1 ) + lz * b3Dot( d, side2 );
			path->weight *= b3MaxFloat( factor, 0.0f );
			if ( factor > 0.0f )
			{
				float m11 = nbMomentForm( moments, side1, side1 );
				float m12 = nbMomentForm( moments, side1, side2 );
				float m22 = nbMomentForm( moments, side2, side2 );
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
				const nbBondMoments* moments = world->bondMoments.data + paths[p].bondIndex;
				b3Vec2 c = { b3Dot( bond->centroid, side1 ), b3Dot( bond->centroid, side2 ) };
				float e1 = sqrtf( 3.0f * nbMomentForm( moments, side1, side1 ) );
				float e2 = sqrtf( 3.0f * nbMomentForm( moments, side2, side2 ) );
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
				const nbBondMoments* moments = world->bondMoments.data + path->bondIndex;
				path->bending = NB_BENDING_STRENGTH * strength * nbSectionModulus( bond, moments, axis ) * path->integrity;
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

// Four times the largest relative error of rounding to a float
#define NB_FLOAT_SLACK ( 4.0 / 16777216.0 )

// Whether any storey can give way, see nbWorldDef::storeyCollapse
static bool nbStoreysCollapse( const nbWorld* world )
{
	return world->def.storeyCollapse.damageMax > 0.0f || world->def.topStoreyCollapse.damageMax > 0.0f;
}

// How a storey gives way: the top one under the roof as nbWorldDef::topStoreyCollapse, the others as
// nbWorldDef::storeyCollapse
static const nbStoreyCollapse* nbStoreyCollapseOf( const nbWorld* world, const nbDestructible* destructible, int storeyIndex )
{
	return storeyIndex == destructible->storeyCount - 1 ? &world->def.topStoreyCollapse : &world->def.storeyCollapse;
}

// The part of the walls it had that a storey loses before it gives way, where the storey lies in the range. Zero for a
// storey that never does.
static float nbStoreyDamage( const nbWorld* world, const nbDestructible* destructible, int storeyIndex )
{
	const nbStoreyCollapse* collapse = nbStoreyCollapseOf( world, destructible, storeyIndex );
	return collapse->damageMin + destructible->storeys[storeyIndex].damageRoll * ( collapse->damageMax - collapse->damageMin );
}

// The part of the walls that are left that stays of a storey that gives way, where the storey lies in the range
static float nbStoreyResidue( const nbWorld* world, const nbDestructible* destructible, int storeyIndex )
{
	const nbStoreyCollapse* collapse = nbStoreyCollapseOf( world, destructible, storeyIndex );
	return collapse->residueMin +
		   destructible->storeys[storeyIndex].residueRoll * ( collapse->residueMax - collapse->residueMin );
}

// Keeps the chunks of a cell, by its first part, among those that stay, see nbFindRemains, and marks every cell bonded to
// one of them at its first part. The chunks of the part carry the stamp and their place in its list.
static void nbKeepCell( const nbWorld* world, const int* chunks, const int* roots, int cell, int n, uint32_t stamp, bool* remains,
						bool* touches )
{
	for ( int k = cell; k < n; ++k )
	{
		if ( roots[k] != cell )
		{
			continue;
		}

		remains[k] = true;
		const nbChunk* chunk = world->chunks.data + chunks[k];
		for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
		{
			const nbBond* bond = world->bonds.data + ( key >> 1 );
			int side = key & 1;
			key = bond->nextKey[side];

			const nbChunk* other = world->chunks.data + bond->chunk[side ^ 1];
			if ( other->searchStamp == stamp )
			{
				touches[roots[other->scratch]] = true;
			}
		}
	}
}

// What stays of a storey that gives way: the part of the walls the part has left in it that nbStoreyResidue gives, as
// whole cells, as close to that as whole cells come. When the walls the storey lost lie mostly on one side, the cells
// closest to the middle of the opposite side, one next to the other, the anchor the part above tilts over, a column of
// them from the floor to the ceiling. When the damage is spread around, cells picked at random, which mostly hang on
// nothing and fall. nbCheckStoreys then breaks what stays into its cells. remaining and moment are the volume and the
// first moment of the walls left in the storey in all parts that stand, roots the first part of the cell of every chunk
// of the part, and the cells carry their volume and center at their first part. Sets the chunks that stay and the cells
// bonded to them, see nbKeepCell, and the volume that stays. Returns whether what stays is an anchor on one side.
static bool nbFindRemains( nbWorld* world, const nbDestructible* destructible, int storeyIndex, float remaining, b3Vec3 moment,
						   const int* chunks, const int* roots, const float* cellVolumes, const b3Vec3* centers,
						   const float* heights, int n, uint32_t stamp, bool* remains, bool* touches, float* keptOut )
{
	const nbStorey* storey = destructible->storeys + storeyIndex;
	int* members = nbArena_AllocArray( &world->arena, int, n );
	int memberCount = 0;
	float walls = 0.0f;
	for ( int k = 0; k < n; ++k )
	{
		if ( roots[k] == k && heights[k] > storey->low && heights[k] < storey->high )
		{
			members[memberCount++] = k;
			walls += cellVolumes[k];
		}
	}

	// The center of what the storey lost lies across the center it had from the center of what is left
	float target = nbStoreyResidue( world, destructible, storeyIndex ) * walls;
	float lost = storey->volume - remaining;
	b3Vec3 offset = b3Vec3_zero;
	if ( lost > 0.0f && remaining > 0.0f )
	{
		offset = b3MulSV( remaining / lost, b3Sub( storey->center, b3MulSV( 1.0f / remaining, moment ) ) );
	}
	offset.y = 0.0f;
	float distance = b3Length( offset );
	b3Vec3 direction = distance > 0.0f ? b3MulSV( 1.0f / distance, offset ) : b3Vec3_zero;
	float reach = b3AbsFloat( direction.x ) * storey->extent.x + b3AbsFloat( direction.z ) * storey->extent.z;
	bool oneSided = reach > 0.0f && distance > NB_ONE_SIDED * reach;
	b3Vec3 aim = b3MulSub( storey->center, reach, direction );
	nbRandom rng = nbMakeRandom( destructible->seed, NB_REMAINS_STREAM + (uint64_t)storeyIndex );

	float kept = 0.0f;
	while ( memberCount > 0 && target > 0.0f )
	{
		// On one side the cell closest to the middle of the strongest side that is bonded to a cell that stays, or the
		// closest of all when none is
		int pick = 0;
		if ( oneSided )
		{
			bool pickBonded = false;
			float pickDistance = FLT_MAX;
			for ( int m = 0; m < memberCount; ++m )
			{
				bool bonded = kept > 0.0f && touches[members[m]];
				b3Vec3 d = b3Sub( centers[members[m]], aim );
				float d2 = d.x * d.x + d.z * d.z;
				if ( ( bonded && pickBonded == false ) || ( bonded == pickBonded && d2 < pickDistance ) )
				{
					pick = m;
					pickBonded = bonded;
					pickDistance = d2;
				}
			}
		}
		else
		{
			pick = (int)( nbRandomU32( &rng ) % (uint32_t)memberCount );
		}

		int cell = members[pick];
		if ( kept > 0.0f && kept + 0.5f * cellVolumes[cell] > target )
		{
			break;
		}

		nbKeepCell( world, chunks, roots, cell, n, stamp, remains, touches );
		kept += cellVolumes[cell];
		members[pick] = members[--memberCount];
	}
	*keptOut = kept;
	return oneSided;
}

// Whether every storey of a building keeps clearly more of its walls than it needs, from what is left of them, see
// nbDestructible::standing. The sum in nbCheckStoreys adds up the same volumes in float, and its rounding errors add up
// to at most one such error per chunk. This keeps four times that apart, so the sum would not find a storey that gives
// way either.
static bool nbStoreysHold( const nbWorld* world, const nbDestructible* destructible )
{
	double keep = 1.0 - ( destructible->chunkCount + 16.0 ) * NB_FLOAT_SLACK;
	for ( int s = 0; s < destructible->storeyCount; ++s )
	{
		float damage = nbStoreyDamage( world, destructible, s );
		double need = ( 1.0 - (double)damage ) * destructible->storeys[s].volume * ( 1.0 + NB_FLOAT_SLACK );
		if ( damage > 0.0f && keep * destructible->standing[s] < need )
		{
			return false;
		}
	}
	return true;
}

// A storey of a building gives way once the part of the walls it had that nbStoreyDamage gives is destroyed, on any floor
// and whatever they could still carry. The part of the walls that are left that nbStoreyResidue gives stays where it is,
// see nbFindRemains, broken into its cells, and lets go only of what comes down. The rest flies out like the stones
// under a collapsing block, see nbCheckSupports, and everything above comes down in one piece, so the rooms up there
// stay whole. It tilts over what stayed as gravity and contacts make it, nothing turns it, and so does the roof over the
// top storey, which takes the cells of an anchor it hangs on along. A cell goes where its center lies, so the cracks
// follow the faces of the cells.
// The storeys count the walls of all static parts of the destructible, and a part lets go of what it holds itself.
// Returns the chunks to throw and the bonds to break in arena memory, and a chunk of every part that comes down.
static int nbCheckStoreys( nbWorld* world, int actorIndex, b3Vec3 up, nbOverload** overloadsOut, int** downsOut,
						   int* downCountOut )
{
	*downCountOut = 0;
	const nbActor* actor = world->actors.data + actorIndex;
	const nbDestructible* destructible = world->destructibles.data + actor->destructibleIndex;
	if ( nbStoreysCollapse( world ) == false || destructible->storeyCount == 0 )
	{
		return 0;
	}

	// A part that came down only while it still stands more or less upright, a building lying on its side has no storeys
	b3WorldTransform transform = nbActor_GetTransform( world, actor );
	if ( actor->isStatic == false && b3Dot( b3RotateVector( transform.q, b3Vec3_axisY ), up ) < NB_UPRIGHT )
	{
		return 0;
	}

	// Most checks end here, the sum below only decides close to the limit. Debug builds sum up anyway and check this.
	bool clear = nbStoreysHold( world, destructible );
#if defined( NDEBUG ) && !defined( NB_ENABLE_ASSERT )
	if ( clear )
	{
		return 0;
	}
#endif

	// What is left of every storey, in the parts of the building that stand and those that came down, and where it is
	float volumes[NB_MAX_STOREYS];
	b3Vec3 moments[NB_MAX_STOREYS];
	for ( int s = 0; s < destructible->storeyCount; ++s )
	{
		volumes[s] = 0.0f;
		moments[s] = b3Vec3_zero;
	}

	// The parts that stand, found from the end of the list: The list holds every fragment of the building, newest first,
	// and the building itself is the oldest. The volumes add up in the order of the list as before.
	int partCount = destructible->buildingPartCount;
	int* parts = nbArena_AllocArray( &world->arena, int, partCount > 0 ? partCount : 1 );
	int found = 0;
	for ( int a = destructible->tailActor; a != NB_NULL_INDEX && found < partCount; a = world->actors.data[a].prevActor )
	{
		const nbActor* part = world->actors.data + a;
		if ( part->isStatic || part->isBuildingPart )
		{
			parts[found++] = a;
		}
	}

#if !defined( NDEBUG )
	// The same parts in the same order as from the front
	int expected = found;
	for ( int a = destructible->headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
	{
		const nbActor* part = world->actors.data + a;
		if ( part->isStatic || part->isBuildingPart )
		{
			expected -= 1;
			NB_ASSERT( expected >= 0 && parts[expected] == a );
		}
	}
	NB_ASSERT( expected == 0 );
#endif

	// The loop waited for the shape of every chunk, so a second cursor fetches the shapes eight chunks ahead
	for ( int i = found - 1; i >= 0; --i )
	{
		const nbActor* part = world->actors.data + parts[i];
		int ahead = part->headChunk;
		for ( int k = 0; k < 8 && ahead != NB_NULL_INDEX; ++k )
		{
			const nbShape* aheadShape = world->chunks.data[ahead].shape;
			NB_PREFETCH( &aheadShape->centroid );
			NB_PREFETCH( &aheadShape->volume );
			ahead = world->chunks.data[ahead].nextChunk;
		}

		for ( int c = part->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
		{
			if ( ahead != NB_NULL_INDEX )
			{
				const nbShape* aheadShape = world->chunks.data[ahead].shape;
				NB_PREFETCH( &aheadShape->centroid );
				NB_PREFETCH( &aheadShape->volume );
				ahead = world->chunks.data[ahead].nextChunk;
			}

			const nbShape* shape = world->chunks.data[c].shape;
			int s = nbFindStorey( destructible, shape->centroid.y );
			if ( s >= 0 )
			{
				volumes[s] += shape->volume;
				moments[s] = b3MulAdd( moments[s], shape->volume, shape->centroid );
			}
		}
	}

	int failing[NB_MAX_STOREYS];
	int failingCount = 0;
	for ( int s = 0; s < destructible->storeyCount; ++s )
	{
		float damage = nbStoreyDamage( world, destructible, s );
		if ( damage > 0.0f && volumes[s] < ( 1.0f - damage ) * destructible->storeys[s].volume )
		{
			failing[failingCount++] = s;
		}
	}

#if !defined( NDEBUG ) || defined( NB_ENABLE_ASSERT )
	// What is left is kept as the sum finds it, apart from rounding
	for ( int s = 0; s < destructible->storeyCount; ++s )
	{
		double standing = destructible->standing[s];
		double slack = ( destructible->chunkCount + 16.0 ) * NB_FLOAT_SLACK * ( standing + destructible->storeys[s].volume );
		NB_ASSERT( volumes[s] - standing <= slack && standing - volumes[s] <= slack );
	}
	NB_ASSERT( clear == false || failingCount == 0 );
#endif

	if ( failingCount == 0 )
	{
		return 0;
	}

	// The height of the center of the cell of every chunk, see nbBond::sibling, and the center of every cell at its first
	// part
	int n = actor->chunkCount;
	int* chunks = nbArena_AllocArray( &world->arena, int, n );
	int* cells = nbArena_AllocArray( &world->arena, int, n );
	float* cellVolumes = nbArena_AllocArray( &world->arena, float, n );
	float* heights = nbArena_AllocArray( &world->arena, float, n );
	b3Vec3* centers = nbArena_AllocArray( &world->arena, b3Vec3, n );
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
		centers[count] = b3Vec3_zero;
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
		centers[root] = b3MulAdd( centers[root], shape->volume, shape->centroid );
	}

	// A cell's root is its first part, so it is done before the other parts
	for ( int k = 0; k < n; ++k )
	{
		int root = nbFindBlock( cells, k );
		heights[k] = k == root ? heights[k] / cellVolumes[k] : heights[root];
		centers[k] = k == root ? b3MulSV( 1.0f / cellVolumes[k], centers[k] ) : centers[k];
		cells[k] = root;
	}

	// What an impact cut off the building since the last update, a part as large as a tenth of a storey or more
	int* fresh = nbArena_AllocArray( &world->arena, int, destructible->actorCount );
	float* freshLows = nbArena_AllocArray( &world->arena, float, destructible->actorCount );
	float* freshHighs = nbArena_AllocArray( &world->arena, float, destructible->actorCount );
	int freshCount = 0;
	for ( int a = destructible->headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
	{
		const nbActor* part = world->actors.data + a;
		if ( part->isStatic || part->isRubble || part->freeTime < world->time )
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

		// Of a storey that gave way before, a few fragments left under a part that came down do not count, nor what stayed
		// of it, with a little more against the rounding of the sums
		bool before = ( destructible->collapsedStoreys & ( 1u << failing[f] ) ) != 0;
		if ( walls == 0.0f || ( before && walls < b3MaxFloat( NB_STOREY_REMAINS * storey->volume, 1.001f * storey->kept ) ) )
		{
			continue;
		}

		for ( int i = 0; i < freshCount; ++i )
		{
			nbActor* part = world->actors.data + fresh[i];
			if ( freshLows[i] > storey->low && freshHighs[i] > storey->high && part->volume >= 0.1f * storey->volume )
			{
				part->fromCollapse = true;
				nbMakeBuildingPart( world, part );
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

	// Every chunk flies out of a storey that gives way, -1, or stays with the stretch of the building between two of them.
	// The storey of every chunk in one, by its place among the storeys that give way.
	int* stretches = nbArena_AllocArray( &world->arena, int, n );
	int* insides = nbArena_AllocArray( &world->arena, int, n );
	bool* remains = nbArena_AllocArray( &world->arena, bool, n );
	bool* touches = nbArena_AllocArray( &world->arena, bool, n );
	for ( int k = 0; k < n; ++k )
	{
		stretches[k] = 0;
		insides[k] = -1;
		remains[k] = false;
		touches[k] = false;
		for ( int t = 0; t < storeyCount && heights[k] > destructible->storeys[storeys[t]].low; ++t )
		{
			stretches[k] = heights[k] >= destructible->storeys[storeys[t]].high ? t + 1 : -1;
			if ( stretches[k] < 0 )
			{
				insides[k] = t;
				break;
			}
		}
	}

	// Part of what is left of each storey stays where it is, on one side an anchor
	bool anchors[NB_MAX_STOREYS];
	float kepts[NB_MAX_STOREYS];
	for ( int t = 0; t < storeyCount; ++t )
	{
		int s = storeys[t];
		anchors[t] = nbFindRemains( world, destructible, s, volumes[s], moments[s], chunks, cells, cellVolumes, centers, heights,
									n, stamp, remains, touches, kepts + t );
	}

	// A stretch without the walls of a storey in it is only floors, the roof over the top storey, and hangs at the side
	// of the walls below. It would come down past an anchor. The cells of the anchor that hold it go with it instead and
	// rest on the rest of the anchor, so it tilts over that like the stories over a storey further down.
	bool walled[NB_MAX_STOREYS + 1];
	for ( int t = 0; t <= storeyCount; ++t )
	{
		walled[t] = false;
	}

	for ( int k = 0; k < n; ++k )
	{
		for ( int s = 0; s < destructible->storeyCount && stretches[k] > 0; ++s )
		{
			const nbStorey* storey = destructible->storeys + s;
			walled[stretches[k]] = walled[stretches[k]] || ( heights[k] > storey->low && heights[k] < storey->high );
		}
	}

	bool* holds = nbArena_AllocArray( &world->arena, bool, n );
	for ( int k = 0; k < n; ++k )
	{
		holds[k] = false;
	}

	for ( int k = 0; k < n; ++k )
	{
		if ( remains[k] == false || anchors[insides[k]] == false || walled[insides[k] + 1] )
		{
			continue;
		}

		const nbChunk* chunk = world->chunks.data + chunks[k];
		for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
		{
			const nbBond* bond = world->bonds.data + ( key >> 1 );
			int side = key & 1;
			key = bond->nextKey[side];

			int other = world->chunks.data[bond->chunk[side ^ 1]].scratch;
			holds[cells[k]] = holds[cells[k]] || stretches[other] == insides[k] + 1;
		}
	}

	for ( int k = 0; k < n; ++k )
	{
		if ( remains[k] && holds[cells[k]] )
		{
			stretches[k] = insides[k] + 1;
			remains[k] = false;
		}
	}

	// The rest flies out. What stays lets go of what comes down over its storey and breaks into its cells, each keeps
	// only its bonds to what is below, so the cells that stand on nothing else stand on each other. The bonds to chunks
	// that fly out break with them.
	nbOverload* overloads = nbArena_AllocArray( &world->arena, nbOverload, n + bondKeys );
	int overloadCount = 0;
	for ( int k = 0; k < n; ++k )
	{
		if ( stretches[k] < 0 && remains[k] == false )
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
			bool breaks = stretches[k] >= 0 ? other > k && stretches[other] >= 0 && stretches[other] != stretches[k]
						  : remains[other]	? other > k && cells[other] != cells[k]
											: stretches[other] > insides[k];
			if ( breaks )
			{
				overloads[overloadCount++] = (nbOverload){ bondIndex, NB_NULL_INDEX, 1.0f, false };
			}
		}
	}

	// Every stretch that comes down stays a building, and so does what is left of a part that came down before
	int* downs = nbArena_AllocArray( &world->arena, int, n );
	int downCount = 0;
	for ( int k = 0; k < n; ++k )
	{
		if ( stretches[k] > 0 || ( stretches[k] == 0 && actor->isStatic == false ) )
		{
			downs[downCount++] = chunks[k];
		}
	}

	// Each storey counts once, also when its remains fly out from under a part that came down before, and keeps what stayed
	// of it in every part
	nbDestructible* building = world->destructibles.data + actor->destructibleIndex;
	for ( int t = 0; t < storeyCount; ++t )
	{
		uint32_t bit = 1u << storeys[t];
		world->stats.collapsedStoreyCount += ( building->collapsedStoreys & bit ) == 0 ? 1 : 0;
		building->collapsedStoreys |= bit;
		building->storeys[storeys[t]].kept += kepts[t];
	}
	*overloadsOut = overloads;
	*downsOut = downs;
	*downCountOut = downCount;
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
		float x = b3Dot( shape->planes[f].normal, side1 );
		float z = b3Dot( shape->planes[f].normal, side2 );
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
	if ( ( world->def.supportScale <= 0.0f && nbStoreysCollapse( world ) == false ) || gravity <= 0.0f )
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

	int budget = nbLimitForStep( world, NB_SUPPORT_BUDGET );
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

		if ( visited > 0 && visited + actor->chunkCount > budget )
		{
			nbArray_Push( world->supportChecks, actorIndex );
			continue;
		}
		visited += actor->chunkCount;
		actor->supportDirty = false;

		// A building gives way storey by storey and never above a hole, only other structures take the load check
		nbBeginOperation( world );
		nbOverload* overloads = NULL;
		int* downs = NULL;
		int downCount = 0;
		bool collapsed = false;
		int overloadCount = nbCheckStoreys( world, actorIndex, up, &overloads, &downs, &downCount );
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
		int crushes[NB_CRUSH_ROOM];
		uint16_t crushGenerations[NB_CRUSH_ROOM];
		int maxCrushes = b3MinInt( nbLimitForStep( world, NB_MAX_CRUSHES ), NB_CRUSH_ROOM );
		int crushCount = 0;
		for ( ; crushCount < maxCrushes; ++crushCount )
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

		// Everything above a storey that gives way comes down onto what stays of it, also what came down there before and
		// froze, up to the top of the building. Frozen, that weighs nothing: when nothing flies out of the storey, what is
		// left of it would carry the rest above until the last of it is shot away, while the storey above, which gave way
		// first, came down onto its own remains with all its weight.
		if ( storey && downCount > 0 )
		{
			const nbDestructible* building = world->destructibles.data + actor->destructibleIndex;
			b3AABB above = b3Shape_GetAABB( world->chunks.data[downs[0]].shapeId );
			for ( int d = 1; d < downCount; ++d )
			{
				above = b3AABB_Union( above, b3Shape_GetAABB( world->chunks.data[downs[d]].shapeId ) );
			}

			b3Vec3 center = b3MulSV( 0.5f, b3Add( above.lowerBound, above.upperBound ) );
			b3Vec3 extents = b3MulSV( 0.5f, b3Sub( above.upperBound, above.lowerBound ) );
			float bottom = b3Dot( up, center ) - b3Dot( b3Abs( up ), extents );
			float height = building->storeys[building->storeyCount - 1].high - building->storeys[0].low + NB_CLEARANCE;
			b3Vec3 clearance = b3MulAdd( (b3Vec3){ NB_CLEARANCE, NB_CLEARANCE, NB_CLEARANCE }, -NB_CLEARANCE, b3Abs( up ) );
			above.lowerBound = b3Sub( above.lowerBound, clearance );
			above.upperBound = b3Add( above.upperBound, clearance );
			above = b3AABB_Union( above, (b3AABB){ b3MulAdd( above.lowerBound, height, up ), b3MulAdd( above.upperBound, height, up ) } );
			nbThawContext filter = { .skipActor = NB_NULL_INDEX, .holder = NB_NULL_INDEX, .collapse = true };
			nbThawRubbleInBoxes( world, &above, 1, up, bottom, &filter );
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

		// What comes down over a storey stays a building
		for ( int d = 0; d < downCount; ++d )
		{
			nbActor* part = world->actors.data + world->chunks.data[downs[d]].actorIndex;
			if ( part->isStatic == false )
			{
				nbMakeBuildingPart( world, part );
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

// The load check reads the second moments of the bonds. The destructibles it can check that did not keep them get them
// now, measured again on the shapes of their chunks. Bonds between Voronoi cells kept the moments of the face the cells
// were cut along, so theirs can differ in the last bits from what they would have kept.
void nbComputeBondMoments( nbWorld* world )
{
	int destructibleCount = world->destructibles.count;
	if ( destructibleCount == 0 )
	{
		return;
	}

	bool* pending = nbAlloc( sizeof( bool ) * (size_t)destructibleCount );
	bool any = false;
	for ( int i = 0; i < destructibleCount; ++i )
	{
		const nbDestructible* destructible = world->destructibles.data + i;
		pending[i] = destructible->isFree == false && destructible->bondMoments == false && destructible->storeyCount == 0;
		any = any || pending[i];
	}

	for ( int i = 0; i < world->bonds.count && any; ++i )
	{
		const nbBond* bond = world->bonds.data + i;
		if ( bond->chunk[0] == NB_NULL_INDEX || pending[world->chunks.data[bond->chunk[0]].destructibleIndex] == false )
		{
			continue;
		}

		// The tolerance the bonds of a new destructible are found with. Should the faces no longer be found, a square
		// of the same area in the plane of the bond stands in.
		const nbShape* a = world->chunks.data[bond->chunk[0]].shape;
		const nbShape* b = world->chunks.data[bond->chunk[1]].shape;
		nbBondGeometry geometry;
		if ( nbShape_ContactArea( a, b, 1.0e-3f, true, &geometry ) <= 0.0f )
		{
			b3Vec3 n = bond->normal;
			float s = bond->area / 12.0f;
			geometry.moments = (b3Vec3){ s * ( 1.0f - n.x * n.x ), s * ( 1.0f - n.y * n.y ), s * ( 1.0f - n.z * n.z ) };
			geometry.crossMoments = (b3Vec3){ -s * n.x * n.y, -s * n.x * n.z, -s * n.y * n.z };
		}
		nbKeepBondMoments( world, i, geometry.moments, geometry.crossMoments );
	}

	for ( int i = 0; i < destructibleCount; ++i )
	{
		world->destructibles.data[i].bondMoments = world->destructibles.data[i].bondMoments || pending[i];
	}
	nbFree( pending, sizeof( bool ) * (size_t)destructibleCount );
}

void nbWorld_SetSupportScale( nbWorldId worldId, float scale )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return;
	}

	world->def.supportScale = scale > 0.0f ? scale : 0.0f;
	if ( world->def.supportScale > 0.0f )
	{
		nbComputeBondMoments( world );
	}

	for ( int i = 0; i < world->actors.count; ++i )
	{
		const nbActor* actor = world->actors.data + i;
		if ( actor->isFree == false && actor->isStatic && actor->chunkCount > 0 )
		{
			nbMarkSupport( world, i );
		}
	}
}

// Both ends of every range between zero and one, the upper not below the lower
static nbStoreyCollapse nbClampStoreyCollapse( nbStoreyCollapse collapse )
{
	collapse.damageMin = b3ClampFloat( collapse.damageMin, 0.0f, 1.0f );
	collapse.damageMax = b3ClampFloat( collapse.damageMax, collapse.damageMin, 1.0f );
	collapse.residueMin = b3ClampFloat( collapse.residueMin, 0.0f, 1.0f );
	collapse.residueMax = b3ClampFloat( collapse.residueMax, collapse.residueMin, 1.0f );
	return collapse;
}

void nbWorld_SetStoreyCollapse( nbWorldId worldId, nbStoreyCollapse storeys, nbStoreyCollapse topStorey )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return;
	}

	world->def.storeyCollapse = nbClampStoreyCollapse( storeys );
	world->def.topStoreyCollapse = nbClampStoreyCollapse( topStorey );
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
	def.debrisRestTime = 0.1f;
	def.debrisSweepDistance = 0.2f;
	def.killDepth = -100.0f;
	def.supportScale = 0.0f;
	def.storeyCollapse = (nbStoreyCollapse){ 0.17f, 0.31f, 1.0f, 1.0f };
	def.topStoreyCollapse = (nbStoreyCollapse){ 0.18f, 0.25f, 1.0f, 1.0f };
	def.collisionSpeedThreshold = 4.0f;
	def.collisionDamageScale = 12.0f;
	def.collisionRadiusScale = 0.035f;
	def.maxCollisionImpactsPerUpdate = 4;
	def.maxFragmentsPerImpact = 160;
	def.fragmentScale = 1.0f;
	def.collisionPassThrough = 0.6f;
	def.workerCount = 1;
	def.asyncStep = true;
	def.stepRate = 60.0f;
	def.internalValue = NB_SECRET_COOKIE;
	return def;
}

// Use the pool or the task system of the application if it has one, otherwise start threads when more than one worker is
// wanted
static void nbStartWorkers( nbWorld* world, int workerCount )
{
	workerCount = workerCount < 1 ? 1 : ( workerCount > NB_MAX_WORKERS ? NB_MAX_WORKERS : workerCount );
	world->workerCount = workerCount;
	world->def.workerCount = workerCount;

	if ( world->def.taskSystem != NULL )
	{
		// The pool of the application, shared with Box3D
		world->scheduler = (nbScheduler*)world->def.taskSystem;
		world->ownsScheduler = false;
		world->enqueueTask = nbSchedulerEnqueueTask;
		world->finishTask = nbSchedulerFinishTask;
		world->userTaskContext = world->scheduler;
	}
	else if ( world->def.enqueueTask != NULL && world->def.finishTask != NULL )
	{
		world->enqueueTask = world->def.enqueueTask;
		world->finishTask = world->def.finishTask;
		world->userTaskContext = world->def.userTaskContext;
	}
	else if ( workerCount > 1 )
	{
		world->scheduler = nbCreateScheduler( workerCount - 1 );
		world->ownsScheduler = true;
		world->enqueueTask = nbSchedulerEnqueueTask;
		world->finishTask = nbSchedulerFinishTask;
		world->userTaskContext = world->scheduler;
	}

	for ( int i = 0; i < workerCount; ++i )
	{
		nbArena_Create( world->workerArenas + i, 64 * 1024 );
	}

	// The checks of nbSettleDebris hand out their slots to the new threads
	memset( world->settleThreads, 0, sizeof( world->settleThreads ) );
	world->settleThreadCount = 0;
}

static void nbStopWorkers( nbWorld* world )
{
	if ( world->ownsScheduler )
	{
		nbDestroyScheduler( world->scheduler );
	}
	world->scheduler = NULL;
	world->ownsScheduler = false;
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
	// When the task went to a worker, and how many milliseconds later it started there, negative for the task of the
	// calling thread
	uint64_t enqueueTicks;
	float wakeTime;

	nbFractureJob* jobs;
	int jobCount;
	const int* itemJobs;
	const int* itemCells;
	int itemCount;
	int siteCapacity;

	// Draws the sites of a job, run as the first items, and one flag per job that is set once its sites are drawn
	nbPrepareJobFn* prepare;
	void* prepareContext;
	int* prepared;

	// Shared counter that hands out the work items
	int* nextItem;

	nbArena* arena;
	nbFractureCounters counters;

	// The items this task took
	int takenCount;
} nbFractureTask;

static void nbFractureTaskMain( void* context )
{
	NB_ZONE( fracture_zone, "Fracture Task" );
	nbFractureTask* task = context;
	if ( task->enqueueTicks != 0 )
	{
		task->wakeTime = b3GetMilliseconds( task->enqueueTicks );
	}

	nbCellScratch scratch;
	nbCellScratch_Create( &scratch, task->arena, task->siteCapacity );

	for ( ;; )
	{
		int item = nbAtomicFetchAddInt( task->nextItem, 1 );
		if ( item >= task->itemCount )
		{
			break;
		}
		task->takenCount += 1;

		// The sites of every job come first. A cell of a job whose sites another worker still draws waits for them, all
		// of them were handed out before any cell.
		if ( task->prepare != NULL && item < task->jobCount )
		{
			task->prepare( task->prepareContext, item );
			nbAtomicFetchAddInt( task->prepared + item, 1 );
			continue;
		}

		int jobIndex = task->itemJobs[item];
		if ( task->prepare != NULL )
		{
			while ( nbAtomicFetchAddInt( task->prepared + jobIndex, 0 ) == 0 )
			{
				b3Yield();
			}
		}

		// A job has as many cells as sites, the items cover the most it can draw
		const nbFractureJob* job = task->jobs + jobIndex;
		int cellIndex = task->itemCells[item];
		if ( cellIndex < job->siteCount && ( task->prepare == NULL || job->siteCount >= 2 ) )
		{
			nbComputeCell( job, cellIndex, task->arena, &scratch, &task->counters );
		}
	}
	NB_ZONE_END( fracture_zone );
}

// How fast the running means of nbWorld::wakeEstimate follow a new measurement
#define NB_ESTIMATE_WEIGHT 0.125f

static void nbEstimate( float* estimate, float value )
{
	*estimate = *estimate > 0.0f ? *estimate + NB_ESTIMATE_WEIGHT * ( value - *estimate ) : value;
}

// The workers help once the calling thread alone would take this many times as long as a sleeping worker takes to start.
// While the workers start, the calling thread does the first items itself, but handing out the tasks and taking their
// results from the caches of other cores cost about as much again. On the VM the cells of a fracture went to the workers
// from 16 items with 67 us to start and were slower than alone, see "Kleine Einschläge ohne Worker" in the docs.
#define NB_WORKER_WAKE_FACTOR 3.0f

// The items from which the workers help with work whose items take itemTime milliseconds each on one thread, see
// NB_WORKER_WAKE_FACTOR and nbWorld::wakeEstimate. Never below minItems and never above maxItems, maxItems until the world
// measured both times. The result of the work must not depend on how many threads share it.
static int nbWorkerThreshold( const nbWorld* world, float itemTime, int minItems, int maxItems )
{
	if ( world->wakeEstimate <= 0.0f || itemTime <= 0.0f )
	{
		return maxItems;
	}

	float items = NB_WORKER_WAKE_FACTOR * world->wakeEstimate / itemTime;
	return items >= (float)maxItems ? maxItems : b3MaxInt( (int)ceilf( items ), minItems );
}

// Items from which the workers help with a fracture at most and at least, see nbWorkerThreshold. A grenade on a house at
// fragment scale 4 has 15 to 30 items. Where a worker takes long to start, as on the VM with 67 us, the calling thread alone
// is done with them sooner than with the workers it would have to wake, and their cells in the caches of other cores.
#define NB_PARALLEL_FRACTURE_ITEMS 32
#define NB_MIN_PARALLEL_FRACTURE_ITEMS 8

int nbFractureWorkerItems( const nbWorld* world )
{
	return nbWorkerThreshold( world, world->cellEstimate, NB_MIN_PARALLEL_FRACTURE_ITEMS, NB_PARALLEL_FRACTURE_ITEMS );
}

void nbStartFractureJobs( nbWorld* world, nbFractureRun* run, nbFractureJob* jobs, int jobCount, nbPrepareJobFn* prepare,
						  void* prepareContext, nbArena* arena, nbArena* workerArenas )
{
	*run = (nbFractureRun){ 0 };
	int prepareCount = prepare != NULL ? jobCount : 0;
	int itemCount = prepareCount;
	int siteCapacity = 0;
	for ( int i = 0; i < jobCount; ++i )
	{
		jobs[i].cells = nbArena_AllocArray( arena, nbCell, jobs[i].siteCount );
		itemCount += jobs[i].siteCount;
		siteCapacity = jobs[i].siteCount > siteCapacity ? jobs[i].siteCount : siteCapacity;
	}

	if ( itemCount == 0 )
	{
		return;
	}

	// Every cell is a work item. Workers grab the next item when they finish one, which balances
	// cheap cells at the impact against the larger cells further out.
	int* itemJobs = nbArena_AllocArray( arena, int, itemCount );
	int* itemCells = nbArena_AllocArray( arena, int, itemCount );
	int* prepared = nbArena_AllocArray( arena, int, jobCount + 1 );
	int item = prepareCount;
	for ( int i = 0; i < jobCount; ++i )
	{
		prepared[i] = 0;
		for ( int k = 0; k < jobs[i].siteCount; ++k )
		{
			itemJobs[item] = i;
			itemCells[item] = k;
			item += 1;
		}
	}

	// Waking threads costs more than a few cells, see nbFractureWorkerItems
	int taskCount = world->enqueueTask != NULL && itemCount >= nbFractureWorkerItems( world ) ? world->workerCount : 1;
	taskCount = taskCount < itemCount ? taskCount : itemCount;

	run->tasks = nbArena_AllocArray( arena, nbFractureTask, taskCount );
	for ( int i = 0; i < taskCount; ++i )
	{
		run->tasks[i] = (nbFractureTask){
			.jobs = jobs,
			.jobCount = jobCount,
			.itemJobs = itemJobs,
			.itemCells = itemCells,
			.itemCount = itemCount,
			.siteCapacity = siteCapacity,
			.prepare = prepare,
			.prepareContext = prepareContext,
			.prepared = prepared,
			.nextItem = &run->nextItem,
			.wakeTime = -1.0f,
			.arena = workerArenas + i,
		};
	}

	for ( int i = 1; i < taskCount; ++i )
	{
		run->tasks[i].enqueueTicks = b3GetTicks();
		run->userTasks[i] = world->enqueueTask( nbFractureTaskMain, run->tasks + i, world->userTaskContext, "nebenan fracture" );
	}
	run->taskCount = taskCount;
}

void nbFinishFractureJobs( nbWorld* world, nbFractureRun* run )
{
	if ( run->taskCount == 0 )
	{
		return;
	}

	uint64_t ticks = b3GetTicks();
	nbFractureTaskMain( run->tasks + 0 );
	run->callerTime = b3GetMilliseconds( ticks );
	run->callerItems = run->tasks[0].takenCount;
	for ( int i = 1; i < run->taskCount; ++i )
	{
		if ( run->userTasks[i] != NULL )
		{
			world->finishTask( run->userTasks[i], world->userTaskContext );
		}
	}

	for ( int i = 0; i < run->taskCount; ++i )
	{
		world->stats.hullFallbackCount += run->tasks[i].counters.hullFallbackCount;
	}
	run->taskCount = 0;
}

void nbRunFractureJobs( nbWorld* world, nbFractureJob* jobs, int jobCount, nbPrepareJobFn* prepare, void* prepareContext,
						nbCallerWorkFn* callerWork, void* callerContext )
{
	// The calling thread does its own work while the workers start, then it works on the cells as well
	NB_ZONE( cells_zone, "Cells" );
	uint64_t ticks = b3GetTicks();
	nbFractureRun run;
	nbStartFractureJobs( world, &run, jobs, jobCount, prepare, prepareContext, &world->arena, world->workerArenas );
	int taskCount = run.taskCount;
	if ( callerWork != NULL )
	{
		callerWork( callerContext );
	}
	nbFinishFractureJobs( world, &run );
	NB_ZONE_END( cells_zone );

	// The cells in the profile, those of the calling thread alone, and how long the workers took to start
	nbProfileSums* profile = &world->profile;
	float time = b3GetMilliseconds( ticks );
	profile->stages[nb_profileCells] += time;
	if ( taskCount <= 1 )
	{
		profile->stages[nb_profileCellsAlone] += time;
	}
	for ( int i = 1; i < taskCount; ++i )
	{
		float wake = run.tasks[i].wakeTime;
		if ( wake >= 0.0f )
		{
			profile->wakeTime += wake;
			profile->longestWake = b3MaxFloat( profile->longestWake, wake );
			profile->wakeCount += 1;
			nbEstimate( &world->wakeEstimate, wake );
		}
	}

	// An item on the calling thread, for the next fracture, see nbFractureWorkerItems
	if ( run.callerItems > 0 )
	{
		nbEstimate( &world->cellEstimate, run.callerTime / (float)run.callerItems );
	}
}

typedef struct nbParallelTask
{
	nbParallelFn* fn;
	void* context;
	int itemCount;
	int* nextItem;
} nbParallelTask;

static void nbParallelTaskMain( void* context )
{
	NB_ZONE( task_zone, "Nebenan Task" );
	nbParallelTask* task = context;
	for ( ;; )
	{
		int item = nbAtomicFetchAddInt( task->nextItem, 1 );
		if ( item >= task->itemCount )
		{
			break;
		}

		task->fn( task->context, item );
	}
	NB_ZONE_END( task_zone );
}

void nbParallelFor( nbWorld* world, int itemCount, int minItems, nbParallelFn* fn, void* context )
{
	int taskCount = world->enqueueTask != NULL && itemCount >= minItems ? world->workerCount : 1;
	taskCount = taskCount < itemCount ? taskCount : itemCount;

	int nextItem = 0;
	nbParallelTask task = { fn, context, itemCount, &nextItem };
	void* userTasks[NB_MAX_WORKERS];
	for ( int i = 1; i < taskCount; ++i )
	{
		userTasks[i] = world->enqueueTask( nbParallelTaskMain, &task, world->userTaskContext, "nebenan parallel" );
	}

	nbParallelTaskMain( &task );

	for ( int i = 1; i < taskCount; ++i )
	{
		if ( userTasks[i] != NULL )
		{
			world->finishTask( userTasks[i], world->userTaskContext );
		}
	}
}

void nbWorld_SetWorkerCount( nbWorldId worldId, int count )
{
	// The steps use the workers, so wait for them from every thread
	nbWorld* world = nbFindWorld( worldId );
	if ( world == NULL )
	{
		return;
	}

	nbWaitForSteps( world );
	nbWaitForCreations( world );
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

void nbWorld_SetDebrisRestTime( nbWorldId worldId, float seconds )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return;
	}

	world->def.debrisRestTime = b3ClampFloat( seconds, 0.0f, 1.0f );
}

// The steps per second of nbWorld_Advance, see nbWorldDef::stepRate
#define NB_MIN_STEP_RATE 10.0f
#define NB_MAX_STEP_RATE 240.0f
#define NB_DEFAULT_STEP_RATE 60.0f

// Substeps per second, see nbGetSubStepCount
#define NB_SUBSTEP_RATE 240.0f

static float nbClampStepRate( float stepRate )
{
	return stepRate > 0.0f ? b3ClampFloat( stepRate, NB_MIN_STEP_RATE, NB_MAX_STEP_RATE ) : NB_DEFAULT_STEP_RATE;
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
	world->def.debrisRestTime = b3ClampFloat( def->debrisRestTime, 0.0f, 1.0f );
	world->def.storeyCollapse = nbClampStoreyCollapse( def->storeyCollapse );
	world->def.topStoreyCollapse = nbClampStoreyCollapse( def->topStoreyCollapse );
	world->def.stepRate = nbClampStepRate( def->stepRate );
	world->stepScale = 1.0f;
	world->physicsWorld = def->physicsWorld;
	nbArena_Create( &world->arena, 256 * 1024 );
	nbStartWorkers( world, def->workerCount );

	return (nbWorldId){ (uint16_t)( index + 1 ), world->generation };
}

void nbDestroyWorld( nbWorldId worldId )
{
	// From every thread, not only the one that handed over the steps
	nbWorld* world = nbFindWorld( worldId );
	if ( world == NULL )
	{
		return;
	}

	nbWaitForSteps( world );
	nbDestroyStepThread( world->stepThread );
	world->stepThread = NULL;

	// The workers are done with the pieces of the program before a stream gives them back
	nbWaitForCreations( world );
	nbDestroyStreams( world );
	nbDestroyCreations( world );

	if ( b3World_IsValid( world->physicsWorld ) )
	{
		for ( int i = 0; i < world->chunks.count; ++i )
		{
			nbChunk* chunk = world->chunks.data + i;
			if ( chunk->shape != NULL && ( chunk->flags & nb_chunkStaticBody ) && b3Body_IsValid( chunk->bodyId ) )
			{
				b3DestroyBody( chunk->bodyId );
			}
		}

		// A shared static body stays with its destructible, even after all its chunks broke away
		for ( int i = 0; i < world->destructibles.count; ++i )
		{
			nbDestructible* destructible = world->destructibles.data + i;
			if ( destructible->isFree == false && B3_IS_NON_NULL( destructible->staticBody ) &&
				 b3Body_IsValid( destructible->staticBody ) )
			{
				b3DestroyBody( destructible->staticBody );
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
		nbChunk* chunk = world->chunks.data + i;
		if ( chunk->shape != NULL )
		{
			nbReleaseShape( world->destructibles.data + chunk->destructibleIndex, chunk->shape );
		}
	}

	nbArray_Free( world->chunks );
	nbArray_Free( world->bonds );
	nbArray_Free( world->bondMoments );
	nbArray_Free( world->actors );
	nbArray_Free( world->destructibles );
	nbArray_Free( world->freeChunks );
	nbArray_Free( world->freeBonds );
	nbArray_Free( world->freeActors );
	nbArray_Free( world->freeDestructibles );
	nbArray_Free( world->shapeToChunk );
	nbArray_Free( world->bodyToActor );
	nbArray_Free( world->debris );
	nbArray_Free( world->movingDebris );
	nbArray_Free( world->largeDebris );
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
	nbArray_Free( world->moves[0] );
	nbArray_Free( world->moves[1] );
	nbArray_Free( world->moveSlots );
	nbArray_Free( world->drawBodies );
	nbArray_Free( world->drawSlots );
	nbArray_Free( world->drawPoses );
	nbArray_Free( world->streams );
	nbArray_Free( world->movedCenters );
	nbArena_Destroy( &world->arena );
	nbStopWorkers( world );

	uint16_t generation = world->generation;
	memset( world, 0, sizeof( nbWorld ) );
	world->generation = generation + 1;
}

// Without waiting for the steps, they do not change it
bool nbWorld_IsValid( nbWorldId worldId )
{
	return nbFindWorld( worldId ) != NULL;
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
	int maxWoken = nbLimitForStep( world, NB_MAX_RUBBLE_HITS );
	for ( int i = 0; i < contactEvents.hitCount && woken < maxWoken; ++i )
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

	// Most pieces hold no rubble, and the loop waited most for that flag of each, from scattered memory. It prefetches
	// the flag eight pieces ahead.
	int first = nbNextMoving( world, 0 );
	int ahead = nbMovingAhead( world, first, 8 );
	int maxReleases = nbLimitForStep( world, NB_MAX_RELEASES );
	for ( int i = first; i < world->debris.count && rubbleCount - world->rubbleCount < maxReleases;
		  i = nbNextMoving( world, i + 1 ) )
	{
		if ( ahead < world->debris.count )
		{
			NB_PREFETCH( &world->actors.data[world->debris.data[ahead]].holdsRubble );
			ahead = nbNextMoving( world, ahead + 1 );
		}

		int actorIndex = world->debris.data[i];
		nbActor* actor = world->actors.data + actorIndex;
		if ( actor->holdsRubble == false )
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
			// The rubble that rested on it lies higher than its center of mass did, or froze on it. Past the limit of a
			// thaw it holds on, and the next update brings back the rest.
			b3Pos center = b3TransformWorldPoint( actor->holdPose, actor->localCenter );
			nbThawContext filter = {
				.skipActor = actor->holdSource,
				.skipGeneration = actor->holdSourceGeneration,
				.holder = actorIndex,
				.grace = actor->isBuildingPart || world->time - actor->budgetTime < NB_FREEZE_GRACE,
			};
			actor->holdsRubble =
				nbThawRubbleInBoxes( world, &actor->holdBounds, 1, up, b3Dot( up, b3ToVec3( center ) ), &filter ) > 0;
		}
	}
}

// A touching contact of a piece with the body on its other side. The looks at what carries a piece read its contacts in
// several passes, and each looked up the other side in Box3D again, see nbDescribeTouches.
typedef struct nbTouch
{
	b3ContactData data;

	// The piece is body A of the contact: its anchors are anchorA, and the normal points away from it
	bool isA;

	b3ShapeId otherShapeId;
	b3BodyId otherBodyId;
	b3BodyType otherType;

	// The actor of the other body, see nbFindActorFromBody, and the chunk of the other shape, see nbFindChunkFromShape
	int otherActor;
	int otherChunk;
} nbTouch;

// The touching contacts of a body as touches, in their order. Each lookup of the other side reads a table and then the
// record it points to, all from scattered memory: the ids from Box3D come first for all touches, the passes after them
// prefetch the entries of the tables, then the records, and a large piece has hundreds of touches to overlap.
static void nbDescribeTouches( const nbWorld* world, b3BodyId bodyId, const b3ContactData* contacts, int count, nbTouch* touches )
{
	for ( int c = 0; c < count; ++c )
	{
		nbTouch* touch = touches + c;
		touch->data = contacts[c];
		touch->isA = B3_ID_EQUALS( b3Shape_GetBody( contacts[c].shapeIdA ), bodyId );
		touch->otherShapeId = touch->isA ? contacts[c].shapeIdB : contacts[c].shapeIdA;
		touch->otherBodyId = b3Shape_GetBody( touch->otherShapeId );
		touch->otherType = b3Body_GetType( touch->otherBodyId );
		int bodyIndex = touch->otherBodyId.index1 - 1;
		if ( 0 <= bodyIndex && bodyIndex < world->bodyToActor.count )
		{
			NB_PREFETCH( world->bodyToActor.data + bodyIndex );
		}

		int shapeIndex = touch->otherShapeId.index1 - 1;
		if ( 0 <= shapeIndex && shapeIndex < world->shapeToChunk.count )
		{
			NB_PREFETCH( world->shapeToChunk.data + shapeIndex );
		}
	}

	for ( int c = 0; c < count; ++c )
	{
		const nbTouch* touch = touches + c;
		int bodyIndex = touch->otherBodyId.index1 - 1;
		if ( 0 <= bodyIndex && bodyIndex < world->bodyToActor.count && world->bodyToActor.data[bodyIndex] != NB_NULL_INDEX )
		{
			const nbActor* actor = world->actors.data + world->bodyToActor.data[bodyIndex];
			NB_PREFETCH( &actor->bodyId );
			NB_PREFETCH( &actor->isFree );
		}

		int shapeIndex = touch->otherShapeId.index1 - 1;
		if ( 0 <= shapeIndex && shapeIndex < world->shapeToChunk.count && world->shapeToChunk.data[shapeIndex] != NB_NULL_INDEX )
		{
			NB_PREFETCH( &world->chunks.data[world->shapeToChunk.data[shapeIndex]].shapeId );
		}
	}

	for ( int c = 0; c < count; ++c )
	{
		nbTouch* touch = touches + c;
		touch->otherActor = nbFindActorFromBody( world, touch->otherBodyId );
		touch->otherChunk = nbFindChunkFromShape( world, touch->otherShapeId );
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

// The answers of nbIsSupportedWithout for the rubble one large piece lies on, see nbFindSupport. A piece lies on few, the
// answers past the room are searched again and come out the same.
#define NB_CARRIER_ANSWERS 16

typedef struct nbCarrierAnswers
{
	int carriers[NB_CARRIER_ANSWERS];
	bool supported[NB_CARRIER_ANSWERS];
	int count;
} nbCarrierAnswers;

// nbIsSupportedWithout for the carriers of one piece, each carrier searched once
static bool nbIsCarrierSupported( const nbWorld* world, nbCarrierAnswers* answers, int carrierIndex, int actorIndex,
								  const nbRestingSet* resting, nbMoverLog* log )
{
	for ( int k = 0; k < answers->count; ++k )
	{
		if ( answers->carriers[k] == carrierIndex )
		{
			return answers->supported[k];
		}
	}

	bool supported = nbIsSupportedWithout( world, carrierIndex, actorIndex, resting, log );
	if ( answers->count < NB_CARRIER_ANSWERS )
	{
		answers->carriers[answers->count] = carrierIndex;
		answers->supported[answers->count] = supported;
		answers->count += 1;
	}
	return supported;
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
static b3BodyId nbFindSqueezer( const nbWorld* world, const nbActor* actor, const nbTouch* touches, int touchCount )
{
	for ( int c = 0; c < touchCount; ++c )
	{
		const nbTouch* touch = touches + c;
		float depth = 0.0f;
		for ( int m = 0; m < touch->data.manifoldCount; ++m )
		{
			const b3Manifold* manifold = touch->data.manifolds + m;
			for ( int p = 0; p < manifold->pointCount; ++p )
			{
				depth = b3MaxFloat( depth, -manifold->points[p].separation );
			}
		}

		if ( depth <= NB_SQUEEZE_DEPTH || touch->otherType != b3_dynamicBody )
		{
			continue;
		}

		int other = touch->otherActor;
		if ( other == NB_NULL_INDEX || world->actors.data[other].volume > actor->volume )
		{
			return touch->otherBodyId;
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
	bool fixed = false;
	for ( int k = 0; k < count; ++k )
	{
		actor->carriers[k] = carriers[k];
		int chunkIndex = carriers[k].chunkIndex;
		const nbActor* carrier =
			chunkIndex != NB_NULL_INDEX ? world->actors.data + world->chunks.data[chunkIndex].actorIndex : NULL;
		fixed = fixed || carrier == NULL || carrier->isStatic || carrier->isRubble;
	}

	// Moving pieces it lies on bring it back to life once they move away, otherwise it would hang in the air. A small
	// piece that also lies on something fixed needs none of them, but a large one counts them for its balance. A piece
	// of a pile that freezes with it lets go of that again.
	for ( int k = 0; k < count && ( fixed == false || actor->volume >= NB_CHECKED_VOLUME ); ++k )
	{
		int chunkIndex = carriers[k].chunkIndex;
		nbActor* carrier = chunkIndex != NB_NULL_INDEX ? world->actors.data + world->chunks.data[chunkIndex].actorIndex : NULL;
		if ( carrier != NULL && carrier->isStatic == false && carrier->isRubble == false && carrier->holdsRubble == false )
		{
			nbHoldRubble( world, carrier );
		}
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

	// Lies on something, fixed or moving, and is not only wedged in or about to touch something. For a piece of at least
	// NB_CHECKED_VOLUME rubble that holds only through it does not count.
	bool lying;

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
// carries nothing either, however it touches. Smaller pieces lie on whatever they touch. The room for the resting set has
// 1 << nbRestingBits( touchCount ) places. The search only reads the world, so the workers look at many pieces at once, see
// nbSettleDebris. The moving bodies the searches through rubble read go to the log if there is one.
static void nbFindSupport( const nbWorld* world, int actorIndex, const nbTouch* touches, int touchCount, b3Vec3 up,
						   const nbSupportFan* baseFan, int* resting, nbSupport* support, int* unsupported, int* unsupportedCount,
						   int unsupportedCapacity, nbMoverLog* log )
{
	const nbActor* actor = world->actors.data + actorIndex;
	bool large = actor->volume >= NB_CHECKED_VOLUME;
	b3Vec3 center = b3ToVec3( b3Body_GetWorldCenter( actor->bodyId ) );
	support->grounded = false;
	support->lying = false;
	support->carrierCount = 0;
	int firstUnsupported = *unsupportedCount;
	int carrying = 0;
	nbSupportFan fan = *baseFan;
	nbSupportFan fixedFan = *baseFan;
	nbCarrierAnswers answers;
	answers.count = 0;

	// Moving bodies that rest on a large piece carry nothing, in a set to look them up
	nbRestingSet restingSet = { resting, nbRestingBits( touchCount ), 0 };
	if ( large )
	{
		memset( resting, 0xff, sizeof( int ) << restingSet.bits );
	}

	for ( int c = 0; large && c < touchCount; ++c )
	{
		const nbTouch* touch = touches + c;
		if ( touch->otherActor != NB_NULL_INDEX && nbCarries( &touch->data, touch->isA == false, up ) )
		{
			nbAddResting( &restingSet, touch->otherActor );
		}
	}

	for ( int c = 0; c < touchCount; ++c )
	{
		// A small piece is done once it lies on something and knows enough of what
		if ( large == false && support->grounded && support->carrierCount == NB_MAX_CARRIERS )
		{
			break;
		}

		const nbTouch* touch = touches + c;
		bool isA = touch->isA;
		bool carries = nbCarries( &touch->data, isA, up );
		if ( carries == false && large == false )
		{
			continue;
		}

		int chunkIndex = touch->otherChunk;
		int otherIndex = chunkIndex != NB_NULL_INDEX ? world->chunks.data[chunkIndex].actorIndex : NB_NULL_INDEX;
		if ( touch->otherType == b3_staticBody )
		{
			if ( large && otherIndex != NB_NULL_INDEX && world->actors.data[otherIndex].isRubble &&
				 nbIsCarrierSupported( world, &answers, otherIndex, actorIndex, &restingSet, log ) == false )
			{
				// Listed once, the rubble that carries the piece first. A full list makes room for it, the rest comes back
				// to life later, the piece is still there.
				int listed = firstUnsupported;
				while ( listed < *unsupportedCount && unsupported[listed] != otherIndex )
				{
					listed += 1;
				}

				int front = firstUnsupported + carrying;
				if ( listed == *unsupportedCount && *unsupportedCount < unsupportedCapacity )
				{
					unsupported[( *unsupportedCount )++] = otherIndex;
				}
				else if ( listed == *unsupportedCount && carries && front < listed )
				{
					listed -= 1;
					unsupported[listed] = otherIndex;
				}

				if ( carries && front <= listed && listed < *unsupportedCount )
				{
					unsupported[listed] = unsupported[front];
					unsupported[front] = otherIndex;
					carrying += 1;
				}

				// It is still what the piece lies on, the search decides what holds
				b3Vec3 point;
				if ( carries && nbTouchPoint( &touch->data, isA, center, NB_TOUCH_GAP, &point ) )
				{
					nbAddCarrier( world, support->carriers, &support->carrierCount, chunkIndex, point );
				}
				continue;
			}

			support->grounded = support->grounded || carries;
			if ( large )
			{
				nbAddSupportPoints( &fixedFan, &touch->data, isA );
			}
		}
		else if ( otherIndex == NB_NULL_INDEX )
		{
			continue;
		}
		else if ( nbHoldsActor( &restingSet, otherIndex ) )
		{
			continue;
		}

		if ( large )
		{
			nbAddSupportPoints( &fan, &touch->data, isA );
		}

		b3Vec3 point;
		if ( carries && nbTouchPoint( &touch->data, isA, center, NB_TOUCH_GAP, &point ) )
		{
			nbAddCarrier( world, support->carriers, &support->carrierCount, chunkIndex, point );
			support->lying = true;
		}
	}

	// A piece that lies on nothing but is wedged in, or about to touch something, remembers what it is closest to, apart
	// from moving bodies that rest on it
	bool wedged = support->carrierCount == 0;
	for ( int c = 0; wedged && c < touchCount && support->carrierCount < NB_MAX_CARRIERS; ++c )
	{
		const nbTouch* touch = touches + c;
		int chunkIndex = touch->otherChunk;
		bool fixed = touch->otherType == b3_staticBody;
		b3Vec3 point;
		if ( ( fixed || ( chunkIndex != NB_NULL_INDEX &&
						  nbHoldsActor( &restingSet, world->chunks.data[chunkIndex].actorIndex ) == false ) ) &&
			 nbTouchPoint( &touch->data, touch->isA, center, FLT_MAX, &point ) )
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
static bool nbCarriesWithout( const nbWorld* world, const nbCarrier* carriers, int count, int actorIndex )
{
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
			   ( carrier->isRubble && nbIsSupportedWithout( world, carrierIndex, actorIndex, NULL, NULL ) ) ) )
		{
			return true;
		}
	}
	return false;
}

// Moving debris per block of the checks in nbSettleDebris. With workers, every thread keeps a segment of the blocks from
// update to update, see nbClaimSettleBlock, and takes the blocks with a piece of at least NB_CHECKED_VOLUME first: Once
// quiet, such a piece takes the look at what carries it 20 times as long as a small one on the VM, at times milliseconds,
// and at the end the other threads waited for it.
#define NB_SETTLE_BLOCK 64

// Blocks the segments of the checks in nbSettleDebris hold at most, the front and the end of one share an int. With more
// the threads take the blocks in order.
#define NB_MAX_SEGMENT_BLOCKS 0x7fff

// Every this many updates time how the threads spend the checks of nbSettleDebris, see nbProfile::settleWork
#define NB_SETTLE_SAMPLE 8

// Moving debris from which the workers help with the checks of nbSettleDebris at most and at least, see nbWorkerThreshold.
// A piece takes the calling thread about 0.2 us on the VM, where a worker starts after 67 us, so with less than 512 it is
// done sooner alone.
#define NB_PARALLEL_SETTLE 512
#define NB_MIN_PARALLEL_SETTLE 128

int nbSettleWorkerPieces( const nbWorld* world )
{
	return nbWorkerThreshold( world, world->settleEstimate, NB_MIN_PARALLEL_SETTLE, NB_PARALLEL_SETTLE );
}

// A quiet piece and what carries it, as the checks of nbSettleDebris found it
typedef struct nbQuietPiece
{
	int actorIndex;

	// Lies on something that holds, see nbSupport::grounded and nbSupport::braced
	bool held;

	// A large piece that would not stay balanced, it does not freeze
	bool blocked;

	// A larger moving body that presses into it, see NB_SQUEEZE_DEPTH
	b3BodyId squeezer;

	nbCarrier carriers[NB_MAX_CARRIERS];
	int carrierCount;

	// The moving pieces it lies on, in the order of its contacts. Those that are quiet too carry it in the search.
	int* below;
	int belowCount;

	// The rubble a large piece found to rest on itself, see nbFindSupport
	int* unsupported;
	int unsupportedCount;
} nbQuietPiece;

// The quiet pieces of a block in the order of the debris
typedef struct nbSettleBlock
{
	nbQuietPiece* pieces;
	int count;
} nbSettleBlock;

// The blocks of a thread in the checks of nbSettleDebris, the next one from the front in the high half and the end in the
// low half, on a cache line of its own
typedef struct nbSettleSegment
{
	_Alignas( 64 ) int range;
} nbSettleSegment;

typedef struct nbSettleTask
{
	// On cache lines of its own, the threads write to their tasks while they check
	_Alignas( 64 ) nbWorld* world;
	nbArena* arena;

	// The moving debris as actors in the order of the debris, its blocks, and the order the threads take them in. With
	// workers a segment of the order for every task, else the order one after the other.
	const int* moving;
	int movingCount;
	nbSettleBlock* blocks;
	const int* blockOrder;
	int blockCount;
	nbSettleSegment* segments;
	int segmentCount;
	int* nextBlock;

	// The block this task checked last, the loop prefetches into the next one
	int lastBlock;

	const nbSupportFan* fan;
	b3Vec3 up;
	float timeStep;

	// When the task went to a worker, and how many milliseconds later it started there, negative for the task of the
	// calling thread. And the moving debris it checked, see nbProfile::settleWorkerShare.
	uint64_t enqueueTicks;
	float wakeTime;
	int checkCount;

	// In a timed update, see NB_SETTLE_SAMPLE, the milliseconds from the start of the checks to the start and the end of
	// the task, in its blocks, of that on the support of the quiet pieces, and in its longest block. Then its blocks and
	// quiet pieces.
	bool timed;
	uint64_t callTicks;
	float startTime;
	float endTime;
	float workTime;
	float supportTime;
	float longestBlock;
	int timedBlockCount;
	int quietCount;
} nbSettleTask;

// What carries a quiet piece, from its touching contacts and the room for all its contacts. It only reads the world and
// writes the record.
static void nbInspectQuietPiece( const nbSettleTask* task, nbQuietPiece* piece, int actorIndex, int capacity,
								 const nbTouch* touches, int touchCount )
{
	const nbWorld* world = task->world;
	const nbActor* actor = world->actors.data + actorIndex;
	piece->actorIndex = actorIndex;
	piece->held = false;
	piece->blocked = false;
	piece->squeezer = b3_nullBodyId;
	piece->carrierCount = 0;
	piece->below = NULL;
	piece->belowCount = 0;
	piece->unsupported = NULL;
	piece->unsupportedCount = 0;
	if ( capacity == 0 )
	{
		return;
	}

	int places = 1 << nbRestingBits( touchCount );
	int* room = nbArena_AllocArray( task->arena, int, places + touchCount );
	int unsupported[NB_MAX_RELEASES];
	int unsupportedCount = 0;
	nbSupport support;
	nbFindSupport( world, actorIndex, touches, touchCount, task->up, task->fan, room, &support, unsupported, &unsupportedCount,
				   NB_MAX_RELEASES, NULL );
	piece->carrierCount = support.carrierCount;
	for ( int k = 0; k < support.carrierCount; ++k )
	{
		piece->carriers[k] = support.carriers[k];
	}

	// A large piece that would not stay balanced does not freeze. The rubble that nothing else holds comes back to life
	// unless the piece freezes after all, see nbSettleDebris, the piece tips or falls without it.
	bool large = actor->volume >= NB_CHECKED_VOLUME;
	if ( support.balanced == false )
	{
		piece->blocked = true;
	}

	if ( unsupportedCount > 0 )
	{
		piece->unsupported = nbArena_AllocArray( task->arena, int, unsupportedCount );
		memcpy( piece->unsupported, unsupported, sizeof( int ) * (size_t)unsupportedCount );
		piece->unsupportedCount = unsupportedCount;
	}

	piece->squeezer = nbFindSqueezer( world, actor, touches, touchCount );

	// The moving pieces that carry this one, the room after the resting set of nbFindSupport
	piece->below = room + places;
	for ( int c = 0; c < touchCount; ++c )
	{
		const nbTouch* touch = touches + c;
		if ( touch->otherType != b3_dynamicBody || nbCarries( &touch->data, touch->isA, task->up ) == false )
		{
			continue;
		}

		if ( touch->otherActor != NB_NULL_INDEX )
		{
			piece->below[piece->belowCount++] = touch->otherActor;
		}
	}

	// A large piece must be held by what holds without it, balanced over it or wedged in
	piece->held = large ? support.braced : support.grounded;
}

// The piece up to the pose it holds rubble at, see nbSettleDebris
static void nbPrefetchPiece( const nbWorld* world, int actorIndex )
{
	const char* piece = (const char*)( world->actors.data + actorIndex );
	const char* poseEnd = piece + offsetof( nbActor, holdBounds ) - 1;
	for ( const char* line = piece; line < poseEnd; line += 64 )
	{
		NB_PREFETCH( line );
	}
	NB_PREFETCH( poseEnd );

	// The rest on the lines at the end, see nbActor::restPose
	NB_PREFETCH( piece + offsetof( nbActor, restPose ) );
	NB_PREFETCH( piece + offsetof( nbActor, checkedAsleep ) );
}

// The quiet pieces of a block of the moving debris and what carries them. A piece changes only its own rest, and the search
// for its support only reads, so the blocks run on any thread in any order.
static void nbCheckSettleBlock( nbSettleTask* task, int blockIndex )
{
	nbWorld* world = task->world;
	const int* moving = task->moving;
	int count = task->movingCount;
	int begin = NB_SETTLE_BLOCK * blockIndex;
	int end = b3MinInt( begin + NB_SETTLE_BLOCK, count );
	task->checkCount += end - begin;

	// The loop reads each piece, and the record, the transform and the velocities of its body, all from scattered memory,
	// and it waited for them most. Three cursors run ahead of it, on into the next block, which the same thread takes when
	// it is alone. Eight pieces ahead it prefetches the piece, four ahead the record of the body, which needs the piece, and
	// two ahead the motion of the body, which needs the record, see b3Body_Prefetch. A thread that comes from another block
	// first prefetches the pieces it would have.
	if ( blockIndex != task->lastBlock + 1 )
	{
		for ( int k = begin; k < b3MinInt( begin + 8, count ); ++k )
		{
			nbPrefetchPiece( world, moving[k] );
		}
	}
	task->lastBlock = blockIndex;

	int quiet[NB_SETTLE_BLOCK];
	int quietCount = 0;
	for ( int k = begin; k < end; ++k )
	{
		if ( k + 8 < count )
		{
			nbPrefetchPiece( world, moving[k + 8] );
		}

#if defined( B3_HAS_BODY_PREFETCH )
		if ( k + 4 < count )
		{
			b3Body_Prefetch( world->actors.data[moving[k + 4]].bodyId );
		}

		if ( k + 2 < count )
		{
			b3Body_PrefetchMotion( world->actors.data[moving[k + 2]].bodyId );
		}
#endif

		if ( nbIsQuiet( world, world->actors.data + moving[k], task->timeStep ) )
		{
			quiet[quietCount++] = moving[k];
		}
	}

	nbSettleBlock* block = task->blocks + blockIndex;
	block->count = quietCount;
	block->pieces = quietCount > 0 ? nbArena_AllocArray( task->arena, nbQuietPiece, quietCount ) : NULL;
	uint64_t supportTicks = task->timed ? b3GetTicks() : 0;

	// The touching contacts of the quiet pieces. Most entries of their lists in Box3D are contacts that do not touch, each in
	// another place in memory, and walking a list cost most of the look at a small piece. Box3D walks the lists of the
	// block side by side, so the waits overlap.
	b3BodyId bodyIds[NB_SETTLE_BLOCK];
	int capacities[NB_SETTLE_BLOCK];
	int offsets[NB_SETTLE_BLOCK];
	int counts[NB_SETTLE_BLOCK];
	int room = 0;
	for ( int k = 0; k < quietCount; ++k )
	{
		bodyIds[k] = world->actors.data[quiet[k]].bodyId;
		capacities[k] = b3Body_GetContactCapacity( bodyIds[k] );
		offsets[k] = room;
		room += capacities[k];
	}

	b3ContactData* contacts = nbArena_AllocArray( task->arena, b3ContactData, room );
#if defined( B3_HAS_CONTACT_DATA_OF_BODIES )
	_Static_assert( NB_SETTLE_BLOCK <= B3_CONTACT_DATA_BODIES, "Box3D walks the lists of a block at once" );
	b3Body_GetContactDataOfBodies( bodyIds, quietCount, contacts, offsets, counts );
#else
	for ( int k = 0; k < quietCount; ++k )
	{
		counts[k] = b3Body_GetContactData( bodyIds[k], contacts + offsets[k], capacities[k] );
	}
#endif

	for ( int k = 0; k < quietCount; ++k )
	{
		nbTouch* touches = nbArena_AllocArray( task->arena, nbTouch, counts[k] );
		nbDescribeTouches( world, bodyIds[k], contacts + offsets[k], counts[k], touches );
		nbInspectQuietPiece( task, block->pieces + k, quiet[k], capacities[k], touches, counts[k] );
	}

	if ( task->timed )
	{
		task->supportTime += b3GetMilliseconds( supportTicks );
		task->quietCount += quietCount;
	}
}

// The slot of the calling thread in the checks of nbSettleDebris, the same from update to update. NB_MAX_SETTLE_THREADS
// for a thread beyond the table.
static int nbGetSettleSlot( nbWorld* world )
{
	int token = nbGetThreadToken();
	int count = nbAtomicLoadInt( &world->settleThreadCount );
	for ( int slot = 0; slot < b3MinInt( count, NB_MAX_SETTLE_THREADS ); ++slot )
	{
		if ( nbAtomicLoadInt( world->settleThreads + slot ) == token )
		{
			return slot;
		}
	}

	if ( count >= NB_MAX_SETTLE_THREADS )
	{
		return NB_MAX_SETTLE_THREADS;
	}

	int slot = nbAtomicFetchAddInt( &world->settleThreadCount, 1 );
	if ( slot >= NB_MAX_SETTLE_THREADS )
	{
		return NB_MAX_SETTLE_THREADS;
	}

	nbAtomicStoreInt( world->settleThreads + slot, token );
	return slot;
}

// The next place in the order of the blocks of the checks in nbSettleDebris for the thread in a slot, -1 once all are
// taken. A thread takes the blocks of its own segment from the front, so it checks about the pieces of the last update,
// which are still in its caches, the large ones first. Then it helps with the segment that has the most blocks left, from
// the back, where the small ones are.
static int nbClaimSettleBlock( nbSettleSegment* segments, int segmentCount, int slot )
{
	if ( slot < segmentCount )
	{
		int* range = &segments[slot].range;
		for ( ;; )
		{
			int value = nbAtomicLoadInt( range );
			int front = value >> 16;
			int end = value & 0xffff;
			if ( front >= end )
			{
				break;
			}

			if ( nbAtomicCompareExchangeInt( range, value, ( ( front + 1 ) << 16 ) | end ) )
			{
				return front;
			}
		}
	}

	for ( ;; )
	{
		int best = -1;
		int bestLeft = 0;
		int bestValue = 0;
		for ( int s = 0; s < segmentCount; ++s )
		{
			int value = nbAtomicLoadInt( &segments[s].range );
			int left = ( value & 0xffff ) - ( value >> 16 );
			if ( left > bestLeft )
			{
				best = s;
				bestLeft = left;
				bestValue = value;
			}
		}

		if ( best < 0 )
		{
			return -1;
		}

		int end = ( bestValue & 0xffff ) - 1;
		if ( nbAtomicCompareExchangeInt( &segments[best].range, bestValue, ( bestValue & ~0xffff ) | end ) )
		{
			return end;
		}
	}
}

static void nbSettleTaskMain( void* context )
{
	nbSettleTask* task = context;
	if ( task->enqueueTicks != 0 )
	{
		task->wakeTime = b3GetMilliseconds( task->enqueueTicks );
	}

	if ( task->timed )
	{
		task->startTime = b3GetMilliseconds( task->callTicks );
	}

	int slot = task->segments != NULL ? nbGetSettleSlot( task->world ) : 0;
	for ( ;; )
	{
		int claim;
		if ( task->segments != NULL )
		{
			claim = nbClaimSettleBlock( task->segments, task->segmentCount, slot );
			if ( claim < 0 )
			{
				break;
			}
		}
		else
		{
			claim = nbAtomicFetchAddInt( task->nextBlock, 1 );
			if ( claim >= task->blockCount )
			{
				break;
			}
		}

		int blockIndex = task->blockOrder[claim];

		if ( task->timed == false )
		{
			nbCheckSettleBlock( task, blockIndex );
			continue;
		}

		uint64_t blockTicks = b3GetTicks();
		nbCheckSettleBlock( task, blockIndex );
		float blockTime = b3GetMilliseconds( blockTicks );
		task->workTime += blockTime;
		task->longestBlock = b3MaxFloat( task->longestBlock, blockTime );
		task->timedBlockCount += 1;
	}

	if ( task->timed )
	{
		task->endTime = b3GetMilliseconds( task->callTicks );
	}
}

// Quiet debris freezes into rubble when something that does not move carries it: the ground, a structure, rubble, or
// quiet debris that is carried itself. The carrying contacts form a graph from the ground up, and a search from the
// grounded pieces freezes whole piles at once, from the bottom up. Nothing freezes while it lies on a moving piece, and
// rubble only carries what does not carry it in turn, see nbFindSupport. Each piece remembers what it froze on. The
// workers check the pieces, then the calling thread puts their answers together in the order of the debris and freezes,
// so the result does not depend on the workers.
static void nbSettleDebris( nbWorld* world, b3Vec3 up, float timeStep )
{
	if ( world->def.enableRubble == false || world->debris.count == 0 )
	{
		return;
	}

	// The parts of the checks in the profile, see nb_profileGatherMoving
	uint64_t lap = b3GetTicks();
	nbBeginOperation( world );
	world->settleStamp += 1;
	uint32_t stamp = world->settleStamp;

	// The moving debris in the order of the debris, in blocks for the workers, and which blocks hold a large piece
	int* moving = nbArena_AllocArray( &world->arena, int, world->debris.count );
	bool* largeBlocks = nbArena_AllocArray( &world->arena, bool, world->debris.count / NB_SETTLE_BLOCK + 1 );
	int movingCount = 0;
	for ( int w = 0; w < world->movingDebris.count; ++w )
	{
		uint64_t largeWord = world->largeDebris.data[w];
		for ( uint64_t word = world->movingDebris.data[w]; word != 0; word &= word - 1 )
		{
			int bit = nbCTZ64( word );
			if ( movingCount % NB_SETTLE_BLOCK == 0 )
			{
				largeBlocks[movingCount / NB_SETTLE_BLOCK] = false;
			}
			largeBlocks[movingCount / NB_SETTLE_BLOCK] |= ( ( largeWord >> bit ) & 1 ) != 0;
			moving[movingCount++] = world->debris.data[64 * w + bit];
		}
	}

	if ( movingCount == 0 )
	{
		nbProfileLap( world, nb_profileGatherMoving, &lap );
		return;
	}

	nbSupportFan fan = nbMakeSupportFan( up );
	int blockCount = ( movingCount + NB_SETTLE_BLOCK - 1 ) / NB_SETTLE_BLOCK;
	nbSettleBlock* blocks = nbArena_AllocArray( &world->arena, nbSettleBlock, blockCount );
	int taskCount = world->enqueueTask != NULL && movingCount >= nbSettleWorkerPieces( world ) ? world->workerCount : 1;
	taskCount = b3MinInt( taskCount, blockCount );

	// With workers a segment of the blocks for every task, the first for the first slot, see nbClaimSettleBlock, and in
	// every segment the blocks with a large piece first, see NB_SETTLE_BLOCK. A thread alone takes them in order, so its
	// loads run on into the next block.
	bool segmented = taskCount > 1 && blockCount <= NB_MAX_SEGMENT_BLOCKS;
	int segmentCount = segmented ? taskCount : 1;
	int* blockOrder = nbArena_AllocArray( &world->arena, int, blockCount );
	nbSettleSegment segments[NB_MAX_WORKERS];
	for ( int s = 0; s < segmentCount; ++s )
	{
		int front = s * blockCount / segmentCount;
		int end = ( s + 1 ) * blockCount / segmentCount;
		int ordered = front;
		for ( int b = front; b < end; ++b )
		{
			if ( taskCount == 1 || largeBlocks[b] )
			{
				blockOrder[ordered++] = b;
			}
		}
		for ( int b = front; b < end && taskCount > 1; ++b )
		{
			if ( largeBlocks[b] == false )
			{
				blockOrder[ordered++] = b;
			}
		}
		segments[s].range = ( front << 16 ) | end;
	}
	int nextBlock = 0;
	nbSettleTask tasks[NB_MAX_WORKERS];
	void* userTasks[NB_MAX_WORKERS];
	bool timed = stamp % NB_SETTLE_SAMPLE == 0;
	nbProfileLap( world, nb_profileGatherMoving, &lap );
	for ( int i = 0; i < taskCount; ++i )
	{
		tasks[i] = ( nbSettleTask ){
			.world = world,
			.arena = world->workerArenas + i,
			.moving = moving,
			.movingCount = movingCount,
			.blocks = blocks,
			.blockOrder = blockOrder,
			.blockCount = blockCount,
			.segments = segmented ? segments : NULL,
			.segmentCount = taskCount,
			.nextBlock = &nextBlock,
			.lastBlock = -2,
			.fan = &fan,
			.up = up,
			.timeStep = timeStep,
			.wakeTime = -1.0f,
			.timed = timed,
			.callTicks = lap,
		};
	}

	for ( int i = 1; i < taskCount; ++i )
	{
		tasks[i].enqueueTicks = b3GetTicks();
		userTasks[i] = world->enqueueTask( nbSettleTaskMain, tasks + i, world->userTaskContext, "nebenan settle" );
	}

	uint64_t callerTicks = b3GetTicks();
	nbSettleTaskMain( tasks );
	float callerTime = b3GetMilliseconds( callerTicks );

	for ( int i = 1; i < taskCount; ++i )
	{
		if ( userTasks[i] != NULL )
		{
			world->finishTask( userTasks[i], world->userTaskContext );
		}
	}

	// How the tasks spent the checks: before their first block, in their blocks, between them and after their last block,
	// which adds up to the time of the checks for every task
	float checkTime = b3GetMillisecondsAndReset( &lap );
	world->profile.stages[nb_profileCheckMoving] += checkTime;
	if ( timed )
	{
		nbProfileSums* profile = &world->profile;
		float longestBlock = 0.0f;
		for ( int i = 0; i < taskCount; ++i )
		{
			const nbSettleTask* task = tasks + i;
			profile->settleWork += task->workTime;
			profile->settleSupportWork += task->supportTime;
			profile->settleLate += task->startTime;
			profile->settleClaim += task->endTime - task->startTime - task->workTime;
			profile->settleDone += checkTime - task->endTime;
			profile->settleMissedCount += task->timedBlockCount == 0 ? 1.0 : 0.0;
			profile->settleQuietCount += task->quietCount;
			longestBlock = b3MaxFloat( longestBlock, task->longestBlock );
		}
		profile->settleSampleCount += 1;
		profile->settleCall += checkTime;
		profile->settleLongestBlock += longestBlock;
		profile->settleTaskCount += taskCount;
		profile->settleBlockCount += blockCount;
		profile->settleMovingCount += movingCount;
		profile->settleCallerCount += tasks[0].checkCount;
	}

	// A piece on the calling thread and how long the workers took to start, for the next update, see nbSettleWorkerPieces
	if ( tasks[0].checkCount > 0 )
	{
		nbEstimate( &world->settleEstimate, callerTime / (float)tasks[0].checkCount );
	}

	// How much the workers took and how long they took to start
	if ( taskCount > 1 )
	{
		nbProfileSums* profile = &world->profile;
		int workerChecks = 0;
		for ( int i = 1; i < taskCount; ++i )
		{
			workerChecks += tasks[i].checkCount;
			if ( tasks[i].wakeTime >= 0.0f )
			{
				profile->settleWakeTime += tasks[i].wakeTime;
				profile->settleWakeCount += 1;
				nbEstimate( &world->wakeEstimate, tasks[i].wakeTime );
			}
		}
		profile->settleSplitCount += 1;
		profile->settleWorkerShare += (double)workerChecks / (double)movingCount;
	}

	// The quiet pieces get their slots in the order of the debris
	int quietCount = 0;
	for ( int b = 0; b < blockCount; ++b )
	{
		quietCount += blocks[b].count;
	}

	if ( quietCount == 0 )
	{
		nbProfileLap( world, nb_profileSearchSupport, &lap );
		return;
	}

	int* quiet = nbArena_AllocArray( &world->arena, int, quietCount );
	const nbQuietPiece** pieces = nbArena_AllocArray( &world->arena, const nbQuietPiece*, quietCount );
	int slot = 0;
	for ( int b = 0; b < blockCount; ++b )
	{
		for ( int k = 0; k < blocks[b].count; ++k )
		{
			const nbQuietPiece* piece = blocks[b].pieces + k;
			nbActor* actor = world->actors.data + piece->actorIndex;
			actor->settleSlot = slot;
			actor->settleStamp = stamp;
			quiet[slot] = piece->actorIndex;
			pieces[slot] = piece;
			slot += 1;
		}
	}

	// Carrying contacts between quiet pieces, and the rubble found to rest on the very piece it would carry
	int* queue = nbArena_AllocArray( &world->arena, int, quietCount );
	bool* stable = nbArena_AllocArray( &world->arena, bool, quietCount );
	bool* blocked = nbArena_AllocArray( &world->arena, bool, quietCount );
	int* edgeCounts = nbArena_AllocArray( &world->arena, int, quietCount + 1 );
	for ( slot = 0; slot < quietCount; ++slot )
	{
		stable[slot] = false;
		blocked[slot] = pieces[slot]->blocked;
		edgeCounts[slot + 1] = 0;
	}
	edgeCounts[0] = 0;
	nbIntArray* edges = &world->scratchList;
	edges->count = 0;
	for ( slot = 0; slot < quietCount; ++slot )
	{
		const nbQuietPiece* piece = pieces[slot];

		// An edge from each quiet piece that carries this one
		for ( int e = 0; e < piece->belowCount; ++e )
		{
			const nbActor* other = world->actors.data + piece->below[e];
			if ( other->settleStamp == stamp )
			{
				nbArray_Push( *edges, other->settleSlot );
				nbArray_Push( *edges, slot );
				edgeCounts[other->settleSlot + 1] += 1;
			}
		}
	}

	// The pieces each quiet piece carries, grouped by carrier
	for ( slot = 0; slot < quietCount; ++slot )
	{
		edgeCounts[slot + 1] += edgeCounts[slot];
	}
	int edgeCount = edges->count / 2;
	int* carried = nbArena_AllocArray( &world->arena, int, edgeCount + 1 );
	int* cursor = nbArena_AllocArray( &world->arena, int, quietCount );
	for ( slot = 0; slot < quietCount; ++slot )
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
		for ( slot = 0; slot < quietCount; ++slot )
		{
			stable[slot] = pieces[slot]->held && blocked[slot] == false;
			if ( stable[slot] )
			{
				queue[queueCount++] = slot;
			}
		}

		for ( int head = 0; head < queueCount; ++head )
		{
			const nbQuietPiece* piece = pieces[queue[head]];
			for ( int e = edgeCounts[queue[head]]; e < edgeCounts[queue[head] + 1]; ++e )
			{
				int above = carried[e];
				if ( stable[above] == false && blocked[above] == false &&
					 ( world->actors.data[quiet[above]].volume < NB_CHECKED_VOLUME ||
					   nbCarriesWithout( world, piece->carriers, piece->carrierCount, quiet[above] ) ) )
				{
					stable[above] = true;
					queue[queueCount++] = above;
				}
			}
		}

		again = false;
		for ( int k = 0; k < queueCount; ++k )
		{
			slot = queue[k];
			if ( B3_IS_NULL( pieces[slot]->squeezer ) )
			{
				continue;
			}

			int other = nbFindActorFromBody( world, pieces[slot]->squeezer );
			const nbActor* squeezer = other != NB_NULL_INDEX ? world->actors.data + other : NULL;
			if ( squeezer == NULL || squeezer->settleStamp != stamp || stable[squeezer->settleSlot] == false )
			{
				blocked[slot] = true;
				again = true;
			}
		}
	}
	nbProfileLap( world, nb_profileSearchSupport, &lap );

#if defined( B3_HAS_SHAPE_BATCH )
	// Box3D sorts every piece that freezes or comes back to life into its tree with a search from the root. The pieces of a
	// pile go in together with one search, see b3World_BeginShapeBatch. Nothing queries the trees before they are in.
	b3World_BeginShapeBatch( world->physicsWorld );
#endif

	for ( int k = 0; k < queueCount; ++k )
	{
		slot = queue[k];
		nbFreezeOnCarriers( world, quiet[slot], pieces[slot]->carriers, pieces[slot]->carrierCount );
	}

	// A large piece that did not freeze lets go of the rubble that only it holds. Frozen, that rubble would hold it up in
	// the air while it holds the rubble: rubble that froze on the piece while it was rubble itself, wedged under one of
	// its edges or jammed in its openings. The piece wakes up to tip or fall without it, and the rubble gets the grace of
	// new debris, see NB_FREEZE_GRACE, the budget would freeze it again where it lies. At most NB_MAX_RELEASES a step
	// come back to life, a piece with more waits for the next look.
	int maxReleases = nbLimitForStep( world, NB_MAX_RELEASES );
	int released = 0;
	bool* waiting = nbArena_AllocArray( &world->arena, bool, quietCount );
	for ( slot = 0; slot < quietCount; ++slot )
	{
		const nbQuietPiece* piece = pieces[slot];
		nbActor* actor = world->actors.data + quiet[slot];
		int first = released;
		waiting[slot] = false;
		for ( int k = 0; actor->isRubble == false && k < piece->unsupportedCount; ++k )
		{
			int rubbleIndex = piece->unsupported[k];
			if ( world->actors.data[rubbleIndex].isRubble == false )
			{
				continue;
			}

			if ( released == maxReleases )
			{
				waiting[slot] = true;
				break;
			}

			nbThawActor( world, rubbleIndex );
			world->actors.data[rubbleIndex].budgetTime = world->time;
			released += 1;
		}

		if ( released > first )
		{
			b3Body_SetAwake( actor->bodyId, true );
		}
	}

	// What sleeps and could not freeze waits for Box3D to wake it. Its island sleeps as a whole, a piece that touches it
	// wakes all of it.
	for ( slot = 0; slot < quietCount; ++slot )
	{
		nbActor* actor = world->actors.data + quiet[slot];
		actor->checkedAsleep = actor->isRubble == false && waiting[slot] == false && b3Body_IsAwake( actor->bodyId ) == false;
	}

#if defined( B3_HAS_SHAPE_BATCH )
	nbEndTypeBatch( world );
#endif
	nbProfileLap( world, nb_profileFreezeRubble, &lap );
}

// Whether one of the contacts of a body is with a static body
static bool nbTouchesStatic( const nbTouch* touches, int touchCount )
{
	for ( int c = 0; c < touchCount; ++c )
	{
		if ( touches[c].otherType == b3_staticBody )
		{
			return true;
		}
	}
	return false;
}

typedef struct nbDebrisRank
{
	float speed;
	int actorIndex;

	// Its look, see nbBudgetLook
	int look;
} nbDebrisRank;

// The key of a ranked piece: slowest first, then by index so the order is total and the same everywhere. A speed is
// never negative, so its bits sort like the number.
static uint64_t nbRankKey( const nbDebrisRank* rank )
{
	uint32_t bits;
	memcpy( &bits, &rank->speed, sizeof( bits ) );
	return ( (uint64_t)bits << 32 ) | (uint32_t)rank->actorIndex;
}

// Sort the ranking by its keys, a byte at a time from the lowest, with the counts of all bytes from one pass. The budget
// ranks hundreds to thousands of pieces, for which qsort took up to three times as long.
static void nbSortRanks( nbArena* arena, nbDebrisRank* ranks, int count )
{
	uint64_t* keys = nbArena_AllocArray( arena, uint64_t, 2 * count );
	int* order = nbArena_AllocArray( arena, int, 2 * count );
	int counts[8][256];
	memset( counts, 0, sizeof( counts ) );
	for ( int i = 0; i < count; ++i )
	{
		uint64_t key = nbRankKey( ranks + i );
		keys[i] = key;
		order[i] = i;
		for ( int b = 0; b < 8; ++b )
		{
			counts[b][( key >> ( 8 * b ) ) & 0xff] += 1;
		}
	}

	uint64_t* fromKeys = keys;
	uint64_t* toKeys = keys + count;
	int* fromOrder = order;
	int* toOrder = order + count;
	for ( int b = 0; b < 8; ++b )
	{
		// A byte that all keys share leaves the order as it is
		int shift = 8 * b;
		if ( counts[b][( fromKeys[0] >> shift ) & 0xff] == count )
		{
			continue;
		}

		int starts[256];
		int start = 0;
		for ( int d = 0; d < 256; ++d )
		{
			starts[d] = start;
			start += counts[b][d];
		}

		for ( int i = 0; i < count; ++i )
		{
			int slot = starts[( fromKeys[i] >> shift ) & 0xff]++;
			toKeys[slot] = fromKeys[i];
			toOrder[slot] = fromOrder[i];
		}

		uint64_t* swapKeys = fromKeys;
		fromKeys = toKeys;
		toKeys = swapKeys;
		int* swapOrder = fromOrder;
		fromOrder = toOrder;
		toOrder = swapOrder;
	}

	nbDebrisRank* sorted = nbArena_AllocArray( arena, nbDebrisRank, count );
	for ( int i = 0; i < count; ++i )
	{
		sorted[i] = ranks[fromOrder[i]];
	}
	memcpy( ranks, sorted, sizeof( nbDebrisRank ) * (size_t)count );
}

// Debris the budget freezes per update at most. After a burst the next updates take the rest, each with a ranking of
// its own, so one update does not pay for all of it.
#define NB_MAX_BUDGET_FREEZES 256

// What the budget finds for a piece of debris before anything froze in its update. Freezing a piece destroys its contacts
// and makes it rubble, nothing else changes until the budget is done: rubble and structures stay as they are, and only
// the moving pieces the look read can change what it found. The workers look at the ranked pieces at once, then the
// calling thread freezes in the order of the ranking and looks again where one of those froze before, so it freezes the
// same pieces as it would alone.
typedef struct nbBudgetLook
{
	nbSupport support;

	// Past the grace and touching something, with its speed, see nbEnforceDebrisBudget
	bool ranked;
	float speed;

	bool squeezed;
	bool touchesStatic;

	// Not looked at, or it read more than it has room for: the calling thread looks again
	bool unsure;

	// The rubble a large piece found to rest on itself, see nbFindSupport
	int* unsupported;
	int unsupportedCount;

	// The moving pieces it read: the ones it touches and the carriers the searches through rubble came across
	int* movers;
	int moverCount;
} nbBudgetLook;

// Carriers the searches through rubble of a look keep at most besides the pieces it touches, see nbBudgetLook. A search
// ends at the first moving carrier that does not rest on the piece.
#define NB_LOOK_CARRIERS 16

// A look at a piece of debris with contacts, see nbBudgetLook. It only reads the world.
static void nbLookAtDebris( const nbWorld* world, nbArena* arena, int actorIndex, b3Vec3 up, const nbSupportFan* fan,
							nbBudgetLook* look )
{
	const nbActor* actor = world->actors.data + actorIndex;
	int capacity = b3Body_GetContactCapacity( actor->bodyId );
	b3ContactData* contacts = nbArena_AllocArray( arena, b3ContactData, capacity );
	int contactCount = b3Body_GetContactData( actor->bodyId, contacts, capacity );
	nbTouch* touches = nbArena_AllocArray( arena, nbTouch, contactCount );
	nbDescribeTouches( world, actor->bodyId, contacts, contactCount, touches );
	nbMoverLog log = { nbArena_AllocArray( arena, int, contactCount + NB_LOOK_CARRIERS ), 0, contactCount + NB_LOOK_CARRIERS,
					   false };
	for ( int c = 0; c < contactCount; ++c )
	{
		int other = touches[c].otherType == b3_dynamicBody ? touches[c].otherActor : NB_NULL_INDEX;
		if ( other != NB_NULL_INDEX )
		{
			nbLogMover( &log, other );
		}
	}

	look->support = ( nbSupport ){ 0 };
	look->squeezed = B3_IS_NULL( nbFindSqueezer( world, actor, touches, contactCount ) ) == false;
	look->touchesStatic = false;
	look->unsupported = NULL;
	look->unsupportedCount = 0;
	if ( look->squeezed == false )
	{
		// One more place than the budget keeps: a look that fills it found more than the calling thread has room for
		int unsupported[NB_MAX_RELEASES + 1];
		int unsupportedCount = 0;
		int* resting = nbArena_AllocArray( arena, int, 1 << nbRestingBits( contactCount ) );
		look->touchesStatic = nbTouchesStatic( touches, contactCount );
		nbFindSupport( world, actorIndex, touches, contactCount, up, fan, resting, &look->support, unsupported, &unsupportedCount,
					   NB_MAX_RELEASES + 1, &log );
		if ( unsupportedCount > 0 )
		{
			look->unsupported = nbArena_AllocArray( arena, int, unsupportedCount );
			memcpy( look->unsupported, unsupported, sizeof( int ) * (size_t)unsupportedCount );
			look->unsupportedCount = unsupportedCount;
		}
	}

	look->movers = log.actors;
	look->moverCount = log.count;
	look->unsure = log.full || look->unsupportedCount > NB_MAX_RELEASES;
}

// Whether a look still holds: none of the moving pieces it read froze since
static bool nbLookHolds( const nbBudgetLook* look, const uint64_t* frozen )
{
	if ( look->unsure )
	{
		return false;
	}

	for ( int k = 0; k < look->moverCount; ++k )
	{
		int mover = look->movers[k];
		if ( ( frozen[mover >> 6] >> ( mover & 63 ) ) & 1 )
		{
			return false;
		}
	}
	return true;
}

// Moving debris per block of the ranking with looks
#define NB_LOOK_BLOCK 32

// The budget freezes the slowest debris until it is back within the limit, often long before the end of its ranking.
// The looks cover debris up to this many times as fast as the fastest piece the budget before came to, plus this many
// meters a second. The calling thread looks at faster pieces itself when it comes to them.
#define NB_LOOK_REACH 1.5f
#define NB_LOOK_REACH_SPEED 0.05f

// Moving debris from which the workers rank it and look at it for the budget at most and at least, see
// nbWorkerThreshold. Alone the calling thread would look at most pieces twice. A look takes the calling thread about 4 us
// on the VM, where a worker starts after 67 us.
#define NB_PARALLEL_LOOKS 128
#define NB_MIN_PARALLEL_LOOKS 32

typedef struct nbLookTask
{
	const nbWorld* world;
	nbArena* arena;

	// The moving debris as actors in the order of the debris, a look for each
	const int* moving;
	int movingCount;
	nbBudgetLook* looks;
	int blockCount;
	int* nextBlock;

	// The block this task took last, the loop prefetches into the next one
	int lastBlock;

	const nbSupportFan* fan;
	b3Vec3 up;
	float speedLimit;

	// The moving debris this task ranked and how long it took, in milliseconds, see nbWorld::lookEstimate
	int rankedCount;
	float workTime;
} nbLookTask;

// The ranking of a block of the moving debris and the looks at what it ranks. Parts of buildings freeze last and touch
// many moving cells, the calling thread looks at them itself when they come up. As in nbCheckSettleBlock, cursors run
// ahead of the loop for the pieces and their bodies, which it waits for most.
static void nbLookAtBlock( nbLookTask* task, int blockIndex )
{
	const nbWorld* world = task->world;
	const int* moving = task->moving;
	int count = task->movingCount;
	int begin = NB_LOOK_BLOCK * blockIndex;
	int end = b3MinInt( begin + NB_LOOK_BLOCK, count );
	task->rankedCount += end - begin;
	if ( blockIndex != task->lastBlock + 1 )
	{
		for ( int k = begin; k < b3MinInt( begin + 8, count ); ++k )
		{
			NB_PREFETCH( world->actors.data + moving[k] );
			NB_PREFETCH( &world->actors.data[moving[k]].isBuildingPart );
		}
	}
	task->lastBlock = blockIndex;

	for ( int k = begin; k < end; ++k )
	{
		if ( k + 8 < count )
		{
			NB_PREFETCH( world->actors.data + moving[k + 8] );
			NB_PREFETCH( &world->actors.data[moving[k + 8]].isBuildingPart );
		}

#if defined( B3_HAS_BODY_PREFETCH )
		if ( k + 4 < count )
		{
			b3Body_Prefetch( world->actors.data[moving[k + 4]].bodyId );
		}

		if ( k + 2 < count )
		{
			b3Body_PrefetchMotion( world->actors.data[moving[k + 2]].bodyId );
		}
#endif

		const nbActor* actor = world->actors.data + moving[k];
		nbBudgetLook* look = task->looks + k;
		look->ranked = world->time - actor->budgetTime >= NB_FREEZE_GRACE && b3Body_GetContactCapacity( actor->bodyId ) > 0;
		look->unsure = true;
		if ( look->ranked )
		{
			look->speed = b3Length( b3Body_GetLinearVelocity( actor->bodyId ) );
			if ( actor->isBuildingPart == false && ( task->speedLimit == 0.0f || look->speed <= task->speedLimit ) )
			{
				nbLookAtDebris( world, task->arena, moving[k], task->up, task->fan, look );
			}
		}
	}
}

static void nbLookTaskMain( void* context )
{
	nbLookTask* task = context;
	uint64_t ticks = b3GetTicks();
	for ( ;; )
	{
		int blockIndex = nbAtomicFetchAddInt( task->nextBlock, 1 );
		if ( blockIndex >= task->blockCount )
		{
			break;
		}

		nbLookAtBlock( task, blockIndex );
	}
	task->workTime = b3GetMilliseconds( ticks );
}

// Rank the moving debris for the budget: what is past the grace and touches something, or with no gravity all of it.
// With enough of it the workers rank it and look at what they rank, see nbBudgetLook, and the looks come back, otherwise
// null. The ranking is the same either way.
static int nbRankDebris( nbWorld* world, nbDebrisRank* ranks, bool hasGravity, b3Vec3 up, const nbSupportFan* fan,
						 nbBudgetLook** looksOut )
{
	*looksOut = NULL;
	int movingCount = world->debris.count - world->rubbleCount;
	int blockCount = ( movingCount + NB_LOOK_BLOCK - 1 ) / NB_LOOK_BLOCK;
	int taskCount =
		hasGravity && world->enqueueTask != NULL &&
				movingCount >= nbWorkerThreshold( world, world->lookEstimate, NB_MIN_PARALLEL_LOOKS, NB_PARALLEL_LOOKS )
			? b3MinInt( world->workerCount, blockCount )
			: 1;
	int rankCount = 0;
	if ( taskCount < 2 )
	{
		for ( int i = nbNextMoving( world, 0 ); i < world->debris.count; i = nbNextMoving( world, i + 1 ) )
		{
			int actorIndex = world->debris.data[i];
			const nbActor* actor = world->actors.data + actorIndex;
			if ( world->time - actor->budgetTime >= NB_FREEZE_GRACE &&
				 ( hasGravity == false || b3Body_GetContactCapacity( actor->bodyId ) > 0 ) )
			{
				ranks[rankCount++] =
					( nbDebrisRank ){ b3Length( b3Body_GetLinearVelocity( actor->bodyId ) ), actorIndex, NB_NULL_INDEX };
			}
		}
		return rankCount;
	}

	int* moving = nbArena_AllocArray( &world->arena, int, world->debris.count );
	movingCount = 0;
	for ( int w = 0; w < world->movingDebris.count; ++w )
	{
		for ( uint64_t word = world->movingDebris.data[w]; word != 0; word &= word - 1 )
		{
			moving[movingCount++] = world->debris.data[64 * w + nbCTZ64( word )];
		}
	}
	blockCount = ( movingCount + NB_LOOK_BLOCK - 1 ) / NB_LOOK_BLOCK;

	nbBudgetLook* looks = nbArena_AllocArray( &world->arena, nbBudgetLook, movingCount );
	int nextBlock = 0;
	nbLookTask tasks[NB_MAX_WORKERS];
	void* userTasks[NB_MAX_WORKERS];
	for ( int i = 0; i < taskCount; ++i )
	{
		tasks[i] = ( nbLookTask ){
			.world = world,
			.arena = world->workerArenas + i,
			.moving = moving,
			.movingCount = movingCount,
			.looks = looks,
			.blockCount = blockCount,
			.nextBlock = &nextBlock,
			.lastBlock = -2,
			.fan = fan,
			.up = up,
			.speedLimit = world->lookSpeedLimit,
		};
	}

	for ( int i = 1; i < taskCount; ++i )
	{
		userTasks[i] = world->enqueueTask( nbLookTaskMain, tasks + i, world->userTaskContext, "nebenan budget" );
	}

	nbLookTaskMain( tasks );

	for ( int i = 1; i < taskCount; ++i )
	{
		if ( userTasks[i] != NULL )
		{
			world->finishTask( userTasks[i], world->userTaskContext );
		}
	}

	// A look on one thread, for the next update, from all tasks: where the workers start quickly, they can take every block
	// before the calling thread gets to one
	float workTime = 0.0f;
	int rankedCount = 0;
	for ( int i = 0; i < taskCount; ++i )
	{
		workTime += tasks[i].workTime;
		rankedCount += tasks[i].rankedCount;
	}

	if ( rankedCount > 0 )
	{
		nbEstimate( &world->lookEstimate, workTime / (float)rankedCount );
	}

	for ( int k = 0; k < movingCount; ++k )
	{
		if ( looks[k].ranked )
		{
			ranks[rankCount++] = ( nbDebrisRank ){ looks[k].speed, moving[k], k };
		}
	}
	*looksOut = looks;
	return rankCount;
}

// Freeze the slowest debris over budget into rubble. Nothing is removed. Once over budget a tenth more freezes, so the
// ranking only runs every so often. What lies on something fixed freezes first: the ground, a structure or rubble. Only
// when that is not enough, slow debris that lies on moving pieces freezes too, and those pieces bring it back to life
// once they move away, see nbFreezeOnCarriers. Debris in flight or only wedged in never freezes here, it would hang in
// the air: the budget waits until it lands. Each piece remembers the chunks it lies on, and a large piece only freezes
// where it would in nbSettleDebris. A squeezed piece never freezes here, see NB_SQUEEZE_DEPTH.
static void nbEnforceDebrisBudget( nbWorld* world, b3Vec3 up )
{
	int budget = world->def.maxDebrisBodies;
	int count = world->debris.count - world->rubbleCount;
	if ( world->def.enableRubble == false || count <= budget )
	{
		return;
	}

	// Debris that touches nothing never freezes here and takes no part in the ranking
	nbBeginOperation( world );
	bool hasGravity = b3LengthSquared( up ) > 0.0f;
	nbSupportFan fan = nbMakeSupportFan( up );
	nbDebrisRank* ranks = nbArena_AllocArray( &world->arena, nbDebrisRank, count );
	nbBudgetLook* looks = NULL;
	int rankCount = nbRankDebris( world, ranks, hasGravity, up, &fan, &looks );
	if ( rankCount > 0 )
	{
		nbSortRanks( &world->arena, ranks, rankCount );
	}
	int unsupported[NB_MAX_RELEASES];
	int unsupportedCount = 0;
	int excess = b3MinInt( count - budget + budget / 10, nbLimitForStep( world, NB_MAX_BUDGET_FREEZES ) );
	int frozen = 0;

	// The pieces that froze in this update, for the looks
	int wordCount = ( world->actors.count + 63 ) / 64;
	uint64_t* frozenSince = nbArena_AllocArray( &world->arena, uint64_t, wordCount );
	memset( frozenSince, 0, sizeof( uint64_t ) * (size_t)wordCount );

#if defined( B3_HAS_SHAPE_BATCH )
	// The pieces that freeze or come back to life go into Box3D's trees together, see nbSettleDebris
	b3World_BeginShapeBatch( world->physicsWorld );
#endif

	// Parts of buildings freeze only when the rest of the debris is not enough. One that sinks onto the loose cells of a
	// storey that gave way is slow enough to come first, and frozen there it would stand on them, see nbIsQuiet. In a
	// single house the other debris is mostly enough, under a barrage on a town less often.
	int reach = 0;
	for ( int round = 0; round < 4 && frozen < excess; ++round )
	{
		int pass = round & 1;
		bool parts = round >= 2;
		for ( int i = 0; i < rankCount && frozen < excess; ++i )
		{
			reach = b3MaxInt( reach, i );
			int actorIndex = ranks[i].actorIndex;
			const nbActor* actor = world->actors.data + actorIndex;
			if ( actor->isRubble || actor->isBuildingPart != parts )
			{
				continue;
			}

			nbSupport support = { .grounded = true, .lying = true, .balanced = true, .braced = true };
			int capacity = hasGravity ? b3Body_GetContactCapacity( actor->bodyId ) : 0;
			const nbBudgetLook* look =
				looks != NULL && capacity > 0 && nbLookHolds( looks + ranks[i].look, frozenSince ) ? looks + ranks[i].look : NULL;

			// A large piece that is not balanced keeps the rubble it rests on on the list, the search would put it there as
			// long as there is room for all of it
			bool keeps = look != NULL && pass == 0 && look->squeezed == false && look->touchesStatic &&
						 actor->volume >= NB_CHECKED_VOLUME && look->support.balanced == false;
			if ( keeps && unsupportedCount + look->unsupportedCount > NB_MAX_RELEASES )
			{
				look = NULL;
			}

			if ( look != NULL )
			{
				if ( look->squeezed || ( pass == 0 && look->touchesStatic == false ) )
				{
					continue;
				}

				support = look->support;
				if ( keeps )
				{
					memcpy( unsupported + unsupportedCount, look->unsupported, sizeof( int ) * (size_t)look->unsupportedCount );
					unsupportedCount += look->unsupportedCount;
				}
			}
			else if ( capacity > 0 )
			{
				b3ContactData* contacts = nbArena_AllocArray( &world->arena, b3ContactData, capacity );
				int contactCount = b3Body_GetContactData( actor->bodyId, contacts, capacity );
				nbTouch* touches = nbArena_AllocArray( &world->arena, nbTouch, contactCount );
				nbDescribeTouches( world, actor->bodyId, contacts, contactCount, touches );
				if ( B3_IS_NULL( nbFindSqueezer( world, actor, touches, contactCount ) ) == false )
				{
					continue;
				}

				// On the first pass only what lies on something fixed freezes, and only a static body is. Without a
				// contact to one the search would find nothing and keep no rubble to bring back: half the pieces the
				// budget looks at in a burst lie on moving ones.
				if ( pass == 0 && nbTouchesStatic( touches, contactCount ) == false )
				{
					continue;
				}

				int firstUnsupported = unsupportedCount;
				int* resting = nbArena_AllocArray( &world->arena, int, 1 << nbRestingBits( contactCount ) );
				nbFindSupport( world, actorIndex, touches, contactCount, up, &fan, resting, &support, unsupported,
							   &unsupportedCount, NB_MAX_RELEASES, NULL );
				if ( pass > 0 || actor->volume < NB_CHECKED_VOLUME || support.balanced )
				{
					unsupportedCount = firstUnsupported;
				}
			}
			else if ( hasGravity )
			{
				// Nothing touches it: it flies, or it just lost what it lay on or came back to life, since what breaks
				// off gets a new body, rubble has no contacts with what is fixed, and Box3D finds the new ones in the
				// next step. Frozen, it would hang in the air.
				continue;
			}

			// A piece on moving pieces only freezes while it lies on them and they are slow, as it is
			if ( support.balanced == false || ( pass == 0 && support.grounded == false ) ||
				 ( pass > 0 && ( support.lying == false || ranks[i].speed >= NB_FLIGHT_SPEED ) ) )
			{
				continue;
			}

			nbFreezeOnCarriers( world, actorIndex, support.carriers, support.carrierCount );
			frozenSince[actorIndex >> 6] |= (uint64_t)1 << ( actorIndex & 63 );
			frozen += 1;
		}
	}

	// How far the next budget looks ahead, all of the ranking if this one went through all of it
	world->lookSpeedLimit = reach + 1 < rankCount ? NB_LOOK_REACH * ranks[reach].speed + NB_LOOK_REACH_SPEED : 0.0f;

	for ( int i = 0; i < unsupportedCount; ++i )
	{
		if ( world->actors.data[unsupported[i]].isRubble )
		{
			nbThawActor( world, unsupported[i] );
		}
	}

#if defined( B3_HAS_SHAPE_BATCH )
	nbEndTypeBatch( world );
#endif
}

// The update of nbWorld_Update and nbWorld_Step
static void nbUpdate( nbWorld* world, float timeStep )
{
	uint64_t ticks = b3GetTicks();
	world->stepScale = b3ClampFloat( 60.0f * timeStep, 0.0f, 60.0f );

	// The stages of the step before, see nbWorld_GetProfile
	nbProfileStep( world );

	// The streams mark the sites near the bodies that moved, before anything changes the world
	if ( world->streams.count > 0 )
	{
		nbMarkStreams( world );
	}

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
		// Most bodies are far above the depth, they need not fetch their actor. The meter covers the rounding.
		const b3BodyMoveEvent* event = bodyEvents.moveEvents + i;
		b3Pos origin = event->transform.p;
		float originHeight = (float)( up.x * origin.x + up.y * origin.y + up.z * origin.z );
		if ( originHeight - world->maxCenterOffset - 1.0f >= world->def.killDepth )
		{
			continue;
		}

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
	NB_ZONE( damage_zone, "Collision Damage" );
	uint64_t lap = b3GetTicks();
	int impactCount = world->collisionImpacts.count;
	int maxImpacts =
		world->def.maxCollisionImpactsPerUpdate > 0 ? nbLimitForStep( world, world->def.maxCollisionImpactsPerUpdate ) : 0;
	if ( impactCount > maxImpacts )
	{
		impactCount = maxImpacts;
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
	nbProfileLap( world, nb_profileCollisionDamage, &lap );
	NB_ZONE_END( damage_zone );

	// Structures that cannot carry themselves give way
	NB_ZONE( supports_zone, "Check Supports" );
	nbCheckSupports( world );
	NB_ZONE_END( supports_zone );

	// Debris ages with the world time, rubble without being visited
	world->time += timeStep;

	// Rubble follows what moved away from under it, then quiet debris freezes. Debris is never removed: past the budget
	// the slowest freezes.
	if ( hasGravity )
	{
		lap = b3GetTicks();
		NB_ZONE( release_zone, "Release" );
		nbReleaseHeldRubble( world, up );
		NB_ZONE_END( release_zone );
		nbProfileLap( world, nb_profileRelease, &lap );
		NB_ZONE( settle_zone, "Settle" );
		nbSettleDebris( world, up, timeStep );
		NB_ZONE_END( settle_zone );
		nbProfileLap( world, nb_profileSettle, &lap );
	}
	lap = b3GetTicks();
	NB_ZONE( budget_zone, "Budget" );
	nbEnforceDebrisBudget( world, hasGravity ? up : b3Vec3_zero );
	NB_ZONE_END( budget_zone );
	nbProfileLap( world, nb_profileBudget, &lap );

	world->stats.updateTime = b3GetMilliseconds( ticks );
	world->profile.stages[nb_profileUpdate] += world->stats.updateTime;
}

void nbWorld_Update( nbWorldId worldId, float timeStep )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return;
	}

	nbUpdate( world, timeStep );
}

// Count the step and keep the poses before and after it of every body that moved in it, see nbWorld_GetDrawPoses. A slot
// from before may point anywhere, the body at it tells, as with the moves.
static void nbCollectDrawMoves( nbWorld* world, b3BodyEvents events )
{
	world->drawStep += 1;
	nbArray_Reserve( world->drawBodies, world->drawBodies.count + events.moveCount );
	for ( int i = 0; i < events.moveCount; ++i )
	{
		const b3BodyMoveEvent* event = events.moveEvents + i;
		int bodyIndex = event->bodyId.index1 - 1;
		if ( bodyIndex >= world->drawSlots.count )
		{
			nbArray_Reserve( world->drawSlots, bodyIndex + 1 );
			for ( int k = world->drawSlots.count; k <= bodyIndex; ++k )
			{
				world->drawSlots.data[k] = NB_NULL_INDEX;
			}
			world->drawSlots.count = bodyIndex + 1;
		}

		int slot = world->drawSlots.data[bodyIndex];
		if ( slot < 0 || slot >= world->drawBodies.count || world->drawBodies.data[slot].bodyId.index1 != event->bodyId.index1 )
		{
			slot = world->drawBodies.count++;
			world->drawSlots.data[bodyIndex] = slot;
		}

		nbDrawBody* body = world->drawBodies.data + slot;
		body->bodyId = event->bodyId;
		body->userData = event->userData;
		body->to = event->transform;
#if defined( B3_HAS_PREVIOUS_TRANSFORM )
		body->from = event->previousTransform;
		body->localCenter = event->localCenter;
#else
		// Without the pose before the step a body is drawn where the step left it
		body->from = event->transform;
		body->localCenter = b3Vec3_zero;
#endif
		body->fromCenter = b3TransformWorldPoint( body->from, body->localCenter );
		body->toCenter = b3TransformWorldPoint( body->to, body->localCenter );
		body->step = world->drawStep;
	}
}

// Keep the latest move of every body that moved in the step, see nbWorld_GetBodyEvents
static void nbCollectMoves( nbWorld* world )
{
	b3BodyEvents events = b3World_GetBodyEvents( world->physicsWorld );
	if ( world->drawEnabled )
	{
		nbCollectDrawMoves( world, events );
	}

	nbBodyMoveArray* moves = world->moves + world->moveBuffer;
	nbArray_Reserve( *moves, moves->count + events.moveCount );

	// A step has one move per body, so the first step after nbWorld_GetBodyEvents has nothing to merge
	bool merge = moves->count > 0;
	for ( int i = 0; i < events.moveCount; ++i )
	{
		const b3BodyMoveEvent* event = events.moveEvents + i;
		int bodyIndex = event->bodyId.index1 - 1;
		if ( bodyIndex >= world->moveSlots.count )
		{
			nbArray_Reserve( world->moveSlots, bodyIndex + 1 );
			for ( int k = world->moveSlots.count; k <= bodyIndex; ++k )
			{
				world->moveSlots.data[k] = NB_NULL_INDEX;
			}
			world->moveSlots.count = bodyIndex + 1;
		}

		// A slot from before may point anywhere, the body at it tells. One that held the index before this body went.
		int slot = world->moveSlots.data[bodyIndex];
		if ( merge && 0 <= slot && slot < moves->count && moves->data[slot].bodyId.index1 == event->bodyId.index1 )
		{
			moves->data[slot] = *event;
		}
		else
		{
			world->moveSlots.data[bodyIndex] = moves->count;
			moves->data[moves->count++] = *event;
		}
	}
}

// One step of nbWorld_Step, on the step thread or the calling one
static void nbTakeStep( nbWorld* world, float timeStep, int subStepCount )
{
	NB_ZONE( step_zone, "Nebenan Step" );
	uint64_t ticks = b3GetTicks();
	b3World_Step( world->physicsWorld, timeStep, subStepCount );
	world->stats.stepTime = b3GetMilliseconds( ticks );

	nbCollectMoves( world );
	NB_ZONE( update_zone, "Nebenan Update" );
	nbUpdate( world, timeStep );
	NB_ZONE_END( update_zone );
	NB_ZONE_END( step_zone );
	NB_FRAME( "Physics Step" );
}

#if defined( B3_HAS_STEP_GUARD )
static void nbTakeQueuedStep( void* context, const nbQueuedStep* step )
{
	if ( step->fcn != NULL )
	{
		step->fcn( step->context );
	}
	else
	{
		nbTakeStep( context, step->timeStep, step->subStepCount );
	}
}

// Box3D calls it before the thread that handed over the steps touches the Box3D world
static void nbStepGuard( void* context )
{
	nbWaitForSteps( context );
}
#endif

// Without the guard of Box3D, as with a Box3D of its own, the thread that steps could touch the Box3D world during a step,
// so the steps stay on the calling thread
static void nbSetStepGuard( nbWorld* world, bool guarded )
{
#if defined( B3_HAS_STEP_GUARD )
	b3World_SetStepGuard( world->physicsWorld, guarded ? nbStepGuard : NULL, guarded ? world : NULL );
#else
	NB_UNUSED( world );
	NB_UNUSED( guarded );
#endif
}

// Wait until the step thread took every step handed to it, then let the thread that handed them over use the worlds again
static void nbWaitForSteps( nbWorld* world )
{
	if ( nbAtomicLoadInt( &world->stepOwner ) == 0 )
	{
		return;
	}

	// A function after a step that calls a function that waits for the steps, see nbWorld_RunAfterStep
	NB_ASSERT( nbStepThread_IsCurrent( world->stepThread ) == false );

	uint64_t ticks = b3GetTicks();
	nbStepThread_Wait( world->stepThread );
	world->stats.waitTime = b3GetMilliseconds( ticks );
	nbSetStepGuard( world, false );
	nbAtomicStoreInt( &world->stepOwner, 0 );
}

void nbWorld_Step( nbWorldId worldId, float timeStep, int subStepCount )
{
	// Not through nbGetWorldFromId, a step follows the ones before without waiting for them
	nbWorld* world = nbFindWorld( worldId );
	if ( world == NULL )
	{
		return;
	}

	// Steps of another thread are done first
	int token = nbGetThreadToken();
	int owner = nbAtomicLoadInt( &world->stepOwner );
	if ( owner != 0 && owner != token )
	{
		nbWaitForSteps( world );
		owner = 0;
	}

#if defined( B3_HAS_STEP_GUARD )
	if ( world->def.asyncStep && world->stepThread == NULL )
	{
		world->stepThread = nbCreateStepThread( nbTakeQueuedStep, world );
	}
#endif

	// Without a thread the step is taken right here
	if ( world->stepThread == NULL || world->def.asyncStep == false )
	{
		nbTakeStep( world, timeStep, subStepCount );
		return;
	}

	if ( owner == 0 )
	{
		nbSetStepGuard( world, true );
		nbAtomicStoreInt( &world->stepOwner, token );
	}

	nbQueuedStep step = { timeStep, subStepCount, NULL, NULL };
	nbStepThread_Push( world->stepThread, &step );
}

// A frame takes steps of at most so many seconds together, half of it unless the last step with its update took less than
// so many milliseconds per step of 1/60 s, and counts at most so many seconds, see nbWorld_PlanSteps. At 60 Hz these are
// two steps or one, at 120 Hz four or two, at 30 Hz one, and never more than at the highest step rate.
#define NB_PLAN_MAX_TIME ( 2.0f / 60.0f )
#define NB_PLAN_CHEAP_TIME 4.0f
#define NB_PLAN_MAX_FRAME 0.1f

int nbWorld_PlanSteps( nbWorldId worldId, float frameTime, float timeStep )
{
	// Through the lookup that waits, the time of the last step decides
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL || timeStep <= 0.0f )
	{
		return 0;
	}

	bool cheap = world->stats.stepTime + world->stats.updateTime < NB_PLAN_CHEAP_TIME * 60.0f * timeStep;
	float maxTime = cheap ? NB_PLAN_MAX_TIME : 0.5f * NB_PLAN_MAX_TIME;
	int maxSteps = (int)( b3ClampFloat( maxTime / timeStep, 1.0f, NB_PLAN_MAX_TIME * NB_MAX_STEP_RATE ) + 0.5f );
	world->planStep = timeStep;
	world->stepClock += b3ClampFloat( frameTime, 0.0f, NB_PLAN_MAX_FRAME );
	int steps = 0;
	while ( world->stepClock >= timeStep && steps < maxSteps )
	{
		world->stepClock -= timeStep;
		steps += 1;
	}

	// What the steps cannot take is dropped, the simulation slows down
	if ( steps == maxSteps )
	{
		world->stepClock = b3MinFloat( world->stepClock, timeStep );
	}
	return steps;
}

int nbGetSubStepCount( float stepRate )
{
	return b3MaxInt( 1, (int)( NB_SUBSTEP_RATE / nbClampStepRate( stepRate ) + 0.5f ) );
}

int nbWorld_Advance( nbWorldId worldId, float frameTime )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return 0;
	}

	float stepRate = world->def.stepRate;
	float timeStep = 1.0f / stepRate;
	int subStepCount = nbGetSubStepCount( stepRate );
	int steps = nbWorld_PlanSteps( worldId, frameTime, timeStep );
	for ( int i = 0; i < steps; ++i )
	{
		nbWorld_Step( worldId, timeStep, subStepCount );
	}
	return steps;
}

void nbWorld_SetStepRate( nbWorldId worldId, float stepRate )
{
	// Only nbWorld_Advance uses the rate, on the thread that plans the steps, so this need not wait for them
	nbWorld* world = nbFindWorld( worldId );
	if ( world == NULL )
	{
		return;
	}

	world->def.stepRate = nbClampStepRate( stepRate );
}

float nbWorld_GetStepRate( nbWorldId worldId )
{
	nbWorld* world = nbFindWorld( worldId );
	return world != NULL ? world->def.stepRate : NB_DEFAULT_STEP_RATE;
}

void nbWorld_RunAfterStep( nbWorldId worldId, nbAfterStepFcn* fcn, void* context )
{
	nbWorld* world = nbFindWorld( worldId );
	if ( world == NULL || fcn == NULL )
	{
		return;
	}

	// Behind the steps of the calling thread, else right here once the steps of another thread are done
	int owner = nbAtomicLoadInt( &world->stepOwner );
	if ( owner == 0 || owner != nbGetThreadToken() )
	{
		nbWaitForSteps( world );
		fcn( context );
		return;
	}

	nbQueuedStep step = { 0.0f, 0, fcn, context };
	nbStepThread_Push( world->stepThread, &step );
}

b3BodyEvents nbWorld_GetBodyEvents( nbWorldId worldId )
{
	b3BodyEvents events = { 0 };
	nbWorld* world = nbFindWorld( worldId );
	if ( world == NULL )
	{
		return events;
	}

	nbWaitForSteps( world );

	// Hand out the moves so far without the bodies that the updates or the program destroyed after they moved, and
	// collect the next ones into the other buffer
	nbBodyMoveArray* moves = world->moves + world->moveBuffer;
	int count = 0;
	for ( int i = 0; i < moves->count; ++i )
	{
		if ( b3Body_IsValid( moves->data[i].bodyId ) )
		{
			moves->data[count++] = moves->data[i];
		}
	}
	moves->count = count;
	world->moveBuffer ^= 1;
	world->moves[world->moveBuffer].count = 0;

	events.moveEvents = moves->data;
	events.moveCount = moves->count;
	return events;
}

// The pose fraction of the way from the pose before the step to the one after it. The center of mass moves on a straight
// line, for the origin of a body can lie meters away from it, and the rotation turns the short way.
static b3WorldTransform nbDrawBetween( const nbDrawBody* body, float fraction )
{
	if ( fraction <= 0.0f )
	{
		return body->from;
	}
	if ( fraction >= 1.0f )
	{
		return body->to;
	}

	b3Pos center = b3OffsetPos( body->fromCenter, b3MulSV( fraction, b3SubPos( body->toCenter, body->fromCenter ) ) );
	b3Quat q = b3NLerp( body->from.q, body->to.q, fraction );
	b3WorldTransform pose = { b3OffsetPos( center, b3Neg( b3RotateVector( q, body->localCenter ) ) ), q };
	return pose;
}

nbDrawPoses nbWorld_GetDrawPoses( nbWorldId worldId, float fraction )
{
	nbDrawPoses poses = { 0 };
	nbWorld* world = nbFindWorld( worldId );
	if ( world == NULL )
	{
		return poses;
	}

	nbWaitForSteps( world );
	world->drawEnabled = true;
	fraction = b3ClampFloat( fraction, 0.0f, 1.0f );

	// A body that moved in the last step is drawn between its poses, one that stopped since at its last pose, then it goes.
	// The bodies that went meanwhile go without a pose.
	nbDrawBodyArray* bodies = &world->drawBodies;
	nbArray_Reserve( world->drawPoses, bodies->count );
	nbDrawPose* poseData = world->drawPoses.data;
	int poseCount = 0;
	int kept = 0;
	for ( int i = 0; i < bodies->count; ++i )
	{
		const nbDrawBody* body = bodies->data + i;
		if ( b3Body_IsValid( body->bodyId ) == false )
		{
			continue;
		}

		bool moving = body->step == world->drawStep;
		nbDrawPose* pose = poseData + poseCount++;
		pose->bodyId = body->bodyId;
		pose->userData = body->userData;
		pose->transform = moving ? nbDrawBetween( body, fraction ) : body->to;
		if ( moving )
		{
			// Only once a body went before it does one move up
			if ( kept != i )
			{
				world->drawSlots.data[body->bodyId.index1 - 1] = kept;
				bodies->data[kept] = *body;
			}
			kept += 1;
		}
	}
	bodies->count = kept;
	world->drawPoses.count = poseCount;

	poses.poses = world->drawPoses.data;
	poses.count = world->drawPoses.count;
	return poses;
}

float nbWorld_GetStepFraction( nbWorldId worldId )
{
	// Only nbWorld_PlanSteps changes the clock, on the thread that plans the steps, so this need not wait for them
	nbWorld* world = nbFindWorld( worldId );
	if ( world == NULL || world->planStep <= 0.0f )
	{
		return 1.0f;
	}
	return b3ClampFloat( world->stepClock / world->planStep, 0.0f, 1.0f );
}

void nbWorld_SetAsyncStep( nbWorldId worldId, bool enabled )
{
	nbWorld* world = nbFindWorld( worldId );
	if ( world == NULL )
	{
		return;
	}

	nbWaitForSteps( world );
	world->def.asyncStep = enabled;
	if ( enabled == false )
	{
		nbDestroyStepThread( world->stepThread );
		world->stepThread = NULL;
	}
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
			b3Plane plane = shape->planes[f];
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
			twiceArea += b3Dot( shape->planes[f].normal, b3Cross( e1, e2 ) );
		}
		visible[f] = covered[f] < NB_FACE_COVERAGE * 0.5f * twiceArea;
	}
	return faceCount;
}
