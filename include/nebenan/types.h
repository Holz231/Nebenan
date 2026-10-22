// SPDX-License-Identifier: MIT
// Nebenan - polygonal real-time destruction for Box3D

#pragma once

#include "base.h"

#include "box3d/types.h"

/**
 * @defgroup ids Ids
 * Handles to destruction objects. Ids are small value types that carry a generation,
 * so stale ids are detected instead of silently referring to reused memory.
 * @{
 */

/// Destruction world id. References a destruction world attached to one Box3D world.
typedef struct nbWorldId
{
	uint16_t index1;
	uint16_t generation;
} nbWorldId;

/// A destructible structure: one or more convex pieces that share a support graph.
typedef struct nbDestructibleId
{
	int32_t index1;
	uint16_t world0;
	uint16_t generation;
} nbDestructibleId;

/// A chunk is a single convex polyhedron. Every chunk owns exactly one Box3D hull shape.
typedef struct nbChunkId
{
	int32_t index1;
	uint16_t world0;
	uint16_t generation;
} nbChunkId;

/// Destructibles the workers prepare in the background, see nbStartCreating.
typedef struct nbCreationId
{
	int32_t index1;
	uint16_t world0;
	uint16_t generation;
} nbCreationId;

/// Loads and unloads the destructibles of a world too large to hold at once, see nbCreateStream.
typedef struct nbStreamId
{
	int32_t index1;
	uint16_t world0;
	uint16_t generation;
} nbStreamId;

static const nbWorldId nb_nullWorldId = NB_ZERO_INIT;
static const nbDestructibleId nb_nullDestructibleId = NB_ZERO_INIT;
static const nbChunkId nb_nullChunkId = NB_ZERO_INIT;
static const nbCreationId nb_nullCreationId = NB_ZERO_INIT;
static const nbStreamId nb_nullStreamId = NB_ZERO_INIT;

/// Use these to test for null. Example: `if ( NB_IS_NULL( chunkId ) ) { ... }`
#define NB_IS_NULL( id ) ( ( id ).index1 == 0 )

/// Use these to test for non-null.
#define NB_IS_NON_NULL( id ) ( ( id ).index1 != 0 )

/// Compare two ids for equality.
#define NB_ID_EQUALS( id1, id2 ) ( ( id1 ).index1 == ( id2 ).index1 && ( id1 ).generation == ( id2 ).generation )

/** @} */

/**
 * @defgroup material Material
 * @{
 */

/// Physical and fracture properties of a destructible material.
typedef struct nbMaterial
{
	/// Mass density in kg/m^3. Concrete is about 2400, brick 1900, wood 600.
	float density;

	/// Coulomb friction coefficient of the chunk shapes.
	float friction;

	/// Coefficient of restitution of the chunk shapes.
	float restitution;

	/// Damage needed to break one square meter of bond area. Bonds are the glued faces between chunks.
	/// Larger values make the material tougher.
	float strength;

	/// Typical edge length of the fragments created at the center of an impact, in meters.
	float fragmentSize;

	/// Fragments with a smaller volume are removed, in cubic meters.
	float minFragmentVolume;

	/// Maximum number of times a chunk can be refined by runtime fracture. Limits how deep the
	/// fracture hierarchy can go around repeated impacts.
	int maxDepth;

	/// User material id stored on the Box3D shapes. It is reported by ray casts and contact events.
	uint64_t userMaterialId;
} nbMaterial;

/// Maximum number of different materials in one destructible.
#define NB_MAX_MATERIALS 8

/** @} */

/**
 * @defgroup destructible Destructible
 * @{
 */

/// An anchor plane in the local frame of a destructible. Chunks that touch the half space
/// `dot(normal, x) <= offset` are glued to the world. Chunks without a path of intact bonds to an
/// anchored chunk collapse.
typedef struct nbAnchorPlane
{
	b3Vec3 normal;
	float offset;
} nbAnchorPlane;

