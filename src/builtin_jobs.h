// Prototypes for functions for executing builtin_jobs functions.
#ifndef FISH_BUILTIN_JOBS_H
#define FISH_BUILTIN_JOBS_H

#include <wchar.h>
#include <cstring>

class parser_t;

int builtin_jobs(parser_t &parser, io_streams_t &streams, wchar_t **argv);
#endif
