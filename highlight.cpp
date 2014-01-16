/** \file highlight.c
  Functions for syntax highlighting
*/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <wchar.h>
#include <wctype.h>
#include <termios.h>
#include <signal.h>
#include <algorithm>

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "highlight.h"
#include "tokenizer.h"
#include "proc.h"
#include "parser.h"
#include "parse_util.h"
#include "parser_keywords.h"
#include "builtin.h"
#include "function.h"
#include "env.h"
#include "expand.h"
#include "sanity.h"
#include "common.h"
#include "complete.h"
#include "output.h"
#include "wildcard.h"
#include "path.h"
#include "history.h"
#include "parse_tree.h"

#define CURSOR_POSITION_INVALID ((size_t)(-1))

/**
   Number of elements in the highlight_var array
*/
#define VAR_COUNT ( sizeof(highlight_var)/sizeof(wchar_t *) )

static void highlight_universal_internal(const wcstring &buff, std::vector<highlight_spec_t> &color, size_t pos);

/** The environment variables used to specify the color of different tokens. This matchest the order in highlight_spec_t */
static const wchar_t * const highlight_var[] =
{
    L"fish_color_normal",
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
    L"fish_color_autosuggestion"
};

/* If the given path looks like it's relative to the working directory, then prepend that working directory. */
static wcstring apply_working_directory(const wcstring &path, const wcstring &working_directory)
{
    if (path.empty() || working_directory.empty())
        return path;

    /* We're going to make sure that if we want to prepend the wd, that the string has no leading / */
    bool prepend_wd;
    switch (path.at(0))
    {
        case L'/':
        case L'~':
            prepend_wd = false;
            break;
        default:
            prepend_wd = true;
            break;
    }

    if (! prepend_wd)
    {
        /* No need to prepend the wd, so just return the path we were given */
        return path;
    }
    else
    {
        /* Remove up to one ./ */
        wcstring path_component = path;
        if (string_prefixes_string(L"./", path_component))
        {
            path_component.erase(0, 2);
        }

        /* Removing leading /s */
        while (string_prefixes_string(L"/", path_component))
        {
            path_component.erase(0, 1);
        }

        /* Construct and return a new path */
        wcstring new_path = working_directory;
        append_path_component(new_path, path_component);
        return new_path;
    }
}

/* Determine if the filesystem containing the given fd is case insensitive. */
typedef std::map<wcstring, bool> case_sensitivity_cache_t;
bool fs_is_case_insensitive(const wcstring &path, int fd, case_sensitivity_cache_t &case_sensitivity_cache)
{
    /* If _PC_CASE_SENSITIVE is not defined, assume case sensitive */
    bool result = false;
#ifdef _PC_CASE_SENSITIVE
    /* Try the cache first */
    case_sensitivity_cache_t::iterator cache = case_sensitivity_cache.find(path);
    if (cache != case_sensitivity_cache.end())
    {
        /* Use the cached value */
        result = cache->second;
    }
    else
    {
        /* Ask the system. A -1 value means error (so assume case sensitive), a 1 value means case sensitive, and a 0 value means case insensitive */
        long ret = fpathconf(fd, _PC_CASE_SENSITIVE);
        result = (ret == 0);
        case_sensitivity_cache[path] = result;
    }
#endif
    return result;
}

/* Tests whether the specified string cpath is the prefix of anything we could cd to. directories is a list of possible parent directories (typically either the working directory, or the cdpath). This does I/O!

   We expect the path to already be unescaped.
*/
bool is_potential_path(const wcstring &const_path, const wcstring_list_t &directories, path_flags_t flags, wcstring *out_path)
{
    ASSERT_IS_BACKGROUND_THREAD();

    const bool require_dir = !!(flags & PATH_REQUIRE_DIR);
    wcstring clean_path;
    int has_magic = 0;
    bool result = false;

    wcstring path(const_path);
    if (flags & PATH_EXPAND_TILDE)
        expand_tilde(path);

    //  debug( 1, L"%ls -> %ls ->%ls", path, tilde, unescaped );

    for (size_t i=0; i < path.size(); i++)
    {
        wchar_t c = path.at(i);
        switch (c)
        {
            case PROCESS_EXPAND:
            case VARIABLE_EXPAND:
            case VARIABLE_EXPAND_SINGLE:
            case BRACKET_BEGIN:
            case BRACKET_END:
            case BRACKET_SEP:
            case ANY_CHAR:
            case ANY_STRING:
            case ANY_STRING_RECURSIVE:
            {
                has_magic = 1;
                break;
            }

            case INTERNAL_SEPARATOR:
            {
                break;
            }

            default:
            {
                clean_path.push_back(c);
                break;
            }

        }

    }

    if (! has_magic && ! clean_path.empty())
    {
        /* Don't test the same path multiple times, which can happen if the path is absolute and the CDPATH contains multiple entries */
        std::set<wcstring> checked_paths;

        /* Keep a cache of which paths / filesystems are case sensitive */
        case_sensitivity_cache_t case_sensitivity_cache;

        for (size_t wd_idx = 0; wd_idx < directories.size() && ! result; wd_idx++)
        {
            const wcstring &wd = directories.at(wd_idx);

            const wcstring abs_path = apply_working_directory(clean_path, wd);

            /* Skip this if it's empty or we've already checked it */
            if (abs_path.empty() || checked_paths.count(abs_path))
                continue;
            checked_paths.insert(abs_path);

            /* If we end with a slash, then it must be a directory */
            bool must_be_full_dir = abs_path.at(abs_path.size()-1) == L'/';
            if (must_be_full_dir)
            {
                struct stat buf;
                if (0 == wstat(abs_path, &buf) && S_ISDIR(buf.st_mode))
                {
                    result = true;
                    /* Return the path suffix, not the whole absolute path */
                    if (out_path)
                        *out_path = clean_path;
                }
            }
            else
            {
                DIR *dir = NULL;

                /* We do not end with a slash; it does not have to be a directory */
                const wcstring dir_name = wdirname(abs_path);
                const wcstring base_name = wbasename(abs_path);
                if (dir_name == L"/" && base_name == L"/")
                {
                    result = true;
                    if (out_path)
                        *out_path = clean_path;
                }
                else if ((dir = wopendir(dir_name)))
                {
                    // We opened the dir_name; look for a string where the base name prefixes it
                    wcstring ent;

                    // Check if we're case insensitive
                    bool case_insensitive = fs_is_case_insensitive(dir_name, dirfd(dir), case_sensitivity_cache);

                    // Don't ask for the is_dir value unless we care, because it can cause extra filesystem acces */
                    bool is_dir = false;
                    while (wreaddir_resolving(dir, dir_name, ent, require_dir ? &is_dir : NULL))
                    {

                        /* Determine which function to call to check for prefixes */
                        bool (*prefix_func)(const wcstring &, const wcstring &);
                        if (case_insensitive)
                        {
                            prefix_func = string_prefixes_string_case_insensitive;
                        }
                        else
                        {
                            prefix_func = string_prefixes_string;
                        }

                        if (prefix_func(base_name, ent) && (! require_dir || is_dir))
                        {
                            result = true;
                            if (out_path)
                            {
                                /* We want to return the path in the same "form" as it was given. Take the given path, get its basename. Append that to the output if the basename actually prefixes the path (which it won't if the given path contains no slashes), and isn't a slash (so we don't duplicate slashes). Then append the directory entry. */

                                out_path->clear();
                                const wcstring path_base = wdirname(const_path);


                                if (prefix_func(path_base, const_path))
                                {
                                    out_path->append(path_base);
                                    if (! string_suffixes_string(L"/", *out_path))
                                        out_path->push_back(L'/');
                                }
                                out_path->append(ent);
                                /* We actually do want a trailing / for directories, since it makes autosuggestion a bit nicer */
                                if (is_dir)
                                    out_path->push_back(L'/');
                            }
                            break;
                        }
                    }
                    closedir(dir);
                }
            }
        }
    }
    return result;
}