/// Maximum number of anchor planes per destructible.
#define NB_MAX_ANCHORS 8

/// A box cut out of a piece, a window or a door. In the frame of the piece, along its axes.
typedef struct nbOpening
{
	/// Center in the frame of the piece.
	b3Vec3 center;

	/// Half extents along the axes of the piece.
	b3Vec3 halfExtents;
} nbOpening;

/// A convex piece of a destructible. Pieces are given in the local frame of the destructible.
/// Touching faces of different pieces are bonded together automatically.
typedef struct nbPieceDef
{
	/// Points of the convex piece. The convex hull of the points is used.
	/// Leave null to make a box from halfExtents and transform.
	const b3Vec3* points;

	/// Number of points.
	int pointCount;

	/// Half extents of a box piece. Used when points is null.
	b3Vec3 halfExtents;

	/// Local transform of the piece within the destructible.
	b3Transform transform;

	/// Render material id of the outer surface of this piece.
	uint8_t surfaceMaterial;

	/// Render material id of the fracture surfaces created inside this piece.
	uint8_t interiorMaterial;

	/// Material of this piece, null for the material of the destructible. A destructible holds up to
	/// NB_MAX_MATERIALS different materials. A bond between two materials is as strong as the weaker one.
	const nbMaterial* material;

	/// Pre-fracture of this piece into Voronoi cells of roughly this size, in meters. Zero takes the cell
	/// size of the destructible, a negative value keeps the piece a single chunk.
	float cellSize;

	/// Boxes cut out of the piece, windows and doors. With a cell size the piece is cut into cells first
	/// and the openings out of the cells, so the cells run around the openings and cracks do not follow
	/// their edges. Without, the piece is cut into convex parts around them. The faces of the openings
	/// get the surface material.
	const nbOpening* openings;

	/// Number of openings.
	int openingCount;
} nbPieceDef;

/// Destructible definition. Must be initialized with nbDefaultDestructibleDef.
typedef struct nbDestructibleDef
{
	/// World position of the destructible frame.
	b3Pos position;

	/// World rotation of the destructible frame.
	b3Quat rotation;

	/// Material of the pieces that do not have their own.
	nbMaterial material;

	/// Static destructibles are glued to the world through their anchors. Dynamic destructibles
	/// start as a free rigid body.
	bool isStatic;

	/// Anchor planes in the local frame. If none are given a static destructible is anchored
	/// at its lowest point along local -Y.
	nbAnchorPlane anchors[NB_MAX_ANCHORS];

	/// Number of anchor planes.
	int anchorCount;

	/// Optional pre-fracture of every piece into Voronoi cells of roughly this size, in meters.
	/// Zero starts every piece as a single chunk. Runtime fracture refines chunks around impacts
	/// either way.
	float cellSize;

	/// Seed for the deterministic fracture patterns and the values every storey picks, see nbStoreyCollapse.
	uint32_t seed;

	/// Collision filter of the chunk shapes.
	b3Filter filter;

	/// Collisions of dynamic chunks with other shapes can damage both sides.
	bool enableCollisionDamage;

	/// User data.
	void* userData;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} nbDestructibleDef;

/** @} */

/**
 * @defgroup impact Impact
 * @{
 */

