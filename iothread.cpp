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

/* A special "thread index" that means service main thread requests */
#define IO_SERVICE_MAIN_THREAD_REQUEST_QUEUE 99

static void iothread_service_main_thread_requests(void);

static int s_active_thread_count;

typedef unsigned char ThreadIndex_t;

static struct WorkerThread_t
{
    ThreadIndex_t idx;
    pthread_t thread;
} threads[IO_MAX_THREADS];

struct SpawnRequest_t
{
    int (*handler)(void *);
    void (*completionCallback)(void *, int);
    void *context;
    int handlerResult;
};

struct MainThreadRequest_t
{
    int (*handler)(void *);
    void *context;
    volatile int handlerResult;
    volatile bool done;
};

static struct WorkerThread_t *next_vacant_thread_slot(void)
{
    for (ThreadIndex_t i=0; i < IO_MAX_THREADS; i++)
    {
        if (! threads[i].thread) return &threads[i];
    }
    return NULL;
}

/* Spawn support */
static pthread_mutex_t s_spawn_queue_lock;
static std::queue<SpawnRequest_t *> s_request_queue;

/* "Do on main thread" support */
static pthread_mutex_t s_main_thread_performer_lock; // protects the main thread requests
static pthread_cond_t s_main_thread_performer_condition; //protects the main thread requests
static pthread_mutex_t s_main_thread_request_queue_lock; // protects the queue
static std::queue<MainThreadRequest_t *> s_main_thread_request_queue;

/* Notifying pipes */
static int s_read_pipe, s_write_pipe;

static void iothread_init(void)
{
    static bool inited = false;
    if (! inited)
    {
        inited = true;

        /* Initialize some locks */
        VOMIT_ON_FAILURE(pthread_mutex_init(&s_spawn_queue_lock, NULL));
        VOMIT_ON_FAILURE(pthread_mutex_init(&s_main_thread_request_queue_lock, NULL));
        VOMIT_ON_FAILURE(pthread_mutex_init(&s_main_thread_performer_lock, NULL));
        VOMIT_ON_FAILURE(pthread_cond_init(&s_main_thread_performer_condition, NULL));

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

static void add_to_queue(struct SpawnRequest_t *req)
{
    ASSERT_IS_LOCKED(s_spawn_queue_lock);
    s_request_queue.push(req);
}

static SpawnRequest_t *dequeue_spawn_request(void)
{
    SpawnRequest_t *result = NULL;
    scoped_lock lock(s_spawn_queue_lock);
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
    struct SpawnRequest_t *req = dequeue_spawn_request();

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
    ASSERT_IS_LOCKED(s_spawn_queue_lock);
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
    struct SpawnRequest_t *req = new SpawnRequest_t();
    req->handler = handler;
    req->completionCallback = completionCallback;
    req->context = context;

    /* Take our lock */
    scoped_lock lock(s_spawn_queue_lock);

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

    if (threadIdx == IO_SERVICE_MAIN_THREAD_REQUEST_QUEUE)
    {
        iothread_service_main_thread_requests();
    }
    else
    {
        assert(threadIdx < IO_MAX_THREADS);

        struct WorkerThread_t *thread = &threads[threadIdx];
        assert(thread->thread != 0);

        struct SpawnRequest_t *req = NULL;
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
        scoped_lock locker(s_spawn_queue_lock);
        iothread_spawn_if_needed();
    }
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

/* "Do on main thread" support */

static void iothread_service_main_thread_requests(void)
{
    ASSERT_IS_MAIN_THREAD();

    // Move the queue to a local variable
    std::queue<MainThreadRequest_t *> request_queue;
    {
        scoped_lock queue_lock(s_main_thread_request_queue_lock);
        std::swap(request_queue, s_main_thread_request_queue);
    }

    if (! request_queue.empty())
    {
        // Perform each of the functions
        // Note we are NOT responsible for deleting these. They are stack allocated in their respective threads!
        scoped_lock cond_lock(s_main_thread_performer_lock);
        while (! request_queue.empty())
        {
            MainThreadRequest_t *req = request_queue.front();
            request_queue.pop();
            req->handlerResult = req->handler(req->context);
            req->done = true;
        }

        // Ok, we've handled everybody. Announce the good news, and allow ourselves to be unlocked
        VOMIT_ON_FAILURE(pthread_cond_broadcast(&s_main_thread_performer_condition));
    }
}

int iothread_perform_on_main_base(int (*handler)(void *), void *context)
{
    // If this is the main thread, just do it
    if (is_main_thread())
    {
        return handler(context);
    }

    // Make a new request. Note we are synchronous, so this can be stack allocated!
    MainThreadRequest_t req;
    req.handler = handler;
    req.context = context;
    req.handlerResult = 0;
    req.done = false;

    // Append it
    {
        scoped_lock queue_lock(s_main_thread_request_queue_lock);
        s_main_thread_request_queue.push(&req);
    }

    // Tell the pipe
    const ThreadIndex_t idx = IO_SERVICE_MAIN_THREAD_REQUEST_QUEUE;
    VOMIT_ON_FAILURE(! write_loop(s_write_pipe, (const char *)&idx, sizeof idx));

    // Wait on the condition, until we're done
    scoped_lock perform_lock(s_main_thread_performer_lock);
    while (! req.done)
    {
        // It would be nice to support checking for cancellation here, but the clients need a deterministic way to clean up to avoid leaks
        VOMIT_ON_FAILURE(pthread_cond_wait(&s_main_thread_performer_condition, &s_main_thread_performer_lock));
    }

    // Ok, the request must now be done
    assert(req.done);
    return req.handlerResult;
}
