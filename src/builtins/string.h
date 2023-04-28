// Prototypes for functions for executing builtin_string functions.
#ifndef FISH_BUILTIN_STRING_H
#define FISH_BUILTIN_STRING_H

#include <cstring>
#include <cwchar>

#include "../io.h"
#include "../maybe.h"
#include "../parser.h"

maybe_t<int> builtin_string(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
int builtin_string_ffi(void *parser, void *streams, const void *argv);
#endif