/// Describes an impact such as a bullet hit or an explosion.
typedef struct nbImpactDef
{
	/// Impact point in world space.
	b3Pos point;

	/// Direction of the incoming projectile in world space. Fragments behind the surface are ejected
	/// along it. Use zero for an explosion that ejects radially.
	b3Vec3 direction;

	/// Optional surface normal at the impact point in world space. Fragments near the surface spall
	/// back out along it, like the crater of a bullet hole. nbWorld_CastImpact fills it in.
	b3Vec3 normal;

	/// Damage radius in meters. Chunks overlapping this sphere are refined and damaged.
	float radius;

	/// Damage at the impact center. Damage falls off to zero at the radius.
	/// A bond breaks when its accumulated damage exceeds material strength times bond area.
	float damage;

	/// Speed of the fragments ejected at the impact center in meters per second.
	float ejectSpeed;

	/// Number of fragments created by runtime fracture around the impact center.
	/// Zero derives the count from the material fragment size and the damaged volume.
	int fragmentCount;

	/// Impulse per square meter of surface with which a Box3D explosion pushes every body around the impact after the
	/// fracture, see b3World_Explode, with the damage radius as its radius and falloff. Nebenan pushes the loose debris in
	/// the blast by itself, the explosion also reaches every other body, like the props and characters of a game, and the
	/// fresh fragments. Zero, the default, starts no explosion. The grenades of the demo take 40 times the eject speed.
	float explosionImpulse;

	/// Limit the impact to one destructible. Null affects all destructibles.
	nbDestructibleId destructibleId;
} nbImpactDef;

/// What an impact did.
typedef struct nbImpactResult
{
	/// Chunks that were split by runtime Voronoi fracture.
	int fracturedChunkCount;

	/// New chunks created by the fracture.
	int createdChunkCount;

	/// Bonds broken by damage.
	int brokenBondCount;

	/// Chunks that changed from glued to free.
	int detachedChunkCount;

	/// New rigid bodies created for debris and collapsing parts.
	int createdBodyCount;

	/// Time in milliseconds from the start of the fracture until all cells are done. The workers draw the fracture
	/// sites and compute the Voronoi cells and their hulls, the calling thread brings the rubble in reach back to
	/// life meanwhile.
	float fractureTime;

	/// Total time of the impact in milliseconds.
	float totalTime;
} nbImpactResult;

/** @} */

/**
 * @defgroup world World
 * @{
 */

/// Maximum number of fracture workers.
#define NB_MAX_WORKERS 32

/// A pool of threads Nebenan and Box3D can share, see nbCreateTaskSystem
typedef struct nbTaskSystem nbTaskSystem;

/// How the storeys of a building give way, see nbWorldDef::storeyCollapse. Every storey picks its own values between the
/// lower and the upper end of each range at creation, from the seed of its building, see nbDestructibleDef::seed, the
/// same for the same building every time. Equal ends give every storey the same.
typedef struct nbStoreyCollapse
{
	/// A storey gives way once this fraction of the walls it had at creation is destroyed. Zero turns it off.
	float damageMin;
	float damageMax;

	/// Of the walls that are left in a storey that gives way, this fraction stays where it is without a push. Zero
	/// throws all of the walls out, one keeps them all.
	float residueMin;
	float residueMax;
} nbStoreyCollapse;

