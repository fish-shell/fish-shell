#ifndef FISH_CANCELLABLE_H
#define FISH_CANCELLABLE_H

#include "maybe.h"

/// A simple value type representing cancellation via a signal.
struct cancellation_t {
    int signal;

    explicit cancellation_t(int sig) : signal(sig) {}
};

/// A cancellable_t<T> is a wrapper around T which may be cancelled, for example by a signal.
template <typename T>
class cancellable_t : private maybe_t<T> {
    using super = maybe_t<T>;

   public:
    /// Construct from a T.
    /* implicit */ cancellable_t(T &&v) : super(v) {}
    /* implicit */ cancellable_t(const T &v) : super(v) {}

    /// Construct from a cancellation.
    /* implicit */ cancellable_t(cancellation_t c) : super(none()), signal_(c.signal) {}

    cancellable_t(const cancellable_t &) = default;
    cancellable_t(cancellable_t &&) = default;

    /// Access the cancellation signal, if this was cancelled.
    int signal() const {
        assert(!has_value() && "Not cancelled");
        return signal_;
    }

    /// \return whether this was cancelled.
    bool is_cancelled() const { return !has_value(); }

    // Expose maybe_t<T>::value and dereferencing.
    using super::has_value;
    using super::value;
    using super::operator->;
    using super::operator*;

    /// Notice we don't compare signals when checking for equality.
    bool operator==(const cancellable_t &rhs) const { super::operator==(rhs); }
    bool operator!=(const cancellable_t &rhs) const { super::operator!=(rhs); }

   private:
    // The cancellation signal.
    int signal_{0};
};

#endif
