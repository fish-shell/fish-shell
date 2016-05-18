#include "config.h"  // IWYU pragma: keep

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#include <queue>

#include "common.h"
#include "iothread.h"

#ifdef _POSIX_THREAD_THREADS_MAX
#if _POSIX_THREAD_THREADS_MAX < 64
#define IO_MAX_THREADS _POSIX_THREAD_THREADS_MAX
#endif
#endif

#ifndef IO_MAX_THREADS
#define IO_MAX_THREADS 64
#endif

// Values for the wakeup bytes sent to the ioport.
#define IO_SERVICE_MAIN_THREAD_REQUEST_QUEUE 99
#define IO_SERVICE_RESULT_QUEUE 100

#define IOTHREAD_LOG if (0)

static void iothread_service_main_thread_requests(void);
static void iothread_service_result_queue();

struct SpawnRequest_t {
    int (*handler)(void *);
    void (*completionCallback)(void *, int);
    void *context;
    int handlerResult;
};

struct MainThreadRequest_t {
    int (*handler)(void *);
    void *context;
    volatile int handlerResult;
    volatile bool done;
};

// Spawn support. Requests are allocated and come in on request_queue. They go out on result_queue,
// at which point they can be deallocated. s_active_thread_count is also protected by the lock.
static pthread_mutex_t s_spawn_queue_lock;
static std::queue<SpawnRequest_t *> s_request_queue;
static int s_active_thread_count;

static pthread_mutex_t s_result_queue_lock;
static std::queue<SpawnRequest_t *> s_result_queue;

// "Do on main thread" support.
static pthread_mutex_t s_main_thread_performer_lock;      // protects the main thread requests
static pthread_cond_t s_main_thread_performer_condition;  // protects the main thread requests
static pthread_mutex_t s_main_thread_request_queue_lock;  // protects the queue
static std::queue<MainThreadRequest_t *> s_main_thread_request_queue;

// Notifying pipes.
static int s_read_pipe, s_write_pipe;

static void iothread_init(void) {
    static bool inited = false;
    if (!inited) {
        inited = true;

        // Initialize some locks.
        VOMIT_ON_FAILURE(pthread_mutex_init(&s_spawn_queue_lock, NULL));
        VOMIT_ON_FAILURE(pthread_mutex_init(&s_result_queue_lock, NULL));
        VOMIT_ON_FAILURE(pthread_mutex_init(&s_main_thread_request_queue_lock, NULL));
        VOMIT_ON_FAILURE(pthread_mutex_init(&s_main_thread_performer_lock, NULL));
        VOMIT_ON_FAILURE(pthread_cond_init(&s_main_thread_performer_condition, NULL));

        // Initialize the completion pipes.
        int pipes[2] = {0, 0};
        VOMIT_ON_FAILURE(pipe(pipes));
        s_read_pipe = pipes[0];
        s_write_pipe = pipes[1];

        // 0 means success to VOMIT_ON_FAILURE. Arrange to pass 0 if fcntl returns anything other
        // than -1.
        VOMIT_ON_FAILURE(-1 == fcntl(s_read_pipe, F_SETFD, FD_CLOEXEC));
        VOMIT_ON_FAILURE(-1 == fcntl(s_write_pipe, F_SETFD, FD_CLOEXEC));
    }
}

static void add_to_queue(struct SpawnRequest_t *req) {
    ASSERT_IS_LOCKED(s_spawn_queue_lock);
    s_request_queue.push(req);
}

static SpawnRequest_t *dequeue_spawn_request(void) {
    ASSERT_IS_LOCKED(s_spawn_queue_lock);
    SpawnRequest_t *result = NULL;
    if (!s_request_queue.empty()) {
        result = s_request_queue.front();
        s_request_queue.pop();
    }
    return result;
}

static void enqueue_thread_result(SpawnRequest_t *req) {
    scoped_lock lock(s_result_queue_lock);
    s_result_queue.push(req);
}

static void *this_thread() { return (void *)(intptr_t)pthread_self(); }

