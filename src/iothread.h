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

inline bool make_detached_pthread(const std::function<void()> &func) {
    std::shared_ptr<callback_t> callback = std::make_shared<callback_t>([=](const void *) {
        func();
        return nullptr;
    });

    return make_detached_pthread(callback);
}
