#include "config.h"  // IWYU pragma: keep

#include "iothread.h"

#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <queue>

#include "common.h"
#include "flog.h"
#include "global_safety.h"
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

static void iothread_service_main_thread_requests();
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
    std::atomic<bool> done{false};
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
static std::mutex s_main_thread_performer_lock;               // protects the main thread requests
static std::condition_variable s_main_thread_performer_cond;  // protects the main thread requests

/// The queue of main thread requests. This queue contains pointers to structs that are
/// stack-allocated on the requesting thread.
static owning_lock<std::queue<main_thread_request_t *>> s_main_thread_request_queue;

// Pipes used for notifying.
struct notify_pipes_t {
    int read;
    int write;
};

/// \return the (immortal) set of pipes used for notifying of completions.
static const notify_pipes_t &get_notify_pipes() {
    static const notify_pipes_t s_notify_pipes = [] {
        int pipes[2] = {0, 0};
        assert_with_errno(pipe(pipes) != -1);
        set_cloexec(pipes[0]);
        set_cloexec(pipes[1]);
        // Mark both ends as non-blocking.
        for (int fd : pipes) {
            if (make_fd_nonblocking(fd)) {
                wperror(L"fcntl");
            }
        }
        return notify_pipes_t{pipes[0], pipes[1]};
    }();
    return s_notify_pipes;
}

static bool dequeue_spawn_request(spawn_request_t *result) {
    auto requests = s_spawn_requests.acquire();
    if (!requests->request_queue.empty()) {
        *result = std::move(requests->request_queue.front());
        requests->request_queue.pop();
        return true;
    }
    return false;
}

static void enqueue_thread_result(spawn_request_t req) {
    s_result_queue.acquire()->push(std::move(req));
}

static void *this_thread() { return (void *)(intptr_t)pthread_self(); }

/// The function that does thread work.
static void *iothread_worker(void *unused) {
    UNUSED(unused);
    struct spawn_request_t req;
    while (dequeue_spawn_request(&req)) {
        debug(5, "pthread %p dequeued", this_thread());

        // Perform the work
        req.handler();

        // If there's a completion handler, we have to enqueue it on the result queue.
        // Note we're using std::function's weirdo operator== here
        if (req.completion != nullptr) {
            // Enqueue the result, and tell the main thread about it.
            enqueue_thread_result(std::move(req));
            const char wakeup_byte = IO_SERVICE_RESULT_QUEUE;
            int notify_fd = get_notify_pipes().write;
            assert_with_errno(write_loop(notify_fd, &wakeup_byte, sizeof wakeup_byte) != -1);
        }
    }

    // We believe we have exhausted the thread request queue. We want to decrement
    // thread_count and exit. But it's possible that a request just came in. Furthermore,
    // it's possible that the main thread saw that thread_count is full, and decided to not
    // spawn a new thread, trusting in one of the existing threads to handle it. But we've already
    // committed to not handling anything else. Therefore, we have to decrement
    // the thread count under the lock, which we still hold. Likewise, the main thread must
    // check the value under the lock.
    int new_thread_count = --s_spawn_requests.acquire()->thread_count;
    assert(new_thread_count >= 0);

    debug(5, "pthread %p exiting", this_thread());
    // We're done.
    return nullptr;
}

/// Spawn another thread. No lock is held when this is called.
static void iothread_spawn() {
    // Spawn a thread. If this fails, it means there's already a bunch of threads; it is very
    // unlikely that they are all on the verge of exiting, so one is likely to be ready to handle
    // extant requests. So we can ignore failure with some confidence.
    pthread_t thread = 0;
    if (make_pthread(&thread, iothread_worker, nullptr)) {
        // We will never join this thread.
        DIE_ON_FAILURE(pthread_detach(thread));
    }
}

int iothread_perform_impl(void_function_t &&func, void_function_t &&completion) {
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_NOT_FORKED_CHILD();

    struct spawn_request_t req(std::move(func), std::move(completion));
    int local_thread_count = -1;
    bool spawn_new_thread = false;
    {
        // Lock around a local region.
        auto spawn_reqs = s_spawn_requests.acquire();
        spawn_reqs->request_queue.push(std::move(req));
        if (spawn_reqs->thread_count < IO_MAX_THREADS) {
            spawn_reqs->thread_count++;
            spawn_new_thread = true;
        }
        local_thread_count = spawn_reqs->thread_count;
    }

    // Kick off the thread if we decided to do so.
    if (spawn_new_thread) {
        iothread_spawn();
    }
    return local_thread_count;
}

