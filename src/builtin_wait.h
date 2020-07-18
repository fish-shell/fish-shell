// Prototypes for executing builtin_wait function.
#ifndef FISH_BUILTIN_WAIT_H
#define FISH_BUILTIN_WAIT_H

#include "maybe.h"

class parser_t;
struct io_streams_t;

maybe_t<int> builtin_wait(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
