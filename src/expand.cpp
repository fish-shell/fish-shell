// String expansion functions. These functions perform several kinds of parameter expansion.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <pwd.h>
#include <stdarg.h>
#include <unistd.h>
#include <wctype.h>

#include <cstring>
#include <cwchar>

#ifdef SunOS
#include <procfs.h>
#endif

#include <algorithm>
#include <map>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common.h"
#include "complete.h"
#include "env.h"
#include "exec.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "history.h"
#include "operation_context.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "parser.h"
#include "path.h"
#include "threads.rs.h"
#include "util.h"
#include "wcstringutil.h"
#include "wildcard.h"
#include "wutil.h"  // IWYU pragma: keep

/// Characters which make a string unclean if they are the first character of the string. See \c
/// expand_is_clean().
#define UNCLEAN_FIRST L"~%"
/// Unclean characters. See \c expand_is_clean().
#define UNCLEAN L"$*?\\\"'({})"

static void remove_internal_separator(wcstring *s, bool conv);

/// Test if the specified argument is clean, i.e. it does not contain any tokens which need to be
/// expanded or otherwise altered. Clean strings can be passed through expand_string and expand_one
/// without changing them. About two thirds of all strings are clean, so skipping expansion on them
/// actually does save a small amount of time, since it avoids multiple memory allocations during
/// the expansion process.
///
/// \param in the string to test
static bool expand_is_clean(const wcstring &in) {
    if (in.empty()) return true;

    // Test characters that have a special meaning in the first character position.
    if (std::wcschr(UNCLEAN_FIRST, in.at(0)) != nullptr) return false;

    // Test characters that have a special meaning in any character position.
    return in.find_first_of(UNCLEAN) == wcstring::npos;
}

/// Append a syntax error to the given error list.
static void append_syntax_error(parse_error_list_t *errors, size_t source_start, const wchar_t *fmt,
                                ...) {
    if (!errors) return;

    parse_error_t error;
    error.source_start = source_start;
    error.source_length = 0;
    error.code = parse_error_code_t::syntax;

    va_list va;
    va_start(va, fmt);
    error.text = std::make_unique<wcstring>(vformat_string(fmt, va));
    va_end(va);

    errors->push_back(std::move(error));
}

/// Append a cmdsub error to the given error list. But only do so if the error hasn't already been
/// recorded. This is needed because command substitution is a recursive process and some errors
/// could consequently be recorded more than once.
static void append_cmdsub_error(parse_error_list_t *errors, size_t source_start, size_t source_end,
                                const wchar_t *fmt, ...) {
    if (!errors) return;

    parse_error_t error;
    error.source_start = source_start;
    error.source_length = source_end - source_start + 1;
    error.code = parse_error_code_t::cmdsubst;

    va_list va;
    va_start(va, fmt);
    error.text = std::make_unique<wcstring>(vformat_string(fmt, va));
    va_end(va);

    for (size_t i = 0; i < errors->size(); i++) {
        if (*error.text == *errors->at(i)->text()) return;
    }

    errors->push_back(std::move(error));
}

/// Append an overflow error, when expansion produces too much data.
static expand_result_t append_overflow_error(parse_error_list_t *errors,
                                             size_t source_start = SOURCE_LOCATION_UNKNOWN) {
    if (errors) {
        parse_error_t error;
        error.source_start = source_start;
        error.source_length = 0;
        error.code = parse_error_code_t::generic;
        error.text = std::make_unique<wcstring>(_(L"Expansion produced too many results"));
        errors->push_back(std::move(error));
    }
    return expand_result_t::make_error(STATUS_EXPAND_ERROR);
}

/// Test if the specified string does not contain character which can not be used inside a quoted
/// string.
static bool is_quotable(const wcstring &str) {
    return str.find_first_of(L"\n\t\r\b\x1B") == wcstring::npos;
}

wcstring expand_escape_variable(const env_var_t &var) {
    wcstring buff;
    const std::vector<wcstring> &lst = var.as_list();

    for (size_t j = 0; j < lst.size(); j++) {
        const wcstring &el = lst.at(j);
        if (j) buff.append(L"  ");

        // We want to use quotes if we have more than one string, or the string contains a space.
        bool prefer_quotes = lst.size() > 1 || el.find(L' ') != wcstring::npos;
        if (prefer_quotes && is_quotable(el)) {
            buff.append(L"'");
            buff.append(el);
            buff.append(L"'");
        } else {
            buff.append(escape_string(el));
        }
    }

    return buff;
}

wcstring expand_escape_string(const wcstring &el) {
    wcstring buff;
    bool prefer_quotes = el.find(L' ') != wcstring::npos;
    if (prefer_quotes && is_quotable(el)) {
        buff.append(L"'");
        buff.append(el);
        buff.append(L"'");
    } else {
        buff.append(escape_string(el));
    }
    return buff;
}

enum class parse_slice_error_t {
    none,
    zero_index,
    invalid_index,
};

