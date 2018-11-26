// The fish_indent program.
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
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include <memory>
#include <string>
#include <vector>

#include "color.h"
#include "common.h"
#include "env.h"
#include "fish_version.h"
#include "highlight.h"
#include "output.h"
#include "parse_constants.h"
#include "print_help.h"
#include "tnode.h"
#include "wutil.h"  // IWYU pragma: keep

#define SPACES_PER_INDENT 4

// An indent_t represents an abstract indent depth. 2 means we are in a doubly-nested block, etc.
typedef unsigned int indent_t;
static bool dump_parse_tree = false;

// Read the entire contents of a file into the specified string.
static wcstring read_file(FILE *f) {
    wcstring result;
    while (1) {
        wint_t c = fgetwc(f);
        if (c == WEOF) {
            if (ferror(f)) {
                wperror(L"fgetwc");
                exit(1);
            }
            break;
        }
        result.push_back((wchar_t)c);
    }
    return result;
}

struct prettifier_t {
    // Original source.
    const wcstring &source;

    // The prettifier output.
    wcstring output;

    // Whether to indent, or just insert spaces.
    const bool do_indent;

    // Whether we are at the beginning of a new line.
    bool has_new_line = true;

    // Whether we need to append a continuation new line before continuing.
    bool needs_continuation_newline = false;

    // Additional indentation due to line continuation (escaped newline)
    uint32_t line_continuation_indent = 0;

    prettifier_t(const wcstring &source, bool do_indent) : source(source), do_indent(do_indent) {}

    void prettify_node_recursive(const parse_node_tree_t &tree,
                                 node_offset_t node_idx, indent_t node_indent,
                                 parse_token_type_t parent_type);

    void maybe_prepend_escaped_newline(const parse_node_t &node) {
        if (node.has_preceding_escaped_newline()) {
            output.append(L" \\");
            append_newline(true);
        }
    }

    void append_newline(bool is_continuation = false) {
        output.push_back('\n');
        has_new_line = true;
        needs_continuation_newline = false;
        line_continuation_indent = is_continuation ? 1 : 0;
    }

    // Append whitespace as necessary. If we have a newline, append the appropriate indent. Otherwise,
    // append a space.
    void append_whitespace(indent_t node_indent) {
        if (needs_continuation_newline) {
            append_newline(true);
        }
        if (!has_new_line) {
            output.push_back(L' ');
        } else if (do_indent) {
            output.append((node_indent + line_continuation_indent) * SPACES_PER_INDENT, L' ');
        }
    }

};

// Dump a parse tree node in a form helpful to someone debugging the behavior of this program.
static void dump_node(indent_t node_indent, const parse_node_t &node, const wcstring &source) {
    wchar_t nextc = L' ';
    wchar_t prevc = L' ';
    wcstring source_txt = L"";
    if (node.source_start != SOURCE_OFFSET_INVALID && node.source_length != SOURCE_OFFSET_INVALID) {
        int nextc_idx = node.source_start + node.source_length;
        if ((size_t)nextc_idx < source.size()) {
            nextc = source[node.source_start + node.source_length];
        }
        if (node.source_start > 0) prevc = source[node.source_start - 1];
        source_txt = source.substr(node.source_start, node.source_length);
    }
    wchar_t prevc_str[4] = {prevc, 0, 0, 0};
    wchar_t nextc_str[4] = {nextc, 0, 0, 0};
    if (prevc < L' ') {
        prevc_str[0] = L'\\';
        prevc_str[1] = L'c';
        prevc_str[2] = prevc + '@';
    }
    if (nextc < L' ') {
        nextc_str[0] = L'\\';
        nextc_str[1] = L'c';
        nextc_str[2] = nextc + '@';
    }
    fwprintf(stderr, L"{off %4u, len %4u, indent %2u, kw %ls, %ls} [%ls|%ls|%ls]\n",
             node.source_start, node.source_length, node_indent, keyword_description(node.keyword),
             token_type_description(node.type), prevc_str, source_txt.c_str(), nextc_str);
}

