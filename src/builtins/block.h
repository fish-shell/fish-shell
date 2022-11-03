// SPDX-FileCopyrightText: © 2017 fish-shell contributors
//
// SPDX-License-Identifier: GPL-2.0-only

// Prototypes for executing builtin_block function.
#ifndef FISH_BUILTIN_BLOCK_H
#define FISH_BUILTIN_BLOCK_H

#include "../maybe.h"

class parser_t;
struct io_streams_t;

maybe_t<int> builtin_block(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
#endif