/// Parse an array slicing specification Returns 0 on success. If a parse error occurs, returns the
/// index of the bad token. Note that 0 can never be a bad index because the string always starts
/// with [.
static size_t parse_slice(const wchar_t *const in, wchar_t **const end_ptr, std::vector<long> &idx,
                          size_t array_size, parse_slice_error_t *const error) {
    const long size = static_cast<long>(array_size);
    size_t pos = 1;  // skip past the opening square bracket

    *error = parse_slice_error_t::none;

    while (true) {
        while (iswspace(in[pos]) || (in[pos] == INTERNAL_SEPARATOR)) pos++;
        if (in[pos] == L']') {
            pos++;
            break;
        }

        const wchar_t *end;
        long tmp;
        if (idx.empty() && in[pos] == L'.' && in[pos + 1] == L'.') {
            // If we are at the first index expression, a missing start-index means the range starts
            // at the first item.
            tmp = 1;  // first index
            end = &in[pos];
        } else {
            tmp = fish_wcstol(&in[pos], &end);
            if (errno > 0) {
                // We don't test `*end` as is typically done because we expect it to not be the null
                // char. Ignore the case of errno==-1 because it means the end char wasn't the null
                // char.
                *error = parse_slice_error_t::invalid_index;
                return pos;
            } else if (tmp == 0) {
                // Explicitly refuse $foo[0] as valid syntax, regardless of whether or not we're
                // going to show an error if the index ultimately evaluates to zero. This will help
                // newcomers to fish avoid a common off-by-one error. See #4862.
                *error = parse_slice_error_t::zero_index;
                return pos;
            }
        }

        long i1 = tmp > -1 ? tmp : size + tmp + 1;
        pos = end - in;
        while (in[pos] == INTERNAL_SEPARATOR) pos++;
        if (in[pos] == L'.' && in[pos + 1] == L'.') {
            pos += 2;
            while (in[pos] == INTERNAL_SEPARATOR) pos++;
            while (iswspace(in[pos])) pos++;  // Allow the space in "[.. ]".

            long tmp1;
            // If we are at the last index range expression  then a missing end-index means the
            // range spans until the last item.
            if (in[pos] == L']') {
                tmp1 = -1;  // last index
                end = &in[pos];
            } else {
                tmp1 = fish_wcstol(&in[pos], &end);
                // Ignore the case of errno==-1 because it means the end char wasn't the null char.
                if (errno > 0) {
                    *error = parse_slice_error_t::invalid_index;
                    return pos;
                } else if (tmp1 == 0) {
                    *error = parse_slice_error_t::zero_index;
                    return pos;
                }
            }
            pos = end - in;

            long i2 = tmp1 > -1 ? tmp1 : size + tmp1 + 1;
            // Skip sequences that are entirely outside.
            // This means "17..18" expands to nothing if there are less than 17 elements.
            if (i1 > size && i2 > size) {
                continue;
            }
            short direction = i2 < i1 ? -1 : 1;
            // If only the beginning is negative, always go reverse.
            // If only the end, always go forward.
            // Prevents `[x..-1]` from going reverse if less than x elements are there.
            if ((tmp1 > -1) != (tmp > -1)) {
                direction = tmp1 > -1 ? -1 : 1;
            } else {
                // Clamp to array size when not forcing direction
                // - otherwise "2..-1" clamps both to 1 and then becomes "1..1".
                i1 = i1 < size ? i1 : size;
                i2 = i2 < size ? i2 : size;
            }
            for (long jjj = i1; jjj * direction <= i2 * direction; jjj += direction) {
                // FLOGF(error, L"Expand range [subst]: %i\n", jjj);
                idx.push_back(jjj);
            }
            continue;
        }

        idx.push_back(i1);
    }

    if (end_ptr) {
        *end_ptr = const_cast<wchar_t *>(in + pos);
    }

    return 0;
}