/// The function that does thread work.
static void *iothread_worker(void *unused) {
    scoped_lock locker(s_spawn_queue_lock);
    struct SpawnRequest_t *req;
    while ((req = dequeue_spawn_request()) != NULL) {
        IOTHREAD_LOG fprintf(stderr, "pthread %p dequeued %p\n", this_thread(), req);
        // Unlock the queue while we execute the request.
        locker.unlock();

        // Perform the work.
        req->handlerResult = req->handler(req->context);

        // If there's a completion handler, we have to enqueue it on the result queue. Otherwise, we
        // can just delete the request!
        if (req->completionCallback == NULL) {
            delete req;
        } else {
            // Enqueue the result, and tell the main thread about it.
            enqueue_thread_result(req);
            const char wakeup_byte = IO_SERVICE_RESULT_QUEUE;
            VOMIT_ON_FAILURE(!write_loop(s_write_pipe, &wakeup_byte, sizeof wakeup_byte));
        }

        // Lock us up again.
        locker.lock();
    }

    // We believe we have exhausted the thread request queue. We want to decrement
    // s_active_thread_count and exit. But it's possible that a request just came in. Furthermore,
    // it's possible that the main thread saw that s_active_thread_count is full, and decided to not
    // spawn a new thread, trusting in one of the existing threads to handle it. But we've already
    // committed to not handling anything else. Therefore, we have to decrement
    // s_active_thread_count under the lock, which we still hold. Likewise, the main thread must
    // check the value under the lock.
    ASSERT_IS_LOCKED(s_spawn_queue_lock);
    assert(s_active_thread_count > 0);
    s_active_thread_count -= 1;

    IOTHREAD_LOG fprintf(stderr, "pthread %p exiting\n", this_thread());
    // We're done.
    return NULL;
}

/// Spawn another thread. No lock is held when this is called.
static void iothread_spawn() {
    // The spawned thread inherits our signal mask. We don't want the thread to ever receive signals
    // on the spawned thread, so temporarily block all signals, spawn the thread, and then restore
    // it.
    sigset_t new_set, saved_set;
    sigfillset(&new_set);
    VOMIT_ON_FAILURE(pthread_sigmask(SIG_BLOCK, &new_set, &saved_set));

    // Spawn a thread. If this fails, it means there's already a bunch of threads; it is very
    // unlikely that they are all on the verge of exiting, so one is likely to be ready to handle
    // extant requests. So we can ignore failure with some confidence.
    pthread_t thread = 0;
    pthread_create(&thread, NULL, iothread_worker, NULL);

    // We will never join this thread.
    VOMIT_ON_FAILURE(pthread_detach(thread));
    IOTHREAD_LOG fprintf(stderr, "pthread %p spawned\n", (void *)(intptr_t)thread);
    // Restore our sigmask.
    VOMIT_ON_FAILURE(pthread_sigmask(SIG_SETMASK, &saved_set, NULL));
}

int iothread_perform_base(int (*handler)(void *), void (*completionCallback)(void *, int),
                          void *context) {
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_NOT_FORKED_CHILD();
    iothread_init();

    // Create and initialize a request.
    struct SpawnRequest_t *req = new SpawnRequest_t();
    req->handler = handler;
    req->completionCallback = completionCallback;
    req->context = context;

    int local_thread_count = -1;
    bool spawn_new_thread = false;
    {
        // Lock around a local region. Note that we can only access s_active_thread_count under the
        // lock.
        scoped_lock lock(s_spawn_queue_lock);
        add_to_queue(req);
        if (s_active_thread_count < IO_MAX_THREADS) {
            s_active_thread_count++;
            spawn_new_thread = true;
        }
        local_thread_count = s_active_thread_count;
    }

    // Kick off the thread if we decided to do so.
    if (spawn_new_thread) {
        iothread_spawn();
    }

    // We return the active thread count for informational purposes only.
    return local_thread_count;
}

int iothread_port(void) {
    iothread_init();
    return s_read_pipe;
}

void iothread_service_completion(void) {
    ASSERT_IS_MAIN_THREAD();
    char wakeup_byte = 0;
    VOMIT_ON_FAILURE(1 != read_loop(iothread_port(), &wakeup_byte, sizeof wakeup_byte));
    switch (wakeup_byte) {
        case IO_SERVICE_MAIN_THREAD_REQUEST_QUEUE: {
            iothread_service_main_thread_requests();
            break;
        }
        case IO_SERVICE_RESULT_QUEUE: {
            iothread_service_result_queue();
            break;
        }
        default: {
            fprintf(stderr, "Unknown wakeup byte %02x in %s\n", wakeup_byte, __FUNCTION__);
            break;
        }
    }
}

static bool iothread_wait_for_pending_completions(long timeout_usec) {
    const long usec_per_sec = 1000000;
    struct timeval tv;
    tv.tv_sec = timeout_usec / usec_per_sec;
    tv.tv_usec = timeout_usec % usec_per_sec;
    const int fd = iothread_port();
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    int ret = select(fd + 1, &fds, NULL, NULL, &tv);
    return ret > 0;
}