/* Given a string, return whether it prefixes a path that we could cd into. Return that path in out_path. Expects path to be unescaped. */
static bool is_potential_cd_path(const wcstring &path, const wcstring &working_directory, path_flags_t flags, wcstring *out_path)
{
    wcstring_list_t directories;

    if (string_prefixes_string(L"./", path))
    {
        /* Ignore the CDPATH in this case; just use the working directory */
        directories.push_back(working_directory);
    }
    else
    {
        /* Get the CDPATH */
        env_var_t cdpath = env_get_string(L"CDPATH");
        if (cdpath.missing_or_empty())
            cdpath = L".";

        /* Tokenize it into directories */
        wcstokenizer tokenizer(cdpath, ARRAY_SEP_STR);
        wcstring next_path;
        while (tokenizer.next(next_path))
        {
            /* Ensure that we use the working directory for relative cdpaths like "." */
            directories.push_back(apply_working_directory(next_path, working_directory));
        }
    }

    /* Call is_potential_path with all of these directories */
    bool result = is_potential_path(path, directories, flags | PATH_REQUIRE_DIR, out_path);
#if 0
    if (out_path)
    {
        printf("%ls -> %ls\n", path.c_str(), out_path->c_str());
    }
#endif
    return result;
}

/* Given a plain statement node in a parse tree, get the command and return it, expanded appropriately for commands. If we succeed, return true. */
bool plain_statement_get_expanded_command(const wcstring &src, const parse_node_tree_t &tree, const parse_node_t &plain_statement, wcstring *out_cmd)
{
    assert(plain_statement.type == symbol_plain_statement);
    bool result = false;

    /* Get the command */
    wcstring cmd;
    if (tree.command_for_plain_statement(plain_statement, src, &cmd))
    {
        /* Try expanding it. If we cannot, it's an error. */
        if (expand_one(cmd, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES | EXPAND_SKIP_JOBS))
        {
            /* Success, return the expanded string by reference */
            std::swap(cmd, *out_cmd);
            result = true;
        }
    }
    return result;
}


rgb_color_t highlight_get_color(highlight_spec_t highlight, bool is_background)
{
    rgb_color_t result = rgb_color_t::normal();

    /* Get the primary variable */
    size_t idx = highlight_get_primary(highlight);
    if (idx >= VAR_COUNT)
    {
        return rgb_color_t::normal();
    }

    env_var_t val_wstr = env_get_string(highlight_var[idx]);

//  debug( 1, L"%d -> %d -> %ls", highlight, idx, val );

    if (val_wstr.missing())
        val_wstr = env_get_string(highlight_var[0]);

    if (! val_wstr.missing())
        result = parse_color(val_wstr, is_background);

    /* Handle modifiers. Just one for now */
    if (highlight & highlight_modifier_valid_path)
    {
        env_var_t val2_wstr =  env_get_string(L"fish_color_valid_path");
        const wcstring val2 = val2_wstr.missing() ? L"" : val2_wstr.c_str();

        rgb_color_t result2 = parse_color(val2, is_background);
        if (result.is_normal())
            result = result2;
        else
        {
            if (result2.is_bold())
                result.set_bold(true);
            if (result2.is_underline())
                result.set_underline(true);
        }
    }
    return result;
}


/**
   Highlight operators (such as $, ~, %, as well as escaped characters.
*/
static void highlight_parameter(const wcstring &buffstr, std::vector<highlight_spec_t> &colors, wcstring_list_t *error)
{
    const wchar_t * const buff = buffstr.c_str();
    enum {e_unquoted, e_single_quoted, e_double_quoted} mode = e_unquoted;
    size_t in_pos, len = buffstr.size();
    int bracket_count=0;
    int normal_status = colors.at(0);

    for (in_pos=0; in_pos<len; in_pos++)
    {
        wchar_t c = buffstr.at(in_pos);
        switch (mode)
        {
                /*
                 Mode 0 means unquoted string
                 */
            case e_unquoted:
            {
                if (c == L'\\')
                {
                    size_t start_pos = in_pos;
                    in_pos++;

                    if (wcschr(L"~%", buff[in_pos]))
                    {
                        if (in_pos == 1)
                        {
                            colors.at(start_pos) = highlight_spec_escape;
                            colors.at(in_pos+1) = normal_status;
                        }
                    }
                    else if (buff[in_pos]==L',')
                    {
                        if (bracket_count)
                        {
                            colors.at(start_pos) = highlight_spec_escape;
                            colors.at(in_pos+1) = normal_status;
                        }
                    }
                    else if (wcschr(L"abefnrtv*?$(){}[]'\"<>^ \\#;|&", buff[in_pos]))
                    {
                        colors.at(start_pos)= highlight_spec_escape;
                        colors.at(in_pos+1)=normal_status;
                    }
                    else if (wcschr(L"c", buff[in_pos]))
                    {
                        colors.at(start_pos) = highlight_spec_escape;
                        if (in_pos+2 < colors.size())
                            colors.at(in_pos+2)=normal_status;
                    }
                    else if (wcschr(L"uUxX01234567", buff[in_pos]))
                    {
                        int i;
                        long long res=0;
                        int chars=2;
                        int base=16;

                        wchar_t max_val = ASCII_MAX;

                        switch (buff[in_pos])
                        {
                            case L'u':
                            {
                                chars=4;
                                max_val = UCS2_MAX;
                                break;
                            }

                            case L'U':
                            {
                                chars=8;
                                max_val = WCHAR_MAX;
                                break;
                            }

                            case L'x':
                            {
                                break;
                            }

                            case L'X':
                            {
                                max_val = BYTE_MAX;
                                break;
                            }

                            default:
                            {
                                base=8;
                                chars=3;
                                in_pos--;
                                break;
                            }
                        }

                        for (i=0; i<chars; i++)
                        {
                            long d = convert_digit(buff[++in_pos],base);

                            if (d < 0)
                            {
                                in_pos--;
                                break;
                            }

                            res=(res*base)|d;
                        }

                        if ((res <= max_val))
                        {
                            colors.at(start_pos) = highlight_spec_escape;
                            colors.at(in_pos+1) = normal_status;
                        }
                        else
                        {
                            colors.at(start_pos) = highlight_spec_error;
                            colors.at(in_pos+1) = normal_status;
                        }
                    }

                }
                else
                {
                    switch (buff[in_pos])
                    {
                        case L'~':
                        case L'%':
                        {
                            if (in_pos == 0)
                            {
                                colors.at(in_pos) = highlight_spec_operator;
                                colors.at(in_pos+1) = normal_status;
                            }
                            break;
                        }

                        case L'$':
                        {
                            wchar_t n = buff[in_pos+1];
                            colors.at(in_pos) = (n==L'$'||wcsvarchr(n))? highlight_spec_operator:highlight_spec_error;
                            colors.at(in_pos+1) = normal_status;
                            break;
                        }


                        case L'*':
                        case L'?':
                        case L'(':
                        case L')':
                        {
                            colors.at(in_pos) = highlight_spec_operator;
                            colors.at(in_pos+1) = normal_status;
                            break;
                        }

                        case L'{':
                        {
                            colors.at(in_pos) = highlight_spec_operator;
                            colors.at(in_pos+1) = normal_status;
                            bracket_count++;
                            break;
                        }

                        case L'}':
                        {
                            colors.at(in_pos) = highlight_spec_operator;
                            colors.at(in_pos+1) = normal_status;
                            bracket_count--;
                            break;
                        }

                        case L',':
                        {
                            if (bracket_count)
                            {
                                colors.at(in_pos) = highlight_spec_operator;
                                colors.at(in_pos+1) = normal_status;
                            }

                            break;
                        }

                        case L'\'':
                        {
                            colors.at(in_pos) = highlight_spec_quote;
                            mode = e_single_quoted;
                            break;
                        }

                        case L'\"':
                        {
                            colors.at(in_pos) = highlight_spec_quote;
                            mode = e_double_quoted;
                            break;
                        }

                    }
                }
                break;
            }

            /*
             Mode 1 means single quoted string, i.e 'foo'
             */
            case e_single_quoted:
            {
                if (c == L'\\')
                {
                    size_t start_pos = in_pos;
                    switch (buff[++in_pos])
                    {
                        case '\\':
                        case L'\'':
                        {
                            colors.at(start_pos) = highlight_spec_escape;
                            colors.at(in_pos+1) = highlight_spec_quote;
                            break;
                        }

                        case 0:
                        {
                            return;
                        }

                    }

                }
                if (c == L'\'')
                {
                    mode = e_unquoted;
                    colors.at(in_pos+1) = normal_status;
                }

                break;
            }

            /*
             Mode 2 means double quoted string, i.e. "foo"
             */
            case e_double_quoted:
            {
                switch (c)
                {
                    case '"':
                    {
                        mode = e_unquoted;
                        colors.at(in_pos+1) = normal_status;
                        break;
                    }

                    case '\\':
                    {
                        size_t start_pos = in_pos;
                        switch (buff[++in_pos])
                        {
                            case L'\0':
                            {
                                return;
                            }

                            case '\\':
                            case L'$':
                            case '"':
                            {
                                colors.at(start_pos) = highlight_spec_escape;
                                colors.at(in_pos+1) = highlight_spec_quote;
                                break;
                            }
                        }
                        break;
                    }

                    case '$':
                    {
                        wchar_t n = buff[in_pos+1];
                        colors.at(in_pos) = (n==L'$'||wcsvarchr(n))? highlight_spec_operator:highlight_spec_error;
                        colors.at(in_pos+1) = highlight_spec_quote;
                        break;
                    }

                }
                break;
            }
        }
    }
}

