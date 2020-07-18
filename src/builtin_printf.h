// Prototypes for functions for executing builtin_printf functions.
#ifndef FISH_BUILTIN_PRINTF_H
#define FISH_BUILTIN_PRINTF_H

#include <cstring>
#include <cwchar>

class parser_t;

maybe_t<int> builtin_printf(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
