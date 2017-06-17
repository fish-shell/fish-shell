// Prototypes for executing builtin_functions function.
#ifndef FISH_BUILTIN_FUNCTIONS_H
#define FISH_BUILTIN_FUNCTIONS_H

class parser_t;
struct io_streams_t;

int builtin_functions(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
