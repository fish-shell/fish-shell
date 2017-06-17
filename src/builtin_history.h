// Prototypes for executing builtin_history function.
#ifndef FISH_BUILTIN_HISTORY_H
#define FISH_BUILTIN_HISTORY_H

class parser_t;
struct io_streams_t;

int builtin_history(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
