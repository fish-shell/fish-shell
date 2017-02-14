// Generic output functions.
//
// Constants for various character classifications. Each character of a command string can be
// classified as one of the following types.
#ifndef FISH_OUTPUT_H
#define FISH_OUTPUT_H

#include <stddef.h>

#include <vector>

#include "color.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep

/// Constants for various colors as used by the set_color function.
enum {
    FISH_COLOR_BLACK,    // 0
    FISH_COLOR_RED,      // 1
    FISH_COLOR_GREEN,    // 2
    FISH_COLOR_YELLOW,   // 3
    FISH_COLOR_BLUE,     // 4
    FISH_COLOR_MAGENTA,  // 5
    FISH_COLOR_CYAN,     // 6
    FISH_COLOR_WHITE,    // 7
    FISH_COLOR_NORMAL,   // 8 terminal default
    FISH_COLOR_RESET     // 9
};

void set_color(rgb_color_t c, rgb_color_t c2);

void writembs_check(char *mbs, const char *mbs_name, const char *file, long line);
#define writembs(mbs) writembs_check((mbs), #mbs, __FILE__, __LINE__)

int writech(wint_t ch);

void writestr(const wchar_t *str);

rgb_color_t parse_color(const wcstring &val, bool is_background);

int writeb(tputs_arg_t b);

void output_set_writer(int (*writer)(char));

int (*output_get_writer())(char);

/// Sets what colors are supported.
enum { color_support_term256 = 1 << 0, color_support_term24bit = 1 << 1 };
typedef unsigned int color_support_t;
color_support_t output_get_color_support();
void output_set_color_support(color_support_t support);

rgb_color_t best_color(const std::vector<rgb_color_t> &colors, color_support_t support);

bool write_color(rgb_color_t color, bool is_fg);
unsigned char index_for_color(rgb_color_t c);

#endif
