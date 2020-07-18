// Prototypes for executing builtin_fg function.
#ifndef FISH_BUILTIN_FG_H
#define FISH_BUILTIN_FG_H

#include "maybe.h"

class parser_t;
struct io_streams_t;

maybe_t<int> builtin_fg(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
