#include "iothread.h"
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>


#define VOMIT_ON_FAILURE(a) do { if (0 != (a)) { int err = errno; fprintf(stderr, "%s failed on line %d in file %s: %d (%s)\n", #a, __LINE__, __FILE__, err, strerror(err)); abort(); }} while (0)

#ifdef _POSIX_THREAD_THREADS_MAX
  #if _POSIX_THREAD_THREADS_MAX < 64
    #define IO_MAX_THREADS _POSIX_THREAD_THREADS_MAX
  #endif
#endif

#ifndef IO_MAX_THREADS
  #define IO_MAX_THREADS 64
#endif

static int s_active_thread_count;

typedef unsigned char ThreadIndex_t;

static struct WorkerThread_t {
	ThreadIndex_t idx;
	pthread_t thread;
} threads[IO_MAX_THREADS];

struct ThreadedRequest_t {
	struct ThreadedRequest_t *next;
	int sequenceNumber;
	
	int (*handler)(void *);
	void (*completionCallback)(void *, int);
	void *context;
	int handlerResult;
};

static struct WorkerThread_t *next_vacant_thread_slot(void) {
	for (ThreadIndex_t i=0; i < IO_MAX_THREADS; i++) {
		if (! threads[i].thread) return &threads[i];
	}
	return NULL;
}

static pthread_mutex_t s_request_queue_lock;
static struct ThreadedRequest_t *s_request_queue_head;
static int s_last_sequence_number;
static int s_read_pipe, s_write_pipe;

static void iothread_init(void) {
	static int inited = 0;
	if (! inited) {
		inited = 1;
		
		/* Initialize the queue lock */
		VOMIT_ON_FAILURE(pthread_mutex_init(&s_request_queue_lock, NULL));
		
		/* Initialize the completion pipes */
		int pipes[2] = {0, 0};
		VOMIT_ON_FAILURE(pipe(pipes));
		s_read_pipe = pipes[0];
		s_write_pipe = pipes[1];
		
		/* Tell each thread its index */
		for (ThreadIndex_t i=0; i < IO_MAX_THREADS; i++) {
			threads[i].idx = i;
		}
	}
}

static void add_to_queue(struct ThreadedRequest_t *req) {
	//requires that the queue lock be held
	if (s_request_queue_head == NULL) {
		s_request_queue_head = req;
	} else {
		struct ThreadedRequest_t *last_in_queue = s_request_queue_head;
		while (last_in_queue->next != NULL) {
			last_in_queue = last_in_queue->next;
		}
		last_in_queue->next = req;
	}
}

/* The function that does thread work. */
static void *iothread_worker(void *threadPtr) {
	assert(threadPtr != NULL);
	struct WorkerThread_t *thread = (struct WorkerThread_t *)threadPtr;
	
    // We don't want to receive signals on this thread
    sigset_t set;
    sigfillset(&set);
    VOMIT_ON_FAILURE(pthread_sigmask(SIG_SETMASK, &set, NULL));
    
	/* Grab a request off of the queue */
	struct ThreadedRequest_t *req;
	VOMIT_ON_FAILURE(pthread_mutex_lock(&s_request_queue_lock));
	req = s_request_queue_head;
	if (req) {
		s_request_queue_head = req->next;
	}
	VOMIT_ON_FAILURE(pthread_mutex_unlock(&s_request_queue_lock));

	/* Run the handler and store the result */
	if (req) {
		req->handlerResult = req->handler(req->context);
	}
	
	/* Write our index to wake up the main thread */
	VOMIT_ON_FAILURE(! write(s_write_pipe, &thread->idx, sizeof thread->idx));
	
	/* We're done */
	return req;
}

/* Spawn another thread if there's work to be done. */
static void iothread_spawn_if_needed(void) {
	if (s_request_queue_head != NULL && s_active_thread_count < IO_MAX_THREADS) {
		struct WorkerThread_t *thread = next_vacant_thread_slot();
		assert(thread != NULL);
		
		/* Spawn a thread */
		int err;
		do {
			err = 0;
			if (pthread_create(&thread->thread, NULL, iothread_worker, thread)) {
				err = errno;
			}
		} while (err == EAGAIN);
		assert(err == 0);
		
		/* Note that we are spawned another thread */
		s_active_thread_count += 1;		
	}
}

int iothread_perform(int (*handler)(void *), void (*completionCallback)(void *, int), void *context) {
	iothread_init();
	
	/* Create and initialize a request. */
	struct ThreadedRequest_t *req = (struct ThreadedRequest_t *)malloc(sizeof *req);
	req->next = NULL;
	req->handler = handler;
	req->completionCallback = completionCallback;
	req->context = context;
	req->sequenceNumber = ++s_last_sequence_number;
	
	/* Take the queue lock */
	VOMIT_ON_FAILURE(pthread_mutex_lock(&s_request_queue_lock));
	
	/* Add to the queue */
	add_to_queue(req);
	
	/* Spawn a thread if necessary */
	iothread_spawn_if_needed();
	
	/* Unlock */
	VOMIT_ON_FAILURE(pthread_mutex_unlock(&s_request_queue_lock));
    
    return 0;
}

int iothread_port(void) {
	iothread_init();
	return s_read_pipe;
}

void iothread_service_completion(void) {
	ThreadIndex_t threadIdx = (ThreadIndex_t)-1;
	VOMIT_ON_FAILURE(! read(iothread_port(), &threadIdx, sizeof threadIdx));
	assert(threadIdx < IO_MAX_THREADS);
	
	struct WorkerThread_t *thread = &threads[threadIdx];
	assert(thread->thread != 0);
	
	struct ThreadedRequest_t *req = NULL;
	VOMIT_ON_FAILURE(pthread_join(thread->thread, (void **)&req));
	
	/* Free up this thread */
	thread->thread = 0;
	assert(s_active_thread_count > 0);
	s_active_thread_count -= 1;
	
	/* Handle the request */
	if (req && req->completionCallback) {
		req->completionCallback(req->context, req->handlerResult);
	}
	
	/* Maybe spawn another thread, if there's more work to be done. */
	VOMIT_ON_FAILURE(pthread_mutex_lock(&s_request_queue_lock));
	iothread_spawn_if_needed();
	VOMIT_ON_FAILURE(pthread_mutex_unlock(&s_request_queue_lock));
}
