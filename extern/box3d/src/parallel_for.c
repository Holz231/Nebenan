// SPDX-FileCopyrightText: 2026 Erin Catto
// SPDX-License-Identifier: MIT

#include "parallel_for.h"

#include "core.h"
#include "physics_world.h"
#include "platform.h"

#include "box3d/base.h"
#include "box3d/constants.h"

#include <stddef.h>

// Added for Nebenan: what one task of a timed b3ParallelForOrdered did, see b3ParallelForTimes. A cache line per task, so
// the threads do not write to the same line.
typedef struct b3ParallelForTaskTimes
{
	uint64_t startTicks;
	uint64_t endTicks;
	uint64_t workTicks;
	uint64_t longestTicks;
	int blockCount;
	char padding[B3_ALIGNMENT - 4 * sizeof( uint64_t ) - sizeof( int )];
} b3ParallelForTaskTimes;

_Static_assert( sizeof( b3ParallelForTaskTimes ) == B3_ALIGNMENT, "a cache line per task" );

// Shared state for one b3ParallelFor invocation. Workers race on nextBlock to
// claim work, so a slow chunk can't strand the other threads.
typedef struct b3ParallelForShared
{
	b3AtomicInt nextBlock;
	int blockCount;
	int blockSize;
	int itemCount;
	b3ParallelForCallback* callback;
	void* context;

	// Added for Nebenan: the claims alternate between the front and the back, see b3_blocksFromBothEnds
	bool fromBothEnds;

	// Added for Nebenan: one per task if the call is timed, see b3ParallelForOrdered
	b3ParallelForTaskTimes* taskTimes;
} b3ParallelForShared;

typedef struct b3ParallelForTask
{
	b3ParallelForShared* shared;
	int workerIndex;
} b3ParallelForTask;

// Added for Nebenan: the block a claim takes. From both ends the even claims take the blocks from the front and the odd
// ones from the back, so the claims meet in the middle, see b3_blocksFromBothEnds.
static inline int b3GetClaimedBlock( int claim, int blockCount, bool fromBothEnds )
{
	if ( fromBothEnds == false )
	{
		return claim;
	}

	return ( claim & 1 ) == 0 ? claim >> 1 : blockCount - 1 - ( claim >> 1 );
}

static void b3ParallelForTrampoline( void* taskContext )
{
	b3ParallelForTask* task = (b3ParallelForTask*)taskContext;
	b3ParallelForShared* shared = task->shared;
	int workerIndex = task->workerIndex;
	void* context = shared->context;
	b3ParallelForCallback* callback = shared->callback;

	int blockCount = shared->blockCount;
	int blockSize = shared->blockSize;
	int itemCount = shared->itemCount;
	bool fromBothEnds = shared->fromBothEnds;

	// Added for Nebenan: the same loop with the clock read around each block, see b3ParallelForOrdered
	if ( shared->taskTimes != NULL )
	{
		b3ParallelForTaskTimes times = { 0 };
		times.startTicks = b3GetTicks();
		for ( ;; )
		{
			int claim = b3AtomicFetchAddInt( &shared->nextBlock, 1 );
			if ( claim >= blockCount )
			{
				break;
			}

			int blockIndex = b3GetClaimedBlock( claim, blockCount, fromBothEnds );
			int start = blockIndex * blockSize;
			int end = start + blockSize;
			if ( end > itemCount )
			{
				end = itemCount;
			}

			uint64_t blockTicks = b3GetTicks();
			callback( start, end, workerIndex, context );
			uint64_t ticks = b3GetTicks() - blockTicks;
			times.workTicks += ticks;
			times.longestTicks = ticks > times.longestTicks ? ticks : times.longestTicks;
			times.blockCount += 1;
		}
		times.endTicks = b3GetTicks();
		shared->taskTimes[workerIndex] = times;
		return;
	}

	for ( ;; )
	{
		// Added for Nebenan: a claim takes a block in the order of the call
		int claim = b3AtomicFetchAddInt( &shared->nextBlock, 1 );
		if ( claim >= blockCount )
		{
			break;
		}

		int blockIndex = b3GetClaimedBlock( claim, blockCount, fromBothEnds );
		int start = blockIndex * blockSize;
		int end = start + blockSize;
		if ( end > itemCount )
		{
			end = itemCount;
		}

		callback( start, end, workerIndex, context );
	}
}

