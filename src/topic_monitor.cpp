#include "config.h"  // IWYU pragma: keep

#include "topic_monitor.h"

#include <limits.h>
#include <unistd.h>

#include "flog.h"
#include "iothread.h"
#include "wcstringutil.h"
#include "wutil.h"

// Whoof. Thread Sanitizer swallows signals and replays them at its leisure, at the point where
// instrumented code makes certain blocking calls. But tsan cannot interrupt a signal call, so
// if we're blocked in read() (like the topic monitor wants to be!), we'll never receive SIGCHLD
// and so deadlock. So if tsan is enabled, we mark our fd as non-blocking (so reads will never
// block) and use select() to poll it.
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define TOPIC_MONITOR_TSAN_WORKAROUND
#endif
#endif
#ifdef __SANITIZE_THREAD__
#define TOPIC_MONITOR_TSAN_WORKAROUND
#endif

wcstring generation_list_t::describe() const {
    wcstring result;
    for (generation_t gen : this->as_array()) {
        if (!result.empty()) result.push_back(L',');
        if (gen == invalid_generation) {
            result.append(L"-1");
        } else {
            result.append(to_string(gen));
        }
    }
    return result;
}

binary_semaphore_t::binary_semaphore_t() : sem_ok_(false) {
    // sem_init always fails with ENOSYS on Mac and has an annoying deprecation warning.
    // On BSD sem_init uses a file descriptor under the hood which doesn't get CLOEXEC (see #7304).
    // So use fast semaphores on Linux only.
#ifdef __linux__
    sem_ok_ = (0 == sem_init(&sem_, 0, 0));
#endif
    if (!sem_ok_) {
        auto pipes = make_autoclose_pipes({});
        assert(pipes.has_value() && "Failed to make pubsub pipes");
        pipes_ = pipes.acquire();

#ifdef TOPIC_MONITOR_TSAN_WORKAROUND
        DIE_ON_FAILURE(make_fd_nonblocking(pipes_.read.fd()));
#endif
    }
}

binary_semaphore_t::~binary_semaphore_t() {
#ifndef __APPLE__
    if (sem_ok_) (void)sem_destroy(&sem_);
#endif
}

void binary_semaphore_t::die(const wchar_t *msg) const {
    wperror(msg);
    DIE("unexpected failure");
}

void binary_semaphore_t::post() {
    if (sem_ok_) {
        int res = sem_post(&sem_);
        // sem_post is non-interruptible.
        if (res < 0) die(L"sem_post");
    } else {
        // Write exactly one byte.
        ssize_t ret;
        do {
            const uint8_t v = 0;
            ret = write(pipes_.write.fd(), &v, sizeof v);
        } while (ret < 0 && errno == EINTR);
        if (ret < 0) die(L"write");
    }
}

void binary_semaphore_t::wait() {
    if (sem_ok_) {
        int res;
        do {
            res = sem_wait(&sem_);
        } while (res < 0 && errno == EINTR);
        // Other errors here are very unexpected.
        if (res < 0) die(L"sem_wait");
    } else {
        int fd = pipes_.read.fd();
#ifdef TOPIC_MONITOR_TSAN_WORKAROUND
        // Under tsan our notifying pipe is non-blocking, so we would busy-loop on the read() call
        // until data is available (that is, fish would use 100% cpu while waiting for processes).
        // The select prevents that.
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        (void)select(fd + 1, &fds, nullptr, nullptr, nullptr /* timeout */);
#endif
        // We must read exactly one byte.
        for (;;) {
            uint8_t ignored;
            auto amt = read(fd, &ignored, sizeof ignored);
            if (amt == 1) break;
            if (amt < 0 && errno != EINTR) die(L"read");
        }
    }
}

/// Implementation of the principal monitor. This uses new (and leaks) to avoid registering a
/// pointless at-exit handler for the dtor.
static topic_monitor_t *const s_principal = new topic_monitor_t();

topic_monitor_t &topic_monitor_t::principal() {
    // Do not attempt to move s_principal to a function-level static, it needs to be accessed from a
    // signal handler so it must not be lazily created.
    return *s_principal;
}

topic_monitor_t::topic_monitor_t() = default;
topic_monitor_t::~topic_monitor_t() = default;

