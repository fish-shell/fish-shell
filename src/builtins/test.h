// SPDX-FileCopyrightText: © 2016 fish-shell contributors
//
// SPDX-License-Identifier: GPL-2.0-only

// Prototypes for functions for executing builtin_test functions.
#ifndef FISH_BUILTIN_TEST_H
#define FISH_BUILTIN_TEST_H

#include "../maybe.h"

class parser_t;
struct io_streams_t;

maybe_t<int> builtin_test(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
#endif
