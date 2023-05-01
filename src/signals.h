// The library for various signal related issues.
#ifndef FISH_SIGNALH
#define FISH_SIGNALH

#include <csignal>
#include <cstdint>

#if INCLUDE_RUST_HEADERS
#include "signal.rs.h"
#endif

/// Returns signals with non-default handlers.
void get_signals_with_handlers(sigset_t *set);

enum class topic_t : uint8_t;
/// A sigint_detector_t can be used to check if a SIGINT (or SIGHUP) has been delivered.
class sigchecker_t {
    const topic_t topic_;
    uint64_t gen_{0};

   public:
    sigchecker_t(topic_t signal);

    /// Check if a sigint has been delivered since the last call to check(), or since the detector
    /// was created.
    bool check();

    /// Wait until a sigint is delivered.
    void wait() const;
};

#endif
