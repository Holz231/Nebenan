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
