// SPDX-FileCopyrightText: 2025 Erin Catto
// SPDX-License-Identifier: MIT

// Solver work is partitioned into fixed-size blocks that worker threads claim
// in parallel via atomic CAS on a per-block syncIndex. The descriptor (b3SolverBlock)
// and the atomic counter sit in a wrapping b3SyncBlock so the CAS-winner can
// pass the descriptor by value into stage tasks without aliasing the atomic
// memory other threads are CAS-writing. Three properties of this design
// matter for performance:
//
// 1. Distributed contention. Per-block atomic syncIndex avoids the cache line stampede
//    that a single shared fetch_add counter would cause. Once a worker
//    settles into a block range, its CAS targets live in its own L1.
//
// 2. Monotonic syncIndex across iterations. Iterative stages (warm start,
//    solve, relax) reuse the same block array every sub-step iteration.
//    syncIndex grows each iteration; workers CAS (prev, prev+1), so the
//    main thread never touches any per-block state between iterations.
//    Non-iterative stages simply use syncIndex 1.
//
// 3. L2 affinity across iterations. Each worker picks a start offset from
//    its workerIndex, then scans forward and (after wrap) backward:
//
//      blocks:   [0] [1] [2] [3] [4] [5] [6] [7]
//                 ^           ^           ^   ^
//                 W0          W1          W2  W3   <- start offsets
//
//    W0 claims 0,1,2,3 (forward), W1 claims 4,5, etc. Under balanced load
//    each worker re-hits the same block range every iteration, keeping that
//    range's hot data resident in its L2. A failed CAS means a neighbour
//    already claimed the block, so the stealing worker stops -- preserving
//    locality under mild imbalance while still draining the queue.
//
// A graph color stage lays out joint blocks first, then contact blocks:
//
//      stage->blocks ->
//        +------+------+------+------+------+------+------+
//        |  J0  |  J1  |  J2  |  C0  |  C1  |  C2  |  C3  |
//        +------+------+------+------+------+------+------+
//        <-- graphJointBlocks --><---- graphContactBlocks ---->
//
// Each block carries its type so the dispatcher routes J-blocks to the joint
// solver and C-blocks to the SIMD contact solver; both kinds run concurrently
// within the stage -- no barrier between them. The type tag lives on the
// block (not the stage) so that mixed-type stages can keep the concurrency.
//
// The solver threading model is inspired by https://github.com/bepu/bepuphysics2

#pragma once

#include "container.h"
#include "core.h"

#include "box3d/math_functions.h"

#include <stdint.h>

typedef struct b3BodySim b3BodySim;
typedef struct b3BodyState b3BodyState;
typedef struct b3ContactConstraint b3ContactConstraint;
typedef struct b3ContactSpec b3ContactSpec;
typedef struct b3JointSim b3JointSim;
typedef struct b3Manifold b3Manifold;
typedef struct b3ManifoldConstraint b3ManifoldConstraint;
typedef struct b3World b3World;

// Solver stages
typedef enum b3SolverStageType
{
	b3_stagePrepareJoints,
	b3_stagePrepareWideContacts,
	b3_stagePrepareContacts,
	b3_stageIntegrateVelocities,
	b3_stageWarmStart,
	b3_stageSolve,
	b3_stageIntegratePositions,
	b3_stageRelax,
	b3_stageRestitution,
	b3_stageStoreWideImpulses,
	b3_stageStoreImpulses,
} b3SolverStageType;

typedef enum b3SolverBlockType
{
	// Block for iterating across bodies.
	b3_bodyBlock,

	// Block for iterating across joints. For prepare.
	b3_jointBlock,

	// Block for iterating across wide contacts. For prepare and store.
	b3_wideContactBlock,

	// Block for iterating across contacts. For prepare and store.
	b3_contactBlock,

	// Block for iterating across joints of a single graph color.
	b3_graphJointBlock,

	// Block for iterating across wide contacts of a single graph color.
	b3_graphWideContactBlock,

	// Block for iterating across contacts of a single graph color.
	b3_graphContactBlock,

	// Block for processing overflow constraints
	b3_overflowBlock,

	// Added for Nebenan: block for iterating across the bodies of a pack of islands, see b3SolvePacks
	b3_packBodyBlock,

	// Added for Nebenan: block for one group of batches of knots, its index as the start, see b3KnotBatch
	b3_knotBlock,
} b3SolverBlockType;

