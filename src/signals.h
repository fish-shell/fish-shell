// The library for various signal related issues.
#ifndef FISH_SIGNALH
#define FISH_SIGNALH

#include <csignal>
#include <cstdint>

#if INCLUDE_RUST_HEADERS
#include "signal.rs.h"
#else
struct IoStreams;
struct SigChecker;
#endif

using sigchecker_t = SigChecker;

#endif