/// Destruction world definition. Must be initialized with nbDefaultWorldDef.
typedef struct nbWorldDef
{
	/// The Box3D world that simulates the chunks. Required.
	b3WorldId physicsWorld;

	/// Maximum number of debris bodies that move. Beyond it the slowest debris that lies on something freezes into rubble
	/// right away: first what lies on the ground, a structure or rubble, then what lies slowly on moving pieces, which
	/// bring it back to life once they move away. Debris in flight never freezes, it would hang in the air, so after a
	/// large blast more debris moves until it lands. Debris is never removed, this only bounds the work of the Box3D step.
	int maxDebrisBodies;

	/// Debris that has come to rest turns into static rubble. It stays where it is and costs the physics step nothing.
	/// Anything that hits it hard or moves away from under it brings it back to life.
	bool enableRubble;

	/// Speed in meters per second below which debris counts as slow. Rest detection follows the reference engine: a piece
	/// comes to rest once it stayed at four times this speed at most for debrisRestTime, or once its mean position held
	/// still for 2 s while the solver kept rocking it. It freezes into rubble when it lies on the ground, a structure or
	/// rubble, or on a resting piece that does, so piles freeze from the bottom up. Box3D also puts whole islands to sleep
	/// that stay slower than this.
	float debrisSleepThreshold;

	/// Seconds a slow piece must stay near one pose before it counts as at rest, at most 1 s. Near means within 0.1 m/s
	/// times this time, counting rotation as the way its farthest corner moves: 1 cm at the default of 0.1 s. Under heavy
	/// fire almost half of the moving debris only waits out this time, so a shorter one keeps fewer bodies and contacts in
	/// the Box3D step. It can also stop a small piece that only paused while it tumbled.
	float debrisRestTime;

	/// Box3D sweeps a moving body against static shapes so it cannot pass through a wall within one step, see
	/// b3BodyDef::safetyFactor. Debris is swept once it moves this far in one step, in meters, or twice the radius of the
	/// largest sphere in its thinnest chunk if that is less, and large debris no later than Box3D sweeps it by default,
	/// once it moves half that radius. Debris that moves less sinks into a wall by less than its own size and is pushed
	/// back out the way it came. Keep it below the thickness of the thinnest wall. Zero keeps Box3D's default.
	float debrisSweepDistance;

	/// Debris that falls below this height along the gravity direction has left the world and is removed, in meters.
	float killDepth;

	/// A load check for structures without floors, like a single wall or a bridge. Buildings with floors never take it,
	/// they only give way storey by storey, see storeyCollapse. The load of every chunk flows along its bonds to the
	/// anchors, and a bond fails when the force through it exceeds its area times the strength of the weaker material,
	/// or the bending of an overhang exceeds what the shape of the bond carries. The part that loses its way to the
	/// anchors falls as one piece, and a chunk crushed under too much weight bursts into fragments. Lower values let
	/// structures give way sooner. Zero, the default, turns the check off: a structure then only loses what is cut off
	/// from its anchors. Change it at run time with nbWorld_SetSupportScale.
	float supportScale;

	/// How the storeys of a static building give way, on any floor and whatever the walls could still carry: once the
	/// storey lost the damage of its walls, part of the walls that are left stays where it is without a push, broken
	/// into its cells. When the damage lies mostly on one side, they are cells on the strongest side, and those bonded to
	/// what is below stay with it, an anchor the part above tilts over. When the damage is spread around, they are cells
	/// picked at random that mostly come loose. The rest of the walls flies out, and everything above comes down in one
	/// piece onto what stayed and tilts over it as gravity and contacts make it, or stays on its loose cells until hits
	/// clear them away. Nothing else of a building collapses, no wall above a hole and no floor, only what is cut off from
	/// the anchors falls. Floors are the flat pieces of a destructible, at most a quarter as high along its local Y axis
	/// as they are wide in both other directions, and a storey is the room under a floor. By default a storey gives way at
	/// 17 to 31 percent of its walls destroyed, and all of the walls that are left stay, so nothing flies out. Change it
	/// at run time with nbWorld_SetStoreyCollapse.
	nbStoreyCollapse storeyCollapse;

	/// The same for the top storey of a building, the one under the roof. By default it gives way at 18 to 25 percent of
	/// its walls destroyed, and all of the walls that are left stay.
	nbStoreyCollapse topStoreyCollapse;

	/// Minimum approach speed for collision damage in meters per second.
	float collisionSpeedThreshold;

	/// Scales the collision energy (Joule) into damage.
	float collisionDamageScale;

	/// Damage radius of a collision is this scale times the cube root of the collision energy (Joule).
	float collisionRadiusScale;

	/// Maximum number of collision impacts processed per update of 1/60 s, in proportion for other steps, see stepRate.
	/// Limits worst case frame time.
	int maxCollisionImpactsPerUpdate;

	/// Upper bound for the fragments one impact creates. Bounds the cost of large explosions.
	int maxFragmentsPerImpact;

	/// Multiplies the fragment size of every material. The number of pieces an impact creates falls with the
	/// square of the fragment size, and with it the bodies, contacts and meshes: twice the size halves the
	/// cost of mass destruction. Change it at run time with nbWorld_SetFragmentScale.
	float fragmentScale;

	/// Fraction of the approach speed a body keeps when it breaks through a chunk. Box3D resolves the
	/// contact before the fracture happens, so without this a cannonball would bounce off the debris.
	float collisionPassThrough;

	/// Number of threads that work on an impact, including the calling thread. They estimate how much of each
	/// chunk the impact damages, draw the fracture sites and compute the Voronoi cells and hulls. Clamped to
	/// [1, NB_MAX_WORKERS]. Above 1, Nebenan uses the task callbacks below, or starts its own threads
	/// when none are given. The fracture result does not depend on the number of workers.
	int workerCount;

	/// Optional pool of threads from nbCreateTaskSystem, shared with Box3D. Takes precedence over enqueueTask and
	/// finishTask. Nebenan prepares the destructibles of nbStartCreating there after all other tasks.
	nbTaskSystem* taskSystem;

	/// Optional task system. The signatures are the ones of Box3D, so one task system can serve both.
	/// Nebenan enqueues at most workerCount - 1 tasks at a time and finishes all of them before it
	/// enqueues more or returns. Only nbStartCreating leaves tasks running, one per destructible, until
	/// nbFinishCreating. The allocator set with nbSetAllocator must be thread safe when workers are used.
	b3EnqueueTaskCallback* enqueueTask;

	/// Finishes a task returned by enqueueTask.
	b3FinishTaskCallback* finishTask;

	/// User context passed to enqueueTask and finishTask.
	void* userTaskContext;

	/// nbWorld_Step takes the step on a thread of its own and returns right away, so the program draws its frame while
	/// the physics runs. The thread that called it waits for the step as soon as it calls a function of Nebenan or Box3D
	/// for the worlds again. Nothing else changes, the worlds end up the same to the bit as with the step on the calling
	/// thread. The allocators of Nebenan and Box3D must be thread safe. The workers of a step wait for their next task
	/// without sleeping, so leave the thread that draws a hardware thread of its own. On by default, see
	/// nbWorld_SetAsyncStep.
	bool asyncStep;

	/// Steps per second of nbWorld_Advance, 60 by default. Every step gets as many substeps as make 240 per second, see
	/// nbGetSubStepCount, so the solver works the same at any rate, and the limits per update, like
	/// maxCollisionImpactsPerUpdate, grow and shrink with the step, so the world takes as much per second. The pair search,
	/// the collision and the update run once per step: a lower rate costs less per second, but contacts and hits come up
	/// to a step later and fast debris passes through other debris more easily. A higher rate is more exact and costs more.
	/// The results are the same with any number of threads at every rate. Clamped to [10, 240], change it at run time
	/// with nbWorld_SetStepRate.
	float stepRate;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} nbWorldDef;

