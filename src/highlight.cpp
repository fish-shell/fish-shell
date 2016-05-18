// Functions for syntax highlighting.
#include "config.h"  // IWYU pragma: keep

// IWYU pragma: no_include <cstddef>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "builtin.h"
#include "color.h"
#include "common.h"
#include "env.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "function.h"
#include "highlight.h"
#include "history.h"
#include "output.h"
#include "parse_constants.h"
#include "parse_tree.h"
#include "parse_util.h"
#include "path.h"
#include "tokenizer.h"
#include "wildcard.h"
#include "wutil.h"  // IWYU pragma: keep

#define CURSOR_POSITION_INVALID ((size_t)(-1))

/// Number of elements in the highlight_var array.
#define VAR_COUNT (sizeof(highlight_var) / sizeof(wchar_t *))

/// The environment variables used to specify the color of different tokens. This matches the order
/// in highlight_spec_t.
static const wchar_t *const highlight_var[] = {
    L"fish_color_normal", L"fish_color_error", L"fish_color_command", L"fish_color_end",
    L"fish_color_param", L"fish_color_comment", L"fish_color_match", L"fish_color_search_match",
    L"fish_color_operator", L"fish_color_escape", L"fish_color_quote", L"fish_color_redirection",
    L"fish_color_autosuggestion", L"fish_color_selection",

    L"fish_pager_color_prefix", L"fish_pager_color_completion", L"fish_pager_color_description",
    L"fish_pager_color_progress", L"fish_pager_color_secondary"

};

/// Determine if the filesystem containing the given fd is case insensitive.
typedef std::map<wcstring, bool> case_sensitivity_cache_t;
bool fs_is_case_insensitive(const wcstring &path, int fd,
                            case_sensitivity_cache_t &case_sensitivity_cache) {
    // If _PC_CASE_SENSITIVE is not defined, assume case sensitive.
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

    const bool require_dir = !!(flags & PATH_REQUIRE_DIR);
    wcstring clean_potential_path_fragment;
    int has_magic = 0;
    bool result = false;

    wcstring path_with_magic(potential_path_fragment);
    if (flags & PATH_EXPAND_TILDE) expand_tilde(path_with_magic);

    // debug( 1, L"%ls -> %ls ->%ls", path, tilde, unescaped );

    for (size_t i = 0; i < path_with_magic.size(); i++) {
        wchar_t c = path_with_magic.at(i);
        switch (c) {
            case PROCESS_EXPAND:
            case VARIABLE_EXPAND:
            case VARIABLE_EXPAND_SINGLE:
            case BRACKET_BEGIN:
            case BRACKET_END:
            case BRACKET_SEP:
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

    if (!has_magic && !clean_potential_path_fragment.empty()) {
        // Don't test the same path multiple times, which can happen if the path is absolute and the
        // CDPATH contains multiple entries.
        std::set<wcstring> checked_paths;

        // Keep a cache of which paths / filesystems are case sensitive.
        case_sensitivity_cache_t case_sensitivity_cache;

        for (size_t wd_idx = 0; wd_idx < directories.size() && !result; wd_idx++) {
            const wcstring &wd = directories.at(wd_idx);

            const wcstring abs_path =
                path_apply_working_directory(clean_potential_path_fragment, wd);

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

                    // We opened the dir_name; look for a string where the base name prefixes it
                    // Don't ask for the is_dir value unless we care, because it can cause extra
                    // filesystem access.
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
                            // We matched.
                            matched_file = ent;
                            break;
                        }
                    }
                    closedir(dir);

                    // We succeeded if we found a match.
                    result = !matched_file.empty();
                }
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
        env_var_t cdpath = env_get_string(L"CDPATH");
        if (cdpath.missing_or_empty()) cdpath = L".";

        // Tokenize it into directories.
        wcstokenizer tokenizer(cdpath, ARRAY_SEP_STR);
        wcstring next_path;
        while (tokenizer.next(next_path)) {
            // Ensure that we use the working directory for relative cdpaths like ".".
            directories.push_back(path_apply_working_directory(next_path, working_directory));
        }
    }

    // Call is_potential_path with all of these directories.
    return is_potential_path(path, directories, flags | PATH_REQUIRE_DIR);
}

