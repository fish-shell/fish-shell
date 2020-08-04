// Functions for syntax highlighting.
#include "config.h"  // IWYU pragma: keep

// IWYU pragma: no_include <cstddef>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cwchar>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "ast.h"
#include "builtin.h"
#include "color.h"
#include "common.h"
#include "env.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "function.h"
#include "future_feature_flags.h"
#include "highlight.h"
#include "history.h"
#include "output.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "parser.h"
#include "path.h"
#include "tokenizer.h"
#include "wcstringutil.h"
#include "wildcard.h"
#include "wutil.h"  // IWYU pragma: keep

#define CURSOR_POSITION_INVALID static_cast<size_t>(-1)

static const wchar_t *get_highlight_var_name(highlight_role_t role) {
    switch (role) {
        case highlight_role_t::normal:
            return L"fish_color_normal";
        case highlight_role_t::error:
            return L"fish_color_error";
        case highlight_role_t::command:
            return L"fish_color_command";
        case highlight_role_t::statement_terminator:
            return L"fish_color_end";
        case highlight_role_t::param:
            return L"fish_color_param";
        case highlight_role_t::comment:
            return L"fish_color_comment";
        case highlight_role_t::search_match:
            return L"fish_color_search_match";
        case highlight_role_t::operat:
            return L"fish_color_operator";
        case highlight_role_t::escape:
            return L"fish_color_escape";
        case highlight_role_t::quote:
            return L"fish_color_quote";
        case highlight_role_t::redirection:
            return L"fish_color_redirection";
        case highlight_role_t::autosuggestion:
            return L"fish_color_autosuggestion";
        case highlight_role_t::selection:
            return L"fish_color_selection";
        case highlight_role_t::pager_progress:
            return L"fish_pager_color_progress";
        case highlight_role_t::pager_background:
            return L"fish_pager_color_background";
        case highlight_role_t::pager_prefix:
            return L"fish_pager_color_prefix";
        case highlight_role_t::pager_completion:
            return L"fish_pager_color_completion";
        case highlight_role_t::pager_description:
            return L"fish_pager_color_description";
        case highlight_role_t::pager_secondary_background:
            return L"fish_pager_color_secondary_background";
        case highlight_role_t::pager_secondary_prefix:
            return L"fish_pager_color_secondary_prefix";
        case highlight_role_t::pager_secondary_completion:
            return L"fish_pager_color_secondary_completion";
        case highlight_role_t::pager_secondary_description:
            return L"fish_pager_color_secondary_description";
        case highlight_role_t::pager_selected_background:
            return L"fish_pager_color_selected_background";
        case highlight_role_t::pager_selected_prefix:
            return L"fish_pager_color_selected_prefix";
        case highlight_role_t::pager_selected_completion:
            return L"fish_pager_color_selected_completion";
        case highlight_role_t::pager_selected_description:
            return L"fish_pager_color_selected_description";
    }
    DIE("invalid highlight role");
}

// Table used to fetch fallback highlights in case the specified one
// wasn't set.
static highlight_role_t get_fallback(highlight_role_t role) {
    switch (role) {
        case highlight_role_t::normal:
            return highlight_role_t::normal;
        case highlight_role_t::error:
            return highlight_role_t::normal;
        case highlight_role_t::command:
            return highlight_role_t::normal;
        case highlight_role_t::statement_terminator:
            return highlight_role_t::normal;
        case highlight_role_t::param:
            return highlight_role_t::normal;
        case highlight_role_t::comment:
            return highlight_role_t::normal;
        case highlight_role_t::search_match:
            return highlight_role_t::normal;
        case highlight_role_t::operat:
            return highlight_role_t::normal;
        case highlight_role_t::escape:
            return highlight_role_t::normal;
        case highlight_role_t::quote:
            return highlight_role_t::normal;
        case highlight_role_t::redirection:
            return highlight_role_t::normal;
        case highlight_role_t::autosuggestion:
            return highlight_role_t::normal;
        case highlight_role_t::selection:
            return highlight_role_t::normal;
        case highlight_role_t::pager_progress:
            return highlight_role_t::normal;
        case highlight_role_t::pager_background:
            return highlight_role_t::normal;
        case highlight_role_t::pager_prefix:
            return highlight_role_t::normal;
        case highlight_role_t::pager_completion:
            return highlight_role_t::normal;
        case highlight_role_t::pager_description:
            return highlight_role_t::normal;
        case highlight_role_t::pager_secondary_background:
            return highlight_role_t::pager_background;
        case highlight_role_t::pager_secondary_prefix:
            return highlight_role_t::pager_prefix;
        case highlight_role_t::pager_secondary_completion:
            return highlight_role_t::pager_completion;
        case highlight_role_t::pager_secondary_description:
            return highlight_role_t::pager_description;
        case highlight_role_t::pager_selected_background:
            return highlight_role_t::search_match;
        case highlight_role_t::pager_selected_prefix:
            return highlight_role_t::pager_prefix;
        case highlight_role_t::pager_selected_completion:
            return highlight_role_t::pager_completion;
        case highlight_role_t::pager_selected_description:
            return highlight_role_t::pager_description;
    }
    DIE("invalid highlight role");
}

/// Determine if the filesystem containing the given fd is case insensitive for lookups regardless
/// of whether it preserves the case when saving a pathname.
///
/// Returns:
///     false: the filesystem is not case insensitive
///     true: the file system is case insensitive
using case_sensitivity_cache_t = std::unordered_map<wcstring, bool>;
bool fs_is_case_insensitive(const wcstring &path, int fd,
                            case_sensitivity_cache_t &case_sensitivity_cache) {
    bool result = false;
#ifdef _PC_CASE_SENSITIVE
    // Try the cache first.
    case_sensitivity_cache_t::iterator cache = case_sensitivity_cache.find(path);
    if (cache != case_sensitivity_cache.end()) {
        /* Use the cached value */
        result = cache->second;
    } else {
        // Ask the system. A -1 value means error (so assume case sensitive), a 1 value means case
        // sensitive, and a 0 value means case insensitive.
        long ret = fpathconf(fd, _PC_CASE_SENSITIVE);
        result = (ret == 0);
        case_sensitivity_cache[path] = result;
    }
#else
    // Silence lint tools about the unused parameters.
    UNUSED(path);
    UNUSED(fd);
    UNUSED(case_sensitivity_cache);
#endif
    return result;
}