void prettifier_t::prettify_node_recursive(const parse_node_tree_t &tree,
                                    node_offset_t node_idx, indent_t node_indent,
                                    parse_token_type_t parent_type) {
    const parse_node_t &node = tree.at(node_idx);
    const parse_token_type_t node_type = node.type;
    const parse_token_type_t prev_node_type =
        node_idx > 0 ? tree.at(node_idx - 1).type : token_type_invalid;

    // Increment the indent if we are either a root job_list, or root case_item_list, or in an if or
    // while header (#1665).
    const bool is_root_job_list = node_type == symbol_job_list && parent_type != symbol_job_list;
    const bool is_root_case_list =
        node_type == symbol_case_item_list && parent_type != symbol_case_item_list;
    const bool is_if_while_header =
        (node_type == symbol_job_conjunction || node_type == symbol_andor_job_list) &&
        (parent_type == symbol_if_clause || parent_type == symbol_while_header);

    if (is_root_job_list || is_root_case_list || is_if_while_header) {
        node_indent += 1;
    }

    if (dump_parse_tree) dump_node(node_indent, node, source);

    // Prepend any escaped newline.
    maybe_prepend_escaped_newline(node);

    // handle comments, which come before the text
    if (node.has_comments()) {
        auto comment_nodes = tree.comment_nodes_for_node(node);
        for (const auto &comment : comment_nodes) {
            maybe_prepend_escaped_newline(*comment.node());
            append_whitespace(node_indent);
            auto source_range = comment.source_range();
            output.append(source, source_range->start, source_range->length);
            needs_continuation_newline = true;
        }
    }

    if (node_type == parse_token_type_end) {
        append_newline();
    } else if ((node_type >= FIRST_PARSE_TOKEN_TYPE && node_type <= LAST_PARSE_TOKEN_TYPE) ||
               node_type == parse_special_type_parse_error) {
        if (node.keyword != parse_keyword_none) {
            append_whitespace(node_indent);
            output.append(keyword_description(node.keyword));
            has_new_line = false;
        } else if (node.has_source()) {
            // Some type representing a particular token.
            if (prev_node_type != parse_token_type_redirection) {
                append_whitespace(node_indent);
            }
            output.append(source, node.source_start, node.source_length);
            has_new_line = false;
        }
    }

    // Recurse to all our children.
    for (node_offset_t idx = 0; idx < node.child_count; idx++) {
        // Note: We pass our type to our child, which becomes its parent node type.
        // Note: While node.child_start could be -1 (NODE_OFFSET_INVALID) the addition is safe
        // because we won't execute this call in that case since node.child_count should be zero.
        prettify_node_recursive(tree, node.child_start + idx, node_indent, node_type);
    }
}

// Entry point for prettification.
static wcstring prettify(const wcstring &src, bool do_indent) {
    parse_node_tree_t parse_tree;
    int parse_flags = (parse_flag_continue_after_error | parse_flag_include_comments |
                       parse_flag_leave_unterminated | parse_flag_show_blank_lines);
    if (!parse_tree_from_string(src, parse_flags, &parse_tree, NULL)) {
        return src;  // we return the original string on failure
    }

    if (dump_parse_tree) {
        const wcstring dump = parse_dump_tree(parse_tree, src);
        fwprintf(stderr, L"%ls\n", dump.c_str());
    }

    // We may have a forest of disconnected trees on a parse failure. We have to handle all nodes
    // that have no parent, and all parse errors.
    prettifier_t prettifier{src, do_indent};
    for (node_offset_t i = 0; i < parse_tree.size(); i++) {
        const parse_node_t &node = parse_tree.at(i);
        if (node.parent == NODE_OFFSET_INVALID || node.type == parse_special_type_parse_error) {
            // A root node.
            prettifier.prettify_node_recursive(parse_tree, i, 0, symbol_job_list);
        }
    }
    return std::move(prettifier.output);
}

// Helper for output_set_writer
static std::string output_receiver;
static int write_to_output_receiver(char c) {
    output_receiver.push_back(c);
    return 0;
}

