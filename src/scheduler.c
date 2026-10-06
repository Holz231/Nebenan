// SPDX-License-Identifier: MIT

#include "scheduler.h"

#include "core.h"

#include "box3d/base.h"
#include "nebenan/nebenan.h"

#include <string.h>

#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <intrin.h>
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
#define NB_SCHEDULER_MAX_TASKS 128

// A slot goes from free to writing for the thread that enqueues, to pending, to running for the thread that takes it, to
// done, and back to free once the task is finished. A task in the background, see nbSchedulerEnqueueBackgroundTask, is
// pending in a state of its own, so a thread tells it apart with the same load that finds it.
enum nbTaskState
{
	nb_taskFree = 0,
	nb_taskWriting,
	nb_taskPending,
	nb_taskPendingBackground,
	nb_taskRunning,
	nb_taskDone,
};

typedef struct nbSchedulerTask
{
	int state;

	// Tasks in the background go in the order they came
	int sequence;
	bool background;

	b3TaskCallback* callback;
	void* context;
} nbSchedulerTask;

// The slots take no lock: a thread claims a slot or a task with a compare and swap, like the scheduler of Box3D, so the
// many small tasks of a step cost little. Unlike that one, a task keeps its slot until it is finished, across steps, so
// destructibles can be prepared in the background. Only a thread without work takes the lock, to sleep.
struct nbScheduler
{
	nbSchedulerTask tasks[NB_SCHEDULER_MAX_TASKS];

	// Tasks enqueued and not taken yet, slots held by tasks in the background, and the order of the tasks
	int pendingCount;
	int backgroundCount;
	int sequence;

	// Threads asleep for lack of work, and whether they should stop
	nbMutex mutex;
	nbCondition workAvailable;
	int sleeperCount;
	int shutdown;

	int threadCount;
	nbThread threads[NB_SCHEDULER_MAX_THREADS];
};

// Atomics with sequential consistency. MSVC's C mode has no stdatomic.h without experimental flags, so use intrinsics.
static int nbAtomicLoad( int* value )
{
#if defined( _MSC_VER ) && !defined( __clang__ )
	return _InterlockedOr( (volatile long*)value, 0 );
#else
	return __atomic_load_n( value, __ATOMIC_SEQ_CST );
#endif
}

static void nbAtomicStore( int* value, int desired )
{
#if defined( _MSC_VER ) && !defined( __clang__ )
	(void)_InterlockedExchange( (volatile long*)value, desired );
#else
	__atomic_store_n( value, desired, __ATOMIC_SEQ_CST );
#endif
}

static bool nbAtomicCompareExchange( int* value, int expected, int desired )
{
#if defined( _MSC_VER ) && !defined( __clang__ )
	return _InterlockedCompareExchange( (volatile long*)value, desired, expected ) == expected;
#else
	return __atomic_compare_exchange_n( value, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST );
#endif
}

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

// Run a task this thread claimed and mark it done
static void nbRunClaimed( nbScheduler* scheduler, nbSchedulerTask* task )
{
	nbAtomicFetchAddInt( &scheduler->pendingCount, -1 );
	task->callback( task->context );
	nbAtomicStore( &task->state, nb_taskDone );
}

// Take a pending task and run it: one that is not in the background, else, if allowed, the oldest one in the background.
// Returns false if there was none to take.
static bool nbRunPending( nbScheduler* scheduler, bool allowBackground )
{
	for ( ;; )
	{
		nbSchedulerTask* oldest = NULL;
		int oldestSequence = 0;
		for ( int i = 0; i < NB_SCHEDULER_MAX_TASKS; ++i )
		{
			nbSchedulerTask* task = scheduler->tasks + i;
			int state = nbAtomicLoad( &task->state );
			if ( state == nb_taskPending && nbAtomicCompareExchange( &task->state, nb_taskPending, nb_taskRunning ) )
			{
				nbRunClaimed( scheduler, task );
				return true;
			}

			// The slot may hold another task by now, then the order is a little off and nothing more
			if ( state == nb_taskPendingBackground && allowBackground )
			{
				int sequence = nbAtomicLoad( &task->sequence );
				if ( oldest == NULL || (int32_t)( (uint32_t)sequence - (uint32_t)oldestSequence ) < 0 )
				{
					oldest = task;
					oldestSequence = sequence;
				}
			}
		}

		if ( oldest == NULL )
		{
			return false;
		}

		// Another thread may have taken it meanwhile, then look again
		if ( nbAtomicCompareExchange( &oldest->state, nb_taskPendingBackground, nb_taskRunning ) )
		{
			nbRunClaimed( scheduler, oldest );
			return true;
		}
	}
}