/// Expand all environment variables in the string *ptr.
///
/// This function is slow, fragile and complicated. There are lots of little corner cases, like
/// $$foo should do a double expansion, $foo$bar should not double expand bar, etc.
///
/// This function operates on strings backwards, starting at last_idx.
///
/// Note: last_idx is considered to be where it previously finished processing. This means it
/// actually starts operating on last_idx-1. As such, to process a string fully, pass string.size()
/// as last_idx instead of string.size()-1.
///
/// \return the result of expansion.
static expand_result_t expand_variables(wcstring instr, completion_receiver_t *out, size_t last_idx,
                                        const environment_t &vars, parse_error_list_t *errors) {
    const size_t insize = instr.size();

    // last_idx may be 1 past the end of the string, but no further.
    assert(last_idx <= insize && "Invalid last_idx");
    if (last_idx == 0) {
        if (!out->add(std::move(instr))) {
            return append_overflow_error(errors);
        }
        return expand_result_t::ok;
    }

    // Locate the last VARIABLE_EXPAND or VARIABLE_EXPAND_SINGLE
    bool is_single = false;
    size_t varexp_char_idx = last_idx;
    while (varexp_char_idx--) {
        const wchar_t c = instr.at(varexp_char_idx);
        if (c == VARIABLE_EXPAND || c == VARIABLE_EXPAND_SINGLE) {
            is_single = (c == VARIABLE_EXPAND_SINGLE);
            break;
        }
    }
    if (varexp_char_idx >= instr.size()) {
        // No variable expand char, we're done.
        if (!out->add(std::move(instr))) {
            return append_overflow_error(errors);
        }
        return expand_result_t::ok;
    }

    // Get the variable name.
    const size_t var_name_start = varexp_char_idx + 1;
    size_t var_name_stop = var_name_start;
    while (var_name_stop < insize) {
        const wchar_t nc = instr.at(var_name_stop);
        if (nc == VARIABLE_EXPAND_EMPTY) {
            var_name_stop++;
            break;
        }
        if (!valid_var_name_char(nc)) break;
        var_name_stop++;
    }
    assert(var_name_stop >= var_name_start && "Bogus variable name indexes");
    const size_t var_name_len = var_name_stop - var_name_start;

    // It's an error if the name is empty.
    if (var_name_len == 0) {
        if (errors) {
            parse_util_expand_variable_error(instr, 0 /* global_token_pos */, varexp_char_idx,
                                             errors);
        }
        return expand_result_t::make_error(STATUS_EXPAND_ERROR);
    }

    // Get the variable name as a string, then try to get the variable from env.
    const wcstring var_name(instr, var_name_start, var_name_len);
    // Do a dirty hack to make sliced history fast (#4650). We expand from either a variable, or a
    // history_t. Note that "history" is read only in env.cpp so it's safe to special-case it in
    // this way (it cannot be shadowed, etc).
    maybe_t<rust::Box<HistorySharedPtr>> history;
    maybe_t<env_var_t> var{};
    if (var_name == L"history") {
        if (is_main_thread()) {
            history = history_with_name(history_session_id(vars));
        }
    } else if (var_name != wcstring{VARIABLE_EXPAND_EMPTY}) {
        var = vars.get(var_name);
    }

    // Parse out any following slice.
    // Record the end of the variable name and any following slice.
    size_t var_name_and_slice_stop = var_name_stop;
    bool all_values = true;
    const size_t slice_start = var_name_stop;
    std::vector<long> var_idx_list;
    if (slice_start < insize && instr.at(slice_start) == L'[') {
        all_values = false;
        const wchar_t *in = instr.c_str();
        wchar_t *slice_end;
        // If a variable is missing, behave as though we have one value, so that $var[1] always
        // works.
        size_t effective_val_count = 1;
        if (var) {
            effective_val_count = var->as_list().size();
        } else if (history) {
            effective_val_count = (**history).size();
        }
        parse_slice_error_t parse_error;
        size_t bad_pos = parse_slice(in + slice_start, &slice_end, var_idx_list,
                                     effective_val_count, &parse_error);
        if (bad_pos != 0) {
            switch (parse_error) {
                case parse_slice_error_t::none:
                    assert(false && "bad_pos != 0 but parse_slice_error_t::none!");
                    break;
                case parse_slice_error_t::zero_index:
                    append_syntax_error(errors, slice_start + bad_pos,
                                        L"array indices start at 1, not 0.");
                    break;
                case parse_slice_error_t::invalid_index:
                    append_syntax_error(errors, slice_start + bad_pos, L"Invalid index value");
                    break;
            }
            return expand_result_t::make_error(STATUS_EXPAND_ERROR);
        }
        var_name_and_slice_stop = (slice_end - in);
    }

    if (!var && !history) {
        // Expanding a non-existent variable.
        if (!is_single) {
            // Normal expansions of missing variables successfully expand to nothing.
            return expand_result_t::ok;
        } else {
            // Expansion to single argument.
            // Replace the variable name and slice with VARIABLE_EXPAND_EMPTY.
            wcstring res(instr, 0, varexp_char_idx);
            if (!res.empty() && res.back() == VARIABLE_EXPAND_SINGLE) {
                res.push_back(VARIABLE_EXPAND_EMPTY);
            }
            res.append(instr, var_name_and_slice_stop, wcstring::npos);
            return expand_variables(std::move(res), out, varexp_char_idx, vars, errors);
        }
    }

    // Ok, we have a variable or a history. Let's expand it.
    // Start by respecting the sliced elements.
    assert((var || history) && "Should have variable or history here");
    std::vector<wcstring> var_item_list;
    if (all_values) {
        if (history) {
            for (const auto &item : (*history)->get_history()->vals) {
                var_item_list.push_back(item);
            }
        } else {
            var->to_list(var_item_list);
        }
    } else {
        // We have to respect the slice.
        if (history) {
            // Ask history to map indexes to item strings.
            // Note this may have missing entries for out-of-bounds.
            auto item_map = (**history).items_at_indexes(
                rust::Slice<const long>(var_idx_list.data(), var_idx_list.size()));
            for (long item_index : var_idx_list) {
                auto item = item_map->get(item_index);
                if (item) {
                    var_item_list.push_back(*item);
                }
            }
        } else {
            const std::vector<wcstring> &all_var_items = var->as_list();
            for (long item_index : var_idx_list) {
                // Check that we are within array bounds. If not, skip the element. Note:
                // Negative indices (`echo $foo[-1]`) are already converted to positive ones
                // here, So tmp < 1 means it's definitely not in.
                // Note we are 1-based.
                if (item_index >= 1 && size_t(item_index) <= all_var_items.size()) {
                    var_item_list.push_back(all_var_items.at(item_index - 1));
                }
            }
        }
    }

    if (is_single) {
        // Quoted expansion. Here we expect the variable's delimiter.
        // Note history always has a space delimiter.
        wchar_t delimit = history ? L' ' : var->get_delimiter();
        wcstring res(instr, 0, varexp_char_idx);
        if (!res.empty()) {
            if (res.back() != VARIABLE_EXPAND_SINGLE) {
                res.push_back(INTERNAL_SEPARATOR);
            } else if (var_item_list.empty() || var_item_list.front().empty()) {
                // First expansion is empty, but we need to recursively expand.
                res.push_back(VARIABLE_EXPAND_EMPTY);
            }
        }

        // Append all entries in var_item_list, separated by the delimiter.
        res.append(join_strings(var_item_list, delimit));
        res.append(instr, var_name_and_slice_stop, wcstring::npos);
        return expand_variables(std::move(res), out, varexp_char_idx, vars, errors);
    } else {
        // Normal cartesian-product expansion.
        for (wcstring &item : var_item_list) {
            if (varexp_char_idx == 0 && var_name_and_slice_stop == insize) {
                if (!out->add(std::move(item))) {
                    return append_overflow_error(errors);
                }
            } else {
                wcstring new_in(instr, 0, varexp_char_idx);
                if (!new_in.empty()) {
                    if (new_in.back() != VARIABLE_EXPAND) {
                        new_in.push_back(INTERNAL_SEPARATOR);
                    } else if (item.empty()) {
                        new_in.push_back(VARIABLE_EXPAND_EMPTY);
                    }
                }
                new_in.append(item);
                new_in.append(instr, var_name_and_slice_stop, wcstring::npos);
                auto res = expand_variables(std::move(new_in), out, varexp_char_idx, vars, errors);
                if (res.result != expand_result_t::ok) {
                    return res;
                }
            }
        }
    }
    return expand_result_t::ok;
}