/// Given a string and list of colors of the same size, return the string with ANSI escape sequences
/// representing the colors.
static std::string ansi_colorize(const wcstring &text,
                                 const std::vector<highlight_spec_t> &colors) {
    assert(colors.size() == text.size());
    assert(output_receiver.empty());

    int (*saved)(char) = output_get_writer();
    output_set_writer(write_to_output_receiver);

    highlight_spec_t last_color = highlight_spec_normal;
    for (size_t i = 0; i < text.size(); i++) {
        highlight_spec_t color = colors.at(i);
        if (color != last_color) {
            set_color(highlight_get_color(color, false), rgb_color_t::normal());
            last_color = color;
        }
        writech(text.at(i));
    }
    set_color(rgb_color_t::normal(), rgb_color_t::normal());
    output_set_writer(saved);
    std::string result;
    result.swap(output_receiver);
    return result;
}

/// Given a string and list of colors of the same size, return the string with HTML span elements
/// for the various colors.
static const wchar_t *html_class_name_for_color(highlight_spec_t spec) {
#define P(x) L"fish_color_" #x
    switch (spec & HIGHLIGHT_SPEC_PRIMARY_MASK) {
        case highlight_spec_normal: {
            return P(normal);
        }
        case highlight_spec_error: {
            return P(error);
        }
        case highlight_spec_command: {
            return P(command);
        }
        case highlight_spec_statement_terminator: {
            return P(statement_terminator);
        }
        case highlight_spec_param: {
            return P(param);
        }
        case highlight_spec_comment: {
            return P(comment);
        }
        case highlight_spec_match: {
            return P(match);
        }
        case highlight_spec_search_match: {
            return P(search_match);
        }
        case highlight_spec_operator: {
            return P(operator);
        }
        case highlight_spec_escape: {
            return P(escape);
        }
        case highlight_spec_quote: {
            return P(quote);
        }
        case highlight_spec_redirection: {
            return P(redirection);
        }
        case highlight_spec_autosuggestion: {
            return P(autosuggestion);
        }
        case highlight_spec_selection: {
            return P(selection);
        }
        default: { return P(other); }
    }
}

static std::string html_colorize(const wcstring &text,
                                 const std::vector<highlight_spec_t> &colors) {
    if (text.empty()) {
        return "";
    }

    assert(colors.size() == text.size());
    wcstring html = L"<pre><code>";
    highlight_spec_t last_color = highlight_spec_normal;
    for (size_t i = 0; i < text.size(); i++) {
        // Handle colors.
        highlight_spec_t color = colors.at(i);
        if (i > 0 && color != last_color) {
            html.append(L"</span>");
        }
        if (i == 0 || color != last_color) {
            append_format(html, L"<span class=\"%ls\">", html_class_name_for_color(color));
        }
        last_color = color;

        // Handle text.
        wchar_t wc = text.at(i);
        switch (wc) {
            case L'&': {
                html.append(L"&amp;");
                break;
            }
            case L'\'': {
                html.append(L"&apos;");
                break;
            }
            case L'"': {
                html.append(L"&quot;");
                break;
            }
            case L'<': {
                html.append(L"&lt;");
                break;
            }
            case L'>': {
                html.append(L"&gt;");
                break;
            }
            default: {
                html.push_back(wc);
                break;
            }
        }
    }
    html.append(L"</span></code></pre>");
    return wcs2string(html);
}

static std::string no_colorize(const wcstring &text) { return wcs2string(text); }

