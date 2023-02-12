#ifndef FISH_EVENT_H
#define FISH_EVENT_H

#include <unistd.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "common.h"
#include "global_safety.h"
#include "wutil.h"

#if INCLUDE_RUST_HEADERS

#include "event.rs.h"

#else

// Hacks to allow us to compile without Rust headers.

struct Event;
struct EventHandler;

#endif

/// The process id that is used to match any process id.
#define EVENT_ANY_PID 0

/// Null-terminated list of valid event filter names.
/// These are what are valid to pass to 'functions --handlers-type'
extern const wchar_t *const event_filter_names[];

class parser_t;

void event_fire_generic(parser_t &parser, const wcstring &name, wcstring_list_t args = {});

#endif
