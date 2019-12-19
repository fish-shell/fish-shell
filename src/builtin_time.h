// Prototypes for executing builtin_time function.
#ifndef FISH_BUILTIN_TIME_H
#define FISH_BUILTIN_TIME_H

class parser_t;
struct io_streams_t;

int builtin_time(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
