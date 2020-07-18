// Prototypes for executing builtin_source function.
#ifndef FISH_BUILTIN_SOURCE_H
#define FISH_BUILTIN_SOURCE_H

#include "maybe.h"

class parser_t;
struct io_streams_t;

maybe_t<int> builtin_source(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