/// A function of the program that nbWorld_RunAfterStep runs after the steps of nbWorld_Step
typedef void nbAfterStepFcn( void* context );

/// Destruction events collected since the last call to nbWorld_GetEvents. Use them to keep a
/// renderer in sync: build meshes for created chunks, drop destroyed chunks, re-parent moved chunks and
/// rebuild the meshes of exposed chunks.
typedef struct nbEvents
{
	/// Chunks created since the last call. They may already be destroyed again, check nbChunk_IsValid.
	const nbChunkId* createdChunks;

	/// Chunks destroyed since the last call.
	const nbChunkId* destroyedChunks;

	/// Chunks that moved to a different body since the last call. Created chunks are not repeated here.
	const nbChunkId* movedChunks;

	/// Chunks that lost a bond since the last call. Faces that the bond covered may be visible now, see
	/// nbChunk_GetVisibleFaces. They may already be destroyed again, check nbChunk_IsValid.
	const nbChunkId* exposedChunks;

	int createdCount;
	int destroyedCount;
	int movedCount;
	int exposedCount;
} nbEvents;

/// A body and where to draw it, see nbWorld_GetDrawPoses
typedef struct nbDrawPose
{
	b3BodyId bodyId;
	void* userData;
	b3WorldTransform transform;
} nbDrawPose;

