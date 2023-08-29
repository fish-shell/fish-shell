#pragma once

#include <cstdlib>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

/// A RAII callback container that can be used when the rust code needs to (or might need to) free
/// up the resources allocated for a callback (either the type-erased std::function wrapping the
/// lambda itself or the parameter to it.)
struct callback_t {
    std::function<void *(const void *param)> callback;
    std::vector<std::function<void()>> cleanups;

    /// The default no-op constructor for the callback_t type.
    callback_t() {
        this->callback = [=](const void *) { return (void *)nullptr; };
    }

    /// Creates a new callback_t instance wrapping the specified type-erased std::function with an
    /// optional parameter (defaulting to nullptr).
    callback_t(std::function<void *(const void *param)> &&callback) {
        this->callback = std::move(callback);
    }

    /// Executes the wrapped callback with the parameter stored at the time of creation and returns
    /// the type-erased (void *) result, but cast to a `const uint8_t *` to please cxx::bridge.
    const uint8_t *invoke() const {
        const void *result = callback(nullptr);
        return (const uint8_t *)result;
    }

    /// Executes the wrapped callback with the provided parameter and returns the type-erased
    /// (void *) result, but cast to a `const uint8_t *` to please cxx::bridge.
    const uint8_t *invoke_with_param(const uint8_t *param) const {
        const void *result = callback((const void *)param);
        return (const uint8_t *)result;
    }

    ~callback_t() {
        if (cleanups.size() > 0) {
            for (const std::function<void()> &dtor : cleanups) {
                (dtor)();
            }
            cleanups.clear();
        }
    }
};
