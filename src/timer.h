// Prototypes for executing builtin_time function.
#ifndef FISH_TIMER_H
#define FISH_TIMER_H

#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>

#include <chrono>

#include "common.h"

class parser_t;
struct io_streams_t;

cleanup_t push_timer(bool enabled);

struct timer_snapshot_t {
   public:
    struct rusage cpu_fish;
    struct rusage cpu_children;
    std::chrono::time_point<std::chrono::steady_clock> wall;

    static timer_snapshot_t take();
    static wcstring print_delta(timer_snapshot_t t1, timer_snapshot_t t2, bool verbose = false);

   private:
    timer_snapshot_t() {}
};

#endif
