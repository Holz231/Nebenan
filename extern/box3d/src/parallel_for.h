// SPDX-FileCopyrightText: 2026 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct b3World b3World;

// Callback invoked by b3ParallelFor to process a range of items. May be called
// multiple times per worker: work is divided into blocks that workers claim
// atomically, so a worker that finishes early picks up the next unclaimed
// block instead of sitting idle. workerIndex is the worker identity and is
// stable across all invocations from the same worker, so it is safe to use as
// an index into per-worker state (e.g. world->taskContexts.data + workerIndex).
typedef void b3ParallelForCallback( int startIndex, int endIndex, int workerIndex, void* context );

// Divide [0, itemCount) into blocks and process them with cooperative claiming:
// up to world->workerCount tasks are enqueued, and each task loops, atomically
// claiming the next unclaimed block until the range is drained. Blocks the
// caller until all work is complete. minRange is the minimum block size; block
// size grows once itemCount exceeds 4 * workerCount * minRange so block count
// stays bounded.
void b3ParallelFor( b3World* world, b3ParallelForCallback* callback, int itemCount, int minRange, void* context,
					const char* name );

// Added for Nebenan: the order in which the tasks of a b3ParallelForOrdered take the blocks
typedef enum b3BlockOrder
{
	// From the first block to the last, as b3ParallelFor does
	b3_blocksInOrder,

	// Alternately from the front and from the back, so the blocks in the middle come last. For a loop whose most
	// expensive items lie at both ends, as in the contact loop, see b3Collide: the expensive blocks go first, and the
	// tasks end on cheap blocks close together. A call with one task takes them in order.
	b3_blocksFromBothEnds,
} b3BlockOrder;

// Added for Nebenan: what the tasks of a timed b3ParallelForOrdered did, in ticks of b3GetTicks summed over the tasks
// unless noted. The time in blocks, before a task started, between its blocks with the claims of them and after it found
// none left add up to the call times the tasks.
typedef struct b3ParallelForTimes
{
	// The call on the calling thread, from before it enqueued the tasks until all of them were done, not summed
	uint64_t callTicks;

	uint64_t workTicks;
	uint64_t lateTicks;
	uint64_t claimTicks;
	uint64_t doneTicks;

	// The longest block, not summed
	uint64_t longestTicks;

	int taskCount;
	int blockCount;

	// The tasks that started only after all blocks were taken
	int missedCount;
} b3ParallelForTimes;

// Added for Nebenan: b3ParallelFor with the blocks taken in the given order, and with times not NULL timed into times:
// every task reads the clock around each of its blocks, and the calling thread takes room for the times from the stack of
// the world. The order changes only which task takes which block, each item is processed once as before.
void b3ParallelForOrdered( b3World* world, b3ParallelForCallback* callback, int itemCount, int minRange, void* context,
						   const char* name, b3BlockOrder order, b3ParallelForTimes* times );
