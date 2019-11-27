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
#include <functional>
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

// The amount of time an IO thread many hang around to service requests, in milliseconds.
#define IO_WAIT_FOR_WORK_DURATION_MS 500

static void iothread_service_main_thread_requests();
static void iothread_service_result_queue();

using void_function_t = std::function<void()>;

struct work_request_t {
    void_function_t handler;
    void_function_t completion;

    work_request_t(void_function_t &&f, void_function_t &&comp)
        : handler(std::move(f)), completion(std::move(comp)) {}

    // Move-only
    work_request_t &operator=(const work_request_t &) = delete;
    work_request_t &operator=(work_request_t &&) = default;
    work_request_t(const work_request_t &) = delete;
    work_request_t(work_request_t &&) = default;
};

struct main_thread_request_t {
    relaxed_atomic_bool_t done{false};
    void_function_t func;

    main_thread_request_t(void_function_t &&f) : func(f) {}

    // No moving OR copying
    // main_thread_requests are always stack allocated, and we deal in pointers to them
    void operator=(const main_thread_request_t &) = delete;
    main_thread_request_t(const main_thread_request_t &) = delete;
    main_thread_request_t(main_thread_request_t &&) = delete;
};

struct thread_pool_t {
    struct data_t {
        /// The queue of outstanding, unclaimed requests.
        std::queue<work_request_t> request_queue{};

        /// The number of threads that exist in the pool.
        int total_threads{0};

        /// The number of threads which are waiting for more work.
        int waiting_threads{0};

        /// A flag indicating we should not process new requests.
        bool drain{false};
    };

    /// Data which needs to be atomically accessed.
    owning_lock<data_t> data{};

    /// The condition variable used to wake up waiting threads.
    /// Note this is tied to data's lock.
    std::condition_variable queue_cond{};

    /// The minimum and maximum number of threads.
    /// Here "minimum" means threads that are kept waiting in the pool.
    /// Note that the pool is initially empty and threads may decide to exit based on a time wait.
    const int soft_min_threads;
    const int max_threads;

    thread_pool_t(int soft_min_threads, int max_threads)
        : soft_min_threads(soft_min_threads), max_threads(max_threads) {
        assert(soft_min_threads >= 0 && max_threads >= 1 && soft_min_threads <= max_threads &&
               "Invalid thread min and max");
    }
};

/// The thread pool for "iothreads" which are used to lift I/O off of the main thread.
/// These are used for completions, etc.
static thread_pool_t s_io_thread_pool(1, IO_MAX_THREADS);

static owning_lock<std::queue<work_request_t>> s_result_queue;

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

/// Dequeue a work item (perhaps waiting on the condition variable), or commit to exiting by
/// reducing the active thread count.
static maybe_t<work_request_t> iothread_dequeue_work_or_commit_to_exit() {
    auto &pool = s_io_thread_pool;
    auto data = pool.data.acquire();

    while (data->request_queue.empty()) {
        bool give_up = true;
        if (!data->drain && data->total_threads == pool.soft_min_threads) {
            // If we exit, it will drop below the soft min. So wait for a while before giving up.
            data->waiting_threads += 1;
            auto wait_ret = pool.queue_cond.wait_for(
                data.get_lock(), std::chrono::milliseconds(IO_WAIT_FOR_WORK_DURATION_MS));
            data->waiting_threads -= 1;
            give_up = (wait_ret == std::cv_status::timeout);
        }
        if (give_up) {
            // Balance the total_threads count from when we were spawned.
            data->total_threads -= 1;
            return none();
        }
    }

    // Oh! The queue has work for us!
    maybe_t<work_request_t> result = std::move(data->request_queue.front());
    data->request_queue.pop();
    return result;
}

static void enqueue_thread_result(work_request_t req) {
    s_result_queue.acquire()->push(std::move(req));
}

static void *this_thread() { return (void *)(intptr_t)pthread_self(); }

