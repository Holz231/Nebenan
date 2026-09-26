// SPDX-License-Identifier: MIT

#pragma once

#include "core.h"
#include "poly.h"

#include "nebenan/nebenan.h"

#define NB_MAX_WORLDS 16

// Rubble bodies one thaw brings back to life at most, so a single impact cannot wake a mountain of rubble
#define NB_MAX_THAW 512

enum nbChunkFlags
{
	// Glued to the world through an anchor plane
	nb_chunkAnchored = 0x01,

	// Created during the current operation, needs a physics shape
	nb_chunkNew = 0x02,

	// Moved to another actor during the current operation, needs its shape on the new body
	nb_chunkMoved = 0x04,

	// In the touched chunk list
	nb_chunkTouched = 0x08,

	// The chunk owns a static body of its own
	nb_chunkOwnsBody = 0x10,

	// Reported as exposed in the current event window, or about to be destroyed
	nb_chunkExposed = 0x20,
};

// A convex piece. The geometry is in the local frame of the destructible, which is also the
// local frame of the body carrying the chunk.
typedef struct nbChunk
{
	nbShape* shape;

	// Box3D body and shape. Static chunks own a static body each, so destroying one chunk never
	// has to walk the contacts of the whole structure. Dynamic chunks share the body of their actor.
	b3BodyId bodyId;
	b3ShapeId shapeId;

	// Hull built at creation in scratch memory, handed to Box3D when the chunk gets its shape
	b3HullData* pendingHull;

	int destructibleIndex;
	int actorIndex;

	// Doubly linked list of the chunks of the actor
	int prevChunk;
	int nextChunk;

	// Linked list of bonds, key = (bondIndex << 1) | side
	int headBondKey;
	int bondCount;

	// Scratch value and visit stamp for graph searches
	int scratch;
	uint32_t searchStamp;

	uint16_t generation;
	uint8_t depth;
	uint8_t flags;

	// Render material of fracture faces inside this chunk
	uint8_t interiorMaterial;

	// Index into the materials of the destructible
	uint8_t materialIndex;
} nbChunk;

// Glue between two touching chunks of the same actor
typedef struct nbBond
{
	// chunk[0] == NB_NULL_INDEX for a free bond
	int chunk[2];
	int prevKey[2];
	int nextKey[2];

	// Interface centroid in the local frame of the destructible
	b3Vec3 centroid;
	float area;

	// Interface normal from chunk[0] to chunk[1]
	b3Vec3 normal;

	// Remaining damage before the bond breaks
	float health;

	uint32_t stamp;
} nbBond;

// A rigid set of chunks. The static actor of a destructible is the set of glued chunks, it has no
// body of its own. A dynamic actor is carried by one Box3D body.
typedef struct nbActor
{
	b3BodyId bodyId;
	int destructibleIndex;

	int headChunk;
	int chunkCount;

	// Doubly linked list of the actors of the destructible
	int prevActor;
	int nextActor;

	// Index into the world debris array or NB_NULL_INDEX
	int debrisIndex;

	float volume;

	// Seconds since the actor became free
	float age;

	// Center of mass in the body frame, cached when the mass changes
	b3Vec3 localCenter;

	// Motion of the actor this one split from. The velocity is set once the new mass is known.
	b3Vec3 sourceLinearVelocity;
	b3Vec3 sourceAngularVelocity;
	b3Pos sourceCenter;

	uint16_t generation;
	bool isStatic;
	bool isFree;

	// Mass needs to be recomputed from the shapes
	bool massDirty;

	// Actor was created during the current operation
	bool isNew;

	// Split off the static structure during the current operation
	bool fromStructure;

	// Debris at rest, carried by a static body until something disturbs it
	bool isRubble;
} nbActor;

typedef struct nbDestructible
{
	// Material 0 is the one of the definition, the others come from pieces with a material of their own
	nbMaterial materials[NB_MAX_MATERIALS];
	int materialCount;
	b3Filter filter;
	nbAnchorPlane anchors[NB_MAX_ANCHORS];
	int anchorCount;

	// Frame of the static chunks
	b3WorldTransform transform;

	int headActor;
	int actorCount;
	int chunkCount;

	uint32_t seed;
	uint32_t fractureCounter;

	bool isStatic;
	bool enableCollisionDamage;
	bool isFree;
	uint16_t generation;
	void* userData;
} nbDestructible;

// A queued collision impact from a Box3D hit event. It only damages the actor that was hit.
typedef struct nbCollisionImpact
{
	b3Pos point;

	// Surface normal pointing out of the damaged chunk toward the other body
	b3Vec3 normal;
	float energy;
	float approachSpeed;

	// The body that hit the chunk
	b3BodyId otherBodyId;

	int actorIndex;
	uint16_t actorGeneration;
} nbCollisionImpact;

NB_ARRAY_DECLARE( nbChunk, nbChunkArray );
NB_ARRAY_DECLARE( nbBond, nbBondArray );
NB_ARRAY_DECLARE( nbActor, nbActorArray );
NB_ARRAY_DECLARE( nbDestructible, nbDestructibleArray );
NB_ARRAY_DECLARE( nbChunkId, nbChunkIdArray );
NB_ARRAY_DECLARE( nbCollisionImpact, nbCollisionImpactArray );