/// Note that this function is quite sketchy. In particular, it drains threads, not requests,
/// meaning that it may leave requests on the queue. This is the desired behavior (it may be called
/// before fork, and we don't want to bother servicing requests before we fork), but in the test
/// suite we depend on it draining all requests. In practice, this works, because a thread in
/// practice won't exit while there is outstanding requests.
///
/// At the moment, this function is only used in the test suite and in a
/// drain-all-threads-before-fork compatibility mode that no architecture requires, so it's OK that
/// it's terrible.
void iothread_drain_all(void) {
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_NOT_FORKED_CHILD();

    scoped_lock locker(s_spawn_queue_lock);

#define TIME_DRAIN 0
#if TIME_DRAIN
    int thread_count = s_active_thread_count;
    double now = timef();
#endif

    // Nasty polling via select().
    while (s_active_thread_count > 0) {
        locker.unlock();
        if (iothread_wait_for_pending_completions(1000)) {
            iothread_service_completion();
        }
        locker.lock();
    }
#if TIME_DRAIN
    double after = timef();
    printf("(Waited %.02f msec for %d thread(s) to drain)\n", 1000 * (after - now), thread_count);
#endif
}

/// "Do on main thread" support.
static void iothread_service_main_thread_requests(void) {
    ASSERT_IS_MAIN_THREAD();

    // Move the queue to a local variable.
    std::queue<MainThreadRequest_t *> request_queue;
    {
        scoped_lock queue_lock(s_main_thread_request_queue_lock);
        std::swap(request_queue, s_main_thread_request_queue);
    }

    if (!request_queue.empty()) {
        // Perform each of the functions. Note we are NOT responsible for deleting these. They are
        // stack allocated in their respective threads!
        while (!request_queue.empty()) {
            MainThreadRequest_t *req = request_queue.front();
            request_queue.pop();
            req->handlerResult = req->handler(req->context);
            req->done = true;
        }

        // Ok, we've handled everybody. Announce the good news, and allow ourselves to be unlocked.
        // Note we must do this while holding the lock. Otherwise we race with the waiting threads:
        //
        // 1. waiting thread checks for done, sees false
        // 2. main thread performs request, sets done to true, posts to condition
        // 3. waiting thread unlocks lock, waits on condition (forever)
        //
        // Because the waiting thread performs step 1 under the lock, if we take the lock, we avoid
        // posting before the waiting thread is waiting.
        scoped_lock broadcast_lock(s_main_thread_performer_lock);
        VOMIT_ON_FAILURE(pthread_cond_broadcast(&s_main_thread_performer_condition));
    }
}

/* Service the queue of results */
static void iothread_service_result_queue() {
    // Move the queue to a local variable.
    std::queue<SpawnRequest_t *> result_queue;
    {
        scoped_lock queue_lock(s_result_queue_lock);
        std::swap(result_queue, s_result_queue);
    }

    // Perform each completion in order. We are responsibile for cleaning them up.
    while (!result_queue.empty()) {
        SpawnRequest_t *req = result_queue.front();
        result_queue.pop();
        if (req->completionCallback) {
            req->completionCallback(req->context, req->handlerResult);
        }
        delete req;
    }
}

int iothread_perform_on_main_base(int (*handler)(void *), void *context) {
    // If this is the main thread, just do it.
    if (is_main_thread()) {
        return handler(context);
    }

    // Make a new request. Note we are synchronous, so this can be stack allocated!
    MainThreadRequest_t req;
    req.handler = handler;
    req.context = context;
    req.handlerResult = 0;
    req.done = false;

    // Append it. Do not delete the nested scope as it is crucial to the proper functioning of this
    // code by virtue of the lock management.
    {
        scoped_lock queue_lock(s_main_thread_request_queue_lock);
        s_main_thread_request_queue.push(&req);
    }

    // Tell the pipe.
    const char wakeup_byte = IO_SERVICE_MAIN_THREAD_REQUEST_QUEUE;
    VOMIT_ON_FAILURE(!write_loop(s_write_pipe, &wakeup_byte, sizeof wakeup_byte));

    // Wait on the condition, until we're done.
    scoped_lock perform_lock(s_main_thread_performer_lock);
    while (!req.done) {
        // It would be nice to support checking for cancellation here, but the clients need a
        // deterministic way to clean up to avoid leaks
        VOMIT_ON_FAILURE(
            pthread_cond_wait(&s_main_thread_performer_condition, &s_main_thread_performer_lock));
    }

    // Ok, the request must now be done.
    assert(req.done);
    return req.handlerResult;
}
