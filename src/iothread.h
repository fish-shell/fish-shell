// Handles IO that may hang.
#ifndef FISH_IOTHREAD_H
#define FISH_IOTHREAD_H
#if INCLUDE_RUST_HEADERS

#include <cstdlib>
#include <functional>
#include <memory>
#include <utility>

#include "threads.rs.h"

struct iothread_callback_t {
    std::function<void *(const void *param)> callback;
    void *param;

    ~iothread_callback_t() {
        if (param) {
            free(param);
            param = nullptr;
        }
    }
};

extern "C" const void *iothread_trampoline(const void *callback);
extern "C" const void *iothread_trampoline2(const void *callback, const void *param);

// iothread_perform invokes a handler on a background thread.
inline void iothread_perform(std::function<void()> &&func) {
    auto callback = new iothread_callback_t{std::bind([=] {
                                                func();
                                                return nullptr;
                                            }),
                                            nullptr};

    iothread_perform((const uint8_t *)&iothread_trampoline, (const uint8_t *)callback);
}

/// Variant of iothread_perform that disrespects the thread limit.
/// It does its best to spawn a new thread if all other threads are occupied.
/// This is for cases where deferring a new thread might lead to deadlock.
inline void iothread_perform_cantwait(std::function<void()> &&func) {
    auto callback = new iothread_callback_t{std::bind([=] {
                                                func();
                                                return nullptr;
                                            }),
                                            nullptr};

    iothread_perform_cantwait((const uint8_t *)&iothread_trampoline, (const uint8_t *)callback);
}

inline uint64_t debounce_perform(const debounce_t &debouncer, const std::function<void()> &func) {
    auto callback = new iothread_callback_t{std::bind([=] {
                                                func();
                                                return nullptr;
                                            }),
                                            nullptr};

    return debouncer.perform((const uint8_t *)&iothread_trampoline, (const uint8_t *)callback);
}

template <typename R>
inline void debounce_perform_with_completion(const debounce_t &debouncer, std::function<R()> &&func,
                                             std::function<void(R)> &&completion) {
    auto callback1 = new iothread_callback_t{[=](const void *) {
                                                 auto *result = new R(func());
                                                 return (void *)result;
                                             },
                                             nullptr};

    auto callback2 = new iothread_callback_t{
        ([=](const void *r) {
            const R *result = (const R *)r;
            completion(*result);
            delete result;
            return nullptr;
        }),
        nullptr,
    };

    debouncer.perform_with_completion(
        (const uint8_t *)&iothread_trampoline, (const uint8_t *)callback1,
        (const uint8_t *)&iothread_trampoline2, (const uint8_t *)callback2);
}

template <typename R>
inline void debounce_perform_with_completion(const debounce_t &debouncer, std::function<R()> &&func,
                                             std::function<void(const R &)> &&completion) {
    auto callback1 = new iothread_callback_t{[=](const void *) {
                                                 auto *result = new R(func());
                                                 return (void *)result;
                                             },
                                             nullptr};

    auto callback2 = new iothread_callback_t{
        ([=](const void *r) {
            const R *result = (const R *)r;
            completion(*result);
            delete result;
            return nullptr;
        }),
        nullptr,
    };

    debouncer.perform_with_completion(
        (const uint8_t *)&iothread_trampoline, (const uint8_t *)callback1,
        (const uint8_t *)&iothread_trampoline2, (const uint8_t *)callback2);
}

inline bool make_detached_pthread(const std::function<void()> &func) {
    auto callback = new iothread_callback_t{
        [=](const void *) {
            func();
            return nullptr;
        },
        nullptr,
    };

    return make_detached_pthread((const uint8_t *)&iothread_trampoline, (const uint8_t *)callback);
}

#endif
#endif
