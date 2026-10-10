// SPDX-License-Identifier: MIT

#pragma once

#include "core.h"
#include "poly.h"

#include "nebenan/nebenan.h"

#define NB_MAX_WORLDS 16

// Rubble bodies one thaw brings back to life at most, so a single impact cannot wake a mountain of rubble
#define NB_MAX_THAW 512

// The static chunks of a destructible share one static body when Box3D keeps the contacts of every shape in a list of
// their own, so destroying a chunk walks only its own contacts. Otherwise every static chunk owns a body, which keeps
// that cheap too.
#if defined( B3_HAS_SHAPE_CONTACT_LISTS )
#define NB_SHARED_STATIC_BODY
#endif

// Chunks rubble remembers it lay on when it froze
#define NB_MAX_CARRIERS 4

// A chunk a piece lay on when it froze, and the point in world space where it touched
typedef struct nbCarrier
{
	b3Vec3 point;
	int chunkIndex;
	uint16_t generation;
} nbCarrier;

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

	// The chunk sits on a static body of its destructible, the shared one or one of its own, see NB_SHARED_STATIC_BODY
	nb_chunkStaticBody = 0x10,

	// Reported as exposed in the current event window, or about to be destroyed
	nb_chunkExposed = 0x20,
};

// A convex piece. The geometry is in the local frame of the destructible, which is also the
// local frame of the body carrying the chunk.
typedef struct nbChunk
{
	nbShape* shape;

	// Box3D body and shape. Static chunks sit on a static body of their destructible, see NB_SHARED_STATIC_BODY. Dynamic
	// chunks share the body of their actor.
	b3BodyId bodyId;
	b3ShapeId shapeId;

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

	// The bond glues two cells of the pre-fracture of one piece, or of pieces that crack together, not two other pieces
	// or fragments. As long as no damage reaches them, such bonds hold the cells together as one block in the load check,
	// see nbCheckSupport.
	bool cohesive;

	// A cohesive bond between two parts of one cell, split by the seam between two pieces or by an opening. Siblings are
	// thrown as one, so the straight faces between them do not show.
	bool sibling;
} nbBond;

// Second moments of the interface of a bond about its centroid per square meter, see nbBondGeometry. Only the load check
// reads them, so only the bonds of destructibles it can check keep them, see nbDestructible::bondMoments.
typedef struct nbBondMoments
{
	b3Vec3 moments;
	b3Vec3 crossMoments;
} nbBondMoments;

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

	// World time when the actor became free. It ages on while it is rubble, without anything to update.
	double freeTime;

	// World time when the actor became free or a storey gave way next to it, see NB_FREEZE_GRACE
	double budgetTime;

	// Bound on the distance of the chunk vertices from the center of mass, cached with the mass
	float radius;

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

	// Split off the static structure or off rubble during the current operation. What rested on it follows once it
	// moves.
	bool fromStructure;

	// Rubble that pieces broke off below its center of mass during the current operation, see nbApplyImpact
	bool lostPieces;

	// Came down in the collapse of a block or a storey, see nbCheckSupport and nbCheckStoreys, or broke off something
	// that did. Collisions do not fracture it.
	bool fromCollapse;

	// A part of a building that came down in the collapse of a storey. Its own storeys still give way, see
	// nbCheckStoreys.
	bool isBuildingPart;

	// Debris at rest, carried by a static body until something disturbs it
	bool isRubble;

	// Asleep in Box3D at the last look of nbSettleDebris, which found that it cannot freeze yet. While it sleeps the
	// contacts it lies on stay as they are, so it waits for Box3D to wake it before the next look.
	bool checkedAsleep;

	// A static actor that changed since its last load check, see nbWorldDef::supportScale
	bool supportDirty;

	// Rest detection after the rubble rest of the reference engine (src/rubble_rest.h on the Referenz branch). The pose
	// when the quiet time began, and how long the actor stayed within reach of it.
	b3WorldTransform restPose;
	float restTime;

	// A body the solver keeps rocking in place is at rest too, once its mean position holds still. The origin of the
	// current half second window, the sum of the offsets from it, the mean of the last window and how many windows in a
	// row matched their predecessor.
	b3WorldTransform jitterPose;
	b3Vec3 jitterSum;
	b3Vec3 jitterMean;
	float jitterTime;
	int jitterMatches;
	bool jitterSampled;

	// Came back to life from rubble and has not moved since. It freezes again after a short probe instead of the whole
	// rest time, see nbIsQuiet.
	bool probing;
	float probeTime;

	// The rubble resting on this actor comes back to life once it moves away from this pose. The bounds are the ones it
	// had there.
	bool holdsRubble;
	b3WorldTransform holdPose;
	b3AABB holdBounds;

	// The rubble this piece broke off. Its release leaves that rubble alone, it came back to life itself if it lost
	// what carried it, see nbApplyImpact.
	int holdSource;
	uint16_t holdSourceGeneration;

	// The chunks this rubble lay on when it froze, or was wedged between if it lay on none. NB_NULL_INDEX stands for a body
	// that is no chunk such as the ground. None for a piece frozen in flight to keep within the budget. See
	// nbIsSupportedWithout.
	nbCarrier carriers[NB_MAX_CARRIERS];
	int carrierCount;

	// Slot among the quiet actors of the current settle pass, valid while the stamp matches
	int settleSlot;
	uint32_t settleStamp;

	// Visit stamp of the search for what carries rubble, see nbIsSupportedWithout
	uint32_t supportStamp;

	// The last answer of that search for this rubble, valid while the stamp matches
	uint32_t answerStamp;

	// A collapse is breaking a structure apart, the parts it drops come from it
	bool collapsing;
	bool supported;

	// Rests on the piece whose support is being inspected, valid while the stamp matches, see nbFindSupport
	uint32_t restStamp;
} nbActor;

