#include "config.h"  // IWYU pragma: keep

#include "limits.h"
#include "topic_monitor.h"
#include "wutil.h"

#include <unistd.h>

// Whoof. Thread Sanitizer swallows signals and replays them at its leisure, at the point where
// instrumented code makes certain blocking calls. But tsan cannot interrupt a signal call, so
// if we're blocked in read() (like the topic monitor wants to be!), we'll never receive SIGCHLD
// and so deadlock. So if tsan is enabled, we mark our fd as non-blocking (so reads will never
// block) and use use select() to poll it.
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define TOPIC_MONITOR_TSAN_WORKAROUND 1
#endif
#endif

/// Implementation of the principal monitor. This uses new (and leaks) to avoid registering a
/// pointless at-exit handler for the dtor.
static topic_monitor_t *const s_principal = new topic_monitor_t();

topic_monitor_t &topic_monitor_t::principal() {
    // Do not attempt to move s_principal to a function-level static, it needs to be accessed from a
    // signal handler so it must not be lazily created.
    return *s_principal;
}

topic_monitor_t::topic_monitor_t() {
    // Set up our pipes. Assert it succeeds.
    auto pipes = make_autoclose_pipes({});
    assert(pipes.has_value() && "Failed to make pubsub pipes");
    pipes_ = pipes.acquire();

    // Make sure that our write side doesn't block, else we risk hanging in a signal handler.
    // The read end must block to avoid spinning in await.
    DIE_ON_FAILURE(make_fd_nonblocking(pipes_.write.fd()));

#if TOPIC_MONITOR_TSAN_WORKAROUND
    DIE_ON_FAILURE(make_fd_nonblocking(pipes_.read.fd()));
#endif
}

topic_monitor_t::~topic_monitor_t() = default;

void topic_monitor_t::post(topic_t topic) {
    // Beware, we may be in a signal handler!
    // Atomically update the pending topics.
    auto rawtopics = topic_set_t{topic}.to_raw();
    auto oldtopics = pending_updates_.fetch_or(rawtopics, std::memory_order_relaxed);
    if ((oldtopics & rawtopics) == rawtopics) {
        // No new bits were set.
        return;
    }

    // Ok, we changed one or more bits. Ensure the topic change is visible, and announce the change
    // by writing a byte to the pipe.
    std::atomic_thread_fence(std::memory_order_release);
    ssize_t ret;
    do {
        // write() is async signal safe.
        const uint8_t v = 0;
        ret = write(pipes_.write.fd(), &v, sizeof v);
    } while (ret < 0 && errno == EINTR);
    // Ignore EAGAIN and other errors (which conceivably could occur during shutdown).
}

void topic_monitor_t::await_metagen(generation_t mgen) {
    // Fast check of the metagen before taking the lock. If it's changed we're done.
    if (mgen != current_metagen()) return;

    // Take the lock (which may take a long time) and then check again.
    std::unique_lock<std::mutex> locker{wait_queue_lock_};
    if (mgen != current_metagen()) return;

    // Our metagen hasn't changed. Push our metagen onto the queue, then wait until we're the
    // lowest. If multiple waiters are the lowest, then anyone can be the observer.
    // Note the reason for picking the lowest metagen is to avoid a priority inversion where a lower
    // metagen (therefore someone who should see changes) is blocked waiting for a higher metagen
    // (who has already seen the changes).
    wait_queue_.push(mgen);
    while (wait_queue_.top() != mgen) {
        wait_queue_notifier_.wait(locker);
    }
    wait_queue_.pop();

    // We now have the lowest metagen in the wait queue. Notice we still hold the lock.
    // Read until the metagen changes. It may already have changed.
    // Note because changes are coalesced, we can read a lot, potentially draining the pipe.
    while (mgen == current_metagen()) {
        int fd = pipes_.read.fd();
#if TOPIC_MONITOR_TSAN_WORKAROUND
        // Under tsan our notifying pipe is non-blocking, so we would busy-loop on the read() call
        // until data is available (that is, fish would use 100% cpu while waiting for processes).
        // The select prevents that.
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        (void)select(fd + 1, &fds, nullptr, nullptr, nullptr /* timeout */);
#endif
        uint8_t ignored[PIPE_BUF];
        (void)read(fd, ignored, sizeof ignored);
    }

    // Release the lock and wake up the remaining waiters.
    locker.unlock();
    wait_queue_notifier_.notify_all();
}

topic_set_t topic_monitor_t::check(generation_list_t *gens, topic_set_t topics, bool wait) {
    if (topics.none()) return topics;

    topic_set_t changed{};
    for (;;) {
        // Load the topic list and see if anything has changed.
        generation_list_t current = updated_gens();
        for (topic_t topic : topic_iter_t{}) {
            if (topics.get(topic)) {
                assert(gens->at(topic) <= current.at(topic) &&
                       "Incoming gen count exceeded published count");
                if (gens->at(topic) < current.at(topic)) {
                    gens->at(topic) = current.at(topic);
                    changed.set(topic);
                }
            }
        }

        // If we're not waiting, or something changed, then we're done.
        if (!wait || changed.any()) {
            break;
        }

        // Try again. Note that we use the metagen corresponding to the topic list we just
        // inspected, not the current one (which may have updates since we checked).
        await_metagen(metagen_for(current));
    }
    return changed;
}
