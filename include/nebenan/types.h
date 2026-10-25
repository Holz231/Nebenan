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

/// A figure of rigid bodies held together by joints, a ragdoll, see nbCreateArticulation.
typedef struct nbArticulationId
{
	int32_t index1;
	uint16_t world0;
	uint16_t generation;
} nbArticulationId;

static const nbWorldId nb_nullWorldId = NB_ZERO_INIT;
static const nbDestructibleId nb_nullDestructibleId = NB_ZERO_INIT;
static const nbChunkId nb_nullChunkId = NB_ZERO_INIT;
static const nbCreationId nb_nullCreationId = NB_ZERO_INIT;
static const nbStreamId nb_nullStreamId = NB_ZERO_INIT;
static const nbArticulationId nb_nullArticulationId = NB_ZERO_INIT;

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

	/// The parts of the solver setup, see b3Profile::setupPacks: the islands in packs on the calling thread, a task on
	/// a worker that gives every body and contact its pack and puts the contacts in order, meanwhile the knots of the
	/// overflow on the calling thread, and its wait for the task
	nb_profileSetupPacks,
	nb_profileSetupOrder,
	nb_profileSetupKnots,
	nb_profileSetupWait,
	nb_profileConstraints,

	/// The part of the constraints that the calling thread spends on the overflow of Box3D's constraint graph, in the
	/// steps that solve by colors, alone but for the groups of knots that the threads solve at once, see
	/// b3Profile::overflow
	nb_profileOverflow,
	nb_profileTransforms,

	/// nbWorld_Update with its collision damage, the debris that comes to rest, the debris budget, the rubble that lost
	/// what held it and the figures
	nb_profileUpdate,
	nb_profileCollisionDamage,
	nb_profileSettle,
	nb_profileBudget,
	nb_profileRelease,

	/// The figures in the update on the calling thread, see nbCreateArticulation and the roadmap of the game, 27.14: their
	/// clips with their recordings, their balance with what it measures and plans and the legs it moves, and the targets
	/// of their joints, which go to Box3D. Their joints and contacts cost in the step.
	nb_profileFigures,
	nb_profileFigureClips,
	nb_profileFigureBalance,
	nb_profileFigureMotors,

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

	/// Where the constraint solver of Box3D works, measured in every eighth step, see b3Profile::solverJoints: the steps
	/// measured, and per such step over all threads the time it solves joints, contacts and the bodies, in milliseconds of
	/// CPU time.
	int solverSampleCount;
	float solverJoints;
	float solverContacts;
	float solverBodies;

	/// The steps in which Box3D solved whole islands in packs, one thread each without a barrier per stage, with a team for
	/// the largest packs or without, see b3Counters::packCount and b3SolvePacks in extern/box3d/src/solver.c. The others
	/// solved color by color with a barrier per stage, for example without workers, with contacts on meshes or with a
	/// single awake island.
	int packStepCount;

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
 * @defgroup articulation Articulation
 * Figures of Box3D bodies and joints that follow a pose: ragdolls with muscles. A rig describes the bones of a figure at
 * rest, an articulation is one figure made from it in the world. Every joint is a spring to a target rotation with a
 * torque limit, so the figure holds its pose but gives way to anything stronger, and nothing teleports it.
 * @{
 */

/// How a bone is joined to its parent
typedef enum nbJointType
{
	/// No motion against the parent, the bone is welded on
	nb_fixedJoint,

	/// One rotation about the z axis of the joint frame, within limits: a knee, an elbow
	nb_hingeJoint,

	/// Rotation about the pivot: a swing of the bone out of its rest direction, the z axis of the joint frame, within a
	/// cone, and a twist about that axis within limits: a hip, a shoulder, the neck
	nb_ballJoint,
} nbJointType;

/// The collision shape of a bone
typedef enum nbBoneShape
{
	/// A capsule between two points with a radius
	nb_capsuleBone,

	/// A box around a center, for a foot that needs a flat sole
	nb_boxBone,
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
	/// pose of the articulation instead.
	b3Transform restTransform;

	/// The collision shape, in the frame of the bone
	nbBoneShape shape;

	/// Capsule: the centers of its two ends and its radius
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

	/// Scales the torque limit of the motor profile for this joint, 1 by default: strong legs, a weak neck
	float strength;
} nbBoneDef;

/// A rig: the bones of a figure at rest, shared by all articulations made from it. Initialize with nbDefaultRigDef.
typedef struct nbRigDef
{
	/// The bones, the root first and every parent before its children
	const nbBoneDef* bones;
	int boneCount;

	/// Friction of all shapes
	float friction;

	/// The axis of the root that points up when the figure stands, y by default, see nbMotorProfile::uprightTorque
	b3Vec3 up;

	/// Bones that are not joined to each other collide. Off, no bone of a figure collides with another of the same
	/// figure. Joined bones never collide with each other.
	bool selfCollision;

	/// The bones damage the destructibles they hit, like cannonballs, see nbWorldDef::collisionDamageScale
	bool enableCollisionDamage;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} nbRigDef;

/// How the joints drive a figure to its target pose, see nbArticulation_SetMotorProfile. Every joint is a spring to its
/// target with a torque limit. All zero is a passive ragdoll, see nbPassiveMotorProfile.
typedef struct nbMotorProfile
{
	/// Stiffness of every joint in newton meters per radian of error, the same for a hip and a wrist. Zero turns the
	/// motors off. Box3D sets a joint spring in cycles per second against the inertia of its two bones, so the library
	/// turns this into cycles per second for every joint from the inertia of its bones, in the pose they have when the
	/// profile is set. An ankle in quiet standing holds about 800.
	float stiffness;

	/// Damping ratio of every joint, 1 is critical
	float dampingRatio;

	/// The most torque a joint applies in newton meters, times the strength of its bone. A spring this stiff reaches it
	/// after a few degrees, from there the joint gives way.
	float maxTorque;

	/// The most torque that holds the root upright, in newton meters, zero for none. This is an external torque against
	/// the world, no muscle: a stand-in for balance until the figure steps to keep it, see the roadmap. A push stronger
	/// than it topples the figure. Say so to the player, it is a game aid, not physics.
	float uprightTorque;

	/// Stiffness in newton meters per radian and damping of the upright hold
	float uprightStiffness;
	float uprightDampingRatio;
} nbMotorProfile;

/// A hit on a bone, see nbArticulation_ApplyHit
typedef struct nbHitDef
{
	/// Number of the hit event. A hit with the number of one of the last 16 hits the figure took is ignored, so a hit that
	/// several events report lands once. A hit with zero always lands.
	uint32_t id;

	/// The bone that is hit
	int bone;

	/// Impulse in newton seconds, in world space
	b3Vec3 impulse;

	/// Where it hits, in world space
	b3Pos point;
} nbHitDef;

/// A pose at a time, see nbClipDef
typedef struct nbKeyframe
{
	/// Seconds from the start of the clip
	float time;

	/// The rotation of every bone against its rest pose, in the frame of the bone, one per bone of the rig. The root is
	/// not driven, its rotation only places the target pose that is drawn.
	const b3Quat* rotations;
} nbKeyframe;

/// A clip of keyframes, sorted by time. Between two keyframes the rotations are interpolated.
typedef struct nbClipDef
{
	const nbKeyframe* keyframes;
	int keyframeCount;

	/// Bones per keyframe, the bones of the rig
	int boneCount;

	/// Start over at the end, else the clip ends on its last pose
	bool loop;
} nbClipDef;

/// What the balance of a figure does, see nbArticulation_EnableBalance
typedef enum nbBalanceState
{
	/// No balance: the figure follows its motors and its clip, or hangs passive
	nb_balanceOff,

	/// The feet carry the figure and the capture point lies within their support
	nb_balanceStanding,

	/// A foot swings to a free spot where the support catches the capture point
	nb_balanceStepping,

	/// Balance is lost: the upright hold lets go, the motors turn soft and hold a protective pose
	nb_balanceFalling,

	/// The figure lies at rest and waits until it can get up safely
	nb_balanceDown,

	/// The figure gets up with a clip: its joints lift it and a bounded hold turns its root along the clip, see
	/// nbBalanceDef::getUpClips
	nb_balanceGettingUp,

	/// The number of states
	nb_balanceStateCount
} nbBalanceState;

/// How a biped keeps its balance, see nbArticulation_EnableBalance. Initialize with nbDefaultBalanceDef, which fits the
/// humanoid of nbMakeHumanoidBones.
typedef struct nbBalanceDef
{
	/// The bones of the legs, left then right. The thighs hang from the root, the pelvis, the shins from the thighs on a
	/// hinge about their local x axis, the feet from the shins. Thighs and shins point along their local y axis to the
	/// next joint, the feet are boxes whose bottom is the sole. The pelvis faces its local z axis. The humanoid of
	/// nbMakeHumanoidBones is built that way.
	int thighs[2];
	int shins[2];
	int feet[2];

	/// Bones that the protective pose of a fall and the get-up clips of the library move, -1 for none: the torso, the head,
	/// the upper arms and forearms, left then right.
	int torso;
	int head;
	int upperArms[2];
	int forearms[2];

	/// A step starts once the capture point lies this far outside the support of the feet in two updates in a row, in
	/// meters. After a step the figure stands again once the capture point lies within this margin.
	float stepMargin;

	/// The farthest an ankle steps from under its hip, in meters
	float maxStepLength;

	/// Seconds a step takes, and how high the foot swings, in meters
	float stepTime;
	float stepHeight;

	/// Steps in a row before the figure gives up and falls
	int maxSteps;

	/// The figure falls once its pelvis tilts this far from upright, in radians, or its center of mass sinks below this
	/// part of its height standing
	float fallAngle;
	float fallHeight;

	/// Motors while the figure falls and lies, softer than standing. A falling figure has no upright hold.
	nbMotorProfile fallProfile;

	/// The figure gets up on its own once it lay still this long, in seconds, and the place is safe: ground under it,
	/// room to stand and nothing on it or moving against it. Zero, it stays down.
	float getUpDelay;

	/// The clips the figure gets up with, from its front and from its back, null for those of the library, see
	/// nbMakeGetUpClip. A get-up clip drives the joints like any clip, and the rotation of its root, which plays no part in
	/// other clips, is where the hold of getting up turns the root: in the frame of the figure lying with its head toward
	/// where it will stand up facing, y up and z ahead. Its first keyframe lies, its last one stands on both feet. The
	/// clips are copied.
	const nbClipDef* getUpClips[2];

	/// How fast the get-up clips play, 1 as authored. A clip waits while the root lags behind the hold, the figure falls
	/// once its root falls out of the clip, and it gives up and lies again once a clip took twice its length.
	float getUpSpeed;

	/// The most torque of the hold of getting up, in newton meters: a motor joint against the world that turns the root
	/// along the clip, its heading too. The joints lift the figure, the hold keeps its root on course, a game aid like the
	/// upright hold.
	float getUpTorque;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} nbBalanceDef;

/// The balance of a figure as the last update saw it, see nbArticulation_GetBalance
typedef struct nbBalance
{
	nbBalanceState state;

	/// Seconds in this state
	float stateTime;

	/// The center of mass of the figure and its velocity
	b3Pos center;
	b3Vec3 velocity;

	/// Where the center of mass would come to rest over the ground if the figure stopped it, on the ground: the center of
	/// mass ahead by its velocity over the frequency of a pendulum as long as its height, see the roadmap 27.9
	b3Pos capturePoint;

	/// How far the capture point lies outside the support, in meters, negative inside. FLT_MAX without a foot on the
	/// ground.
	float captureMargin;

	/// The support: the corners of the soles on the ground, counter clockwise around up
	b3Pos support[8];
	int supportCount;

	/// Which feet carry the figure, left and right
	bool footContact[2];

	/// The foot that steps, 0 left, 1 right, -1 none, and where its ankle lands
	int stepFoot;
	b3Pos stepTarget;

	/// Steps in a row since the figure last stood
	int stepCount;

	/// The figure lies on its back, else on its front or side
	bool onBack;

	/// Getting up, the clip that plays, 0 from the front, 1 from the back, and how far it played, in seconds of the clip,
	/// see nbBalanceDef::getUpClips. Zero in the other states.
	int getUpClip;
	float getUpTime;

	/// Since the figure got balance: its falls, the times it got up and stood, and the times getting up ended in a fall
	int fallCount;
	int getUpCount;
	int failedGetUpCount;
} nbBalance;

/// What the figures of a world do, see nbWorld_GetFigureStats and the roadmap of the game, 27.14 and 32.8
typedef struct nbFigureStats
{
	/// The figures, their bones, their joints with the upright hold, the bones that are awake, and the figures that play
	/// a clip
	int figureCount;
	int boneCount;
	int jointCount;
	int awakeBoneCount;
	int playingCount;

	/// The figures by the state of their balance, see nbBalanceState, those without balance under nb_balanceOff
	int stateCounts[nb_balanceStateCount];

	/// The falls of the figures there are, the times they got up and stood, and the times getting up ended in a fall,
	/// see nbBalance::fallCount
	int fallCount;
	int getUpCount;
	int failedGetUpCount;

	/// The joints whose motor works at its torque limit, see nbArticulation_GetMotorLoad, and the largest error of a
	/// joint from its target, in radians, see nbArticulation_GetJointError
	int saturatedJointCount;
	float largestJointError;
} nbFigureStats;

/// The bones of the figure that nbMakeHumanoidBones builds, in the order of its array
typedef enum nbHumanoidBone
{
	nb_humanoidPelvis,
	nb_humanoidTorso,
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

/// The skeleton of the humanoid of nbMakeHumanoidBones in the files of clips, see nbCreatePoseClip
#define NB_HUMANOID_SKELETON "nebenan-humanoid"

/// The most keyframes of a get-up clip of the library, see nbMakeGetUpClip
#define NB_GETUP_KEYFRAMES 24

/** @} */

/**
 * @defgroup poseclips Authored clips
 * Clips of keyframes as files, authored by hand, from a pose estimator or with an assistant, see the roadmap of the game,
 * chapters 32.5, 32.6 and 33.2. Approved keyframes are locked: nothing but an explicit unlock changes them.
 * @{
 */

/// An authored clip, see nbCreatePoseClip. A document of its own, no part of a world.
typedef struct nbPoseClip nbPoseClip;

/// What a keyframe is: a proposal, which tools and assistants may change, or approved, which nothing changes until it is
/// unlocked
typedef enum nbKeyframeStatus
{
	nb_keyframeProposed,
	nb_keyframeApproved,
} nbKeyframeStatus;

/// Where a keyframe came from
typedef enum nbKeyframeOrigin
{
	/// Posed by hand in an editor
	nb_originHand,

	/// Proposed by a pose estimator from a reference video, see nbHumanoidPoseFromKeypoints
	nb_originEstimator,

	/// Proposed by an assistant, an AI that suggests poses
	nb_originAssistant,

	/// Recorded from a simulated figure, see nbPoseClip_RecordArticulation
	nb_originSimulation,

	nb_originCount
} nbKeyframeOrigin;

/// What a clip holds, see the roadmap 32.6: poses authored by hand or with tools, poses computed from them, or the poses
/// a simulation reached
typedef enum nbPoseClipKind
{
	nb_clipAuthored,
	nb_clipComputed,
	nb_clipSimulated,
	nb_clipKindCount
} nbPoseClipKind;

/// Contacts of a keyframe, bit flags: the feet and hands that touch the ground or a hold. Two keyframes in a row with the
/// same contact make an interval in which that foot or hand does not slide.
typedef enum nbContact
{
	nb_contactLeftFoot = 1,
	nb_contactRightFoot = 2,
	nb_contactLeftHand = 4,
	nb_contactRightHand = 8,
} nbContact;

/// A keyframe of an authored clip, as nbPoseClip_GetKeyframe reads it. The arrays belong to the clip and stay valid until
/// the clip changes.
typedef struct nbPoseKeyframe
{
	/// When, in frames of the clip, see nbPoseClip_GetFrameRate
	int frame;

	nbKeyframeStatus status;
	nbKeyframeOrigin origin;

	/// The frame of the reference video the pose was taken from, -1 for none
	int sourceFrame;

	/// The root bone relative to the origin of the clip. Shown and checked, but a figure that plays the clip does not
	/// follow it: the physics moves the root, see the roadmap 31.3.
	b3Transform root;

	/// The feet and hands in contact, see nbContact
	uint32_t contacts;

	/// The rotation of every bone against its rest pose, in the frame of the bone, as nbKeyframe has it. The root's is
	/// unused, the root takes its rotation from root.
	const b3Quat* rotations;

	/// How sure an estimator was of every bone, from 0 to 1, 1 for poses by hand
	const float* confidence;
} nbPoseKeyframe;

/// An event of a clip: a name at a frame, like the start of a phase of an attack, windup, impact, followthrough and
/// recovery, see the roadmap 33.2
typedef struct nbPoseEvent
{
	int frame;
	char name[32];
} nbPoseEvent;

/// Why a text is no clip, see nbParsePoseClip
typedef struct nbPoseClipError
{
	int line;
	int column;
	char message[128];
} nbPoseClipError;

/// What nbPoseClip_Check finds
typedef enum nbPoseIssueKind
{
	/// The rig has a bone the clip lacks. The clip does not fit the rig.
	nb_issueBoneMissing,

	/// The clip has a bone the rig lacks, or its bones come in another order. The clip does not fit the rig.
	nb_issueBoneUnknown,

	/// A rotation is not of unit length
	nb_issueNotNormalized,

	/// A joint is turned beyond its limits: the motors cannot follow there, the figure stops at the limit
	nb_issueJointLimit,

	/// An estimator was unsure of a bone
	nb_issueUnsure,

	/// A bone reaches into the ground
	nb_issueGround,

	/// A bone turns, or the root moves or turns, faster between two keyframes than a body can
	nb_issueJump,

	/// A foot or hand in contact moves away from where it touched, see nbPoseCheckDef::slideDistance
	nb_issueSliding,

	/// Keypoints give a bone a length that does not fit the figure, see nbHumanoidPoseFromKeypoints
	nb_issueBoneLength,

	nb_issueKindCount
} nbPoseIssueKind;

/// A finding of nbPoseClip_Check
typedef struct nbPoseIssue
{
	nbPoseIssueKind kind;

	/// The keyframe, -1 for the clip as a whole, and the bone, -1 for none
	int keyframe;
	int bone;

	/// How far off: radians beyond a limit, the confidence, meters into the ground or slid, radians or meters per second of
	/// a jump, the ratio of a length to the length it should have
	float value;
} nbPoseIssue;

/// What nbPoseClip_Check lets pass. Initialize with nbDefaultPoseCheckDef.
typedef struct nbPoseCheckDef
{
	/// The height of the ground in the frame of the clip, along y, and how far a bone may reach into it, in meters
	float groundHeight;
	float groundTolerance;

	/// A bone of less confidence is unsure
	float minConfidence;

	/// The most a bone turns, in radians per second, and the most the root moves, in meters per second, and turns, in
	/// radians per second, between two keyframes
	float maxTurnSpeed;
	float maxRootSpeed;
	float maxRootTurnSpeed;

	/// How far a foot or hand in contact may move from where it touched, over all keyframes it stays in contact, in
	/// meters. A slide found, it touches where it is then.
	float slideDistance;

	/// How far a joint may turn beyond its limits, in radians
	float limitTolerance;

	/// The feet and hands of the contacts, left then right, -1 for none. Those of the humanoid by default.
	int feet[2];
	int hands[2];

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} nbPoseCheckDef;

/// How a pose estimator numbers its keypoints, see nbHumanoidPoseFromKeypoints
typedef enum nbKeypointLayout
{
	/// The 17 points of COCO: nose, eyes, ears, shoulders, elbows, wrists, hips, knees and ankles, no hands and feet
	nb_keypointsCoco17,

	/// The 33 points of MediaPipe Pose, with hands, heels and toes
	nb_keypointsMediaPipe33,

	nb_keypointLayoutCount
} nbKeypointLayout;

/** @} */
