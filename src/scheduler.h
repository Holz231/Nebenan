// SPDX-License-Identifier: MIT

#pragma once

#include "box3d/types.h"

// A small thread pool with the Box3D task callback signatures. Nebenan uses it when the application
// asks for several workers but does not provide its own task system.
typedef struct nbScheduler nbScheduler;

// Starts threadCount threads. A thread that waits for a task helps with pending tasks meanwhile.
nbScheduler* nbCreateScheduler( int threadCount );
void nbDestroyScheduler( nbScheduler* scheduler );

// b3EnqueueTaskCallback and b3FinishTaskCallback. The user context is the scheduler.
void* nbSchedulerEnqueueTask( b3TaskCallback* task, void* taskContext, void* userContext, const char* taskName );
void nbSchedulerFinishTask( void* userTask, void* userContext );

// A task that runs in the background while the calling thread goes on, finished with nbSchedulerFinishTask. The threads
// take the other tasks first, and a thread that waits for another task never helps with it, so it does not hold up an
// operation that waits for its own tasks. Such tasks take at most half of the slots, beyond that the task runs right here
// and the result is null.
void* nbSchedulerEnqueueBackgroundTask( b3TaskCallback* task, void* taskContext, nbScheduler* scheduler );
