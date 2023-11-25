#ifndef FISH_SCREEN_H
#define FISH_SCREEN_H
#include "config.h"  // IWYU pragma: keep

class pager_t;
class page_rendering_t;

#if INCLUDE_RUST_HEADERS
#include "screen.rs.h"
#else
struct Line;
struct ScreenData;
struct Screen;
struct PromptLayout;
struct LayoutCache;
#endif

using line_t = Line;
using screen_data_t = ScreenData;
using screen_t = Screen;
using prompt_layout_t = PromptLayout;
using layout_cache_t = LayoutCache;

#endif
