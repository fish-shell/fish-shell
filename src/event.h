// event.h and event.cpp only contain cpp-side ffi compat code to make event.rs.h a drop-in
// replacement. There is no logic still in here that needs to be ported to rust.

#ifndef FISH_EVENT_H
#define FISH_EVENT_H

#include <unistd.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "common.h"
#include "cxx.h"
#include "global_safety.h"
#include "parser.h"
#include "wutil.h"

#if INCLUDE_RUST_HEADERS
#include "event.rs.h"
#else
struct Event;
#endif

/// The process id that is used to match any process id.
// TODO: Remove after porting functions.cpp
#define EVENT_ANY_PID 0

/// Null-terminated list of valid event filter names.
/// These are what are valid to pass to 'functions --handlers-type'
// TODO: Remove after porting functions.cpp
extern const wchar_t *const event_filter_names[];

void event_fire_generic(const parser_t &parser, const wcstring &name,
                        const std::vector<wcstring> &args = g_empty_string_list);

#endif
