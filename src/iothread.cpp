#include "config.h"  // IWYU pragma: keep

#include "iothread.h"

#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <queue>
#include <thread>

#include "common.h"
#include "fds.h"
#include "flog.h"
#include "global_safety.h"
#include "wutil.h"

// We just define a thread limit of 1024.
// On all systems I've seen the limit is higher,
// but on some (like linux with glibc) the setting for _POSIX_THREAD_THREADS_MAX is 64,
// which is too low, even tho the system can handle more than 64 threads.
#define IO_MAX_THREADS 1024

// iothread has a thread pool. Sometimes there's no work to do, but extant threads wait around for a
// while (on a condition variable) in case new work comes soon. However condition variables are not
// properly instrumented with Thread Sanitizer, so it fails to recognize when our mutex is locked.
// See https://github.com/google/sanitizers/issues/1259
// When using TSan, disable the wait-around feature.
#ifdef FISH_TSAN_WORKAROUNDS
#define IO_WAIT_FOR_WORK_DURATION_MS 0
#else
#define IO_WAIT_FOR_WORK_DURATION_MS 500
#endif

using void_function_t = std::function<void()>;

namespace {
struct work_request_t : noncopyable_t {
    void_function_t handler;
    explicit work_request_t(void_function_t &&f) : handler(std::move(f)) {}
};

struct thread_pool_t : noncopyable_t, nonmovable_t {
    struct data_t {
        /// The queue of outstanding, unclaimed requests.
        std::queue<work_request_t> request_queue{};

        /// The number of threads that exist in the pool.
        size_t total_threads{0};

        /// The number of threads which are waiting for more work.
        size_t waiting_threads{0};

        /// A flag indicating we should not process new requests.
        bool drain{false};
    };

    /// Data which needs to be atomically accessed.
    owning_lock<data_t> req_data{};

    /// The condition variable used to wake up waiting threads.
    /// Note this is tied to data's lock.
    std::condition_variable queue_cond{};

    /// The minimum and maximum number of threads.
    /// Here "minimum" means threads that are kept waiting in the pool.
    /// Note that the pool is initially empty and threads may decide to exit based on a time wait.
    const size_t soft_min_threads;
    const size_t max_threads;

    /// Construct with a soft minimum and maximum thread count.
    thread_pool_t(size_t soft_min_threads, size_t max_threads)
        : soft_min_threads(soft_min_threads), max_threads(max_threads) {}

    /// Enqueue a new work item onto the thread pool.
    /// The function \p func will execute in one of the pool's threads.
    /// If \p cant_wait is set, disrespect the thread limit, because extant threads may
    /// want to wait for new threads.
    int perform(void_function_t &&func, bool cant_wait);

   private:
    /// The worker loop for this thread.
    void *run();

    /// Dequeue a work item (perhaps waiting on the condition variable), or commit to exiting by
    /// reducing the active thread count.
    /// This runs in the background thread.
    maybe_t<work_request_t> dequeue_work_or_commit_to_exit();

    /// Trampoline function for pthread_spawn compatibility.
    static void *run_trampoline(void *vpool);

    /// Attempt to spawn a new pthread.
    bool spawn() const;
};

/// The thread pool for "iothreads" which are used to lift I/O off of the main thread.
/// These are used for completions, etc.
/// Leaked to avoid shutdown dtor registration (including tsan).
static thread_pool_t &s_io_thread_pool = *(new thread_pool_t(1, IO_MAX_THREADS));

/// A queue of "things to do on the main thread."
struct main_thread_queue_t : noncopyable_t {
    // Functions to invoke as the completion callback from debounce.
    std::vector<void_function_t> completions;

    // iothread_perform_on_main requests.
    // Note this contains pointers to structs that are stack-allocated on the requesting thread.
    std::vector<void_function_t> requests;

    /// Transfer ownership of ourselves to a new queue and return it.
    /// 'this' is left empty.
    main_thread_queue_t take() {
        main_thread_queue_t result;
        std::swap(result.completions, this->completions);
        std::swap(result.requests, this->requests);
        return result;
    }

