#ifndef FISH_ENV_UNIVERSAL_COMMON_H
#define FISH_ENV_UNIVERSAL_COMMON_H
#include "config.h"  // IWYU pragma: keep

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common.h"
#include "env.h"
#include "fds.h"
#include "maybe.h"
#include "wutil.h"

#if INCLUDE_RUST_HEADERS
#include "env_universal_common.rs.h"
#endif

#endif
