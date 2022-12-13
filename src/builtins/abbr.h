// Prototypes for executing builtin_abbr function.
#ifndef FISH_BUILTIN_ABBR_H
#define FISH_BUILTIN_ABBR_H

#include "../maybe.h"

class parser_t;
struct io_streams_t;

maybe_t<int> builtin_abbr(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
#endif
