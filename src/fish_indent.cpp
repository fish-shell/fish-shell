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
#include <wctype.h>

#include <cstring>
#include <cwchar>
#include <memory>
#include <stack>
#include <string>
#include <tuple>
#include <vector>

#include "color.h"
#include "common.h"
#include "env.h"
#include "expand.h"
#include "fish_version.h"
#include "flog.h"
#include "highlight.h"
#include "operation_context.h"
#include "output.h"
#include "parse_constants.h"
#include "print_help.h"
#include "tnode.h"
#include "wutil.h"  // IWYU pragma: keep

// The number of spaces per indent isn't supposed to be configurable.
// See discussion at https://github.com/fish-shell/fish-shell/pull/6790
#define SPACES_PER_INDENT 4

// An indent_t represents an abstract indent depth. 2 means we are in a doubly-nested block, etc.
using indent_t = unsigned int;
static bool dump_parse_tree = false;
static int ret = 0;

// Read the entire contents of a file into the specified string.
static wcstring read_file(FILE *f) {
    wcstring result;
    while (true) {
        wint_t c = std::fgetwc(f);

        if (c == WEOF) {
            if (ferror(f)) {
                if (errno == EILSEQ) {
                    // Illegal byte sequence. Try to skip past it.
                    clearerr(f);
                    int ch = fgetc(f);  // for printing the warning, and seeks forward 1 byte.
                    FLOGF(warning, "%s (byte=%X)", std::strerror(errno), ch);
                    ret = 1;
                    continue;
                } else {
                    wperror(L"fgetwc");
                    exit(1);
                }
            }
            break;
        }
        result.push_back(static_cast<wchar_t>(c));
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

    // Whether the last token was a semicolon.
    bool last_was_semicolon = false;

    // Whether we need to append a continuation new line before continuing.
    bool needs_continuation_newline = false;

    // Additional indentation due to line continuation (escaped newline)
    uint32_t line_continuation_indent = 0;

    prettifier_t(const wcstring &source, bool do_indent) : source(source), do_indent(do_indent) {}

    void prettify_node(const parse_node_tree_t &tree, node_offset_t node_idx, indent_t node_indent,
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

    // Append whitespace as necessary. If we have a newline, append the appropriate indent.
    // Otherwise, append a space.
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
    wcstring source_txt;
    if (node.source_start != SOURCE_OFFSET_INVALID && node.source_length != SOURCE_OFFSET_INVALID) {
        int nextc_idx = node.source_start + node.source_length;
        if (static_cast<size_t>(nextc_idx) < source.size()) {
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
    std::fwprintf(stderr, L"{off %4u, len %4u, indent %2u, kw %ls, %ls} [%ls|%ls|%ls]\n",
                  node.source_start, node.source_length, node_indent,
                  keyword_description(node.keyword), token_type_description(node.type), prevc_str,
                  source_txt.c_str(), nextc_str);
}

void prettifier_t::prettify_node(const parse_node_tree_t &tree, node_offset_t node_idx,
                                 indent_t node_indent, parse_token_type_t parent_type) {
    // Use an explicit stack to avoid stack overflow.
    struct pending_node_t {
        node_offset_t index;
        indent_t indent;
        parse_token_type_t parent_type;
    };
    std::stack<pending_node_t> pending_node_stack;

    pending_node_stack.push({node_idx, node_indent, parent_type});
    while (!pending_node_stack.empty()) {
        pending_node_t args = pending_node_stack.top();
        pending_node_stack.pop();
        auto node_idx = args.index;
        auto node_indent = args.indent;
        auto parent_type = args.parent_type;

        const parse_node_t &node = tree.at(node_idx);
        const parse_token_type_t node_type = node.type;
        const parse_token_type_t prev_node_type =
            node_idx > 0 ? tree.at(node_idx - 1).type : token_type_invalid;

        // Increment the indent if we are either a root job_list, or root case_item_list, or in an
        // if or while header (#1665).
        const bool is_root_job_list =
            node_type == symbol_job_list && parent_type != symbol_job_list;
        const bool is_root_case_list =
            node_type == symbol_case_item_list && parent_type != symbol_case_item_list;
        const bool is_if_while_header =
            (node_type == symbol_job_conjunction || node_type == symbol_andor_job_list) &&
            (parent_type == symbol_if_clause || parent_type == symbol_while_header);

        if (is_root_job_list || is_root_case_list || is_if_while_header) {
            node_indent += 1;
        }

        if (dump_parse_tree) dump_node(node_indent, node, source);

        // Prepend any escaped newline, but only for certain cases.
        // We allow it to split arguments (including at the end - this is like trailing commas in
        // lists, makes for better diffs), to separate pipelines (but it has to be *before* the
        // pipe, so the pipe symbol is the first thing on the new line after the indent) and to
        // separate &&/|| job lists (`and` and `or` are handled separately below, as they *allow*
        // semicolons)
        // TODO: Handle
        //     foo | \
        //         bar
        // so it just removes the escape - pipes don't need it. This was changed in some fish
        // version, figure out which it was and if it is worth supporting.
        if (prev_node_type == symbol_arguments_or_redirections_list ||
            prev_node_type == symbol_argument_list || node_type == parse_token_type_andand ||
            node_type == parse_token_type_pipe || node_type == parse_token_type_end) {
            maybe_prepend_escaped_newline(node);
        }

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
            // For historical reasons, semicolon also get "TOK_END".
            // We need to distinguish between them, because otherwise `a;;;;` gets extra lines
            // instead of the semicolons. Semicolons are just ignored, unless they are followed by a
            // command. So `echo;` removes the semicolon, but `echo; echo` removes it and adds a
            // newline.
            last_was_semicolon = false;
            if (node.get_source(source) == L"\n") {
                append_newline();
            } else if (!has_new_line) {
                // The semicolon is only useful if we haven't just had a newline.
                last_was_semicolon = true;
            }
        } else if ((node_type >= FIRST_PARSE_TOKEN_TYPE && node_type <= LAST_PARSE_TOKEN_TYPE) ||
                   node_type == parse_special_type_parse_error) {
            if (last_was_semicolon) {
                // We keep the semicolon for `; and` and `; or`,
                // others we turn into newlines.
                if (node.keyword != parse_keyword_and && node.keyword != parse_keyword_or) {
                    append_newline();
                } else {
                    output.push_back(L';');
                }
                last_was_semicolon = false;
            }

            if (node.has_source()) {
                // Some type representing a particular token.
                if (prev_node_type != parse_token_type_redirection) {
                    append_whitespace(node_indent);
                }
                wcstring unescaped{source, node.source_start, node.source_length};
                // Unescape the string - this leaves special markers around if there are any
                // expansions or anything. We specifically tell it to not compute backslash-escapes
                // like \U or \x, because we want to leave them intact.
                unescape_string_in_place(&unescaped, UNESCAPE_SPECIAL | UNESCAPE_NO_BACKSLASHES);

                // Remove INTERNAL_SEPARATOR because that's a quote.
                auto quote = [](wchar_t ch) { return ch == INTERNAL_SEPARATOR; };
                unescaped.erase(std::remove_if(unescaped.begin(), unescaped.end(), quote),
                                unescaped.end());

                // If no non-"good" char is left, use the unescaped version.
                // This can be extended to other characters, but giving the precise list is tough,
                // can change over time (see "^", "%" and "?", in some cases "{}") and it just makes
                // people feel more at ease.
                auto goodchars = [](wchar_t ch) {
                    return fish_iswalnum(ch) || ch == L'_' || ch == L'-' || ch == L'/';
                };
                if (std::find_if_not(unescaped.begin(), unescaped.end(), goodchars) ==
                        unescaped.end() &&
                    !unescaped.empty()) {
                    output.append(unescaped);
                } else {
                    output.append(source, node.source_start, node.source_length);
                }
                has_new_line = false;
            }
        }

        // Put all children in stack in reversed order
        // This way they will be processed in correct order.
        for (node_offset_t idx = node.child_count; idx > 0; idx--) {
            // Note: We pass our type to our child, which becomes its parent node type.
            // Note: While node.child_start could be -1 (NODE_OFFSET_INVALID) the addition is safe
            // because we won't execute this call in that case since node.child_count should be
            // zero.
            pending_node_stack.push({node.child_start + (idx - 1), node_indent, node_type});
        }
    }
}

static const char *highlight_role_to_string(highlight_role_t role) {
#define TEST_ROLE(x)          \
    case highlight_role_t::x: \
        return #x;
    switch (role) {
        TEST_ROLE(normal)
        TEST_ROLE(error)
        TEST_ROLE(command)
        TEST_ROLE(statement_terminator)
        TEST_ROLE(param)
        TEST_ROLE(comment)
        TEST_ROLE(match)
        TEST_ROLE(search_match)
        TEST_ROLE(operat)
        TEST_ROLE(escape)
        TEST_ROLE(quote)
        TEST_ROLE(redirection)
        TEST_ROLE(autosuggestion)
        TEST_ROLE(selection)
        TEST_ROLE(pager_progress)
        TEST_ROLE(pager_background)
        TEST_ROLE(pager_prefix)
        TEST_ROLE(pager_completion)
        TEST_ROLE(pager_description)
        TEST_ROLE(pager_secondary_background)
        TEST_ROLE(pager_secondary_prefix)
        TEST_ROLE(pager_secondary_completion)
        TEST_ROLE(pager_secondary_description)
        TEST_ROLE(pager_selected_background)
        TEST_ROLE(pager_selected_prefix)
        TEST_ROLE(pager_selected_completion)
        TEST_ROLE(pager_selected_description)
        default:
            DIE("UNKNOWN ROLE");
    }
#undef TEST_ROLE
}

// Entry point for Pygments CSV output.
// Our output is a newline-separated string.
// Each line is of the form `start,end,role`
// start and end is the half-open token range, value is a string from highlight_role_t.
// Example:
// 3,7,command
static std::string make_pygments_csv(const wcstring &src) {
    const size_t len = src.size();
    std::vector<highlight_spec_t> colors;
    highlight_shell_no_io(src, colors, src.size(), operation_context_t::globals());
    assert(colors.size() == len && "Colors and src should have same size");

    struct token_range_t {
        unsigned long start;
        unsigned long end;
        highlight_role_t role;
    };

    std::vector<token_range_t> token_ranges;
    for (size_t i = 0; i < len; i++) {
        highlight_role_t role = colors.at(i).foreground;
        // See if we can extend the last range.
        if (!token_ranges.empty()) {
            auto &last = token_ranges.back();
            if (last.role == role && last.end == i) {
                last.end = i + 1;
                continue;
            }
        }
        // We need a new range.
        token_ranges.push_back(token_range_t{i, i + 1, role});
    }

    // Now render these to a string.
    std::string result;
    for (const auto &range : token_ranges) {
        char buff[128];
        snprintf(buff, sizeof buff, "%lu,%lu,%s\n", range.start, range.end,
                 highlight_role_to_string(range.role));
        result.append(buff);
    }
    return result;
}

// Entry point for prettification.
static wcstring prettify(const wcstring &src, bool do_indent) {
    parse_node_tree_t parse_tree;
    int parse_flags = (parse_flag_continue_after_error | parse_flag_include_comments |
                       parse_flag_leave_unterminated | parse_flag_show_blank_lines);
    if (!parse_tree_from_string(src, parse_flags, &parse_tree, nullptr)) {
        return src;  // we return the original string on failure
    }

    if (dump_parse_tree) {
        const wcstring dump = parse_dump_tree(parse_tree, src);
        std::fwprintf(stderr, L"%ls\n", dump.c_str());
    }

    // We may have a forest of disconnected trees on a parse failure. We have to handle all nodes
    // that have no parent, and all parse errors.
    prettifier_t prettifier{src, do_indent};
    for (node_offset_t i = 0; i < parse_tree.size(); i++) {
        const parse_node_t &node = parse_tree.at(i);
        if (node.parent == NODE_OFFSET_INVALID || node.type == parse_special_type_parse_error) {
            // A root node.
            prettifier.prettify_node(parse_tree, i, 0, symbol_job_list);
        }
    }
    return std::move(prettifier.output);
}

/// Given a string and list of colors of the same size, return the string with HTML span elements
/// for the various colors.
static const wchar_t *html_class_name_for_color(highlight_spec_t spec) {
#define P(x) L"fish_color_" #x
    switch (spec.foreground) {
        case highlight_role_t::normal: {
            return P(normal);
        }
        case highlight_role_t::error: {
            return P(error);
        }
        case highlight_role_t::command: {
            return P(command);
        }
        case highlight_role_t::statement_terminator: {
            return P(statement_terminator);
        }
        case highlight_role_t::param: {
            return P(param);
        }
        case highlight_role_t::comment: {
            return P(comment);
        }
        case highlight_role_t::match: {
            return P(match);
        }
        case highlight_role_t::search_match: {
            return P(search_match);
        }
        case highlight_role_t::operat: {
            return P(operator);
        }
        case highlight_role_t::escape: {
            return P(escape);
        }
        case highlight_role_t::quote: {
            return P(quote);
        }
        case highlight_role_t::redirection: {
            return P(redirection);
        }
        case highlight_role_t::autosuggestion: {
            return P(autosuggestion);
        }
        case highlight_role_t::selection: {
            return P(selection);
        }
        default: {
            return P(other);
        }
    }
}

static std::string html_colorize(const wcstring &text,
                                 const std::vector<highlight_spec_t> &colors) {
    if (text.empty()) {
        return "";
    }

    assert(colors.size() == text.size());
    wcstring html = L"<pre><code>";
    highlight_spec_t last_color = highlight_role_t::normal;
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
    program_name = L"fish_indent";
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
        output_type_pygments_csv,
        output_type_html
    } output_type = output_type_plain_text;
    const char *output_location = "";
    bool do_indent = true;

    const char *short_opts = "+d:hvwiD:";
    const struct option long_opts[] = {{"debug-level", required_argument, nullptr, 'd'},
                                       {"debug-stack-frames", required_argument, nullptr, 'D'},
                                       {"dump-parse-tree", no_argument, nullptr, 'P'},
                                       {"no-indent", no_argument, nullptr, 'i'},
                                       {"help", no_argument, nullptr, 'h'},
                                       {"version", no_argument, nullptr, 'v'},
                                       {"write", no_argument, nullptr, 'w'},
                                       {"html", no_argument, nullptr, 1},
                                       {"ansi", no_argument, nullptr, 2},
                                       {"pygments", no_argument, nullptr, 3},
                                       {nullptr, 0, nullptr, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'P': {
                dump_parse_tree = true;
                break;
            }
            case 'h': {
                print_help("fish_indent", 1);
                exit(0);
            }
            case 'v': {
                std::fwprintf(stderr, _(L"%ls, version %s\n"), program_name, get_fish_version());
                exit(0);
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
            case 3: {
                output_type = output_type_pygments_csv;
                break;
            }
            case 'd': {
                char *end;
                long tmp;

                errno = 0;
                tmp = strtol(optarg, &end, 10);

                if (tmp >= 0 && tmp <= 10 && !*end && !errno) {
                    debug_level = static_cast<int>(tmp);
                } else {
                    std::fwprintf(stderr, _(L"Invalid value '%s' for debug-level flag"), optarg);
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
                    set_debug_stack_frames(static_cast<int>(tmp));
                } else {
                    std::fwprintf(stderr, _(L"Invalid value '%s' for debug-stack-frames flag"),
                                  optarg);
                    exit(1);
                }
                break;
            }
            default: {
                // We assume getopt_long() has already emitted a diagnostic msg.
                exit(1);
            }
        }
    }

    argc -= optind;
    argv += optind;

    wcstring src;
    for (int i = 0; i < argc || (argc == 0 && i == 0); i++) {
        if (argc == 0 && i == 0) {
            if (output_type == output_type_file) {
                std::fwprintf(
                    stderr, _(L"Expected file path to read/write for -w:\n\n $ %ls -w foo.fish\n"),
                    program_name);
                exit(1);
            }
            src = read_file(stdin);
        } else {
            FILE *fh = fopen(argv[i], "r");
            if (fh) {
                src = read_file(fh);
                fclose(fh);
                output_location = argv[i];
            } else {
                std::fwprintf(stderr, _(L"Opening \"%s\" failed: %s\n"), *argv,
                              std::strerror(errno));
                exit(1);
            }
        }

        if (output_type == output_type_pygments_csv) {
            std::string output = make_pygments_csv(src);
            fputs(output.c_str(), stdout);
            continue;
        }

        const wcstring output_wtext = prettify(src, do_indent);

        // Maybe colorize.
        std::vector<highlight_spec_t> colors;
        if (output_type != output_type_plain_text) {
            highlight_shell_no_io(output_wtext, colors, output_wtext.size(),
                                  operation_context_t::globals());
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
                    std::fputws(output_wtext.c_str(), fh);
                    fclose(fh);
                } else {
                    std::fwprintf(stderr, _(L"Opening \"%s\" failed: %s\n"), output_location,
                                  std::strerror(errno));
                    exit(1);
                }
                break;
            }
            case output_type_ansi: {
                colored_output = colorize(output_wtext, colors);
                break;
            }
            case output_type_html: {
                colored_output = html_colorize(output_wtext, colors);
                break;
            }
            case output_type_pygments_csv: {
                DIE("pygments_csv should have been handled above");
            }
        }

        std::fputws(str2wcstring(colored_output).c_str(), stdout);
    }
    return 0;
}
