// Prototypes for executing builtin_contains function.
#ifndef FISH_BUILTIN_CONTAINS_H
#define FISH_BUILTIN_CONTAINS_H

#include "maybe.h"

class parser_t;
struct io_streams_t;

maybe_t<int> builtin_contains(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
