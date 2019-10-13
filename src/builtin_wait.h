// Prototypes for executing builtin_wait function.
#ifndef FISH_BUILTIN_WAIT_H
#define FISH_BUILTIN_WAIT_H

class parser_t;
struct io_streams_t;

int builtin_wait(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
