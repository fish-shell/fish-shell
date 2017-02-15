#include "config.h"  // IWYU pragma: keep

#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <queue>

#include "common.h"
#include "iothread.h"
#include "wutil.h"

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

static void iothread_service_main_thread_requests(void);
static void iothread_service_result_queue();

typedef std::function<void(void)> void_function_t;

struct spawn_request_t {
    void_function_t handler;
    void_function_t completion;

    spawn_request_t() {}

    spawn_request_t(void_function_t &&f, void_function_t &&comp) : handler(f), completion(comp) {}

    // Move-only
    spawn_request_t &operator=(const spawn_request_t &) = delete;
    spawn_request_t &operator=(spawn_request_t &&) = default;
    spawn_request_t(const spawn_request_t &) = delete;
    spawn_request_t(spawn_request_t &&) = default;
};

struct main_thread_request_t {
    volatile bool done = false;
    void_function_t func;

    main_thread_request_t(void_function_t &&f) : func(f) {}

    // No moving OR copying
    // main_thread_requests are always stack allocated, and we deal in pointers to them
    void operator=(const main_thread_request_t &) = delete;
    main_thread_request_t(const main_thread_request_t &) = delete;
    main_thread_request_t(main_thread_request_t &&) = delete;
};

// Spawn support. Requests are allocated and come in on request_queue and go out on result_queue
struct thread_data_t {
    std::queue<spawn_request_t> request_queue;
    int thread_count = 0;
};
static owning_lock<thread_data_t> s_spawn_requests;
static owning_lock<std::queue<spawn_request_t>> s_result_queue;

// "Do on main thread" support.
static pthread_mutex_t s_main_thread_performer_lock =
    PTHREAD_MUTEX_INITIALIZER;                       // protects the main thread requests
static pthread_cond_t s_main_thread_performer_cond;  // protects the main thread requests
static pthread_mutex_t s_main_thread_request_q_lock =
    PTHREAD_MUTEX_INITIALIZER;  // protects the queue
static std::queue<main_thread_request_t *> s_main_thread_request_queue;

// Notifying pipes.
static int s_read_pipe, s_write_pipe;

static void iothread_init(void) {
    static bool inited = false;
    if (!inited) {
        inited = true;

        // Initialize some locks.
        DIE_ON_FAILURE(pthread_cond_init(&s_main_thread_performer_cond, NULL));

        // Initialize the completion pipes.
        int pipes[2] = {0, 0};
        assert_with_errno(pipe(pipes) != -1);
        s_read_pipe = pipes[0];
        s_write_pipe = pipes[1];

        set_cloexec(s_read_pipe);
        set_cloexec(s_write_pipe);
    }
}

static bool dequeue_spawn_request(spawn_request_t *result) {
    auto locker = s_spawn_requests.acquire();
    thread_data_t &td = locker.value;
    if (!td.request_queue.empty()) {
        *result = std::move(td.request_queue.front());
        td.request_queue.pop();
        return true;
    }
    return false;
}

static void enqueue_thread_result(spawn_request_t req) {
    s_result_queue.acquire().value.push(std::move(req));
}

static void *this_thread() { return (void *)(intptr_t)pthread_self(); }

/// The function that does thread work.
static void *iothread_worker(void *unused) {
    UNUSED(unused);
    struct spawn_request_t req;
    while (dequeue_spawn_request(&req)) {
        debug(5, "pthread %p dequeued\n", this_thread());

        // Perform the work
        req.handler();

        // If there's a completion handler, we have to enqueue it on the result queue.
        // Note we're using std::function's weirdo operator== here
        if (req.completion != nullptr) {
            // Enqueue the result, and tell the main thread about it.
            enqueue_thread_result(std::move(req));
            const char wakeup_byte = IO_SERVICE_RESULT_QUEUE;
            assert_with_errno(write_loop(s_write_pipe, &wakeup_byte, sizeof wakeup_byte) != -1);
        }
    }

    // We believe we have exhausted the thread request queue. We want to decrement
    // thread_count and exit. But it's possible that a request just came in. Furthermore,
    // it's possible that the main thread saw that thread_count is full, and decided to not
    // spawn a new thread, trusting in one of the existing threads to handle it. But we've already
    // committed to not handling anything else. Therefore, we have to decrement
    // the thread count under the lock, which we still hold. Likewise, the main thread must
    // check the value under the lock.
    int new_thread_count = --s_spawn_requests.acquire().value.thread_count;
    assert(new_thread_count >= 0);

    debug(5, "pthread %p exiting\n", this_thread());
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
    DIE_ON_FAILURE(pthread_sigmask(SIG_BLOCK, &new_set, &saved_set));

    // Spawn a thread. If this fails, it means there's already a bunch of threads; it is very
    // unlikely that they are all on the verge of exiting, so one is likely to be ready to handle
    // extant requests. So we can ignore failure with some confidence.
    pthread_t thread = 0;
    pthread_create(&thread, NULL, iothread_worker, NULL);

    // We will never join this thread.
    DIE_ON_FAILURE(pthread_detach(thread));
    debug(5, "pthread %p spawned\n", (void *)(intptr_t)thread);
    // Restore our sigmask.
    DIE_ON_FAILURE(pthread_sigmask(SIG_SETMASK, &saved_set, NULL));
}

