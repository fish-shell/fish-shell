// Prototypes for executing builtin_disown function.
#ifndef FISH_BUILTIN_DISOWN_H
#define FISH_BUILTIN_DISOWN_H

#include "../maybe.h"
#include "../parser.h"

struct io_streams_t;

maybe_t<int> builtin_disown(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
int builtin_disown_ffi(void *parser, void *streams, const void *argv);
#endif
