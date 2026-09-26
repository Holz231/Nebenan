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

typedef struct nbThawContext
{
	nbWorld* world;
	nbIntArray* actors;

	// With boxes, only rubble that overlaps one of them
	const b3AABB* boxes;
	int boxCount;
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
	if ( actor->isRubble && thawContext->boxCount > 0 )
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

	if ( actor->isRubble )
	{
		// Thaw right away, so the rubble is not collected twice
		actor->isRubble = false;
		world->rubbleCount -= 1;
		nbArray_Push( *thawContext->actors, actorIndex );
	}
	return true;
}

// Debris at rest becomes static and leaves the island it was part of. Box3D wakes that island once when the
// first of its bodies changes type, the others freeze in the same update.
static void nbFreezeActor( nbWorld* world, int actorIndex )
{
	nbActor* actor = world->actors.data + actorIndex;
	b3Body_SetType( actor->bodyId, b3_staticBody );
	actor->isRubble = true;
	world->rubbleCount += 1;
}

// Rubble in the boxes, then whatever rested on the thawed rubble, in the order Box3D finds it. Box3D's trees are
// deterministic, so is the order. Many boxes are searched with one query over all of them, the neighborhoods
// of the debris of one impact overlap a lot.
static void nbThawRubbleInBoxes( nbWorld* world, const b3AABB* boxes, int boxCount )
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

	nbIntArray* thawed = &world->actorList;
	thawed->count = 0;
	nbThawContext context = { world, thawed, boxCount > 1 ? boxes : NULL, boxCount > 1 ? boxCount : 0 };
	b3World_OverlapAABB( world->physicsWorld, total, b3DefaultQueryFilter(), nbThawCallback, &context );

	context.boxes = NULL;
	context.boxCount = 0;
	for ( int i = 0; i < thawed->count && i < NB_MAX_THAW; ++i )
	{
		int actorIndex = thawed->data[i];
		nbActor* actor = world->actors.data + actorIndex;
		b3Body_SetType( actor->bodyId, b3_dynamicBody );
		b3Body_ApplyMassFromShapes( actor->bodyId );
		b3Body_SetAwake( actor->bodyId, true );
		actor->age = 0.0f;
		if ( world->rubbleCount > 0 )
		{
			b3World_OverlapAABB( world->physicsWorld, nbGetActorBounds( world, actor ), b3DefaultQueryFilter(), nbThawCallback, &context );
		}
	}

	// Past the limit the rest stays rubble
	for ( int i = NB_MAX_THAW; i < thawed->count; ++i )
	{
		world->actors.data[thawed->data[i]].isRubble = true;
		world->rubbleCount += 1;
	}
	thawed->count = 0;
}

void nbThawRubble( nbWorld* world, b3AABB box )
{
	nbThawRubbleInBoxes( world, &box, 1 );
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
	// Neighborhoods of the parts that fell off a structure, for the rubble resting on them
	b3AABB* thawBoxes = nbArena_AllocArray( &world->arena, b3AABB, world->touchedActors.count + 1 );
	int thawBoxCount = 0;

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
		}
		actor->massDirty = false;

		nbUpdateDebris( world, actorIndex );

		// Rubble that rested on a part falling off the structure falls with it
		if ( actor->fromStructure )
		{
			actor->fromStructure = false;
			thawBoxes[thawBoxCount++] = nbGetActorBounds( world, actor );
		}
	}

	nbThawRubbleInBoxes( world, thawBoxes, thawBoxCount );
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

nbWorldDef nbDefaultWorldDef( void )
{
	nbWorldDef def = { 0 };
	def.physicsWorld = b3_nullWorldId;
	def.maxDebrisBodies = 1500;
	def.enableRubble = true;
	def.maxRubbleBodies = 20000;
	def.debrisSleepThreshold = 0.12f;
	def.debrisLifetime = 0.0f;
	def.smallDebrisVolume = 0.002f;
	def.killDepth = -100.0f;
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

void nbWorld_SetDebrisBudget( nbWorldId worldId, int maxDebrisBodies, int maxRubbleBodies )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return;
	}

	world->def.maxDebrisBodies = b3MaxInt( maxDebrisBodies, 0 );
	world->def.maxRubbleBodies = b3MaxInt( maxRubbleBodies, 0 );
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

