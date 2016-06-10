// Prototypes for functions for executing builtin_test functions.
#ifndef FISH_BUILTIN_TEST_H
#define FISH_BUILTIN_TEST_H

class parser_t;
struct io_streams_t;

int builtin_test(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