/// The bodies whose pose to draw changed since the last call to nbWorld_GetDrawPoses
typedef struct nbDrawPoses
{
	const nbDrawPose* poses;
	int count;
} nbDrawPoses;

/// Counters and timings of a destruction world.
typedef struct nbStats
{
	int destructibleCount;
	int chunkCount;
	int bondCount;
	int staticBodyCount;
	int dynamicBodyCount;
	int debrisCount;

	/// Debris that came to rest and became static rubble. Also counted as debris and dynamic bodies.
	int rubbleCount;

	/// Totals since the world was created. Overloaded bonds broke because they could not carry their load.
	int impactCount;
	int fractureCount;
	int createdChunkCount;
	int overloadedBondCount;

	/// Storeys that gave way, see nbWorldDef::storeyCollapse. Total since the world was created.
	int collapsedStoreyCount;

	/// Chunks whose physics hull needed the quickhull fallback instead of the direct build.
	int hullFallbackCount;

	/// Timings of the last update and the last impact in milliseconds.
	float updateTime;
	float lastImpactTime;
	float lastFractureTime;

	/// The Box3D step that nbWorld_Step took last, and how long the thread that called it waited for its steps the last
	/// time it had to, in milliseconds. See nbWorldDef::asyncStep.
	float stepTime;
	float waitTime;

	/// Bytes allocated by Nebenan (all worlds).
	int64_t byteCount;
} nbStats;

/// The stages of a step that nbProfile times. A stage can hold others: the step holds the pairs, the collide and the
/// solve, these hold their parts on the calling thread, and the update holds its parts. Freezing and thawing and the
/// cells of the fractures happen inside the update and the impacts.
typedef enum nbProfileStage
{
	/// The step, the update and the impacts together
	nb_profileTotal,

	/// b3World_Step with the stages of b3Profile: the pair update and the new contacts in it, which run on the calling
	/// thread alone, the contact loop and the contacts in it that begin or end touching, again on the calling thread
	/// alone, the solver with its setup and its constraints, and the finish of the bodies with the continuous collision
	nb_profileStep,
	nb_profilePairs,
	nb_profileCreateContacts,
	nb_profileCollide,
	nb_profileContactStates,
	nb_profileSolve,
	nb_profileSolverSetup,
	nb_profileConstraints,

	/// The part of the constraints that the calling thread solves alone, the overflow of Box3D's constraint graph, in the
	/// steps that solve by colors, see b3Profile::overflow
	nb_profileOverflow,
	nb_profileTransforms,

	/// nbWorld_Update with its collision damage, the debris that comes to rest, the debris budget and the rubble that
	/// lost what held it
	nb_profileUpdate,
	nb_profileCollisionDamage,
	nb_profileSettle,
	nb_profileBudget,
	nb_profileRelease,

	/// Debris that freezes into rubble and rubble that comes back to life, in the update and the impacts
	nb_profileTypeChanges,

	/// nbWorld_ApplyImpact and nbWorld_CastImpact between the steps
	nb_profileImpacts,

	/// The cells of the fractures of impacts and collision damage, and those of them that the calling thread computed
	/// alone, because a fracture had too few cells for the workers
	nb_profileCells,
	nb_profileCellsAlone,

	nb_profileStageCount
} nbProfileStage;

