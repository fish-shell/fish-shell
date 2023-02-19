#ifndef FISH_TOPIC_MONITOR_H
#define FISH_TOPIC_MONITOR_H

#include "config.h"

#include <stdint.h>

using generation_t = uint64_t;

#if INCLUDE_RUST_HEADERS

#include "topic_monitor.rs.h"

#else

// Hacks to allow us to compile without Rust headers.
struct generation_list_t {
    uint64_t sighupint;
    uint64_t sigchld;
    uint64_t internal_exit;
};

#endif

#endif
