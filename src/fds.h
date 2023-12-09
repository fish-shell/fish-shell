/** Facilities for working with file descriptors. */

#ifndef FISH_FDS_H
#define FISH_FDS_H

#include "config.h"  // IWYU pragma: keep

#include <poll.h>        // IWYU pragma: keep
#include <sys/select.h>  // IWYU pragma: keep
#include <sys/types.h>

#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#include "common.h"
#include "maybe.h"

#if INCLUDE_RUST_HEADERS
#include "fds.rs.h"
#endif

/// A sentinel value indicating no timeout.
#define kNoTimeout (std::numeric_limits<uint64_t>::max())

/// Mark an fd as blocking; returns errno or 0 on success.
int make_fd_blocking(int fd);

#endif
