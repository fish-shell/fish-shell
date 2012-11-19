#include "config.h"
#include "iothread.h"
#include "common.h"
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <queue>

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

static struct WorkerThread_t
{
    ThreadIndex_t idx;
    pthread_t thread;
} threads[IO_MAX_THREADS];

struct ThreadedRequest_t
{
    int sequenceNumber;

    int (*handler)(void *);
    void (*completionCallback)(void *, int);
    void *context;
    int handlerResult;
};

static struct WorkerThread_t *next_vacant_thread_slot(void)
{
    for (ThreadIndex_t i=0; i < IO_MAX_THREADS; i++)
    {
        if (! threads[i].thread) return &threads[i];
    }
    return NULL;
}

static pthread_mutex_t s_request_queue_lock;
static std::queue<ThreadedRequest_t *> s_request_queue;
static int s_last_sequence_number;
static int s_read_pipe, s_write_pipe;

static void iothread_init(void)
{
    static bool inited = false;
    if (! inited)
    {
        inited = true;

        /* Initialize the queue lock */
        VOMIT_ON_FAILURE(pthread_mutex_init(&s_request_queue_lock, NULL));

        /* Initialize the completion pipes */
        int pipes[2] = {0, 0};
        VOMIT_ON_FAILURE(pipe(pipes));
        s_read_pipe = pipes[0];
        s_write_pipe = pipes[1];

        // 0 means success to VOMIT_ON_FAILURE. Arrange to pass 0 if fcntl returns anything other than -1.
        VOMIT_ON_FAILURE(-1 == fcntl(s_read_pipe, F_SETFD, FD_CLOEXEC));
        VOMIT_ON_FAILURE(-1 == fcntl(s_write_pipe, F_SETFD, FD_CLOEXEC));

        /* Tell each thread its index */
        for (ThreadIndex_t i=0; i < IO_MAX_THREADS; i++)
        {
            threads[i].idx = i;
        }
    }
}

static void add_to_queue(struct ThreadedRequest_t *req)
{
    ASSERT_IS_LOCKED(s_request_queue_lock);
    s_request_queue.push(req);
}

static ThreadedRequest_t *dequeue_request(void)
{
    ThreadedRequest_t *result = NULL;
    scoped_lock lock(s_request_queue_lock);
    if (! s_request_queue.empty())
    {
        result = s_request_queue.front();
        s_request_queue.pop();
    }
    return result;
}

/* The function that does thread work. */
static void *iothread_worker(void *threadPtr)
{
    assert(threadPtr != NULL);
    struct WorkerThread_t *thread = (struct WorkerThread_t *)threadPtr;

    /* Grab a request off of the queue */
    struct ThreadedRequest_t *req = dequeue_request();

    /* Run the handler and store the result */
    if (req)
    {
        req->handlerResult = req->handler(req->context);
    }

    /* Write our index to wake up the main thread */
    VOMIT_ON_FAILURE(! write_loop(s_write_pipe, (const char *)&thread->idx, sizeof thread->idx));

    /* We're done */
    return req;
}

/* Spawn another thread if there's work to be done. */
static void iothread_spawn_if_needed(void)
{
    ASSERT_IS_LOCKED(s_request_queue_lock);
    if (! s_request_queue.empty() && s_active_thread_count < IO_MAX_THREADS)
    {
        struct WorkerThread_t *thread = next_vacant_thread_slot();
        assert(thread != NULL);

        /* The spawned thread inherits our signal mask. We don't want the thread to ever receive signals on the spawned thread, so temporarily block all signals, spawn the thread, and then restore it. */
        sigset_t newSet, savedSet;
        sigfillset(&newSet);
        VOMIT_ON_FAILURE(pthread_sigmask(SIG_BLOCK, &newSet, &savedSet));

        /* Spawn a thread.  */
        int err;
        do
        {
            err = 0;
            if (pthread_create(&thread->thread, NULL, iothread_worker, thread))
            {
                err = errno;
            }
        }
        while (err == EAGAIN);

        /* Need better error handling - perhaps try again later. */
        assert(err == 0);

        /* Note that we are spawned another thread */
        s_active_thread_count += 1;

        /* Restore our sigmask */
        VOMIT_ON_FAILURE(pthread_sigmask(SIG_SETMASK, &savedSet, NULL));
    }
}

int iothread_perform_base(int (*handler)(void *), void (*completionCallback)(void *, int), void *context)
{
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_NOT_FORKED_CHILD();
    iothread_init();

    /* Create and initialize a request. */
    struct ThreadedRequest_t *req = new ThreadedRequest_t();
    req->handler = handler;
    req->completionCallback = completionCallback;
    req->context = context;
    req->sequenceNumber = ++s_last_sequence_number;

    /* Take our lock */
    scoped_lock lock(s_request_queue_lock);

    /* Add to the queue */
    add_to_queue(req);

    /* Spawn a thread if necessary */
    iothread_spawn_if_needed();
    return 0;
}

int iothread_port(void)
{
    iothread_init();
    return s_read_pipe;
}

void iothread_service_completion(void)
{
    ASSERT_IS_MAIN_THREAD();
    ThreadIndex_t threadIdx = (ThreadIndex_t)-1;
    VOMIT_ON_FAILURE(1 != read_loop(iothread_port(), &threadIdx, sizeof threadIdx));
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
    if (req)
    {
        if (req->completionCallback)
            req->completionCallback(req->context, req->handlerResult);
        delete req;
    }

    /* Maybe spawn another thread, if there's more work to be done. */
    VOMIT_ON_FAILURE(pthread_mutex_lock(&s_request_queue_lock));
    iothread_spawn_if_needed();
    VOMIT_ON_FAILURE(pthread_mutex_unlock(&s_request_queue_lock));
}

void iothread_drain_all(void)
{
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_NOT_FORKED_CHILD();
    if (s_active_thread_count == 0)
        return;
#define TIME_DRAIN 0
#if TIME_DRAIN
    int thread_count = s_active_thread_count;
    double now = timef();
#endif
    while (s_active_thread_count > 0)
    {
        iothread_service_completion();
    }
#if TIME_DRAIN
    double after = timef();
    printf("(Waited %.02f msec for %d thread(s) to drain)\n", 1000 * (after - now), thread_count);
#endif
}
