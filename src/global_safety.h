// Support for enforcing correct access to globals.
#ifndef FISH_GLOBAL_SAFETY_H
#define FISH_GLOBAL_SAFETY_H

#include "config.h"  // IWYU pragma: keep

#include "common.h"

#include <cassert>

// fish is multithreaded. Global (which includes function and file-level statics) when used naively
// may therefore lead to data races. Use the following types to characterize and enforce correct
// access patterns.

namespace detail {
// An empty value type that cannot be copied or moved.
// Include this as an instance variable to prevent globals from being copied or moved.
struct fixed_t {
    fixed_t(const fixed_t &) = delete;
    fixed_t(fixed_t &&) = delete;
    fixed_t &operator=(fixed_t &&) = delete;
    fixed_t &operator=(const fixed_t &) = delete;
    fixed_t() = default;
};
}  // namespace detail

/// A mainthread_t variable may only be accessed on the main thread.
template <typename T>
class mainthread_t : detail::fixed_t {
    T value_{};

   public:
    mainthread_t(T value) : value_(std::move(value)) {}
    mainthread_t() = default;

    operator T &() {
        ASSERT_IS_MAIN_THREAD();
        return value_;
    }

    operator const T &() const {
        ASSERT_IS_MAIN_THREAD();
        return value_;
    }

    void operator=(T value) {
        ASSERT_IS_MAIN_THREAD();
        value_ = std::move(value);
    }
};

/// A latch variable may only be set once, on the main thread.
/// The value is a immortal.
template <typename T>
class latch_t : detail::fixed_t {
    T *value_{};

   public:
    operator T *() { return value_; }
    operator const T *() const { return value_; }

    T *operator->() { return value_; }
    const T *operator->() const { return value_; }

    template <typename... Args>
    void emplace(Args &&... args) {
        ASSERT_IS_MAIN_THREAD();
        assert(value_ == nullptr && "Latch variable initialized multiple times");
        value_ = new T(std::forward<Args>(args)...);
    }
};

#endif
