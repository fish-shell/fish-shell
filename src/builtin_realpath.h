// Prototypes for executing builtin_realpath function.
#ifndef FISH_BUILTIN_REALPATH_H
#define FISH_BUILTIN_REALPATH_H

class parser_t;
struct io_streams_t;

int builtin_realpath(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
