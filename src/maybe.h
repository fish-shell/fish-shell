#ifndef FISH_MAYBE_H
#define FISH_MAYBE_H

#include <cassert>
#include <new>
#include <type_traits>
#include <utility>

namespace maybe_detail {
// Template magic to make maybe_t<T> (trivially) copyable iff T is.

// This is an unsafe implementation: it is always trivially copyable.
template <typename T>
struct maybe_impl_trivially_copyable_t {
    alignas(T) char storage[sizeof(T)];
    bool filled = false;

    T &value() {
        assert(filled && "maybe_t does not have a value");
        return *reinterpret_cast<T *>(storage);
    }

    const T &value() const {
        assert(filled && "maybe_t does not have a value");
        return *reinterpret_cast<const T *>(storage);
    }

    void reset() {
        if (this->filled) {
            value().~T();
            this->filled = false;
        }
    }

    T acquire() {
        assert(filled && "maybe_t does not have a value");
        T res = std::move(value());
        reset();
        return res;
    }

    maybe_impl_trivially_copyable_t() = default;

    // Move construction/assignment from a T.
    explicit maybe_impl_trivially_copyable_t(T &&v) : filled(true) {
        new (storage) T(std::forward<T>(v));
    }
    maybe_impl_trivially_copyable_t &operator=(T &&v) {
        if (filled) {
            value() = std::move(v);
        } else {
            new (storage) T(std::move(v));
            filled = true;
        }
        return *this;
    }

    // Copy construction/assignment from a T.
    explicit maybe_impl_trivially_copyable_t(const T &v) : filled(true) { new (storage) T(v); }
    maybe_impl_trivially_copyable_t &operator=(const T &v) {
        if (filled) {
            value() = v;
        } else {
            new (storage) T(v);
            filled = true;
        }
        return *this;
    }
};

// This is an unsafe implementation: it is always copyable.
template <typename T>
struct maybe_impl_not_trivially_copyable_t : public maybe_impl_trivially_copyable_t<T> {
    using base_t = maybe_impl_trivially_copyable_t<T>;
    using base_t::maybe_impl_trivially_copyable_t;
    using base_t::operator=;
    using base_t::filled;
    using base_t::reset;
    using base_t::storage;

    // Move construction/assignment from another instance.
    maybe_impl_not_trivially_copyable_t(maybe_impl_not_trivially_copyable_t &&v) {
        filled = v.filled;
        if (filled) {
            new (storage) T(std::move(v.value()));
        }
    }

    maybe_impl_not_trivially_copyable_t &operator=(maybe_impl_not_trivially_copyable_t &&v) {
        if (!v.filled) {
            reset();
        } else {
            *this = std::move(v.value());
        }
        return *this;
    }

    // Copy construction/assignment from another instance.
    maybe_impl_not_trivially_copyable_t(const maybe_impl_not_trivially_copyable_t &v) : base_t() {
        filled = v.filled;
        if (v.filled) {
            new (storage) T(v.value());
        }
    }

    maybe_impl_not_trivially_copyable_t &operator=(const maybe_impl_not_trivially_copyable_t &v) {
        if (&v == this) return *this;
        if (!v.filled) {
            reset();
        } else {
            *this = v.value();
        }
        return *this;
    }

    maybe_impl_not_trivially_copyable_t() = default;
    ~maybe_impl_not_trivially_copyable_t() { reset(); }
};

struct copyable_t {};
struct noncopyable_t {
    noncopyable_t() = default;
    noncopyable_t(noncopyable_t &&) = default;
    noncopyable_t &operator=(noncopyable_t &&) = default;
    noncopyable_t(const noncopyable_t &) = delete;
    noncopyable_t &operator=(const noncopyable_t &) = delete;
};

// conditionally_copyable_t is copyable iff T is copyable.
// This enables conditionally copyable wrapper types by inheriting from it.
template <typename T>
using conditionally_copyable_t = typename std::conditional<std::is_copy_constructible<T>::value,
                                                           copyable_t, noncopyable_t>::type;

};  // namespace maybe_detail

// A none_t is a helper type used to implicitly initialize maybe_t.
// Example usage:
//   maybe_t<int> sqrt(int x) {
//      if (x < 0) return none();
//      return (int)sqrt(x);
//   }
enum class none_t { none = 1 };
inline constexpr none_t none() { return none_t::none; }

