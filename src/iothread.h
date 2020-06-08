// Handles IO that may hang.
#ifndef FISH_IOTHREAD_H
#define FISH_IOTHREAD_H

#include <pthread.h>

#include <cstdint>  // for uint64_t
#include <functional>
#include <memory>
#include <type_traits>

#include "maybe.h"

/// Runs a command on a thread.
///
/// \param handler The function to execute on a background thread. Accepts an arbitrary context
/// pointer, and returns an int, which is passed to the completionCallback.
/// \param completionCallback The function to execute on the main thread once the background thread
/// is complete. Accepts an int (the return value of handler) and the context.
/// \param context An arbitrary context pointer to pass to the handler and completion callback.
/// \return A sequence number, currently not very useful.
int iothread_perform_base(int (*handler)(void *), void (*completionCallback)(void *, int),
                          void *context);

/// Gets the fd on which to listen for completion callbacks.
///
/// \return A file descriptor on which to listen for completion callbacks.
int iothread_port(void);

/// Services one iothread completion callback.
void iothread_service_completion(void);

/// Waits for all iothreads to terminate.
/// \return the number of threads that were running.
int iothread_drain_all(void);

// Internal implementation
int iothread_perform_impl(std::function<void(void)> &&func, std::function<void(void)> &&completion,
                          bool cant_wait = false);

// This is the glue part of the handler-completion handoff.
// Given a Handler and Completion, where the return value of Handler should be passed to Completion,
// this generates new void->void functions that wraps that behavior. The type T is the return type
// of Handler and the argument to Completion
template <typename Handler, typename Completion,
          typename Result = typename std::result_of<Handler()>::type>
struct iothread_trampoline_t {
    iothread_trampoline_t(const Handler &hand, const Completion &comp) {
        auto result = std::make_shared<maybe_t<Result>>();
        this->handler = [=] { *result = hand(); };
        this->completion = [=] { comp(result->acquire()); };
    }

    // The generated handler and completion functions.
    std::function<void()> handler;
    std::function<void()> completion;
};

// Void specialization.
template <typename Handler, typename Completion>
struct iothread_trampoline_t<Handler, Completion, void> {
    iothread_trampoline_t(std::function<void()> hand, std::function<void()> comp)
        : handler(std::move(hand)), completion(std::move(comp)) {}

    // The generated handler and completion functions.
    std::function<void()> handler;
    std::function<void()> completion;
};

// iothread_perform invokes a handler on a background thread, and then a completion function
// on the main thread. The value returned from the handler is passed to the completion.
// In other words, this is like Completion(Handler()) except the handler part is invoked
// on a background thread.
template <typename Handler, typename Completion>
int iothread_perform(const Handler &handler, const Completion &completion) {
    iothread_trampoline_t<Handler, Completion> tramp(handler, completion);
    return iothread_perform_impl(std::move(tramp.handler), std::move(tramp.completion));
}

// variant of iothread_perform without a completion handler
inline int iothread_perform(std::function<void(void)> &&func) {
    return iothread_perform_impl(std::move(func), {});
}

/// Variant of iothread_perform that disrespects the thread limit.
/// It does its best to spawn a new thread if all other threads are occupied.
/// This is for cases where deferring a new thread might lead to deadlock.
inline int iothread_perform_cantwait(std::function<void(void)> &&func) {
    return iothread_perform_impl(std::move(func), {}, true);
}

/// Performs a function on the main thread, blocking until it completes.
void iothread_perform_on_main(std::function<void(void)> &&func);

/// Creates a pthread, manipulating the signal mask so that the thread receives no signals.
/// The thread is detached.
/// The pthread runs \p func.
/// \returns true on success, false on failure.
bool make_detached_pthread(void *(*func)(void *), void *param);
bool make_detached_pthread(std::function<void(void)> &&func);

/// \returns a thread ID for this thread.
/// Thread IDs are never repeated.
uint64_t thread_id();

/// A Debounce is a simple class which executes one function in a background thread,
/// while enqueuing at most one more. New execution requests overwrite the enqueued one.
/// It has an optional timeout; if a handler does not finish within the timeout, then
/// a new thread is spawned.
class debounce_t {
   public:
    /// Enqueue \p handler to be performed on a background thread, and \p completion (if any) to be
    /// performed on the main thread. If a function is already enqueued, this overwrites it; that
    /// function will not execute.
    /// This returns the active thread token, which is only of interest to tests.
    template <typename Handler, typename Completion>
    uint64_t perform(Handler handler, Completion completion) {
        iothread_trampoline_t<Handler, Completion> tramp(handler, completion);
        return perform_impl(std::move(tramp.handler), std::move(tramp.completion));
    }

    /// One-argument form with no completion.
    uint64_t perform(std::function<void()> func) { return perform_impl(std::move(func), {}); }

    explicit debounce_t(long timeout_msec = 0);
    ~debounce_t();

   private:
    /// Implementation of perform().
    uint64_t perform_impl(std::function<void()> handler, std::function<void()> completion);

    const long timeout_msec_;
    struct impl_t;
    const std::shared_ptr<impl_t> impl_;
};

#endif
