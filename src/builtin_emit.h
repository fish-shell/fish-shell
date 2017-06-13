// Prototypes for executing builtin_emit function.
#ifndef FISH_BUILTIN_EMIT_H
#define FISH_BUILTIN_EMIT_H

class parser_t;
struct io_streams_t;

int builtin_emit(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
