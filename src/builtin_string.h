// Prototypes for functions for executing builtin_string functions.
#ifndef FISH_BUILTIN_STRING_H
#define FISH_BUILTIN_STRING_H

#include <cstring>
#include <cwchar>

class parser_t;

maybe_t<int> builtin_string(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
