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

/**
   Number of elements in the highlight_var array
*/
#define VAR_COUNT ( sizeof(highlight_var)/sizeof(wchar_t *) )

static void highlight_universal_internal(const wcstring &buff, std::vector<int> &color, size_t pos);

/**
   The environment variables used to specify the color of different tokens.
*/
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
    L"fish_color_valid_path",
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

rgb_color_t highlight_get_color(int highlight, bool is_background)
{
    size_t idx=0;
    rgb_color_t result;

    if (highlight < 0)
        return rgb_color_t::normal();
    if (highlight > (1<<VAR_COUNT))
        return rgb_color_t::normal();
    for (size_t i=0; i<VAR_COUNT; i++)
    {
        if (highlight & (1<<i))
        {
            idx = i;
            break;
        }
    }

    env_var_t val_wstr = env_get_string(highlight_var[idx]);

//  debug( 1, L"%d -> %d -> %ls", highlight, idx, val );

    if (val_wstr.missing())
        val_wstr = env_get_string(highlight_var[0]);

    if (! val_wstr.missing())
        result = parse_color(val_wstr, is_background);

    if (highlight & HIGHLIGHT_VALID_PATH)
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
static void highlight_param(const wcstring &buffstr, std::vector<int> &colors, wcstring_list_t *error)
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
                            colors.at(start_pos) = HIGHLIGHT_ESCAPE;
                            colors.at(in_pos+1) = normal_status;
                        }
                    }
                    else if (buff[in_pos]==L',')
                    {
                        if (bracket_count)
                        {
                            colors.at(start_pos) = HIGHLIGHT_ESCAPE;
                            colors.at(in_pos+1) = normal_status;
                        }
                    }
                    else if (wcschr(L"abefnrtv*?$(){}[]'\"<>^ \\#;|&", buff[in_pos]))
                    {
                        colors.at(start_pos)=HIGHLIGHT_ESCAPE;
                        colors.at(in_pos+1)=normal_status;
                    }
                    else if (wcschr(L"c", buff[in_pos]))
                    {
                        colors.at(start_pos)=HIGHLIGHT_ESCAPE;
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
                            colors.at(start_pos) = HIGHLIGHT_ESCAPE;
                            colors.at(in_pos+1) = normal_status;
                        }
                        else
                        {
                            colors.at(start_pos) = HIGHLIGHT_ERROR;
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
                                colors.at(in_pos) = HIGHLIGHT_OPERATOR;
                                colors.at(in_pos+1) = normal_status;
                            }
                            break;
                        }

                        case L'$':
                        {
                            wchar_t n = buff[in_pos+1];
                            colors.at(in_pos) = (n==L'$'||wcsvarchr(n))? HIGHLIGHT_OPERATOR:HIGHLIGHT_ERROR;
                            colors.at(in_pos+1) = normal_status;
                            break;
                        }


                        case L'*':
                        case L'?':
                        case L'(':
                        case L')':
                        {
                            colors.at(in_pos) = HIGHLIGHT_OPERATOR;
                            colors.at(in_pos+1) = normal_status;
                            break;
                        }

                        case L'{':
                        {
                            colors.at(in_pos) = HIGHLIGHT_OPERATOR;
                            colors.at(in_pos+1) = normal_status;
                            bracket_count++;
                            break;
                        }

                        case L'}':
                        {
                            colors.at(in_pos) = HIGHLIGHT_OPERATOR;
                            colors.at(in_pos+1) = normal_status;
                            bracket_count--;
                            break;
                        }

                        case L',':
                        {
                            if (bracket_count)
                            {
                                colors.at(in_pos) = HIGHLIGHT_OPERATOR;
                                colors.at(in_pos+1) = normal_status;
                            }

                            break;
                        }

                        case L'\'':
                        {
                            colors.at(in_pos) = HIGHLIGHT_QUOTE;
                            mode = e_single_quoted;
                            break;
                        }

                        case L'\"':
                        {
                            colors.at(in_pos) = HIGHLIGHT_QUOTE;
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
                            colors.at(start_pos) = HIGHLIGHT_ESCAPE;
                            colors.at(in_pos+1) = HIGHLIGHT_QUOTE;
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
                                colors.at(start_pos) = HIGHLIGHT_ESCAPE;
                                colors.at(in_pos+1) = HIGHLIGHT_QUOTE;
                                break;
                            }
                        }
                        break;
                    }

                    case '$':
                    {
                        wchar_t n = buff[in_pos+1];
                        colors.at(in_pos) = (n==L'$'||wcsvarchr(n))? HIGHLIGHT_OPERATOR:HIGHLIGHT_ERROR;
                        colors.at(in_pos+1) = HIGHLIGHT_QUOTE;
                        break;
                    }

                }
                break;
            }
        }
    }
}

