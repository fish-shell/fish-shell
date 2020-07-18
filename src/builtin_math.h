// Prototypes for executing builtin_math function.
#ifndef FISH_BUILTIN_MATH_H
#define FISH_BUILTIN_MATH_H

#include "maybe.h"

class parser_t;
struct io_streams_t;

maybe_t<int> builtin_math(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