/// Perform brace expansion, placing the expanded strings into \p out.
static expand_result_t expand_braces(wcstring &&instr, expand_flags_t flags,
                                     completion_receiver_t *out, parse_error_list_t *errors) {
    bool syntax_error = false;
    int brace_count = 0;

    const wchar_t *brace_begin = nullptr, *brace_end = nullptr;
    const wchar_t *last_sep = nullptr;

    const wchar_t *item_begin;
    size_t length_preceding_braces, length_following_braces, tot_len;

    const wchar_t *const in = instr.c_str();

    // Locate the first non-nested brace pair.
    for (const wchar_t *pos = in; (*pos) && !syntax_error; pos++) {
        switch (*pos) {
            case BRACE_BEGIN: {
                if (brace_count == 0) brace_begin = pos;
                brace_count++;
                break;
            }
            case BRACE_END: {
                brace_count--;
                if (brace_count < 0) {
                    syntax_error = true;
                } else if (brace_count == 0) {
                    brace_end = pos;
                }
                break;
            }
            case BRACE_SEP: {
                if (brace_count == 1) last_sep = pos;
                break;
            }
            default: {
                break;  // we ignore all other characters here
            }
        }
    }

    if (brace_count > 0) {
        if (!(flags & expand_flag::for_completions)) {
            syntax_error = true;
        } else {
            // The user hasn't typed an end brace yet; make one up and append it, then expand
            // that.
            wcstring mod;
            if (last_sep) {
                mod.append(in, brace_begin - in + 1);
                mod.append(last_sep + 1);
                mod.push_back(BRACE_END);
            } else {
                mod.append(in);
                mod.push_back(BRACE_END);
            }

            // Note: this code looks very fishy, apparently it has never worked.
            return expand_braces(std::move(mod), expand_flag::skip_cmdsubst, out, errors);
        }
    }

    if (syntax_error) {
        append_syntax_error(errors, SOURCE_LOCATION_UNKNOWN, _(L"Mismatched braces"));
        return expand_result_t::make_error(STATUS_EXPAND_ERROR);
    }

    if (brace_begin == nullptr) {
        // No more brace expansions left; we can return the value as-is.
        if (!out->add(std::move(instr))) {
            return expand_result_t::error;
        }
        return expand_result_t::ok;
    }

    length_preceding_braces = (brace_begin - in);
    length_following_braces = instr.size() - (brace_end - in) - 1;
    tot_len = length_preceding_braces + length_following_braces;
    item_begin = brace_begin + 1;
    for (const wchar_t *pos = (brace_begin + 1); true; pos++) {
        if (brace_count == 0 && ((*pos == BRACE_SEP) || (pos == brace_end))) {
            assert(pos >= item_begin);
            size_t item_len = pos - item_begin;
            wcstring item = wcstring(item_begin, item_len);
            item = trim(item, (const wchar_t[]){BRACE_SPACE, L'\0'});
            for (auto &c : item) {
                if (c == BRACE_SPACE) {
                    c = ' ';
                }
            }

            // `whole_item` is a whitespace- and brace-stripped member of a single pass of brace
            // expansion, e.g. in `{ alpha , b,{c, d }}`, `alpha`, `b`, and `c, d` will, in the
            // first round of expansion, each in turn be a `whole_item` (with recursive commas
            // replaced by special placeholders).
            // We recursively call `expand_braces` with each item until it's been fully expanded.
            wcstring whole_item;
            whole_item.reserve(tot_len + item_len + 2);
            whole_item.append(in, length_preceding_braces);
            whole_item.append(item.begin(), item.end());
            whole_item.append(brace_end + 1);
            expand_braces(std::move(whole_item), flags, out, errors);

            item_begin = pos + 1;
            if (pos == brace_end) break;
        }

        if (*pos == BRACE_BEGIN) {
            brace_count++;
        }

        if (*pos == BRACE_END) {
            brace_count--;
        }
    }
    return expand_result_t::ok;
}