static int has_expand_reserved(const wchar_t *str)
{
    while (*str)
    {
        if (*str >= EXPAND_RESERVED &&
                *str <= EXPAND_RESERVED_END)
        {
            return 1;
        }
        str++;
    }
    return 0;
}

/* Parse a command line. Return by reference the last command, its arguments, and the offset in the string of the beginning of the last argument. This is used by autosuggestions */
static bool autosuggest_parse_command(const wcstring &str, wcstring *out_command, wcstring_list_t *out_arguments, int *out_last_arg_pos)
{
    if (str.empty())
        return false;

    wcstring cmd;
    wcstring_list_t args;
    int arg_pos = -1;

    bool had_cmd = false;
    tokenizer_t tok(str.c_str(), TOK_ACCEPT_UNFINISHED | TOK_SQUASH_ERRORS);
    for (; tok_has_next(&tok); tok_next(&tok))
    {
        int last_type = tok_last_type(&tok);

        switch (last_type)
        {
            case TOK_STRING:
            {
                if (had_cmd)
                {
                    /* Parameter to the command. We store these escaped. */
                    args.push_back(tok_last(&tok));
                    arg_pos = tok_get_pos(&tok);
                }
                else
                {
                    /* Command. First check that the command actually exists. */
                    wcstring local_cmd = tok_last(&tok);
                    bool expanded = expand_one(cmd, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES);
                    if (! expanded || has_expand_reserved(cmd.c_str()))
                    {
                        /* We can't expand this cmd, ignore it */
                    }
                    else
                    {
                        bool is_subcommand = false;
                        int mark = tok_get_pos(&tok);

                        if (parser_keywords_is_subcommand(cmd))
                        {
                            int sw;
                            tok_next(&tok);

                            sw = parser_keywords_is_switch(tok_last(&tok));
                            if (!parser_keywords_is_block(cmd) &&
                                    sw == ARG_SWITCH)
                            {
                                /* It's an argument to the subcommand itself */
                            }
                            else
                            {
                                if (sw == ARG_SKIP)
                                    mark = tok_get_pos(&tok);
                                is_subcommand = true;
                            }
                            tok_set_pos(&tok, mark);
                        }

                        if (!is_subcommand)
                        {
                            /* It's really a command */
                            had_cmd = true;
                            cmd = local_cmd;
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
                    break;
                }
                tok_next(&tok);
                break;
            }

            case TOK_PIPE:
            case TOK_BACKGROUND:
            case TOK_END:
            {
                had_cmd = false;
                cmd.clear();
                args.clear();
                arg_pos = -1;
                break;
            }

            case TOK_COMMENT:
            case TOK_ERROR:
            default:
            {
                break;
            }
        }
    }

    /* Remember our command if we have one */
    if (had_cmd)
    {
        if (out_command) out_command->swap(cmd);
        if (out_arguments) out_arguments->swap(args);
        if (out_last_arg_pos) *out_last_arg_pos = arg_pos;
    }
    return had_cmd;
}


/* We have to return an escaped string here */
bool autosuggest_suggest_special(const wcstring &str, const wcstring &working_directory, wcstring &outSuggestion)
{
    if (str.empty())
        return false;

    ASSERT_IS_BACKGROUND_THREAD();

    /* Parse the string */
    wcstring parsed_command;
    wcstring_list_t parsed_arguments;
    int parsed_last_arg_pos = -1;
    if (! autosuggest_parse_command(str, &parsed_command, &parsed_arguments, &parsed_last_arg_pos))
    {
        return false;
    }

    bool result = false;
    if (parsed_command == L"cd" && ! parsed_arguments.empty())
    {
        /* We can possibly handle this specially */
        const wcstring escaped_dir = parsed_arguments.back();
        wcstring suggested_path;

        /* We always return true because we recognized the command. This prevents us from falling back to dumber algorithms; for example we won't suggest a non-directory for the cd command. */
        result = true;
        outSuggestion.clear();

        /* Unescape the parameter */
        wcstring unescaped_dir = escaped_dir;
        bool unescaped = unescape_string(unescaped_dir, UNESCAPE_INCOMPLETE);

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
            outSuggestion = str;
            outSuggestion.erase(parsed_last_arg_pos);
            if (quote != L'\0') outSuggestion.push_back(quote);
            outSuggestion.append(escaped_suggested_path);
            if (quote != L'\0') outSuggestion.push_back(quote);
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
    wcstring_list_t parsed_arguments;
    int parsed_last_arg_pos = -1;
    if (! autosuggest_parse_command(item.str(), &parsed_command, &parsed_arguments, &parsed_last_arg_pos))
        return false;

    if (parsed_command == L"cd" && ! parsed_arguments.empty())
    {
        /* We can possibly handle this specially */
        wcstring dir = parsed_arguments.back();
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
static void tokenize(const wchar_t * const buff, std::vector<int> &color, const size_t pos, wcstring_list_t *error, const wcstring &working_directory, const env_vars_snapshot_t &vars)
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

    std::fill(color.begin(), color.end(), -1);

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
                            color.at(tok_get_pos(&tok)) = HIGHLIGHT_PARAM;
                        }
                        else if (accept_switches)
                        {
                            if (complete_is_valid_option(last_cmd, param, error, false /* no autoload */))
                                color.at(tok_get_pos(&tok)) = HIGHLIGHT_PARAM;
                            else
                                color.at(tok_get_pos(&tok)) = HIGHLIGHT_ERROR;
                        }
                        else
                        {
                            color.at(tok_get_pos(&tok)) = HIGHLIGHT_PARAM;
                        }
                    }
                    else
                    {
                        color.at(tok_get_pos(&tok)) = HIGHLIGHT_PARAM;
                    }

                    if (cmd == L"cd")
                    {
                        wcstring dir = tok_last(&tok);
                        if (expand_one(dir, EXPAND_SKIP_CMDSUBST))
                        {
                            int is_help = string_prefixes_string(dir, L"--help") || string_prefixes_string(dir, L"-h");
                            if (!is_help && ! is_potential_cd_path(dir, working_directory, PATH_EXPAND_TILDE, NULL))
                            {
                                color.at(tok_get_pos(&tok)) = HIGHLIGHT_ERROR;
                            }
                        }
                    }

                    /* Highlight the parameter. highlight_param wants to write one more color than we have characters (hysterical raisins) so allocate one more in the vector. But don't copy it back. */
                    const wcstring param_str = param;
                    size_t tok_pos = tok_get_pos(&tok);

                    std::vector<int>::const_iterator where = color.begin() + tok_pos;
                    std::vector<int> subcolors(where, where + param_str.size());
                    subcolors.push_back(-1);
                    highlight_param(param_str, subcolors, error);

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
                    if (! expanded || has_expand_reserved(cmd.c_str()))
                    {
                        color.at(tok_get_pos(&tok)) = HIGHLIGHT_ERROR;
                    }
                    else
                    {
                        bool is_cmd = false;
                        int is_subcommand = 0;
                        int mark = tok_get_pos(&tok);
                        color.at(tok_get_pos(&tok)) = use_builtin ? HIGHLIGHT_COMMAND : HIGHLIGHT_ERROR;

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
                                    color.at(tok_get_pos(&tok)) = HIGHLIGHT_PARAM;
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
                                color.at(tok_get_pos(&tok)) = HIGHLIGHT_COMMAND;
                            }
                            else
                            {
                                if (error)
                                {
                                    error->push_back(format_string(L"Unknown command \'%ls\'", cmd.c_str()));
                                }
                                color.at(tok_get_pos(&tok)) = (HIGHLIGHT_ERROR);
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
                    color.at(tok_get_pos(&tok)) = HIGHLIGHT_ERROR;
                    if (error)
                        error->push_back(L"Redirection without a command");
                    break;
                }

                wcstring target_str;
                const wchar_t *target=NULL;

                color.at(tok_get_pos(&tok)) = HIGHLIGHT_REDIRECTION;
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
                            color.at(pos) = HIGHLIGHT_ERROR;
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
                            color.at(tok_get_pos(&tok)) = HIGHLIGHT_ERROR;
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
                            color.at(tok_get_pos(&tok)) = HIGHLIGHT_ERROR;
                            if (error)
                                error->push_back(format_string(L"File \'%ls\' does not exist", target));
                        }
                    }
                    if (last_type == TOK_REDIRECT_NOCLOB)
                    {
                        if (wstat(target, &buff) != -1)
                        {
                            color.at(tok_get_pos(&tok)) = HIGHLIGHT_ERROR;
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
                    color.at(tok_get_pos(&tok)) = HIGHLIGHT_END;
                    had_cmd = 0;
                    use_command  = 1;
                    use_function = 1;
                    use_builtin  = 1;
                    accept_switches = 1;
                }
                else
                {
                    color.at(tok_get_pos(&tok)) = HIGHLIGHT_ERROR;
                    if (error)
                        error->push_back(L"No job to put in background");
                }

                break;
            }

            case TOK_END:
            {
                color.at(tok_get_pos(&tok)) = HIGHLIGHT_END;
                had_cmd = 0;
                use_command  = 1;
                use_function = 1;
                use_builtin  = 1;
                accept_switches = 1;
                break;
            }

            case TOK_COMMENT:
            {
                color.at(tok_get_pos(&tok)) = HIGHLIGHT_COMMENT;
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
                color.at(tok_get_pos(&tok)) = HIGHLIGHT_ERROR;
                break;
            }
        }
    }
}