// The room under a floor of a building, along the local Y axis of its destructible, see nbCheckStoreys. It runs from
// the top of the floor below, or the foot of the building, to the bottom of the floor above.
typedef struct nbStorey
{
	float low;
	float high;

	// Volume of the chunks with their centroid in the storey at creation
	float volume;
} nbStorey;

#define NB_MAX_STOREYS 32

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

	// The static body all static chunks share, created with the first of them, see NB_SHARED_STATIC_BODY
	b3BodyId staticBody;

	// Storeys of a static building from the bottom up, and a bit for each of them that gave way
	nbStorey storeys[NB_MAX_STOREYS];
	int storeyCount;
	uint32_t collapsedStoreys;

	// The bonds keep their second moments in nbWorld::bondMoments, as the load check is on and the destructible has no
	// storeys. Without, nothing computes them.
	bool bondMoments;

	// Something changed it since it was created: a bond took damage or broke, or a chunk went. See
	// nbDestructible_IsIntact.
	bool damaged;

	// The shapes of its first chunks in one allocation, when the workers prepared it in the background, and how many of
	// them are still in use, see nbReleaseShape
	uint8_t* shapeBlock;
	size_t shapeBlockSize;
	int shapeBlockCount;

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

// Destructibles the workers prepare in the background, one load each, see nbStartCreating
struct nbBackgroundLoad;
typedef struct nbCreation
{
	struct nbBackgroundLoad* loads;
	int count;
	uint16_t generation;
	bool inUse;
} nbCreation;

NB_ARRAY_DECLARE( nbChunk, nbChunkArray );
NB_ARRAY_DECLARE( nbBond, nbBondArray );
NB_ARRAY_DECLARE( nbBondMoments, nbBondMomentsArray );
NB_ARRAY_DECLARE( nbActor, nbActorArray );
NB_ARRAY_DECLARE( nbDestructible, nbDestructibleArray );
NB_ARRAY_DECLARE( nbChunkId, nbChunkIdArray );
NB_ARRAY_DECLARE( nbCollisionImpact, nbCollisionImpactArray );
NB_ARRAY_DECLARE( nbCreation, nbCreationArray );

