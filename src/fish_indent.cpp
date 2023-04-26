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

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ast.h"
#include "env.h"
#include "fds.h"
#include "ffi_baggage.h"
#include "ffi_init.rs.h"
#include "fish_indent_common.h"
#include "fish_version.h"
#include "flog.h"
#include "future_feature_flags.h"
#include "highlight.h"
#include "operation_context.h"
#include "print_help.h"
#include "tokenizer.h"
#include "wcstringutil.h"
#include "wutil.h"  // IWYU pragma: keep

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

static const char *highlight_role_to_string(highlight_role_t role) {
#define TEST_ROLE(x)          \
    case highlight_role_t::x: \
        return #x;
    switch (role) {
        TEST_ROLE(normal)
        TEST_ROLE(error)
        TEST_ROLE(command)
        TEST_ROLE(keyword)
        TEST_ROLE(statement_terminator)
        TEST_ROLE(param)
        TEST_ROLE(option)
        TEST_ROLE(comment)
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
    highlight_shell(src, colors, operation_context_t::globals());
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
    if (dump_parse_tree) {
        auto ast = ast_parse(src, parse_flag_leave_unterminated | parse_flag_include_comments |
                                      parse_flag_show_extra_semis);
        wcstring ast_dump = *ast->dump(src);
        std::fwprintf(stderr, L"%ls\n", ast_dump.c_str());
    }

    pretty_printer_t printer{src, do_indent};
    wcstring output = printer.prettify();
    return output;
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
        case highlight_role_t::option: {
            return P(option);
        }
        case highlight_role_t::comment: {
            return P(comment);
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
    return wcs2zstring(html);
}

static std::string no_colorize(const wcstring &text) { return wcs2zstring(text); }

int main(int argc, char *argv[]) {
    program_name = L"fish_indent";
    rust_init();
    // Using the user's default locale could be a problem if it doesn't use UTF-8 encoding. That's
    // because the fish project assumes Unicode UTF-8 encoding in all of its scripts.
    //
    // TODO: Auto-detect the encoding of the script. We should look for a vim style comment
    // (e.g., "# vim: set fileencoding=<encoding-name>:") or an emacs style comment
    // (e.g., "# -*- coding: <encoding-name> -*-").
    setlocale(LC_ALL, "");
    env_init();

    if (auto features_var = env_stack_t::globals().get(L"fish_features")) {
        for (const wcstring &s : features_var->as_list()) {
            mutable_fish_features()->set_from_string(s.c_str());
        }
    }

    // Types of output we support.
    enum {
        output_type_plain_text,
        output_type_file,
        output_type_ansi,
        output_type_pygments_csv,
        output_type_check,
        output_type_html
    } output_type = output_type_plain_text;
    const char *output_location = "";
    bool do_indent = true;
    // File path for debug output.
    std::string debug_output;

    const char *short_opts = "+d:hvwicD:";
    const struct option long_opts[] = {{"debug", required_argument, nullptr, 'd'},
                                       {"debug-output", required_argument, nullptr, 'o'},
                                       {"debug-stack-frames", required_argument, nullptr, 'D'},
                                       {"dump-parse-tree", no_argument, nullptr, 'P'},
                                       {"no-indent", no_argument, nullptr, 'i'},
                                       {"help", no_argument, nullptr, 'h'},
                                       {"version", no_argument, nullptr, 'v'},
                                       {"write", no_argument, nullptr, 'w'},
                                       {"html", no_argument, nullptr, 1},
                                       {"ansi", no_argument, nullptr, 2},
                                       {"pygments", no_argument, nullptr, 3},
                                       {"check", no_argument, nullptr, 'c'},
                                       {}};

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
                std::fwprintf(stdout, _(L"%ls, version %s\n"), program_name, get_fish_version());
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
            case 'c': {
                output_type = output_type_check;
                break;
            }
            case 'd': {
                activate_flog_categories_by_pattern(str2wcstring(optarg));
                for (auto cat : get_flog_categories()) {
                    if (cat->enabled) {
                        std::fwprintf(stdout, L"Debug enabled for category: %ls\n", cat->name);
                    }
                }
                break;
            }
            case 'D': {
                // TODO: Option is currently useless.
                // Either remove it or make it work with FLOG.
                break;
            }
            case 'o': {
                debug_output = optarg;
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

    // Direct any debug output right away.
    FILE *debug_output_file = nullptr;
    if (!debug_output.empty()) {
        debug_output_file = fopen(debug_output.c_str(), "w");
        if (!debug_output_file) {
            fprintf(stderr, "Could not open file %s\n", debug_output.c_str());
            perror("fopen");
            exit(-1);
        }
        set_cloexec(fileno(debug_output_file));
        setlinebuf(debug_output_file);
        set_flog_output_file(debug_output_file);
    }

    int retval = 0;

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
                std::fwprintf(stderr, _(L"Opening \"%s\" failed: %s\n"), argv[i],
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
            highlight_shell(output_wtext, colors, operation_context_t::globals());
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
                colored_output = colorize(output_wtext, colors, env_stack_t::globals());
                break;
            }
            case output_type_html: {
                colored_output = html_colorize(output_wtext, colors);
                break;
            }
            case output_type_pygments_csv: {
                DIE("pygments_csv should have been handled above");
            }
            case output_type_check: {
                if (output_wtext != src) {
                    if (argc) {
                        std::fwprintf(stderr, _(L"%s\n"), argv[i]);
                    }
                    retval++;
                }
                break;
            }
        }

        std::fputws(str2wcstring(colored_output).c_str(), stdout);
    }
    return retval;
}
