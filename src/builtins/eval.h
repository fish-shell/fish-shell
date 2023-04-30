// Prototypes for executing builtin_eval function.
#ifndef FISH_BUILTIN_EVAL_H
#define FISH_BUILTIN_EVAL_H

#include "../maybe.h"
#include "../parser.h"

struct io_streams_t;

maybe_t<int> builtin_eval(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
int builtin_eval_ffi(void *parser, void *streams, const void *argv);
#endif
