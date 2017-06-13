// Prototypes for executing builtin_function function.
#ifndef FISH_BUILTIN_FUNCTION_H
#define FISH_BUILTIN_FUNCTION_H

class parser_t;
struct io_streams_t;

int builtin_function(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