/// Expand a command substitution \p input, executing on \p ctx, and inserting the results into
/// \p out_list, or any errors into \p errors. \return an expand result.
static expand_result_t expand_cmdsubst(wcstring input, const operation_context_t &ctx,
                                       completion_receiver_t *out, parse_error_list_t *errors) {
    assert(ctx.parser && "Cannot expand without a parser");
    size_t cursor = 0;
    size_t paren_begin = 0;
    size_t paren_end = 0;
    wcstring subcmd;

    bool is_quoted = false;
    bool has_dollar = false;
    switch (parse_util_locate_cmdsubst_range(input, &cursor, &subcmd, &paren_begin, &paren_end,
                                             false, &is_quoted, &has_dollar)) {
        case -1: {
            append_syntax_error(errors, SOURCE_LOCATION_UNKNOWN, L"Mismatched parenthesis");
            return expand_result_t::make_error(STATUS_EXPAND_ERROR);
        }
        case 0: {
            if (!out->add(std::move(input))) {
                return append_overflow_error(errors);
            }
            return expand_result_t::ok;
        }
        case 1: {
            break;
        }
        default: {
            DIE("unhandled parse_ret value");
        }
    }

    std::vector<wcstring> sub_res;
    int subshell_status = exec_subshell_for_expand(subcmd, *ctx.parser, ctx.job_group, sub_res);
    if (subshell_status != 0) {
        // TODO: Ad-hoc switch, how can we enumerate the possible errors more safely?
        const wchar_t *err;
        switch (subshell_status) {
            case STATUS_READ_TOO_MUCH:
                err = L"Too much data emitted by command substitution so it was discarded";
                break;
            // TODO: STATUS_CMD_ERROR is overused and too generic. We shouldn't have to test things
            // to figure out what error to show after we've already been given an error code.
            case STATUS_CMD_ERROR:
                err = L"Too many active file descriptors";
                if (ctx.parser->is_eval_depth_exceeded()) {
                    err = L"Unable to evaluate string substitution";
                }
                break;
            case STATUS_CMD_UNKNOWN:
                err = L"Unknown command";
                break;
            case STATUS_ILLEGAL_CMD:
                err = L"Commandname was invalid";
                break;
            case STATUS_NOT_EXECUTABLE:
                err = L"Command not executable";
                break;
            default:
                err = L"Unknown error while evaluating command substitution";
                break;
        }
        append_cmdsub_error(errors, paren_begin, paren_end, _(err));
        return expand_result_t::make_error(subshell_status);
    }

    // Expand slices like (cat /var/words)[1]
    size_t tail_begin = paren_end + 1;
    if (tail_begin < input.size() && input.at(tail_begin) == L'[') {
        const wchar_t *in = input.c_str();
        std::vector<long> slice_idx;
        const wchar_t *const slice_begin = in + tail_begin;
        wchar_t *slice_end = nullptr;
        parse_slice_error_t parse_error;
        size_t bad_pos =
            parse_slice(slice_begin, &slice_end, slice_idx, sub_res.size(), &parse_error);
        if (bad_pos != 0) {
            switch (parse_error) {
                case parse_slice_error_t::none:
                    assert(false && "bad_pos != 0 but parse_slice_error_t::none!");
                    break;
                case parse_slice_error_t::zero_index:
                    append_syntax_error(errors, slice_begin - in + bad_pos,
                                        L"array indices start at 1, not 0.");
                    break;
                case parse_slice_error_t::invalid_index:
                    append_syntax_error(errors, slice_begin - in + bad_pos, L"Invalid index value");
                    break;
            }
            return expand_result_t::make_error(STATUS_EXPAND_ERROR);
        }

        std::vector<wcstring> sub_res2;
        tail_begin = slice_end - in;
        for (long idx : slice_idx) {
            if (static_cast<size_t>(idx) > sub_res.size() || idx < 1) {
                continue;
            }
            // -1 to convert from 1-based slice index to C++ 0-based vector index.
            sub_res2.push_back(sub_res.at(idx - 1));
        }
        sub_res = std::move(sub_res2);
    }

    // Recursively call ourselves to expand any remaining command substitutions. The result of this
    // recursive call using the tail of the string is inserted into the tail_expand array list
    completion_receiver_t tail_expand_recv = out->subreceiver();
    wcstring tail = input.substr(tail_begin);
    // A command substitution inside double quotes magically closes the quoted string.
    // Reopen the quotes just after the command substitution.
    if (is_quoted) {
        tail.insert(0, L"\"");
    }
    expand_cmdsubst(std::move(tail), ctx, &tail_expand_recv,
                    errors);  // TODO: offset error locations
    completion_list_t tail_expand = tail_expand_recv.take();

    // Combine the result of the current command substitution with the result of the recursive tail
    // expansion.

    if (is_quoted) {
        // Awkwardly reconstruct the command output.
        size_t approx_size = 0;
        for (const wcstring &sub_item : sub_res) {
            approx_size += sub_item.size() + 1;
        }
        wcstring sub_res_joined;
        sub_res_joined.reserve(approx_size);
        for (wcstring &line : sub_res) {
            sub_res_joined.append(escape_string_for_double_quotes(std::move(line)));
            sub_res_joined.push_back(L'\n');
        }
        // Mimic POSIX shells by stripping all trailing newlines.
        if (!sub_res_joined.empty()) {
            size_t i;
            for (i = sub_res_joined.size(); i > 0; i--) {
                if (sub_res_joined[i - 1] != L'\n') break;
            }
            sub_res_joined.erase(i);
        }
        // Instead of performing cartesian product expansion, we directly insert the command
        // substitution output into the current expansion results.
        for (const completion_t &tail_item : tail_expand) {
            wcstring whole_item;
            whole_item.reserve(paren_begin + 1 + sub_res_joined.size() + 1 +
                               tail_item.completion.size());
            whole_item.append(input, 0, paren_begin - has_dollar);
            whole_item.push_back(INTERNAL_SEPARATOR);
            whole_item.append(sub_res_joined);
            whole_item.push_back(INTERNAL_SEPARATOR);
            whole_item.append(tail_item.completion.substr(const_strlen(L"\"")));
            if (!out->add(std::move(whole_item))) {
                return append_overflow_error(errors);
            }
        }

        return expand_result_t::ok;
    }

    for (const wcstring &sub_item : sub_res) {
        wcstring sub_item2 = escape_string(sub_item);
        for (const completion_t &tail_item : tail_expand) {
            wcstring whole_item;
            whole_item.reserve(paren_begin + 1 + sub_item2.size() + 1 +
                               tail_item.completion.size());
            whole_item.append(input, 0, paren_begin - has_dollar);
            whole_item.push_back(INTERNAL_SEPARATOR);
            whole_item.append(sub_item2);
            whole_item.push_back(INTERNAL_SEPARATOR);
            whole_item.append(tail_item.completion);
            if (!out->add(std::move(whole_item))) {
                return append_overflow_error(errors);
            }
        }
    }

    return expand_result_t::ok;
}

// Given that input[0] is HOME_DIRECTORY or tilde (ugh), return the user's name. Return the empty
// string if it is just a tilde. Also return by reference the index of the first character of the
// remaining part of the string (e.g. the subsequent slash).
static wcstring get_home_directory_name(const wcstring &input, size_t *out_tail_idx) {
    assert(input[0] == HOME_DIRECTORY || input[0] == L'~');

    auto pos = input.find_first_of(L'/');
    // We get the position of the /, but we need to remove it as well.
    if (pos != wcstring::npos) {
        *out_tail_idx = pos;
        pos -= 1;
    } else {
        *out_tail_idx = input.length();
    }

    return input.substr(1, pos);
}

/// Attempts tilde expansion of the string specified, modifying it in place.
static void expand_home_directory(wcstring &input, const environment_t &vars) {
    if (!input.empty() && input.at(0) == HOME_DIRECTORY) {
        size_t tail_idx;
        wcstring username = get_home_directory_name(input, &tail_idx);

        maybe_t<wcstring> home;
        if (username.empty()) {
            // Current users home directory.
            auto home_var = vars.get_unless_empty(L"HOME");
            if (!home_var) {
                input.clear();
                return;
            }
            home = home_var->as_string();
            tail_idx = 1;
        } else {
            // Some other user's home directory.
            std::string name_cstr = wcs2zstring(username);
            struct passwd userinfo;
            struct passwd *result;
            char buf[8192];
            int retval = getpwnam_r(name_cstr.c_str(), &userinfo, buf, sizeof(buf), &result);
            if (!retval && result) {
                home = str2wcstring(userinfo.pw_dir);
            }
        }

        if (home) {
            input.replace(input.begin(), input.begin() + tail_idx, normalize_path(*home));
        } else {
            input[0] = L'~';
        }
    }
}

/// Expand the %self escape. Note this can only come at the beginning of the string.
static void expand_percent_self(wcstring &input) {
    if (!input.empty() && input.front() == PROCESS_EXPAND_SELF) {
        input.replace(0, 1, to_string(getpid()));
    }
}

