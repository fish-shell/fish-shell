/** \file builtin_complete.c Functions defining the complete builtin

Functions used for implementing the complete builtin.

*/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include <sys/types.h>
#include <termios.h>
#include <signal.h>

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "builtin.h"
#include "common.h"
#include "complete.h"
#include "wgetopt.h"
#include "parser.h"
#include "reader.h"


/**
   Internal storage for the builtin_complete_get_temporary_buffer() function.
*/
static const wchar_t *temporary_buffer;

/*
  builtin_complete_* are a set of rather silly looping functions that
  make sure that all the proper combinations of complete_add or
  complete_remove get called. This is needed since complete allows you
  to specify multiple switches on a single commandline, like 'complete
  -s a -s b -s c', but the complete_add function only accepts one
  short switch and one long switch.
*/

/**
   Silly function
*/
static void  builtin_complete_add2(const wchar_t *cmd,
                                   int cmd_type,
                                   const wchar_t *short_opt,
                                   const wcstring_list_t &gnu_opt,
                                   const wcstring_list_t &old_opt,
                                   int result_mode,
                                   const wchar_t *condition,
                                   const wchar_t *comp,
                                   const wchar_t *desc,
                                   int flags)
{
    size_t i;
    const wchar_t *s;

    for (s=short_opt; *s; s++)
    {
        complete_add(cmd,
                     cmd_type,
                     *s,
                     0,
                     0,
                     result_mode,
                     condition,
                     comp,
                     desc,
                     flags);
    }

    for (i=0; i<gnu_opt.size(); i++)
    {
        complete_add(cmd,
                     cmd_type,
                     0,
                     gnu_opt.at(i).c_str(),
                     0,
                     result_mode,
                     condition,
                     comp,
                     desc,
                     flags);
    }

    for (i=0; i<old_opt.size(); i++)
    {
        complete_add(cmd,
                     cmd_type,
                     0,
                     old_opt.at(i).c_str(),
                     1,
                     result_mode,
                     condition,
                     comp,
                     desc,
                     flags);
    }

    if (old_opt.empty() && gnu_opt.empty() && wcslen(short_opt) == 0)
    {
        complete_add(cmd,
                     cmd_type,
                     0,
                     0,
                     0,
                     result_mode,
                     condition,
                     comp,
                     desc,
                     flags);
    }
}

/**
   Silly function
*/
static void  builtin_complete_add(const wcstring_list_t &cmd,
                                  const wcstring_list_t &path,
                                  const wchar_t *short_opt,
                                  wcstring_list_t &gnu_opt,
                                  wcstring_list_t &old_opt,
                                  int result_mode,
                                  int authoritative,
                                  const wchar_t *condition,
                                  const wchar_t *comp,
                                  const wchar_t *desc,
                                  int flags)
{
    for (size_t i=0; i<cmd.size(); i++)
    {
        builtin_complete_add2(cmd.at(i).c_str(),
                              COMMAND,
                              short_opt,
                              gnu_opt,
                              old_opt,
                              result_mode,
                              condition,
                              comp,
                              desc,
                              flags);

        if (authoritative != -1)
        {
            complete_set_authoritative(cmd.at(i).c_str(),
                                       COMMAND,
                                       authoritative);
        }

    }

    for (size_t i=0; i<path.size(); i++)
    {
        builtin_complete_add2(path.at(i).c_str(),
                              PATH,
                              short_opt,
                              gnu_opt,
                              old_opt,
                              result_mode,
                              condition,
                              comp,
                              desc,
                              flags);

        if (authoritative != -1)
        {
            complete_set_authoritative(path.at(i).c_str(),
                                       PATH,
                                       authoritative);
        }

    }
}

/**
   Silly function
*/
static void builtin_complete_remove3(const wchar_t *cmd,
                                     int cmd_type,
                                     wchar_t short_opt,
                                     const wcstring_list_t &long_opt)
{
    for (size_t i=0; i<long_opt.size(); i++)
    {
        complete_remove(cmd,
                        cmd_type,
                        short_opt,
                        long_opt.at(i).c_str());
    }
}

/**
   Silly function
*/
static void  builtin_complete_remove2(const wchar_t *cmd,
                                      int cmd_type,
                                      const wchar_t *short_opt,
                                      const wcstring_list_t &gnu_opt,
                                      const wcstring_list_t &old_opt)
{
    const wchar_t *s = (wchar_t *)short_opt;
    if (*s)
    {
        for (; *s; s++)
        {
            if (old_opt.empty() && gnu_opt.empty())
            {
                complete_remove(cmd,
                                cmd_type,
                                *s,
                                0);

            }
            else
            {
                builtin_complete_remove3(cmd,
                                         cmd_type,
                                         *s,
                                         gnu_opt);
                builtin_complete_remove3(cmd,
                                         cmd_type,
                                         *s,
                                         old_opt);
            }
        }
    }
    else
    {
        builtin_complete_remove3(cmd,
                                 cmd_type,
                                 0,
                                 gnu_opt);
        builtin_complete_remove3(cmd,
                                 cmd_type,
                                 0,
                                 old_opt);

    }


}

