#ifndef FISH_MAYBE_H
#define FISH_MAYBE_H

#include <cassert>
#include <utility>

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
class maybe_t {
    alignas(T) char storage[sizeof(T)];
    bool filled = false;

   public:
    // return whether the receiver contains a value.
    bool has_value() const { return filled; }

    // bool conversion indicates whether the receiver contains a value.
    explicit operator bool() const { return filled; }

    // The default constructor constructs a maybe with no value.
    maybe_t() = default;

    // Construct a maybe_t from a none_t
    /* implicit */ maybe_t(none_t n) { (void)n; }

    // Construct a maybe_t from a value T.
    /* implicit */ maybe_t(T &&v) : filled(true) { new (storage) T(std::forward<T>(v)); }

    // Construct a maybe_t from a value T.
    /* implicit */ maybe_t(const T &v) : filled(true) { new (storage) T(v); }

    // Copy constructor.
    maybe_t(const maybe_t &v) : filled(v.filled) {
        if (v.filled) {
            new (storage) T(v.value());
        }
    }

    // Move constructor.
    /* implicit */ maybe_t(maybe_t &&v) : filled(v.filled) {
        if (v.filled) {
            new (storage) T(std::move(v.value()));
        }
    }

    // Construct a value in-place.
    template <class... Args>
    void emplace(Args &&... args) {
        reset();
        filled = true;
        new (storage) T(std::forward<Args>(args)...);
    }

    // Access the value.
    T &value() {
        assert(filled && "maybe_t does not have a value");
        return *reinterpret_cast<T *>(storage);
    }

    const T &value() const {
        assert(filled && "maybe_t does not have a value");
        return *reinterpret_cast<const T *>(storage);
    }

    // Transfer the value to the caller.
    T acquire() {
        assert(filled && "maybe_t does not have a value");
        T res = std::move(value());
        reset();
        return res;
    }

    // Clear the value.
    void reset() {
        if (filled) {
            value().~T();
            filled = false;
        }
    }

    // Assign a new value.
    maybe_t &operator=(T &&v) {
        if (filled) {
            value() = std::move(v);
        } else {
            new (storage) T(std::move(v));
            filled = true;
        }
        return *this;
    }

    maybe_t &operator=(maybe_t &&v) {
        if (!v) {
            reset();
        } else {
            *this = std::move(*v);
        }
        return *this;
    }

    // Dereference support.
    const T *operator->() const { return &value(); }
    T *operator->() { return &value(); }
    const T &operator*() const { return value(); }
    T &operator*() { return value(); }

    // Helper to replace missing_or_empty() on env_var_t.
    // Uses SFINAE to only introduce this function if T has an empty() type.
    template<typename S = T>
    decltype(S().empty(), bool()) missing_or_empty() const {
        return ! has_value() || value().empty();
    }

    // Compare values for equality.
    bool operator==(const maybe_t &rhs) const {
        if (this->has_value() && rhs.has_value()) return this->value() == rhs.value();
        return this->has_value() == rhs.has_value();
    }

    bool operator!=(const maybe_t &rhs) const { return !(*this == rhs); }

    ~maybe_t() { reset(); }
};

#endif
