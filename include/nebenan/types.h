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

/// A figure of bones on joints that an animation moves and that falls as an active ragdoll when hit hard, see
/// nbCreateFigure.
typedef struct nbFigureId
{
	int32_t index1;
	uint16_t world0;
	uint16_t generation;
} nbFigureId;

static const nbWorldId nb_nullWorldId = NB_ZERO_INIT;
static const nbDestructibleId nb_nullDestructibleId = NB_ZERO_INIT;
static const nbChunkId nb_nullChunkId = NB_ZERO_INIT;
static const nbCreationId nb_nullCreationId = NB_ZERO_INIT;
static const nbStreamId nb_nullStreamId = NB_ZERO_INIT;
static const nbFigureId nb_nullFigureId = NB_ZERO_INIT;

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
	/// anchors falls as one piece, and a chunk crushed under too much weight bursts into fragments. A part of such a
	/// structure that lands hard breaks in two, with or without the check. Lower values let structures give way
	/// sooner, one is the default. Zero turns the check off: a structure then only loses what is cut off from its
	/// anchors. Change it at run time with nbWorld_SetSupportScale.
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
	/// the physics runs. The thread that called it waits for the step as soon as it calls a function of Nebenan or
	/// Box3D for the worlds again. Nothing else changes, the worlds end up the same to the bit as with the step on the
	/// calling thread. The allocators of Nebenan and Box3D must be thread safe. The workers of a step wait for their
	/// next task without sleeping, so leave the thread that draws a hardware thread of its own. It pays off when the
	/// program has enough to do meanwhile: the threads of the step share the cores with the program, and a stage of the
	/// step waits for its slowest worker, so the step loses time while the program runs. A program that does little
	/// between its steps runs faster without. On by default, see nbWorld_SetAsyncStep.
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

	/// Parts of structures without floors that broke in two where they landed hard, see nbWorldDef::supportScale. Total
	/// since the world was created.
	int landingSplitCount;

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

	/// The parts of the contact loop before the contacts that begin or end touching, see b3Profile::gatherContacts: the
	/// calling thread gathers the awake contacts and clears the bits of the workers, the threads update the contacts, and
	/// the calling thread merges what they found
	nb_profileGatherContacts,
	nb_profileUpdateContacts,
	nb_profileMergeContacts,
	nb_profileContactStates,
	nb_profileSolve,
	nb_profileSolverSetup,

	/// The parts of the solver setup, see b3Profile::setupPacks: the islands in packs on the calling thread, two tasks on
	/// workers that give every body and contact its pack and put the contacts in order, meanwhile the knots of the
	/// overflow on the calling thread, and its waits for the tasks
	nb_profileSetupPacks,
	nb_profileSetupTasks,
	nb_profileSetupKnots,
	nb_profileSetupWait,
	nb_profileConstraints,

	/// The part of the constraints that the calling thread spends on the overflow of Box3D's constraint graph, in the
	/// steps that solve by colors, alone but for the groups of knots that the threads solve at once, see
	/// b3Profile::overflow
	nb_profileOverflow,
	nb_profileTransforms,

	/// nbWorld_Update with its collision damage, the debris that comes to rest, the debris budget and the rubble that
	/// lost what held it
	nb_profileUpdate,
	nb_profileCollisionDamage,
	nb_profileSettle,

	/// The parts of the checks of the debris that comes to rest: the calling thread gathers the moving debris, the threads
	/// check it, then the calling thread searches what carries the quiet pieces and freezes them, with the type changes of
	/// the freezes, see nbProfile::settleWork
	nb_profileGatherMoving,
	nb_profileCheckMoving,
	nb_profileSearchSupport,
	nb_profileFreezeRubble,
	nb_profileBudget,
	nb_profileRelease,

	/// The figures in the update: their clips, the targets of their bones and joints, their hits and whether they get up,
	/// see nbCreateFigure. Their bodies, contacts and joints cost in the step.
	nb_profileFigures,

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

	/// The steps in which a team of all workers solved the largest packs of islands stage by stage, while they solved the
	/// other packs whole in between, see b3Counters::teamWork, and the share of those packs in the work of the solver, mean
	/// over those steps. Box3D takes a team once the largest pack holds clearly more than the even share of a worker, and
	/// with it every further pack above that share. teamJoinStepCount counts the steps with more than one pack in the team.
	int teamStepCount;
	float teamShare;
	int teamJoinStepCount;

	/// The constraints in the overflow of Box3D's constraint graph per step, the contacts of them that Box3D solves in
	/// batches of knots, several at once, and those batches, see b3Counters::knotContactCount
	float overflowCount;
	float knotContactCount;
	float knotBatchCount;

	/// Per step the batches of knots that one thread solves in a row, the most in a group, and the copies of the knots in
	/// several groups, see b3Counters::knotGroupBatchCount. A large overflow goes into groups that the threads solve at
	/// once.
	float knotGroupBatchCount;
	float knotCopyCount;

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

	/// Where the constraints of the steps with a team go, timed in every eighth of them, see b3Profile::teamTime: the
	/// timed steps, and in milliseconds per timed step the time of the team, the part the calling thread spends on the
	/// overflow of the team's pack and waiting at the end of the stages, and per worker the waits for a new stage and
	/// the time on other packs. Then per timed step the stages with a barrier and how often a worker gave up its core.
	int teamSampleCount;
	float teamTime;
	float teamOverflow;
	float teamStageWait;
	float teamWorkerWait;
	float teamWorkerPacks;
	float teamStageCount;
	float teamYieldCount;

	/// How the timed steps with a team spent the stages, by what they solve and by color, mean per timed step, see
	/// b3TeamStages: how long the stages were open and how much of that the threads solved blocks or other packs,
	/// waited before they came in or after their last block, what the calling thread solved alone, and how late the
	/// workers came in.
	b3TeamStages teamStages;

	/// How long the calling thread waits for the last packs of islands, milliseconds per step
	float packWait;

	/// What a unit of the solver's work costs, sampled in every eighth step, see b3Counters::solverSample: the sampled
	/// steps, and per sampled step the work of the awake islands, see b3Counters::islandWork, the milliseconds of the
	/// constraints on the calling thread, the milliseconds all threads spent solving whole packs of islands, one thread a
	/// pack, and the work of those packs. teamSampleWork is the work of the team per step that timed it, see
	/// teamSampleCount and b3Counters::teamWork, for the time of its blocks in teamStages.
	int solverSampleCount;
	float solverWork;
	float solverConstraints;
	float packSolveTime;
	float packSolveWork;
	float teamSampleWork;

	/// How the threads spent the contact loop, timed in every eighth step, see b3Profile::collideWork: the timed steps, and
	/// in milliseconds per timed step the loop on the calling thread and, summed over its tasks, the time in blocks, of it
	/// the checks of the contacts and the narrowphase, the time before a task started, between its blocks and after it
	/// found none left, then the longest block. Then per timed step the tasks, the blocks, the tasks that found no block
	/// left, the awake contacts and those of them that needed the narrowphase.
	int collideSampleCount;
	float collideLoop;
	float collideWork;
	float collideChecks;
	float collideNarrowphase;
	float collideLate;
	float collideClaim;
	float collideDone;
	float collideLongestBlock;
	float collideTaskCount;
	float collideBlockCount;
	float collideMissedCount;
	float collideContactCount;
	float collideNarrowphaseCount;

	/// How the threads spent the checks of the update, timed in every eighth update, see nb_profileCheckMoving: the timed
	/// updates, and in milliseconds per timed update the checks on the calling thread and, summed over their tasks, the
	/// time in blocks, of it the look at what carries the quiet pieces, the time before a task started, between its blocks
	/// and after it found none left, then the longest block. Then per timed update the tasks, the blocks, the tasks that
	/// found no block left, the moving pieces, those of them that were quiet and those the calling thread checked.
	int settleSampleCount;
	float settleCall;
	float settleWork;
	float settleSupportWork;
	float settleLate;
	float settleClaim;
	float settleDone;
	float settleLongestBlock;
	float settleTaskCount;
	float settleBlockCount;
	float settleMissedCount;
	float settleMovingCount;
	float settleQuietCount;
	float settleCallerCount;

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