// Added for Nebenan: b3ParallelFor and b3ParallelForOrdered, with taskTimes NULL if the call is not timed
static void b3RunParallelFor( b3World* world, b3ParallelForCallback* callback, int itemCount, int minRange, void* context,
							  const char* name, b3BlockOrder order, b3ParallelForTaskTimes* taskTimes, int* blockCountOut,
							  int* taskCountOut )
{
	B3_ASSERT( itemCount > 0 );
	B3_ASSERT( minRange > 0 );

	// Added for Nebenan: the workers the step wakes, see b3GetStepWorkerCount
	int workerCount = world->activeWorkerCount;
	B3_ASSERT( 0 < workerCount && workerCount <= B3_MAX_WORKERS );

	// Target multiple blocks per worker to reduce thread stalls.
	// block size grows once items exceed maxBlockCount * minRange
	// so the block count stays bounded and per-block sync overhead stays low.
	// Benchmarking shows 32 is optimal for the convex pile benchmark and others.
	// This is a lot different than Box2D because the 3D narrow phase cost
	// varies a lot.
	int blocksPerWorker = 32;
	int maxBlockCount = blocksPerWorker * workerCount;

	int blockSize;
	int blockCount;
	if ( itemCount <= minRange * maxBlockCount )
	{
		blockSize = minRange;
		blockCount = ( itemCount + blockSize - 1 ) / blockSize;
	}
	else
	{
		blockSize = ( itemCount + maxBlockCount - 1 ) / maxBlockCount;
		blockCount = ( itemCount + blockSize - 1 ) / blockSize;
	}
	B3_ASSERT( blockCount >= 1 );
	B3_ASSERT( blockSize * blockCount >= itemCount );

	// No point enqueueing more tasks than blocks.
	int taskCount = workerCount < blockCount ? workerCount : blockCount;

	b3ParallelForShared shared;
	shared.blockCount = blockCount;
	shared.blockSize = blockSize;
	shared.itemCount = itemCount;
	shared.callback = callback;
	shared.context = context;
	shared.fromBothEnds = order == b3_blocksFromBothEnds && taskCount > 1;
	shared.taskTimes = taskTimes;
	b3AtomicStoreInt( &shared.nextBlock, 0 );

	*blockCountOut = blockCount;
	*taskCountOut = taskCount;

	// Added for Nebenan: a single task runs right here instead of waking a worker that has to fetch its data
	if ( taskCount == 1 )
	{
		b3ParallelForTask task = { .shared = &shared, .workerIndex = 0 };
		b3ParallelForTrampoline( &task );
		return;
	}

	b3ParallelForTask tasks[B3_MAX_WORKERS];
	void* handles[B3_MAX_WORKERS];
	for ( int i = 0; i < taskCount; ++i )
	{
		tasks[i].shared = &shared;
		tasks[i].workerIndex = i;

		if (world->taskCount < B3_MAX_TASKS)
		{
			handles[i] = world->enqueueTaskFcn( &b3ParallelForTrampoline, tasks + i, world->userTaskContext, name );
			world->taskCount += 1;
		}
		else
		{
			handles[i] = NULL;
			b3ParallelForTrampoline( tasks + i );
		}
	}

	for ( int i = 0; i < taskCount; ++i )
	{
		if ( handles[i] != NULL )
		{
			world->finishTaskFcn( handles[i], world->userTaskContext );
		}
	}
}

void b3ParallelFor( b3World* world, b3ParallelForCallback* callback, int itemCount, int minRange, void* context,
					const char* name )
{
	if ( itemCount <= 0 )
	{
		return;
	}

	int blockCount, taskCount;
	b3RunParallelFor( world, callback, itemCount, minRange, context, name, b3_blocksInOrder, NULL, &blockCount, &taskCount );
}

// Added for Nebenan
void b3ParallelForOrdered( b3World* world, b3ParallelForCallback* callback, int itemCount, int minRange, void* context,
						   const char* name, b3BlockOrder order, b3ParallelForTimes* times )
{
	if ( times == NULL )
	{
		if ( itemCount > 0 )
		{
			int blockCount, taskCount;
			b3RunParallelFor( world, callback, itemCount, minRange, context, name, order, NULL, &blockCount, &taskCount );
		}
		return;
	}

	*times = (b3ParallelForTimes){ 0 };
	if ( itemCount <= 0 )
	{
		return;
	}

	b3ParallelForTaskTimes* taskTimes = (b3ParallelForTaskTimes*)b3StackAlloc(
		&world->stack, B3_MAX_WORKERS * (int)sizeof( b3ParallelForTaskTimes ), "parallel for times" );

	uint64_t startTicks = b3GetTicks();
	int blockCount, taskCount;
	b3RunParallelFor( world, callback, itemCount, minRange, context, name, order, taskTimes, &blockCount, &taskCount );
	uint64_t endTicks = b3GetTicks();

	times->callTicks = endTicks - startTicks;
	times->taskCount = taskCount;
	times->blockCount = blockCount;
	for ( int i = 0; i < taskCount; ++i )
	{
		const b3ParallelForTaskTimes* task = taskTimes + i;
		times->workTicks += task->workTicks;
		times->lateTicks += task->startTicks - startTicks;
		times->claimTicks += task->endTicks - task->startTicks - task->workTicks;
		times->doneTicks += endTicks - task->endTicks;
		times->longestTicks = task->longestTicks > times->longestTicks ? task->longestTicks : times->longestTicks;
		times->missedCount += task->blockCount == 0 ? 1 : 0;
	}

	b3StackFree( &world->stack, taskTimes );
}
