/* MollenOS
 *
 * Copyright 2011 - 2017, Philip Meulengracht
 *
 * This program is free software : you can redistribute it and / or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ? , either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * MollenOS MCore - Threading Pool Support Definitions & Structures
 * - This header describes the base threadingpool-structures, prototypes
 *   and functionality, refer to the individual things for descriptions
 */

/* Includes
 * - System */
#include <os/binarysemaphore.h>
#include <os/threadpool.h>
#include <os/utils.h>

/* Includes
 * - Library */
#include <stdlib.h>
#include <signal.h>

/* ThreadPoolJob (Private)
 * Describes a linked list of jobs for threads to execute */
typedef struct _ThreadPoolJob {
	struct _ThreadPoolJob*  Previous;
	ThreadFunc_t			Function;
	void *					Argument;
} ThreadPoolJob_t;

/* ThreadPoolJobQueue (Private)
 * Contains a list of job entries (ThreadPoolJob), and keeps
 * the synchronization between retrieving and adding */
typedef struct _ThreadPoolJobQueue {
	Mutex_t					Lock;
	ThreadPoolJob_t	*		Head;
	ThreadPoolJob_t	*		Tail;
	BinarySemaphore_t *		HasJobs;
	int						Length;
} ThreadPoolJobQueue_t;

/* ThreadPoolThread (Private) 
 * Contains the thread information and some extra information */
typedef struct _ThreadPoolThread {
	int						Id;
	UUId_t					Thread;
	ThreadPool_t *			Pool;
} ThreadPoolThread_t;

/* ThreadPool (Private) 
 * Contains all the neccessary information about the threadpool
 * and it's locks/threads/jobs */
typedef struct _ThreadPool {
	volatile int			ThreadsAlive;
	volatile int			ThreadsWorking;
	volatile int			ThreadsKeepAlive;
	volatile sig_atomic_t	ThreadsOnHold;

	// Resources
	Mutex_t					ThreadLock;
	Condition_t				ThreadsIdle;
	ThreadPoolThread_t **	Threads;
	ThreadPoolJobQueue_t	JobQueue;
} ThreadPool_t;

/* Globals
 * Keeps volatile/static information related to state */
static TlsKey_t __GlbThreadPoolKey = TLS_KEY_INVALID;

/* JobQueueInitialize
 * Allocates resources and initializes the job-queue */
OsStatus_t
JobQueueInitialize(
	_In_ ThreadPoolJobQueue_t *JobQueue)
{
	// Sanitize
	if (JobQueue == NULL) {
		return OsError;
	}

	// Reset members
	JobQueue->Length = 0;
	JobQueue->Head = NULL;
	JobQueue->Tail = NULL;

	// Allocate a new lock
	JobQueue->HasJobs = (BinarySemaphore_t*)malloc(sizeof(BinarySemaphore_t));

	// Initialize resources
	MutexConstruct(&JobQueue->Lock, MUTEX_PLAIN);
	BinarySemaphoreConstruct(JobQueue->HasJobs, 0);

	// Done
	return OsSuccess;
}

/* JobQueuePush
 * Adds a new job to the end of the queue, the job will be executed
 * as fast as possible. */
OsStatus_t
JobQueuePush(
	_In_ ThreadPoolJobQueue_t *JobQueue,
	_In_ ThreadPoolJob_t *Job)
{
	// Sanitize
	if (JobQueue == NULL) {
		return OsError;
	}

	// Acquire lock and reset previous
	MutexLock(&JobQueue->Lock);
	Job->Previous = NULL;

	// Either add to start or end
	if (JobQueue->Length == 0) {
		JobQueue->Head = Job;
		JobQueue->Tail = Job;
	}
	else {
		JobQueue->Tail->Previous = Job;
		JobQueue->Tail = Job;
	}

	// Increase length
	JobQueue->Length++;

	// Notify threads
	BinarySemaphorePost(JobQueue->HasJobs);
	return MutexUnlock(&JobQueue->Lock);
}

/* JobQueuePull
 * Get first job from queue and removes it from the job-queue
 * The caller must hold a mutex */