typedef struct nbWorld
{
	b3WorldId physicsWorld;
	nbWorldDef def;

	nbChunkArray chunks;
	nbBondArray bonds;
	nbActorArray actors;
	nbDestructibleArray destructibles;

	nbIntArray freeChunks;
	nbIntArray freeBonds;
	nbIntArray freeActors;
	nbIntArray freeDestructibles;

	// Box3D shape index to chunk index
	nbIntArray shapeToChunk;

	// Box3D body index to dynamic actor index
	nbIntArray bodyToActor;

	// Free dynamic actors that are subject to lifetime and budget rules
	nbIntArray debris;

	// Chunks and actors touched by the current operation
	nbIntArray touchedChunks;
	nbIntArray touchedActors;

	// Chunks whose connectivity may have changed during the current operation
	nbIntArray splitSeeds;

	// Scratch list for queries
	nbIntArray scratchList;

	// Actors to freeze into rubble or to bring back to life
	nbIntArray actorList;

	// Event buffers. The write buffers collect events, nbWorld_GetEvents swaps them with the read buffers.
	nbChunkIdArray createdEvents[2];
	nbChunkIdArray destroyedEvents[2];
	nbChunkIdArray movedEvents[2];
	nbChunkIdArray exposedEvents[2];
	int eventBuffer;

	nbCollisionImpactArray collisionImpacts;

	nbArena arena;

	// Fracture workers: the application's task system or the internal scheduler. Each worker has
	// its own arena, reset with the main arena at the start of every operation.
	int workerCount;
	b3EnqueueTaskCallback* enqueueTask;
	b3FinishTaskCallback* finishTask;
	void* userTaskContext;
	struct nbScheduler* scheduler;
	nbArena workerArenas[NB_MAX_WORKERS];

	int chunkCount;
	int bondCount;
	int dynamicActorCount;
	int staticActorCount;
	int destructibleCount;
	int rubbleCount;

	uint32_t bondStamp;
	uint32_t searchStamp;

	nbStats stats;

	uint16_t worldIndex;
	uint16_t generation;
	bool inUse;
} nbWorld;

nbWorld* nbGetWorld( int index );
nbWorld* nbGetWorldFromId( nbWorldId id );

nbChunkId nbMakeChunkId( const nbWorld* world, int chunkIndex );
nbChunk* nbGetChunkFromId( nbChunkId id, nbWorld** world );
nbDestructible* nbGetDestructibleFromId( nbDestructibleId id, nbWorld** world );

int nbFindChunkFromShape( const nbWorld* world, b3ShapeId shapeId );

// Start an operation that creates, fractures or splits chunks
void nbBeginOperation( nbWorld* world );

// Compute the cells of the fracture jobs, spread over the workers. Allocates the cell arrays. The cell
// memory stays valid until the next operation begins.
struct nbFractureJob;
void nbRunFractureJobs( nbWorld* world, struct nbFractureJob* jobs, int jobCount );

// Create a chunk from a shape and add it to an actor. Builds the Box3D hull right away.
// Takes ownership of the shape. Returns NB_NULL_INDEX and destroys the shape if the hull is degenerate.
int nbCreateChunk( nbWorld* world, int destructibleIndex, int actorIndex, nbShape* shape, int depth, uint8_t interiorMaterial,
				   int materialIndex );

// Same with a hull built beforehand, for example by a fracture worker. The hull memory must stay valid
// until the end of the operation. A null hull drops the shape.
int nbCreateChunkWithHull( nbWorld* world, int destructibleIndex, int actorIndex, nbShape* shape, b3HullData* hull, int depth,
						   uint8_t interiorMaterial, int materialIndex );

int nbAllocActor( nbWorld* world, int destructibleIndex, bool isStatic );
void nbFreeActor( nbWorld* world, int actorIndex );

// The geometry normal points from chunk A to chunk B
int nbCreateBond( nbWorld* world, int chunkA, int chunkB, const nbBondGeometry* geometry, float health );
void nbDestroyBond( nbWorld* world, int bondIndex );

// Fragment size of a material in this world
static inline float nbGetFragmentSize( const nbWorld* world, const nbMaterial* material )
{
	return world->def.fragmentScale * material->fragmentSize;
}

// Material of a chunk
const nbMaterial* nbGetChunkMaterial( const nbWorld* world, const nbChunk* chunk );

// Damage per square meter of bond area that breaks a bond: the strength of the weaker of its two materials
float nbGetBondStrength( const nbWorld* world, const nbBond* bond );

void nbActor_AddChunk( nbWorld* world, int actorIndex, int chunkIndex );
void nbActor_RemoveChunk( nbWorld* world, int actorIndex, int chunkIndex );

// World transform of the frame an actor's chunks are defined in
b3WorldTransform nbActor_GetTransform( const nbWorld* world, const nbActor* actor );

// Create the Box3D body of a dynamic actor and register it for body event lookups
void nbCreateActorBody( nbWorld* world, int actorIndex, b3WorldTransform transform );

// Remove a chunk with its bonds and physics shape and report it as destroyed.
void nbDestroyChunk( nbWorld* world, int chunkIndex );

// Destroy an actor with all of its chunks and bodies.
void nbDestroyActor( nbWorld* world, int actorIndex );

bool nbIsAnchored( const nbDestructible* destructible, const nbShape* shape );

// Split actors whose bonds broke into rigid islands. Unsupported islands become dynamic bodies.
void nbSplitActors( nbWorld* world, nbImpactResult* result );

// Create or move Box3D shapes for all touched chunks, update masses and remove empty actors.
void nbCommitPhysics( nbWorld* world );

void nbPushEvent( nbChunkIdArray* events, nbChunkId id );
void nbTouchChunk( nbWorld* world, int chunkIndex );
void nbTouchActor( nbWorld* world, int actorIndex );
void nbUpdateDebris( nbWorld* world, int actorIndex );

// Bring rubble in a box back to life, and the rubble resting on it
void nbThawRubble( nbWorld* world, b3AABB box );

// Apply an impact, optionally limited to one actor
nbImpactResult nbApplyImpact( nbWorld* world, const nbImpactDef* def, int actorFilter );