void expand_tilde(wcstring &input, const environment_t &vars) {
    // Avoid needless COW behavior by ensuring we use const at.
    const wcstring &tmp = input;
    if (!tmp.empty() && tmp.at(0) == L'~') {
        input.at(0) = HOME_DIRECTORY;
        expand_home_directory(input, vars);
    }
}

// If the given path contains the user's home directory, replace that with a tilde. We don't try to
// be smart about case insensitivity, etc.
wcstring replace_home_directory_with_tilde(const wcstring &str, const environment_t &vars) {
    // Only absolute paths get this treatment.
    wcstring result = str;
    if (string_prefixes_string(L"/", result)) {
        wcstring home_directory = L"~";
        expand_tilde(home_directory, vars);
        if (!string_suffixes_string(L"/", home_directory)) {
            home_directory.push_back(L'/');
        }

        // Now check if the home_directory prefixes the string.
        if (string_prefixes_string(home_directory, result)) {
            // Success
            result.replace(0, home_directory.size(), L"~/");
        }
    }
    return result;
}

/// Remove any internal separators. Also optionally convert wildcard characters to regular
/// equivalents. This is done to support skip_wildcards.
static void remove_internal_separator(wcstring *str, bool conv) {
    // Remove all instances of INTERNAL_SEPARATOR.
    str->erase(std::remove(str->begin(), str->end(), static_cast<wchar_t>(INTERNAL_SEPARATOR)),
               str->end());

    // If conv is true, replace all instances of ANY_STRING with '*',
    // ANY_STRING_RECURSIVE with '*'.
    if (conv) {
        for (auto &idx : *str) {
            switch (idx) {
                case ANY_CHAR: {
                    idx = L'?';
                    break;
                }
                case ANY_STRING:
                case ANY_STRING_RECURSIVE: {
                    idx = L'*';
                    break;
                }
                default: {
                    break;  // we ignore all other characters
                }
            }
        }
    }
}

namespace {
/// A type that knows how to perform expansions.
class expander_t {
    /// Operation context for this expansion.
    const operation_context_t &ctx;

    /// Flags to use during expansion.
    const expand_flags_t flags;

    /// List to receive any errors generated during expansion, or null to ignore errors.
    parse_error_list_t *const errors;

    /// An expansion stage is a member function pointer.
    /// It accepts the input string (transferring ownership) and returns the list of output
    /// completions by reference. It may return an error, which halts expansion.
    using stage_t = expand_result_t (expander_t::*)(wcstring, completion_receiver_t *);

    expand_result_t stage_cmdsubst(wcstring input, completion_receiver_t *out);
    expand_result_t stage_variables(wcstring input, completion_receiver_t *out);
    expand_result_t stage_braces(wcstring input, completion_receiver_t *out);
    expand_result_t stage_home_and_self(wcstring input, completion_receiver_t *out);
    expand_result_t stage_wildcards(wcstring path_to_expand, completion_receiver_t *out);

    expander_t(const operation_context_t &ctx, expand_flags_t flags, parse_error_list_t *errors)
        : ctx(ctx), flags(flags), errors(errors) {}

    // Given an original input string, if it starts with a tilde, "unexpand" the expanded home
    // directory.  Note this may be just a tilde or a user name like ~foo/.
    void unexpand_tildes(const wcstring &input, completion_list_t *completions) const;

