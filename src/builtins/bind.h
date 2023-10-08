// Prototypes for executing builtin_bind function.
#ifndef FISH_BUILTIN_BIND_H
#define FISH_BUILTIN_BIND_H

#include "../maybe.h"

struct Parser;
struct IoStreams;
using parser_t = Parser;
using io_streams_t = IoStreams;

int builtin_bind(const void *parser, void *streams, void *argv);

#endif