// Given a plain statement node in a parse tree, get the command and return it, expanded
// appropriately for commands. If we succeed, return true.
bool plain_statement_get_expanded_command(const wcstring &src, const parse_node_tree_t &tree,
                                          const parse_node_t &plain_statement, wcstring *out_cmd) {
    assert(plain_statement.type == symbol_plain_statement);
    bool result = false;

    // Get the command.
    wcstring cmd;
    if (tree.command_for_plain_statement(plain_statement, src, &cmd)) {
        // Try expanding it. If we cannot, it's an error.
        if (expand_one(cmd, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES | EXPAND_SKIP_JOBS)) {
            // Success, return the expanded string by reference.
            out_cmd->swap(cmd);
            result = true;
        }
    }
    return result;
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

    env_var_t val_wstr = env_get_string(highlight_var[idx]);

    // debug( 1, L"%d -> %d -> %ls", highlight, idx, val );

    if (val_wstr.missing()) val_wstr = env_get_string(highlight_var[0]);

    if (!val_wstr.missing()) result = parse_color(val_wstr, treat_as_background);

    // Handle modifiers.
    if (highlight & highlight_modifier_valid_path) {
        env_var_t val2_wstr = env_get_string(L"fish_color_valid_path");
        const wcstring val2 = val2_wstr.missing() ? L"" : val2_wstr.c_str();

        rgb_color_t result2 = parse_color(val2, is_background);
        if (result.is_normal())
            result = result2;
        else {
            if (result2.is_bold()) result.set_bold(true);
            if (result2.is_underline()) result.set_underline(true);
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
// (as a copied node), if any. This is used by autosuggestions.
static bool autosuggest_parse_command(const wcstring &buff, wcstring *out_expanded_command,
                                      parse_node_t *out_last_arg) {
    bool result = false;

    // Parse the buffer.
    parse_node_tree_t parse_tree;
    parse_tree_from_string(buff,
                           parse_flag_continue_after_error | parse_flag_accept_incomplete_tokens,
                           &parse_tree, NULL);

    // Find the last statement.
    const parse_node_t *last_statement =
        parse_tree.find_last_node_of_type(symbol_plain_statement, NULL);
    if (last_statement != NULL) {
        if (plain_statement_get_expanded_command(buff, parse_tree, *last_statement,
                                                 out_expanded_command)) {
            // We got it.
            result = true;

            // Find the last argument. If we don't get one, return an invalid node.
            const parse_node_t *last_arg =
                parse_tree.find_last_node_of_type(symbol_argument, last_statement);
            if (last_arg != NULL) {
                *out_last_arg = *last_arg;
            }
        }
    }
    return result;
}

bool autosuggest_validate_from_history(const history_item_t &item,
                                       file_detection_context_t &detector,
                                       const wcstring &working_directory,
                                       const env_vars_snapshot_t &vars) {
    ASSERT_IS_BACKGROUND_THREAD();

    bool handled = false, suggestionOK = false;

    // Parse the string.
    wcstring parsed_command;
    parse_node_t last_arg_node(token_type_invalid);
    if (!autosuggest_parse_command(item.str(), &parsed_command, &last_arg_node)) return false;

    if (parsed_command == L"cd" && last_arg_node.type == symbol_argument &&
        last_arg_node.has_source()) {
        // We can possibly handle this specially.
        wcstring dir = last_arg_node.get_source(item.str());
        if (expand_one(dir, EXPAND_SKIP_CMDSUBST)) {
            handled = true;
            bool is_help =
                string_prefixes_string(dir, L"--help") || string_prefixes_string(dir, L"-h");
            if (is_help) {
                suggestionOK = false;
            } else {
                wcstring path;
                bool can_cd = path_get_cdpath(dir, &path, working_directory.c_str(), vars);
                if (!can_cd) {
                    suggestionOK = false;
                } else if (paths_are_same_file(working_directory, path)) {
                    // Don't suggest the working directory as the path!
                    suggestionOK = false;
                } else {
                    suggestionOK = true;
                }
            }
        }
    }

    // If not handled specially, handle it here.
    if (!handled) {
        bool cmd_ok = false;

        if (path_get_path(parsed_command, NULL)) {
            cmd_ok = true;
        } else if (builtin_exists(parsed_command) ||
                   function_exists_no_autoload(parsed_command, vars)) {
            cmd_ok = true;
        }

        if (cmd_ok) {
            const path_list_t &paths = item.get_required_paths();
            if (paths.empty()) {
                suggestionOK = true;
            } else {
                suggestionOK = detector.paths_are_valid(paths);
            }
        }
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
        if (next == L'$' || wcsvarchr(next)) {
            colors[idx] = highlight_spec_operator;
        } else {
            colors[idx] = highlight_spec_error;
        }
        idx++;
        dollar_count++;
    }

    // Handle a sequence of variable characters.
    while (wcsvarchr(in[idx])) {
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

/// This function is a disaster badly in need of refactoring. It colors an argument, without regard
/// to command substitutions.
static void color_argument_internal(const wcstring &buffstr,
                                    std::vector<highlight_spec_t>::iterator colors) {
    const size_t buff_len = buffstr.size();
    std::fill(colors, colors + buff_len, (highlight_spec_t)highlight_spec_param);

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
                        case L'~':
                        case L'%': {
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
                        case L'*':
                        case L'?':
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
                if (colors[in_pos] == highlight_spec_param) {
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
    // Color an argument.
    void color_argument(const parse_node_t &node);
    // Color a redirection.
    void color_redirection(const parse_node_t &node);
    // Color the arguments of the given node.
    void color_arguments(const parse_node_t &list_node);
    // Color the redirections of the given node.
    void color_redirections(const parse_node_t &list_node);
    // Color all the children of the command with the given type.
    void color_children(const parse_node_t &parent, parse_token_type_t type,
                        highlight_spec_t color);
    // Colors the source range of a node with a given color.
    void color_node(const parse_node_t &node, highlight_spec_t color);

   public:
    // Constructor
    highlighter_t(const wcstring &str, size_t pos, const env_vars_snapshot_t &ev,
                  const wcstring &wd, bool can_do_io)
        : buff(str),
          cursor_pos(pos),
          vars(ev),
          io_ok(can_do_io),
          working_directory(wd),
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

// node does not necessarily have type symbol_argument here.
void highlighter_t::color_argument(const parse_node_t &node) {
    if (!node.has_source()) return;

    const wcstring arg_str = node.get_source(this->buff);

    // Get an iterator to the colors associated with the argument.
    const size_t arg_start = node.source_start;
    const color_array_t::iterator arg_colors = color_array.begin() + arg_start;

    // Color this argument without concern for command substitutions.
    color_argument_internal(arg_str, arg_colors);

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

// Color all of the arguments of the given command.
void highlighter_t::color_arguments(const parse_node_t &list_node) {
    // Hack: determine whether the parent is the cd command, so we can show errors for
    // non-directories.
    bool cmd_is_cd = false;
    if (this->io_ok) {
        const parse_node_t *parent = this->parse_tree.get_parent(list_node, symbol_plain_statement);
        if (parent != NULL) {
            wcstring cmd_str;
            if (plain_statement_get_expanded_command(this->buff, this->parse_tree, *parent,
                                                     &cmd_str)) {
                cmd_is_cd = (cmd_str == L"cd");
            }
        }
    }

    // Find all the arguments of this list.
    const parse_node_tree_t::parse_node_list_t nodes =
        this->parse_tree.find_nodes(list_node, symbol_argument);

    for (size_t i = 0; i < nodes.size(); i++) {
        const parse_node_t *child = nodes.at(i);
        assert(child != NULL && child->type == symbol_argument);
        this->color_argument(*child);

        if (cmd_is_cd) {
            // Mark this as an error if it's not 'help' and not a valid cd path.
            wcstring param = child->get_source(this->buff);
            if (expand_one(param, EXPAND_SKIP_CMDSUBST)) {
                bool is_help = string_prefixes_string(param, L"--help") ||
                               string_prefixes_string(param, L"-h");
                if (!is_help && this->io_ok &&
                    !is_potential_cd_path(param, working_directory, PATH_EXPAND_TILDE)) {
                    this->color_node(*child, highlight_spec_error);
                }
            }
        }
    }
}

void highlighter_t::color_redirection(const parse_node_t &redirection_node) {
    assert(redirection_node.type == symbol_redirection);
    if (!redirection_node.has_source()) return;

    const parse_node_t *redirection_primitive =
        this->parse_tree.get_child(redirection_node, 0, parse_token_type_redirection);  // like 2>
    const parse_node_t *redirection_target = this->parse_tree.get_child(
        redirection_node, 1, parse_token_type_string);  // like &1 or file path

    if (redirection_primitive != NULL) {
        wcstring target;
        const enum token_type redirect_type =
            this->parse_tree.type_for_redirection(redirection_node, this->buff, NULL, &target);

        // We may get a TOK_NONE redirection type, e.g. if the redirection is invalid.
        this->color_node(*redirection_primitive, redirect_type == TOK_NONE
                                                     ? highlight_spec_error
                                                     : highlight_spec_redirection);

        // Check if the argument contains a command substitution. If so, highlight it as a param
        // even though it's a command redirection, and don't try to do any other validation.
        if (parse_util_locate_cmdsubst(target.c_str(), NULL, NULL, true) != 0) {
            if (redirection_target != NULL) {
                this->color_argument(*redirection_target);
            }
        } else {
            // No command substitution, so we can highlight the target file or fd. For example,
            // disallow redirections into a non-existent directory.
            bool target_is_valid = true;

            if (!this->io_ok) {
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
                switch (redirect_type) {
                    case TOK_REDIRECT_FD: {
                        // Target should be an fd. It must be all digits, and must not overflow.
                        // fish_wcstoi returns INT_MAX on overflow; we could instead check errno to
                        // disambiguiate this from a real INT_MAX fd, but instead we just disallow
                        // that.
                        const wchar_t *target_cstr = target.c_str();
                        wchar_t *end = NULL;
                        int fd = fish_wcstoi(target_cstr, &end, 10);

                        // The iswdigit check ensures there's no leading whitespace, the *end check
                        // ensures the entire string was consumed, and the numeric checks ensure the
                        // fd is at least zero and there was no overflow.
                        target_is_valid =
                            (iswdigit(target_cstr[0]) && *end == L'\0' && fd >= 0 && fd < INT_MAX);
                        break;
                    }
                    case TOK_REDIRECT_IN: {
                        // Input redirections must have a readable non-directory.
                        struct stat buf = {};
                        target_is_valid = !waccess(target_path, R_OK) &&
                                          !wstat(target_path, &buf) && !S_ISDIR(buf.st_mode);
                        break;
                    }
                    case TOK_REDIRECT_OUT:
                    case TOK_REDIRECT_APPEND:
                    case TOK_REDIRECT_NOCLOB: {
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
                                          !(file_exists && redirect_type == TOK_REDIRECT_NOCLOB);
                        break;
                    }
                    default: {
                        // We should not get here, since the node was marked as a redirection, but
                        // treat it as an error for paranoia.
                        target_is_valid = false;
                        break;
                    }
                }
            }

            if (redirection_target != NULL) {
                this->color_node(*redirection_target, target_is_valid ? highlight_spec_redirection
                                                                      : highlight_spec_error);
            }
        }
    }
}

/// Color all of the redirections of the given command.
void highlighter_t::color_redirections(const parse_node_t &list_node) {
    const parse_node_tree_t::parse_node_list_t nodes =
        this->parse_tree.find_nodes(list_node, symbol_redirection);
    for (size_t i = 0; i < nodes.size(); i++) {
        this->color_redirection(*nodes.at(i));
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
    if (!is_valid && implicit_cd_ok)
        is_valid = path_can_be_implicit_cd(cmd, NULL, working_directory.c_str(), vars);

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

#if 0
    const wcstring dump = parse_dump_tree(parse_tree, buff);
    fprintf(stderr, "%ls\n", dump.c_str());
#endif

    // Walk the node tree.
    for (parse_node_tree_t::const_iterator iter = parse_tree.begin(); iter != parse_tree.end();
         ++iter) {
        const parse_node_t &node = *iter;

        switch (node.type) {
            // Color direct string descendants, e.g. 'for' and 'in'.
            case symbol_while_header:
            case symbol_begin_header:
            case symbol_function_header:
            case symbol_if_clause:
            case symbol_else_clause:
            case symbol_case_item:
            case symbol_boolean_statement:
            case symbol_decorated_statement:
            case symbol_if_statement: {
                this->color_children(node, parse_token_type_string, highlight_spec_command);
                break;
            }
            case symbol_switch_statement: {
                const parse_node_t *literal_switch =
                    this->parse_tree.get_child(node, 0, parse_token_type_string);
                const parse_node_t *switch_arg =
                    this->parse_tree.get_child(node, 1, symbol_argument);
                this->color_node(*literal_switch, highlight_spec_command);
                this->color_node(*switch_arg, highlight_spec_param);
                break;
            }
            case symbol_for_header: {
                // Color the 'for' and 'in' as commands.
                const parse_node_t *literal_for_node =
                    this->parse_tree.get_child(node, 0, parse_token_type_string);
                const parse_node_t *literal_in_node =
                    this->parse_tree.get_child(node, 2, parse_token_type_string);
                this->color_node(*literal_for_node, highlight_spec_command);
                this->color_node(*literal_in_node, highlight_spec_command);

                // Color the variable name as a parameter.
                const parse_node_t *var_name_node =
                    this->parse_tree.get_child(node, 1, parse_token_type_string);
                this->color_argument(*var_name_node);
                break;
            }
            case parse_token_type_pipe:
            case parse_token_type_background:
            case parse_token_type_end:
            case symbol_optional_background: {
                this->color_node(node, highlight_spec_statement_terminator);
                break;
            }
            case symbol_plain_statement: {
                // Get the decoration from the parent.
                enum parse_statement_decoration_t decoration =
                    parse_tree.decoration_for_plain_statement(node);

                // Color the command.
                const parse_node_t *cmd_node =
                    parse_tree.get_child(node, 0, parse_token_type_string);
                if (cmd_node != NULL && cmd_node->has_source()) {
                    bool is_valid_cmd = false;
                    if (!this->io_ok) {
                        // We cannot check if the command is invalid, so just assume it's valid.
                        is_valid_cmd = true;
                    } else {
                        // Check to see if the command is valid.
                        wcstring cmd(buff, cmd_node->source_start, cmd_node->source_length);

                        // Try expanding it. If we cannot, it's an error.
                        bool expanded = expand_one(
                            cmd, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES | EXPAND_SKIP_JOBS);
                        if (expanded && !has_expand_reserved(cmd)) {
                            is_valid_cmd =
                                command_is_valid(cmd, decoration, working_directory, vars);
                        }
                    }
                    this->color_node(*cmd_node,
                                     is_valid_cmd ? highlight_spec_command : highlight_spec_error);
                }
                break;
            }
            case symbol_arguments_or_redirections_list:
            case symbol_argument_list: {
                // Only work on root lists, so that we don't re-color child lists.
                if (parse_tree.argument_list_is_root(node)) {
                    this->color_arguments(node);
                    this->color_redirections(node);
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

    if (this->io_ok && this->cursor_pos <= this->buff.size()) {
        // If the cursor is over an argument, and that argument is a valid path, underline it.
        for (parse_node_tree_t::const_iterator iter = parse_tree.begin(); iter != parse_tree.end();
             ++iter) {
            const parse_node_t &node = *iter;

            // Must be an argument with source.
            if (node.type != symbol_argument || !node.has_source()) continue;

            // See if this node contains the cursor. We check <= source_length so that, when
            // backspacing (and the cursor is just beyond the last token), we may still underline
            // it.
            if (this->cursor_pos >= node.source_start &&
                this->cursor_pos - node.source_start <= node.source_length) {
                // See if this is a valid path.
                if (node_is_potential_path(buff, node, working_directory)) {
                    // It is, underline it.
                    for (size_t i = node.source_start; i < node.source_start + node.source_length;
                         i++) {
                        // Don't color highlight_spec_error because it looks dorky. For example,
                        // trying to cd into a non-directory would show an underline and also red.
                        if (highlight_get_primary(this->color_array.at(i)) !=
                            highlight_spec_error) {
                            this->color_array.at(i) |= highlight_modifier_valid_path;
                        }
                    }
                }
            }
        }
    }

    return color_array;
}

void highlight_shell(const wcstring &buff, std::vector<highlight_spec_t> &color, size_t pos,
                     wcstring_list_t *error, const env_vars_snapshot_t &vars) {
    // Do something sucky and get the current working directory on this background thread. This
    // should really be passed in.
    const wcstring working_directory = env_get_pwd_slash();

    // Highlight it!
    highlighter_t highlighter(buff, pos, vars, working_directory, true /* can do IO */);
    color = highlighter.highlight();
}

void highlight_shell_no_io(const wcstring &buff, std::vector<highlight_spec_t> &color, size_t pos,
                           wcstring_list_t *error, const env_vars_snapshot_t &vars) {
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
    assert(buff.size() == color.size());
    std::fill(color.begin(), color.end(), 0);
    highlight_universal_internal(buff, color, pos);
}