    // Moving is allowed, but not copying.
    main_thread_queue_t() = default;
    main_thread_queue_t(main_thread_queue_t &&) = default;
    main_thread_queue_t &operator=(main_thread_queue_t &&) = default;
};
static owning_lock<main_thread_queue_t> s_main_thread_queue;

/// \return the signaller for completions and main thread requests.
static fd_event_signaller_t &get_notify_signaller() {
    // Leaked to avoid shutdown dtors.
    static auto s_signaller = new fd_event_signaller_t();
    return *s_signaller;
}

/// Dequeue a work item (perhaps waiting on the condition variable), or commit to exiting by
/// reducing the active thread count.
maybe_t<work_request_t> thread_pool_t::dequeue_work_or_commit_to_exit() {
    auto data = this->req_data.acquire();
    // If the queue is empty, check to see if we should wait.
    // We should wait if our exiting would drop us below the soft min.
    if (data->request_queue.empty() && data->total_threads == this->soft_min_threads &&
        IO_WAIT_FOR_WORK_DURATION_MS > 0) {
        data->waiting_threads += 1;
        this->queue_cond.wait_for(data.get_lock(),
                                  std::chrono::milliseconds(IO_WAIT_FOR_WORK_DURATION_MS));
        data->waiting_threads -= 1;
    }

    // Now that we've perhaps waited, see if there's something on the queue.
    maybe_t<work_request_t> result{};
    if (!data->request_queue.empty()) {
        result = std::move(data->request_queue.front());
        data->request_queue.pop();
    }
    // If we are returning none, then ensure we balance the thread count increment from when we were
    // created. This has to be done here in this awkward place because we've already committed to
    // exiting - we will never pick up more work. So we need to ensure we decrement the thread count
    // while holding the lock as we are effectively exited.
    if (!result) {
        data->total_threads -= 1;
    }
    return result;
}

static intptr_t this_thread() { return (intptr_t)pthread_self(); }

void *thread_pool_t::run() {
    while (auto req = dequeue_work_or_commit_to_exit()) {
        FLOGF(iothread, L"pthread %p got work", this_thread());
        // Perform the work
        req->handler();
    }
    FLOGF(iothread, L"pthread %p exiting", this_thread());
    return nullptr;
}

void *thread_pool_t::run_trampoline(void *pool) {
    assert(pool && "No thread pool given");
    return static_cast<thread_pool_t *>(pool)->run();
}

/// Spawn another thread. No lock is held when this is called.
bool thread_pool_t::spawn() const {
    return make_detached_pthread(&run_trampoline, const_cast<thread_pool_t *>(this));
}

int thread_pool_t::perform(void_function_t &&func, bool cant_wait) {
    assert(func && "Missing function");
    // Note we permit an empty completion.
    struct work_request_t req(std::move(func));
    int local_thread_count = -1;
    auto &pool = s_io_thread_pool;
    bool spawn_new_thread = false;
    bool wakeup_thread = false;
    {
        // Lock around a local region.
        auto data = pool.req_data.acquire();
        data->request_queue.push(std::move(req));
        FLOGF(iothread, L"enqueuing work item (count is %lu)", data->request_queue.size());
        if (data->drain) {
            // Do nothing here.
        } else if (data->waiting_threads >= data->request_queue.size()) {
            // There's enough waiting threads, wake one up.
            wakeup_thread = true;
        } else if (cant_wait || data->total_threads < pool.max_threads) {
            // No threads are waiting but we can or must spawn a new thread.
            data->total_threads += 1;
            spawn_new_thread = true;
        }
        local_thread_count = data->total_threads;
    }

    // Kick off the thread if we decided to do so.
    if (wakeup_thread) {
        FLOGF(iothread, L"notifying a thread", this_thread());
        pool.queue_cond.notify_one();
    }
    if (spawn_new_thread) {
        // Spawn a thread. If this fails, it means there's already a bunch of threads; it is very
        // unlikely that they are all on the verge of exiting, so one is likely to be ready to
        // handle extant requests. So we can ignore failure with some confidence.
        if (this->spawn()) {
            FLOGF(iothread, L"pthread spawned");
        } else {
            // We failed to spawn a thread; decrement the thread count.
            pool.req_data.acquire()->total_threads -= 1;
        }
    }
    return local_thread_count;
}
}  // namespace

