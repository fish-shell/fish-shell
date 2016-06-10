// Prototypes for functions for executing builtin_complete functions.
#ifndef FISH_BUILTIN_COMPLETE_H
#define FISH_BUILTIN_COMPLETE_H

#include <wchar.h>
#include <cstring>

class parser_t;

int builtin_complete(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
