// Prototypes for executing builtin_time function.
#ifndef FISH_TIMER_H
#define FISH_TIMER_H

#include <sys/resource.h>

#include <chrono>

#include "common.h"

cleanup_t push_timer(bool enabled);

struct timer_snapshot_t {
   public:
    struct rusage cpu_fish;
    struct rusage cpu_children;
    std::chrono::time_point<std::chrono::steady_clock> wall;

    static timer_snapshot_t take();
    static wcstring print_delta(const timer_snapshot_t &t1, const timer_snapshot_t &t2,
                                bool verbose = false);

   private:
    timer_snapshot_t() {}
};

#endif
