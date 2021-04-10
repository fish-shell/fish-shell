#ifndef FISH_FD_MONITOR_H
#define FISH_FD_MONITOR_H

#include <chrono>
#include <cstdint>
#include <functional>
// Needed for musl
#include <sys/select.h>  // IWYU pragma: keep

#include "common.h"
#include "fds.h"
#include "maybe.h"

class fd_monitor_t;

/// Each item added to fd_monitor_t is assigned a unique ID, which is not recycled.
/// Items may have their callback triggered immediately by passing the ID.
/// Zero is a sentinel.
using fd_monitor_item_id_t = uint64_t;

/// Reasons for waking an item.
enum class item_wake_reason_t {
    readable,  // the fd became readable
    timeout,   // the requested timeout was hit
    poke,      // the item was "poked" (woken up explicitly)
};

/// An item containing an fd and callback, which can be monitored to watch when it becomes readable,
/// and invoke the callback.
struct fd_monitor_item_t {
    /// The callback type for the item. It is passed \p fd, and the reason for waking \p reason.
    /// The callback may close \p fd, in which case the item is removed.
    using callback_t = std::function<void(autoclose_fd_t &fd, item_wake_reason_t reason)>;

    /// A sentinel value meaning no timeout.
    static constexpr uint64_t kNoTimeout = select_wrapper_t::kNoTimeout;

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

    // The ID for this item. This is assigned by the fd monitor.
    fd_monitor_item_id_t item_id{0};

    // \return the number of microseconds until the timeout should trigger, or kNoTimeout for none.
    // A 0 return means we are at or past the timeout.
    uint64_t usec_remaining(const time_point_t &now) const;

    // Invoke this item's callback if its value is set in fd or has timed out.
    // \return true to retain the item, false to remove it.
    bool service_item(const select_wrapper_t &fds, const time_point_t &now);

    // Invoke this item's callback with a poke, if its ID is present in the (sorted) pokelist.
    // \return true to retain the item, false to remove it.
    using poke_list_t = std::vector<fd_monitor_item_id_t>;
    bool poke_item(const poke_list_t &pokelist);

    friend class fd_monitor_t;
};

/// A class which can monitor a set of fds, invoking a callback when any becomes readable, or when
/// per-item-configurable timeouts are hit.
class fd_monitor_t {
   public:
    using item_list_t = std::vector<fd_monitor_item_t>;

    // A "pokelist" is a sorted list of item IDs which need explicit wakeups.
    using poke_list_t = std::vector<fd_monitor_item_id_t>;

    fd_monitor_t();
    ~fd_monitor_t();

    /// Add an item to monitor. \return the ID assigned to the item.
    fd_monitor_item_id_t add(fd_monitor_item_t &&item);

    /// Mark that an item with a given ID needs to be explicitly woken up.
    void poke_item(fd_monitor_item_id_t item_id);

   private:
    // The background thread runner.
    void run_in_background();

    // If our self-signaller is reported as ready, this reads from it and handles any changes.
    // Called in the background thread.
    void handle_self_signal_in_background();

    // Poke items in the pokelist, removing any items that close their FD.
    // The pokelist is consumed after this.
    // This is only called in the background thread.
    void poke_in_background(const poke_list_t &pokelist);

    // The list of items to monitor. This is only accessed on the background thread.
    item_list_t items_{};

    struct data_t {
        /// Pending items. This is set under the lock, then the background thread grabs them.
        item_list_t pending{};

        /// List of IDs for items that need to be poked (explicitly woken up).
        poke_list_t pokelist{};

        /// The last ID assigned, or if none.
        fd_monitor_item_id_t last_id{0};

        /// Whether the thread is running.
        bool running{false};

        // Set if we should terminate.
        bool terminate{false};
    };
    owning_lock<data_t> data_;

    /// Our self-signaller. When this is written to, it means there are new items pending, or new
    /// items in the pokelist, or terminate is set.
    fd_event_signaller_t change_signaller_;
};

#endif
