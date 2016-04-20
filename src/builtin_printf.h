// Prototypes for functions for executing builtin_printf functions.
#ifndef FISH_BUILTIN_PRINTF_H
#define FISH_BUILTIN_PRINTF_H

#include <wchar.h>
#include <cstring>

class parser_t;

int builtin_printf(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
