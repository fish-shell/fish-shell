// Support for monitoring a set of fds.
#include "config.h"  // IWYU pragma: keep

#include "fd_monitor.h"

#include <errno.h>

#include <algorithm>
#include <iterator>
#include <thread>  //this_thread::sleep_for
#include <type_traits>

#include "flog.h"
#include "iothread.h"
#include "wutil.h"

static constexpr uint64_t kUsecPerMsec = 1000;

fd_monitor_t::fd_monitor_t() = default;

fd_monitor_t::~fd_monitor_t() {
    // In orindary usage, we never invoke the dtor.
    // This is used in the tests to not leave stale fds around.
    // That is why this is very hacky!
    data_.acquire()->terminate = true;
    change_signaller_.post();
    while (data_.acquire()->running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

fd_monitor_item_id_t fd_monitor_t::add(fd_monitor_item_t &&item) {
    assert(item.fd.valid() && "Invalid fd");
    assert(item.timeout_usec != 0 && "Invalid timeout");
    assert(item.item_id == 0 && "Item should not already have an ID");
    bool start_thread = false;
    fd_monitor_item_id_t item_id{};
    {
        // Lock around a local region.
        auto data = data_.acquire();

        // Assign an id and add the item to pending.
        item_id = ++data->last_id;
        item.item_id = item_id;
        data->pending.push_back(std::move(item));

        // Maybe plan to start the thread.
        if (!data->running) {
            FLOG(fd_monitor, "Thread starting");
            data->running = true;
            start_thread = true;
        }
    }
    if (start_thread) {
        void *(*trampoline)(void *) = [](void *self) -> void * {
            static_cast<fd_monitor_t *>(self)->run_in_background();
            return nullptr;
        };
        bool made_thread = make_detached_pthread(trampoline, this);
        if (!made_thread) {
            DIE("Unable to create a new pthread");
        }
    }
    // Tickle our signaller.
    change_signaller_.post();
    return item_id;
}

void fd_monitor_t::poke_item(fd_monitor_item_id_t item_id) {
    assert(item_id > 0 && "Invalid item ID");
    bool needs_notification = false;
    {
        auto data = data_.acquire();
        needs_notification = data->pokelist.empty();
        // Insert it, sorted.
        auto where = std::lower_bound(data->pokelist.begin(), data->pokelist.end(), item_id);
        data->pokelist.insert(where, item_id);
    }
    if (needs_notification) {
        change_signaller_.post();
    }
}

uint64_t fd_monitor_item_t::usec_remaining(const time_point_t &now) const {
    assert(last_time.has_value() && "Should always have a last_time");
    if (timeout_usec == kNoTimeout) return kNoTimeout;
    assert(now >= *last_time && "steady clock went backwards!");
    uint64_t since = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(now - *last_time).count());
    return since >= timeout_usec ? 0 : timeout_usec - since;
}

bool fd_monitor_item_t::service_item(const fd_readable_set_t &fds, const time_point_t &now) {
    bool should_retain = true;
    bool readable = fds.test(fd.fd());
    bool timed_out = !readable && usec_remaining(now) == 0;
    if (readable || timed_out) {
        last_time = now;
        item_wake_reason_t reason =
            readable ? item_wake_reason_t::readable : item_wake_reason_t::timeout;
        callback(fd, reason);
        should_retain = fd.valid();
    }
    return should_retain;
}

bool fd_monitor_item_t::poke_item(const poke_list_t &pokelist) {
    if (item_id == 0 || !std::binary_search(pokelist.begin(), pokelist.end(), item_id)) {
        // Not pokeable or not in the pokelist.
        return true;
    }
    callback(fd, item_wake_reason_t::poke);
    return fd.valid();
}

void fd_monitor_t::run_in_background() {
    ASSERT_IS_BACKGROUND_THREAD();
    poke_list_t pokelist;
    fd_readable_set_t fds;
    for (;;) {
        // Poke any items that need it.
        if (!pokelist.empty()) {
            this->poke_in_background(pokelist);
            pokelist.clear();
        }

        fds.clear();

        // Our change_signaller is special cased.
        int change_signal_fd = change_signaller_.read_fd();
        fds.add(change_signal_fd);

        auto now = std::chrono::steady_clock::now();
        uint64_t timeout_usec = fd_monitor_item_t::kNoTimeout;

        for (auto &item : items_) {
            fds.add(item.fd.fd());
            if (!item.last_time.has_value()) item.last_time = now;
            timeout_usec = std::min(timeout_usec, item.usec_remaining(now));
        }

        // If we have no items, then we wish to allow the thread to exit, but after a time, so we
        // aren't spinning up and tearing down the thread repeatedly.
        // Set a timeout of 256 msec; if nothing becomes readable by then we will exit.
        // We refer to this as the wait-lap.
        bool is_wait_lap = (items_.size() == 0);
        if (is_wait_lap) {
            assert(timeout_usec == fd_monitor_item_t::kNoTimeout &&
                   "Should not have a timeout on wait-lap");
            timeout_usec = 256 * kUsecPerMsec;
        }

        // Call select().
        int ret = fds.check_readable(timeout_usec);
        if (ret < 0 && errno != EINTR) {
            // Surprising error.
            wperror(L"select");
        }

        // A predicate which services each item in turn, returning true if it should be removed.
        auto servicer = [&fds, &now](fd_monitor_item_t &item) {
            int fd = item.fd.fd();
            bool remove = !item.service_item(fds, now);
            if (remove) FLOG(fd_monitor, "Removing fd", fd);
            return remove;
        };

        // Service all items that are either readable or timed out, and remove any which say to do
        // so.
        now = std::chrono::steady_clock::now();
        items_.erase(std::remove_if(items_.begin(), items_.end(), servicer), items_.end());

        // Handle any changes if the change signaller was set. Alternatively this may be the wait
        // lap, in which case we might want to commit to exiting.
        bool change_signalled = fds.test(change_signal_fd);
        if (change_signalled || is_wait_lap) {
            // Clear the change signaller before processing incoming changes.
            change_signaller_.try_consume();
            auto data = data_.acquire();

            // Move from 'pending' to 'items'.
            items_.insert(items_.end(), std::make_move_iterator(data->pending.begin()),
                          std::make_move_iterator(data->pending.end()));
            data->pending.clear();

            // Grab any pokelist.
            assert(pokelist.empty() && "pokelist should be empty or else we're dropping pokes");
            pokelist = std::move(data->pokelist);
            data->pokelist.clear();

            if (data->terminate ||
                (is_wait_lap && items_.empty() && pokelist.empty() && !change_signalled)) {
                // Maybe terminate is set.
                // Alternatively, maybe we had no items, waited a bit, and still have no items.
                // It's important to do this while holding the lock, otherwise we race with new
                // items being added.
                assert(data->running && "Thread should be running because we're that thread");
                FLOG(fd_monitor, "Thread exiting");
                data->running = false;
                return;
            }
        }
    }
}

void fd_monitor_t::poke_in_background(const poke_list_t &pokelist) {
    ASSERT_IS_BACKGROUND_THREAD();
    auto poker = [&pokelist](fd_monitor_item_t &item) {
        int fd = item.fd.fd();
        bool remove = !item.poke_item(pokelist);
        if (remove) FLOG(fd_monitor, "Removing fd", fd);
        return remove;
    };
    items_.erase(std::remove_if(items_.begin(), items_.end(), poker), items_.end());
}
