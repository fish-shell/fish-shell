// Prototypes for executing builtin_function function.
#ifndef FISH_BUILTIN_FUNCTION_H
#define FISH_BUILTIN_FUNCTION_H

#include "common.h"

class parser_t;
struct io_streams_t;

int builtin_function(parser_t &parser, io_streams_t &streams, const wcstring_list_t &c_args,
                     const wcstring &contents, int definition_line_offset);
#endif
