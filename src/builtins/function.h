// SPDX-FileCopyrightText: © 2017 fish-shell contributors
//
// SPDX-License-Identifier: GPL-2.0-only

// Prototypes for executing builtin_function function.
#ifndef FISH_BUILTIN_FUNCTION_H
#define FISH_BUILTIN_FUNCTION_H

#include "../common.h"
#include "../parse_tree.h"

class parser_t;
struct io_streams_t;

namespace ast {
struct block_statement_t;
}

int builtin_function(parser_t &parser, io_streams_t &streams,
                              const wcstring_list_t &c_args, const parsed_source_ref_t &source,
                              const ast::block_statement_t &func_node);
#endif