int iothread_perform_impl(void_function_t &&func, void_function_t &&completion) {
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_NOT_FORKED_CHILD();
    iothread_init();

    struct spawn_request_t req(std::move(func), std::move(completion));
    int local_thread_count = -1;
    bool spawn_new_thread = false;
    {
        // Lock around a local region.
        auto locker = s_spawn_requests.acquire();
        thread_data_t &td = locker.value;
        td.request_queue.push(std::move(req));
        if (td.thread_count < IO_MAX_THREADS) {
            td.thread_count++;
            spawn_new_thread = true;
        }
        local_thread_count = td.thread_count;
    }

    // Kick off the thread if we decided to do so.
    if (spawn_new_thread) {
        iothread_spawn();
    }
    return local_thread_count;
}

int iothread_port(void) {
    iothread_init();
    return s_read_pipe;
}

void iothread_service_completion(void) {
    ASSERT_IS_MAIN_THREAD();
    char wakeup_byte;

    assert_with_errno(read_loop(iothread_port(), &wakeup_byte, sizeof wakeup_byte) == 1);
    if (wakeup_byte == IO_SERVICE_MAIN_THREAD_REQUEST_QUEUE) {
        iothread_service_main_thread_requests();
    } else if (wakeup_byte == IO_SERVICE_RESULT_QUEUE) {
        iothread_service_result_queue();
    } else {
        debug(0, "Unknown wakeup byte %02x in %s", wakeup_byte, __FUNCTION__);
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

#define TIME_DRAIN 0
#if TIME_DRAIN
    int thread_count = s_spawn_requests.acquire().value.thread_count;
    double now = timef();
#endif

    // Nasty polling via select().
    while (s_spawn_requests.acquire().value.thread_count > 0) {
        if (iothread_wait_for_pending_completions(1000)) {
            iothread_service_completion();
        }
    }
#if TIME_DRAIN
    double after = timef();
    fwprintf(stdout, L"(Waited %.02f msec for %d thread(s) to drain)\n", 1000 * (after - now),
             thread_count);
#endif
}

/// "Do on main thread" support.
static void iothread_service_main_thread_requests(void) {
    ASSERT_IS_MAIN_THREAD();

    // Move the queue to a local variable.
    std::queue<main_thread_request_t *> request_queue;
    {
        scoped_lock queue_lock(s_main_thread_request_q_lock);
        request_queue.swap(s_main_thread_request_queue);
    }

    if (!request_queue.empty()) {
        // Perform each of the functions. Note we are NOT responsible for deleting these. They are
        // stack allocated in their respective threads!
        while (!request_queue.empty()) {
            main_thread_request_t *req = request_queue.front();
            request_queue.pop();
            req->func();
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
        DIE_ON_FAILURE(pthread_cond_broadcast(&s_main_thread_performer_cond));
    }
}

// Service the queue of results
static void iothread_service_result_queue() {
    // Move the queue to a local variable.
    std::queue<spawn_request_t> result_queue;
    s_result_queue.acquire().value.swap(result_queue);

    // Perform each completion in order
    while (!result_queue.empty()) {
        spawn_request_t req(std::move(result_queue.front()));
        result_queue.pop();
        // ensure we don't invoke empty functions, that raises an exception
        if (req.completion != nullptr) {
            req.completion();
        }
    }
}

void iothread_perform_on_main(void_function_t &&func) {
    if (is_main_thread()) {
        func();
        return;
    }

    // Make a new request. Note we are synchronous, so this can be stack allocated!
    main_thread_request_t req(std::move(func));

    // Append it. Do not delete the nested scope as it is crucial to the proper functioning of this
    // code by virtue of the lock management.
    {
        scoped_lock queue_lock(s_main_thread_request_q_lock);
        s_main_thread_request_queue.push(&req);
    }

    // Tell the pipe.
    const char wakeup_byte = IO_SERVICE_MAIN_THREAD_REQUEST_QUEUE;
    assert_with_errno(write_loop(s_write_pipe, &wakeup_byte, sizeof wakeup_byte) != -1);

    // Wait on the condition, until we're done.
    scoped_lock perform_lock(s_main_thread_performer_lock);
    while (!req.done) {
        // It would be nice to support checking for cancellation here, but the clients need a
        // deterministic way to clean up to avoid leaks
        DIE_ON_FAILURE(
            pthread_cond_wait(&s_main_thread_performer_cond, &s_main_thread_performer_lock));
    }

    // Ok, the request must now be done.
    assert(req.done);
}
