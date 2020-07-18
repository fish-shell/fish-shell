// Prototypes for executing builtin_disown function.
#ifndef FISH_BUILTIN_DISOWN_H
#define FISH_BUILTIN_DISOWN_H

#include "maybe.h"

class parser_t;
struct io_streams_t;

maybe_t<int> builtin_disown(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
