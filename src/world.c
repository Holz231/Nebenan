// SPDX-License-Identifier: MIT

#include "world.h"

#include "fracture.h"
#include "hull_builder.h"

#include <float.h>

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

// The hull lives in the scratch arena until the chunk gets its shape, Box3D clones it into its hull database.
static b3HullData* nbCreateScratchHull( nbWorld* world, const nbShape* shape )
{
	int byteCount = nbGetHullByteCount( shape );
	if ( byteCount > 0 )
	{
		void* memory = nbArena_Alloc( &world->arena, (size_t)byteCount );
		b3HullData* hull = nbBuildHull( shape, memory );
		if ( hull != NULL )
		{
			return hull;
		}
	}

	// Fall back to quickhull for shapes beyond the direct path, it also merges degenerate features
	b3HullData* heapHull = b3CreateHull( shape->vertices, shape->vertexCount, B3_MAX_HULL_VERTICES );
	if ( heapHull == NULL )
	{
		return NULL;
	}

	b3HullData* hull = nbArena_Alloc( &world->arena, (size_t)heapHull->byteCount );
	memcpy( hull, heapHull, (size_t)heapHull->byteCount );
	b3DestroyHull( heapHull );
	world->stats.hullFallbackCount += 1;
	return hull;
}

int nbCreateChunk( nbWorld* world, int destructibleIndex, int actorIndex, nbShape* shape, int depth, uint8_t interiorMaterial )
{
	b3HullData* hull = nbCreateScratchHull( world, shape );
	if ( hull == NULL )
	{
		// Degenerate sliver. It cannot be simulated, so it turns into dust.
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

int nbCreateBond( nbWorld* world, int chunkA, int chunkB, float area, b3Vec3 centroid, float health )
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
	bond->centroid = centroid;
	bond->area = area;
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

		// Both ends may have lost their path to an anchor
		nbArray_Push( world->splitSeeds, chunkIndex );
		if ( chunk->actorIndex != NB_NULL_INDEX )
		{
			nbTouchActor( world, chunk->actorIndex );
			if ( world->actors.data[chunk->actorIndex].isStatic )
			{
				world->destructibles.data[chunk->destructibleIndex].spanDirty = true;
			}
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

static b3ShapeDef nbMakeShapeDef( const nbDestructible* destructible, bool isStatic )
{
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.density = destructible->material.density;
	shapeDef.baseMaterial.friction = destructible->material.friction;
	shapeDef.baseMaterial.restitution = destructible->material.restitution;
	shapeDef.baseMaterial.userMaterialId = destructible->material.userMaterialId;
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
		b3ShapeDef shapeDef = nbMakeShapeDef( destructible, actor->isStatic );

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

// Dijkstra from the anchors over the bond graph of the static actor. Moving along gravity is free,
// moving sideways costs the horizontal distance between the chunk centroids. The resulting distance is
// how far the load of a chunk has to travel sideways to reach the ground. Bonds from supported chunks
// to chunks beyond the span are cut, and the regular split then drops the overhanging parts.
void nbCheckSpans( nbWorld* world, int destructibleIndex )
{
	nbDestructible* destructible = world->destructibles.data + destructibleIndex;
	destructible->spanDirty = false;
	float maxSpan = destructible->material.maxSpan;
	if ( destructible->isStatic == false || maxSpan <= 0.0f )
	{
		return;
	}

	int actorIndex = NB_NULL_INDEX;
	for ( int a = destructible->headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
	{
		if ( world->actors.data[a].isStatic )
		{
			actorIndex = a;
			break;
		}
	}

	if ( actorIndex == NB_NULL_INDEX )
	{
		return;
	}

	b3Vec3 gravity = b3World_GetGravity( world->physicsWorld );
	b3Vec3 down = b3InvRotateVector( destructible->transform.q, b3Normalize( gravity ) );
	if ( b3LengthSquared( down ) < 0.5f )
	{
		return;
	}

	nbBeginOperation( world );

	nbActor* actor = world->actors.data + actorIndex;
	int count = actor->chunkCount;
	int* chunkList = nbArena_AllocArray( &world->arena, int, count );
	float* distance = nbArena_AllocArray( &world->arena, float, count );
	uint64_t* heap = nbArena_AllocArray( &world->arena, uint64_t, 4 * count + 8 );
	int heapCapacity = 4 * count + 8;
	int heapCount = 0;

	int n = 0;
	for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
	{
		nbChunk* chunk = world->chunks.data + c;
		chunk->scratch = n;
		chunkList[n] = c;
		if ( chunk->flags & nb_chunkAnchored )
		{
			distance[n] = 0.0f;
			heap[heapCount] = (uint64_t)(uint32_t)n;
			nbSiftUp( heap, heapCount );
			heapCount += 1;
		}
		else
		{
			distance[n] = FLT_MAX;
		}
		n += 1;
	}

	while ( heapCount > 0 )
	{
		uint64_t top = heap[0];
		heap[0] = heap[--heapCount];
		nbSiftDown( heap, heapCount, 0 );

		int i = (int)( top & 0xFFFFFFFFu );
		float d;
		uint32_t bits = (uint32_t)( top >> 32 );
		memcpy( &d, &bits, sizeof( d ) );
		if ( d > distance[i] )
		{
			// Stale entry
			continue;
		}

		const nbChunk* chunk = world->chunks.data + chunkList[i];
		b3Vec3 centroid = chunk->shape->centroid;
		for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
		{
			const nbBond* bond = world->bonds.data + ( key >> 1 );
			int side = key & 1;
			key = bond->nextKey[side];

			const nbChunk* other = world->chunks.data + bond->chunk[side ^ 1];
			int j = other->scratch;
			b3Vec3 delta = b3Sub( other->shape->centroid, centroid );
			b3Vec3 sideways = b3MulSub( delta, b3Dot( delta, down ), down );
			float candidate = d + b3Length( sideways );
			if ( candidate < distance[j] && heapCount < heapCapacity )
			{
				distance[j] = candidate;
				heap[heapCount] = ( (uint64_t)nbFloatKey( candidate ) << 32 ) | (uint32_t)j;
				nbSiftUp( heap, heapCount );
				heapCount += 1;
			}
		}
	}

	// Cut at the span boundary. Unreachable chunks were already dropped by the connectivity split.
	nbIntArray* cut = &world->scratchList;
	cut->count = 0;
	for ( int i = 0; i < n; ++i )
	{
		if ( distance[i] > maxSpan )
		{
			continue;
		}

		const nbChunk* chunk = world->chunks.data + chunkList[i];
		for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
		{
			const nbBond* bond = world->bonds.data + ( key >> 1 );
			int side = key & 1;
			int bondIndex = key >> 1;
			key = bond->nextKey[side];

			int j = world->chunks.data[bond->chunk[side ^ 1]].scratch;
			if ( distance[j] > maxSpan )
			{
				nbArray_Push( *cut, bondIndex );
			}
		}
	}

	if ( cut->count == 0 )
	{
		return;
	}

	for ( int i = 0; i < cut->count; ++i )
	{
		if ( world->bonds.data[cut->data[i]].chunk[0] != NB_NULL_INDEX )
		{
			nbDestroyBond( world, cut->data[i] );
		}
	}

	nbImpactResult result = { 0 };
	nbSplitActors( world, &result );
	nbCommitPhysics( world );

	for ( int i = 0; i < world->touchedActors.count; ++i )
	{
		world->actors.data[world->touchedActors.data[i]].isNew = false;
	}
	world->touchedActors.count = 0;

	// Collapsed parts may overhang again
	destructible = world->destructibles.data + destructibleIndex;
	destructible->spanDirty = false;
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
	def.maxDebrisBodies = 3000;
	def.debrisLifetime = 0.0f;
	def.smallDebrisVolume = 0.002f;
	def.killDepth = -100.0f;
	def.collisionSpeedThreshold = 4.0f;
	def.collisionDamageScale = 12.0f;
	def.collisionRadiusScale = 0.035f;
	def.maxCollisionImpactsPerUpdate = 4;
	def.maxFragmentsPerImpact = 160;
	def.collisionPassThrough = 0.6f;
	def.internalValue = NB_SECRET_COOKIE;
	return def;
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
	world->physicsWorld = def->physicsWorld;
	nbArena_Create( &world->arena, 256 * 1024 );

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
	for ( int i = 0; i < 2; ++i )
	{
		nbArray_Free( world->createdEvents[i] );
		nbArray_Free( world->destroyedEvents[i] );
		nbArray_Free( world->movedEvents[i] );
	}
	nbArray_Free( world->collisionImpacts );
	nbArena_Destroy( &world->arena );

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
	world->eventBuffer = writeBuffer;

	events.createdChunks = world->createdEvents[readBuffer].data;
	events.createdCount = world->createdEvents[readBuffer].count;
	events.destroyedChunks = world->destroyedEvents[readBuffer].data;
	events.destroyedCount = world->destroyedEvents[readBuffer].count;
	events.movedChunks = world->movedEvents[readBuffer].data;
	events.movedCount = world->movedEvents[readBuffer].count;
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
			if ( radius < destructible->material.fragmentSize )
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

	// Debris below the kill depth. Only bodies that moved can have crossed it.
	b3Vec3 gravity = b3World_GetGravity( world->physicsWorld );
	b3Vec3 up = b3Neg( b3Normalize( gravity ) );
	world->scratchList.count = 0;
	if ( b3LengthSquared( up ) > 0.5f )
	{
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
			if ( height < world->def.killDepth )
			{
				nbArray_Push( world->scratchList, actorIndex );
			}
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

	// Structures that lost material may now overhang
	for ( int i = 0; i < world->destructibles.count; ++i )
	{
		nbDestructible* destructible = world->destructibles.data + i;
		if ( destructible->isFree == false && destructible->spanDirty )
		{
			nbCheckSpans( world, i );
		}
	}

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

	// Over budget: remove the oldest small debris first, then the oldest of any size
	while ( world->debris.count > world->def.maxDebrisBodies )
	{
		int oldest = NB_NULL_INDEX;
		float oldestAge = -1.0f;
		bool oldestIsSmall = false;
		for ( int i = 0; i < world->debris.count; ++i )
		{
			const nbActor* actor = world->actors.data + world->debris.data[i];
			bool isSmall = actor->volume < world->def.smallDebrisVolume;
			if ( ( isSmall && oldestIsSmall == false ) || ( isSmall == oldestIsSmall && actor->age > oldestAge ) )
			{
				oldest = world->debris.data[i];
				oldestAge = actor->age;
				oldestIsSmall = isSmall;
			}
		}

		if ( oldest == NB_NULL_INDEX )
		{
			break;
		}
		nbDestroyActor( world, oldest );
	}

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

int nbChunk_GetMeshVertexCount( nbChunkId chunkId )
{
	nbChunk* chunk = nbGetChunkFromId( chunkId, NULL );
	return chunk != NULL ? nbShape_GetMeshVertexCount( chunk->shape ) : 0;
}

int nbChunk_BuildMesh( nbChunkId chunkId, nbMeshVertex* vertices, int capacity, float uvScale )
{
	nbChunk* chunk = nbGetChunkFromId( chunkId, NULL );
	if ( chunk == NULL )
	{
		return 0;
	}
	return nbShape_BuildMesh( chunk->shape, vertices, capacity, uvScale );
}
