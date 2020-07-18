// Prototypes for executing builtin_eval function.
#ifndef FISH_BUILTIN_EVAL_H
#define FISH_BUILTIN_EVAL_H

#include "maybe.h"

class parser_t;
struct io_streams_t;

maybe_t<int> builtin_eval(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
