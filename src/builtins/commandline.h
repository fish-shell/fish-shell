// Prototypes for functions for executing builtin_commandline functions.
#ifndef FISH_BUILTIN_COMMANDLINE_H
#define FISH_BUILTIN_COMMANDLINE_H

#include "../maybe.h"
#include "../parser.h"

struct io_streams_t;

maybe_t<int> builtin_commandline(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
int builtin_commandline_ffi(void *parser, void *streams, const void *argv);
#endif
