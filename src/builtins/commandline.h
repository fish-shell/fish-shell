// Prototypes for functions for executing builtin_commandline functions.
#ifndef FISH_BUILTIN_COMMANDLINE_H
#define FISH_BUILTIN_COMMANDLINE_H

#include "../maybe.h"

struct Parser;
using parser_t = Parser;
struct IoStreams;
using io_streams_t = IoStreams;

int builtin_commandline(const void *parser, void *streams, void *argv);
#endif
