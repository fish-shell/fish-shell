// Prototypes for executing builtin_bg function.
#ifndef FISH_BUILTIN_BG_H
#define FISH_BUILTIN_BG_H

class parser_t;
struct io_streams_t;

int builtin_bg(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
