// Programmatic representation of ghoti code.
#include "config.h"  // IWYU pragma: keep

#include "parse_tree.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "ast.h"
#include "common.h"
#include "enum_map.h"
#include "fallback.h"
#include "maybe.h"
#include "parse_constants.h"
#include "tokenizer.h"
#include "wutil.h"  // IWYU pragma: keep

parse_error_code_t parse_error_from_tokenizer_error(tokenizer_error_t err) {
    switch (err) {
        case tokenizer_error_t::none:
            return parse_error_code_t::none;
        case tokenizer_error_t::unterminated_quote:
            return parse_error_code_t::tokenizer_unterminated_quote;
        case tokenizer_error_t::unterminated_subshell:
            return parse_error_code_t::tokenizer_unterminated_subshell;
        case tokenizer_error_t::unterminated_slice:
            return parse_error_code_t::tokenizer_unterminated_slice;
        case tokenizer_error_t::unterminated_escape:
            return parse_error_code_t::tokenizer_unterminated_escape;
        default:
            return parse_error_code_t::tokenizer_other;
    }
}

/// Returns a string description of the given parse token.
wcstring parse_token_t::describe() const {
    wcstring result = token_type_description(type);
    if (keyword != parse_keyword_t::none) {
        append_format(result, L" <%ls>", keyword_description(keyword));
    }
    return result;
}

/// A string description appropriate for presentation to the user.
wcstring parse_token_t::user_presentable_description() const {
    return *token_type_user_presentable_description(type, keyword);
}

parsed_source_t::parsed_source_t(wcstring &&s, ast::ast_t &&ast)
    : src(std::move(s)), ast(std::move(ast)) {}

parsed_source_t::~parsed_source_t() = default;

parsed_source_ref_t parse_source(wcstring &&src, parse_tree_flags_t flags,
                                 parse_error_list_t *errors) {
    using namespace ast;
    ast_t ast = ast_t::parse(src, flags, errors);
    if (ast.errored() && !(flags & parse_flag_continue_after_error)) {
        return nullptr;
    }
    return std::make_shared<parsed_source_t>(std::move(src), std::move(ast));
}