/**
 * @defgroup figure Figure
 * Figures after the hybrid of Overgrowth, see the roadmap of the game, chapters 26 to 28 and 31 to 33. A rig describes
 * the bones at rest, a figure is one made from it in the world. Normally a figure is animated: its bones are kinematic
 * Box3D bodies that follow its clip, debris bounces off them, and Box3D solves none of its joints. A hard hit, a blast,
 * heavy debris or ground that gives way turn it into an active ragdoll: the bones become dynamic with the velocities they
 * had, the hit acts once, spread over the bones near it, and the joints are springs with a torque limit that pull toward
 * a pose to fall with, as strong as the figure is fast: flailing while it flies fast, relaxed while it is slow, curled up
 * before it hits something and with the arms out ahead before it lands on its front. Lying still, it
 * gets up with a clip from its front or its back once it finds ground, room and nothing on it, blended from the pose it
 * lies in, and stands animated again. Nothing is teleported: a ragdoll moves by the physics alone, an animated figure
 * moves its bodies with velocities.
 * @{
 */

/// How a bone is joined to its parent
typedef enum nbJointType
{
	/// No motion against the parent, the bone is welded on
	nb_fixedJoint,

	/// One rotation about the z axis of the joint frame, within limits: a knee, an elbow
	nb_hingeJoint,

	/// Rotation about the pivot: a swing of the z axis of the joint frame within a cone, and a twist about that axis within
	/// limits: a hip, a shoulder, the neck
	nb_ballJoint,
} nbJointType;

