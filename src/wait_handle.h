#ifndef FISH_WAIT_HANDLE_H
#define FISH_WAIT_HANDLE_H

// Hacks to allow us to compile without Rust headers.
struct WaitHandleStoreFFI;
struct WaitHandleRefFFI;

#if INCLUDE_RUST_HEADERS
#include "wait_handle.rs.h"
#endif

#endif
