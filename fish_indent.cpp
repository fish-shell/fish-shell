/*
Copyright (C) 2005-2008 Axel Liljencrantz

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


/** \file fish_indent.cpp
  The fish_indent proegram.
*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <locale.h>

#include "fallback.h"
#include "util.h"
#include "common.h"
#include "wutil.h"
#include "tokenizer.h"
#include "print_help.h"
#include "parser_keywords.h"

/**
   The string describing the single-character options accepted by the main fish binary
*/
#define GETOPT_STRING "hvi"

/**
   Read the entire contents of a file into the specified string
 */
static void read_file(FILE *f, wcstring &b)
{
    while (1)
    {
        errno=0;
        wint_t c = fgetwc(f);
        if (c == WEOF)
        {
            if (errno)
            {
                wperror(L"fgetwc");
                exit(1);
            }

            break;
        }
        b.push_back((wchar_t)c);
    }
}

/**
   Insert the specified number of tabs into the output buffer
 */
static void insert_tabs(wcstring &out, int indent)
{
    if (indent > 0)
        out.append((size_t)indent, L'\t');

}

/**
   Indent the specified input
 */
static int indent(wcstring &out, const wcstring &in, int flags)
{
    int res=0;
    int is_command = 1;
    int indent = 0;
    int do_indent = 1;
    int prev_type = 0;
    int prev_prev_type = 0;

    tokenizer_t tok(in.c_str(), TOK_SHOW_COMMENTS);
    for (; tok_has_next(&tok); tok_next(&tok))
    {
        int type = tok_last_type(&tok);
        const wchar_t *last = tok_last(&tok);

        switch (type)
        {
            case TOK_STRING:
            {
                if (is_command)
                {
                    int next_indent = indent;
                    is_command = 0;

                    wcstring unesc = last;
                    unescape_string(unesc, UNESCAPE_SPECIAL);

                    if (parser_keywords_is_block(unesc))
                    {
                        next_indent++;
                    }
                    else if (unesc == L"else")
                    {
                        indent--;
                    }
                    /* case should have the same indent level as switch*/
                    else if (unesc == L"case")
                    {
                        indent--;
                    }
                    else if (unesc == L"end")
                    {
                        indent--;
                        next_indent--;
                    }


                    if (do_indent && flags && prev_type != TOK_PIPE)
                    {
                        insert_tabs(out, indent);
                    }

                    append_format(out, L"%ls", last);

                    indent = next_indent;

                }
                else
                {
                    if (prev_type != TOK_REDIRECT_FD)
                        out.append(L" ");
                    out.append(last);
                }

                break;
            }

            case TOK_END:
            {
                if (prev_type != TOK_END || prev_prev_type != TOK_END)
                    out.append(L"\n");
                do_indent = 1;
                is_command = 1;
                break;
            }

            case TOK_PIPE:
            {
                out.append(L" ");
                if (last[0] == '2' && !last[1])
                {
                    out.append(L"^");
                }
                else if (last[0] != '1' || last[1])
                {
                    out.append(last);
                    out.append(L">");
                }
                out.append(L" | ");
                is_command = 1;
                break;
            }

            case TOK_REDIRECT_OUT:
            {
                out.append(L" ");
                if (wcscmp(last, L"2") == 0)
                {
                    out.append(L"^");
                }
                else
                {
                    if (wcscmp(last, L"1") != 0)
                        out.append(last);
                    out.append(L"> ");
                }
                break;
            }

            case TOK_REDIRECT_APPEND:
            {
                out.append(L" ");
                if (wcscmp(last, L"2") == 0)
                {
                    out.append(L"^^");
                }
                else
                {
                    if (wcscmp(last, L"1") != 0)
                        out.append(last);
                    out.append(L">> ");
                }
                break;
            }

            case TOK_REDIRECT_IN:
            {
                out.append(L" ");
                if (wcscmp(last, L"0") != 0)
                    out.append(last);
                out.append(L"< ");
                break;
            }

            case TOK_REDIRECT_FD:
            {
                out.append(L" ");
                if (wcscmp(last, L"1") != 0)
                    out.append(last);
                out.append(L">& ");
                break;
            }

            case TOK_BACKGROUND:
            {
                out.append(L"&\n");
                do_indent = 1;
                is_command = 1;
                break;
            }

            case TOK_COMMENT:
            {
                if (do_indent && flags)
                {
                    insert_tabs(out, indent);
                }

                append_format(out, L"%ls", last);
                do_indent = 1;
                break;
            }

            default:
            {
                debug(0, L"Unknown token '%ls'", last);
                exit(1);
            }
        }

        prev_prev_type = prev_type;
        prev_type = type;

    }

    return res;
}

/**
   Remove any prefix and suffix newlines from the specified
   string.
 */
static void trim(wcstring &str)
{
    if (str.empty())
        return;

    size_t pos = str.find_first_not_of(L" \n");
    if (pos > 0)
        str.erase(0, pos);

    pos = str.find_last_not_of(L" \n");
    if (pos != wcstring::npos && pos + 1 < str.length())
        str.erase(pos + 1);
}


/**
   The main mathod. Run the program.
 */
int main(int argc, char **argv)
{
    int do_indent=1;
    set_main_thread();
    setup_fork_guards();

    wsetlocale(LC_ALL, L"");
    program_name=L"fish_indent";

    while (1)
    {
        static struct option
                long_options[] =
        {
            {
                "no-indent", no_argument, 0, 'i'
            }
            ,
            {
                "help", no_argument, 0, 'h'
            }
            ,
            {
                "version", no_argument, 0, 'v'
            }
            ,
            {
                0, 0, 0, 0
            }
        }
        ;

        int opt_index = 0;

        int opt = getopt_long(argc,
                              argv,
                              GETOPT_STRING,
                              long_options,
                              &opt_index);

        if (opt == -1)
            break;

        switch (opt)
        {
            case 0:
            {
                break;
            }

            case 'h':
            {
                print_help("fish_indent", 1);
                exit(0);
                break;
            }

            case 'v':
            {
                fwprintf(stderr,
                         _(L"%ls, version %s\n"),
                         program_name,
                         FISH_BUILD_VERSION);
                exit(0);
            }

            case 'i':
            {
                do_indent = 0;
                break;
            }


            case '?':
            {
                exit(1);
            }

        }
    }

    wcstring sb_in, sb_out;
    read_file(stdin, sb_in);

    wutil_init();

    if (!indent(sb_out, sb_in, do_indent))
    {
        trim(sb_out);
        fwprintf(stdout, L"%ls", sb_out.c_str());
    }
    else
    {
        /*
          Indenting failed - print original input
        */
        fwprintf(stdout, L"%ls", sb_in.c_str());
    }


    wutil_destroy();

    return 0;
}