/**
   Silly function
*/
static void  builtin_complete_remove(const wcstring_list_t &cmd,
                                     const wcstring_list_t &path,
                                     const wchar_t *short_opt,
                                     const wcstring_list_t &gnu_opt,
                                     const wcstring_list_t &old_opt)
{
    for (size_t i=0; i<cmd.size(); i++)
    {
        builtin_complete_remove2(cmd.at(i).c_str(),
                                 COMMAND,
                                 short_opt,
                                 gnu_opt,
                                 old_opt);
    }

    for (size_t i=0; i<path.size(); i++)
    {
        builtin_complete_remove2(path.at(i).c_str(),
                                 PATH,
                                 short_opt,
                                 gnu_opt,
                                 old_opt);
    }

}


const wchar_t *builtin_complete_get_temporary_buffer()
{
    ASSERT_IS_MAIN_THREAD();
    return temporary_buffer;
}

/**
   The complete builtin. Used for specifying programmable
   tab-completions. Calls the functions in complete.c for any heavy
   lifting. Defined in builtin_complete.c
*/
static int builtin_complete(parser_t &parser, wchar_t **argv)
{
    ASSERT_IS_MAIN_THREAD();
    bool res=false;
    int argc=0;
    int result_mode=SHARED;
    int remove = 0;
    int authoritative = -1;

    wcstring short_opt;
    wcstring_list_t gnu_opt, old_opt;
    const wchar_t *comp=L"", *desc=L"", *condition=L"";

    bool do_complete = false;
    wcstring do_complete_param;

    wcstring_list_t cmd;
    wcstring_list_t path;

    static int recursion_level=0;

    argc = builtin_count_args(argv);

    woptind=0;

    while (! res)
    {
        static const struct woption
                long_options[] =
        {
            { L"exclusive", no_argument, 0, 'x' },
            { L"no-files", no_argument, 0, 'f' },
            { L"require-parameter", no_argument, 0, 'r' },
            { L"path", required_argument, 0, 'p' },
            { L"command", required_argument, 0, 'c' },
            { L"short-option", required_argument, 0, 's' },
            { L"long-option", required_argument, 0, 'l' },
            { L"old-option", required_argument, 0, 'o' },
            { L"description", required_argument, 0, 'd' },
            { L"arguments", required_argument, 0, 'a' },
            { L"erase", no_argument, 0, 'e' },
            { L"unauthoritative", no_argument, 0, 'u' },
            { L"authoritative", no_argument, 0, 'A' },
            { L"condition", required_argument, 0, 'n' },
            { L"do-complete", optional_argument, 0, 'C' },
            { L"help", no_argument, 0, 'h' },
            { 0, 0, 0, 0 }
        };

        int opt_index = 0;

        int opt = wgetopt_long(argc,
                               argv,
                               L"a:c:p:s:l:o:d:frxeuAn:C::h",
                               long_options,
                               &opt_index);
        if (opt == -1)
            break;

        switch (opt)
        {
            case 0:
                if (long_options[opt_index].flag != 0)
                    break;
                append_format(stderr_buffer,
                              BUILTIN_ERR_UNKNOWN,
                              argv[0],
                              long_options[opt_index].name);
                builtin_print_help(parser, argv[0], stderr_buffer);


                res = true;
                break;

            case 'x':
                result_mode |= EXCLUSIVE;
                break;

            case 'f':
                result_mode |= NO_FILES;
                break;

            case 'r':
                result_mode |= NO_COMMON;
                break;

            case 'p':
            case 'c':
            {
                wcstring tmp;
                if (unescape_string(woptarg, &tmp, UNESCAPE_SPECIAL))
                {
                    if (opt=='p')
                        path.push_back(tmp);
                    else
                        cmd.push_back(tmp);
                }
                else
                {
                    append_format(stderr_buffer, L"%ls: Invalid token '%ls'\n", argv[0], woptarg);
                    res = true;
                }
                break;
            }

            case 'd':
                desc = woptarg;
                break;

            case 'u':
                authoritative=0;
                break;

            case 'A':
                authoritative=1;
                break;

            case 's':
                short_opt.append(woptarg);
                break;

            case 'l':
                gnu_opt.push_back(woptarg);
                break;

            case 'o':
                old_opt.push_back(woptarg);
                break;

            case 'a':
                comp = woptarg;
                break;

            case 'e':
                remove = 1;
                break;

            case 'n':
                condition = woptarg;
                break;

            case 'C':
                do_complete = true;
                do_complete_param = woptarg ? woptarg : reader_get_buffer();
                break;

            case 'h':
                builtin_print_help(parser, argv[0], stdout_buffer);
                return 0;

            case '?':
                builtin_unknown_option(parser, argv[0], argv[woptind-1]);
                res = true;
                break;

        }

    }

    if (!res)
    {
        if (condition && wcslen(condition))
        {
            const wcstring condition_string = condition;
            parse_error_list_t errors;
            if (parse_util_detect_errors(condition_string, &errors, false /* do not accept incomplete */))
            {
                append_format(stderr_buffer,
                              L"%ls: Condition '%ls' contained a syntax error",
                              argv[0],
                              condition);
                for (size_t i=0; i < errors.size(); i++)
                {
                    append_format(stderr_buffer, L"\n%s: ", argv[0]);
                    stderr_buffer.append(errors.at(i).describe(condition_string));
                }
                res = true;
            }
        }
    }

    if (!res)
    {
        if (comp && wcslen(comp))
        {
            wcstring prefix;
            if (argv[0])
            {
                prefix.append(argv[0]);
                prefix.append(L": ");
            }

            wcstring err_text;
            if (parser.detect_errors_in_argument_list(comp, &err_text, prefix.c_str()))
            {
                append_format(stderr_buffer,
                              L"%ls: Completion '%ls' contained a syntax error\n",
                              argv[0],
                              comp);
                stderr_buffer.append(err_text);
                stderr_buffer.push_back(L'\n');
                res = true;
            }
        }
    }

    if (!res)
    {
        if (do_complete)
        {
            const wchar_t *token;

            parse_util_token_extent(do_complete_param.c_str(), do_complete_param.size(), &token, 0, 0, 0);

            const wchar_t *prev_temporary_buffer = temporary_buffer;
            temporary_buffer = do_complete_param.c_str();

            if (recursion_level < 1)
            {
                recursion_level++;

                std::vector<completion_t> comp;
                complete(do_complete_param, comp, COMPLETION_REQUEST_DEFAULT);

                for (size_t i=0; i< comp.size() ; i++)
                {
                    const completion_t &next =  comp.at(i);

                    /* Make a fake commandline, and then apply the completion to it.  */
                    const wcstring faux_cmdline = token;
                    size_t tmp_cursor = faux_cmdline.size();
                    wcstring faux_cmdline_with_completion = completion_apply_to_command_line(next.completion, next.flags, faux_cmdline, &tmp_cursor, false);

                    /* completion_apply_to_command_line will append a space unless COMPLETE_NO_SPACE is set. We don't want to set COMPLETE_NO_SPACE because that won't close quotes. What we want is to close the quote, but not append the space. So we just look for the space and clear it. */
                    if (!(next.flags & COMPLETE_NO_SPACE) && string_suffixes_string(L" ", faux_cmdline_with_completion))
                    {
                        faux_cmdline_with_completion.resize(faux_cmdline_with_completion.size() - 1);
                    }

                    /* The input data is meant to be something like you would have on the command line, e.g. includes backslashes. The output should be raw, i.e. unescaped. So we need to unescape the command line. See #1127 */
                    unescape_string_in_place(&faux_cmdline_with_completion, UNESCAPE_DEFAULT);
                    stdout_buffer.append(faux_cmdline_with_completion);

                    /* Append any description */
                    if (! next.description.empty())
                    {
                        stdout_buffer.push_back(L'\t');
                        stdout_buffer.append(next.description);
                    }
                    stdout_buffer.push_back(L'\n');
                }

                recursion_level--;
            }

            temporary_buffer = prev_temporary_buffer;

        }
        else if (woptind != argc)
        {
            append_format(stderr_buffer,
                          _(L"%ls: Too many arguments\n"),
                          argv[0]);
            builtin_print_help(parser, argv[0], stderr_buffer);

            res = true;
        }
        else if (cmd.empty() && path.empty())
        {
            /* No arguments specified, meaning we print the definitions of
             * all specified completions to stdout.*/
            complete_print(stdout_buffer);
        }
        else
        {
            int flags = COMPLETE_AUTO_SPACE;

            if (remove)
            {
                builtin_complete_remove(cmd,
                                        path,
                                        short_opt.c_str(),
                                        gnu_opt,
                                        old_opt);
            }
            else
            {
                builtin_complete_add(cmd,
                                     path,
                                     short_opt.c_str(),
                                     gnu_opt,
                                     old_opt,
                                     result_mode,
                                     authoritative,
                                     condition,
                                     comp,
                                     desc,
                                     flags);
            }

        }
    }

    return res ? 1 : 0;
}
