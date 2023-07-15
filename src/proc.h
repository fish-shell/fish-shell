// Prototypes for utilities for keeping track of jobs, processes and subshells, as well as signal
// handling functions for tracking children. These functions do not themselves launch new processes,
// the exec library will call proc to create representations of the running jobs as needed.
#ifndef FISH_PROC_H
#define FISH_PROC_H
#include "config.h"  // IWYU pragma: keep

#include <sys/time.h>  // IWYU pragma: keep
#include <sys/wait.h>  // IWYU pragma: keep

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ast.h"
#include "common.h"
#include "cxx.h"
#include "maybe.h"
#include "parse_tree.h"
#include "parser.h"
#include "redirection.h"
#include "topic_monitor.h"

struct Parser;

#if INCLUDE_RUST_HEADERS
#include "proc.rs.h"
#else
struct JobRefFfi;
struct JobGroupRefFfi;
struct JobListFFI;
#endif

#endif