static bool has_expand_reserved(const wcstring &str)
{
    bool result = false;
    for (size_t i=0; i < str.size(); i++)
    {
        wchar_t wc = str.at(i);
        if (wc >= EXPAND_RESERVED && wc <= EXPAND_RESERVED_END)
        {
            result = true;
            break;
        }
    }
    return result;
}

/* Parse a command line. Return by reference the last command, and the last argument to that command (as a copied node), if any. This is used by autosuggestions */
static bool autosuggest_parse_command(const wcstring &buff, wcstring *out_expanded_command, parse_node_t *out_last_arg)
{
    bool result = false;

    /* Parse the buffer */
    parse_node_tree_t parse_tree;
    parse_tree_from_string(buff, parse_flag_continue_after_error | parse_flag_accept_incomplete_tokens, &parse_tree, NULL);

    /* Find the last statement */
    const parse_node_t *last_statement = parse_tree.find_last_node_of_type(symbol_plain_statement, NULL);
    if (last_statement != NULL)
    {
        if (plain_statement_get_expanded_command(buff, parse_tree, *last_statement, out_expanded_command))
        {
            /* We got it */
            result = true;

            /* Find the last argument. If we don't get one, return an invalid node. */
            const parse_node_t *last_arg = parse_tree.find_last_node_of_type(symbol_argument, last_statement);
            if (last_arg != NULL)
            {
                *out_last_arg = *last_arg;
            }
        }
    }
    return result;
}

/* We have to return an escaped string here */
bool autosuggest_suggest_special(const wcstring &str, const wcstring &working_directory, wcstring &out_suggestion)
{
    if (str.empty())
        return false;

    ASSERT_IS_BACKGROUND_THREAD();

    /* Parse the string */
    wcstring parsed_command;
    parse_node_t last_arg_node(token_type_invalid);
    if (! autosuggest_parse_command(str, &parsed_command, &last_arg_node))
        return false;

    bool result = false;
    if (parsed_command == L"cd" && last_arg_node.type == symbol_argument && last_arg_node.has_source())
    {
        /* We can possibly handle this specially */
        const wcstring escaped_dir = last_arg_node.get_source(str);
        wcstring suggested_path;

        /* We always return true because we recognized the command. This prevents us from falling back to dumber algorithms; for example we won't suggest a non-directory for the cd command. */
        result = true;
        out_suggestion.clear();

        /* Unescape the parameter */
        wcstring unescaped_dir;
        bool unescaped = unescape_string(escaped_dir, &unescaped_dir, UNESCAPE_INCOMPLETE);

        /* Determine the quote type we got from the input directory. */
        wchar_t quote = L'\0';
        parse_util_get_parameter_info(escaped_dir, 0, &quote, NULL, NULL);

        /* Big hack to avoid expanding a tilde inside quotes */
        path_flags_t path_flags = (quote == L'\0') ? PATH_EXPAND_TILDE : 0;
        if (unescaped && is_potential_cd_path(unescaped_dir, working_directory, path_flags, &suggested_path))
        {
            /* Note: this looks really wrong for strings that have an "unescapable" character in them, e.g. a \t, because parse_util_escape_string_with_quote will insert that character */
            wcstring escaped_suggested_path = parse_util_escape_string_with_quote(suggested_path, quote);

            /* Return it */
            out_suggestion = str;
            out_suggestion.erase(last_arg_node.source_start);
            if (quote != L'\0') out_suggestion.push_back(quote);
            out_suggestion.append(escaped_suggested_path);
            if (quote != L'\0') out_suggestion.push_back(quote);
        }
    }
    else
    {
        /* Either an error or some other command, so we don't handle it specially */
    }
    return result;
}

bool autosuggest_validate_from_history(const history_item_t &item, file_detection_context_t &detector, const wcstring &working_directory, const env_vars_snapshot_t &vars)
{
    ASSERT_IS_BACKGROUND_THREAD();

    bool handled = false, suggestionOK = false;

    /* Parse the string */
    wcstring parsed_command;
    parse_node_t last_arg_node(token_type_invalid);
    if (! autosuggest_parse_command(item.str(), &parsed_command, &last_arg_node))
        return false;

    if (parsed_command == L"cd" && last_arg_node.type == symbol_argument && last_arg_node.has_source())
    {
        /* We can possibly handle this specially */
        wcstring dir = last_arg_node.get_source(item.str());
        if (expand_one(dir, EXPAND_SKIP_CMDSUBST))
        {
            handled = true;
            bool is_help = string_prefixes_string(dir, L"--help") || string_prefixes_string(dir, L"-h");
            if (is_help)
            {
                suggestionOK = false;
            }
            else
            {
                wcstring path;
                bool can_cd = path_get_cdpath(dir, &path, working_directory.c_str(), vars);
                if (! can_cd)
                {
                    suggestionOK = false;
                }
                else if (paths_are_same_file(working_directory, path))
                {
                    /* Don't suggest the working directory as the path! */
                    suggestionOK = false;
                }
                else
                {
                    suggestionOK = true;
                }
            }
        }
    }

    /* If not handled specially, handle it here */
    if (! handled)
    {
        bool cmd_ok = false;

        if (path_get_path(parsed_command, NULL))
        {
            cmd_ok = true;
        }
        else if (builtin_exists(parsed_command) || function_exists_no_autoload(parsed_command, vars))
        {
            cmd_ok = true;
        }

        if (cmd_ok)
        {
            const path_list_t &paths = item.get_required_paths();
            if (paths.empty())
            {
                suggestionOK= true;
            }
            else
            {
                detector.potential_paths = paths;
                suggestionOK = detector.paths_are_valid(paths);
            }
        }
    }

    return suggestionOK;
}