int main(int argc, char *argv[]) {
    program_name = argv[0];
    set_main_thread();
    setup_fork_guards();
    // Using the user's default locale could be a problem if it doesn't use UTF-8 encoding. That's
    // because the fish project assumes Unicode UTF-8 encoding in all of its scripts.
    //
    // TODO: Auto-detect the encoding of the script. We should look for a vim style comment
    // (e.g., "# vim: set fileencoding=<encoding-name>:") or an emacs style comment
    // (e.g., "# -*- coding: <encoding-name> -*-").
    setlocale(LC_ALL, "");
    env_init();

    // Types of output we support.
    enum {
        output_type_plain_text,
        output_type_file,
        output_type_ansi,
        output_type_html
    } output_type = output_type_plain_text;
    const char *output_location = "";
    bool do_indent = true;

    const char *short_opts = "+d:hvwiD:";
    const struct option long_opts[] = {{"debug-level", required_argument, NULL, 'd'},
                                       {"debug-stack-frames", required_argument, NULL, 'D'},
                                       {"dump-parse-tree", no_argument, NULL, 'P'},
                                       {"no-indent", no_argument, NULL, 'i'},
                                       {"help", no_argument, NULL, 'h'},
                                       {"version", no_argument, NULL, 'v'},
                                       {"write", no_argument, NULL, 'w'},
                                       {"html", no_argument, NULL, 1},
                                       {"ansi", no_argument, NULL, 2},
                                       {NULL, 0, NULL, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch (opt) {
            case 'P': {
                dump_parse_tree = true;
                break;
            }
            case 'h': {
                print_help("fish_indent", 1);
                exit(0);
                break;
            }
            case 'v': {
                fwprintf(stderr, _(L"%ls, version %s\n"), program_name, get_fish_version());
                exit(0);
                break;
            }
            case 'w': {
                output_type = output_type_file;
                break;
            }
            case 'i': {
                do_indent = false;
                break;
            }
            case 1: {
                output_type = output_type_html;
                break;
            }
            case 2: {
                output_type = output_type_ansi;
                break;
            }
            case 'd': {
                char *end;
                long tmp;

                errno = 0;
                tmp = strtol(optarg, &end, 10);

                if (tmp >= 0 && tmp <= 10 && !*end && !errno) {
                    debug_level = (int)tmp;
                } else {
                    fwprintf(stderr, _(L"Invalid value '%s' for debug-level flag"), optarg);
                    exit(1);
                }
                break;
            }
            case 'D': {
                char *end;
                long tmp;

                errno = 0;
                tmp = strtol(optarg, &end, 10);

                if (tmp > 0 && tmp <= 128 && !*end && !errno) {
                    debug_stack_frames = (int)tmp;
                } else {
                    fwprintf(stderr, _(L"Invalid value '%s' for debug-stack-frames flag"), optarg);
                    exit(1);
                }
                break;
            }
            default: {
                // We assume getopt_long() has already emitted a diagnostic msg.
                exit(1);
                break;
            }
        }
    }

    argc -= optind;
    argv += optind;

    wcstring src;
    if (argc == 0) {
        if (output_type == output_type_file) {
            fwprintf(stderr, _(L"Expected file path to read/write for -w:\n\n $ %ls -w foo.fish\n"),
                     program_name);
            exit(1);
        }
        src = read_file(stdin);
    } else if (argc == 1) {
        FILE *fh = fopen(*argv, "r");
        if (fh) {
            src = read_file(fh);
            fclose(fh);
            output_location = *argv;
        } else {
            fwprintf(stderr, _(L"Opening \"%s\" failed: %s\n"), *argv, strerror(errno));
            exit(1);
        }
    } else {
        fwprintf(stderr, _(L"Too many arguments\n"));
        exit(1);
    }

    const wcstring output_wtext = prettify(src, do_indent);

    // Maybe colorize.
    std::vector<highlight_spec_t> colors;
    if (output_type != output_type_plain_text) {
        highlight_shell_no_io(output_wtext, colors, output_wtext.size(), NULL,
                              env_vars_snapshot_t::current());
    }

    std::string colored_output;
    switch (output_type) {
        case output_type_plain_text: {
            colored_output = no_colorize(output_wtext);
            break;
        }
        case output_type_file: {
            FILE *fh = fopen(output_location, "w");
            if (fh) {
                fputws(output_wtext.c_str(), fh);
                fclose(fh);
                exit(0);
            } else {
                fwprintf(stderr, _(L"Opening \"%s\" failed: %s\n"), output_location,
                         strerror(errno));
                exit(1);
            }
            break;
        }
        case output_type_ansi: {
            colored_output = ansi_colorize(output_wtext, colors);
            break;
        }
        case output_type_html: {
            colored_output = html_colorize(output_wtext, colors);
            break;
        }
    }

    fputws(str2wcstring(colored_output).c_str(), stdout);
    return 0;
}
