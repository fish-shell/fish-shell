#include "config.h"  // IWYU pragma: keep

#include "topic_monitor.h"

#include <limits.h>
#include <unistd.h>

#include "flog.h"
#include "iothread.h"
#include "wutil.h"

// Whoof. Thread Sanitizer swallows signals and replays them at its leisure, at the point where
// instrumented code makes certain blocking calls. But tsan cannot interrupt a signal call, so
// if we're blocked in read() (like the topic monitor wants to be!), we'll never receive SIGCHLD
// and so deadlock. So if tsan is enabled, we mark our fd as non-blocking (so reads will never
// block) and use use select() to poll it.
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define TOPIC_MONITOR_TSAN_WORKAROUND
#endif
#endif
#ifdef __SANITIZE_THREAD__
#define TOPIC_MONITOR_TSAN_WORKAROUND
#endif

/// Implementation of the principal monitor. This uses new (and leaks) to avoid registering a
/// pointless at-exit handler for the dtor.
static topic_monitor_t *const s_principal = new topic_monitor_t();

/// \return the metagen for a topic generation list.
/// The metagen is simply the sum of topic generations. Note it is monotone.
static generation_t metagen_for(const generation_list_t &lst) {
    return std::accumulate(lst.begin(), lst.end(), generation_t{0});
}

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

#ifdef TOPIC_MONITOR_TSAN_WORKAROUND
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

generation_list_t topic_monitor_t::updated_gens_in_data(acquired_lock<data_t> &data) {
    // Atomically acquire the pending updates, swapping in 0.
    // If there are no pending updates (likely), just return.
    // Otherwise CAS in 0 and update our topics.
    const auto relaxed = std::memory_order_relaxed;
    topic_set_raw_t raw;
    bool cas_success;
    do {
        raw = pending_updates_.load(relaxed);
        if (raw == 0) return data->current_gens;
        cas_success = pending_updates_.compare_exchange_weak(raw, 0, relaxed, relaxed);
    } while (!cas_success);

    // Update the current generation with our topics and return it.
    auto topics = topic_set_t::from_raw(raw);
    for (topic_t topic : topic_iter_t{}) {
        if (topics.get(topic)) {
            data->current_gens.at(topic) += 1;
            FLOG(topic_monitor, "Updating topic", static_cast<int>(topic), "to",
                 data->current_gens.at(topic));
        }
    }
    // Report our change.
    data_notifier_.notify_all();
    return data->current_gens;
}

generation_list_t topic_monitor_t::updated_gens() {
    auto data = data_.acquire();
    return updated_gens_in_data(data);
}

bool topic_monitor_t::try_update_gens_maybe_becoming_reader(generation_list_t *gens) {
    bool become_reader = false;
    auto data = data_.acquire();
    for (;;) {
        // See if the updated gen list has changed. If so we don't need to become the reader.
        auto current = updated_gens_in_data(data);
        FLOG(topic_monitor, "TID", thread_id(), "local mgen", metagen_for(*gens), ": current",
             metagen_for(current));
        if (*gens != current) {
            *gens = current;
            break;
        }

        // The generations haven't changed. Perhaps we become the reader.
        if (!data->has_reader) {
            become_reader = true;
            data->has_reader = true;
            break;
        }
        // Not the reader, wait until the reader notifies us and loop again.
        data_notifier_.wait(data.get_lock());
    }
    return become_reader;
}

generation_list_t topic_monitor_t::await_gens(const generation_list_t &input_gens) {
    generation_list_t gens = input_gens;
    while (gens == input_gens) {
        bool become_reader = try_update_gens_maybe_becoming_reader(&gens);
        if (become_reader) {
            // Now we are the reader. Read from the pipe, and then update with any changes.
            // Note we no longer hold the lock.
            assert(gens == input_gens &&
                   "Generations should not have changed if we are the reader.");
            int fd = pipes_.read.fd();
#ifdef TOPIC_MONITOR_TSAN_WORKAROUND
            // Under tsan our notifying pipe is non-blocking, so we would busy-loop on the read()
            // call until data is available (that is, fish would use 100% cpu while waiting for
            // processes). The select prevents that.
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            (void)select(fd + 1, &fds, nullptr, nullptr, nullptr /* timeout */);
#endif
            uint8_t ignored[PIPE_BUF];
            auto unused = read(fd, ignored, sizeof ignored);
            if (unused) {
            }

            // We are finished reading. We must stop being the reader, and post on the condition
            // variable to wake up any other threads waiting for us to finish reading.
            auto data = data_.acquire();
            gens = data->current_gens;
            FLOG(topic_monitor, "TID", thread_id(), "local mgen", metagen_for(input_gens),
                 "read() complete, current mgen is", metagen_for(gens));
            assert(data->has_reader && "We should be the reader");
            data->has_reader = false;
            data_notifier_.notify_all();
        }
    }
    return gens;
}

topic_set_t topic_monitor_t::check(generation_list_t *gens, topic_set_t topics, bool wait) {
    if (topics.none()) return topics;

    generation_list_t current = updated_gens();
    topic_set_t changed{};
    for (;;) {
        // Load the topic list and see if anything has changed.
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

        // Wait until our gens change.
        current = await_gens(current);
    }
    return changed;
}