int iothread_port() { return get_notify_pipes().read; }

void iothread_service_completion() {
    ASSERT_IS_MAIN_THREAD();
    // Drain the read buffer, and then service completions.
    // The order is important.
    int port = iothread_port();
    char buff[256];
    while (read(port, buff, sizeof buff) > 0) {
        // pass
    }

    iothread_service_main_thread_requests();
    iothread_service_result_queue();
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
    int ret = select(fd + 1, &fds, nullptr, nullptr, &tv);
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
void iothread_drain_all() {
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_NOT_FORKED_CHILD();

#define TIME_DRAIN 0
#if TIME_DRAIN
    int thread_count = s_spawn_requests.acquire().value.thread_count;
    double now = timef();
#endif

    // Nasty polling via select().
    while (s_spawn_requests.acquire()->thread_count > 0) {
        if (iothread_wait_for_pending_completions(1000)) {
            iothread_service_completion();
        }
    }
#if TIME_DRAIN
    double after = timef();
    std::fwprintf(stdout, L"(Waited %.02f msec for %d thread(s) to drain)\n", 1000 * (after - now),
                  thread_count);
#endif
}

/// "Do on main thread" support.
static void iothread_service_main_thread_requests() {
    ASSERT_IS_MAIN_THREAD();

    // Move the queue to a local variable.
    std::queue<main_thread_request_t *> request_queue;
    s_main_thread_request_queue.acquire()->swap(request_queue);

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
        // TODO: revisit this logic, this feels sketchy.
        scoped_lock broadcast_lock(s_main_thread_performer_lock);
        s_main_thread_performer_cond.notify_all();
    }
}

// Service the queue of results
static void iothread_service_result_queue() {
    // Move the queue to a local variable.
    std::queue<spawn_request_t> result_queue;
    s_result_queue.acquire()->swap(result_queue);

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

    // Append it. Ensure we don't hold the lock after.
    s_main_thread_request_queue.acquire()->push(&req);

    // Tell the pipe.
    const char wakeup_byte = IO_SERVICE_MAIN_THREAD_REQUEST_QUEUE;
    int notify_fd = get_notify_pipes().write;
    assert_with_errno(write_loop(notify_fd, &wakeup_byte, sizeof wakeup_byte) != -1);

    // Wait on the condition, until we're done.
    std::unique_lock<std::mutex> perform_lock(s_main_thread_performer_lock);
    while (!req.done) {
        // It would be nice to support checking for cancellation here, but the clients need a
        // deterministic way to clean up to avoid leaks
        s_main_thread_performer_cond.wait(perform_lock);
    }

    // Ok, the request must now be done.
    assert(req.done);
}

bool make_pthread(pthread_t *result, void *(*func)(void *), void *param) {
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
    int err = pthread_create(&thread, nullptr, func, param);
    if (err == 0) {
        // Success, return the thread.
        debug(5, "pthread %p spawned", (void *)(intptr_t)thread);
        *result = thread;
    } else {
        perror("pthread_create");
    }
    // Restore our sigmask.
    DIE_ON_FAILURE(pthread_sigmask(SIG_SETMASK, &saved_set, nullptr));
    return err == 0;
}

using void_func_t = std::function<void(void)>;

static void *func_invoker(void *param) {
    // Acquire a thread id for this thread.
    (void)thread_id();
    void_func_t *vf = static_cast<void_func_t *>(param);
    (*vf)();
    delete vf;
    return nullptr;
}

bool make_pthread(pthread_t *result, void_func_t &&func) {
    // Copy the function into a heap allocation.
    void_func_t *vf = new void_func_t(std::move(func));
    if (make_pthread(result, func_invoker, vf)) {
        return true;
    }
    // Thread spawning failed, clean up our heap allocation.
    delete vf;
    return false;
}

static uint64_t next_thread_id() {
    // Note 0 is an invalid thread id.
    static owning_lock<uint64_t> s_last_thread_id{};
    auto tid = s_last_thread_id.acquire();
    return ++*tid;
}

uint64_t thread_id() {
    static thread_local uint64_t tl_tid = next_thread_id();
    return tl_tid;
}
