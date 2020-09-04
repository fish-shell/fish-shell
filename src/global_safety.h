// Support for enforcing correct access to globals.
#ifndef FISH_GLOBAL_SAFETY_H
#define FISH_GLOBAL_SAFETY_H

#include "config.h"  // IWYU pragma: keep

#include <atomic>
#include <cassert>

#include "common.h"

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

    T *operator->() {
        ASSERT_IS_MAIN_THREAD();
        return &value_;
    }

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

    void operator=(std::unique_ptr<T> value) {
        ASSERT_IS_MAIN_THREAD();
        assert(value_ == nullptr && "Latch variable initialized multiple times");
        assert(value != nullptr && "Latch variable initialized with null");
        // Note: deliberate leak.
        value_ = value.release();
    }

    void operator=(T &&value) { *this = make_unique<T>(std::move(value)); }

    template <typename... Args>
    void emplace(Args &&... args) {
        *this = make_unique<T>(std::forward<Args>(args)...);
    }
};

/// An atomic type that always use relaxed reads.
template <typename T>
class relaxed_atomic_t {
    std::atomic<T> value_{};

   public:
    relaxed_atomic_t() = default;
    relaxed_atomic_t(T value) : value_(value) {}

    operator T() const volatile { return value_.load(std::memory_order_relaxed); }

    void operator=(T v) { return value_.store(v, std::memory_order_relaxed); }
    void operator=(T v) volatile { return value_.store(v, std::memory_order_relaxed); }

    // Perform a CAS operation, returning whether it succeeded.
    bool compare_exchange(T expected, T desired) {
        return value_.compare_exchange_strong(expected, desired, std::memory_order_relaxed);
    }

    // postincrement
    T operator++(int) { return value_.fetch_add(1, std::memory_order_relaxed); }

    // preincrement
    T operator++() { return 1 + value_.fetch_add(1, std::memory_order_relaxed); }
};

using relaxed_atomic_bool_t = relaxed_atomic_t<bool>;

#endif