/// The collision shape of a bone
typedef enum nbBoneShape
{
	/// A capsule between two points with a radius
	nb_capsuleBone,

	/// A box around a center, for a foot that needs a flat sole
	nb_boxBone,

	/// A box around a center with its edges and corners rounded by the radius, like the trunk of a mannequin: a convex
	/// hull of 48 points, two faces on every edge, see nbCreateBoneHull
	nb_roundedBoxBone,
} nbBoneShape;

/// A bone of a rig: one rigid body with one shape, joined to its parent at the origin of the bone. Initialize with
/// nbDefaultBoneDef.
typedef struct nbBoneDef
{
	/// Optional name, for debugging
	const char* name;

	/// Index of the parent bone in the rig, -1 for the root. Parents come before their children.
	int parent;

	/// The pose of the bone in the frame of its parent at rest. Its position is the pivot of the joint. The root takes the
	/// pose of the figure instead.
	b3Transform restTransform;

	/// The collision shape, in the frame of the bone
	nbBoneShape shape;

	/// Capsule: the centers of its two ends and its radius. Rounded box: the radius of its edges.
	b3Vec3 start;
	b3Vec3 end;
	float radius;

	/// Box: its center and half extents
	b3Vec3 center;
	b3Vec3 halfExtents;

	/// Mass in kilograms. The density follows from it and the shape.
	float mass;

	/// How the bone is joined to its parent, nothing for the root
	nbJointType joint;

	/// The joint frame in the frame of the bone: its z axis is the hinge axis, or the axis of the cone, which is the rest
	/// direction of the bone. Identity puts the axis along z.
	b3Quat jointFrame;

	/// Hinge: the range of the angle in radians, within -0.99 pi and 0.99 pi. Ball: the range of the twist.
	float lowerAngle;
	float upperAngle;

	/// Ball: how far the bone swings out of its rest direction, in radians, up to 0.5 pi
	float coneAngle;

	/// Scales the torque limit of the motors for this joint, 1 by default: strong legs, a weak neck
	float strength;

	/// Scales the stiffness of the motors for this joint, 1 by default. Joints in a row bend like one joint of a third of
	/// their stiffness, so the three joints of a spine take three times it to hold the trunk like one. A light bone between
	/// two springs, like a neck, takes less: they turn it the faster the lighter it is, and from about half the substeps
	/// per second on it shakes.
	float stiffness;

	/// Scales the damping ratio of the motors for this joint, 1 by default
	float damping;
} nbBoneDef;

/// A rig: the bones of a figure at rest, standing with its y axis up and facing +z, shared by all figures made from it.
/// Initialize with nbDefaultRigDef.
typedef struct nbRigDef
{
	/// The bones, the root first and every parent before its children
	const nbBoneDef* bones;
	int boneCount;

	/// Friction of all shapes
	float friction;

	/// Bones that are not joined to each other collide. Off, no bone of a figure collides with another of the same
	/// figure. Joined bones never collide with each other.
	bool selfCollision;

	/// The bones of a ragdoll damage the destructibles they hit, like cannonballs, see nbWorldDef::collisionDamageScale
	bool enableCollisionDamage;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} nbRigDef;

/// A pose at a time, see nbClipDef
typedef struct nbKeyframe
{
	/// Seconds from the start of the clip
	float time;

	/// The rotation of every bone against its rest pose, in the frame of the bone, one per bone of the rig. The root's
	/// goes unused, its pose is root.
	const b3Quat* rotations;

	/// The root bone in the frame of the figure, see nbClipDef::moveRoot: its position over the point on the ground the
	/// figure stands on, x to its left, y up, z ahead, and its rotation
	b3Transform root;
} nbKeyframe;

