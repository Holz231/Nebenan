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

// A step of nbWorld_Step, or a function of the program to run after the steps before it, see nbWorld_RunAfterStep
typedef struct nbQueuedStep
{
	float timeStep;
	int subStepCount;
	void ( *fcn )( void* context );
	void* context;
} nbQueuedStep;

// A thread of its own for the steps of nbWorld_Step. It takes the steps one after the other in the order they came, while
// the thread that hands them over goes on.
typedef struct nbStepThread nbStepThread;
typedef void nbStepFn( void* context, const nbQueuedStep* step );

// Null if the system has no thread to give
nbStepThread* nbCreateStepThread( nbStepFn* fn, void* context );

// Takes the steps that are left, then stops the thread
void nbDestroyStepThread( nbStepThread* stepThread );

// Hand over one more step
void nbStepThread_Push( nbStepThread* stepThread, const nbQueuedStep* step );

// Wait until every step that was handed over is taken
void nbStepThread_Wait( nbStepThread* stepThread );

// Whether the calling thread is the step thread, which must never wait for itself
bool nbStepThread_IsCurrent( nbStepThread* stepThread );
