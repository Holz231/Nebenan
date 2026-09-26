// SPDX-License-Identifier: MIT

#include "scheduler.h"

#include "core.h"

#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
typedef SRWLOCK nbMutex;
typedef CONDITION_VARIABLE nbCondition;
typedef HANDLE nbThread;
#else
#include <pthread.h>
typedef pthread_mutex_t nbMutex;
typedef pthread_cond_t nbCondition;
typedef pthread_t nbThread;
#endif

#define NB_SCHEDULER_MAX_THREADS 31
#define NB_SCHEDULER_MAX_TASKS 64

enum nbTaskState
{
	nb_taskFree = 0,
	nb_taskPending,
	nb_taskRunning,
	nb_taskDone,
};

typedef struct nbSchedulerTask
{
	b3TaskCallback* callback;
	void* context;
	int state;
} nbSchedulerTask;

// One mutex guards the task slots. Nebenan enqueues a task per worker and operation, so the lock
// is taken a handful of times per impact and never becomes a bottleneck.
struct nbScheduler
{
	nbMutex mutex;
	nbCondition workAvailable;
	nbCondition workDone;
	nbSchedulerTask tasks[NB_SCHEDULER_MAX_TASKS];
	int pendingCount;
	bool shutdown;
	int threadCount;
	nbThread threads[NB_SCHEDULER_MAX_THREADS];
};

#if defined( _WIN32 )

static void nbMutexInit( nbMutex* mutex )
{
	InitializeSRWLock( mutex );
}

static void nbMutexDestroy( nbMutex* mutex )
{
	NB_UNUSED( mutex );
}

static void nbMutexLock( nbMutex* mutex )
{
	AcquireSRWLockExclusive( mutex );
}

static void nbMutexUnlock( nbMutex* mutex )
{
	ReleaseSRWLockExclusive( mutex );
}

static void nbConditionInit( nbCondition* condition )
{
	InitializeConditionVariable( condition );
}

static void nbConditionDestroy( nbCondition* condition )
{
	NB_UNUSED( condition );
}

static void nbConditionWait( nbCondition* condition, nbMutex* mutex )
{
	SleepConditionVariableSRW( condition, mutex, INFINITE, 0 );
}

static void nbConditionSignal( nbCondition* condition )
{
	WakeConditionVariable( condition );
}

static void nbConditionBroadcast( nbCondition* condition )
{
	WakeAllConditionVariable( condition );
}

#else

static void nbMutexInit( nbMutex* mutex )
{
	pthread_mutex_init( mutex, NULL );
}

static void nbMutexDestroy( nbMutex* mutex )
{
	pthread_mutex_destroy( mutex );
}

static void nbMutexLock( nbMutex* mutex )
{
	pthread_mutex_lock( mutex );
}

static void nbMutexUnlock( nbMutex* mutex )
{
	pthread_mutex_unlock( mutex );
}

static void nbConditionInit( nbCondition* condition )
{
	pthread_cond_init( condition, NULL );
}

static void nbConditionDestroy( nbCondition* condition )
{
	pthread_cond_destroy( condition );
}

static void nbConditionWait( nbCondition* condition, nbMutex* mutex )
{
	pthread_cond_wait( condition, mutex );
}

static void nbConditionSignal( nbCondition* condition )
{
	pthread_cond_signal( condition );
}

static void nbConditionBroadcast( nbCondition* condition )
{
	pthread_cond_broadcast( condition );
}

#endif

static nbSchedulerTask* nbFindPendingTask( nbScheduler* scheduler )
{
	for ( int i = 0; i < NB_SCHEDULER_MAX_TASKS; ++i )
	{
		if ( scheduler->tasks[i].state == nb_taskPending )
		{
			return scheduler->tasks + i;
		}
	}
	return NULL;
}

// Called and returns with the mutex held
static void nbRunTask( nbScheduler* scheduler, nbSchedulerTask* task )
{
	task->state = nb_taskRunning;
	scheduler->pendingCount -= 1;
	nbMutexUnlock( &scheduler->mutex );

	task->callback( task->context );

	nbMutexLock( &scheduler->mutex );
	task->state = nb_taskDone;
	nbConditionBroadcast( &scheduler->workDone );
}

