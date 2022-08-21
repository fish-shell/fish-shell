// Prototypes for functions for executing builtin_set_color functions.
#ifndef FISH_BUILTIN_SET_COLOR_H
#define FISH_BUILTIN_SET_COLOR_H

#include "../maybe.h"

class parser_t;
struct io_streams_t;

maybe_t<int> builtin_set_color(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
#endif
