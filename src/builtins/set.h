// Prototypes for functions for executing builtin_set functions.
#ifndef FISH_BUILTIN_SET_H
#define FISH_BUILTIN_SET_H

#include "../maybe.h"

class Parser; using parser_t = Parser;
class IoStreams; using io_streams_t = IoStreams;
maybe_t<int> builtin_set(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
#endif
