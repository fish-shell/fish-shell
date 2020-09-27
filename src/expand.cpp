// String expansion functions. These functions perform several kinds of parameter expansion.
// IWYU pragma: no_include <cstddef>
#include "config.h"

#include <errno.h>
#include <pwd.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <wctype.h>

#include <cstring>
#include <cwchar>

#ifdef SunOS
#include <procfs.h>
#endif
#ifdef __APPLE__
#include <sys/time.h>  // Required to build with old SDK versions
// proc.h needs to be included *after* time.h, this comment stops clang-format from reordering.
#include <sys/proc.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#include <algorithm>
#include <functional>
#include <map>
#include <memory>  // IWYU pragma: keep
#include <type_traits>
#include <utility>
#include <vector>

#include "common.h"
#include "complete.h"
#include "env.h"
#include "exec.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "history.h"
#include "iothread.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "parser.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
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
    error.code = parse_error_syntax;

    va_list va;
    va_start(va, fmt);
    error.text = vformat_string(fmt, va);
    va_end(va);

    errors->push_back(error);
}

/// Append a cmdsub error to the given error list. But only do so if the error hasn't already been
/// recorded. This is needed because command substitution is a recursive process and some errors
/// could consequently be recorded more than once.
static void append_cmdsub_error(parse_error_list_t *errors, size_t source_start, const wchar_t *fmt,
                                ...) {
    if (!errors) return;

    parse_error_t error;
    error.source_start = source_start;
    error.source_length = 0;
    error.code = parse_error_cmdsubst;

    va_list va;
    va_start(va, fmt);
    error.text = vformat_string(fmt, va);
    va_end(va);

    for (const auto &it : *errors) {
        if (error.text == it.text) return;
    }

    errors->push_back(error);
}

/// Test if the specified string does not contain character which can not be used inside a quoted
/// string.
static bool is_quotable(const wcstring &str) {
    return str.find_first_of(L"\n\t\r\b\x1B") == wcstring::npos;
}

wcstring expand_escape_variable(const env_var_t &var) {
    wcstring buff;
    wcstring_list_t lst;

    var.to_list(lst);
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
            buff.append(escape_string(el, 1));
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
        buff.append(escape_string(el, 1));
    }
    return buff;
}