/// Where the time of the steps went, see nbWorld_GetProfile.
typedef struct nbProfile
{
	/// The workers of the Box3D world during the steps
	int workerCount;

	/// Steps counted, and the impacts between them
	int stepCount;
	int impactCount;

	/// Milliseconds per step, see nbProfileStage
	float stages[nb_profileStageCount];

	/// Freezes into rubble and thaws back to life per step
	float freezeCount;
	float thawCount;

	/// The share of the largest awake island in the work of the solver, see b3Counters::islandWork, mean over the steps of
	/// a Box3D world with workers, and how many steps that were. colorStepCount counts the steps with more than half of
	/// the work in one island. In the others a pack gives the island to one thread, so without a team the packs with n
	/// threads take at least the larger of its share and 1/n of the work. islandFloor holds the mean of that bound over
	/// those steps for 2, 4, 8 and 16 threads.
	float largestIsland;
	float islandFloor[4];
	int islandStepCount;
	int colorStepCount;

	/// The steps in which a team of all workers solved the largest pack of islands stage by stage, while they solved the
	/// other packs whole in between, see b3Counters::teamWork, and the share of that pack in the work of the solver, mean
	/// over those steps. Box3D takes a team once the pack holds clearly more than the even share of a worker.
	int teamStepCount;
	float teamShare;

	/// The constraints in the overflow of Box3D's constraint graph per step, the contacts of them that Box3D solves in
	/// batches of knots, several at once, and those batches, see b3Counters::knotContactCount
	float overflowCount;
	float knotContactCount;
	float knotBatchCount;

	/// How long a sleeping worker of Nebenan took to start a task it was given, in microseconds, mean and longest, over
	/// the tasks of the impacts and the collision damage that went to the workers
	float wakeTime;
	float longestWake;
	int wakeCount;

	/// The updates whose checks of the debris that comes to rest went to the workers, the share of the moving debris the
	/// workers checked in them, mean over those updates, and how long a sleeping worker took to start them, in
	/// microseconds, mean over their tasks. The calling thread checks what the workers do not take.
	int settleSplitCount;
	float settleWorkerShare;
	float settleWakeTime;

	/// Where the constraints of the steps with a team go, timed in every eighth of them, see b3Profile::teamTime: the timed
	/// steps, and in milliseconds per timed step the time of the team, the part the calling thread spends alone on the
	/// overflow of the team's pack and waiting at the end of the stages, and per worker the waits for a new stage and the
	/// time on other packs. Then per timed step the stages with a barrier and how often a worker gave up its core.
	int teamSampleCount;
	float teamTime;
	float teamOverflow;
	float teamStageWait;
	float teamWorkerWait;
	float teamWorkerPacks;
	float teamStageCount;
	float teamYieldCount;

	/// How long the calling thread waits for the last packs of islands, milliseconds per step
	float packWait;

	/// From how many items the workers help with a fracture and from how many moving pieces with the checks of the update,
	/// when the profile was read. Nebenan chooses both by how fast a worker starts: the calling thread alone would take
	/// longer than three times that time. Where workers start slowly they stay at 32 and 512.
	int fractureWorkerItems;
	int settleWorkerPieces;
} nbProfile;

/// The stages of profiles measured with different numbers of workers, fitted to T(n) = serial + parallel / n
/// milliseconds per step, see nbFitScaling.
typedef struct nbScaling
{
	int profileCount;
	float serial[nb_profileStageCount];
	float parallel[nb_profileStageCount];
} nbScaling;

/** @} */

/**
 * @defgroup stream Stream
 * @{
 */

/// What a stream builds the destructible of a site from, see nbStreamDef::prepareFcn. The pointers stay valid until the
/// stream calls nbStreamDef::releaseFcn for the site.
typedef struct nbSiteDef
{
	nbDestructibleDef def;
	const nbPieceDef* pieces;
	int pieceCount;
} nbSiteDef;

/// Fill in the definition of a site the stream is about to load. Called on the thread that streams, see nbStream_Update.
typedef void nbSitePrepareFcn( int site, nbSiteDef* siteDef, void* context );

/// The stream no longer needs what nbSitePrepareFcn gave it for a site.
typedef void nbSiteReleaseFcn( int site, void* context );

