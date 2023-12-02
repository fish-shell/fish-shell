// Pager support.
#ifndef FISH_PAGER_H
#define FISH_PAGER_H

#include <stddef.h>

#include <string>
#include <vector>

#include "common.h"
#include "complete.h"
#include "cxx.h"
#include "highlight.h"
#include "reader.h"
#include "screen.h"

struct termsize_t;

#define PAGER_SELECTION_NONE static_cast<size_t>(-1)

#if INCLUDE_RUST_HEADERS
#include "pager.rs.h"
#else
struct PageRendering;
enum class selection_motion_t;
struct Pager;
#endif

using page_rendering_t = PageRendering;
using pager_t = Pager;

#endif
