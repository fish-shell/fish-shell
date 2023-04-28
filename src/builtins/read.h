// Prototypes for executing builtin_read function.
#ifndef FISH_BUILTIN_READ_H
#define FISH_BUILTIN_READ_H

#include "../maybe.h"
#include "../parser.h"

struct io_streams_t;

maybe_t<int> builtin_read(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
int builtin_read_ffi(void *parser, void *streams, const void *argv);
#endif
