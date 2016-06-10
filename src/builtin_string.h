// Prototypes for functions for executing builtin_string functions.
#ifndef FISH_BUILTIN_STRING_H
#define FISH_BUILTIN_STRING_H

#include <wchar.h>
#include <cstring>

class parser_t;

int builtin_string(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
