// Prototypes for executing builtin_read function.
#ifndef FISH_BUILTIN_READ_H
#define FISH_BUILTIN_READ_H

#include "../maybe.h"

class Parser; using parser_t = Parser;
class IoStreams; using io_streams_t = IoStreams;

maybe_t<int> builtin_read(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
#endif