typedef struct nbDebrisRank
{
	int isLarge;
	float age;
	int actorIndex;
} nbDebrisRank;

// Small before large, old before young, then by index so the order is total and the same everywhere
static int nbCompareDebris( const void* a, const void* b )
{
	const nbDebrisRank* x = a;
	const nbDebrisRank* y = b;
	if ( x->isLarge != y->isLarge )
	{
		return x->isLarge - y->isLarge;
	}
	if ( x->age != y->age )
	{
		return x->age > y->age ? -1 : 1;
	}
	return x->actorIndex - y->actorIndex;
}

// Remove debris over budget: the oldest small pieces first, then the oldest of any size. Moving debris and rubble have
// budgets of their own. Once over budget a tenth more goes, so the ranking only runs every so often.
static void nbEnforceDebrisBudget( nbWorld* world, bool rubble, int budget )
{
	int count = 0;
	for ( int i = 0; i < world->debris.count; ++i )
	{
		count += world->actors.data[world->debris.data[i]].isRubble == rubble ? 1 : 0;
	}

	if ( count <= budget )
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
		if ( actor->isRubble == rubble )
		{
			ranks[rankCount++] = (nbDebrisRank){ actor->volume < world->def.smallDebrisVolume ? 0 : 1, actor->age, actorIndex };
		}
	}

	qsort( ranks, (size_t)rankCount, sizeof( nbDebrisRank ), nbCompareDebris );
	int excess = count - budget + budget / 10;
	for ( int i = 0; i < excess && i < rankCount; ++i )
	{
		if ( world->actors.data[ranks[i].actorIndex].isFree == false )
		{
			nbDestroyActor( world, ranks[i].actorIndex );
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

	// Debris below the kill depth, and debris that fell asleep and turns into rubble. Only bodies that moved can have
	// crossed the depth or fallen asleep.
	b3Vec3 gravity = b3World_GetGravity( world->physicsWorld );
	b3Vec3 up = b3Neg( b3Normalize( gravity ) );
	bool hasGravity = b3LengthSquared( up ) > 0.5f;
	world->scratchList.count = 0;
	world->actorList.count = 0;
	b3BodyEvents bodyEvents = b3World_GetBodyEvents( world->physicsWorld );
	for ( int i = 0; i < bodyEvents.moveCount; ++i )
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
		if ( hasGravity && height < world->def.killDepth )
		{
			nbArray_Push( world->scratchList, actorIndex );
		}
		else if ( event->fellAsleep && world->def.enableRubble && actor->isStatic == false && actor->isRubble == false )
		{
			nbArray_Push( world->actorList, actorIndex );
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

	for ( int i = 0; i < world->actorList.count; ++i )
	{
		int actorIndex = world->actorList.data[i];
		if ( world->actors.data[actorIndex].isFree == false )
		{
			nbFreezeActor( world, actorIndex );
		}
	}
	world->actorList.count = 0;

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

	// Debris aging and lifetime
	float lifetime = world->def.debrisLifetime;
	for ( int i = world->debris.count - 1; i >= 0; --i )
	{
		if ( i >= world->debris.count )
		{
			continue;
		}

		int actorIndex = world->debris.data[i];
		nbActor* actor = world->actors.data + actorIndex;
		actor->age += timeStep;
		if ( lifetime > 0.0f && actor->volume < world->def.smallDebrisVolume && actor->age > lifetime )
		{
			nbDestroyActor( world, actorIndex );
		}
	}

	nbEnforceDebrisBudget( world, false, world->def.maxDebrisBodies );
	nbEnforceDebrisBudget( world, true, world->def.maxRubbleBodies );

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