static void nbWorkerLoop( nbScheduler* scheduler )
{
	for ( ;; )
	{
		if ( nbRunPending( scheduler, true ) )
		{
			continue;
		}

		// Sleep until there is work. A thread that enqueues sees the sleeper, or this thread sees the task.
		nbMutexLock( &scheduler->mutex );
		nbAtomicFetchAddInt( &scheduler->sleeperCount, 1 );
		while ( nbAtomicLoad( &scheduler->pendingCount ) == 0 && nbAtomicLoad( &scheduler->shutdown ) == 0 )
		{
			nbConditionWait( &scheduler->workAvailable, &scheduler->mutex );
		}
		nbAtomicFetchAddInt( &scheduler->sleeperCount, -1 );
		nbMutexUnlock( &scheduler->mutex );

		if ( nbAtomicLoad( &scheduler->shutdown ) != 0 )
		{
			break;
		}
	}
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
	nbAtomicStore( &scheduler->shutdown, 1 );
	nbConditionBroadcast( &scheduler->workAvailable );
	nbMutexUnlock( &scheduler->mutex );

	for ( int i = 0; i < scheduler->threadCount; ++i )
	{
		nbJoinThread( scheduler->threads[i] );
	}

	nbConditionDestroy( &scheduler->workAvailable );
	nbMutexDestroy( &scheduler->mutex );
	nbFree( scheduler, sizeof( nbScheduler ) );
}

static void* nbEnqueue( nbScheduler* scheduler, b3TaskCallback* task, void* taskContext, bool background )
{
	// Tasks in the background leave half of the slots to the others, Nebenan enqueues fewer of them at a time
	bool full = false;
	if ( background )
	{
		full = nbAtomicFetchAddInt( &scheduler->backgroundCount, 1 ) >= NB_SCHEDULER_MAX_TASKS / 2;
	}

	for ( int i = 0; full == false && i < NB_SCHEDULER_MAX_TASKS; ++i )
	{
		nbSchedulerTask* slot = scheduler->tasks + i;
		if ( nbAtomicLoad( &slot->state ) != nb_taskFree ||
			 nbAtomicCompareExchange( &slot->state, nb_taskFree, nb_taskWriting ) == false )
		{
			continue;
		}

		slot->callback = task;
		slot->context = taskContext;
		slot->background = background;
		nbAtomicStore( &slot->sequence, nbAtomicFetchAddInt( &scheduler->sequence, 1 ) );
		nbAtomicFetchAddInt( &scheduler->pendingCount, 1 );
		nbAtomicStore( &slot->state, background ? nb_taskPendingBackground : nb_taskPending );

		// Wake a thread if one sleeps, see nbWorkerLoop
		if ( nbAtomicLoad( &scheduler->sleeperCount ) > 0 )
		{
			nbMutexLock( &scheduler->mutex );
			nbConditionSignal( &scheduler->workAvailable );
			nbMutexUnlock( &scheduler->mutex );
		}
		return slot;
	}

	if ( background )
	{
		nbAtomicFetchAddInt( &scheduler->backgroundCount, -1 );
	}

	// All slots busy: run it right here. Null tells the caller there is nothing to finish.
	task( taskContext );
	return NULL;
}

void* nbSchedulerEnqueueTask( b3TaskCallback* task, void* taskContext, void* userContext, const char* taskName )
{
	NB_UNUSED( taskName );
	return nbEnqueue( userContext, task, taskContext, false );
}

void* nbSchedulerEnqueueBackgroundTask( b3TaskCallback* task, void* taskContext, nbScheduler* scheduler )
{
	return nbEnqueue( scheduler, task, taskContext, true );
}

void nbSchedulerFinishTask( void* userTask, void* userContext )
{
	if ( userTask == NULL )
	{
		return;
	}

	// The slot stays with the target until it is free again, so its fields hold still
	nbScheduler* scheduler = userContext;
	nbSchedulerTask* target = userTask;
	bool background = target->background;
	int pending = background ? nb_taskPendingBackground : nb_taskPending;
	while ( nbAtomicLoad( &target->state ) != nb_taskDone )
	{
		// The target itself if no thread took it yet, then help with what is pending instead of sleeping, except with
		// tasks in the background, they may take long
		if ( nbAtomicCompareExchange( &target->state, pending, nb_taskRunning ) )
		{
			nbRunClaimed( scheduler, target );
			break;
		}

		if ( nbRunPending( scheduler, false ) == false )
		{
			b3Yield();
		}
	}

	nbAtomicStore( &target->state, nb_taskFree );
	if ( background )
	{
		nbAtomicFetchAddInt( &scheduler->backgroundCount, -1 );
	}
}

nbTaskSystem* nbCreateTaskSystem( int threadCount )
{
	return (nbTaskSystem*)nbCreateScheduler( threadCount );
}

void nbDestroyTaskSystem( nbTaskSystem* taskSystem )
{
	nbDestroyScheduler( (nbScheduler*)taskSystem );
}

void* nbEnqueueTask( b3TaskCallback* task, void* taskContext, void* userContext, const char* taskName )
{
	return nbSchedulerEnqueueTask( task, taskContext, userContext, taskName );
}

void nbFinishTask( void* userTask, void* userContext )
{
	nbSchedulerFinishTask( userTask, userContext );
}
