// SPDX-FileCopyrightText: 2026 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>

typedef void b3TaskCallback( void* taskContext );
typedef struct b3Scheduler b3Scheduler;

b3Scheduler* b3CreateScheduler( int workerCount );
void b3DestroyScheduler( b3Scheduler* scheduler );
void b3ResetScheduler( b3Scheduler* scheduler );

// See b3EnqueueTaskCallback and b3FinishTaskCallback
void* b3SchedulerEnqueueTask( b3TaskCallback* task, void* taskContext, void* userContext, const char* name );
void b3SchedulerFinishTask( void* userTask, void* userContext );

// Added for Nebenan: while a step is active, the workers wait for its next task instead of sleeping. A world sets it for
// the length of b3World_Step, so the workers sleep between steps and leave the cores to the application.
void b3SchedulerSetStepActive( b3Scheduler* scheduler, bool active );
