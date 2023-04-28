// Prototypes for functions for executing builtin_ulimit functions.
#ifndef FISH_BUILTIN_ULIMIT_H
#define FISH_BUILTIN_ULIMIT_H

#include "../maybe.h"
#include "../parser.h"

struct io_streams_t;

maybe_t<int> builtin_ulimit(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
int builtin_ulimit_ffi(void *parser, void *streams, const void *argv);
#endif