// Solver block describes a multithreaded unit of work.
typedef struct b3SolverBlock
{
	int startIndex;
	uint16_t count;
	// b3SolverBlockType
	uint8_t blockType;
	uint8_t colorIndex;
} b3SolverBlock;

// A unit of multithreaded work along with atomic synchronization. The syncIndex grows
// monotonically allowing the solver block to be re-used across sub-steps.
typedef struct b3SyncBlock
{
	b3SolverBlock block;
	b3AtomicInt syncIndex;
} b3SyncBlock;

// Each stage must be completed before going to the next stage.
// Non-iterative stages use a stage instance once while iterative stages re-use the same instance each iteration.
typedef struct b3SolverStage
{
	b3SyncBlock* blocks;
	b3SolverStageType type;
	int blockCount;
	uint8_t colorIndex;
	b3AtomicInt completionCount;
} b3SolverStage;

// Constraint softness
typedef struct b3Softness
{
	float biasRate;
	float massScale;
	float impulseScale;
} b3Softness;

// Prepare/store run as a flat parallel-for over the whole wide-constraint
// range. Each span maps a slice of that range back to the owning color's
// contacts so workers can decode flat wide-slot indices without touching
// graph state. The spans array has one entry per active color plus a sentinel
// whose start == wideContactCount.
typedef struct b3WidePrepareSpan
{
	int start;
	int count;
	int* contacts;

	// Added for Nebenan: per contact the scale of the inverse mass and inertia of its bodies, for the copies of a knot, or
	// null, see b3KnotBatch
	const float* scaleA;
	const float* scaleB;
} b3WidePrepareSpan;

// Added for Nebenan: the lanes of a batch of knot contacts, two wide constraints of four or one of eight
#define B3_KNOT_LANES 8

// Added for Nebenan: the groups of batches of knots that the threads solve at once, see b3BuildKnots
#define B3_KNOT_GROUPS 8

// Added for Nebenan: a knot is a body with many contacts in the overflow of the graph, see b3BuildKnots. A batch holds up
// to B3_KNOT_LANES of its contacts with different partners, which its lanes solve at once. Each lane changes its own copy
// of the knot, which carries its share of the knot's mass, and the knot then takes the mean velocity of its copies: mass
// splitting after Tonge, Benevolenski and Voroshilov, Mass Splitting for Jitter-Free Parallel Rigid Body Simulation, 2012.
// A large overflow goes into groups with partners of their own, one thread solves a group batch after batch, and a knot
// in several groups has a copy in each. A batch then changes the copy of its group, and once all groups are through, the
// knot takes the mean velocity of these copies, see b3KnotSplit.
typedef struct b3KnotBatch
{
	// The awake index of the knot plus one, as b3ContactConstraintWide::indexA
	int knot;

	// The contacts of the batch from the first lane on, the number of copies of the knot
	int laneCount;

	// The copy of the knot in the group of the batch, see b3KnotCopy, or B3_NULL_INDEX for a knot in one group, which
	// the batch changes itself
	int copy;

	// Whether the batch is the first of its knot in its group, which starts the copy from the knot
	int first;
} b3KnotBatch;

// Added for Nebenan: the velocity of a knot in one of its groups, on a cache line of its own, see b3KnotBatch
typedef struct b3KnotCopy
{
	b3Vec3 linearVelocity;
	b3Vec3 angularVelocity;
	char padding[64 - 2 * sizeof( b3Vec3 )];
} b3KnotCopy;

// Added for Nebenan: a knot in several groups and its copies, one per group in the order of the groups, see b3KnotBatch
typedef struct b3KnotSplit
{
	// The awake index of the knot
	int knot;
	int firstCopy;
	int copyCount;
} b3KnotSplit;