/// Keep the bytes of a damaged site that goes with its rubble, a file for example. Return false if they could not be
/// kept, then the site stays in the world.
typedef bool nbSiteStoreFcn( int site, const void* data, size_t size, void* context );

/// Write the bytes kept for a site into the buffer, as many as the store got. Return false if they are gone, then the
/// stream asks again the next time it wants the site.
typedef bool nbSiteFetchFcn( int site, void* buffer, size_t size, void* context );

/// How a stream loads and unloads the destructibles of its sites, see nbCreateStream. The distances are in meters and
/// the times in seconds of simulation.
typedef struct nbStreamDef
{
	/// A site whose center lies within this distance of a viewer is loaded, the nearest first, see nbStream_Update.
	float loadRadius;

	/// A site goes again only beyond the load radius plus this margin, so one at the edge does not come and go.
	float unloadMargin;

	/// A body that moves loads the sites whose bounds lie within this distance of its center, before it can reach them.
	float activeLoadRadius;

	/// A body that moves keeps the sites whose bounds lie within this distance of its center from going.
	float activeHoldRadius;

	/// How long no body may have moved within activeHoldRadius of a site before it goes.
	float holdTime;

	/// A site that could not go yet is asked again after this time.
	float retryTime;

	/// Nothing that moves or can be moved may lie within this distance of a site that goes, see nbDestructible_CanUnload
	/// and nbCanSaveDestructibles.
	float margin;

	/// Sites that start to load per update, the nearest first, and sites that go per update, of those at most saveBudget
	/// damaged ones, which go with their rubble.
	int loadBudget;
	int unloadBudget;
	int saveBudget;

	/// Updates from the start of a load in the background to the update that builds it into the world, see
	/// nbStartCreating. One suits a game, the workers have a frame. More suit updates that come back to back.
	int loadDelay;

	/// The definitions of the sites, required, and the release of what prepareFcn gave, optional.
	nbSitePrepareFcn* prepareFcn;
	nbSiteReleaseFcn* releaseFcn;

	/// Where the damaged sites go with their rubble, both or neither. Without, the stream keeps them in memory.
	nbSiteStoreFcn* storeFcn;
	nbSiteFetchFcn* fetchFcn;

	/// Passed to the functions above.
	void* userContext;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} nbStreamDef;

/// Counters and timings of a stream.
typedef struct nbStreamStats
{
	int siteCount;

	/// Sites in the world now, of those the damaged ones, sites the workers prepare, and damaged sites out of the world
	/// with their rubble
	int loadedCount;
	int damagedCount;
	int loadingCount;
	int savedCount;

	/// Totals since the stream was created: sites built into the world, of those the ones a moving body asked for, intact
	/// sites that went, damaged ones that went with their rubble, and saved ones that came back.
	int loads;
	int urgentLoads;
	int unloads;
	int saves;
	int restores;

	/// Bytes the stream keeps for its saved sites, zero with a store of the program
	size_t savedBytes;

	/// The last nbStream_Update and the marks of the moving bodies in the last step, in milliseconds
	float updateTime;
	float markTime;
} nbStreamStats;

/** @} */

/**
 * @defgroup geometry Chunk geometry
 * @{
 */

/// A polygon face of a chunk. Its plane is in nbGeometry::planes.
typedef struct nbFace
{
	/// First index into the index array.
	uint16_t firstIndex;

	/// Number of vertices of this convex polygon, counter clockwise seen from outside.
	uint8_t indexCount;

	/// Render material. The surface material of the piece or the interior material for fracture faces.
	uint8_t material;
} nbFace;

/// Read-only view of the convex polyhedron of a chunk. The data lives as long as the chunk.
typedef struct nbGeometry
{
	const b3Vec3* vertices;

	/// Outward plane of every face in the local frame of the chunk body, one per face
	const b3Plane* planes;

	const nbFace* faces;
	const uint8_t* indices;
	int vertexCount;
	int faceCount;
	int indexCount;
} nbGeometry;

/** @} */