   public:
    static expand_result_t expand_string(wcstring input, completion_receiver_t *out_completions,
                                         expand_flags_t flags, const operation_context_t &ctx,
                                         parse_error_list_t *errors);
};

expand_result_t expander_t::stage_cmdsubst(wcstring input, completion_receiver_t *out) {
    if (flags & expand_flag::skip_cmdsubst) {
        size_t cur = 0, start = 0, end;
        switch (parse_util_locate_cmdsubst_range(input, &cur, nullptr, &start, &end, true)) {
            case 0:
                if (!out->add(std::move(input))) {
                    return append_overflow_error(errors);
                }
                return expand_result_t::ok;
            case 1:
                append_cmdsub_error(errors, start, end,
                                    L"command substitutions not allowed here");  // clang-format off
                __fallthrough__
            case -1:
                // clang-format on
            default:
                return expand_result_t::make_error(STATUS_EXPAND_ERROR);
        }
    } else {
        assert(ctx.parser && "Must have a parser to expand command substitutions");
        return expand_cmdsubst(std::move(input), ctx, out, errors);
    }
}

// We pass by value to match other stages. NOLINTNEXTLINE(performance-unnecessary-value-param)
expand_result_t expander_t::stage_variables(wcstring input, completion_receiver_t *out) {
    // We accept incomplete strings here, since complete uses expand_string to expand incomplete
    // strings from the commandline.
    wcstring next;
    if (auto unescaped = unescape_string(input, UNESCAPE_SPECIAL | UNESCAPE_INCOMPLETE))
        next = *unescaped;

    if (flags & expand_flag::skip_variables) {
        for (auto &i : next) {
            if (i == VARIABLE_EXPAND || i == VARIABLE_EXPAND_SINGLE) {
                i = L'$';
            }
        }
        if (!out->add(std::move(next))) {
            return append_overflow_error(errors);
        }
        return expand_result_t::ok;
    } else {
        size_t size = next.size();
        return expand_variables(std::move(next), out, size, ctx.vars, errors);
    }
}

expand_result_t expander_t::stage_braces(wcstring input, completion_receiver_t *out) {
    return expand_braces(std::move(input), flags, out, errors);
}

expand_result_t expander_t::stage_home_and_self(wcstring input, completion_receiver_t *out) {
    expand_home_directory(input, ctx.vars);
    expand_percent_self(input);
    if (!out->add(std::move(input))) {
        return append_overflow_error(errors);
    }
    return expand_result_t::ok;
}

expand_result_t expander_t::stage_wildcards(wcstring path_to_expand, completion_receiver_t *out) {
    expand_result_t result = expand_result_t::ok;

    remove_internal_separator(&path_to_expand, flags & expand_flag::skip_wildcards);
    const bool has_wildcard = wildcard_has_internal(path_to_expand);  // e.g. ANY_STRING
    const bool for_completions = flags & expand_flag::for_completions;
    const bool skip_wildcards = flags & expand_flag::skip_wildcards;

    if (has_wildcard && (flags & expand_flag::executables_only)) {
        // don't do wildcard expansion for executables, see issue #785
    } else if ((for_completions && !skip_wildcards) || has_wildcard) {
        // We either have a wildcard, or we don't have a wildcard but we're doing completion
        // expansion (so we want to get the completion of a file path). Note that if
        // skip_wildcards is set, we stomped wildcards in remove_internal_separator above, so
        // there actually aren't any.
        //
        // So we're going to treat this input as a file path. Compute the "working directories",
        // which may be CDPATH if the special flag is set.
        const wcstring working_dir = ctx.vars.get_pwd_slash();
        std::vector<wcstring> effective_working_dirs;
        bool for_cd = flags & expand_flag::special_for_cd;
        bool for_command = flags & expand_flag::special_for_command;
        if (!for_cd && !for_command) {
            // Common case.
            effective_working_dirs.push_back(working_dir);
        } else {
            // Either special_for_command or special_for_cd. We can handle these
            // mostly the same. There's the following differences:
            //
            // 1. An empty CDPATH should be treated as '.', but an empty PATH should be left empty
            // (no commands can be found). Also, an empty element in either is treated as '.' for
            // consistency with POSIX shells. Note that we rely on the latter by having called
            // `munge_colon_delimited_array()` for these special env vars. Thus we do not
            // special-case them here.
            //
            // 2. PATH is only "one level," while CDPATH is multiple levels. That is, input like
            // 'foo/bar' should resolve against CDPATH, but not PATH.
            //
            // In either case, we ignore the path if we start with ./ or /. Also ignore it if we are
            // doing command completion and we contain a slash, per IEEE 1003.1, chapter 8 under
            // PATH.
            if (string_prefixes_string(L"/", path_to_expand) ||
                string_prefixes_string(L"./", path_to_expand) ||
                string_prefixes_string(L"../", path_to_expand) ||
                (for_command && path_to_expand.find(L'/') != wcstring::npos)) {
                effective_working_dirs.push_back(working_dir);
            } else {
                // Get the PATH/CDPATH and CWD. Perhaps these should be passed in. An empty CDPATH
                // implies just the current directory, while an empty PATH is left empty.
                std::vector<wcstring> paths;
                if (auto paths_var = ctx.vars.get(for_cd ? L"CDPATH" : L"PATH")) {
                    paths = paths_var->as_list();
                }

                // The current directory is always valid.
                paths.emplace_back(for_cd ? L"." : L"");
                for (const wcstring &next_path : paths) {
                    effective_working_dirs.push_back(
                        path_apply_working_directory(next_path, working_dir));
                }
            }
        }

        result = expand_result_t::wildcard_no_match;
        completion_receiver_t expanded_recv = out->subreceiver();
        for (const auto &effective_working_dir : effective_working_dirs) {
            wildcard_result_t expand_res = wildcard_expand_string(
                path_to_expand, effective_working_dir, flags, ctx.cancel_checker, &expanded_recv);
            switch (expand_res) {
                case wildcard_result_t::match:
                    result = expand_result_t::ok;
                    break;
                case wildcard_result_t::no_match:
                    break;
                case wildcard_result_t::overflow:
                    return append_overflow_error(errors);
                case wildcard_result_t::cancel:
                    return expand_result_t::cancel;
            }
        }

        completion_list_t expanded = expanded_recv.take();
        std::sort(expanded.begin(), expanded.end(),
                  [&](const completion_t &a, const completion_t &b) {
                      return wcsfilecmp_glob(a.completion.c_str(), b.completion.c_str()) < 0;
                  });
        if (!out->add_list(std::move(expanded))) {
            result = expand_result_t::error;
        }
    } else {
        // Can't fully justify this check. I think it's that SKIP_WILDCARDS is used when completing
        // to mean don't do file expansions, so if we're not doing file expansions, just drop this
        // completion on the floor.
        if (!(flags & expand_flag::for_completions)) {
            if (!out->add(std::move(path_to_expand))) {
                return append_overflow_error(errors);
            }
        }
    }
    return result;
}

void expander_t::unexpand_tildes(const wcstring &input, completion_list_t *completions) const {
    // If input begins with tilde, then try to replace the corresponding string in each completion
    // with the tilde. If it does not, there's nothing to do.
    if (input.empty() || input.at(0) != L'~') return;

    // This is a subtle kludge. We need to decide whether to unexpand tildes for all
    // completions, or only those which replace their tokens. The problem is that we're sloppy
    // about setting the COMPLETE_REPLACES_TOKEN flag, except when we're completing in the
    // wildcard stage, because no other clients of string expansion care. Example:
    //   HOME=/foo
    //   mkdir ~/foo # makes /foo/foo
    //   cd ~/<tab>
    // Here we are likely to get a completion 'foo' which may match $HOME, but it extends its token
    // instead of replacing it, so we don't modify it (it will just be appended to the original ~/).
    //
    // However if we are not completing, just expanding, then expansion just produces the full paths
    // so we should unconditionally unexpand tildes.
    bool only_replacers = bool(flags & expand_flag::for_completions);

    // Helper to decide whether to process a completion.
    auto should_process = [=](const completion_t &c) {
        return only_replacers ? c.replaces_token() : true;
    };

    // Early out if none qualify.
    if (std::none_of(completions->begin(), completions->end(), should_process)) return;

    // Get the username_with_tilde (like ~bert) and expand it into a home directory.
    size_t tail_idx;
    wcstring username_with_tilde = L"~" + get_home_directory_name(input, &tail_idx);
    wcstring home = username_with_tilde;
    expand_tilde(home, ctx.vars);

    // Now for each completion that starts with home, replace it with the username_with_tilde.
    for (auto &comp : *completions) {
        if (should_process(comp) && string_prefixes_string(home, comp.completion)) {
            comp.completion.replace(0, home.size(), username_with_tilde);

            // And mark that our tilde is literal, so it doesn't try to escape it.
            comp.flags |= COMPLETE_DONT_ESCAPE_TILDES;
        }
    }
}

expand_result_t expander_t::expand_string(wcstring input, completion_receiver_t *out_completions,
                                          expand_flags_t flags, const operation_context_t &ctx,
                                          parse_error_list_t *errors) {
    assert(((flags & expand_flag::skip_cmdsubst) || ctx.parser) &&
           "Must have a parser if not skipping command substitutions");
    // Early out. If we're not completing, and there's no magic in the input, we're done.
    if (!(flags & expand_flag::for_completions) && expand_is_clean(input)) {
        if (!out_completions->add(std::move(input))) {
            return append_overflow_error(errors);
        }
        return expand_result_t::ok;
    }

    expander_t expand(ctx, flags, errors);

    // Our expansion stages.
    const stage_t stages[] = {&expander_t::stage_cmdsubst, &expander_t::stage_variables,
                              &expander_t::stage_braces, &expander_t::stage_home_and_self,
                              &expander_t::stage_wildcards};

    // Load up our single initial completion.
    completion_list_t completions;
    append_completion(&completions, input);

    completion_receiver_t output_storage = out_completions->subreceiver();
    expand_result_t total_result = expand_result_t::ok;
    for (stage_t stage : stages) {
        for (completion_t &comp : completions) {
            if (ctx.check_cancel()) {
                total_result = expand_result_t::cancel;
                break;
            }
            expand_result_t this_result =
                (expand.*stage)(std::move(comp.completion), &output_storage);
            total_result = this_result;
            if (total_result == expand_result_t::error) {
                break;
            }
        }

        // Output becomes our next stage's input.
        output_storage.swap(completions);
        output_storage.clear();
        if (total_result == expand_result_t::error) {
            break;
        }
    }

    // This is a little tricky: if one wildcard failed to match but we still got output, it
    // means that a previous expansion resulted in multiple strings. For example:
    //   set dirs ./a ./b
    //   echo $dirs/*.txt
    // Here if ./a/*.txt matches and ./b/*.txt does not, then we don't want to report a failed
    // wildcard. So swallow failed-wildcard errors if we got any output.
    if (total_result == expand_result_t::wildcard_no_match && !completions.empty()) {
        total_result = expand_result_t::ok;
    }

    if (total_result == expand_result_t::ok) {
        // Unexpand tildes if we want to preserve them (see #647).
        if (flags.get(expand_flag::preserve_home_tildes)) {
            expand.unexpand_tildes(input, &completions);
        }
        if (!out_completions->add_list(std::move(completions))) {
            total_result = append_overflow_error(errors);
        }
    }
    return total_result;
}
}  // namespace

