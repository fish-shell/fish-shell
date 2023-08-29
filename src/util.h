#ifndef FISH_UTIL_H
#define FISH_UTIL_H

#if INCLUDE_RUST_HEADERS

#include "util.rs.h"

#else

// Hacks to allow us to compile without Rust headers.
int wcsfilecmp(const wchar_t *a, const wchar_t *b);
int wcsfilecmp_glob(const wchar_t *a, const wchar_t *b);
long long get_time();

#endif

#endif
