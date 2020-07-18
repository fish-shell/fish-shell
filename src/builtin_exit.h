// Prototypes for executing builtin_exit function.
#ifndef FISH_BUILTIN_EXIT_H
#define FISH_BUILTIN_EXIT_H

#include "maybe.h"

class parser_t;
struct io_streams_t;

maybe_t<int> builtin_exit(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
