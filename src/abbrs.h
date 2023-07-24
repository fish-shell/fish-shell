// Support for abbreviations.
//
#ifndef FISH_ABBRS_H
#define FISH_ABBRS_H

#include <unordered_map>
#include <unordered_set>

#include "common.h"
#include "maybe.h"
#include "parse_constants.h"

#if INCLUDE_RUST_HEADERS

#include "abbrs.rs.h"

#else
// Hacks to allow us to compile without Rust headers.
struct abbrs_replacer_t;

struct abbrs_replacement_t;

struct abbreviation_t;
#endif

#endif
