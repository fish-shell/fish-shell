// Prototypes for executing builtin_source function.
#ifndef FISH_BUILTIN_SOURCE_H
#define FISH_BUILTIN_SOURCE_H

#include "../maybe.h"

class Parser; using parser_t = Parser;
class IoStreams; using io_streams_t = IoStreams;

maybe_t<int> builtin_source(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
#endif