// This function does I/O
static void tokenize(const wchar_t * const buff, std::vector<highlight_spec_t> &color, const size_t pos, wcstring_list_t *error, const wcstring &working_directory, const env_vars_snapshot_t &vars)
{
    ASSERT_IS_BACKGROUND_THREAD();

    wcstring cmd;
    int had_cmd=0;
    wcstring last_cmd;

    int accept_switches = 1;

    int use_function = 1;
    int use_command = 1;
    int use_builtin = 1;

    CHECK(buff,);

    if (buff[0] == L'\0')
        return;

    std::fill(color.begin(), color.end(), (highlight_spec_t)highlight_spec_invalid);

    tokenizer_t tok(buff, TOK_SHOW_COMMENTS | TOK_SQUASH_ERRORS);
    for (; tok_has_next(&tok); tok_next(&tok))
    {
        int last_type = tok_last_type(&tok);

        switch (last_type)
        {
            case TOK_STRING:
            {
                if (had_cmd)
                {

                    /*Parameter */
                    const wchar_t *param = tok_last(&tok);
                    if (param[0] == L'-')
                    {
                        if (wcscmp(param, L"--") == 0)
                        {
                            accept_switches = 0;
                            color.at(tok_get_pos(&tok)) = highlight_spec_param;
                        }
                        else if (accept_switches)
                        {
                            if (complete_is_valid_option(last_cmd, param, error, false /* no autoload */))
                                color.at(tok_get_pos(&tok)) = highlight_spec_param;
                            else
                                color.at(tok_get_pos(&tok)) = highlight_spec_error;
                        }
                        else
                        {
                            color.at(tok_get_pos(&tok)) = highlight_spec_param;
                        }
                    }
                    else
                    {
                        color.at(tok_get_pos(&tok)) = highlight_spec_param;
                    }

                    if (cmd == L"cd")
                    {
                        wcstring dir = tok_last(&tok);
                        if (expand_one(dir, EXPAND_SKIP_CMDSUBST))
                        {
                            int is_help = string_prefixes_string(dir, L"--help") || string_prefixes_string(dir, L"-h");
                            if (!is_help && ! is_potential_cd_path(dir, working_directory, PATH_EXPAND_TILDE, NULL))
                            {
                                color.at(tok_get_pos(&tok)) = highlight_spec_error;
                            }
                        }
                    }

                    /* Highlight the parameter. highlight_parameter wants to write one more color than we have characters (hysterical raisins) so allocate one more in the vector. But don't copy it back. */
                    const wcstring param_str = param;
                    size_t tok_pos = tok_get_pos(&tok);

                    std::vector<highlight_spec_t>::const_iterator where = color.begin() + tok_pos;
                    std::vector<highlight_spec_t> subcolors(where, where + param_str.size());
                    subcolors.push_back(highlight_spec_invalid);
                    highlight_parameter(param_str, subcolors, error);

                    /* Copy the subcolors back into our colors array */
                    std::copy(subcolors.begin(), subcolors.begin() + param_str.size(), color.begin() + tok_pos);
                }
                else
                {
                    /*
                     Command. First check that the command actually exists.
                     */
                    cmd = tok_last(&tok);
                    bool expanded = expand_one(cmd, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES | EXPAND_SKIP_JOBS);
                    if (! expanded || has_expand_reserved(cmd))
                    {
                        color.at(tok_get_pos(&tok)) = highlight_spec_error;
                    }
                    else
                    {
                        bool is_cmd = false;
                        int is_subcommand = 0;
                        int mark = tok_get_pos(&tok);
                        color.at(tok_get_pos(&tok)) = use_builtin ? highlight_spec_command : highlight_spec_error;

                        if (parser_keywords_is_subcommand(cmd))
                        {

                            int sw;

                            if (cmd == L"builtin")
                            {
                                use_function = 0;
                                use_command  = 0;
                                use_builtin  = 1;
                            }
                            else if (cmd == L"command" || cmd == L"exec")
                            {
                                use_command  = 1;
                                use_function = 0;
                                use_builtin  = 0;
                            }

                            tok_next(&tok);

                            sw = parser_keywords_is_switch(tok_last(&tok));

                            if (!parser_keywords_is_block(cmd) &&
                                    sw == ARG_SWITCH)
                            {
                                /*
                                 The 'builtin' and 'command' builtins
                                 are normally followed by another
                                 command, but if they are invoked
                                 with a switch, they aren't.

                                 */
                                use_command  = 1;
                                use_function = 1;
                                use_builtin  = 2;
                            }
                            else
                            {
                                if (sw == ARG_SKIP)
                                {
                                    color.at(tok_get_pos(&tok)) = highlight_spec_param;
                                    mark = tok_get_pos(&tok);
                                }

                                is_subcommand = 1;
                            }
                            tok_set_pos(&tok, mark);
                        }

                        if (!is_subcommand)
                        {
                            /*
                             OK, this is a command, it has been
                             successfully expanded and everything
                             looks ok. Lets check if the command
                             exists.
                             */

                            /*
                             First check if it is a builtin or
                             function, since we don't have to stat
                             any files for that
                             */
                            if (! is_cmd && use_builtin)
                                is_cmd = builtin_exists(cmd);

                            if (! is_cmd && use_function)
                                is_cmd = function_exists_no_autoload(cmd, vars);

                            if (! is_cmd)
                                is_cmd = expand_abbreviation(cmd, NULL);

                            /*
                             Moving on to expensive tests
                             */

                            /*
                             Check if this is a regular command
                             */
                            if (! is_cmd && use_command)
                            {
                                is_cmd = path_get_path(cmd, NULL, vars);
                            }

                            /* Maybe it is a path for a implicit cd command. */
                            if (! is_cmd)
                            {
                                if (use_builtin || (use_function && function_exists_no_autoload(L"cd", vars)))
                                    is_cmd = path_can_be_implicit_cd(cmd, NULL, working_directory.c_str(), vars);
                            }

                            if (is_cmd)
                            {
                                color.at(tok_get_pos(&tok)) = highlight_spec_command;
                            }
                            else
                            {
                                if (error)
                                {
                                    error->push_back(format_string(L"Unknown command \'%ls\'", cmd.c_str()));
                                }
                                color.at(tok_get_pos(&tok)) = (highlight_spec_error);
                            }
                            had_cmd = 1;
                        }

                        if (had_cmd)
                        {
                            last_cmd = tok_last(&tok);
                        }
                    }

                }
                break;
            }

            case TOK_REDIRECT_NOCLOB:
            case TOK_REDIRECT_OUT:
            case TOK_REDIRECT_IN:
            case TOK_REDIRECT_APPEND:
            case TOK_REDIRECT_FD:
            {
                if (!had_cmd)
                {
                    color.at(tok_get_pos(&tok)) = highlight_spec_error;
                    if (error)
                        error->push_back(L"Redirection without a command");
                    break;
                }

                wcstring target_str;
                const wchar_t *target=NULL;

                color.at(tok_get_pos(&tok)) = highlight_spec_redirection;
                tok_next(&tok);

                /*
                 Check that we are redirecting into a file
                 */

                switch (tok_last_type(&tok))
                {
                    case TOK_STRING:
                    {
                        target_str = tok_last(&tok);
                        if (expand_one(target_str, EXPAND_SKIP_CMDSUBST))
                        {
                            target = target_str.c_str();
                        }
                        /*
                         Redirect filename may contain a cmdsubst.
                         If so, it will be ignored/not flagged.
                         */
                    }
                    break;
                    default:
                    {
                        size_t pos = tok_get_pos(&tok);
                        if (pos < color.size())
                        {
                            color.at(pos) = highlight_spec_error;
                        }
                        if (error)
                            error->push_back(L"Invalid redirection");
                    }

                }

                if (target != 0)
                {
                    wcstring dir = target;
                    size_t slash_idx = dir.find_last_of(L'/');
                    struct stat buff;
                    /*
                     If file is in directory other than '.', check
                     that the directory exists.
                     */
                    if (slash_idx != wcstring::npos)
                    {
                        dir.resize(slash_idx);
                        if (wstat(dir, &buff) == -1)
                        {
                            color.at(tok_get_pos(&tok)) = highlight_spec_error;
                            if (error)
                                error->push_back(format_string(L"Directory \'%ls\' does not exist", dir.c_str()));

                        }
                    }

                    /*
                     If the file is read from or appended to, check
                     if it exists.
                     */
                    if (last_type == TOK_REDIRECT_IN ||
                            last_type == TOK_REDIRECT_APPEND)
                    {
                        if (wstat(target, &buff) == -1)
                        {
                            color.at(tok_get_pos(&tok)) = highlight_spec_error;
                            if (error)
                                error->push_back(format_string(L"File \'%ls\' does not exist", target));
                        }
                    }
                    if (last_type == TOK_REDIRECT_NOCLOB)
                    {
                        if (wstat(target, &buff) != -1)
                        {
                            color.at(tok_get_pos(&tok)) = highlight_spec_error;
                            if (error)
                                error->push_back(format_string(L"File \'%ls\' exists", target));
                        }
                    }
                }
                break;
            }

            case TOK_PIPE:
            case TOK_BACKGROUND:
            {
                if (had_cmd)
                {
                    color.at(tok_get_pos(&tok)) = highlight_spec_statement_terminator;
                    had_cmd = 0;
                    use_command  = 1;
                    use_function = 1;
                    use_builtin  = 1;
                    accept_switches = 1;
                }
                else
                {
                    color.at(tok_get_pos(&tok)) = highlight_spec_error;
                    if (error)
                        error->push_back(L"No job to put in background");
                }

                break;
            }

            case TOK_END:
            {
                color.at(tok_get_pos(&tok)) = highlight_spec_statement_terminator;
                had_cmd = 0;
                use_command  = 1;
                use_function = 1;
                use_builtin  = 1;
                accept_switches = 1;
                break;
            }

            case TOK_COMMENT:
            {
                color.at(tok_get_pos(&tok)) = highlight_spec_comment;
                break;
            }

            case TOK_ERROR:
            default:
            {
                /*
                 If the tokenizer reports an error, highlight it as such.
                 */
                if (error)
                    error->push_back(tok_last(&tok));
                color.at(tok_get_pos(&tok)) = highlight_spec_error;
                break;
            }
        }
    }
}