/// A clip of keyframes, sorted by time. Between two keyframes the rotations and the root are interpolated.
typedef struct nbClipDef
{
	const nbKeyframe* keyframes;
	int keyframeCount;

	/// Bones per keyframe, the bones of the rig
	int boneCount;

	/// Start over at the end, else the clip ends on its last pose
	bool loop;

	/// The clip moves the root, as getting up does. Without, the root stands upright at the standing height of the
	/// figure, see nbFigureDef::standHeight, and the root of the keyframes goes unused.
	bool moveRoot;
} nbClipDef;

/// The clips of a figure, see nbFigureDef::clips and nbMakeHumanoidClip
typedef enum nbFigureClip
{
	/// While the figure stands, in a loop: it breathes and sways a little
	nb_clipIdle,

	/// The pose an active ragdoll falls with, in a loop, like the flailing of Overgrowth: the arms up and out, the knees
	/// bent. As far as the ragdoll is weak it relaxes toward its rest pose instead, see nbFigureInfo::strength.
	nb_clipFall,

	/// Mixed in while the figure flies at something it does not face: knees to the chest, the forearms before the face,
	/// see nbFigureInfo::protect
	nb_clipCurl,

	/// Mixed in instead of the curl while the figure flies at something with its front: the arms out ahead to catch it,
	/// the back straight, see nbFigureInfo::brace
	nb_clipBrace,

	/// Getting up from the front and from the back: from lying with the head ahead, that is along +z of the figure, to
	/// standing at the height of the idle clip, moving the root
	nb_clipGetUpFront,
	nb_clipGetUpBack,

	nb_figureClipCount
} nbFigureClip;

/// How hard the joints of an active ragdoll pull toward the pose it falls with, after the ragdoll strength of
/// Overgrowth. At strength 1 a joint has the full stiffness and torque limit, at 0 the ragdoll hangs slack.
typedef struct nbFigureMotors
{
	/// Stiffness at strength 1 in newton meters per radian of error, before the scale of the bone. Box3D sets a joint
	/// spring in cycles per second against the inertia of its two bodies, the library turns this into them for every
	/// joint, so a heavy thigh and a light foot hold alike.
	float stiffness;

	/// Damping ratio of the joints, 1 is critical
	float dampingRatio;

	/// The most torque of a joint at strength 1 in newton meters, times the strength of its bone. Beyond it the joint gives
	/// way.
	float maxTorque;

	/// Strength per meter per second of the average speed of the bones, up to fallStrength: a figure that flies holds
	/// its pose, one that slows down goes slack and lies as it fell
	float strengthPerSpeed;
	float fallStrength;

	/// Seconds of slack after a hit that knocked the figure down or met it as a ragdoll: the strength starts that much
	/// lower and comes back at one per second, so the hit throws a slack body
	float stunTime;
} nbFigureMotors;

/// A figure, see nbCreateFigure. Initialize with nbDefaultFigureDef.
typedef struct nbFigureDef
{
	/// The bones. The figure keeps what it needs, the rig can go afterwards.
	const nbRigDef* rig;

	/// The point on the ground the figure stands on, and where it faces: radians about up, zero facing +z
	b3Pos position;
	float heading;

	/// The height of the root bone over the ground while the figure stands, see nbMakeHumanoidBones
	float standHeight;

	/// The clips, copied. Null takes the clip of the library for the humanoid of nbMakeHumanoidBones, see
	/// nbMakeHumanoidClip. A rig of other bones without a clip holds its rest pose there.
	const nbClipDef* clips[nb_figureClipCount];

	/// The motors of the ragdoll
	nbFigureMotors motors;

	/// A hit of at least this many newton seconds knocks the figure down. A weaker one makes it flinch.
	float knockdownImpulse;

	/// How far the trunk of a standing figure flinches from a hit, in radians per newton second, up to flinchLimit, and
	/// how fast it straightens again, in cycles per second
	float flinchScale;
	float flinchLimit;
	float flinchHertz;

	/// Bodies that press on a standing figure from above with at least this many newtons knock it down, so what falls on
	/// it never comes to rest on it. A figure that gets up falls from a quarter of it. What its bones push aside does not
	/// count.
	float crushForce;

	/// A ragdoll counts as lying still while its bones move slower than this on average, in meters per second
	float restSpeed;

	/// Getting up: not before this many seconds as a ragdoll and a quarter second lying still, at this speed of the
	/// get-up clips, 1 as authored. Without canGetUp a ragdoll stays down.
	float getUpDelay;
	float getUpSpeed;
	bool canGetUp;

	/// Start as an active ragdoll, falling from the pose it is placed in
	bool startAsRagdoll;

	/// Free for the program, see nbFigure_GetUserData
	void* userData;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} nbFigureDef;