static void nbWorkerLoop( nbScheduler* scheduler )
{
	nbMutexLock( &scheduler->mutex );
	for ( ;; )
	{
		while ( scheduler->shutdown == false && scheduler->pendingCount == 0 )
		{
			nbConditionWait( &scheduler->workAvailable, &scheduler->mutex );
		}

		if ( scheduler->shutdown )
		{
			break;
		}

		nbSchedulerTask* task = nbFindPendingTask( scheduler );
		NB_ASSERT( task != NULL );
		nbRunTask( scheduler, task );
	}
	nbMutexUnlock( &scheduler->mutex );
}

#if defined( _WIN32 )

static DWORD WINAPI nbWorkerMain( LPVOID parameter )
{
	nbWorkerLoop( parameter );
	return 0;
}

static bool nbStartThread( nbThread* thread, nbScheduler* scheduler )
{
	*thread = CreateThread( NULL, 0, nbWorkerMain, scheduler, 0, NULL );
	return *thread != NULL;
}

static void nbJoinThread( nbThread thread )
{
	WaitForSingleObject( thread, INFINITE );
	CloseHandle( thread );
}

#else

static void* nbWorkerMain( void* parameter )
{
	nbWorkerLoop( parameter );
	return NULL;
}

static bool nbStartThread( nbThread* thread, nbScheduler* scheduler )
{
	return pthread_create( thread, NULL, nbWorkerMain, scheduler ) == 0;
}

static void nbJoinThread( nbThread thread )
{
	pthread_join( thread, NULL );
}

#endif

nbScheduler* nbCreateScheduler( int threadCount )
{
	nbScheduler* scheduler = nbAlloc( sizeof( nbScheduler ) );
	memset( scheduler, 0, sizeof( nbScheduler ) );
	nbMutexInit( &scheduler->mutex );
	nbConditionInit( &scheduler->workAvailable );
	nbConditionInit( &scheduler->workDone );

	threadCount = threadCount < 0 ? 0 : ( threadCount > NB_SCHEDULER_MAX_THREADS ? NB_SCHEDULER_MAX_THREADS : threadCount );
	for ( int i = 0; i < threadCount; ++i )
	{
		if ( nbStartThread( scheduler->threads + scheduler->threadCount, scheduler ) == false )
		{
			break;
		}
		scheduler->threadCount += 1;
	}
	return scheduler;
}

void nbDestroyScheduler( nbScheduler* scheduler )
{
	if ( scheduler == NULL )
	{
		return;
	}

	nbMutexLock( &scheduler->mutex );
	scheduler->shutdown = true;
	nbConditionBroadcast( &scheduler->workAvailable );
	nbMutexUnlock( &scheduler->mutex );

	for ( int i = 0; i < scheduler->threadCount; ++i )
	{
		nbJoinThread( scheduler->threads[i] );
	}

	nbConditionDestroy( &scheduler->workDone );
	nbConditionDestroy( &scheduler->workAvailable );
	nbMutexDestroy( &scheduler->mutex );
	nbFree( scheduler, sizeof( nbScheduler ) );
}

void* nbSchedulerEnqueueTask( b3TaskCallback* task, void* taskContext, void* userContext, const char* taskName )
{
	NB_UNUSED( taskName );
	nbScheduler* scheduler = userContext;

	nbMutexLock( &scheduler->mutex );
	for ( int i = 0; i < NB_SCHEDULER_MAX_TASKS; ++i )
	{
		nbSchedulerTask* slot = scheduler->tasks + i;
		if ( slot->state == nb_taskFree )
		{
			slot->callback = task;
			slot->context = taskContext;
			slot->state = nb_taskPending;
			scheduler->pendingCount += 1;
			nbConditionSignal( &scheduler->workAvailable );
			nbMutexUnlock( &scheduler->mutex );
			return slot;
		}
	}
	nbMutexUnlock( &scheduler->mutex );

	// All slots busy: run it right here. Null tells the caller there is nothing to finish.
	task( taskContext );
	return NULL;
}

void nbSchedulerFinishTask( void* userTask, void* userContext )
{
	nbScheduler* scheduler = userContext;
	nbSchedulerTask* target = userTask;

	nbMutexLock( &scheduler->mutex );
	while ( target->state != nb_taskDone )
	{
		// Help instead of sleeping. This also runs the target itself if no thread picked it up yet.
		nbSchedulerTask* pending = nbFindPendingTask( scheduler );
		if ( pending != NULL )
		{
			nbRunTask( scheduler, pending );
		}
		else
		{
			nbConditionWait( &scheduler->workDone, &scheduler->mutex );
		}
	}
	target->state = nb_taskFree;
	nbMutexUnlock( &scheduler->mutex );
}