ThreadPoolJob_t*
JobQueuePull(
	_In_ ThreadPoolJobQueue_t *JobQueue)
{
	// Variables
	ThreadPoolJob_t *Job = NULL;

	// Sanitize
	if (JobQueue == NULL) {
		return NULL;
	}

	// Acquire lock and reset previous
	MutexLock(&JobQueue->Lock);
	Job = JobQueue->Head;

	// On pull we have three different cases
	// Either it's empty, return null
	// If there is one job, clear head/tail
	// If more, get head and notify
	if (JobQueue->Length == 0) {
		// Do nothing
	}
	else if (JobQueue->Length == 1) {
		JobQueue->Head = NULL;
		JobQueue->Tail = NULL;
		JobQueue->Length--;
	}
	else {
		JobQueue->Head = Job->Previous;
		JobQueue->Length--;

		// Update that we still have jobs
		BinarySemaphorePost(JobQueue->HasJobs);
	}

	// Unlock and return the job
	MutexUnlock(&JobQueue->Lock);
	return Job;
}

/* JobQueueClear
 * Clears the job-queue and resets it's members. Also frees
 * any resources associated with the jobs */
OsStatus_t
JobQueueClear(
	_In_ ThreadPoolJobQueue_t *JobQueue)
{
	// Sanitize
	if (JobQueue == NULL) {
		return OsError;
	}

	// Iterate and free jobs
	while (JobQueue->Length) {
		free(JobQueuePull(JobQueue));
	}

	// Reset members
	JobQueue->Head = NULL;
	JobQueue->Tail = NULL;
	BinarySemaphoreReset(JobQueue->HasJobs);
	JobQueue->Length = 0;
	
	// Done
	return OsSuccess;
}

/* JobQueueDestroy
 * Free all queue resources back to the system and clears the queue */
void
JobQueueDestroy(
	_In_ ThreadPoolJobQueue_t *JobQueue)
{
	// Clear queue, then cleanup
	JobQueueClear(JobQueue);
	free(JobQueue->HasJobs);
}

/* ThreadPoolThreadHold
 * Signal-handler: Sets the calling thread on hold */
void
ThreadPoolThreadHold(
	_In_ int SignalCode)
{
	// Variables 
	ThreadPool_t *Tp = NULL;

	// Unused
	_CRT_UNUSED(SignalCode);

	// Extract pool from tls
	Tp = (ThreadPool_t*)TLSGetKey(__GlbThreadPoolKey);

	// Set on hold
	if (Tp != NULL) {
		Tp->ThreadsOnHold = 1;
		while (Tp->ThreadsOnHold) {
			ThreadSleep(1);
		}
	}
}

/* ThreadPoolThreadLoop
 * The primary loop of each thread */
int
ThreadPoolThreadLoop(
	_In_ void *Argument)
{
	// Variables
	ThreadPoolThread_t* Thread = NULL;
	ThreadPoolJob_t *Job = NULL;
	ThreadPool_t *Pool = NULL;

	// Instantiate the pointers
	Thread = (ThreadPoolThread_t*)Argument;
	Pool = Thread->Pool;

	// Update tls and store the pool
	TLSSetKey(__GlbThreadPoolKey, Pool);

	// Update signal handler for this thread
	signal(SIGUSR1, ThreadPoolThreadHold);

	// Increase thread-live count
	MutexLock(&Pool->ThreadLock);
	Pool->ThreadsAlive += 1;
	MutexUnlock(&Pool->ThreadLock);

	// Enter job-queue loop
	while (Pool->ThreadsKeepAlive) {
		BinarySemaphoreWait(Pool->JobQueue.HasJobs);

		// Make sure we check it again after wake-up
		if (Pool->ThreadsKeepAlive) {
			// Increase thread-working count
			MutexLock(&Pool->ThreadLock);
			Pool->ThreadsWorking++;
			MutexUnlock(&Pool->ThreadLock);

			// Get next job and execute
			Job = JobQueuePull(&Pool->JobQueue);
			if (Job != NULL) {
				Job->Function(Job->Argument);
				free(Job);
			}

			// Decrease thread-working count
			MutexLock(&Pool->ThreadLock);
			Pool->ThreadsWorking--;

			// If none are working anymore, signal all idle
			if (!Pool->ThreadsWorking) {
				ConditionSignal(&Pool->ThreadsIdle);
			}
			MutexUnlock(&Pool->ThreadLock);
		}
	}

	// Decrease thread-live count
	MutexLock(&Pool->ThreadLock);
	Pool->ThreadsAlive--;
	MutexUnlock(&Pool->ThreadLock);

	// Thread is done
	return 0;
}