/// What a figure does, see nbFigure_GetState
typedef enum nbFigureState
{
	/// The bones follow the clip, kinematic
	nb_figureAnimated,

	/// The bones are dynamic, the joints pull toward the pose to fall with
	nb_figureRagdoll,

	/// The bones are kinematic again and follow a get-up clip, blended from the pose the ragdoll lay in
	nb_figureGettingUp,
} nbFigureState;

/// Why a lying figure does not get up yet, see nbFigureInfo::wait
typedef enum nbFigureWait
{
	nb_waitNone,

	/// Not long enough a ragdoll, or it still moves
	nb_waitMoving,

	/// No ground that carries it under its pelvis
	nb_waitGround,

	/// Something stands where it would get up
	nb_waitRoom,

	/// Something lies on it
	nb_waitDebris,

	/// It may not, see nbFigureDef::canGetUp
	nb_waitStaysDown,
} nbFigureWait;

/// The state of a figure as the last update left it, see nbFigure_GetInfo
typedef struct nbFigureInfo
{
	nbFigureState state;

	/// Seconds in this state
	float stateTime;

	/// The strength of the joints of the ragdoll, from 0 to 1, see nbFigureMotors
	float strength;

	/// How far the ragdoll protects itself from what it flies at, from 0 to 1, and how much of that it braces with its
	/// arms ahead. The rest of it, protect minus brace, it curls up. See nb_clipCurl and nb_clipBrace.
	float protect;
	float brace;

	/// The average speed of the bones, weighted by their mass, in meters per second
	float speed;

	/// Seconds the ragdoll has been lying still, see nbFigureDef::restSpeed
	float restTime;

	/// A lying figure lies on its back and gets up with nb_clipGetUpBack
	bool onBack;

	/// Why a lying figure does not get up yet
	nbFigureWait wait;

	/// Seconds into the get-up clip, and how far it blended from the pose the ragdoll lay in, from 0 to 1
	float getUpTime;
	float getUpBlend;

	/// The flinch of the trunk of a standing figure, as a rotation vector in radians
	b3Vec3 flinch;

	/// Hits that landed, how often the figure was knocked down, got up and broke off getting up
	int hitCount;
	int knockdownCount;
	int getUpCount;
	int abortCount;
} nbFigureInfo;

/// A hit on a bone, see nbFigure_ApplyHit
typedef struct nbHitDef
{
	/// Number of the hit event. A hit with the number of one of the last 16 hits the figure took is ignored, so a hit that
	/// several events report lands once. A hit with zero always lands.
	uint32_t id;

	/// The bone that is hit
	int bone;

	/// Impulse in newton seconds, in world space. It spreads from the point over the bones near it, see
	/// nbFigure_ApplyHit.
	b3Vec3 impulse;

	/// Where it hits, in world space
	b3Pos point;
} nbHitDef;

/// What the figures of a world do, see nbWorld_GetFigureStats
typedef struct nbFigureStats
{
	int figureCount;
	int animatedCount;
	int ragdollCount;
	int gettingUpCount;

	/// Ragdolls whose bones are awake in Box3D. Their joints keep Box3D from solving whole islands per thread, see
	/// "Gelenke und Pakete" in the README.
	int awakeRagdollCount;

	/// Since the figures were made: hits that landed, knockdowns, get-ups and get-ups broken off
	int hitCount;
	int knockdownCount;
	int getUpCount;
	int abortCount;
} nbFigureStats;

/// The bones of the humanoid that nbMakeHumanoidBones builds, in the order of its array. The trunk runs from the pelvis
/// through belly and ribs to the chest, which carries the arms and on the neck the head.
typedef enum nbHumanoidBone
{
	nb_humanoidPelvis,
	nb_humanoidBelly,
	nb_humanoidRibs,
	nb_humanoidChest,
	nb_humanoidNeck,
	nb_humanoidHead,
	nb_humanoidUpperArmLeft,
	nb_humanoidForearmLeft,
	nb_humanoidHandLeft,
	nb_humanoidUpperArmRight,
	nb_humanoidForearmRight,
	nb_humanoidHandRight,
	nb_humanoidThighLeft,
	nb_humanoidShinLeft,
	nb_humanoidFootLeft,
	nb_humanoidThighRight,
	nb_humanoidShinRight,
	nb_humanoidFootRight,
	nb_humanoidBoneCount
} nbHumanoidBone;

/// The most keyframes of a clip of the library, see nbMakeHumanoidClip
#define NB_HUMANOID_CLIP_KEYFRAMES 96

/** @} */
