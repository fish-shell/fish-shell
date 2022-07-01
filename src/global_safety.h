// Support for enforcing correct access to globals.
#ifndef FISH_GLOBAL_SAFETY_H
#define FISH_GLOBAL_SAFETY_H

#include "config.h"  // IWYU pragma: keep

#include <atomic>
#include <cassert>

#include "common.h"

/// A latch variable may only be set once.
/// The value is immortal.
template <typename T>
class latch_t : noncopyable_t, nonmovable_t {
    T *value_{};

   public:
    operator T *() { return value_; }
    operator const T *() const { return value_; }

    T *operator->() { return value_; }
    const T *operator->() const { return value_; }

    bool is_set() const { return value_ != nullptr; }

    void operator=(std::unique_ptr<T> value) {
        assert(value_ == nullptr && "Latch variable initialized multiple times");
        assert(value != nullptr && "Latch variable initialized with null");
        // Note: deliberate leak.
        value_ = value.release();
    }

    void operator=(T &&value) { *this = make_unique<T>(std::move(value)); }
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
    T operator--(int) { return value_.fetch_sub(1, std::memory_order_relaxed); }

    // preincrement
    T operator++() { return value_.fetch_add(1, std::memory_order_relaxed) + 1; }
    T operator--() { return value_.fetch_sub(1, std::memory_order_relaxed) - 1; }
};

using relaxed_atomic_bool_t = relaxed_atomic_t<bool>;

#endif
