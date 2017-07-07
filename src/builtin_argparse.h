// Prototypes for executing builtin_getopt function.
#ifndef FISH_BUILTIN_ARGPARSE_H
#define FISH_BUILTIN_ARGPARSE_H

class parser_t;
struct io_streams_t;

int builtin_argparse(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
