// Functions for syntax highlighting.
#include "config.h"  // IWYU pragma: keep

// IWYU pragma: no_include <cstddef>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>

#include <algorithm>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

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
#include "path.h"
#include "tnode.h"
#include "tokenizer.h"
#include "wildcard.h"
#include "wutil.h"  // IWYU pragma: keep

namespace g = grammar;

#define CURSOR_POSITION_INVALID ((size_t)(-1))

/// Number of elements in the highlight_var array.
#define VAR_COUNT (sizeof(highlight_var) / sizeof(wchar_t *))

/// The environment variables used to specify the color of different tokens. This matches the order
/// in highlight_spec_t.
static const wchar_t *const highlight_var[] = {L"fish_color_normal",
                                               L"fish_color_error",
                                               L"fish_color_command",
                                               L"fish_color_end",
                                               L"fish_color_param",
                                               L"fish_color_comment",
                                               L"fish_color_match",
                                               L"fish_color_search_match",
                                               L"fish_color_operator",
                                               L"fish_color_escape",
                                               L"fish_color_quote",
                                               L"fish_color_redirection",
                                               L"fish_color_autosuggestion",
                                               L"fish_color_selection",
                                               L"fish_pager_color_prefix",
                                               L"fish_pager_color_completion",
                                               L"fish_pager_color_description",
                                               L"fish_pager_color_progress",
                                               L"fish_pager_color_secondary"};

