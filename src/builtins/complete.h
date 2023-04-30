// Prototypes for functions for executing builtin_complete functions.
#ifndef FISH_BUILTIN_COMPLETE_H
#define FISH_BUILTIN_COMPLETE_H

#include "../maybe.h"
#include "../parser.h"

struct io_streams_t;

maybe_t<int> builtin_complete(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
int builtin_complete_ffi(void *parser, void *streams, const void *argv);
#endif