void highlight_shell(const wcstring &buff, std::vector<highlight_spec_t> &color, size_t pos, wcstring_list_t *error, const env_vars_snapshot_t &vars)
{
    if (1)
    {
        highlight_shell_new_parser(buff, color, pos, error, vars);
    }
    else
    {
        highlight_shell_classic(buff, color, pos, error, vars);
    }
}

// PCA This function does I/O, (calls is_potential_path, path_get_path, maybe others) and so ought to only run on a background thread
void highlight_shell_classic(const wcstring &buff, std::vector<highlight_spec_t> &color, size_t pos, wcstring_list_t *error, const env_vars_snapshot_t &vars)
{
    ASSERT_IS_BACKGROUND_THREAD();

    const size_t length = buff.size();
    assert(buff.size() == color.size());


    if (length == 0)
        return;

    std::fill(color.begin(), color.end(), (highlight_spec_t)highlight_spec_invalid);

    /* Do something sucky and get the current working directory on this background thread. This should really be passed in. */
    const wcstring working_directory = env_get_pwd_slash();

    /* Tokenize the string */
    tokenize(buff.c_str(), color, pos, error, working_directory, vars);

    /* Locate and syntax highlight cmdsubsts recursively */

    wchar_t * const subbuff = wcsdup(buff.c_str());
    wchar_t * subpos = subbuff;
    bool done = false;

    while (1)
    {
        wchar_t *begin, *end;

        if (parse_util_locate_cmdsubst(subpos, &begin, &end, true) <= 0)
        {
            break;
        }

        /* Note: This *end = 0 writes into subbuff! */
        if (!*end)
            done = true;
        else
            *end = 0;

        //our subcolors start at color + (begin-subbuff)+1
        size_t start = begin - subbuff + 1, len = wcslen(begin + 1);
        std::vector<highlight_spec_t> subcolors(len, highlight_spec_invalid);

        highlight_shell(begin+1, subcolors, -1, error, vars);

        // insert subcolors
        std::copy(subcolors.begin(), subcolors.end(), color.begin() + start);

        // highlight the end of the subcommand
        assert(end >= subbuff);
        if ((size_t)(end - subbuff) < length)
        {
            color.at(end-subbuff)=highlight_spec_operator;
        }

        if (done)
            break;

        subpos = end+1;
    }
    free(subbuff);

    /*
      The highlighting code only changes the first element when the
      color changes. This fills in the rest.
    */
    int last_val=0;
    for (size_t i=0; i < buff.size(); i++)
    {
        highlight_spec_t &current_val = color.at(i);
        if (current_val != highlight_spec_invalid)
        {
            last_val = current_val;
        }
        else
        {
            current_val = last_val; //note - this writes into the vector
        }
    }

    /*
      Color potentially valid paths in a special path color if they
      are the current token.
        For reasons that I don't yet understand, it's required that pos be allowed to be length (e.g. when backspacing).
    */
    if (pos <= length)
    {

        const wchar_t *cbuff = buff.c_str();
        const wchar_t *tok_begin, *tok_end;
        parse_util_token_extent(cbuff, pos, &tok_begin, &tok_end, 0, 0);
        if (tok_begin && tok_end)
        {
            wcstring token(tok_begin, tok_end-tok_begin);
            if (unescape_string_in_place(&token, UNESCAPE_SPECIAL))
            {
                /* Big hack: is_potential_path expects a tilde, but unescape_string gives us HOME_DIRECTORY. Put it back. */
                if (! token.empty() && token.at(0) == HOME_DIRECTORY)
                    token.at(0) = L'~';

                const wcstring_list_t working_directory_list(1, working_directory);
                if (is_potential_path(token, working_directory_list, PATH_EXPAND_TILDE))
                {
                    for (ptrdiff_t i=tok_begin-cbuff; i < (tok_end-cbuff); i++)
                    {
                        // Don't color highlight_spec_error because it looks dorky. For example, trying to cd into a non-directory would show an underline and also red.
                        if (highlight_get_primary(color.at(i)) != highlight_spec_error)
                        {
                            color.at(i) |= highlight_modifier_valid_path;
                        }
                    }
                }
            }
        }
    }


    highlight_universal_internal(buff, color, pos);

    /*
      Spaces should not be highlighted at all, since it makes cursor look funky in some terminals
    */
    for (size_t i=0; i < buff.size(); i++)
    {
        if (iswspace(buff.at(i)))
        {
            color.at(i)=0;
        }
    }
}

/* This function is a disaster badly in need of refactoring. */
static void color_argument_internal(const wcstring &buffstr, std::vector<highlight_spec_t>::iterator colors)
{
    const size_t buff_len = buffstr.size();
    std::fill(colors, colors + buff_len, (highlight_spec_t)highlight_spec_param);

    enum {e_unquoted, e_single_quoted, e_double_quoted} mode = e_unquoted;
    int bracket_count=0;
    for (size_t in_pos=0; in_pos < buff_len; in_pos++)
    {
        const wchar_t c = buffstr.at(in_pos);
        switch (mode)
        {
            case e_unquoted:
            {
                if (c == L'\\')
                {
                    int fill_color = highlight_spec_escape; //may be set to highlight_error
                    const size_t backslash_pos = in_pos;
                    size_t fill_end = backslash_pos;

                    // Move to the escaped character
                    in_pos++;
                    const wchar_t escaped_char = (in_pos < buff_len ? buffstr.at(in_pos) : L'\0');

                    if (escaped_char == L'\0')
                    {
                        fill_end = in_pos;
                        fill_color = highlight_spec_error;
                    }
                    else if (wcschr(L"~%", escaped_char))
                    {
                        if (in_pos == 1)
                        {
                            fill_end = in_pos + 1;
                        }
                    }
                    else if (escaped_char == L',')
                    {
                        if (bracket_count)
                        {
                            fill_end = in_pos + 1;
                        }
                    }
                    else if (wcschr(L"abefnrtv*?$(){}[]'\"<>^ \\#;|&", escaped_char))
                    {
                        fill_end = in_pos + 1;
                    }
                    else if (wcschr(L"c", escaped_char))
                    {
                        // Like \ci. So highlight three characters
                        fill_end = in_pos + 1;
                    }
                    else if (wcschr(L"uUxX01234567", escaped_char))
                    {
                        long long res=0;
                        int chars=2;
                        int base=16;

                        wchar_t max_val = ASCII_MAX;

                        switch (escaped_char)
                        {
                            case L'u':
                            {
                                chars=4;
                                max_val = UCS2_MAX;
                                in_pos++;
                                break;
                            }

                            case L'U':
                            {
                                chars=8;
                                max_val = WCHAR_MAX;
                                in_pos++;
                                break;
                            }

                            case L'x':
                            {
                                in_pos++;
                                break;
                            }

                            case L'X':
                            {
                                max_val = BYTE_MAX;
                                in_pos++;
                                break;
                            }

                            default:
                            {
                                // a digit like \12
                                base=8;
                                chars=3;
                                break;
                            }
                        }

                        // Consume
                        for (int i=0; i < chars && in_pos < buff_len; i++)
                        {
                            long d = convert_digit(buffstr.at(in_pos), base);
                            if (d < 0)
                                break;
                            res = (res * base) + d;
                            in_pos++;
                        }
                        //in_pos is now at the first character that could not be converted (or buff_len)
                        assert(in_pos >= backslash_pos && in_pos <= buff_len);
                        fill_end = in_pos;

                        // It's an error if we exceeded the max value
                        if (res > max_val)
                            fill_color = highlight_spec_error;

                        // Subtract one from in_pos, so that the increment in the loop will move to the next character
                        in_pos--;
                    }
                    assert(fill_end >= backslash_pos);
                    std::fill(colors + backslash_pos, colors + fill_end, fill_color);
                }
                else
                {
                    // Not a backslash
                    switch (c)
                    {
                        case L'~':
                        case L'%':
                        {
                            if (in_pos == 0)
                            {
                                colors[in_pos] = highlight_spec_operator;
                            }
                            break;
                        }

                        case L'$':
                        {
                            assert(in_pos < buff_len);
                            int dollar_color = highlight_spec_error;
                            if (in_pos + 1 < buff_len)
                            {
                                wchar_t next = buffstr.at(in_pos + 1);
                                if (next == L'$' || wcsvarchr(next))
                                    dollar_color = highlight_spec_operator;
                            }
                            colors[in_pos] = dollar_color;
                            break;
                        }


                        case L'*':
                        case L'?':
                        case L'(':
                        case L')':
                        {
                            colors[in_pos] = highlight_spec_operator;
                            break;
                        }

                        case L'{':
                        {
                            colors[in_pos] = highlight_spec_operator;
                            bracket_count++;
                            break;
                        }

                        case L'}':
                        {
                            colors[in_pos] = highlight_spec_operator;
                            bracket_count--;
                            break;
                        }

                        case L',':
                        {
                            if (bracket_count > 0)
                            {
                                colors[in_pos] = highlight_spec_operator;
                            }

                            break;
                        }

                        case L'\'':
                        {
                            colors[in_pos] = highlight_spec_quote;
                            mode = e_single_quoted;
                            break;
                        }

                        case L'\"':
                        {
                            colors[in_pos] = highlight_spec_quote;
                            mode = e_double_quoted;
                            break;
                        }

                    }
                }
                break;
            }

            /*
             Mode 1 means single quoted string, i.e 'foo'
             */
            case e_single_quoted:
            {
                colors[in_pos] = highlight_spec_quote;
                if (c == L'\\')
                {
                    // backslash
                    if (in_pos + 1 < buff_len)
                    {
                        const wchar_t escaped_char = buffstr.at(in_pos + 1);
                        if (escaped_char == L'\\' || escaped_char == L'\'')
                        {
                            colors[in_pos] = highlight_spec_escape; //backslash
                            colors[in_pos + 1] = highlight_spec_escape; //escaped char
                            in_pos += 1; //skip over backslash
                        }
                    }
                }
                else if (c == L'\'')
                {
                    mode = e_unquoted;
                }
                break;
            }

            /*
             Mode 2 means double quoted string, i.e. "foo"
             */
            case e_double_quoted:
            {
                colors[in_pos] = highlight_spec_quote;
                switch (c)
                {
                    case L'"':
                    {
                        mode = e_unquoted;
                        break;
                    }

                    case L'\\':
                    {
                        // backslash
                        if (in_pos + 1 < buff_len)
                        {
                            const wchar_t escaped_char = buffstr.at(in_pos + 1);
                            if (escaped_char == L'\\' || escaped_char == L'\'' || escaped_char == L'$')
                            {
                                colors[in_pos] = highlight_spec_escape; //backslash
                                colors[in_pos + 1] = highlight_spec_escape; //escaped char
                                in_pos += 1; //skip over backslash
                            }
                        }
                        break;
                    }

                    case L'$':
                    {
                        int dollar_color = highlight_spec_error;
                        if (in_pos + 1 < buff_len)
                        {
                            wchar_t next = buffstr.at(in_pos + 1);
                            if (next == L'$' || wcsvarchr(next))
                                dollar_color = highlight_spec_operator;
                        }
                        colors[in_pos] = dollar_color;
                        break;
                    }

                }
                break;
            }
        }
    }
}

