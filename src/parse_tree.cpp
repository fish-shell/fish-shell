// Programmatic representation of fish code.
#include "config.h"  // IWYU pragma: keep

#include "parse_tree.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include <algorithm>
#include <cwchar>
#include <string>
#include <type_traits>
#include <vector>

#include "ast.h"
#include "common.h"
#include "fallback.h"
#include "flog.h"
#include "parse_constants.h"
#include "parse_tree.h"
#include "proc.h"
#include "tokenizer.h"
#include "wutil.h"  // IWYU pragma: keep

parse_error_code_t parse_error_from_tokenizer_error(tokenizer_error_t err) {
    switch (err) {
        case tokenizer_error_t::none:
            return parse_error_none;
        case tokenizer_error_t::unterminated_quote:
            return parse_error_tokenizer_unterminated_quote;
        case tokenizer_error_t::unterminated_subshell:
            return parse_error_tokenizer_unterminated_subshell;
        case tokenizer_error_t::unterminated_slice:
            return parse_error_tokenizer_unterminated_slice;
        case tokenizer_error_t::unterminated_escape:
            return parse_error_tokenizer_unterminated_escape;
        default:
            return parse_error_tokenizer_other;
    }
}

/// Returns a string description of this parse error.
wcstring parse_error_t::describe_with_prefix(const wcstring &src, const wcstring &prefix,
                                             bool is_interactive, bool skip_caret) const {
    wcstring result = prefix;
    switch (code) {
        default:
            if (skip_caret && this->text.empty()) return L"";
            break;
        case parse_error_andor_in_pipeline:
            append_format(result, EXEC_ERR_MSG,
                          src.substr(this->source_start, this->source_length).c_str());
            return result;
        case parse_error_bare_variable_assignment: {
            wcstring assignment_src = src.substr(this->source_start, this->source_length);
            maybe_t<size_t> equals_pos = variable_assignment_equals_pos(assignment_src);
            assert(equals_pos);
            wcstring variable = assignment_src.substr(0, *equals_pos);
            wcstring value = assignment_src.substr(*equals_pos + 1);
            append_format(result, ERROR_BAD_COMMAND_ASSIGN_ERR_MSG, variable.c_str(),
                          value.c_str());
            return result;
        }
    }
    result.append(this->text);
    if (skip_caret || source_start >= src.size() || source_start + source_length > src.size()) {
        return result;
    }

    // Locate the beginning of this line of source.
    size_t line_start = 0;

    // Look for a newline prior to source_start. If we don't find one, start at the beginning of
    // the string; otherwise start one past the newline. Note that source_start may itself point
    // at a newline; we want to find the newline before it.
    if (source_start > 0) {
        size_t newline = src.find_last_of(L'\n', source_start - 1);
        if (newline != wcstring::npos) {
            line_start = newline + 1;
        }
    }

    // Look for the newline after the source range. If the source range itself includes a
    // newline, that's the one we want, so start just before the end of the range.
    size_t last_char_in_range =
        (source_length == 0 ? source_start : source_start + source_length - 1);
    size_t line_end = src.find(L'\n', last_char_in_range);
    if (line_end == wcstring::npos) {
        line_end = src.size();
    }

    assert(line_end >= line_start);
    assert(source_start >= line_start);

    // Don't include the caret and line if we're interactive and this is the first line, because
    // then it's obvious.
    bool interactive_skip_caret = is_interactive && source_start == 0;
    if (interactive_skip_caret) {
        return result;
    }

    // Append the line of text.
    if (!result.empty()) result.push_back(L'\n');
    result.append(src, line_start, line_end - line_start);

    // Append the caret line. The input source may include tabs; for that reason we
    // construct a "caret line" that has tabs in corresponding positions.
    wcstring caret_space_line;
    caret_space_line.reserve(source_start - line_start);
    for (size_t i = line_start; i < source_start; i++) {
        wchar_t wc = src.at(i);
        if (wc == L'\t') {
            caret_space_line.push_back(L'\t');
        } else if (wc == L'\n') {
            // It's possible that the source_start points at a newline itself. In that case,
            // pretend it's a space. We only expect this to be at the end of the string.
            caret_space_line.push_back(L' ');
        } else {
            int width = fish_wcwidth(wc);
            if (width > 0) {
                caret_space_line.append(static_cast<size_t>(width), L' ');
            }
        }
    }
    result.push_back(L'\n');
    result.append(caret_space_line);
    result.push_back(L'^');
    return result;
}

wcstring parse_error_t::describe(const wcstring &src, bool is_interactive) const {
    return this->describe_with_prefix(src, wcstring(), is_interactive, false);
}

void parse_error_offset_source_start(parse_error_list_t *errors, size_t amt) {
    assert(errors != nullptr);
    if (amt > 0) {
        size_t i, max = errors->size();
        for (i = 0; i < max; i++) {
            parse_error_t *error = &errors->at(i);
            // Preserve the special meaning of -1 as 'unknown'.
            if (error->source_start != SOURCE_LOCATION_UNKNOWN) {
                error->source_start += amt;
            }
        }
    }
}

/// Returns a string description for the given token type.
const wchar_t *token_type_description(parse_token_type_t type) {
    const wchar_t *description = enum_to_str(type, token_enum_map);
    if (description) return description;
    return L"unknown_token_type";
}

const wchar_t *keyword_description(parse_keyword_t type) {
    const wchar_t *keyword = enum_to_str(type, keyword_enum_map);
    if (keyword) return keyword;
    return L"unknown_keyword";
}

wcstring token_type_user_presentable_description(parse_token_type_t type, parse_keyword_t keyword) {
    if (keyword != parse_keyword_t::none) {
        return format_string(L"keyword '%ls'", keyword_description(keyword));
    }

    switch (type) {
        case parse_token_type_t::string:
            return L"a string";
        case parse_token_type_t::pipe:
            return L"a pipe";
        case parse_token_type_t::redirection:
            return L"a redirection";
        case parse_token_type_t::background:
            return L"a '&'";
        case parse_token_type_t::andand:
            return L"'&&'";
        case parse_token_type_t::oror:
            return L"'||'";
        case parse_token_type_t::end:
            return L"end of the statement";
        case parse_token_type_t::terminate:
            return L"end of the input";
        default: {
            return format_string(L"a %ls", token_type_description(type));
        }
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
    return token_type_user_presentable_description(type, keyword);
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
