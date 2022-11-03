// SPDX-FileCopyrightText: © 2017 fish-shell contributors
//
// SPDX-License-Identifier: GPL-2.0-only

// Prototypes for executing builtin_bind function.
#ifndef FISH_BUILTIN_BIND_H
#define FISH_BUILTIN_BIND_H

#include "../maybe.h"

class parser_t;
struct io_streams_t;
maybe_t<int> builtin_bind(parser_t &parser, io_streams_t &streams, const wchar_t **argv);

#endif
