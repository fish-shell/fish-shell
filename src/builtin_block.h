// Prototypes for executing builtin_block function.
#ifndef FISH_BUILTIN_BLOCK_H
#define FISH_BUILTIN_BLOCK_H

#include "maybe.h"

class parser_t;
struct io_streams_t;

maybe_t<int> builtin_block(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