/* Syntax highlighter helper */
class highlighter_t
{
    /* The string we're highlighting. Note this is a reference memmber variable (to avoid copying)! We must not outlive this! */
    const wcstring &buff;

    /* Cursor position */
    const size_t cursor_pos;

    /* Environment variables. Again, a reference member variable! */
    const env_vars_snapshot_t &vars;

    /* Working directory */
    const wcstring working_directory;

    /* The resulting colors */
    typedef std::vector<highlight_spec_t> color_array_t;
    color_array_t color_array;

    /* The parse tree of the buff */
    parse_node_tree_t parse_tree;

    /* Color an argument */
    void color_argument(const parse_node_t &node);

    /* Color a redirection */
    void color_redirection(const parse_node_t &node);

    /* Color the arguments of the given node */
    void color_arguments(const parse_node_t &list_node);

    /* Color the redirections of the given node */
    void color_redirections(const parse_node_t &list_node);

    /* Color all the children of the command with the given type */
    void color_children(const parse_node_t &parent, parse_token_type_t type, int color);

    /* Colors the source range of a node with a given color */
    void color_node(const parse_node_t &node, int color);

public:

    /* Constructor */
    highlighter_t(const wcstring &str, size_t pos, const env_vars_snapshot_t &ev, const wcstring &wd) : buff(str), cursor_pos(pos), vars(ev), working_directory(wd), color_array(str.size())
    {
        /* Parse the tree */
        this->parse_tree.clear();
        parse_tree_from_string(buff, parse_flag_continue_after_error | parse_flag_include_comments, &this->parse_tree, NULL);
    }

    /* Perform highlighting, returning an array of colors */
    const color_array_t &highlight();
};

void highlighter_t::color_node(const parse_node_t &node, int color)
{
    // Can only color nodes with valid source ranges
    if (! node.has_source())
        return;

    // Fill the color array with our color in the corresponding range
    size_t source_end = node.source_start + node.source_length;
    assert(source_end >= node.source_start);
    assert(source_end <= color_array.size());

    std::fill(this->color_array.begin() + node.source_start, this->color_array.begin() + source_end, color);
}

/* node does not necessarily have type symbol_argument here */
void highlighter_t::color_argument(const parse_node_t &node)
{
    if (! node.has_source())
        return;

    const wcstring arg_str = node.get_source(this->buff);

    /* Get an iterator to the colors associated with the argument */
    const size_t arg_start = node.source_start;
    const color_array_t::iterator arg_colors = color_array.begin() + arg_start;

    /* Color this argument without concern for command substitutions */
    color_argument_internal(arg_str, arg_colors);

    /* Now do command substitutions */
    size_t cmdsub_cursor = 0, cmdsub_start = 0, cmdsub_end = 0;
    wcstring cmdsub_contents;
    while (parse_util_locate_cmdsubst_range(arg_str, &cmdsub_cursor, &cmdsub_contents, &cmdsub_start, &cmdsub_end, true /* accept incomplete */) > 0)
    {
        /* The cmdsub_start is the open paren. cmdsub_end is either the close paren or the end of the string. cmdsub_contents extends from one past cmdsub_start to cmdsub_end */
        assert(cmdsub_end > cmdsub_start);
        assert(cmdsub_end - cmdsub_start - 1 == cmdsub_contents.size());

        /* Found a command substitution. Compute the position of the start and end of the cmdsub contents, within our overall src. */
        const size_t arg_subcmd_start = arg_start + cmdsub_start, arg_subcmd_end = arg_start + cmdsub_end;

        /* Highlight the parens. The open paren must exist; the closed paren may not if it was incomplete. */
        assert(cmdsub_start < arg_str.size());
        this->color_array.at(arg_subcmd_start) = highlight_spec_operator;
        if (arg_subcmd_end < this->buff.size())
            this->color_array.at(arg_subcmd_end) = highlight_spec_operator;

        /* Compute the cursor's position within the cmdsub. We must be past the open paren (hence >) but can be at the end of the string or closed paren (hence <=) */
        size_t cursor_subpos = CURSOR_POSITION_INVALID;
        if (cursor_pos != CURSOR_POSITION_INVALID && cursor_pos > arg_subcmd_start && cursor_pos <= arg_subcmd_end)
        {
            /* The -1 because the cmdsub_contents does not include the open paren */
            cursor_subpos = cursor_pos - arg_subcmd_start - 1;
        }

        /* Highlight it recursively. */
        highlighter_t cmdsub_highlighter(cmdsub_contents, cursor_subpos, this->vars, this->working_directory);
        const color_array_t &subcolors = cmdsub_highlighter.highlight();

        /* Copy out the subcolors back into our array */
        assert(subcolors.size() == cmdsub_contents.size());
        std::copy(subcolors.begin(), subcolors.end(), this->color_array.begin() + arg_subcmd_start + 1);
    }
}

// Indicates whether the source range of the given node forms a valid path in the given working_directory
static bool node_is_potential_path(const wcstring &src, const parse_node_t &node, const wcstring &working_directory)
{
    if (! node.has_source())
        return false;


    /* Get the node source, unescape it, and then pass it to is_potential_path along with the working directory (as a one element list) */
    bool result = false;
    wcstring token(src, node.source_start, node.source_length);
    if (unescape_string_in_place(&token, UNESCAPE_SPECIAL))
    {
        /* Big hack: is_potential_path expects a tilde, but unescape_string gives us HOME_DIRECTORY. Put it back. */
        if (! token.empty() && token.at(0) == HOME_DIRECTORY)
            token.at(0) = L'~';

        const wcstring_list_t working_directory_list(1, working_directory);
        result = is_potential_path(token, working_directory_list, PATH_EXPAND_TILDE);
    }
    return result;
}

