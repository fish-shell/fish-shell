#include "config.h"  // IWYU pragma: keep

#include "ast.h"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdlib>
#include <string>

#include "common.h"
#include "enum_map.h"
#include "flog.h"
#include "parse_constants.h"
#include "parse_tree.h"
#include "tokenizer.h"
#include "wutil.h"  // IWYU pragma: keep

rust::Box<Ast> ast_parse(const wcstring &src, parse_tree_flags_t flags,
                         parse_error_list_t *out_errors) {
    return ast_parse_ffi(src, flags, out_errors);
}
rust::Box<Ast> ast_parse_argument_list(const wcstring &src, parse_tree_flags_t flags,
                                       parse_error_list_t *out_errors) {
    return ast_parse_argument_list_ffi(src, flags, out_errors);
}
