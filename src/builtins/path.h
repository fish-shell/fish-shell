#ifndef FISH_BUILTIN_PATH_H
#define FISH_BUILTIN_PATH_H

#include "../maybe.h"
#include "../parser.h"

struct io_streams_t;

maybe_t<int> builtin_path(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
int builtin_path_ffi(void *parser, void *streams, const void *argv);
#endif
