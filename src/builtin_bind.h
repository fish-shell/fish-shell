// Prototypes for executing builtin_bind function.
#ifndef FISH_BUILTIN_BIND_H
#define FISH_BUILTIN_BIND_H

#define DEFAULT_BIND_MODE L"default"

class parser_t;
struct io_streams_t;

int builtin_bind(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
