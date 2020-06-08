// Support for monitoring a set of fds.
#include "config.h"  // IWYU pragma: keep

#include "fd_monitor.h"

#include "flog.h"
#include "io.h"
#include "iothread.h"
#include "wutil.h"

static constexpr uint64_t kUsecPerMsec = 1000;
static constexpr uint64_t kUsecPerSec = 1000 * kUsecPerMsec;

fd_monitor_t::fd_monitor_t() {
    auto self_pipe = make_autoclose_pipes({});
    if (!self_pipe) {
        DIE("Unable to create pipes");
    }

    // Ensure the write side is nonblocking to avoid deadlock.
    notify_write_fd_ = std::move(self_pipe->write);
    if (make_fd_nonblocking(notify_write_fd_.fd())) {
        wperror(L"fcntl");
    }

    // Add an item for ourselves.
    // We don't need to go through 'pending' because we have not yet launched the thread, and don't
    // want to yet.
    auto callback = [this](const autoclose_fd_t &fd, bool timed_out) {
        ASSERT_IS_BACKGROUND_THREAD();
        assert(!timed_out && "Should not time out with kNoTimeout");
        (void)timed_out;
        // Read some to take data off of the notifier.
        char buff[4096];
        ssize_t amt = read(fd.fd(), buff, sizeof buff);
        if (amt > 0) {
            this->has_pending_ = true;
        } else if (amt == 0) {
            this->terminate_ = true;
        } else {
            wperror(L"read");
        }
    };
    items_.push_back(fd_monitor_item_t(std::move(self_pipe->read), std::move(callback)));
}

// Extremely hacky destructor to clean up.
// This is used in the tests to not leave stale fds around.
// In fish shell, we never invoke the dtor so it doesn't matter that this is very dumb.
fd_monitor_t::~fd_monitor_t() {
    notify_write_fd_.close();
    while (data_.acquire()->running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void fd_monitor_t::add(fd_monitor_item_t &&item) {
    assert(item.fd.valid() && "Invalid fd");
    assert(item.timeout_usec != 0 && "Invalid timeout");
    bool start_thread = add_pending_get_start_thread(std::move(item));
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
    // Tickle our notifier.
    char byte = 0;
    (void)write_loop(notify_write_fd_.fd(), &byte, 1);
}

bool fd_monitor_t::add_pending_get_start_thread(fd_monitor_item_t &&item) {
    auto data = data_.acquire();
    data->pending.push_back(std::move(item));
    if (!data->running) {
        FLOG(fd_monitor, "Thread starting");
        data->running = true;
        return true;
    }
    return false;
}

// Given a usec count, populate and return a timeval.
// If the usec count is kNoTimeout, return nullptr.
static struct timeval *usec_to_tv_or_null(uint64_t usec, struct timeval *timeout) {
    if (usec == fd_monitor_item_t::kNoTimeout) return nullptr;
    timeout->tv_sec = usec / kUsecPerSec;
    timeout->tv_usec = usec % kUsecPerSec;
    return timeout;
}

uint64_t fd_monitor_item_t::usec_remaining(const time_point_t &now) const {
    assert(last_time.has_value() && "Should always have a last_time");
    if (timeout_usec == kNoTimeout) return kNoTimeout;
    assert(now >= *last_time && "steady clock went backwards!");
    uint64_t since = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(now - *last_time).count());
    return since >= timeout_usec ? 0 : timeout_usec - since;
}

bool fd_monitor_item_t::service_item(const fd_set *fds, const time_point_t &now) {
    bool should_retain = true;
    bool readable = FD_ISSET(fd.fd(), fds);
    bool timed_out = !readable && usec_remaining(now) == 0;
    if (readable || timed_out) {
        last_time = now;
        callback(fd, timed_out);
        should_retain = fd.valid();
    }
    return should_retain;
}

void fd_monitor_t::run_in_background() {
    ASSERT_IS_BACKGROUND_THREAD();
    for (;;) {
        uint64_t timeout_usec = fd_monitor_item_t::kNoTimeout;
        int max_fd = -1;
        fd_set fds;
        FD_ZERO(&fds);
        auto now = std::chrono::steady_clock::now();

        for (auto &item : items_) {
            FD_SET(item.fd.fd(), &fds);
            if (!item.last_time.has_value()) item.last_time = now;
            timeout_usec = std::min(timeout_usec, item.usec_remaining(now));
            max_fd = std::max(max_fd, item.fd.fd());
        }

        // If we have only one item, it means that we are not actively monitoring any fds other than
        // our self-pipe. In this case we wish to allow the thread to exit, but after a time, so we
        // aren't spinning up and tearing down the thread repeatedly.
        // Set a timeout of 16 msec; if nothing becomes readable by then we will exit.
        // We refer to this as the wait-lap.
        bool is_wait_lap = (items_.size() == 1);
        if (is_wait_lap) {
            assert(timeout_usec == fd_monitor_item_t::kNoTimeout &&
                   "Should not have a timeout on wait-lap");
            timeout_usec = 16 * kUsecPerMsec;
        }

        // Call select().
        struct timeval tv;
        int ret = select(max_fd + 1, &fds, nullptr, nullptr, usec_to_tv_or_null(timeout_usec, &tv));
        if (ret < 0 && errno != EINTR) {
            // Surprising error.
            wperror(L"select");
        }

        // A predicate which services each item in turn, returning true if it should be removed.
        auto servicer = [&fds, &now](fd_monitor_item_t &item) {
            int fd = item.fd.fd();
            bool remove = !item.service_item(&fds, now);
            if (remove) FLOG(fd_monitor, "Removing fd", fd);
            return remove;
        };

        // Service all items that are either readable or timed our, and remove any which say to do
        // so.
        now = std::chrono::steady_clock::now();
        items_.erase(std::remove_if(items_.begin(), items_.end(), servicer), items_.end());

        if (terminate_) {
            // Time to go.
            data_.acquire()->running = false;
            return;
        }

        // Maybe we got some new items. Check if our callback says so, or if this is the wait
        // lap, in which case we might want to commit to exiting.
        if (has_pending_ || is_wait_lap) {
            auto data = data_.acquire();
            // Move from 'pending' to 'items'.
            items_.insert(items_.end(), std::make_move_iterator(data->pending.begin()),
                          std::make_move_iterator(data->pending.end()));
            data->pending.clear();
            has_pending_ = false;

            if (is_wait_lap && items_.size() == 1) {
                // We had no items, waited a bit, and still have no items. We're going to shut down.
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
