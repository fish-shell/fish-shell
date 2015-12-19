/*
Copyright (C) 2014 ridiculous_fish

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

/** \file fish_indent.cpp
  The fish_indent proegram.
*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <vector>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <assert.h>
#include <locale.h>
#include <stddef.h>
#include <string>

#include "color.h"
#include "highlight.h"
#include "parse_constants.h"
#include "wutil.h"
#include "common.h"
#include "output.h"
#include "env.h"
#include "input.h"
#include "parse_tree.h"
#include "print_help.h"
#include "fish_version.h"

#define SPACES_PER_INDENT 4

/* An indent_t represents an abstract indent depth. 2 means we are in a doubly-nested block, etc. */
typedef unsigned int indent_t;

/* Read the entire contents of a file into the specified string */
static wcstring read_file(FILE *f)
{
    wcstring result;
    while (1)
    {
        wint_t c = fgetwc(f);
        if (c == WEOF)
        {
            if (ferror(f))
            {
                wperror(L"fgetwc");
                exit(1);
            }
            break;
        }
        result.push_back((wchar_t)c);
    }
    return result;
}

/* Append whitespace as necessary. If we have a newline, append the appropriate indent. Otherwise, append a space. */
static void append_whitespace(indent_t node_indent, bool do_indent, bool has_new_line, wcstring *out_result)
{
    if (! has_new_line)
    {
        out_result->push_back(L' ');
    }
    else if (do_indent)
    {
        out_result->append(node_indent * SPACES_PER_INDENT, L' ');
    }
}

static void prettify_node_recursive(const wcstring &source, const parse_node_tree_t &tree, node_offset_t node_idx, indent_t node_indent, parse_token_type_t parent_type, bool *has_new_line, wcstring *out_result, bool do_indent)
{
    const parse_node_t &node = tree.at(node_idx);
    const parse_token_type_t node_type = node.type;

    /* Increment the indent if we are either a root job_list, or root case_item_list, or in an if or while header (#1665) */
    const bool is_root_job_list = (node_type == symbol_job_list && parent_type != symbol_job_list);
    const bool is_root_case_item_list = (node_type == symbol_case_item_list && parent_type != symbol_case_item_list);
    const bool is_if_while_header = ((node_type == symbol_job || node_type == symbol_andor_job_list) &&
                                    (parent_type == symbol_if_clause || parent_type == symbol_while_header));
    if (is_root_job_list || is_root_case_item_list || is_if_while_header)
    {
        node_indent += 1;
    }

    /* Handle comments, which come before the text */
    if (node.has_comments())
    {
        const parse_node_tree_t::parse_node_list_t comment_nodes = tree.comment_nodes_for_node(node);
        for (size_t i=0; i < comment_nodes.size(); i++)
        {
            const parse_node_t &comment_node = *comment_nodes.at(i);
            append_whitespace(node_indent, do_indent, *has_new_line, out_result);
            out_result->append(source, comment_node.source_start, comment_node.source_length);
        }
    }

    if (node_type == parse_token_type_end)
    {
        /* Newline */
        out_result->push_back(L'\n');
        *has_new_line = true;
    }
    else if ((node_type >= FIRST_PARSE_TOKEN_TYPE && node_type <= LAST_PARSE_TOKEN_TYPE) || node_type == parse_special_type_parse_error)
    {
        if (node.has_source())
        {
            /* Some type representing a particular token */
            append_whitespace(node_indent, do_indent, *has_new_line, out_result);
            out_result->append(source, node.source_start, node.source_length);
            *has_new_line = false;
        }
    }

    /* Recurse to all our children */
    for (node_offset_t idx = 0; idx < node.child_count; idx++)
    {
        /* Note we pass our type to our child, which becomes its parent node type */
        prettify_node_recursive(source, tree, node.child_start + idx, node_indent, node_type, has_new_line, out_result, do_indent);
    }
}

/* Entry point for prettification. */
static wcstring prettify(const wcstring &src, bool do_indent)
{
    parse_node_tree_t tree;
    if (! parse_tree_from_string(src, parse_flag_continue_after_error | parse_flag_include_comments | parse_flag_leave_unterminated | parse_flag_show_blank_lines, &tree, NULL /* errors */))
    {
        /* We return the initial string on failure */
        return src;
    }

    /* We may have a forest of disconnected trees on a parse failure. We have to handle all nodes that have no parent, and all parse errors. */
    bool has_new_line = true;
    wcstring result;
    for (node_offset_t i=0; i < tree.size(); i++)
    {
        const parse_node_t &node = tree.at(i);
        if (node.parent == NODE_OFFSET_INVALID || node.type == parse_special_type_parse_error)
        {
            /* A root node */
            prettify_node_recursive(src, tree, i, 0, symbol_job_list, &has_new_line, &result, do_indent);
        }
    }
    return result;
}


// Helper for output_set_writer
static std::string output_receiver;
static int write_to_output_receiver(char c)
{
    output_receiver.push_back(c);
    return 0;
}