// Added for Nebenan: a stage of one block the calling thread solved alone, one of the other packs a worker solved, and
// a stage a worker came into after giving up its core while it waited, see b3TeamEntry
typedef enum b3TeamEntryFlags
{
	b3_teamEntryAlone = 1,
	b3_teamEntryPack = 2,
	b3_teamEntryAfterYield = 4,
} b3TeamEntryFlags;

// Added for Nebenan: a stage one thread took part in, in a step that times its team, see b3TeamStages. The calling
// thread opens the stage, solves blocks until none is left and sees all of them done, or solves a stage of one block
// alone. A worker comes into the stage and solves blocks until none is left. Each notes the blocks it solved. A worker
// also notes each of the other packs it solves, from start to done, so its entries follow each other in time.
typedef struct b3TeamEntry
{
	uint64_t startTicks;
	uint64_t doneTicks;
	uint64_t closeTicks;
	uint32_t syncBits;
	uint16_t blockCount;
	uint8_t flags;
} b3TeamEntry;

typedef struct b3ContactPrepareSpan
{
	int start;
	int count;
	b3ContactSpec* contacts;
} b3ContactPrepareSpan;

typedef struct b3JointPrepareSpan
{
	int start;
	int count;
	b3JointSim* joints;
} b3JointPrepareSpan;

// Context for a time step. Recreated each time step.
typedef struct b3StepContext
{
	// time step
	float dt;

	// inverse time step (0 if dt == 0).
	float inv_dt;

	// sub-step
	float h;
	float inv_h;

	int subStepCount;

	b3Softness contactSoftness;
	b3Softness staticSoftness;

	float maxLinearVelocity;

	struct b3World* world;
	struct b3ConstraintGraph* graph;

	// shortcut to body states from awake set
	b3BodyState* states;

	// shortcut to body sims from awake set
	b3BodySim* sims;

	// array of all shape ids for shapes that have enlarged AABBs
	int* enlargedShapes;
	int enlargedShapeCount;

	// Array of bullet bodies that need continuous collision handling
	int* bulletBodies;
	b3AtomicInt bulletBodyCount;

	b3AtomicInt anyRestitution;

	// Contact ids for simplified parallel-for access. Used in narrow-phase.
	// These contacts may or may not be touching. They are associated with awake bodies.
	int* awakeContactIndices;

	// Flat view of the wide contact constraint array used by prepare and store.
	// prepareSpans has activeColorCount + 1 entries, the last being a sentinel
	// at wideContactCount. wideContactConstraints is the contiguous base
	// pointer; per-color slices live at colors[i].wideConstraints.
	// Added for Nebenan: one more span before the sentinel for the knots, see b3KnotBatch.
	void* wideConstraints; // Added for Nebenan: in the width of the world
	b3WidePrepareSpan* widePrepareSpans;
	int wideContactCount;

	// Similar for mesh/overflow contact constraints
	struct b3ManifoldConstraint* manifoldConstraints;
	struct b3ContactConstraint* contactConstraints;
	b3ContactPrepareSpan* contactPrepareSpans;
	b3ContactPrepareSpan* overflowSpans;
	b3JointPrepareSpan* jointPrepareSpans;

	int activeColorCount;
	int workerCount;

	b3SolverStage* stages;
	int stageCount;
	bool enableWarmStarting;

	// Added for Nebenan: the awake islands in packs, each solved whole by one thread without a barrier, see b3SolvePacks.
	// No packs if the step solves color by color. Per pack its bodies as awake indices, its overflow constraints and, per
	// active color, its wide constraints, each as a range up to the start of the next pack. With a team the stages hold
	// the first pack, and the threads solve the others whole in between, see b3SolveOtherPack.
	int packCount;
	bool team;

	// Added for Nebenan: this step times its team, see b3Profile::teamTime
	bool sampleTeam;

	// Added for Nebenan: this step times its contact loop, see b3Profile::collideWork
	bool sampleCollide;
	const int* packBodyStarts;
	const int* packBodies;
	const int* packOverflowStarts;
	const int* packWideStarts;
	const int* activeColorIndices;

	// Added for Nebenan: the batches of the knots in the overflow, solved before the other overflow constraints, see
	// b3BuildKnots. Their wide constraints follow those of the colors, B3_KNOT_LANES / B3_SIMD_WIDTH per batch, from
	// knotWideStart on. With packs each pack has the range of batches up to the start of the next.
	const b3KnotBatch* knotBatches;
	int knotBatchCount;
	void* knotConstraints;
	int knotWideStart;
	const int* packKnotStarts;

	// Added for Nebenan: the groups of the batches, pack by pack B3_KNOT_GROUPS, or as many for the step without packs,
	// as ranges up to the start of the next. The knots in several groups, pack by pack as ranges up to the start of the
	// next, and their copies, see b3KnotSplit.
	const int* knotGroupStarts;
	b3KnotCopy* knotCopies;
	const b3KnotSplit* knotSplits;
	const int* packSplitStarts;

	// Added for Nebenan: the overflow contacts outside the knots, solved one at a time
	int overflowContactCount;

	// padding to prevent false sharing
	char padding1[64];

	// This atomic is central to multi-threaded solver task synchronization.
	// It prevents ABA problems by monotonically growing as the solver advances.
	// This means a delayed worker thread will catch up without repeating already completed
	// work (causing a race condition).
	// sync index (16-bits) | stage type (16-bits)
	b3AtomicU32 atomicSyncBits;

	// padding to prevent false sharing
	char padding2[64];

	// Race flag claimed by whichever runner reaches b3SolverTask with workerIndex 0 first.
	// The calling thread of b3World_Step also races for this slot so the orchestrator can
	// always make progress, regardless of how the user's task system schedules tasks (out
	// of order, fewer threads than workers, or synchronously inside enqueueTaskFcn). The
	// loser of the race no-ops as workerIndex 0.
	b3AtomicInt mainClaimed;

	// padding to prevent false sharing
	char padding3[64];

	// Added for Nebenan: the next pack to take and the packs solved, see b3SolvePacks and b3SolveOtherPack
	b3AtomicInt packNext;
	b3AtomicInt packDone;

	// padding to prevent false sharing
	char padding4[64];

	// Added for Nebenan: in a step that times its team the entries of the threads into the stages, from workerIndex *
	// teamEntryCapacity on, see b3TeamEntry. Then the start and the end of the team on the calling thread and the time
	// it merges the copies of knots, see b3TeamStages. At the end, where they keep the other fields where they were.
	b3TeamEntry* teamEntries;
	int teamEntryCapacity;
	uint64_t teamStartTicks;
	uint64_t teamEndTicks;
	float teamMergeTime;

	// Added for Nebenan: the packs whose bodies and constraints the stages hold, the first ones, with a team those it
	// solves, see b3SolveOtherPack, and one without packs, which holds all knots, see b3BuildKnots
	int stagePackCount;
} b3StepContext;