void iothread_perform_impl(void_function_t &&func, bool cant_wait) {
    ASSERT_IS_NOT_FORKED_CHILD();
    s_io_thread_pool.perform(std::move(func), cant_wait);
}

int iothread_port() { return get_notify_signaller().read_fd(); }

void iothread_service_main_with_timeout(uint64_t timeout_usec) {
    if (select_wrapper_t::is_fd_readable(iothread_port(), timeout_usec)) {
        iothread_service_main();
    }
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
        auto data = pool.req_data.acquire();
        assert(!data->drain && "Should not be draining already");
        data->drain = true;
        thread_count = data->total_threads;
    }

    // Wake everyone up.
    pool.queue_cond.notify_all();

    double now = timef();

    // Nasty polling via select().
    while (pool.req_data.acquire()->total_threads > 0) {
        iothread_service_main_with_timeout(1000);
    }

    // Clear the drain flag.
    // Even though we released the lock, nobody should have added a new thread while the drain flag
    // is set.
    {
        auto data = pool.req_data.acquire();
        assert(data->total_threads == 0 && "Should be no threads");
        assert(data->drain && "Should be draining");
        data->drain = false;
    }

    double after = timef();
    FLOGF(iothread, "Drained %d thread(s) in %.02f msec", thread_count, 1000 * (after - now));
    return thread_count;
}

// Service the main thread queue, by invoking any functions enqueued for the main thread.
void iothread_service_main() {
    ASSERT_IS_MAIN_THREAD();
    // Note the order here is important: we must consume events before handling requests, as posting
    // uses the opposite order.
    (void)get_notify_signaller().try_consume();

    // Move the queue to a local variable.
    // Note the s_main_thread_queue lock is not held after this.
    main_thread_queue_t queue = s_main_thread_queue.acquire()->take();

    // Perform each completion in order.
    for (const void_function_t &func : queue.completions) {
        // ensure we don't invoke empty functions, that raises an exception
        if (func) func();
    }

    // Perform each main thread request.
    for (const void_function_t &func : queue.requests) {
        if (func) func();
    }
}

void iothread_perform_on_main(const void_function_t &func) {
    if (is_main_thread()) {
        func();
        return;
    }

    // Make a new request. Note we are synchronous, so our closure can use references instead of
    // copying.
    std::promise<void> wait_until_done;
    auto handler = [&] {
        func();
        wait_until_done.set_value();
    };
    // Append it. Ensure we don't hold the lock after.
    s_main_thread_queue.acquire()->requests.emplace_back(std::move(handler));

    // Tell the signaller and then wait until our future is set.
    get_notify_signaller().post();
    wait_until_done.get_future().wait();
}

