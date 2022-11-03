// SPDX-FileCopyrightText: © 2016 fish-shell contributors
//
// SPDX-License-Identifier: GPL-2.0-only

// Prototypes for functions for executing builtin_jobs functions.
#ifndef FISH_BUILTIN_JOBS_H
#define FISH_BUILTIN_JOBS_H

#include <cstring>
#include <cwchar>

#include "../io.h"
#include "../maybe.h"

class parser_t;

maybe_t<int> builtin_jobs(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
#endif