void topic_monitor_t::post(topic_t topic) {
    // Beware, we may be in a signal handler!
    // Atomically update the pending topics.
    const uint8_t topicbit = topic_to_bit(topic);

    // CAS in our bit, capturing the old status value.
    status_bits_t oldstatus;
    bool cas_success = false;
    while (!cas_success) {
        oldstatus = status_.load(std::memory_order_relaxed);
        // Clear wakeup bit and set our topic bit.
        status_bits_t newstatus = oldstatus;
        newstatus &= ~STATUS_NEEDS_WAKEUP;
        newstatus |= topicbit;
        cas_success = status_.compare_exchange_weak(oldstatus, newstatus);
    }
    // Note that if the STATUS_NEEDS_WAKEUP bit is set, no other bits must be set.
    assert(((oldstatus == STATUS_NEEDS_WAKEUP) == bool(oldstatus & STATUS_NEEDS_WAKEUP)) &&
           "If STATUS_NEEDS_WAKEUP is set no other bits should be set");

    // If the bit was already set, then someone else posted to this topic and nobody has reacted to
    // it yet. In that case we're done.
    if (oldstatus & topicbit) {
        return;
    }

    // We set a new bit.
    // Check if we should wake up a thread because it was waiting.
    if (oldstatus & STATUS_NEEDS_WAKEUP) {
        std::atomic_thread_fence(std::memory_order_release);
        sema_.post();
    }
}

generation_list_t topic_monitor_t::updated_gens_in_data(acquired_lock<data_t> &data) {
    // Atomically acquire the pending updates, swapping in 0.
    // If there are no pending updates (likely) or a thread is waiting, just return.
    // Otherwise CAS in 0 and update our topics.
    const auto relaxed = std::memory_order_relaxed;
    topic_bitmask_t changed_topic_bits;
    bool cas_success;
    do {
        changed_topic_bits = status_.load(relaxed);
        if (changed_topic_bits == 0 || changed_topic_bits == STATUS_NEEDS_WAKEUP)
            return data->current;
        cas_success = status_.compare_exchange_weak(changed_topic_bits, 0);
    } while (!cas_success);
    assert((changed_topic_bits & STATUS_NEEDS_WAKEUP) == 0 &&
           "Thread waiting bit should not be set");

    // Update the current generation with our topics and return it.
    for (topic_t topic : all_topics()) {
        if (changed_topic_bits & topic_to_bit(topic)) {
            data->current.at(topic) += 1;
            FLOG(topic_monitor, "Updating topic", static_cast<int>(topic), "to",
                 data->current.at(topic));
        }
    }
    // Report our change.
    data_notifier_.notify_all();
    return data->current;
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
        FLOG(topic_monitor, "TID", thread_id(), "local ", gens->describe(), ": current",
             current.describe());
        if (*gens != current) {
            *gens = current;
            break;
        }

        // The generations haven't changed. Perhaps we become the reader.
        // Note we still hold the lock, so this cannot race with any other thread becoming the
        // reader.
        if (data->has_reader) {
            // We already have a reader, wait for it to notify us and loop again.
            data_notifier_.wait(data.get_lock());
            continue;
        } else {
            // We will try to become the reader.
            // Reader bit should not be set in this case.
            assert((status_.load() & STATUS_NEEDS_WAKEUP) == 0 && "No thread should be waiting");
            // Try becoming the reader by marking the reader bit.
            status_bits_t expected_old = 0;
            if (!status_.compare_exchange_strong(expected_old, STATUS_NEEDS_WAKEUP)) {
                // We failed to become the reader, perhaps because another topic post just arrived.
                // Loop again.
                continue;
            }
            // We successfully did a CAS from 0 -> STATUS_NEEDS_WAKEUP.
            // Now any successive topic post must signal us.
            FLOG(topic_monitor, "TID", thread_id(), "becoming reader");
            become_reader = true;
            data->has_reader = true;
            break;
        }
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

            // Wait to be woken up.
            sema_.wait();

            // We are finished waiting. We must stop being the reader, and post on the condition
            // variable to wake up any other threads waiting for us to finish reading.
            auto data = data_.acquire();
            gens = data->current;
            FLOG(topic_monitor, "TID", thread_id(), "local", input_gens.describe(),
                 "read() complete, current is", gens.describe());
            assert(data->has_reader && "We should be the reader");
            data->has_reader = false;
            data_notifier_.notify_all();
        }
    }
    return gens;
}

bool topic_monitor_t::check(generation_list_t *gens, bool wait) {
    if (!gens->any_valid()) return false;

    generation_list_t current = updated_gens();
    bool changed = false;
    for (;;) {
        // Load the topic list and see if anything has changed.
        for (topic_t topic : all_topics()) {
            if (gens->is_valid(topic)) {
                assert(gens->at(topic) <= current.at(topic) &&
                       "Incoming gen count exceeded published count");
                if (gens->at(topic) < current.at(topic)) {
                    gens->at(topic) = current.at(topic);
                    changed = true;
                }
            }
        }

        // If we're not waiting, or something changed, then we're done.
        if (!wait || changed) {
            break;
        }

        // Wait until our gens change.
        current = await_gens(current);
    }
    return changed;
}
