// Prototypes for executing builtin_status function.
#ifndef FISH_BUILTIN_STATUS_H
#define FISH_BUILTIN_STATUS_H

class parser_t;
struct io_streams_t;

int builtin_status(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