/// Tests whether the specified string cpath is the prefix of anything we could cd to. directories
/// is a list of possible parent directories (typically either the working directory, or the
/// cdpath). This does I/O!
///
/// Hack: if out_suggested_cdpath is not NULL, it returns the autosuggestion for cd. This descends
/// the deepest unique directory hierarchy.
///
/// We expect the path to already be unescaped.
bool is_potential_path(const wcstring &potential_path_fragment, const wcstring_list_t &directories,
                       const operation_context_t &ctx, path_flags_t flags) {
    ASSERT_IS_BACKGROUND_THREAD();

    if (ctx.check_cancel()) return false;

    const bool require_dir = static_cast<bool>(flags & PATH_REQUIRE_DIR);
    wcstring clean_potential_path_fragment;
    int has_magic = 0;

    wcstring path_with_magic(potential_path_fragment);
    if (flags & PATH_EXPAND_TILDE) expand_tilde(path_with_magic, ctx.vars);

    for (auto c : path_with_magic) {
        switch (c) {
            case PROCESS_EXPAND_SELF:
            case VARIABLE_EXPAND:
            case VARIABLE_EXPAND_SINGLE:
            case BRACE_BEGIN:
            case BRACE_END:
            case BRACE_SEP:
            case ANY_CHAR:
            case ANY_STRING:
            case ANY_STRING_RECURSIVE: {
                has_magic = 1;
                break;
            }
            case INTERNAL_SEPARATOR: {
                break;
            }
            default: {
                clean_potential_path_fragment.push_back(c);
                break;
            }
        }
    }

    if (has_magic || clean_potential_path_fragment.empty()) {
        return false;
    }

    // Don't test the same path multiple times, which can happen if the path is absolute and the
    // CDPATH contains multiple entries.
    std::unordered_set<wcstring> checked_paths;

    // Keep a cache of which paths / filesystems are case sensitive.
    case_sensitivity_cache_t case_sensitivity_cache;

    for (const wcstring &wd : directories) {
        if (ctx.check_cancel()) return false;
        wcstring abs_path = path_apply_working_directory(clean_potential_path_fragment, wd);
        if (flags & PATH_FOR_CD) {
            abs_path = normalize_path(abs_path);
        }

        // Skip this if it's empty or we've already checked it.
        if (abs_path.empty() || checked_paths.count(abs_path)) continue;
        checked_paths.insert(abs_path);

        // If we end with a slash, then it must be a directory.
        bool must_be_full_dir = abs_path.at(abs_path.size() - 1) == L'/';
        if (must_be_full_dir) {
            struct stat buf;
            if (0 == wstat(abs_path, &buf) && S_ISDIR(buf.st_mode)) {
                return true;
            }
        } else {
            // We do not end with a slash; it does not have to be a directory.
            DIR *dir = nullptr;
            const wcstring dir_name = wdirname(abs_path);
            const wcstring filename_fragment = wbasename(abs_path);
            if (dir_name == L"/" && filename_fragment == L"/") {
                // cd ///.... No autosuggestion.
                return true;
            } else if ((dir = wopendir(dir_name))) {
                cleanup_t cleanup_dir([&] { closedir(dir); });

                // Check if we're case insensitive.
                const bool do_case_insensitive =
                    fs_is_case_insensitive(dir_name, dirfd(dir), case_sensitivity_cache);

                // We opened the dir_name; look for a string where the base name prefixes it Don't
                // ask for the is_dir value unless we care, because it can cause extra filesystem
                // access.
                wcstring ent;
                bool is_dir = false;
                while (wreaddir_resolving(dir, dir_name, ent, require_dir ? &is_dir : nullptr)) {
                    if (ctx.check_cancel()) return false;

                    // Maybe skip directories.
                    if (require_dir && !is_dir) {
                        continue;
                    }

                    if (string_prefixes_string(filename_fragment, ent) ||
                        (do_case_insensitive &&
                         string_prefixes_string_case_insensitive(filename_fragment, ent))) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

// Given a string, return whether it prefixes a path that we could cd into. Return that path in
// out_path. Expects path to be unescaped.
static bool is_potential_cd_path(const wcstring &path, const wcstring &working_directory,
                                 const operation_context_t &ctx, path_flags_t flags) {
    wcstring_list_t directories;

    if (string_prefixes_string(L"./", path)) {
        // Ignore the CDPATH in this case; just use the working directory.
        directories.push_back(working_directory);
    } else {
        // Get the CDPATH.
        auto cdpath = ctx.vars.get(L"CDPATH");
        wcstring_list_t pathsv =
            cdpath.missing_or_empty() ? wcstring_list_t{L"."} : cdpath->as_list();

        for (auto next_path : pathsv) {
            if (next_path.empty()) next_path = L".";
            // Ensure that we use the working directory for relative cdpaths like ".".
            directories.push_back(path_apply_working_directory(next_path, working_directory));
        }
    }

    // Call is_potential_path with all of these directories.
    return is_potential_path(path, directories, ctx, flags | PATH_REQUIRE_DIR | PATH_FOR_CD);
}

// Given a plain statement node in a parse tree, get the command and return it, expanded
// appropriately for commands. If we succeed, return true.
static bool statement_get_expanded_command(const wcstring &src,
                                           const ast::decorated_statement_t &stmt,
                                           const operation_context_t &ctx, wcstring *out_cmd) {
    // Get the command. Try expanding it. If we cannot, it's an error.
    maybe_t<wcstring> cmd = stmt.command.source(src);
    if (!cmd) return false;
    expand_result_t err = expand_to_command_and_args(*cmd, ctx, out_cmd, nullptr);
    return err == expand_result_t::ok;
}

rgb_color_t highlight_color_resolver_t::resolve_spec_uncached(const highlight_spec_t &highlight,
                                                              bool is_background,
                                                              const environment_t &vars) const {
    rgb_color_t result = rgb_color_t::normal();
    highlight_role_t role = is_background ? highlight.background : highlight.foreground;

    auto var = vars.get(get_highlight_var_name(role));
    if (!var) var = vars.get(get_highlight_var_name(get_fallback(role)));
    if (!var) var = vars.get(get_highlight_var_name(highlight_role_t::normal));
    if (var) result = parse_color(*var, is_background);

    // Handle modifiers.
    if (!is_background && highlight.valid_path) {
        auto var2 = vars.get(L"fish_color_valid_path");
        if (var2) {
            rgb_color_t result2 = parse_color(*var2, is_background);
            if (result.is_normal())
                result = result2;
            else {
                if (result2.is_bold()) result.set_bold(true);
                if (result2.is_underline()) result.set_underline(true);
                if (result2.is_italics()) result.set_italics(true);
                if (result2.is_dim()) result.set_dim(true);
                if (result2.is_reverse()) result.set_reverse(true);
            }
        }
    }

    if (!is_background && highlight.force_underline) {
        result.set_underline(true);
    }

    return result;
}

rgb_color_t highlight_color_resolver_t::resolve_spec(const highlight_spec_t &highlight,
                                                     bool is_background,
                                                     const environment_t &vars) {
    auto &cache = is_background ? bg_cache_ : fg_cache_;
    auto p = cache.insert(std::make_pair(highlight, rgb_color_t{}));
    auto iter = p.first;
    bool did_insert = p.second;
    if (did_insert) {
        // Insertion happened, meaning the cache needs to be populated.
        iter->second = resolve_spec_uncached(highlight, is_background, vars);
    }
    return iter->second;
}

static bool command_is_valid(const wcstring &cmd, enum statement_decoration_t decoration,
                             const wcstring &working_directory, const environment_t &vars);

static bool has_expand_reserved(const wcstring &str) {
    bool result = false;
    for (auto wc : str) {
        if (wc >= EXPAND_RESERVED_BASE && wc <= EXPAND_RESERVED_END) {
            result = true;
            break;
        }
    }
    return result;
}

// Parse a command line. Return by reference the first command, and the first argument to that
// command (as a string), if any. This is used to validate autosuggestions.
static void autosuggest_parse_command(const wcstring &buff, const operation_context_t &ctx,
                                      wcstring *out_expanded_command, wcstring *out_arg) {
    auto ast = ast::ast_t::parse(
        buff, parse_flag_continue_after_error | parse_flag_accept_incomplete_tokens);

    // Find the first statement.
    const ast::decorated_statement_t *first_statement = nullptr;
    if (const ast::job_conjunction_t *jc = ast.top()->as<ast::job_list_t>()->at(0)) {
        first_statement = jc->job.statement.contents->try_as<ast::decorated_statement_t>();
    }

    if (first_statement &&
        statement_get_expanded_command(buff, *first_statement, ctx, out_expanded_command)) {
        // Check if the first argument or redirection is, in fact, an argument.
        if (const auto *arg_or_redir = first_statement->args_or_redirs.at(0)) {
            if (arg_or_redir && arg_or_redir->is_argument()) {
                *out_arg = arg_or_redir->argument().source(buff);
            }
        }
    }
}

bool autosuggest_validate_from_history(const history_item_t &item,
                                       const wcstring &working_directory,
                                       const operation_context_t &ctx) {
    ASSERT_IS_BACKGROUND_THREAD();

    bool handled = false;
    bool suggestion_ok = false;

    // Parse the string.
    wcstring parsed_command;
    wcstring cd_dir;
    autosuggest_parse_command(item.str(), ctx, &parsed_command, &cd_dir);

    // This is for autosuggestions which are not decorated commands, e.g. function declarations.
    if (parsed_command.empty()) {
        return true;
    }

    if (parsed_command == L"cd" && !cd_dir.empty()) {
        // We can possibly handle this specially.
        if (expand_one(cd_dir, expand_flag::skip_cmdsubst, ctx)) {
            handled = true;
            bool is_help =
                string_prefixes_string(cd_dir, L"--help") || string_prefixes_string(cd_dir, L"-h");
            if (!is_help) {
                auto path = path_get_cdpath(cd_dir, working_directory, ctx.vars);
                if (path && !paths_are_same_file(working_directory, *path)) {
                    suggestion_ok = true;
                }
            }
        }
    }

    if (handled) {
        return suggestion_ok;
    }

    // Not handled specially so handle it here.
    bool cmd_ok = builtin_exists(parsed_command) || function_exists_no_autoload(parsed_command) ||
                  path_get_path(parsed_command, nullptr, ctx.vars);

    if (cmd_ok) {
        const path_list_t &paths = item.get_required_paths();
        suggestion_ok = all_paths_are_valid(paths, working_directory);
    }

    return suggestion_ok;
}

// Highlights the variable starting with 'in', setting colors within the 'colors' array. Returns the
// number of characters consumed.
static size_t color_variable(const wchar_t *in, size_t in_len,
                             std::vector<highlight_spec_t>::iterator colors) {
    assert(in_len > 0);
    assert(in[0] == L'$');

    // Handle an initial run of $s.
    size_t idx = 0;
    size_t dollar_count = 0;
    while (in[idx] == '$') {
        // Our color depends on the next char.
        wchar_t next = in[idx + 1];
        if (next == L'$' || valid_var_name_char(next)) {
            colors[idx] = highlight_role_t::operat;
        } else {
            colors[idx] = highlight_role_t::error;
        }
        idx++;
        dollar_count++;
    }

    // Handle a sequence of variable characters.
    while (valid_var_name_char(in[idx])) {
        colors[idx++] = highlight_role_t::operat;
    }

    // Handle a slice, up to dollar_count of them. Note that we currently don't do any validation of
    // the slice's contents, e.g. $foo[blah] will not show an error even though it's invalid.
    for (size_t slice_count = 0; slice_count < dollar_count && in[idx] == L'['; slice_count++) {
        wchar_t *slice_begin = nullptr, *slice_end = nullptr;
        int located = parse_util_locate_slice(in + idx, &slice_begin, &slice_end, false);
        if (located == 1) {
            size_t slice_begin_idx = slice_begin - in, slice_end_idx = slice_end - in;
            assert(slice_end_idx > slice_begin_idx);
            colors[slice_begin_idx] = highlight_role_t::operat;
            colors[slice_end_idx] = highlight_role_t::operat;
            idx = slice_end_idx + 1;
        } else if (located == 0) {
            // not a slice
            break;
        } else {
            assert(located < 0);
            // Syntax error. Normally the entire token is colored red for us, but inside a
            // double-quoted string that doesn't happen. As such, color the variable + the slice
            // start red. Coloring any more than that looks bad, unless we're willing to try and
            // detect where the double-quoted string ends, and I'd rather not do that.
            std::fill(colors, colors + idx + 1, highlight_role_t::error);
            break;
        }
    }
    return idx;
}

/// This function is a disaster badly in need of refactoring. It colors an argument or command,
/// without regard to command substitutions.
static void color_string_internal(const wcstring &buffstr, highlight_spec_t base_color,
                                  std::vector<highlight_spec_t>::iterator colors) {
    // Clarify what we expect.
    assert((base_color == highlight_role_t::param || base_color == highlight_role_t::command) &&
           "Unexpected base color");
    const size_t buff_len = buffstr.size();
    std::fill(colors, colors + buff_len, base_color);

    // Hacky support for %self which must be an unquoted literal argument.
    if (buffstr == PROCESS_EXPAND_SELF_STR) {
        std::fill_n(colors, std::wcslen(PROCESS_EXPAND_SELF_STR), highlight_role_t::operat);
        return;
    }

    enum { e_unquoted, e_single_quoted, e_double_quoted } mode = e_unquoted;
    maybe_t<size_t> unclosed_quote_offset;
    int bracket_count = 0;
    for (size_t in_pos = 0; in_pos < buff_len; in_pos++) {
        const wchar_t c = buffstr.at(in_pos);
        switch (mode) {
            case e_unquoted: {
                if (c == L'\\') {
                    auto fill_color = highlight_role_t::escape;  // may be set to highlight_error
                    const size_t backslash_pos = in_pos;
                    size_t fill_end = backslash_pos;

                    // Move to the escaped character.
                    in_pos++;
                    const wchar_t escaped_char = (in_pos < buff_len ? buffstr.at(in_pos) : L'\0');

                    if (escaped_char == L'\0') {
                        fill_end = in_pos;
                        fill_color = highlight_role_t::error;
                    } else if (std::wcschr(L"~%", escaped_char)) {
                        if (in_pos == 1) {
                            fill_end = in_pos + 1;
                        }
                    } else if (escaped_char == L',') {
                        if (bracket_count) {
                            fill_end = in_pos + 1;
                        }
                    } else if (std::wcschr(L"abefnrtv*?$(){}[]'\"<>^ \\#;|&", escaped_char)) {
                        fill_end = in_pos + 1;
                    } else if (std::wcschr(L"c", escaped_char)) {
                        // Like \ci. So highlight three characters.
                        fill_end = in_pos + 1;
                    } else if (std::wcschr(L"uUxX01234567", escaped_char)) {
                        long long res = 0;
                        int chars = 2;
                        int base = 16;
                        wchar_t max_val = ASCII_MAX;

                        switch (escaped_char) {
                            case L'u': {
                                chars = 4;
                                max_val = UCS2_MAX;
                                in_pos++;
                                break;
                            }
                            case L'U': {
                                chars = 8;
                                max_val = WCHAR_MAX;
                                in_pos++;
                                break;
                            }
                            case L'x': {
                                in_pos++;
                                break;
                            }
                            case L'X': {
                                max_val = BYTE_MAX;
                                in_pos++;
                                break;
                            }
                            default: {
                                // a digit like \12
                                base = 8;
                                chars = 3;
                                break;
                            }
                        }

                        // Consume
                        for (int i = 0; i < chars && in_pos < buff_len; i++) {
                            long d = convert_digit(buffstr.at(in_pos), base);
                            if (d < 0) break;
                            res = (res * base) + d;
                            in_pos++;
                        }
                        // in_pos is now at the first character that could not be converted (or
                        // buff_len).
                        assert(in_pos >= backslash_pos && in_pos <= buff_len);
                        fill_end = in_pos;

                        // It's an error if we exceeded the max value.
                        if (res > max_val) fill_color = highlight_role_t::error;

                        // Subtract one from in_pos, so that the increment in the loop will move to
                        // the next character.
                        in_pos--;
                    }
                    assert(fill_end >= backslash_pos);
                    std::fill(colors + backslash_pos, colors + fill_end, fill_color);
                } else {
                    // Not a backslash.
                    switch (c) {
                        case L'~': {
                            if (in_pos == 0) {
                                colors[in_pos] = highlight_role_t::operat;
                            }
                            break;
                        }
                        case L'$': {
                            assert(in_pos < buff_len);
                            in_pos += color_variable(buffstr.c_str() + in_pos, buff_len - in_pos,
                                                     colors + in_pos);
                            // Subtract one to account for the upcoming loop increment.
                            in_pos -= 1;
                            break;
                        }
                        case L'?': {
                            if (!feature_test(features_t::qmark_noglob)) {
                                colors[in_pos] = highlight_role_t::operat;
                            }
                            break;
                        }
                        case L'*':
                        case L'(':
                        case L')': {
                            colors[in_pos] = highlight_role_t::operat;
                            break;
                        }
                        case L'{': {
                            colors[in_pos] = highlight_role_t::operat;
                            bracket_count++;
                            break;
                        }
                        case L'}': {
                            colors[in_pos] = highlight_role_t::operat;
                            bracket_count--;
                            break;
                        }
                        case L',': {
                            if (bracket_count > 0) {
                                colors[in_pos] = highlight_role_t::operat;
                            }
                            break;
                        }
                        case L'\'': {
                            colors[in_pos] = highlight_role_t::quote;
                            unclosed_quote_offset = in_pos;
                            mode = e_single_quoted;
                            break;
                        }
                        case L'\"': {
                            colors[in_pos] = highlight_role_t::quote;
                            unclosed_quote_offset = in_pos;
                            mode = e_double_quoted;
                            break;
                        }
                        default: {
                            break;  // we ignore all other characters
                        }
                    }
                }
                break;
            }
            // Mode 1 means single quoted string, i.e 'foo'.
            case e_single_quoted: {
                colors[in_pos] = highlight_role_t::quote;
                if (c == L'\\') {
                    // backslash
                    if (in_pos + 1 < buff_len) {
                        const wchar_t escaped_char = buffstr.at(in_pos + 1);
                        if (escaped_char == L'\\' || escaped_char == L'\'') {
                            colors[in_pos] = highlight_role_t::escape;      // backslash
                            colors[in_pos + 1] = highlight_role_t::escape;  // escaped char
                            in_pos += 1;                                    // skip over backslash
                        }
                    }
                } else if (c == L'\'') {
                    unclosed_quote_offset = none();
                    mode = e_unquoted;
                }
                break;
            }
            // Mode 2 means double quoted string, i.e. "foo".
            case e_double_quoted: {
                // Slices are colored in advance, past `in_pos`, and we don't want to overwrite
                // that.
                if (colors[in_pos] == base_color) {
                    colors[in_pos] = highlight_role_t::quote;
                }
                switch (c) {
                    case L'"': {
                        unclosed_quote_offset = none();
                        mode = e_unquoted;
                        break;
                    }
                    case L'\\': {
                        // Backslash
                        if (in_pos + 1 < buff_len) {
                            const wchar_t escaped_char = buffstr.at(in_pos + 1);
                            if (std::wcschr(L"\\\"\n$", escaped_char)) {
                                colors[in_pos] = highlight_role_t::escape;      // backslash
                                colors[in_pos + 1] = highlight_role_t::escape;  // escaped char
                                in_pos += 1;  // skip over backslash
                            }
                        }
                        break;
                    }
                    case L'$': {
                        in_pos += color_variable(buffstr.c_str() + in_pos, buff_len - in_pos,
                                                 colors + in_pos);
                        // Subtract one to account for the upcoming increment in the loop.
                        in_pos -= 1;
                        break;
                    }
                    default: {
                        break;  // we ignore all other characters
                    }
                }
                break;
            }
        }
    }

    // Error on unclosed quotes.
    if (unclosed_quote_offset) {
        colors[*unclosed_quote_offset] = highlight_role_t::error;
    }
}

/// Syntax highlighter helper.
class highlighter_t {
    // The string we're highlighting. Note this is a reference memmber variable (to avoid copying)!
    // We must not outlive this!
    const wcstring &buff;
    // Cursor position.
    const size_t cursor_pos;
    // The operation context. Again, a reference member variable!
    const operation_context_t &ctx;
    // Whether it's OK to do I/O.
    const bool io_ok;
    // Working directory.
    const wcstring working_directory;
    // The ast we produced.
    ast::ast_t ast;
    // The resulting colors.
    using color_array_t = std::vector<highlight_spec_t>;
    color_array_t color_array;

    // Flags we use for AST parsing.
    static constexpr parse_tree_flags_t ast_flags =
        parse_flag_continue_after_error | parse_flag_include_comments |
        parse_flag_accept_incomplete_tokens | parse_flag_leave_unterminated |
        parse_flag_show_extra_semis;

    // Color a command.
    void color_command(const ast::string_t &node);
    // Color a node as if it were an argument.
    void color_as_argument(const ast::node_t &node);
    // Color a redirection.
    void color_redirection(const ast::redirection_t &node);
    // Color all the children of the command with the given type.
    void color_children(const ast::node_t &parent, ast::type_t type, highlight_spec_t color);
    // Colors the source range of a node with a given color.
    void color_node(const ast::node_t &node, highlight_spec_t color);
    // Colors a range with a given color.
    void color_range(source_range_t range, highlight_spec_t color);
    // return whether a plain statement is 'cd'.
    bool is_cd(const ast::decorated_statement_t &stmt) const;

    /// \return a substring of our buffer.
    wcstring get_source(source_range_t r) const;

   public:
    // Visit the children of a node.
    void visit_children(const ast::node_t &node) {
        ast::node_visitor(*this).accept_children_of(&node);
    }

    // AST visitor implementations.
    void visit(const ast::keyword_base_t &kw);
    void visit(const ast::token_base_t &tok);
    void visit(const ast::redirection_t &redir);
    void visit(const ast::variable_assignment_t &varas);
    void visit(const ast::semi_nl_t &semi_nl);
    void visit(const ast::decorated_statement_t &stmt);

    // Visit an argument, perhaps knowing that our command is cd.
    void visit(const ast::argument_t &arg, bool cmd_is_cd = false);

    // Default implementation is to just visit children.
    void visit(const ast::node_t &node) { visit_children(node); }

    // Constructor
    highlighter_t(const wcstring &str, size_t pos, const operation_context_t &ctx, wcstring wd,
                  bool can_do_io)
        : buff(str),
          cursor_pos(pos),
          ctx(ctx),
          io_ok(can_do_io),
          working_directory(std::move(wd)),
          ast(ast::ast_t::parse(buff, ast_flags)) {}

    // Perform highlighting, returning an array of colors.
    color_array_t highlight();
};

wcstring highlighter_t::get_source(source_range_t r) const {
    assert(r.start + r.length >= r.start && "Overflow");
    assert(r.start + r.length <= this->buff.size() && "Out of range");
    return this->buff.substr(r.start, r.length);
}

void highlighter_t::color_node(const ast::node_t &node, highlight_spec_t color) {
    color_range(node.source_range(), color);
}

void highlighter_t::color_range(source_range_t range, highlight_spec_t color) {
    assert(range.start + range.length <= this->color_array.size() && "Range out of bounds");
    std::fill_n(this->color_array.begin() + range.start, range.length, color);
}

void highlighter_t::color_command(const ast::string_t &node) {
    source_range_t source_range = node.source_range();
    const wcstring cmd_str = get_source(source_range);

    // Get an iterator to the colors associated with the argument.
    const size_t arg_start = source_range.start;
    const color_array_t::iterator colors = color_array.begin() + arg_start;
    color_string_internal(cmd_str, highlight_role_t::command, colors);
}

// node does not necessarily have type symbol_argument here.
void highlighter_t::color_as_argument(const ast::node_t &node) {
    auto source_range = node.source_range();
    const wcstring arg_str = get_source(source_range);

    // Get an iterator to the colors associated with the argument.
    const size_t arg_start = source_range.start;
    const color_array_t::iterator arg_colors = color_array.begin() + arg_start;

    // Color this argument without concern for command substitutions.
    color_string_internal(arg_str, highlight_role_t::param, arg_colors);

    // Now do command substitutions.
    size_t cmdsub_cursor = 0, cmdsub_start = 0, cmdsub_end = 0;
    wcstring cmdsub_contents;
    while (parse_util_locate_cmdsubst_range(arg_str, &cmdsub_cursor, &cmdsub_contents,
                                            &cmdsub_start, &cmdsub_end,
                                            true /* accept incomplete */) > 0) {
        // The cmdsub_start is the open paren. cmdsub_end is either the close paren or the end of
        // the string. cmdsub_contents extends from one past cmdsub_start to cmdsub_end.
        assert(cmdsub_end > cmdsub_start);
        assert(cmdsub_end - cmdsub_start - 1 == cmdsub_contents.size());

        // Found a command substitution. Compute the position of the start and end of the cmdsub
        // contents, within our overall src.
        const size_t arg_subcmd_start = arg_start + cmdsub_start,
                     arg_subcmd_end = arg_start + cmdsub_end;

        // Highlight the parens. The open paren must exist; the closed paren may not if it was
        // incomplete.
        assert(cmdsub_start < arg_str.size());
        this->color_array.at(arg_subcmd_start) = highlight_role_t::operat;
        if (arg_subcmd_end < this->buff.size())
            this->color_array.at(arg_subcmd_end) = highlight_role_t::operat;

        // Compute the cursor's position within the cmdsub. We must be past the open paren (hence >)
        // but can be at the end of the string or closed paren (hence <=).
        size_t cursor_subpos = CURSOR_POSITION_INVALID;
        if (cursor_pos != CURSOR_POSITION_INVALID && cursor_pos > arg_subcmd_start &&
            cursor_pos <= arg_subcmd_end) {
            // The -1 because the cmdsub_contents does not include the open paren.
            cursor_subpos = cursor_pos - arg_subcmd_start - 1;
        }

        // Highlight it recursively.
        highlighter_t cmdsub_highlighter(cmdsub_contents, cursor_subpos, this->ctx,
                                         this->working_directory, this->io_ok);
        const color_array_t &subcolors = cmdsub_highlighter.highlight();

        // Copy out the subcolors back into our array.
        assert(subcolors.size() == cmdsub_contents.size());
        std::copy(subcolors.begin(), subcolors.end(),
                  this->color_array.begin() + arg_subcmd_start + 1);
    }
}

/// Indicates whether the source range of the given node forms a valid path in the given
/// working_directory.
static bool range_is_potential_path(const wcstring &src, const source_range_t &range,
                                    const operation_context_t &ctx,
                                    const wcstring &working_directory) {
    // Get the node source, unescape it, and then pass it to is_potential_path along with the
    // working directory (as a one element list).
    bool result = false;
    wcstring token = src.substr(range.start, range.length);
    if (unescape_string_in_place(&token, UNESCAPE_SPECIAL)) {
        // Big hack: is_potential_path expects a tilde, but unescape_string gives us HOME_DIRECTORY.
        // Put it back.
        if (!token.empty() && token.at(0) == HOME_DIRECTORY) token.at(0) = L'~';

        const wcstring_list_t working_directory_list(1, working_directory);
        result = is_potential_path(token, working_directory_list, ctx, PATH_EXPAND_TILDE);
    }
    return result;
}

bool highlighter_t::is_cd(const ast::decorated_statement_t &stmt) const {
    wcstring cmd_str;
    if (this->io_ok && statement_get_expanded_command(this->buff, stmt, ctx, &cmd_str)) {
        return cmd_str == L"cd";
    }
    return false;
}

void highlighter_t::visit(const ast::keyword_base_t &kw) {
    highlight_role_t role = highlight_role_t::normal;
    switch (kw.kw) {
        case parse_keyword_t::kw_begin:
        case parse_keyword_t::kw_builtin:
        case parse_keyword_t::kw_case:
        case parse_keyword_t::kw_command:
        case parse_keyword_t::kw_else:
        case parse_keyword_t::kw_end:
        case parse_keyword_t::kw_exec:
        case parse_keyword_t::kw_for:
        case parse_keyword_t::kw_function:
        case parse_keyword_t::kw_if:
        case parse_keyword_t::kw_in:
        case parse_keyword_t::kw_switch:
        case parse_keyword_t::kw_while:
            role = highlight_role_t::command;
            break;

        case parse_keyword_t::kw_and:
        case parse_keyword_t::kw_or:
        case parse_keyword_t::kw_not:
        case parse_keyword_t::kw_exclam:
        case parse_keyword_t::kw_time:
            role = highlight_role_t::operat;
            break;

        case parse_keyword_t::none:
            break;
    }
    color_node(kw, role);
}

void highlighter_t::visit(const ast::token_base_t &tok) {
    maybe_t<highlight_role_t> role = highlight_role_t::normal;
    switch (tok.type) {
        case parse_token_type_t::end:
        case parse_token_type_t::pipe:
        case parse_token_type_t::background:
            role = highlight_role_t::statement_terminator;
            break;

        case parse_token_type_t::andand:
        case parse_token_type_t::oror:
            role = highlight_role_t::operat;
            break;

        case parse_token_type_t::string:
            // Assume all strings are params. This handles e.g. the variables a for header or
            // function header. Other strings (like arguments to commands) need more complex
            // handling, which occurs in their respective overrides of visit().
            role = highlight_role_t::param;

        default:
            break;
    }
    if (role) color_node(tok, *role);
}

void highlighter_t::visit(const ast::semi_nl_t &semi_nl) {
    color_node(semi_nl, highlight_role_t::statement_terminator);
}

void highlighter_t::visit(const ast::argument_t &arg, bool cmd_is_cd) {
    color_as_argument(arg);
    if (cmd_is_cd && io_ok) {
        // Mark this as an error if it's not 'help' and not a valid cd path.
        wcstring param = arg.source(this->buff);
        if (expand_one(param, expand_flag::skip_cmdsubst, ctx)) {
            bool is_help =
                string_prefixes_string(param, L"--help") || string_prefixes_string(param, L"-h");
            if (!is_help && this->io_ok &&
                !is_potential_cd_path(param, working_directory, ctx, PATH_EXPAND_TILDE)) {
                this->color_node(arg, highlight_role_t::error);
            }
        }
    }
}

void highlighter_t::visit(const ast::variable_assignment_t &varas) {
    color_as_argument(varas);
    // TODO: Color the '=' in the variable assignment as an operator, for fun.
    //    if (auto where = variable_assignment_equals_pos(varas.source(this->buff))) {
    //        this->color_array.at(*where) = highlight_role_t::operat;
    //    }
}

void highlighter_t::visit(const ast::decorated_statement_t &stmt) {
    // Color any decoration.
    if (stmt.opt_decoration) this->visit(*stmt.opt_decoration);

    // Color the command's source code.
    // If we get no source back, there's nothing to color.
    maybe_t<wcstring> cmd = stmt.command.try_source(this->buff);
    if (!cmd.has_value()) return;

    wcstring expanded_cmd;
    bool is_valid_cmd = false;
    if (!this->io_ok) {
        // We cannot check if the command is invalid, so just assume it's valid.
        is_valid_cmd = true;
    } else if (variable_assignment_equals_pos(*cmd)) {
        is_valid_cmd = true;
    } else {
        // Check to see if the command is valid.
        // Try expanding it. If we cannot, it's an error.
        bool expanded = statement_get_expanded_command(buff, stmt, ctx, &expanded_cmd);
        if (expanded && !has_expand_reserved(expanded_cmd)) {
            is_valid_cmd =
                command_is_valid(expanded_cmd, stmt.decoration(), working_directory, ctx.vars);
        }
    }

    // Color our statement.
    if (is_valid_cmd) {
        this->color_command(stmt.command);
    } else {
        this->color_node(stmt.command, highlight_role_t::error);
    }

    // Color arguments and redirections.
    // Except if our command is 'cd' we have special logic for how arguments are colored.
    bool is_cd = (expanded_cmd == L"cd");
    for (const ast::argument_or_redirection_t &v : stmt.args_or_redirs) {
        if (v.is_argument()) {
            this->visit(v.argument(), is_cd);
        } else {
            this->visit(v.redirection());
        }
    }
}

/// \return whether a string contains a command substitution.
static bool has_cmdsub(const wcstring &src) {
    size_t cursor = 0;
    size_t start = 0;
    size_t end = 0;
    return parse_util_locate_cmdsubst_range(src, &cursor, nullptr, &start, &end, true) != 0;
}

void highlighter_t::visit(const ast::redirection_t &redir) {
    maybe_t<pipe_or_redir_t> oper =
        pipe_or_redir_t::from_string(redir.oper.source(this->buff));  // like 2>
    wcstring target = redir.target.source(this->buff);                // like &1 or file path

    assert(oper.has_value() &&
           "Should have successfully parsed a pipe_or_redir_t since it was in our ast");

    // Color the > part.
    // It may have parsed successfully yet still be invalid (e.g. 9999999999999>&1)
    // If so, color the whole thing invalid and stop.
    if (!oper->is_valid()) {
        this->color_node(redir, highlight_role_t::error);
        return;
    }

    // Color the operator part like 2>.
    this->color_node(redir.oper, highlight_role_t::redirection);

    // Color the target part.
    // Check if the argument contains a command substitution. If so, highlight it as a param
    // even though it's a command redirection, and don't try to do any other validation.
    if (has_cmdsub(target)) {
        this->color_as_argument(redir.target);
    } else {
        // No command substitution, so we can highlight the target file or fd. For example,
        // disallow redirections into a non-existent directory.
        bool target_is_valid = true;
        if (!this->io_ok) {
            // I/O is disallowed, so we don't have much hope of catching anything but gross
            // errors. Assume it's valid.
            target_is_valid = true;
        } else if (!expand_one(target, expand_flag::skip_cmdsubst, ctx)) {
            // Could not be expanded.
            target_is_valid = false;
        } else {
            // Ok, we successfully expanded our target. Now verify that it works with this
            // redirection. We will probably need it as a path (but not in the case of fd
            // redirections). Note that the target is now unescaped.
            const wcstring target_path =
                path_apply_working_directory(target, this->working_directory);
            switch (oper->mode) {
                case redirection_mode_t::fd: {
                    if (target == L"-") {
                        target_is_valid = true;
                    } else {
                        int fd = fish_wcstoi(target.c_str());
                        target_is_valid = !errno && fd >= 0;
                    }
                    break;
                }
                case redirection_mode_t::input: {
                    // Input redirections must have a readable non-directory.
                    struct stat buf = {};
                    target_is_valid = !waccess(target_path, R_OK) && !wstat(target_path, &buf) &&
                                      !S_ISDIR(buf.st_mode);
                    break;
                }
                case redirection_mode_t::overwrite:
                case redirection_mode_t::append:
                case redirection_mode_t::noclob: {
                    // Test whether the file exists, and whether it's writable (possibly after
                    // creating it). access() returns failure if the file does not exist.
                    bool file_exists = false, file_is_writable = false;
                    int err = 0;

                    struct stat buf = {};
                    if (wstat(target_path, &buf) < 0) {
                        err = errno;
                    }

                    if (string_suffixes_string(L"/", target)) {
                        // Redirections to things that are directories is definitely not
                        // allowed.
                        file_exists = false;
                        file_is_writable = false;
                    } else if (err == 0) {
                        // No err. We can write to it if it's not a directory and we have
                        // permission.
                        file_exists = true;
                        file_is_writable = !S_ISDIR(buf.st_mode) && !waccess(target_path, W_OK);
                    } else if (err == ENOENT) {
                        // File does not exist. Check if its parent directory is writable.
                        wcstring parent = wdirname(target_path);

                        // Ensure that the parent ends with the path separator. This will ensure
                        // that we get an error if the parent directory is not really a
                        // directory.
                        if (!string_suffixes_string(L"/", parent)) parent.push_back(L'/');

                        // Now the file is considered writable if the parent directory is
                        // writable.
                        file_exists = false;
                        file_is_writable = (0 == waccess(parent, W_OK));
                    } else {
                        // Other errors we treat as not writable. This includes things like
                        // ENOTDIR.
                        file_exists = false;
                        file_is_writable = false;
                    }

                    // NOCLOB means that we must not overwrite files that exist.
                    target_is_valid = file_is_writable &&
                                      !(file_exists && oper->mode == redirection_mode_t::noclob);
                    break;
                }
            }
        }
        this->color_node(redir.target,
                         target_is_valid ? highlight_role_t::redirection : highlight_role_t::error);
    }
}

/// Determine if a command is valid.
static bool command_is_valid(const wcstring &cmd, enum statement_decoration_t decoration,
                             const wcstring &working_directory, const environment_t &vars) {
    // Determine which types we check, based on the decoration.
    bool builtin_ok = true, function_ok = true, abbreviation_ok = true, command_ok = true,
         implicit_cd_ok = true;
    if (decoration == statement_decoration_t::command ||
        decoration == statement_decoration_t::exec) {
        builtin_ok = false;
        function_ok = false;
        abbreviation_ok = false;
        command_ok = true;
        implicit_cd_ok = false;
    } else if (decoration == statement_decoration_t::builtin) {
        builtin_ok = true;
        function_ok = false;
        abbreviation_ok = false;
        command_ok = false;
        implicit_cd_ok = false;
    }

    // Check them.
    bool is_valid = false;

    // Builtins
    if (!is_valid && builtin_ok) is_valid = builtin_exists(cmd);

    // Functions
    if (!is_valid && function_ok) is_valid = function_exists_no_autoload(cmd);

    // Abbreviations
    if (!is_valid && abbreviation_ok) is_valid = expand_abbreviation(cmd, vars).has_value();

    // Regular commands
    if (!is_valid && command_ok) is_valid = path_get_path(cmd, nullptr, vars);

    // Implicit cd
    if (!is_valid && implicit_cd_ok) {
        is_valid = path_as_implicit_cd(cmd, working_directory, vars).has_value();
    }

    // Return what we got.
    return is_valid;
}

highlighter_t::color_array_t highlighter_t::highlight() {
    // If we are doing I/O, we must be in a background thread.
    if (io_ok) {
        ASSERT_IS_BACKGROUND_THREAD();
    }

    this->color_array.resize(this->buff.size());
    std::fill(this->color_array.begin(), this->color_array.end(), highlight_spec_t{});

    this->visit_children(*ast.top());
    if (ctx.check_cancel()) return std::move(color_array);

    // Color every comment.
    const auto &extras = ast.extras();
    for (const source_range_t &r : extras.comments) {
        this->color_range(r, highlight_role_t::comment);
    }

    // Color every extra semi.
    for (const source_range_t &r : extras.semis) {
        this->color_range(r, highlight_role_t::statement_terminator);
    }

    // Color every error range.
    for (const source_range_t &r : extras.errors) {
        this->color_range(r, highlight_role_t::error);
    }

    // Underline every valid path.
    if (io_ok) {
        for (const ast::node_t &node : ast) {
            const ast::argument_t *arg = node.try_as<ast::argument_t>();
            if (!arg || arg->unsourced) continue;
            if (ctx.check_cancel()) break;
            if (range_is_potential_path(buff, arg->range, ctx, working_directory)) {
                // Don't color highlight_role_t::error because it looks dorky. For example,
                // trying to cd into a non-directory would show an underline and also red.
                for (size_t i = arg->range.start, end = arg->range.start + arg->range.length;
                     i < end; i++) {
                    if (this->color_array.at(i).foreground != highlight_role_t::error) {
                        this->color_array.at(i).valid_path = true;
                    }
                }
            }
        }
    }

    return std::move(color_array);
}

std::string colorize(const wcstring &text, const std::vector<highlight_spec_t> &colors,
                     const environment_t &vars) {
    assert(colors.size() == text.size());
    highlight_color_resolver_t rv;
    outputter_t outp;

    highlight_spec_t last_color = highlight_role_t::normal;
    for (size_t i = 0; i < text.size(); i++) {
        highlight_spec_t color = colors.at(i);
        if (color != last_color) {
            outp.set_color(rv.resolve_spec(color, false, vars), rgb_color_t::normal());
            last_color = color;
        }
        outp.writech(text.at(i));
    }
    outp.set_color(rgb_color_t::normal(), rgb_color_t::normal());
    return outp.contents();
}

void highlight_shell(const wcstring &buff, std::vector<highlight_spec_t> &color, size_t pos,
                     const operation_context_t &ctx, bool io_ok) {
    const wcstring working_directory = ctx.vars.get_pwd_slash();
    highlighter_t highlighter(buff, pos, ctx, working_directory, io_ok);
    color = highlighter.highlight();
}