void b3Solve( b3World* world, b3StepContext* stepContext );

static inline b3Softness b3MakeSoft( float hertz, float zeta, float h )
{
	if ( hertz == 0.0f )
	{
		return B3_LITERAL( b3Softness ){
			.biasRate = 0.0f,
			.massScale = 0.0f,
			.impulseScale = 0.0f,
		};
	}

	float omega = 2.0f * B3_PI * hertz;
	float a1 = 2.0f * zeta + h * omega;
	float a2 = h * omega * a1;
	float a3 = 1.0f / ( 1.0f + a2 );

	// bias = w / (2 * z + hw)
	// massScale = hw * (2 * z + hw) / (1 + hw * (2 * z + hw))
	// impulseScale = 1 / (1 + hw * (2 * z + hw))

	// If z == 0
	// bias = 1/h
	// massScale = hw^2 / (1 + hw^2)
	// impulseScale = 1 / (1 + hw^2)

	// w -> inf
	// bias = 1/h
	// massScale = 1
	// impulseScale = 0

	// if w = pi / 4  * inv_h
	// massScale = (pi/4)^2 / (1 + (pi/4)^2) = pi^2 / (16 + pi^2) ~= 0.38
	// impulseScale = 1 / (1 + (pi/4)^2) = 16 / (16 + pi^2) ~= 0.62

	// In all cases:
	// massScale + impulseScale == 1

	return ( b3Softness ){
		.biasRate = omega / a1,
		.massScale = a2 * a3,
		.impulseScale = a3,
	};
}