expand_result_t expand_string(wcstring input, completion_list_t *out_completions,
                              expand_flags_t flags, const operation_context_t &ctx,
                              parse_error_list_t *errors) {
    completion_receiver_t recv(std::move(*out_completions), ctx.expansion_limit);
    auto res = expand_string(std::move(input), &recv, flags, ctx, errors);
    *out_completions = recv.take();
    return res;
}

expand_result_t expand_string(wcstring input, completion_receiver_t *out_completions,
                              expand_flags_t flags, const operation_context_t &ctx,
                              parse_error_list_t *errors) {
    return expander_t::expand_string(std::move(input), out_completions, flags, ctx, errors);
}

bool expand_one(wcstring &string, expand_flags_t flags, const operation_context_t &ctx,
                parse_error_list_t *errors) {
    completion_list_t completions;

    if (!flags.get(expand_flag::for_completions) && expand_is_clean(string)) {
        return true;
    }

    if (expand_string(std::move(string), &completions, flags, ctx, errors) == expand_result_t::ok &&
        completions.size() == 1) {
        string = std::move(completions.at(0).completion);
        return true;
    }
    return false;
}

expand_result_t expand_to_command_and_args(const wcstring &instr, const operation_context_t &ctx,
                                           wcstring *out_cmd, std::vector<wcstring> *out_args,
                                           parse_error_list_t *errors, bool skip_wildcards) {
    // Fast path.
    if (expand_is_clean(instr)) {
        *out_cmd = instr;
        return expand_result_t::ok;
    }

    expand_flags_t eflags{expand_flag::skip_cmdsubst};
    if (skip_wildcards) {
        eflags.set(expand_flag::skip_wildcards);
    }

    completion_list_t completions;
    expand_result_t expand_err = expand_string(instr, &completions, eflags, ctx, errors);
    if (expand_err == expand_result_t::ok) {
        // The first completion is the command, any remaning are arguments.
        bool first = true;
        for (auto &comp : completions) {
            if (first) {
                if (out_cmd) *out_cmd = std::move(comp.completion);
                first = false;
            } else {
                if (out_args) out_args->push_back(std::move(comp.completion));
            }
        }
    }
    return expand_err;
}

// https://github.com/fish-shell/fish-shell/issues/367
//
// With them the Seed of Wisdom did I sow,
// And with my own hand labour'd it to grow:
// And this was all the Harvest that I reap'd---
// "I came like Water, and like Wind I go."

static std::string escape_single_quoted_hack_hack_hack_hack(const char *str) {
    std::string result;
    size_t len = std::strlen(str);
    result.reserve(len + 2);
    result.push_back('\'');
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        // Escape backslashes and single quotes only.
        if (c == '\\' || c == '\'') result.push_back('\\');
        result.push_back(c);
    }
    result.push_back('\'');
    return result;
}

bool fish_xdm_login_hack_hack_hack_hack(std::vector<std::string> *cmds, int argc,
                                        const char *const *argv) {
    if (!cmds || cmds->size() != 1) {
        return false;
    }

    bool result = false;
    const std::string &cmd = cmds->at(0);
    if (cmd == "exec \"${@}\"" || cmd == "exec \"$@\"") {
        // We're going to construct a new command that starts with exec, and then has the
        // remaining arguments escaped.
        std::string new_cmd = "exec";
        for (int i = 1; i < argc; i++) {
            const char *arg = argv[i];
            if (arg) {
                new_cmd.push_back(' ');
                new_cmd.append(escape_single_quoted_hack_hack_hack_hack(arg));
            }
        }

        cmds->at(0) = new_cmd;
        result = true;
    }
    return result;
}
