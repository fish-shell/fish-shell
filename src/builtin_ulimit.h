// Prototypes for functions for executing builtin_ulimit functions.
#ifndef FISH_BUILTIN_ULIMIT_H
#define FISH_BUILTIN_ULIMIT_H

#include <wchar.h>
#include <cstring>

class parser_t;

int builtin_ulimit(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
