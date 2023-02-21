// Programmatic representation of fish code.
#ifndef FISH_PARSE_PRODUCTIONS_H
#define FISH_PARSE_PRODUCTIONS_H

#include <memory>

#include "ast.h"
#include "common.h"
#include "parse_constants.h"
#include "tokenizer.h"

/// A struct representing the token type that we use internally.
struct parse_token_t {
    parse_token_type_t type;  // The type of the token as represented by the parser
    parse_keyword_t keyword{parse_keyword_t::none};  // Any keyword represented by this token
    bool has_dash_prefix{false};   // Hackish: whether the source contains a dash prefix
    bool is_help_argument{false};  // Hackish: whether the source looks like '-h' or '--help'
    bool is_newline{false};        // Hackish: if TOK_END, whether the source is a newline.
    bool may_be_variable_assignment{false};  // Hackish: whether this token is a string like FOO=bar
    tokenizer_error_t tok_error{
        tokenizer_error_t::none};  // If this is a tokenizer error, that error.
    source_offset_t source_start{SOURCE_OFFSET_INVALID};
    source_offset_t source_length{0};

    /// \return the source range.
    /// Note the start may be invalid.
    source_range_t range() const { return source_range_t{source_start, source_length}; }

    /// \return whether we are a string with the dash prefix set.
    bool is_dash_prefix_string() const {
        return type == parse_token_type_t::string && has_dash_prefix;
    }

    wcstring describe() const;
    wcstring user_presentable_description() const;

    constexpr parse_token_t(parse_token_type_t type) : type(type) {}
};

parse_error_code_t parse_error_from_tokenizer_error(tokenizer_error_t err);

/// A type wrapping up a parse tree and the original source behind it.
struct parsed_source_t : noncopyable_t, nonmovable_t {
    wcstring src;
    ast::ast_t ast;

    parsed_source_t(wcstring &&s, ast::ast_t &&ast);
    ~parsed_source_t();
};

/// Return a shared pointer to parsed_source_t, or null on failure.
/// If parse_flag_continue_after_error is not set, this will return null on any error.
using parsed_source_ref_t = std::shared_ptr<const parsed_source_t>;
parsed_source_ref_t parse_source(wcstring &&src, parse_tree_flags_t flags,
                                 parse_error_list_t *errors);

/// Error message when a command may not be in a pipeline.
#define INVALID_PIPELINE_CMD_ERR_MSG _(L"The '%ls' command can not be used in a pipeline")

#endif