// PCA This function does I/O, (calls is_potential_path, path_get_path, maybe others) and so ought to only run on a background thread
void highlight_shell(const wcstring &buff, std::vector<int> &color, size_t pos, wcstring_list_t *error, const env_vars_snapshot_t &vars)
{
    ASSERT_IS_BACKGROUND_THREAD();

    const size_t length = buff.size();
    assert(buff.size() == color.size());


    if (length == 0)
        return;

    std::fill(color.begin(), color.end(), -1);

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
        std::vector<int> subcolors(len, -1);

        highlight_shell(begin+1, subcolors, -1, error, vars);

        // insert subcolors
        std::copy(subcolors.begin(), subcolors.end(), color.begin() + start);

        // highlight the end of the subcommand
        assert(end >= subbuff);
        if ((size_t)(end - subbuff) < length)
        {
            color.at(end-subbuff)=HIGHLIGHT_OPERATOR;
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
        int &current_val = color.at(i);
        if (current_val >= 0)
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
            const wcstring_list_t working_directory_list(1, working_directory);
            if (unescape_string(token, 1))
            {
                /* Big hack: is_potential_path expects a tilde, but unescape_string gives us HOME_DIRECTORY. Put it back. */
                if (! token.empty() && token.at(0) == HOME_DIRECTORY)
                    token.at(0) = L'~';
                if (is_potential_path(token, working_directory_list, PATH_EXPAND_TILDE))
                {
                    for (ptrdiff_t i=tok_begin-cbuff; i < (tok_end-cbuff); i++)
                    {
                        // Don't color HIGHLIGHT_ERROR because it looks dorky. For example, trying to cd into a non-directory would show an underline and also red.
                        if (!(color.at(i) & HIGHLIGHT_ERROR))
                        {
                            color.at(i) |= HIGHLIGHT_VALID_PATH;
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



/**
   Perform quote and parenthesis highlighting on the specified string.
*/
static void highlight_universal_internal(const wcstring &buffstr, std::vector<int> &color, size_t pos)
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
                                    color.at(pos1)|=HIGHLIGHT_MATCH<<16;
                                    color.at(pos2)|=HIGHLIGHT_MATCH<<16;
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
                color.at(pos) = HIGHLIGHT_ERROR<<16;
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
                    color.at(pos)|=HIGHLIGHT_MATCH<<16;
                    color.at(pos2)|=HIGHLIGHT_MATCH<<16;
                    match_found=1;
                    break;
                }
            }

            if (!match_found)
                color[pos] = HIGHLIGHT_ERROR<<16;
        }
    }
}

void highlight_universal(const wcstring &buff, std::vector<int> &color, size_t pos, wcstring_list_t *error, const env_vars_snapshot_t &vars)
{
    assert(buff.size() == color.size());
    std::fill(color.begin(), color.end(), 0);
    highlight_universal_internal(buff, color, pos);
}