// Color all of the arguments of the given command
void highlighter_t::color_arguments(const parse_node_t &list_node)
{
    /* Hack: determine whether the parent is the cd command, so we can show errors for non-directories */
    bool cmd_is_cd = false;
    const parse_node_t *parent = this->parse_tree.get_parent(list_node, symbol_plain_statement);
    if (parent != NULL)
    {
        wcstring cmd_str;
        if (plain_statement_get_expanded_command(this->buff, this->parse_tree, *parent, &cmd_str))
        {
            cmd_is_cd = (cmd_str == L"cd");
        }
    }

    /* Find all the arguments of this list */
    const parse_node_tree_t::parse_node_list_t nodes = this->parse_tree.find_nodes(list_node, symbol_argument);

    for (size_t i=0; i < nodes.size(); i++)
    {
        const parse_node_t *child = nodes.at(i);
        assert(child != NULL && child->type == symbol_argument);
        this->color_argument(*child);

        if (cmd_is_cd)
        {
            /* Mark this as an error if it's not 'help' and not a valid cd path */
            wcstring param = child->get_source(this->buff);
            if (expand_one(param, EXPAND_SKIP_CMDSUBST))
            {
                bool is_help = string_prefixes_string(param, L"--help") || string_prefixes_string(param, L"-h");
                if (!is_help && ! is_potential_cd_path(param, working_directory, PATH_EXPAND_TILDE, NULL))
                {
                    this->color_node(*child, highlight_spec_error);
                }
            }
        }
    }
}

void highlighter_t::color_redirection(const parse_node_t &redirection_node)
{
    assert(redirection_node.type == symbol_redirection);
    if (! redirection_node.has_source())
        return;

    const parse_node_t *redirection_primitive = this->parse_tree.get_child(redirection_node, 0, parse_token_type_redirection); //like 2>
    const parse_node_t *redirection_target = this->parse_tree.get_child(redirection_node, 1, parse_token_type_string); //like &1 or file path

    if (redirection_primitive != NULL)
    {
        wcstring target;
        const enum token_type redirect_type = this->parse_tree.type_for_redirection(redirection_node, this->buff, NULL, &target);

        /* We may get a TOK_NONE redirection type, e.g. if the redirection is invalid */
        this->color_node(*redirection_primitive, redirect_type == TOK_NONE ? highlight_spec_error : highlight_spec_redirection);

        /* Check if the argument contains a command substitution. If so, highlight it as a param even though it's a command redirection, and don't try to do any other validation. */
        if (parse_util_locate_cmdsubst(target.c_str(), NULL, NULL, true) != 0)
        {
            if (redirection_target != NULL)
                this->color_argument(*redirection_target);
        }
        else
        {
            /* No command substitution, so we can highlight the target file or fd. For example, disallow redirections into a non-existent directory */
            bool target_is_valid = true;

            if (! expand_one(target, EXPAND_SKIP_CMDSUBST))
            {
                /* Could not be expanded */
                target_is_valid = false;
            }
            else
            {
                /* Ok, we successfully expanded our target. Now verify that it works with this redirection. We will probably need it as a path (but not in the case of fd redirections */
                const wcstring target_path = apply_working_directory(target, this->working_directory);
                switch (redirect_type)
                {
                    case TOK_REDIRECT_FD:
                    {
                        /* target should be an fd. It must be all digits, and must not overflow. fish_wcstoi returns INT_MAX on overflow; we could instead check errno to disambiguiate this from a real INT_MAX fd, but instead we just disallow that. */
                        const wchar_t *target_cstr = target.c_str();
                        wchar_t *end = NULL;
                        int fd = fish_wcstoi(target_cstr, &end, 10);

                        /* The iswdigit check ensures there's no leading whitespace, the *end check ensures the entire string was consumed, and the numeric checks ensure the fd is at least zero and there was no overflow */
                        target_is_valid = (iswdigit(target_cstr[0]) && *end == L'\0' && fd >= 0 && fd < INT_MAX);
                    }
                    break;

                    case TOK_REDIRECT_IN:
                    {
                        /* Input redirections must have a readable non-directory */
                        struct stat buf = {};
                        target_is_valid = ! waccess(target_path, R_OK) && ! wstat(target_path, &buf) && ! S_ISDIR(buf.st_mode);
                    }
                    break;

                    case TOK_REDIRECT_OUT:
                    case TOK_REDIRECT_APPEND:
                    case TOK_REDIRECT_NOCLOB:
                    {
                        /* Test whether the file exists, and whether it's writable (possibly after creating it). access() returns failure if the file does not exist. */
                        bool file_exists = false, file_is_writable = false;
                        int err = 0;

                        struct stat buf = {};
                        if (wstat(target_path, &buf) < 0)
                        {
                            err = errno;
                        }

                        if (string_suffixes_string(L"/", target))
                        {
                            /* Redirections to things that are directories is definitely not allowed */
                            file_exists = false;
                            file_is_writable = false;
                        }
                        else if (err == 0)
                        {
                            /* No err. We can write to it if it's not a directory and we have permission */
                            file_exists = true;
                            file_is_writable = ! S_ISDIR(buf.st_mode) && ! waccess(target_path, W_OK);
                        }
                        else if (err == ENOENT)
                        {
                            /* File does not exist. Check if its parent directory is writable. */
                            wcstring parent = wdirname(target_path);

                            /* Ensure that the parent ends with the path separator. This will ensure that we get an error if the parent directory is not really a directory. */
                            if (! string_suffixes_string(L"/", parent))
                                parent.push_back(L'/');

                            /* Now the file is considered writable if the parent directory is writable */
                            file_exists = false;
                            file_is_writable = (0 == waccess(parent, W_OK));
                        }
                        else
                        {
                            /* Other errors we treat as not writable. This includes things like ENOTDIR. */
                            file_exists = false;
                            file_is_writable = false;
                        }

                        /* NOCLOB means that we must not overwrite files that exist */
                        target_is_valid = file_is_writable && !(file_exists && redirect_type == TOK_REDIRECT_NOCLOB);
                    }
                    break;

                    default:
                        /* We should not get here, since the node was marked as a redirection, but treat it as an error for paranoia */
                        target_is_valid = false;
                        break;
                }
            }

            if (redirection_target != NULL)
            {
                this->color_node(*redirection_target, target_is_valid ? highlight_spec_redirection : highlight_spec_error);
            }
        }
    }
}

// Color all of the redirections of the given command
void highlighter_t::color_redirections(const parse_node_t &list_node)
{
    const parse_node_tree_t::parse_node_list_t nodes = this->parse_tree.find_nodes(list_node, symbol_redirection);
    for (size_t i=0; i < nodes.size(); i++)
    {
        this->color_redirection(*nodes.at(i));
    }
}

/* Color all the children of the command with the given type */
void highlighter_t::color_children(const parse_node_t &parent, parse_token_type_t type, int color)
{
    for (node_offset_t idx=0; idx < parent.child_count; idx++)
    {
        const parse_node_t *child = this->parse_tree.get_child(parent, idx);
        if (child != NULL && child->type == type)
        {
            this->color_node(*child, color);
        }
    }
}

/* Determine if a command is valid */
static bool command_is_valid(const wcstring &cmd, enum parse_statement_decoration_t decoration, const wcstring &working_directory, const env_vars_snapshot_t &vars)
{
    /* Determine which types we check, based on the decoration */
    bool builtin_ok = true, function_ok = true, abbreviation_ok = true, command_ok = true, implicit_cd_ok = true;
    if (decoration == parse_statement_decoration_command)
    {
        builtin_ok = false;
        function_ok = false;
        abbreviation_ok = false;
        command_ok = true;
        implicit_cd_ok = false;
    }
    else if (decoration == parse_statement_decoration_builtin)
    {
        builtin_ok = true;
        function_ok = false;
        abbreviation_ok = false;
        command_ok = false;
        implicit_cd_ok = false;
    }

    /* Check them */
    bool is_valid = false;

    /* Builtins */
    if (! is_valid && builtin_ok)
        is_valid = builtin_exists(cmd);

    /* Functions */
    if (! is_valid && function_ok)
        is_valid = function_exists_no_autoload(cmd, vars);

    /* Abbreviations */
    if (! is_valid && abbreviation_ok)
        is_valid = expand_abbreviation(cmd, NULL);

    /* Regular commands */
    if (! is_valid && command_ok)
        is_valid = path_get_path(cmd, NULL, vars);

    /* Implicit cd */
    if (! is_valid && implicit_cd_ok)
        is_valid = path_can_be_implicit_cd(cmd, NULL, working_directory.c_str(), vars);

    /* Return what we got */
    return is_valid;
}

