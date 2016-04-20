// Prototypes for functions for executing builtin_set functions.
#ifndef FISH_BUILTIN_SET_H
#define FISH_BUILTIN_SET_H

#include <wchar.h>
#include <cstring>

class parser_t;

int builtin_set(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
