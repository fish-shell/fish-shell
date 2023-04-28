// Prototypes for functions for executing builtin_test functions.
#ifndef FISH_BUILTIN_TEST_H
#define FISH_BUILTIN_TEST_H

#include "../maybe.h"
#include "../parser.h"

struct io_streams_t;

maybe_t<int> builtin_test(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
int builtin_test_ffi(void *parser, void *streams, const void *argv);
#endif