const highlighter_t::color_array_t & highlighter_t::highlight()
{
    ASSERT_IS_BACKGROUND_THREAD();

    const size_t length = buff.size();
    assert(this->buff.size() == this->color_array.size());

    if (length == 0)
        return color_array;

    /* Start out at zero */
    std::fill(this->color_array.begin(), this->color_array.end(), 0);

    /* Parse the buffer */
    parse_node_tree_t parse_tree;
    parse_tree_from_string(buff, parse_flag_continue_after_error | parse_flag_include_comments, &parse_tree, NULL);

#if 0
    const wcstring dump = parse_dump_tree(parse_tree, buff);
    fprintf(stderr, "%ls\n", dump.c_str());
#endif

    /* Walk the node tree */
    for (parse_node_tree_t::const_iterator iter = parse_tree.begin(); iter != parse_tree.end(); ++iter)
    {
        const parse_node_t &node = *iter;

        switch (node.type)
        {
                // Color direct string descendants, e.g. 'for' and 'in'.
            case symbol_for_header:
            case symbol_while_header:
            case symbol_begin_header:
            case symbol_function_header:
            case symbol_if_clause:
            case symbol_else_clause:
            case symbol_case_item:
            case symbol_switch_statement:
            case symbol_boolean_statement:
            case symbol_decorated_statement:
            case symbol_if_statement:
            {
                this->color_children(node, parse_token_type_string, highlight_spec_command);
                // Color the 'end'
                this->color_children(node, symbol_end_command, highlight_spec_command);
            }
            break;

            case parse_token_type_background:
            case parse_token_type_end:
            {
                this->color_node(node, highlight_spec_statement_terminator);
            }
            break;

            case symbol_plain_statement:
            {
                // Get the decoration from the parent
                enum parse_statement_decoration_t decoration = parse_tree.decoration_for_plain_statement(node);

                /* Color the command */
                const parse_node_t *cmd_node = parse_tree.get_child(node, 0, parse_token_type_string);
                if (cmd_node != NULL && cmd_node->has_source())
                {
                    bool is_valid_cmd = false;
                    wcstring cmd(buff, cmd_node->source_start, cmd_node->source_length);

                    /* Try expanding it. If we cannot, it's an error. */
                    bool expanded = expand_one(cmd, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES | EXPAND_SKIP_JOBS);
                    if (expanded && ! has_expand_reserved(cmd))
                    {
                        is_valid_cmd = command_is_valid(cmd, decoration, working_directory, vars);
                    }
                    this->color_node(*cmd_node, is_valid_cmd ? highlight_spec_command : highlight_spec_error);
                }
            }
            break;


            case symbol_arguments_or_redirections_list:
            case symbol_argument_list:
            {
                /* Only work on root lists, so that we don't re-color child lists */
                if (parse_tree.argument_list_is_root(node))
                {
                    this->color_arguments(node);
                    this->color_redirections(node);
                }
            }
            break;

            case parse_special_type_parse_error:
            case parse_special_type_tokenizer_error:
                this->color_node(node, highlight_spec_error);
                break;

            case parse_special_type_comment:
                this->color_node(node, highlight_spec_comment);
                break;

            default:
                break;
        }
    }

    if (this->cursor_pos <= this->buff.size())
    {
        /* If the cursor is over an argument, and that argument is a valid path, underline it */
        for (parse_node_tree_t::const_iterator iter = parse_tree.begin(); iter != parse_tree.end(); ++iter)
        {
            const parse_node_t &node = *iter;

            /* Must be an argument with source */
            if (node.type != symbol_argument || ! node.has_source())
                continue;

            /* See if this node contains the cursor. We check <= source_length so that, when backspacing (and the cursor is just beyond the last token), we may still underline it */
            if (this->cursor_pos >= node.source_start && this->cursor_pos - node.source_start <= node.source_length)
            {
                /* See if this is a valid path */
                if (node_is_potential_path(buff, node, working_directory))
                {
                    /* It is, underline it. */
                    for (size_t i=node.source_start; i < node.source_start + node.source_length; i++)
                    {
                        /* Don't color highlight_spec_error because it looks dorky. For example, trying to cd into a non-directory would show an underline and also red. */
                        if (highlight_get_primary(this->color_array.at(i)) != highlight_spec_error)
                        {
                            this->color_array.at(i) |= highlight_modifier_valid_path;
                        }
                    }
                }
            }
        }
    }

    return color_array;
}

void highlight_shell_new_parser(const wcstring &buff, std::vector<highlight_spec_t> &color, size_t pos, wcstring_list_t *error, const env_vars_snapshot_t &vars)
{
    /* Do something sucky and get the current working directory on this background thread. This should really be passed in. */
    const wcstring working_directory = env_get_pwd_slash();

    /* Highlight it! */
    highlighter_t highlighter(buff, pos, vars, working_directory);
    color = highlighter.highlight();
}

/**
   Perform quote and parenthesis highlighting on the specified string.
*/
static void highlight_universal_internal(const wcstring &buffstr, std::vector<highlight_spec_t> &color, size_t pos)
{
    assert(buffstr.size() == color.size());
    if (pos < buffstr.size())
    {

        /*
          Highlight matching quotes
        */
        if ((buffstr.at(pos) == L'\'') || (buffstr.at(pos) == L'\"'))
        {
            std::vector<size_t> lst;

            int level=0;
            wchar_t prev_q=0;

            const wchar_t * const buff = buffstr.c_str();
            const wchar_t *str = buff;

            int match_found=0;

            while (*str)
            {
                switch (*str)
                {
                    case L'\\':
                        str++;
                        break;
                    case L'\"':
                    case L'\'':
                        if (level == 0)
                        {
                            level++;
                            lst.push_back(str-buff);
                            prev_q = *str;
                        }
                        else
                        {
                            if (prev_q == *str)
                            {
                                size_t pos1, pos2;

                                level--;
                                pos1 = lst.back();
                                pos2 = str-buff;
                                if (pos1==pos || pos2==pos)
                                {
                                    color.at(pos1)|=highlight_make_background(highlight_spec_match);
                                    color.at(pos2)|=highlight_make_background(highlight_spec_match);
                                    match_found = 1;

                                }
                                prev_q = *str==L'\"'?L'\'':L'\"';
                            }
                            else
                            {
                                level++;
                                lst.push_back(str-buff);
                                prev_q = *str;
                            }
                        }

                        break;
                }
                if ((*str == L'\0'))
                    break;

                str++;
            }

            if (!match_found)
                color.at(pos) = highlight_make_background(highlight_spec_error);
        }

        /*
          Highlight matching parenthesis
        */
        const wchar_t c = buffstr.at(pos);
        if (wcschr(L"()[]{}", c))
        {
            int step = wcschr(L"({[", c)?1:-1;
            wchar_t dec_char = *(wcschr(L"()[]{}", c) + step);
            wchar_t inc_char = c;
            int level = 0;
            int match_found=0;
            for (long i=pos; i >= 0 && (size_t)i < buffstr.size(); i+=step)
            {
                const wchar_t test_char = buffstr.at(i);
                if (test_char == inc_char)
                    level++;
                if (test_char == dec_char)
                    level--;
                if (level == 0)
                {
                    long pos2 = i;
                    color.at(pos)|=highlight_spec_match<<16;
                    color.at(pos2)|=highlight_spec_match<<16;
                    match_found=1;
                    break;
                }
            }

            if (!match_found)
                color[pos] = highlight_make_background(highlight_spec_error);
        }
    }
}

void highlight_universal(const wcstring &buff, std::vector<highlight_spec_t> &color, size_t pos, wcstring_list_t *error, const env_vars_snapshot_t &vars)
{
    assert(buff.size() == color.size());
    std::fill(color.begin(), color.end(), 0);
    highlight_universal_internal(buff, color, pos);
}