/* ThreadPoolInitializeThread
 * Initialize a thread in the thread pool */
int 
ThreadPoolInitializeThread(
	_In_ ThreadPool_t* ThreadPool,
	_In_ ThreadPoolThread_t** Thread,
	_In_ int Id)
{
	// Allocate a new instance of a thread
	*Thread = (ThreadPoolThread_t*)malloc(sizeof(ThreadPoolThread_t));
	(*Thread)->Id = Id;
	(*Thread)->Pool = ThreadPool;

	// Spawn it
	(*Thread)->Thread = ThreadCreate(ThreadPoolThreadLoop, *Thread);
	return 0;
}

/* ThreadPoolThreadDestroy
 * Frees any resources related to the given thread */
void
ThreadPoolThreadDestroy(
	_In_ ThreadPoolThread_t *Thread)
{
	// Simply free it
	free(Thread);
}

/* ThreadPoolInitialize 
 * Initializes a new thread-pool with the given number of threads */
OsStatus_t
ThreadPoolInitialize(
	_In_ int NumThreads,
	_Out_ ThreadPool_t **ThreadPool)
{
	// Variables
	ThreadPool_t *Tp = NULL;
	int i;

	// Trace
	TRACE("ThreadPoolInitialize(%i)", NumThreads);
	
	// Handle thread count
	if (NumThreads == THREADPOOL_DEFAULT_WORKERS) {
		NumThreads = 4;
		// for now.. set to num_cpus
	}

	// Sanitize parameters
	if (ThreadPool == NULL || NumThreads < 0) {
		ERROR("Invalid parameters");
		return OsError;
	}

	// Sanitize the tls-key
	if (__GlbThreadPoolKey == TLS_KEY_INVALID) {
		__GlbThreadPoolKey = TLSCreateKey(NULL);
	}

	// Allocate a new instance of threadpool
	Tp = (ThreadPool_t*)malloc(sizeof(ThreadPool_t));
	Tp->ThreadsAlive = 0;
	Tp->ThreadsWorking = 0;
	Tp->ThreadsOnHold = 0;
	Tp->ThreadsKeepAlive = 1;

	// Initialize job queue
	if (JobQueueInitialize(&Tp->JobQueue) != OsSuccess) {
		free(Tp);
		return OsError;
	}

	// Initialize the list of threads
	Tp->Threads = (ThreadPoolThread_t**)malloc(NumThreads * sizeof(ThreadPoolThread_t*));

	// Initialize locks
	ConditionConstruct(&Tp->ThreadsIdle);
	MutexConstruct(&Tp->ThreadLock, MUTEX_PLAIN);

	// Spawn threads
	for (i = 0; i < NumThreads; i++) {
		ThreadPoolInitializeThread(Tp, &Tp->Threads[i], i);
	}

	// Wait for all threads to spin-up
	while (Tp->ThreadsAlive != NumThreads);

	// Update out
	*ThreadPool = Tp;

	// Done - no errors
	return OsSuccess;
}

/* ThreadPoolAddWork
 * Takes an action and its argument and adds it to the threadpool's job queue. 
 * If you want to add to work a function with more than one arguments then
 * a way to implement this is by passing a pointer to a structure. */
OsStatus_t
ThreadPoolAddWork(
	_In_ ThreadPool_t *ThreadPool,
	_In_ ThreadFunc_t Function,
	_In_ void *Argument)
{
	// Variables
	ThreadPoolJob_t *Job = NULL;

	// Sanitize parameters
	if (ThreadPool == NULL) {
		return OsError;
	}

	// Allocate a new instance of the thread job
	Job = (ThreadPoolJob_t*)malloc(sizeof(ThreadPoolJob_t));
	Job->Function = Function;
	Job->Argument = Argument;

	// Add the job to our queue
	return JobQueuePush(&ThreadPool->JobQueue, Job);
}