/// Parse an array slicing specification Returns 0 on success. If a parse error occurs, returns the
/// index of the bad token. Note that 0 can never be a bad index because the string always starts
/// with [.
static size_t parse_slice(const wchar_t *in, wchar_t **end_ptr, std::vector<long> &idx,
                          size_t array_size) {
    const long size = static_cast<long>(array_size);
    size_t pos = 1;  // skip past the opening square brace

    int zero_index = -1;
    bool literal_zero_index = true;

    while (true) {
        while (iswspace(in[pos]) || (in[pos] == INTERNAL_SEPARATOR)) pos++;
        if (in[pos] == L']') {
            pos++;
            break;
        }

        // Explicitly refuse $foo[0] as valid syntax, regardless of whether or not we're going
        // to show an error if the index ultimately evaluates to zero. This will help newcomers
        // to fish avoid a common off-by-one error. See #4862.
        if (literal_zero_index) {
            if (in[pos] == L'0') {
                zero_index = pos;
            } else {
                literal_zero_index = false;
            }
        }

        const wchar_t *end;
        long tmp;
        if (idx.empty() && in[pos] == L'.' && in[pos + 1] == L'.') {
            // If we are at the first index expression, a missing start index means the range starts
            // at the first item.
            tmp = 1;  // first index
            end = &in[pos];
        } else {
            tmp = fish_wcstol(&in[pos], &end);
            if (errno > 0) {
                // We don't test `*end` as is typically done because we expect it to not be the null
                // char. Ignore the case of errno==-1 because it means the end char wasn't the null
                // char.
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
            // Check if we are at the last index range expression, a missing end index means the
            // range spans until the last item.
            if (in[pos] == L']') {
                tmp1 = -1;  // last index
                end = &in[pos];
            } else {
                tmp1 = fish_wcstol(&in[pos], &end);
                // Ignore the case of errno==-1 because it means the end char wasn't the null char.
                if (errno > 0) {
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

        literal_zero_index = literal_zero_index && tmp == 0;
        idx.push_back(i1);
    }

    if (literal_zero_index && zero_index != -1) {
        return zero_index;
    }

    if (end_ptr) {
        *end_ptr = const_cast<wchar_t *>(in + pos);
    }

    return 0;
}

/// Expand all environment variables in the string *ptr.
///
/// This function is slow, fragile and complicated. There are lots of little corner cases, like
/// $$foo should do a double expansion, $foo$bar should not double expand bar, etc. Also, it's easy
/// to accidentally leak memory on array out of bounds errors an various other situations. All in
/// all, this function should be rewritten, split out into multiple logical units and carefully
/// tested. After that, it can probably be optimized to do fewer memory allocations, fewer string
/// scans and overall just less work. But until that happens, don't edit it unless you know exactly
/// what you are doing, and do proper testing afterwards.
///
/// This function operates on strings backwards, starting at last_idx.
///
/// Note: last_idx is considered to be where it previously finished procesisng. This means it
/// actually starts operating on last_idx-1. As such, to process a string fully, pass string.size()
/// as last_idx instead of string.size()-1.
static bool expand_variables(wcstring instr, completion_list_t *out, size_t last_idx,
                             const environment_t &vars, parse_error_list_t *errors) {
    const size_t insize = instr.size();

    // last_idx may be 1 past the end of the string, but no further.
    assert(last_idx <= insize && "Invalid last_idx");
    if (last_idx == 0) {
        append_completion(out, std::move(instr));
        return true;
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
        append_completion(out, std::move(instr));
        return true;
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
        return false;
    }

    // Get the variable name as a string, then try to get the variable from env.
    const wcstring var_name(instr, var_name_start, var_name_len);
    // Do a dirty hack to make sliced history fast (#4650). We expand from either a variable, or a
    // history_t. Note that "history" is read only in env.cpp so it's safe to special-case it in
    // this way (it cannot be shadowed, etc).
    history_t *history = nullptr;
    maybe_t<env_var_t> var{};
    if (var_name == L"history") {
        // Note reader_get_history may return null, if we are running non-interactively (e.g. from
        // web_config).
        if (is_main_thread()) {
            history = &history_t::history_with_name(history_session_id(env_stack_t::principal()));
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
            effective_val_count = history->size();
        }
        size_t bad_pos =
            parse_slice(in + slice_start, &slice_end, var_idx_list, effective_val_count);
        if (bad_pos != 0) {
            if (in[slice_start + bad_pos] == L'0') {
                append_syntax_error(errors, slice_start + bad_pos,
                                    L"array indices start at 1, not 0.");
            } else {
                append_syntax_error(errors, slice_start + bad_pos, L"Invalid index value");
            }
            return false;
        }
        var_name_and_slice_stop = (slice_end - in);
    }

    if (!var && !history) {
        // Expanding a non-existent variable.
        if (!is_single) {
            // Normal expansions of missing variables successfully expand to nothing.
            return true;
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
    wcstring_list_t var_item_list;
    if (all_values) {
        if (history) {
            history->get_history(var_item_list);
        } else {
            var->to_list(var_item_list);
        }
    } else {
        // We have to respect the slice.
        if (history) {
            // Ask history to map indexes to item strings.
            // Note this may have missing entries for out-of-bounds.
            auto item_map = history->items_at_indexes(var_idx_list);
            for (long item_index : var_idx_list) {
                auto iter = item_map.find(item_index);
                if (iter != item_map.end()) {
                    var_item_list.push_back(iter->second);
                }
            }
        } else {
            const wcstring_list_t &all_var_items = var->as_list();
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
        for (const wcstring &item : var_item_list) {
            if (varexp_char_idx == 0 && var_name_and_slice_stop == insize) {
                append_completion(out, item);
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
                if (!expand_variables(std::move(new_in), out, varexp_char_idx, vars, errors)) {
                    return false;
                }
            }
        }
    }
    return true;
}

/// Perform brace expansion, placing the expanded strings into \p out.
static expand_result_t expand_braces(wcstring &&instr, expand_flags_t flags, completion_list_t *out,
                                     parse_error_list_t *errors) {
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
        append_completion(out, std::move(instr));
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
                                       completion_list_t *out_list, parse_error_list_t *errors) {
    assert(ctx.parser && "Cannot expand without a parser");
    size_t cursor = 0;
    size_t paren_begin = 0;
    size_t paren_end = 0;
    wcstring subcmd;

    switch (parse_util_locate_cmdsubst_range(input, &cursor, &subcmd, &paren_begin, &paren_end,
                                             false)) {
        case -1: {
            append_syntax_error(errors, SOURCE_LOCATION_UNKNOWN, L"Mismatched parenthesis");
            return expand_result_t::make_error(STATUS_EXPAND_ERROR);
        }
        case 0: {
            append_completion(out_list, std::move(input));
            return expand_result_t::ok;
        }
        case 1: {
            break;
        }
        default: {
            DIE("unhandled parse_ret value");
        }
    }

    wcstring_list_t sub_res;
    int subshell_status = exec_subshell_for_expand(subcmd, *ctx.parser, ctx.job_group, sub_res);
    if (subshell_status != 0) {
        // TODO: Ad-hoc switch, how can we enumerate the possible errors more safely?
        const wchar_t *err;
        switch (subshell_status) {
            case STATUS_READ_TOO_MUCH:
                err = L"Too much data emitted by command substitution so it was discarded";
                break;
            case STATUS_CMD_ERROR:
                err = L"Too many active file descriptors";
                break;
            default:
                err = L"Unknown error while evaluating command substitution";
                break;
        }
        append_cmdsub_error(errors, paren_begin, _(err));
        return expand_result_t::make_error(subshell_status);
    }

    // Expand slices like (cat /var/words)[1]
    size_t tail_begin = paren_end + 1;
    if (tail_begin < input.size() && input.at(tail_begin) == L'[') {
        const wchar_t *in = input.c_str();
        std::vector<long> slice_idx;
        const wchar_t *const slice_begin = in + tail_begin;
        wchar_t *slice_end = nullptr;
        size_t bad_pos = parse_slice(slice_begin, &slice_end, slice_idx, sub_res.size());
        if (bad_pos != 0) {
            if (slice_begin[bad_pos] == L'0') {
                append_syntax_error(errors, slice_begin - in + bad_pos,
                                    L"array indices start at 1, not 0.");
            } else {
                append_syntax_error(errors, slice_begin - in + bad_pos, L"Invalid index value");
            }
            return expand_result_t::make_error(STATUS_EXPAND_ERROR);
        }

        wcstring_list_t sub_res2;
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
    completion_list_t tail_expand;
    expand_cmdsubst(input.substr(tail_begin), ctx, &tail_expand,
                    errors);  // TODO: offset error locations

    // Combine the result of the current command substitution with the result of the recursive tail
    // expansion.
    for (const wcstring &sub_item : sub_res) {
        wcstring sub_item2 = escape_string(sub_item, ESCAPE_ALL);
        for (const completion_t &tail_item : tail_expand) {
            wcstring whole_item;
            whole_item.reserve(paren_begin + 1 + sub_item2.size() + 1 +
                               tail_item.completion.size());
            whole_item.append(input, 0, paren_begin);
            whole_item.push_back(INTERNAL_SEPARATOR);
            whole_item.append(sub_item2);
            whole_item.push_back(INTERNAL_SEPARATOR);
            whole_item.append(tail_item.completion);
            append_completion(out_list, std::move(whole_item));
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
            auto home_var = vars.get(L"HOME");
            if (home_var.missing_or_empty()) {
                input.clear();
                return;
            }
            home = home_var->as_string();
            tail_idx = 1;
        } else {
            // Some other user's home directory.
            std::string name_cstr = wcs2string(username);
            struct passwd userinfo;
            struct passwd *result;
            char buf[8192];
            int retval = getpwnam_r(name_cstr.c_str(), &userinfo, buf, sizeof(buf), &result);
            if (!retval && result) {
                home = str2wcstring(userinfo.pw_dir);
            }
        }

        maybe_t<wcstring> realhome;
        if (home) realhome = normalize_path(*home);

        if (realhome) {
            input.replace(input.begin(), input.begin() + tail_idx, *realhome);
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

static void unexpand_tildes(const wcstring &input, const environment_t &vars,
                            completion_list_t *completions) {
    // If input begins with tilde, then try to replace the corresponding string in each completion
    // with the tilde. If it does not, there's nothing to do.
    if (input.empty() || input.at(0) != L'~') return;

    // We only operate on completions that replace their contents. If we don't have any, we're done.
    // In particular, empty vectors are common.
    bool has_candidate_completion = false;
    for (const auto &completion : *completions) {
        if (completion.flags & COMPLETE_REPLACES_TOKEN) {
            has_candidate_completion = true;
            break;
        }
    }
    if (!has_candidate_completion) return;

    size_t tail_idx;
    wcstring username_with_tilde = L"~";
    username_with_tilde.append(get_home_directory_name(input, &tail_idx));

    // Expand username_with_tilde.
    wcstring home = username_with_tilde;
    expand_tilde(home, vars);

    // Now for each completion that starts with home, replace it with the username_with_tilde.
    for (auto &comp : *completions) {
        if ((comp.flags & COMPLETE_REPLACES_TOKEN) &&
            string_prefixes_string(home, comp.completion)) {
            comp.completion.replace(0, home.size(), username_with_tilde);

            // And mark that our tilde is literal, so it doesn't try to escape it.
            comp.flags |= COMPLETE_DONT_ESCAPE_TILDES;
        }
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
    using stage_t = expand_result_t (expander_t::*)(wcstring, completion_list_t *);

    expand_result_t stage_cmdsubst(wcstring input, completion_list_t *out);
    expand_result_t stage_variables(wcstring input, completion_list_t *out);
    expand_result_t stage_braces(wcstring input, completion_list_t *out);
    expand_result_t stage_home_and_self(wcstring input, completion_list_t *out);
    expand_result_t stage_wildcards(wcstring path_to_expand, completion_list_t *out);

    expander_t(const operation_context_t &ctx, expand_flags_t flags, parse_error_list_t *errors)
        : ctx(ctx), flags(flags), errors(errors) {}

   public:
    static expand_result_t expand_string(wcstring input, completion_list_t *out_completions,
                                         expand_flags_t flags, const operation_context_t &ctx,
                                         parse_error_list_t *errors);
};

expand_result_t expander_t::stage_cmdsubst(wcstring input, completion_list_t *out) {
    if (flags & expand_flag::skip_cmdsubst) {
        size_t cur = 0, start = 0, end;
        switch (parse_util_locate_cmdsubst_range(input, &cur, nullptr, &start, &end, true)) {
            case 0:
                append_completion(out, std::move(input));
                return expand_result_t::ok;
            case 1:
                append_cmdsub_error(errors, start, L"Command substitutions not allowed");
                /* intentionally falls through */
            case -1:
            default:
                return expand_result_t::make_error(STATUS_EXPAND_ERROR);
        }
    } else {
        assert(ctx.parser && "Must have a parser to expand command substitutions");
        return expand_cmdsubst(std::move(input), ctx, out, errors);
    }
}

expand_result_t expander_t::stage_variables(wcstring input, completion_list_t *out) {
    // We accept incomplete strings here, since complete uses expand_string to expand incomplete
    // strings from the commandline.
    wcstring next;
    unescape_string(input, &next, UNESCAPE_SPECIAL | UNESCAPE_INCOMPLETE);

    if (flags & expand_flag::skip_variables) {
        for (auto &i : next) {
            if (i == VARIABLE_EXPAND) {
                i = L'$';
            }
        }
        append_completion(out, std::move(next));
    } else {
        size_t size = next.size();
        if (!expand_variables(std::move(next), out, size, ctx.vars, errors)) {
            return expand_result_t::make_error(STATUS_EXPAND_ERROR);
        }
    }
    return expand_result_t::ok;
}

expand_result_t expander_t::stage_braces(wcstring input, completion_list_t *out) {
    return expand_braces(std::move(input), flags, out, errors);
}

expand_result_t expander_t::stage_home_and_self(wcstring input, completion_list_t *out) {
    if (!(flags & expand_flag::skip_home_directories)) {
        expand_home_directory(input, ctx.vars);
    }
    expand_percent_self(input);
    append_completion(out, std::move(input));
    return expand_result_t::ok;
}

expand_result_t expander_t::stage_wildcards(wcstring path_to_expand, completion_list_t *out) {
    expand_result_t result = expand_result_t::ok;

    remove_internal_separator(&path_to_expand, flags & expand_flag::skip_wildcards);
    const bool has_wildcard = wildcard_has(path_to_expand, true /* internal, i.e. ANY_STRING */);
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
        wcstring_list_t effective_working_dirs;
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
                wcstring_list_t paths;
                if (auto paths_var = ctx.vars.get(for_cd ? L"CDPATH" : L"PATH")) {
                    paths = paths_var->as_list();
                }
                if (paths.empty()) {
                    paths.emplace_back(for_cd ? L"." : L"");
                }
                for (const wcstring &next_path : paths) {
                    effective_working_dirs.push_back(
                        path_apply_working_directory(next_path, working_dir));
                }
            }
        }

        result = expand_result_t::wildcard_no_match;
        completion_list_t expanded;
        for (const auto &effective_working_dir : effective_working_dirs) {
            wildcard_expand_result_t expand_res = wildcard_expand_string(
                path_to_expand, effective_working_dir, flags, ctx.cancel_checker, &expanded);
            switch (expand_res) {
                case wildcard_expand_result_t::match:
                    result = expand_result_t::ok;
                    break;
                case wildcard_expand_result_t::no_match:
                    break;
                case wildcard_expand_result_t::cancel:
                    result = expand_result_t::cancel;
                    break;
            }
        }

        std::sort(expanded.begin(), expanded.end(),
                  [&](const completion_t &a, const completion_t &b) {
                      return wcsfilecmp_glob(a.completion.c_str(), b.completion.c_str()) < 0;
                  });

        std::move(expanded.begin(), expanded.end(), std::back_inserter(*out));
    } else {
        // Can't fully justify this check. I think it's that SKIP_WILDCARDS is used when completing
        // to mean don't do file expansions, so if we're not doing file expansions, just drop this
        // completion on the floor.
        if (!(flags & expand_flag::for_completions)) {
            append_completion(out, std::move(path_to_expand));
        }
    }
    return result;
}

expand_result_t expander_t::expand_string(wcstring input, completion_list_t *out_completions,
                                          expand_flags_t flags, const operation_context_t &ctx,
                                          parse_error_list_t *errors) {
    assert(((flags & expand_flag::skip_cmdsubst) || ctx.parser) &&
           "Must have a parser if not skipping command substitutions");
    // Early out. If we're not completing, and there's no magic in the input, we're done.
    if (!(flags & expand_flag::for_completions) && expand_is_clean(input)) {
        append_completion(out_completions, std::move(input));
        return expand_result_t::ok;
    }

    expander_t expand(ctx, flags, errors);

    // Our expansion stages.
    const stage_t stages[] = {&expander_t::stage_cmdsubst, &expander_t::stage_variables,
                              &expander_t::stage_braces, &expander_t::stage_home_and_self,
                              &expander_t::stage_wildcards};

    // Load up our single initial completion.
    completion_list_t completions, output_storage;
    append_completion(&completions, input);

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
        completions.swap(output_storage);
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
        // Hack to un-expand tildes (see #647).
        if (!(flags & expand_flag::skip_home_directories)) {
            unexpand_tildes(input, ctx.vars, &completions);
        }
        out_completions->insert(out_completions->end(),
                                std::make_move_iterator(completions.begin()),
                                std::make_move_iterator(completions.end()));
    }
    return total_result;
}
}  // namespace

expand_result_t expand_string(wcstring input, completion_list_t *out_completions,
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
                                           wcstring *out_cmd, wcstring_list_t *out_args,
                                           parse_error_list_t *errors) {
    // Fast path.
    if (expand_is_clean(instr)) {
        *out_cmd = instr;
        return expand_result_t::ok;
    }

    completion_list_t completions;
    expand_result_t expand_err = expand_string(
        instr, &completions, {expand_flag::skip_cmdsubst, expand_flag::skip_jobs}, ctx, errors);
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

maybe_t<wcstring> expand_abbreviation(const wcstring &src, const environment_t &vars) {
    if (src.empty()) return none();

    wcstring esc_src = escape_string(src, 0, STRING_STYLE_VAR);
    if (esc_src.empty()) {
        return none();
    }
    wcstring var_name = L"_fish_abbr_" + esc_src;
    if (auto var_value = vars.get(var_name)) {
        return var_value->as_string();
    }
    return none();
}

std::map<wcstring, wcstring> get_abbreviations(const environment_t &vars) {
    const wcstring prefix = L"_fish_abbr_";
    auto names = vars.get_names(0);
    std::map<wcstring, wcstring> result;
    for (const wcstring &name : names) {
        if (string_prefixes_string(prefix, name)) {
            wcstring key;
            unescape_string(name.substr(prefix.size()), &key, UNESCAPE_DEFAULT, STRING_STYLE_VAR);
            result[key] = vars.get(name)->as_string();
        }
    }
    return result;
}
