#ifndef FISH_FD_MONITOR_H
#define FISH_FD_MONITOR_H

#include <chrono>
#include <cstdint>
#include <functional>
// Needed for musl
#include <sys/select.h>  // IWYU pragma: keep

#include "common.h"
#include "maybe.h"

class fd_monitor_t;

/// An item containing an fd and callback, which can be monitored to watch when it becomes readable,
/// and invoke the callback.
struct fd_monitor_item_t {
    friend class fd_monitor_t;

    /// The callback type for the item.
    /// It will be invoked when either \p fd is readable, or if the timeout was hit.
    using callback_t = std::function<void(autoclose_fd_t &fd, bool timed_out)>;

    /// A sentinel value meaning no timeout.
    static constexpr uint64_t kNoTimeout = std::numeric_limits<uint64_t>::max();

    /// The fd to monitor.
    autoclose_fd_t fd{};

    /// A callback to be invoked when the fd is readable, or when we are timed out.
    /// If we time out, then timed_out will be true.
    /// If the fd is invalid on return from the function, then the item is removed.
    callback_t callback{};

    /// The timeout in microseconds, or kNoTimeout for none.
    /// 0 timeouts are unsupported.
    uint64_t timeout_usec{kNoTimeout};

    /// Construct from a file, callback, and optional timeout.
    fd_monitor_item_t(autoclose_fd_t fd, callback_t callback, uint64_t timeout_usec = kNoTimeout)
        : fd(std::move(fd)), callback(std::move(callback)), timeout_usec(timeout_usec) {
        assert(timeout_usec > 0 && "Invalid timeout");
    }

    fd_monitor_item_t() = default;

   private:
    // Fields and methods for the private use of fd_monitor_t.
    using time_point_t = std::chrono::time_point<std::chrono::steady_clock>;

    // The last time we were called, or the initialization point.
    maybe_t<time_point_t> last_time{};

    // \return the number of microseconds until the timeout should trigger, or kNoTimeout for none.
    // A 0 return means we are at or past the timeout.
    uint64_t usec_remaining(const time_point_t &now) const;

    // Invoke this item's callback if its value is set in fd or has timed out.
    // \return true to retain the item, false to remove it.
    bool service_item(const fd_set *fds, const time_point_t &now);
};

/// A class which can monitor a set of fds, invoking a callback when any becomes readable, or when
/// per-item-configurable timeouts are hit.
class fd_monitor_t {
   public:
    using item_list_t = std::vector<fd_monitor_item_t>;

    fd_monitor_t();
    ~fd_monitor_t();

    /// Add an item to monitor.
    void add(fd_monitor_item_t &&item);

   private:
    // The background thread runner.
    void run_in_background();

    // Add a pending item, marking the thread as running.
    // \return true if we should start the thread.
    bool add_pending_get_start_thread(fd_monitor_item_t &&item);

    // The list of items to monitor. This is only accessed on the background thread.
    item_list_t items_{};

    // Set to true by the background thread when our self-pipe becomes readable.
    bool has_pending_{false};

    // Latched to true by the background thread if our self-pipe is closed, which indicates we are
    // in the destructor and so should terminate.
    bool terminate_{false};

    struct data_t {
        /// Pending items.
        item_list_t pending{};

        /// Whether the thread is running.
        bool running{false};
    };
    owning_lock<data_t> data_;

    /// The write end of our self-pipe.
    autoclose_fd_t notify_write_fd_{};
};

#endif