typedef struct nbWorld
{
	b3WorldId physicsWorld;
	nbWorldDef def;

	nbChunkArray chunks;
	nbBondArray bonds;
	nbActorArray actors;
	nbDestructibleArray destructibles;

	// Second moments by bond index, as far as the bonds of destructibles that keep them reach, see nbBondMoments. Empty
	// as long as the load check never came on.
	nbBondMomentsArray bondMoments;

	nbIntArray freeChunks;
	nbIntArray freeBonds;
	nbIntArray freeActors;
	nbIntArray freeDestructibles;

	// Destructibles the workers prepare in the background until they are built in, see nbStartCreating
	nbCreationArray creations;
	nbIntArray freeCreations;

	// Box3D shape index to chunk index
	nbIntArray shapeToChunk;

	// Box3D body index to dynamic actor index
	nbIntArray bodyToActor;

	// Free dynamic actors that are subject to lifetime and budget rules
	nbIntArray debris;

	// One bit per entry of debris, set while it moves, so not for rubble. The loops of every update visit only these, in
	// the order of debris, and skip the rubble a word at a time. The bits past the end of debris are clear.
	nbBitArray movingDebris;

	// Seconds simulated so far. Debris ages against it, see nbActor::freeTime.
	double time;

	// Chunks and actors touched by the current operation
	nbIntArray touchedChunks;
	nbIntArray touchedActors;

	// Chunks whose connectivity may have changed during the current operation
	nbIntArray splitSeeds;

	// Scratch list for queries
	nbIntArray scratchList;

	// Actors to freeze into rubble or to bring back to life
	nbIntArray actorList;

	// Static actors waiting for a load check, and the ones the current update checks
	nbIntArray supportChecks;
	nbIntArray supportQueue;

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
	bool ownsScheduler;
	nbArena workerArenas[NB_MAX_WORKERS];

	int chunkCount;
	int bondCount;
	int dynamicActorCount;
	int staticActorCount;
	int destructibleCount;
	int rubbleCount;

	uint32_t bondStamp;
	uint32_t searchStamp;
	uint32_t settleStamp;
	uint32_t supportStamp;
	uint32_t answerStamp;

	// A collapse is breaking a structure apart, the parts it drops come from it
	bool collapsing;

	// While a chunk crushed under its load bursts, its fragments get this speed along the axis out of its wall, see
	// nbCrushChunk. With both faces of the wall free each fragment leaves through the nearer one. Zero otherwise.
	b3Vec3 pushAxis;
	float pushSpeed;
	bool pushBothSides;

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

// Draws the sites of a job on a worker, see nbRunFractureJobs
typedef void nbPrepareJobFn( void* context, int jobIndex );

// Work of the calling thread while the workers compute the cells, see nbRunFractureJobs
typedef void nbCallerWorkFn( void* context );

// Compute the cells of the fracture jobs, spread over the workers. Allocates the cell arrays. The cell
// memory stays valid until the next operation begins. With a prepare function the workers draw the sites
// of every job first: the site count of a job is how many it may draw, and the prepare function sets the
// sites and how many there are. A job with fewer than two gets no cells. With caller work the calling thread
// does that first, while the workers start on the jobs, then it joins them. The caller work must not touch the jobs.
struct nbFractureJob;
void nbRunFractureJobs( nbWorld* world, struct nbFractureJob* jobs, int jobCount, nbPrepareJobFn* prepare,
						void* prepareContext, nbCallerWorkFn* callerWork, void* callerContext );

// Fracture jobs the workers compute while the calling thread does something else
struct nbFractureTask;
typedef struct nbFractureRun
{
	struct nbFractureTask* tasks;
	void* userTasks[NB_MAX_WORKERS];
	int taskCount;
	int nextItem;
} nbFractureRun;

// The two halves of nbRunFractureJobs. Start hands the jobs to the workers and returns, finish has the calling thread
// join them and waits until all cells are done. In between the calling thread may do anything that touches neither the
// jobs nor the arenas. The cell arrays, the work items and the tasks live in the arena, what a task allocates in its
// worker arena, one per worker. The run must stay in place until it is finished.
void nbStartFractureJobs( nbWorld* world, nbFractureRun* run, struct nbFractureJob* jobs, int jobCount,
						  nbPrepareJobFn* prepare, void* prepareContext, nbArena* arena, nbArena* workerArenas );
void nbFinishFractureJobs( nbWorld* world, nbFractureRun* run );

// Run a function for every item, spread over the workers if there are at least minItems. The items must not depend on
// each other or change the world.
typedef void nbParallelFn( void* context, int item );
void nbParallelFor( nbWorld* world, int itemCount, int minItems, nbParallelFn* fn, void* context );

// Create a chunk from a shape and add it to an actor. Builds the Box3D hull right away.
// Takes ownership of the shape. Returns NB_NULL_INDEX and destroys the shape if the hull is degenerate.
int nbCreateChunk( nbWorld* world, int destructibleIndex, int actorIndex, nbShape* shape, int depth, uint8_t interiorMaterial,
				   int materialIndex );

// Same for a shape whose hull is built already, for example by a fracture worker. A shape without a hull, a sliver that
// has no valid one, is dropped.
int nbCreateChunkWithHull( nbWorld* world, int destructibleIndex, int actorIndex, nbShape* shape, int depth, uint8_t interiorMaterial,
						   int materialIndex );

int nbAllocActor( nbWorld* world, int destructibleIndex, bool isStatic );
void nbFreeActor( nbWorld* world, int actorIndex );

// A chunk slot without shape, actor or bonds
int nbAllocChunk( nbWorld* world );

// Keep the second moments of a bond, see nbBondMoments
void nbKeepBondMoments( nbWorld* world, int bondIndex, b3Vec3 moments, b3Vec3 crossMoments );

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

// The definition of the Box3D body of a dynamic actor
b3BodyDef nbMakeActorBodyDef( const nbWorld* world, b3WorldTransform transform );

// The static body for a static chunk of a destructible, see NB_SHARED_STATIC_BODY
b3BodyId nbGetStaticBody( nbWorld* world, nbDestructible* destructible );

// The Box3D shape of a chunk of a destructible with this material, on a static actor or a dynamic one
b3ShapeDef nbMakeShapeDef( const nbDestructible* destructible, const nbMaterial* material, bool isStatic );

// Register a Box3D shape of a chunk and a Box3D body of an actor for the lookups of events and queries
void nbMapShape( nbWorld* world, b3ShapeId shapeId, int chunkIndex );
void nbMapBody( nbWorld* world, b3BodyId bodyId, int actorIndex );

// Remove a chunk with its bonds and physics shape and report it as destroyed.
void nbDestroyChunk( nbWorld* world, int chunkIndex );

// Destroy an actor with all of its chunks and bodies.
void nbDestroyActor( nbWorld* world, int actorIndex );

// Destroy all actors, chunks, bonds, bodies and shapes of a destructible and report its chunks as destroyed
void nbDestroyDestructibleParts( nbWorld* world, int destructibleIndex );

// Free the shape of a chunk of a destructible. One in its block of shapes only counts off, the last one frees the block.
void nbReleaseShape( nbDestructible* destructible, nbShape* shape );

// Whether a shape lies in a block of shapes
static inline bool nbIsInShapeBlock( const uint8_t* block, size_t blockSize, const nbShape* shape )
{
	return block != NULL && (uintptr_t)shape - (uintptr_t)block < blockSize;
}

// Wait until the workers are done with every destructible they prepare in the background, before they stop
void nbWaitForCreations( nbWorld* world );

// Drop what the workers prepared and was never built in, when the world goes
void nbDestroyCreations( nbWorld* world );

// Measure the second moments of the bonds of all destructibles without storeys that do not have them yet, see
// nbBondMoments
void nbComputeBondMoments( nbWorld* world );

bool nbIsAnchored( const nbDestructible* destructible, const nbShape* shape );

// Split actors whose bonds broke into rigid islands. Unsupported islands become dynamic bodies.
void nbSplitActors( nbWorld* world, nbImpactResult* result );

// Create or move Box3D shapes for all touched chunks, update masses and remove empty actors.
void nbCommitPhysics( nbWorld* world );

void nbPushEvent( nbChunkIdArray* events, nbChunkId id );
void nbTouchChunk( nbWorld* world, int chunkIndex );
void nbTouchActor( nbWorld* world, int actorIndex );
void nbUpdateDebris( nbWorld* world, int actorIndex );

// Let the maps from Box3D indices cover all bodies and shapes, see world.c
void nbCoverIndices( nbWorld* world );

// The speed an impact that ejects its fragments at the given speed pushes loose debris with at most. Heavy parts barely
// move, the push falls with the volume beyond four fragments.
float nbGetImpactPush( const nbWorld* world, const nbActor* actor, float ejectSpeed );

// Whether an impact pushes rubble hard enough to bring it back to life
bool nbImpactMoves( const nbWorld* world, const nbActor* actor, float ejectSpeed );

// Bring the rubble in a box back to life that an impact ejecting at the given speed moves. The rubble resting on it
// follows once it moves.
void nbThawRubble( nbWorld* world, b3AABB box, float ejectSpeed );

// Bring one rubble actor back to life. The rubble resting on it follows once it moves.
void nbThawActor( nbWorld* world, int actorIndex );

// Apply an impact, optionally limited to one actor
nbImpactResult nbApplyImpact( nbWorld* world, const nbImpactDef* def, int actorFilter );
