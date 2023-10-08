// Prototypes for functions for executing builtin_ulimit functions.
#ifndef FISH_BUILTIN_ULIMIT_H
#define FISH_BUILTIN_ULIMIT_H

#include "../maybe.h"

struct Parser;
struct IoStreams;
using parser_t = Parser;
using io_streams_t = IoStreams;

int builtin_ulimit(const void *parser, void *streams, void *argv);
#endif
