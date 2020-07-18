// Prototypes for executing builtin_builtin function.
#ifndef FISH_BUILTIN_BUILTIN_H
#define FISH_BUILTIN_BUILTIN_H

#include "maybe.h"

class parser_t;
struct io_streams_t;

maybe_t<int> builtin_builtin(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
