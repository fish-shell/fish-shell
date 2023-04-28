// Prototypes for executing builtin_getopt function.
#ifndef FISH_BUILTIN_ARGPARSE_H
#define FISH_BUILTIN_ARGPARSE_H

#include "../maybe.h"
#include "../parser.h"

struct io_streams_t;

maybe_t<int> builtin_argparse(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
int builtin_argparse_ffi(void *parser, void *streams, const void *argv);

#endif
