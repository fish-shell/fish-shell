#ifndef FISH_REDIRECTION_H
#define FISH_REDIRECTION_H

#if INCLUDE_RUST_HEADERS

#include "redirection.rs.h"

#else

// Hacks to allow us to compile without Rust headers.

enum class RedirectionMode {
    overwrite,
    append,
    input,
    fd,
    noclob,
};
struct Dup2Action;
class Dup2List;
struct RedirectionSpec;
struct RedirectionSpecList;

#endif

using redirection_mode_t = RedirectionMode;
using redirection_spec_t = RedirectionSpec;
using redirection_spec_list_t = RedirectionSpecList;
using dup2_action_t = Dup2Action;
using dup2_list_t = Dup2List;

#endif