// Support for a maybe, also known as Optional.
// This is a value-type class that stores a value of T in aligned storage.
template <typename T>
class maybe_t : private maybe_detail::conditionally_copyable_t<T> {
    // Making maybe_t trivially copyable results in some heisenbugs if compiled under gcc 8.3.0
    // targeting 32-bit armhf platforms (Debian 10 armhf toolchain). It's confirmed not to happen if
    // using clang 7 (under Debian 10 armhf) or gcc 10 (under Debian 11 armhf) so we're also
    // excluding gcc 9 unless/until 32-bit armhf builds under GCC 9 are observed to pass all tests
    // with this version check lowered. As this is only an optimization, just disable it across the
    // board rather than only for armhf targets out of an abundance of caution.
#if __GNUG__ && __GNUC__ < 10
    using maybe_impl_t = maybe_detail::maybe_impl_not_trivially_copyable_t<T>;
#else
    using maybe_impl_t =
        typename std::conditional<std::is_trivially_copyable<T>::value,
                                  maybe_detail::maybe_impl_trivially_copyable_t<T>,
                                  maybe_detail::maybe_impl_not_trivially_copyable_t<T> >::type;
#endif
    maybe_impl_t impl_;

   public:
    // return whether the receiver contains a value.
    bool has_value() const { return impl_.filled; }

    // A bool operator as a shortcut to test if the maybe_t has a value.
    // Not enabled if the type T is already bool-convertible to prevent accidental misuse,
    // otherwise the "typename std::enable_if<....>::type" evaluates to bool, giving us a definition
    // of `explicit operator bool() const { ... }`
    template <typename U = T>
    explicit operator typename std::enable_if<!std::is_convertible<U, bool>::value, bool>::type()
        const {
        return impl_.filled;
    }

    // The default constructor constructs a maybe with no value.
    maybe_t() = default;

    // Construct a maybe_t from a none_t
    /* implicit */ maybe_t(none_t) {}

    // Construct a maybe_t from a value T.
    /* implicit */ maybe_t(T &&v) : impl_(std::move(v)) {}

    // Construct a maybe_t from a value T.
    /* implicit */ maybe_t(const T &v) : impl_(v) {}

    // Copy and move constructors.
    maybe_t(const maybe_t &) = default;
    maybe_t(maybe_t &&) = default;

    // Construct a value in-place.
    template <class... Args>
    void emplace(Args &&...args) {
        reset();
        impl_.filled = true;
        new (impl_.storage) T(std::forward<Args>(args)...);
    }

    // Access the value.
    T &value() { return impl_.value(); }

    const T &value() const { return impl_.value(); }

    // Transfer the value to the caller.
    T acquire() { return impl_.acquire(); }

    // Return (a copy of) our value, or the given value if we are empty.
    T value_or(T v) const {
        if (this->has_value()) {
            return this->value();
        }
        return v;
    }

    // Clear the value.
    void reset() { impl_.reset(); }

    // Assign a new value.
    maybe_t &operator=(T &&v) {
        impl_ = std::move(v);
        return *this;
    }

    // Note that defaulting these allows these to be conditionally deleted via
    // conditionally_copyable_t().
    maybe_t &operator=(const maybe_t &) = default;
    maybe_t &operator=(maybe_t &&) = default;

    // Dereference support.
    const T *operator->() const { return &value(); }
    T *operator->() { return &value(); }
    const T &operator*() const { return value(); }
    T &operator*() { return value(); }

    // Helper to replace missing_or_empty() on env_var_t.
    // Uses SFINAE to only introduce this function if T has an empty() type.
    template <typename S = T>
    decltype(S().empty(), bool()) missing_or_empty() const {
        return !has_value() || value().empty();
    }

    // Compare values for equality.
    bool operator==(const maybe_t &rhs) const {
        if (this->has_value() && rhs.has_value()) return this->value() == rhs.value();
        return this->has_value() == rhs.has_value();
    }

    bool operator!=(const maybe_t &rhs) const { return !(*this == rhs); }

    bool operator==(const T &rhs) const { return this->has_value() && this->value() == rhs; }

    bool operator!=(const T &rhs) const { return !(*this == rhs); }
};

#endif