/* Given a string and list of colors of the same size, return the string with ANSI escape sequences representing the colors. */
static std::string ansi_colorize(const wcstring &text, const std::vector<highlight_spec_t> &colors)
{
    assert(colors.size() == text.size());
    assert(output_receiver.empty());

    int (*saved)(char) = output_get_writer();
    output_set_writer(write_to_output_receiver);

    highlight_spec_t last_color = highlight_spec_normal;
    for (size_t i=0; i < text.size(); i++)
    {
        highlight_spec_t color = colors.at(i);
        if (color != last_color)
        {
            set_color(highlight_get_color(color, false), rgb_color_t::normal());
            last_color = color;
        }
        writech(text.at(i));
    }

    output_set_writer(saved);
    std::string result;
    result.swap(output_receiver);
    return result;
}

/* Given a string and list of colors of the same size, return the string with HTML span elements for the various colors. */
static const wchar_t *html_class_name_for_color(highlight_spec_t spec)
{
    #define P(x) L"fish_color_"  #x
    switch (spec & HIGHLIGHT_SPEC_PRIMARY_MASK)
    {
        case highlight_spec_normal: return P(normal);
        case highlight_spec_error: return P(error);
        case highlight_spec_command: return P(command);
        case highlight_spec_statement_terminator: return P(statement_terminator);
        case highlight_spec_param: return P(param);
        case highlight_spec_comment: return P(comment);
        case highlight_spec_match: return P(match);
        case highlight_spec_search_match: return P(search_match);
        case highlight_spec_operator: return P(operator);
        case highlight_spec_escape: return P(escape);
        case highlight_spec_quote: return P(quote);
        case highlight_spec_redirection: return P(redirection);
        case highlight_spec_autosuggestion: return P(autosuggestion);
        case highlight_spec_selection: return P(selection);

        default: return P(other);
    }
}

static std::string html_colorize(const wcstring &text, const std::vector<highlight_spec_t> &colors)
{
    if (text.empty())
    {
        return "";
    }

    assert(colors.size() == text.size());
    wcstring html = L"<pre><code>";
    highlight_spec_t last_color = highlight_spec_normal;
    for (size_t i=0; i < text.size(); i++)
    {
        /* Handle colors */
        highlight_spec_t color = colors.at(i);
        if (i > 0 && color != last_color)
        {
            html.append(L"</span>");
        }
        if (i == 0 || color != last_color)
        {
            append_format(html, L"<span class=\"%ls\">", html_class_name_for_color(color));
        }
        last_color = color;

        /* Handle text */
        wchar_t wc = text.at(i);
        switch (wc)
        {
            case L'&':
                html.append(L"&amp;");
                break;
            case L'\'':
                html.append(L"&apos;");
                break;
            case L'"':
                html.append(L"&quot;");
                break;
            case L'<':
                html.append(L"&lt;");
                break;
            case L'>':
                html.append(L"&gt;");
                break;
            default:
                html.push_back(wc);
                break;
        }
    }
    html.append(L"</span></code></pre>");
    return wcs2string(html);
}

static std::string no_colorize(const wcstring &text)
{
    return wcs2string(text);
}

int main(int argc, char *argv[])
{
    set_main_thread();
    setup_fork_guards();

    wsetlocale(LC_ALL, L"");
    program_name=L"fish_indent";

    env_init();
    input_init();

    /* Types of output we support */
    enum
    {
        output_type_plain_text,
        output_type_ansi,
        output_type_html
    } output_type = output_type_plain_text;

    /* Whether to indent (true) or just reformat to one job per line (false) */
    bool do_indent = true;

    while (1)
    {
        const struct option long_options[] =
        {
            { "no-indent", no_argument, 0, 'i' },
            { "help", no_argument, 0, 'h' },
            { "version", no_argument, 0, 'v' },
            { "html", no_argument, 0, 1 },
            { "ansi", no_argument, 0, 2 },
            { 0, 0, 0, 0 }
        };

        int opt_index = 0;
        int opt = getopt_long(argc, argv, "hvi", long_options, &opt_index);
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
                assert(0 && "Unreachable code reached");
                break;
            }

            case 'v':
            {
                fwprintf(stderr, _(L"%ls, version %s\n"), program_name, get_fish_version());
                exit(0);
                assert(0 && "Unreachable code reached");
                break;
            }

            case 'i':
            {
                do_indent = false;
                break;
            }

            case 1:
            {
                output_type = output_type_html;
                break;
            }

            case 2:
            {
                output_type = output_type_ansi;
                break;
            }

            case '?':
            {
                exit(1);
            }
        }
    }

    const wcstring src = read_file(stdin);
    const wcstring output_wtext = prettify(src, do_indent);

    /* Maybe colorize */
    std::vector<highlight_spec_t> colors;
    if (output_type != output_type_plain_text)
    {
        highlight_shell_no_io(output_wtext, colors, output_wtext.size(), NULL, env_vars_snapshot_t::current());
    }

    std::string colored_output;
    switch (output_type)
    {
        case output_type_plain_text:
            colored_output = no_colorize(output_wtext);
            break;

        case output_type_ansi:
            colored_output = ansi_colorize(output_wtext, colors);
            break;

        case output_type_html:
            colored_output = html_colorize(output_wtext, colors);
            break;
    }

    fputs(colored_output.c_str(), stdout);
    return 0;
}