/* ThreadPoolWait
 * Will wait for all jobs - both queued and currently running to finish.
 * Once the queue is empty and all work has completed, the calling thread
 * (probably the main program) will continue. */
OsStatus_t
ThreadPoolWait(
	_In_ ThreadPool_t *ThreadPool)
{
	// Sanitize parameters
	if (ThreadPool == NULL) {
		return OsError;
	}

	// Grab a hold of the mutex
	MutexLock(&ThreadPool->ThreadLock);

	// Now wait for all threads
	while (ThreadPool->JobQueue.Length || ThreadPool->ThreadsWorking) {
		ConditionWait(&ThreadPool->ThreadsIdle, &ThreadPool->ThreadLock);
	}

	// Done, unlock again
	MutexUnlock(&ThreadPool->ThreadLock);
	return OsSuccess;
}

/* ThreadPoolPause
 * The threads will be paused no matter if they are idle or working.
 * The threads return to their previous states once thpool_resume
 * is called. */
OsStatus_t
ThreadPoolPause(
	_In_ ThreadPool_t *ThreadPool)
{
	// Variables
	int i;

	// Sanitize the parameters
	if (ThreadPool == NULL) {
		return OsError;
	}

	// Iterate and pause threads
	for (i = 0; i < ThreadPool->ThreadsAlive; i++) {
		ThreadSignal(ThreadPool->Threads[i]->Thread, SIGUSR1);
	}

	// Done
	return OsSuccess;
}

/* ThreadPoolResume
 * Unpauses all threads if they are paused. */
OsStatus_t
ThreadPoolResume(
	_In_ ThreadPool_t *ThreadPool)
{
	// Sanitize the parameters
	if (ThreadPool == NULL) {
		return OsError;
	}

	// Resume
	ThreadPool->ThreadsOnHold = 0;
	return OsSuccess;
}

/* ThreadPoolDestroy
 * This will wait for the currently active threads to finish and then 'kill'
 * the whole threadpool to free up memory. */
OsStatus_t
ThreadPoolDestroy(
	_In_ ThreadPool_t *ThreadPool)
{
	// Variables
	volatile int ThreadsTotal;
	double TimeOut = 1.0;
	double TimePassed = 0.0;
	time_t Start, End;
	int i;

	// Sanitize the parameters
	if (ThreadPool == NULL) {
		return OsError;
	}

	// Store information and end infinite loop
	ThreadsTotal = ThreadPool->ThreadsAlive;
	ThreadPool->ThreadsKeepAlive = 0;

	// Give it a second to shut-down threads
	time(&Start);
	while (TimePassed < TimeOut && ThreadPool->ThreadsAlive) {
		BinarySemaphorePostAll(ThreadPool->JobQueue.HasJobs);
		time(&End);
		TimePassed = difftime(End, Start);
	}

	// Wait for remaining threads to shut-down
	while (ThreadPool->ThreadsAlive) {
		BinarySemaphorePostAll(ThreadPool->JobQueue.HasJobs);
		ThreadSleep(1);
	}

	// Cleanup job-queue
	JobQueueDestroy(&ThreadPool->JobQueue);
	
	// Deallocate threading resources
	for (i = 0; i < ThreadsTotal; i++) {
		ThreadPoolThreadDestroy(ThreadPool->Threads[i]);
	}

	// Cleanup
	free(ThreadPool->Threads);
	free(ThreadPool);
	return OsSuccess;
}

/* ThreadPoolGetWorkingCount
 * Returns the number of working threads are the threads that are performing work (not idle). */
size_t
ThreadPoolGetWorkingCount(
	_In_ ThreadPool_t *ThreadPool)
{
	// Sanitize the parameters
	if (ThreadPool == NULL) {
		return 0;
	}

	// Ok - return count
	return ThreadPool->ThreadsWorking;
}