bool make_detached_pthread(void *(*func)(void *), void *param) {
    // The spawned thread inherits our signal mask. Temporarily block signals, spawn the thread, and
    // then restore it. But we must not block SIGBUS, SIGFPE, SIGILL, or SIGSEGV; that's undefined
    // (#7837). Conservatively don't try to mask SIGKILL or SIGSTOP either; that's ignored on Linux
    // but maybe has an effect elsewhere.
    sigset_t new_set, saved_set;
    sigfillset(&new_set);
    sigdelset(&new_set, SIGILL);   // bad jump
    sigdelset(&new_set, SIGFPE);   // divide by zero
    sigdelset(&new_set, SIGBUS);   // unaligned memory access
    sigdelset(&new_set, SIGSEGV);  // bad memory access
    sigdelset(&new_set, SIGSTOP);  // unblockable
    sigdelset(&new_set, SIGKILL);  // unblockable
    DIE_ON_FAILURE(pthread_sigmask(SIG_BLOCK, &new_set, &saved_set));

    // Spawn a thread. If this fails, it means there's already a bunch of threads; it is very
    // unlikely that they are all on the verge of exiting, so one is likely to be ready to handle
    // extant requests. So we can ignore failure with some confidence.
    pthread_t thread = 0;
    int err = pthread_create(&thread, nullptr, func, param);
    if (err == 0) {
        // Success, return the thread.
        FLOGF(iothread, "pthread %p spawned", (intptr_t)thread);
        DIE_ON_FAILURE(pthread_detach(thread));
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
    auto vf = static_cast<void_func_t *>(param);
    (*vf)();
    delete vf;
    return nullptr;
}

bool make_detached_pthread(void_func_t &&func) {
    // Copy the function into a heap allocation.
    auto vf = new void_func_t(std::move(func));
    if (make_detached_pthread(func_invoker, vf)) {
        return true;
    }
    // Thread spawning failed, clean up our heap allocation.
    delete vf;
    return false;
}

static uint64_t next_thread_id() {
    // Note 0 is an invalid thread id.
    // Note fetch_add is a CAS which returns the value *before* the modification.
    static std::atomic<uint64_t> s_last_thread_id{};
    uint64_t res = 1 + s_last_thread_id.fetch_add(1, std::memory_order_relaxed);
    return res;
}

uint64_t thread_id() {
    static FISH_THREAD_LOCAL uint64_t tl_tid = next_thread_id();
    return tl_tid;
}

// Debounce implementation note: we would like to enqueue at most one request, except if a thread
// hangs (e.g. on fs access) then we do not want to block indefinitely; such threads are called
// "abandoned". This is implemented via a monotone uint64 counter, called a token.
// Every time we spawn a thread, increment the token. When the thread is completed, it compares its
// token to the active token; if they differ then this thread was abandoned.
struct debounce_t::impl_t {
    // Synchronized data from debounce_t.
    struct data_t {
        // The (at most 1) next enqueued request, or none if none.
        maybe_t<work_request_t> next_req{};

        // The token of the current non-abandoned thread, or 0 if no thread is running.
        uint64_t active_token{0};

        // The next token to use when spawning a thread.
        uint64_t next_token{1};

        // The start time of the most recently run thread spawn, or request (if any).
        std::chrono::time_point<std::chrono::steady_clock> start_time{};
    };
    owning_lock<data_t> data{};

    /// Run an iteration in the background, with the given thread token.
    /// \return true if we handled a request, false if there were none.
    bool run_next(uint64_t token);
};

bool debounce_t::impl_t::run_next(uint64_t token) {
    assert(token > 0 && "Invalid token");
    // Note we are on a background thread.
    maybe_t<work_request_t> req;
    {
        auto d = data.acquire();
        if (d->next_req) {
            // The value was dequeued, we are going to execute it.
            req = d->next_req.acquire();
            d->start_time = std::chrono::steady_clock::now();
        } else {
            // There is no request. If we are active, mark ourselves as no longer running.
            if (token == d->active_token) {
                d->active_token = 0;
            }
            return false;
        }
    }

    assert(req && req->handler && "Request should have value");
    req->handler();
    return true;
}

uint64_t debounce_t::perform(std::function<void()> handler) {
    uint64_t active_token{0};
    bool spawn{false};
    // Local lock.
    {
        auto d = impl_->data.acquire();
        d->next_req = work_request_t{std::move(handler)};
        // If we have a timeout, and our running thread has exceeded it, abandon that thread.
        if (d->active_token && timeout_msec_ > 0 &&
            std::chrono::steady_clock::now() - d->start_time >
                std::chrono::milliseconds(timeout_msec_)) {
            // Abandon this thread by marking nothing as active.
            d->active_token = 0;
        }
        if (!d->active_token) {
            // We need to spawn a new thread.
            // Mark the current time so that a new request won't immediately abandon us.
            spawn = true;
            d->active_token = d->next_token++;
            d->start_time = std::chrono::steady_clock::now();
        }
        active_token = d->active_token;
        assert(active_token && "Something should be active");
    }
    if (spawn) {
        // Equip our background thread with a reference to impl, to keep it alive.
        auto impl = impl_;
        iothread_perform([=] {
            while (impl->run_next(active_token))
                ;  // pass
        });
    }
    return active_token;
}

// static
void debounce_t::enqueue_main_thread_result(std::function<void()> func) {
    s_main_thread_queue.acquire()->completions.push_back(std::move(func));
    get_notify_signaller().post();
}

debounce_t::debounce_t(long timeout_msec)
    : timeout_msec_(timeout_msec), impl_(std::make_shared<impl_t>()) {}
debounce_t::~debounce_t() = default;
