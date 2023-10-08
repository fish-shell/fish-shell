// Generic output functions.
//
// Constants for various character classifications. Each character of a command string can be
// classified as one of the following types.
#ifndef FISH_OUTPUT_H
#define FISH_OUTPUT_H

#include <stdint.h>

#include "color.h"

#if INCLUDE_RUST_HEADERS
#include "output.rs.h"
#else
// Hacks to allow us to compile without Rust headers.
struct outputter_t;
#endif

#include "env.h"

rgb_color_t parse_color(const env_var_t &var, bool is_background);

/// Sets what colors are supported.
enum { color_support_term256 = 1 << 0, color_support_term24bit = 1 << 1 };
using color_support_t = uint8_t;
extern "C" {
color_support_t output_get_color_support();
void output_set_color_support(color_support_t val);
}

rgb_color_t best_color(const std::vector<rgb_color_t> &candidates, color_support_t support);

// Temporary to support builtin set_color.
void writembs_nofail(outputter_t &outp, const char *str);
void writembs(outputter_t &outp, const char *str);

#endif
