#ifndef FISH_BUILTIN_PATH_H
#define FISH_BUILTIN_PATH_H

#include <cstring>
#include <cwchar>

#include "../maybe.h"
#include "../io.h"

class parser_t;

maybe_t<int> builtin_path(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
#endif
