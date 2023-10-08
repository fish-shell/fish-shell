// Prototypes for executing builtin_read function.
#ifndef FISH_BUILTIN_READ_H
#define FISH_BUILTIN_READ_H

#include "../maybe.h"

struct Parser;
struct IoStreams;
using parser_t = Parser;
using io_streams_t = IoStreams;

int builtin_read(const void *parser, void *streams, void *argv);
#endif