/// Determine if the filesystem containing the given fd is case insensitive for lookups regardless
/// of whether it preserves the case when saving a pathname.
///
/// Returns:
///     false: the filesystem is not case insensitive
///     true: the file system is case insensitive
typedef std::unordered_map<wcstring, bool> case_sensitivity_cache_t;
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
                       path_flags_t flags) {
    ASSERT_IS_BACKGROUND_THREAD();

    const bool require_dir = static_cast<bool>(flags & PATH_REQUIRE_DIR);
    wcstring clean_potential_path_fragment;
    int has_magic = 0;
    bool result = false;

    wcstring path_with_magic(potential_path_fragment);
    if (flags & PATH_EXPAND_TILDE) expand_tilde(path_with_magic);

    // debug( 1, L"%ls -> %ls ->%ls", path, tilde, unescaped );

    for (size_t i = 0; i < path_with_magic.size(); i++) {
        wchar_t c = path_with_magic.at(i);
        switch (c) {
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
        return result;
    }

    // Don't test the same path multiple times, which can happen if the path is absolute and the
    // CDPATH contains multiple entries.
    std::unordered_set<wcstring> checked_paths;

    // Keep a cache of which paths / filesystems are case sensitive.
    case_sensitivity_cache_t case_sensitivity_cache;

    for (size_t wd_idx = 0; wd_idx < directories.size() && !result; wd_idx++) {
        const wcstring &wd = directories.at(wd_idx);

        const wcstring abs_path = path_apply_working_directory(clean_potential_path_fragment, wd);

        // Skip this if it's empty or we've already checked it.
        if (abs_path.empty() || checked_paths.count(abs_path)) continue;
        checked_paths.insert(abs_path);

        // If we end with a slash, then it must be a directory.
        bool must_be_full_dir = abs_path.at(abs_path.size() - 1) == L'/';
        if (must_be_full_dir) {
            struct stat buf;
            if (0 == wstat(abs_path, &buf) && S_ISDIR(buf.st_mode)) {
                result = true;
            }
        } else {
            // We do not end with a slash; it does not have to be a directory.
            DIR *dir = NULL;
            const wcstring dir_name = wdirname(abs_path);
            const wcstring filename_fragment = wbasename(abs_path);
            if (dir_name == L"/" && filename_fragment == L"/") {
                // cd ///.... No autosuggestion.
                result = true;
            } else if ((dir = wopendir(dir_name))) {
                // Check if we're case insensitive.
                const bool do_case_insensitive =
                    fs_is_case_insensitive(dir_name, dirfd(dir), case_sensitivity_cache);

                wcstring matched_file;

                // We opened the dir_name; look for a string where the base name prefixes it Don't
                // ask for the is_dir value unless we care, because it can cause extra filesystem
                // access.
                wcstring ent;
                bool is_dir = false;
                while (wreaddir_resolving(dir, dir_name, ent, require_dir ? &is_dir : NULL)) {
                    // Maybe skip directories.
                    if (require_dir && !is_dir) {
                        continue;
                    }

                    if (string_prefixes_string(filename_fragment, ent) ||
                        (do_case_insensitive &&
                         string_prefixes_string_case_insensitive(filename_fragment, ent))) {
                        matched_file = ent;  // we matched
                        break;
                    }
                }
                closedir(dir);

                result = !matched_file.empty();  // we succeeded if we found a match
            }
        }
    }

    return result;
}

// Given a string, return whether it prefixes a path that we could cd into. Return that path in
// out_path. Expects path to be unescaped.
static bool is_potential_cd_path(const wcstring &path, const wcstring &working_directory,
                                 path_flags_t flags) {
    wcstring_list_t directories;

    if (string_prefixes_string(L"./", path)) {
        // Ignore the CDPATH in this case; just use the working directory.
        directories.push_back(working_directory);
    } else {
        // Get the CDPATH.
        auto cdpath = env_get(L"CDPATH");
        std::vector<wcstring> pathsv =
            cdpath.missing_or_empty() ? wcstring_list_t{L"."} : cdpath->as_list();

        for (auto next_path : pathsv) {
            if (next_path.empty()) next_path = L".";
            // Ensure that we use the working directory for relative cdpaths like ".".
            directories.push_back(path_apply_working_directory(next_path, working_directory));
        }
    }

    // Call is_potential_path with all of these directories.
    return is_potential_path(path, directories, flags | PATH_REQUIRE_DIR);
}

// Given a plain statement node in a parse tree, get the command and return it, expanded
// appropriately for commands. If we succeed, return true.
static bool plain_statement_get_expanded_command(const wcstring &src,
                                                 tnode_t<g::plain_statement> stmt,
                                                 wcstring *out_cmd) {
    // Get the command. Try expanding it. If we cannot, it's an error.
    maybe_t<wcstring> cmd = command_for_plain_statement(stmt, src);
    if (!cmd) return false;
    expand_error_t err = expand_to_command_and_args(*cmd, out_cmd, nullptr);
    return err == EXPAND_OK || err == EXPAND_WILDCARD_MATCH;
}

rgb_color_t highlight_get_color(highlight_spec_t highlight, bool is_background) {
    rgb_color_t result = rgb_color_t::normal();

    // If sloppy_background is set, then we look at the foreground color even if is_background is
    // set.
    bool treat_as_background = is_background && !(highlight & highlight_modifier_sloppy_background);

    // Get the primary variable.
    size_t idx = highlight_get_primary(highlight);
    if (idx >= VAR_COUNT) {
        return rgb_color_t::normal();
    }

    auto var = env_get(highlight_var[idx]);

    // debug( 1, L"%d -> %d -> %ls", highlight, idx, val );

    if (!var) var = env_get(highlight_var[0]);

    if (var) result = parse_color(*var, treat_as_background);

    // Handle modifiers.
    if (highlight & highlight_modifier_valid_path) {
        auto var2 = env_get(L"fish_color_valid_path");
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

    if (highlight & highlight_modifier_force_underline) {
        result.set_underline(true);
    }

    return result;
}

static bool has_expand_reserved(const wcstring &str) {
    bool result = false;
    for (size_t i = 0; i < str.size(); i++) {
        wchar_t wc = str.at(i);
        if (wc >= EXPAND_RESERVED_BASE && wc <= EXPAND_RESERVED_END) {
            result = true;
            break;
        }
    }
    return result;
}

// Parse a command line. Return by reference the last command, and the last argument to that command
// (as a string), if any. This is used by autosuggestions.
static bool autosuggest_parse_command(const wcstring &buff, wcstring *out_expanded_command,
                                      wcstring *out_last_arg) {
    // Parse the buffer.
    parse_node_tree_t parse_tree;
    parse_tree_from_string(buff,
                           parse_flag_continue_after_error | parse_flag_accept_incomplete_tokens,
                           &parse_tree, NULL);

    // Find the last statement.
    auto last_statement = parse_tree.find_last_node<g::plain_statement>();
    if (last_statement &&
        plain_statement_get_expanded_command(buff, last_statement, out_expanded_command)) {
        // Find the last argument. If we don't get one, return an invalid node.
        if (auto last_arg = parse_tree.find_last_node<g::argument>(last_statement)) {
            *out_last_arg = last_arg.get_source(buff);
        }
        return true;
    }
    return false;
}

bool autosuggest_validate_from_history(const history_item_t &item,
                                       const wcstring &working_directory,
                                       const env_vars_snapshot_t &vars) {
    ASSERT_IS_BACKGROUND_THREAD();

    bool handled = false, suggestionOK = false;

    // Parse the string.
    wcstring parsed_command;
    wcstring cd_dir;
    if (!autosuggest_parse_command(item.str(), &parsed_command, &cd_dir)) return false;

    if (parsed_command == L"cd" && !cd_dir.empty()) {
        // We can possibly handle this specially.
        if (expand_one(cd_dir, EXPAND_SKIP_CMDSUBST)) {
            handled = true;
            bool is_help =
                string_prefixes_string(cd_dir, L"--help") || string_prefixes_string(cd_dir, L"-h");
            if (!is_help) {
                wcstring path;
                bool can_cd = path_get_cdpath(cd_dir, &path, working_directory.c_str(), vars);
                if (can_cd && !paths_are_same_file(working_directory, path)) {
                    suggestionOK = true;
                }
            }
        }
    }

    if (handled) {
        return suggestionOK;
    }

    // Not handled specially so handle it here.
    bool cmd_ok = false;
    if (path_get_path(parsed_command, NULL)) {
        cmd_ok = true;
    } else if (builtin_exists(parsed_command) ||
               function_exists_no_autoload(parsed_command, vars)) {
        cmd_ok = true;
    }

    if (cmd_ok) {
        const path_list_t &paths = item.get_required_paths();
        suggestionOK = all_paths_are_valid(paths, working_directory);
    }

    return suggestionOK;
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
            colors[idx] = highlight_spec_operator;
        } else {
            colors[idx] = highlight_spec_error;
        }
        idx++;
        dollar_count++;
    }

    // Handle a sequence of variable characters.
    while (valid_var_name_char(in[idx])) {
        colors[idx++] = highlight_spec_operator;
    }

    // Handle a slice, up to dollar_count of them. Note that we currently don't do any validation of
    // the slice's contents, e.g. $foo[blah] will not show an error even though it's invalid.
    for (size_t slice_count = 0; slice_count < dollar_count && in[idx] == L'['; slice_count++) {
        wchar_t *slice_begin = NULL, *slice_end = NULL;
        int located = parse_util_locate_slice(in + idx, &slice_begin, &slice_end, false);
        if (located == 1) {
            size_t slice_begin_idx = slice_begin - in, slice_end_idx = slice_end - in;
            assert(slice_end_idx > slice_begin_idx);
            colors[slice_begin_idx] = highlight_spec_operator;
            colors[slice_end_idx] = highlight_spec_operator;
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
            std::fill(colors, colors + idx + 1, (highlight_spec_t)highlight_spec_error);
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
    assert((base_color == highlight_spec_param || base_color == highlight_spec_command) &&
           "Unexpected base color");
    const size_t buff_len = buffstr.size();
    std::fill(colors, colors + buff_len, base_color);

    enum { e_unquoted, e_single_quoted, e_double_quoted } mode = e_unquoted;
    int bracket_count = 0;
    for (size_t in_pos = 0; in_pos < buff_len; in_pos++) {
        const wchar_t c = buffstr.at(in_pos);
        switch (mode) {
            case e_unquoted: {
                if (c == L'\\') {
                    int fill_color = highlight_spec_escape;  // may be set to highlight_error
                    const size_t backslash_pos = in_pos;
                    size_t fill_end = backslash_pos;

                    // Move to the escaped character.
                    in_pos++;
                    const wchar_t escaped_char = (in_pos < buff_len ? buffstr.at(in_pos) : L'\0');

                    if (escaped_char == L'\0') {
                        fill_end = in_pos;
                        fill_color = highlight_spec_error;
                    } else if (wcschr(L"~%", escaped_char)) {
                        if (in_pos == 1) {
                            fill_end = in_pos + 1;
                        }
                    } else if (escaped_char == L',') {
                        if (bracket_count) {
                            fill_end = in_pos + 1;
                        }
                    } else if (wcschr(L"abefnrtv*?$(){}[]'\"<>^ \\#;|&", escaped_char)) {
                        fill_end = in_pos + 1;
                    } else if (wcschr(L"c", escaped_char)) {
                        // Like \ci. So highlight three characters.
                        fill_end = in_pos + 1;
                    } else if (wcschr(L"uUxX01234567", escaped_char)) {
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
                        if (res > max_val) fill_color = highlight_spec_error;

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
                                colors[in_pos] = highlight_spec_operator;
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
                                colors[in_pos] = highlight_spec_operator;
                            }
                            break;
                        }
                        case L'*':
                        case L'(':
                        case L')': {
                            colors[in_pos] = highlight_spec_operator;
                            break;
                        }
                        case L'{': {
                            colors[in_pos] = highlight_spec_operator;
                            bracket_count++;
                            break;
                        }
                        case L'}': {
                            colors[in_pos] = highlight_spec_operator;
                            bracket_count--;
                            break;
                        }
                        case L',': {
                            if (bracket_count > 0) {
                                colors[in_pos] = highlight_spec_operator;
                            }
                            break;
                        }
                        case L'\'': {
                            colors[in_pos] = highlight_spec_quote;
                            mode = e_single_quoted;
                            break;
                        }
                        case L'\"': {
                            colors[in_pos] = highlight_spec_quote;
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
                colors[in_pos] = highlight_spec_quote;
                if (c == L'\\') {
                    // backslash
                    if (in_pos + 1 < buff_len) {
                        const wchar_t escaped_char = buffstr.at(in_pos + 1);
                        if (escaped_char == L'\\' || escaped_char == L'\'') {
                            colors[in_pos] = highlight_spec_escape;      // backslash
                            colors[in_pos + 1] = highlight_spec_escape;  // escaped char
                            in_pos += 1;                                 // skip over backslash
                        }
                    }
                } else if (c == L'\'') {
                    mode = e_unquoted;
                }
                break;
            }
            // Mode 2 means double quoted string, i.e. "foo".
            case e_double_quoted: {
                // Slices are colored in advance, past `in_pos`, and we don't want to overwrite
                // that.
                if (colors[in_pos] == base_color) {
                    colors[in_pos] = highlight_spec_quote;
                }
                switch (c) {
                    case L'"': {
                        mode = e_unquoted;
                        break;
                    }
                    case L'\\': {
                        // Backslash
                        if (in_pos + 1 < buff_len) {
                            const wchar_t escaped_char = buffstr.at(in_pos + 1);
                            if (wcschr(L"\\\"\n$", escaped_char)) {
                                colors[in_pos] = highlight_spec_escape;      // backslash
                                colors[in_pos + 1] = highlight_spec_escape;  // escaped char
                                in_pos += 1;                                 // skip over backslash
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
}

/// Syntax highlighter helper.
class highlighter_t {
    // The string we're highlighting. Note this is a reference memmber variable (to avoid copying)!
    // We must not outlive this!
    const wcstring &buff;
    // Cursor position.
    const size_t cursor_pos;
    // Environment variables. Again, a reference member variable!
    const env_vars_snapshot_t &vars;
    // Whether it's OK to do I/O.
    const bool io_ok;
    // Working directory.
    const wcstring working_directory;
    // The resulting colors.
    typedef std::vector<highlight_spec_t> color_array_t;
    color_array_t color_array;
    // The parse tree of the buff.
    parse_node_tree_t parse_tree;
    // Color a command.
    void color_command(tnode_t<g::tok_string> node);
    // Color an argument.
    void color_argument(tnode_t<g::tok_string> node);
    // Color a redirection.
    void color_redirection(tnode_t<g::redirection> node);
    // Color a list of arguments. If cmd_is_cd is true, then the arguments are for 'cd'; detect
    // invalid directories.
    void color_arguments(const std::vector<tnode_t<g::argument>> &args, bool cmd_is_cd = false);
    // Color the redirections of the given node.
    void color_redirections(tnode_t<g::arguments_or_redirections_list> list);
    // Color all the children of the command with the given type.
    void color_children(const parse_node_t &parent, parse_token_type_t type,
                        highlight_spec_t color);
    // Colors the source range of a node with a given color.
    void color_node(const parse_node_t &node, highlight_spec_t color);
    // return whether a plain statement is 'cd'.
    bool is_cd(tnode_t<g::plain_statement> stmt) const;

   public:
    // Constructor
    highlighter_t(const wcstring &str, size_t pos, const env_vars_snapshot_t &ev,
                  wcstring wd, bool can_do_io)
        : buff(str),
          cursor_pos(pos),
          vars(ev),
          io_ok(can_do_io),
          working_directory(std::move(wd)),
          color_array(str.size()) {
        // Parse the tree.
        parse_tree_from_string(buff, parse_flag_continue_after_error | parse_flag_include_comments,
                               &this->parse_tree, NULL);
    }

    // Perform highlighting, returning an array of colors.
    const color_array_t &highlight();
};

void highlighter_t::color_node(const parse_node_t &node, highlight_spec_t color) {
    // Can only color nodes with valid source ranges.
    if (!node.has_source() || node.source_length == 0) return;

    // Fill the color array with our color in the corresponding range.
    size_t source_end = node.source_start + node.source_length;
    assert(source_end >= node.source_start);
    assert(source_end <= color_array.size());

    std::fill(this->color_array.begin() + node.source_start, this->color_array.begin() + source_end,
              color);
}

void highlighter_t::color_command(tnode_t<g::tok_string> node) {
    auto source_range = node.source_range();
    if (!source_range) return;

    const wcstring cmd_str = node.get_source(this->buff);

    // Get an iterator to the colors associated with the argument.
    const size_t arg_start = source_range->start;
    const color_array_t::iterator colors = color_array.begin() + arg_start;
    color_string_internal(cmd_str, highlight_spec_command, colors);
}

// node does not necessarily have type symbol_argument here.
void highlighter_t::color_argument(tnode_t<g::tok_string> node) {
    auto source_range = node.source_range();
    if (!source_range) return;

    const wcstring arg_str = node.get_source(this->buff);

    // Get an iterator to the colors associated with the argument.
    const size_t arg_start = source_range->start;
    const color_array_t::iterator arg_colors = color_array.begin() + arg_start;

    // Color this argument without concern for command substitutions.
    color_string_internal(arg_str, highlight_spec_param, arg_colors);

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
        this->color_array.at(arg_subcmd_start) = highlight_spec_operator;
        if (arg_subcmd_end < this->buff.size())
            this->color_array.at(arg_subcmd_end) = highlight_spec_operator;

        // Compute the cursor's position within the cmdsub. We must be past the open paren (hence >)
        // but can be at the end of the string or closed paren (hence <=).
        size_t cursor_subpos = CURSOR_POSITION_INVALID;
        if (cursor_pos != CURSOR_POSITION_INVALID && cursor_pos > arg_subcmd_start &&
            cursor_pos <= arg_subcmd_end) {
            // The -1 because the cmdsub_contents does not include the open paren.
            cursor_subpos = cursor_pos - arg_subcmd_start - 1;
        }

        // Highlight it recursively.
        highlighter_t cmdsub_highlighter(cmdsub_contents, cursor_subpos, this->vars,
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
static bool node_is_potential_path(const wcstring &src, const parse_node_t &node,
                                   const wcstring &working_directory) {
    if (!node.has_source()) return false;

    // Get the node source, unescape it, and then pass it to is_potential_path along with the
    // working directory (as a one element list).
    bool result = false;
    wcstring token(src, node.source_start, node.source_length);
    if (unescape_string_in_place(&token, UNESCAPE_SPECIAL)) {
        // Big hack: is_potential_path expects a tilde, but unescape_string gives us HOME_DIRECTORY.
        // Put it back.
        if (!token.empty() && token.at(0) == HOME_DIRECTORY) token.at(0) = L'~';

        const wcstring_list_t working_directory_list(1, working_directory);
        result = is_potential_path(token, working_directory_list, PATH_EXPAND_TILDE);
    }
    return result;
}

bool highlighter_t::is_cd(tnode_t<g::plain_statement> stmt) const {
    bool cmd_is_cd = false;
    if (this->io_ok && stmt.has_source()) {
        wcstring cmd_str;
        if (plain_statement_get_expanded_command(this->buff, stmt, &cmd_str)) {
            cmd_is_cd = (cmd_str == L"cd");
        }
    }
    return cmd_is_cd;
}

// Color all of the arguments of the given node list, which should be argument_list or
// argument_or_redirection_list.
void highlighter_t::color_arguments(const std::vector<tnode_t<g::argument>> &args, bool cmd_is_cd) {
    // Find all the arguments of this list.
    for (tnode_t<g::argument> arg : args) {
        this->color_argument(arg.child<0>());

        if (cmd_is_cd) {
            // Mark this as an error if it's not 'help' and not a valid cd path.
            wcstring param = arg.get_source(this->buff);
            if (expand_one(param, EXPAND_SKIP_CMDSUBST)) {
                bool is_help = string_prefixes_string(param, L"--help") ||
                               string_prefixes_string(param, L"-h");
                if (!is_help && this->io_ok &&
                    !is_potential_cd_path(param, working_directory, PATH_EXPAND_TILDE)) {
                    this->color_node(arg, highlight_spec_error);
                }
            }
        }
    }
}

void highlighter_t::color_redirection(tnode_t<g::redirection> redirection_node) {
    if (!redirection_node.has_source()) return;

    tnode_t<g::tok_redirection> redir_prim = redirection_node.child<0>();  // like 2>
    tnode_t<g::tok_string> redir_target = redirection_node.child<1>();     // like &1 or file path

    if (redir_prim) {
        wcstring target;
        const maybe_t<redirection_type_t> redirect_type =
            redirection_type(redirection_node, this->buff, nullptr, &target);

        // We may get a missing redirection type if the redirection is invalid.
        auto hl = redirect_type ? highlight_spec_redirection : highlight_spec_error;
        this->color_node(redir_prim, hl);

        // Check if the argument contains a command substitution. If so, highlight it as a param
        // even though it's a command redirection, and don't try to do any other validation.
        if (parse_util_locate_cmdsubst(target.c_str(), NULL, NULL, true) != 0) {
            this->color_argument(redir_target);
        } else {
            // No command substitution, so we can highlight the target file or fd. For example,
            // disallow redirections into a non-existent directory.
            bool target_is_valid = true;

            if (!redirect_type) {
                // not a valid redirection
                target_is_valid = false;
            } else if (!this->io_ok) {
                // I/O is disallowed, so we don't have much hope of catching anything but gross
                // errors. Assume it's valid.
                target_is_valid = true;
            } else if (!expand_one(target, EXPAND_SKIP_CMDSUBST)) {
                // Could not be expanded.
                target_is_valid = false;
            } else {
                // Ok, we successfully expanded our target. Now verify that it works with this
                // redirection. We will probably need it as a path (but not in the case of fd
                // redirections). Note that the target is now unescaped.
                const wcstring target_path =
                    path_apply_working_directory(target, this->working_directory);
                switch (*redirect_type) {
                    case redirection_type_t::fd: {
                        int fd = fish_wcstoi(target.c_str());
                        target_is_valid = !errno && fd >= 0;
                        break;
                    }
                    case redirection_type_t::input: {
                        // Input redirections must have a readable non-directory.
                        struct stat buf = {};
                        target_is_valid = !waccess(target_path, R_OK) &&
                                          !wstat(target_path, &buf) && !S_ISDIR(buf.st_mode);
                        break;
                    }
                    case redirection_type_t::overwrite:
                    case redirection_type_t::append:
                    case redirection_type_t::noclob: {
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
                        target_is_valid =
                            file_is_writable &&
                            !(file_exists && redirect_type == redirection_type_t::noclob);
                        break;
                    }
                }
            }

            if (redir_target) {
                auto hl = target_is_valid ? highlight_spec_redirection : highlight_spec_error;
                this->color_node(redir_target, hl);
            }
        }
    }
}

/// Color all of the redirections of the given command.
void highlighter_t::color_redirections(tnode_t<g::arguments_or_redirections_list> list) {
    for (const auto &node : list.descendants<g::redirection>()) {
        this->color_redirection(node);
    }
}

/// Color all the children of the command with the given type.
void highlighter_t::color_children(const parse_node_t &parent, parse_token_type_t type,
                                   highlight_spec_t color) {
    for (node_offset_t idx = 0; idx < parent.child_count; idx++) {
        const parse_node_t *child = this->parse_tree.get_child(parent, idx);
        if (child != NULL && child->type == type) {
            this->color_node(*child, color);
        }
    }
}

/// Determine if a command is valid.
static bool command_is_valid(const wcstring &cmd, enum parse_statement_decoration_t decoration,
                             const wcstring &working_directory, const env_vars_snapshot_t &vars) {
    // Determine which types we check, based on the decoration.
    bool builtin_ok = true, function_ok = true, abbreviation_ok = true, command_ok = true,
         implicit_cd_ok = true;
    if (decoration == parse_statement_decoration_command ||
        decoration == parse_statement_decoration_exec) {
        builtin_ok = false;
        function_ok = false;
        abbreviation_ok = false;
        command_ok = true;
        implicit_cd_ok = false;
    } else if (decoration == parse_statement_decoration_builtin) {
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
    if (!is_valid && function_ok) is_valid = function_exists_no_autoload(cmd, vars);

    // Abbreviations
    if (!is_valid && abbreviation_ok) is_valid = expand_abbreviation(cmd, NULL);

    // Regular commands
    if (!is_valid && command_ok) is_valid = path_get_path(cmd, NULL, vars);

    // Implicit cd
    if (!is_valid && implicit_cd_ok) {
        is_valid = path_can_be_implicit_cd(cmd, NULL, working_directory.c_str(), vars);
    }

    // Return what we got.
    return is_valid;
}

const highlighter_t::color_array_t &highlighter_t::highlight() {
    // If we are doing I/O, we must be in a background thread.
    if (io_ok) {
        ASSERT_IS_BACKGROUND_THREAD();
    }

    const size_t length = buff.size();
    assert(this->buff.size() == this->color_array.size());
    if (length == 0) return color_array;

    // Start out at zero.
    std::fill(this->color_array.begin(), this->color_array.end(), 0);

    // Walk the node tree.
    for (const parse_node_t &node : parse_tree) {
        switch (node.type) {
            // Color direct string descendants, e.g. 'for' and 'in'.
            case symbol_while_header:
            case symbol_begin_header:
            case symbol_function_header:
            case symbol_if_clause:
            case symbol_else_clause:
            case symbol_case_item:
            case symbol_decorated_statement:
            case symbol_if_statement: {
                this->color_children(node, parse_token_type_string, highlight_spec_command);
                break;
            }
            case symbol_switch_statement: {
                tnode_t<g::switch_statement> switchn(&parse_tree, &node);
                auto literal_switch = switchn.child<0>();
                auto switch_arg = switchn.child<1>();
                this->color_node(literal_switch, highlight_spec_command);
                this->color_node(switch_arg, highlight_spec_param);
                break;
            }
            case symbol_for_header: {
                tnode_t<g::for_header> fhead(&parse_tree, &node);
                // Color the 'for' and 'in' as commands.
                auto literal_for = fhead.child<0>();
                auto literal_in = fhead.child<2>();
                this->color_node(literal_for, highlight_spec_command);
                this->color_node(literal_in, highlight_spec_command);

                // Color the variable name as a parameter.
                this->color_argument(fhead.child<1>());
                break;
            }

            case parse_token_type_andand:
            case parse_token_type_oror:
                this->color_node(node, highlight_spec_operator);
                break;

            case symbol_not_statement:
                this->color_children(node, parse_token_type_string, highlight_spec_operator);
                break;

            case symbol_job_decorator:
                this->color_node(node, highlight_spec_operator);
                break;

            case parse_token_type_pipe:
            case parse_token_type_background:
            case parse_token_type_end:
            case symbol_optional_background: {
                this->color_node(node, highlight_spec_statement_terminator);
                break;
            }
            case symbol_plain_statement: {
                tnode_t<g::plain_statement> stmt(&parse_tree, &node);
                // Get the decoration from the parent.
                enum parse_statement_decoration_t decoration = get_decoration(stmt);

                // Color the command.
                tnode_t<g::tok_string> cmd_node = stmt.child<0>();
                maybe_t<wcstring> cmd = cmd_node.get_source(buff);
                if (!cmd) {
                    break;  // not much as we can do without a node that has source text
                }

                bool is_valid_cmd = false;
                if (!this->io_ok) {
                    // We cannot check if the command is invalid, so just assume it's valid.
                    is_valid_cmd = true;
                } else {
                    wcstring expanded_cmd;
                    // Check to see if the command is valid.
                    // Try expanding it. If we cannot, it's an error.
                    bool expanded = plain_statement_get_expanded_command(buff, stmt, &expanded_cmd);
                    if (expanded && !has_expand_reserved(expanded_cmd)) {
                        is_valid_cmd =
                            command_is_valid(expanded_cmd, decoration, working_directory, vars);
                    }
                }
                if (!is_valid_cmd) {
                    this->color_node(*cmd_node, highlight_spec_error);
                } else {
                    this->color_command(cmd_node);
                }
                break;
            }
            // Only work on root lists, so that we don't re-color child lists.
            case symbol_arguments_or_redirections_list: {
                tnode_t<g::arguments_or_redirections_list> list(&parse_tree, &node);
                if (argument_list_is_root(list)) {
                    bool cmd_is_cd = is_cd(list.try_get_parent<g::plain_statement>());
                    this->color_arguments(list.descendants<g::argument>(), cmd_is_cd);
                    this->color_redirections(list);
                }
                break;
            }
            case symbol_argument_list: {
                tnode_t<g::argument_list> list(&parse_tree, &node);
                if (argument_list_is_root(list)) {
                    this->color_arguments(list.descendants<g::argument>());
                }
                break;
            }
            case symbol_end_command: {
                this->color_node(node, highlight_spec_command);
                break;
            }
            case parse_special_type_parse_error:
            case parse_special_type_tokenizer_error: {
                this->color_node(node, highlight_spec_error);
                break;
            }
            case parse_special_type_comment: {
                this->color_node(node, highlight_spec_comment);
                break;
            }
            default: { break; }
        }
    }

    if (!this->io_ok || this->cursor_pos > this->buff.size()) {
        return color_array;
    }

    // If the cursor is over an argument, and that argument is a valid path, underline it.
    for (parse_node_tree_t::const_iterator iter = parse_tree.begin(); iter != parse_tree.end();
         ++iter) {
        const parse_node_t &node = *iter;

        // Must be an argument with source.
        if (node.type != symbol_argument || !node.has_source()) continue;

        // See if this node contains the cursor. We check <= source_length so that, when backspacing
        // (and the cursor is just beyond the last token), we may still underline it.
        if (this->cursor_pos >= node.source_start &&
            this->cursor_pos - node.source_start <= node.source_length &&
            node_is_potential_path(buff, node, working_directory)) {
            // It is, underline it.
            for (size_t i = node.source_start; i < node.source_start + node.source_length; i++) {
                // Don't color highlight_spec_error because it looks dorky. For example,
                // trying to cd into a non-directory would show an underline and also red.
                if (highlight_get_primary(this->color_array.at(i)) != highlight_spec_error) {
                    this->color_array.at(i) |= highlight_modifier_valid_path;
                }
            }
        }
    }

    return color_array;
}

void highlight_shell(const wcstring &buff, std::vector<highlight_spec_t> &color, size_t pos,
                     wcstring_list_t *error, const env_vars_snapshot_t &vars) {
    UNUSED(error);
    // Do something sucky and get the current working directory on this background thread. This
    // should really be passed in.
    const wcstring working_directory = env_get_pwd_slash();

    // Highlight it!
    highlighter_t highlighter(buff, pos, vars, working_directory, true /* can do IO */);
    color = highlighter.highlight();
}

void highlight_shell_no_io(const wcstring &buff, std::vector<highlight_spec_t> &color, size_t pos,
                           wcstring_list_t *error, const env_vars_snapshot_t &vars) {
    UNUSED(error);
    // Do something sucky and get the current working directory on this background thread. This
    // should really be passed in.
    const wcstring working_directory = env_get_pwd_slash();

    // Highlight it!
    highlighter_t highlighter(buff, pos, vars, working_directory, false /* no IO allowed */);
    color = highlighter.highlight();
}

/// Perform quote and parenthesis highlighting on the specified string.
static void highlight_universal_internal(const wcstring &buffstr,
                                         std::vector<highlight_spec_t> &color, size_t pos) {
    assert(buffstr.size() == color.size());
    if (pos < buffstr.size()) {
        // Highlight matching quotes.
        if ((buffstr.at(pos) == L'\'') || (buffstr.at(pos) == L'\"')) {
            std::vector<size_t> lst;
            int level = 0;
            wchar_t prev_q = 0;
            const wchar_t *const buff = buffstr.c_str();
            const wchar_t *str = buff;
            int match_found = 0;

            while (*str) {
                switch (*str) {
                    case L'\\': {
                        str++;
                        break;
                    }
                    case L'\"':
                    case L'\'': {
                        if (level == 0) {
                            level++;
                            lst.push_back(str - buff);
                            prev_q = *str;
                        } else {
                            if (prev_q == *str) {
                                size_t pos1, pos2;

                                level--;
                                pos1 = lst.back();
                                pos2 = str - buff;
                                if (pos1 == pos || pos2 == pos) {
                                    color.at(pos1) |=
                                        highlight_make_background(highlight_spec_match);
                                    color.at(pos2) |=
                                        highlight_make_background(highlight_spec_match);
                                    match_found = 1;
                                }
                                prev_q = *str == L'\"' ? L'\'' : L'\"';
                            } else {
                                level++;
                                lst.push_back(str - buff);
                                prev_q = *str;
                            }
                        }
                        break;
                    }
                    default: {
                        break;  // we ignore all other characters
                    }
                }
                if ((*str == L'\0')) break;
                str++;
            }

            if (!match_found) color.at(pos) = highlight_make_background(highlight_spec_error);
        }

        // Highlight matching parenthesis.
        const wchar_t c = buffstr.at(pos);
        if (wcschr(L"()[]{}", c)) {
            int step = wcschr(L"({[", c) ? 1 : -1;
            wchar_t dec_char = *(wcschr(L"()[]{}", c) + step);
            wchar_t inc_char = c;
            int level = 0;
            int match_found = 0;
            for (long i = pos; i >= 0 && (size_t)i < buffstr.size(); i += step) {
                const wchar_t test_char = buffstr.at(i);
                if (test_char == inc_char) level++;
                if (test_char == dec_char) level--;
                if (level == 0) {
                    long pos2 = i;
                    color.at(pos) |= highlight_spec_match << 16;
                    color.at(pos2) |= highlight_spec_match << 16;
                    match_found = 1;
                    break;
                }
            }

            if (!match_found) color[pos] = highlight_make_background(highlight_spec_error);
        }
    }
}

void highlight_universal(const wcstring &buff, std::vector<highlight_spec_t> &color, size_t pos,
                         wcstring_list_t *error, const env_vars_snapshot_t &vars) {
    UNUSED(error);
    UNUSED(vars);
    assert(buff.size() == color.size());
    std::fill(color.begin(), color.end(), 0);
    highlight_universal_internal(buff, color, pos);
}
