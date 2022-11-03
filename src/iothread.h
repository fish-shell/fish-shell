// SPDX-FileCopyrightText: © 2011 fish-shell contributors
//
// SPDX-License-Identifier: GPL-2.0-only

// Handles IO that may hang.
#ifndef FISH_IOTHREAD_H
#define FISH_IOTHREAD_H

#include <cstdint>  // for uint64_t
#include <functional>
#include <memory>
#include <utility>

/// \return the fd on which to listen for completion callbacks.
int iothread_port();

/// Services iothread main thread completions and requests.
/// This does not block.
void iothread_service_main();

// Services any main thread requests. Does not wait more than \p timeout_usec.
void iothread_service_main_with_timeout(uint64_t timeout_usec);

/// Waits for all iothreads to terminate.
/// This is a hacky function only used in the test suite.
void iothread_drain_all();

// Internal implementation
void iothread_perform_impl(std::function<void()> &&, bool cant_wait = false);

// iothread_perform invokes a handler on a background thread.
inline void iothread_perform(std::function<void()> &&func) {
    iothread_perform_impl(std::move(func));
}

/// Variant of iothread_perform that disrespects the thread limit.
/// It does its best to spawn a new thread if all other threads are occupied.
/// This is for cases where deferring a new thread might lead to deadlock.
inline void iothread_perform_cantwait(std::function<void()> &&func) {
    iothread_perform_impl(std::move(func), true);
}

/// Creates a pthread, manipulating the signal mask so that the thread receives no signals.
/// The thread is detached.
/// The pthread runs \p func.
/// \returns true on success, false on failure.
bool make_detached_pthread(void *(*func)(void *), void *param);
bool make_detached_pthread(std::function<void()> &&func);

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
    /// If the function executes, then \p completion will be invoked on the main thread, with the
    /// result of the handler.
    /// The result is a token which is only of interest to the tests.
    template <typename Handler, typename Completion>
    uint64_t perform(const Handler &handler, const Completion &completion) {
        // Make a trampoline function which calls the handler, puts the result into a shared
        // pointer, and then enqueues a completion.
        auto trampoline = [=] {
            using result_type_t = decltype(handler());
            auto result = std::make_shared<result_type_t>(handler());
            enqueue_main_thread_result([=] { completion(std::move(*result)); });
        };
        return perform(std::move(trampoline));
    }

    /// One-argument form with no completion.
    /// The result is a token which is only of interest to the tests.
    uint64_t perform(std::function<void()> handler);

    explicit debounce_t(long timeout_msec = 0);
    ~debounce_t();

   private:
    /// Helper to enqueue a function to run on the main thread.
    static void enqueue_main_thread_result(std::function<void()> func);

    const long timeout_msec_;
    struct impl_t;
    const std::shared_ptr<impl_t> impl_;
};

#endif