/// The function that does thread work.
static void *iothread_worker(void *unused) {
    UNUSED(unused);
    while (auto req = iothread_dequeue_work_or_commit_to_exit()) {
        FLOGF(iothread, L"pthread %p got work", this_thread());

        // Perform the work
        req->handler();

        // If there's a completion handler, we have to enqueue it on the result queue.
        // Note we're using std::function's weirdo operator== here
        if (req->completion != nullptr) {
            // Enqueue the result, and tell the main thread about it.
            enqueue_thread_result(req.acquire());
            const char wakeup_byte = IO_SERVICE_RESULT_QUEUE;
            int notify_fd = get_notify_pipes().write;
            assert_with_errno(write_loop(notify_fd, &wakeup_byte, sizeof wakeup_byte) != -1);
        }
    }
    FLOGF(iothread, L"pthread %p exiting", this_thread());
    return nullptr;
}

/// Spawn another thread. No lock is held when this is called.
static pthread_t iothread_spawn() {
    // Spawn a thread. If this fails, it means there's already a bunch of threads; it is very
    // unlikely that they are all on the verge of exiting, so one is likely to be ready to handle
    // extant requests. So we can ignore failure with some confidence.
    pthread_t thread = 0;
    if (make_pthread(&thread, iothread_worker, nullptr)) {
        // We will never join this thread.
        DIE_ON_FAILURE(pthread_detach(thread));
    }
    return thread;
}

int iothread_perform_impl(void_function_t &&func, void_function_t &&completion) {
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_NOT_FORKED_CHILD();
    assert(func && "Null function");

    struct work_request_t req(std::move(func), std::move(completion));
    int local_thread_count = -1;
    auto &pool = s_io_thread_pool;
    bool spawn_new_thread = false;
    bool wakeup_thread = false;
    {
        // Lock around a local region.
        auto data = pool.data.acquire();
        data->request_queue.push(std::move(req));
        if (data->drain) {
            // Do nothing here.
        } else if (data->waiting_threads > 0) {
            // At least one thread is waiting, we will wake it up.
            wakeup_thread = true;
        } else if (data->total_threads < pool.max_threads) {
            // No threads are waiting but we can spawn a new thread.
            data->total_threads += 1;
            spawn_new_thread = true;
        }
        local_thread_count = data->total_threads;
    }

    // Kick off the thread if we decided to do so.
    if (wakeup_thread) {
        pool.queue_cond.notify_one();
    }
    if (spawn_new_thread) {
        pthread_t pt = iothread_spawn();
        FLOGF(iothread, L"pthread %p spawned", pt);
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

/// At the moment, this function is only used in the test suite and in a
/// drain-all-threads-before-fork compatibility mode that no architecture requires, so it's OK that
/// it's terrible.
int iothread_drain_all() {
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_NOT_FORKED_CHILD();

    int thread_count;
    auto &pool = s_io_thread_pool;
    // Set the drain flag.
    {
        auto data = pool.data.acquire();
        assert(!data->drain && "Should not be draining already");
        data->drain = true;
        thread_count = data->total_threads;
    }

    // Wake everyone up.
    pool.queue_cond.notify_all();

    double now = timef();

    // Nasty polling via select().
    while (pool.data.acquire()->total_threads > 0) {
        if (iothread_wait_for_pending_completions(1000)) {
            iothread_service_completion();
        }
    }

    // Clear the drain flag.
    // Even though we released the lock, nobody should have added a new thread while the drain flag
    // is set.
    {
        auto data = pool.data.acquire();
        assert(data->total_threads == 0 && "Should be no threads");
        assert(data->drain && "Should be draining");
        data->drain = false;
    }

    double after = timef();
    FLOGF(iothread, "Drained %d thread(s) in %.02f msec", thread_count, 1000 * (after - now));
    return thread_count;
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
    std::queue<work_request_t> result_queue;
    s_result_queue.acquire()->swap(result_queue);

    // Perform each completion in order
    while (!result_queue.empty()) {
        work_request_t req(std::move(result_queue.front()));
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
