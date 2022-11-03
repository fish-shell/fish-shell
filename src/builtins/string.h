// SPDX-FileCopyrightText: © 2016 fish-shell contributors
//
// SPDX-License-Identifier: GPL-2.0-only

// Prototypes for functions for executing builtin_string functions.
#ifndef FISH_BUILTIN_STRING_H
#define FISH_BUILTIN_STRING_H

#include <cstring>
#include <cwchar>

#include "../io.h"
#include "../maybe.h"

class parser_t;

maybe_t<int> builtin_string(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
#endif
