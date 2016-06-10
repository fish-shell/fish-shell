// Prototypes for functions for executing builtin_commandline functions.
#ifndef FISH_BUILTIN_COMMANDLINE_H
#define FISH_BUILTIN_COMMANDLINE_H

#include <wchar.h>
#include <cstring>

class parser_t;

int builtin_commandline(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
