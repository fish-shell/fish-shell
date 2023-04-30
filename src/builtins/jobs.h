// Prototypes for functions for executing builtin_jobs functions.
#ifndef FISH_BUILTIN_JOBS_H
#define FISH_BUILTIN_JOBS_H

#include <cstring>
#include <cwchar>

#include "../io.h"
#include "../maybe.h"
#include "../parser.h"

maybe_t<int> builtin_jobs(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
int builtin_jobs_ffi(void *parser, void *streams, const void *argv);
#endif
