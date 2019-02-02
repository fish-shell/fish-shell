#ifndef FISH_TOPIC_MONITOR_H
#define FISH_TOPIC_MONITOR_H

#include "common.h"
#include "enum_set.h"
#include "io.h"

#include <array>
#include <atomic>
#include <bitset>
#include <condition_variable>
#include <limits>
#include <numeric>
#include <queue>

/** Topic monitoring support. Topics are conceptually "a thing that can happen." For example,
 delivery of a SIGINT, a child process exits, etc. It is possible to post to a topic, which means
 that that thing happened.

 Associated with each topic is a current generation, which is a 64 bit value. When you query a
 topic, you get back a generation. If on the next query the generation has increased, then it
 indicates someone posted to the topic.

 For example, if you are monitoring a child process, you can query the sigchld topic. If it has
 increased since your last query, it is possible that your child process has exited.

 Topic postings may be coalesced. That is there may be two posts to a given topic, yet the
 generation only increases by 1. The only guarantee is that after a topic post, the current
 generation value is larger than any value previously queried.

 Tying this all together is the topic_monitor_t. This provides the current topic generations, and
 also provides the ability to perform a blocking wait for any topic to change in a particular topic
 set. This is the real power of topics: you can wait for a sigchld signal OR a thread exit.
 */

/// The list of topics that may be observed.
enum class topic_t : uint8_t {
    sigchld,  // Corresponds to SIGCHLD signal.
    COUNT
};

/// Allow enum_iter to be used.
template <>
struct enum_info_t<topic_t> {
    static constexpr auto count = topic_t::COUNT;
};

/// Set of topics.
using topic_set_t = enum_set_t<topic_t>;

/// Counting iterator for topics.
using topic_iter_t = enum_iter_t<topic_t>;

/// A generation is a counter incremented every time the value of a topic changes.
/// It is 64 bit so it will never wrap.
using generation_t = uint64_t;

/// A generation value which is guaranteed to never be set and be larger than any valid generation.
constexpr generation_t invalid_generation = std::numeric_limits<generation_t>::max();

/// List of generation values, indexed by topics.
/// The initial value of a generation is always 0.
using generation_list_t = enum_array_t<generation_t, topic_t>;

/// Teh topic monitor class. This permits querying the current generation values for topics,
/// optionally blocking until they increase.
class topic_monitor_t {
   private:
    using topic_set_raw_t = uint8_t;

    static_assert(sizeof(topic_set_raw_t) * CHAR_BIT >= enum_count<topic_t>(),
                  "topic_set_raw is too small");

    /// The current topic generation list, protected by a mutex. Note this may be opportunistically
    /// updated at the point it is queried.
    owning_lock<generation_list_t> current_gen_{{}};

    /// The set of topics which have pending increments.
    /// This is managed via atomics.
    std::atomic<topic_set_raw_t> pending_updates_{};

    /// When a topic set is queried in a blocking way, the waiters are put into a queue. The waiter
    /// with the smallest metagen is responsible for announcing the change to the rest of the
    /// waiters. (The metagen is just the sum of the current generations.) Note that this is a
    /// max-heap that defaults to std::less; by using std::greater it becomes a min heap. This is
    /// protected by wait_queue_lock_.
    std::priority_queue<generation_t, std::vector<generation_t>, std::greater<generation_t>>
        wait_queue_;

    /// Mutex guarding the wait queue.
    std::mutex wait_queue_lock_{};

    /// Condition variable for broadcasting notifications.
    std::condition_variable wait_queue_notifier_{};

    /// Pipes used to communicate changes from the signal handler.
    autoclose_pipes_t pipes_;

    /// \return the metagen for a topic generation list.
    /// The metagen is simply the sum of topic generations. Note it is monotone.
    static inline generation_t metagen_for(const generation_list_t &lst) {
        return std::accumulate(lst.begin(), lst.end(), generation_t{0});
    }

    /// Wait for the current metagen to become different from \p gen.
    /// If it is already different, return immediately.
    void await_metagen(generation_t gen);

    /// Return the current generation list, opportunistically applying any pending updates.
    generation_list_t updated_gens() {
        auto current_gens = current_gen_.acquire();

        // Atomically acquire the pending updates, swapping in 0.
        // If there are no pending updates (likely), just return.
        // Otherwise CAS in 0 and update our topics.
        const auto relaxed = std::memory_order_relaxed;
        topic_set_raw_t raw;
        bool cas_success;
        do {
            raw = pending_updates_.load(relaxed);
            if (raw == 0) return *current_gens;
            cas_success = pending_updates_.compare_exchange_weak(raw, 0, relaxed, relaxed);
        } while (!cas_success);

        // Update the current generation with our topics and return it.
        auto topics = topic_set_t::from_raw(raw);
        for (topic_t topic : topic_iter_t{}) {
            current_gens->at(topic) += topics.get(topic) ? 1 : 0;
        }
        return *current_gens;
    }

    /// \return the metagen for the current topic generation list.
    inline generation_t current_metagen() { return metagen_for(updated_gens()); }

   public:
    topic_monitor_t();
    ~topic_monitor_t();

    /// topic_monitors should not be copied, and there should be no reason to move one.
    void operator=(const topic_monitor_t &) = delete;
    topic_monitor_t(const topic_monitor_t &) = delete;
    void operator=(topic_monitor_t &&) = delete;
    topic_monitor_t(topic_monitor_t &&) = delete;

    /// The principal topic_monitor. This may be fetched from a signal handler.
    static topic_monitor_t &principal();

    /// Post to a topic, potentially from a signal handler.
    void post(topic_t topic);

    /// Access the current generations.
    generation_list_t current_generations() { return updated_gens(); }

    /// Access the generation for a topic.
    generation_t generation_for_topic(topic_t topic) { return current_generations().at(topic); }

    /// See if for any topic (specified in \p topics) has changed from the values in the generation
    /// list \p gens. If \p wait is set, then wait if there are no changes; otherwise return
    /// immediately.
    /// \return the set of topics that changed, updating the generation list \p gens.
    topic_set_t check(generation_list_t *gens, topic_set_t topics, bool wait);
};

#endif
