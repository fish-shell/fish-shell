#pragma once

#include <cassert>

#include "callback.h"
#include "threads.rs.h"

// iothread_perform invokes a handler on a background thread.
inline void iothread_perform(std::function<void()> &&func) {
    std::shared_ptr<callback_t> callback = std::make_shared<callback_t>([=](const void *) {
        func();
        return nullptr;
    });

    iothread_perform(callback);
}

/// Variant of iothread_perform that disrespects the thread limit.
/// It does its best to spawn a new thread if all other threads are occupied.
/// This is for cases where deferring a new thread might lead to deadlock.
inline void iothread_perform_cantwait(std::function<void()> &&func) {
    std::shared_ptr<callback_t> callback = std::make_shared<callback_t>([=](const void *) {
        func();
        return nullptr;
    });

    iothread_perform_cantwait(callback);
}

inline uint64_t debounce_perform(const debounce_t &debouncer, const std::function<void()> &func) {
    std::shared_ptr<callback_t> callback = std::make_shared<callback_t>([=](const void *) {
        func();
        return nullptr;
    });

    return debouncer.perform(callback);
}

template <typename R>
inline void debounce_perform_with_completion(const debounce_t &debouncer, std::function<R()> &&func,
                                             std::function<void(R)> &&completion) {
    std::shared_ptr<callback_t> callback2 = std::make_shared<callback_t>([=](const void *r) {
        assert(r != nullptr && "callback1 result was null!");
        const R *result = (const R *)r;
        completion(*result);
        return nullptr;
    });

    std::shared_ptr<callback_t> callback1 = std::make_shared<callback_t>([=](const void *) {
        const R *result = new R(func());
        callback2->cleanups.push_back([result]() { delete result; });
        return (void *)result;
    });

    debouncer.perform_with_completion(callback1, callback2);
}

template <typename R>
inline void debounce_perform_with_completion(const debounce_t &debouncer, std::function<R()> &&func,
                                             std::function<void(const R &)> &&completion) {
    std::shared_ptr<callback_t> callback2 = std::make_shared<callback_t>([=](const void *r) {
        assert(r != nullptr && "callback1 result was null!");
        const R *result = (const R *)r;
        completion(*result);
        return nullptr;
    });

    std::shared_ptr<callback_t> callback1 = std::make_shared<callback_t>([=](const void *) {
        const R *result = new R(func());
        callback2->cleanups.push_back([result]() { delete result; });
        return (void *)result;
    });

    debouncer.perform_with_completion(callback1, callback2);
}

inline bool make_detached_pthread(const std::function<void()> &func) {
    std::shared_ptr<callback_t> callback = std::make_shared<callback_t>([=](const void *) {
        func();
        return nullptr;
    });

    return make_detached_pthread(callback);
}
