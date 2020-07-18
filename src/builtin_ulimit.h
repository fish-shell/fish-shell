// Prototypes for functions for executing builtin_ulimit functions.
#ifndef FISH_BUILTIN_ULIMIT_H
#define FISH_BUILTIN_ULIMIT_H

#include <cstring>
#include <cwchar>

class parser_t;

maybe_t<int> builtin_ulimit(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
