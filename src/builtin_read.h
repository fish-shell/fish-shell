// Prototypes for executing builtin_read function.
#ifndef FISH_BUILTIN_READ_H
#define FISH_BUILTIN_READ_H

class parser_t;
struct io_streams_t;

int builtin_read(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
