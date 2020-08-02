// Functions used for implementing the set_color builtin.
#include "config.h"

#include <cstddef>
#include <cstdlib>

#if HAVE_CURSES_H
#include <curses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#elif HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#endif
#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif

#include <memory>
#include <string>
#include <vector>

#include "builtin.h"
#include "color.h"
#include "common.h"
#include "env.h"
#include "io.h"
#include "output.h"
#include "parser.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

class parser_t;

static void print_colors(io_streams_t &streams) {
    outputter_t outp;

    for (const auto &color_name : rgb_color_t::named_color_names()) {
        if (!streams.out_is_redirected && isatty(STDOUT_FILENO)) {
            rgb_color_t color = rgb_color_t(color_name);
            outp.set_color(color, rgb_color_t::none());
        }
        outp.writestr(color_name);
        outp.writech(L'\n');
    }  // conveniently, 'normal' is always the last color so we don't need to reset here

    streams.out.append(str2wcstring(outp.contents()));
}

static const wchar_t *const short_options = L":b:hvoidrcu";
static const struct woption long_options[] = {{L"background", required_argument, nullptr, 'b'},
                                              {L"help", no_argument, nullptr, 'h'},
                                              {L"bold", no_argument, nullptr, 'o'},
                                              {L"underline", no_argument, nullptr, 'u'},
                                              {L"italics", no_argument, nullptr, 'i'},
                                              {L"dim", no_argument, nullptr, 'd'},
                                              {L"reverse", no_argument, nullptr, 'r'},
                                              {L"version", no_argument, nullptr, 'v'},
                                              {L"print-colors", no_argument, nullptr, 'c'},
                                              {nullptr, 0, nullptr, 0}};

#ifdef __APPLE__
static char sitm_esc[] = "\x1B[3m";
static char ritm_esc[] = "\x1B[23m";
static char dim_esc[] = "\x1B[2m";
#endif

/// set_color builtin.
maybe_t<int> builtin_set_color(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    // By the time this is called we should have initialized the curses subsystem.
    assert(curses_initialized);

// Hack in missing italics and dim capabilities omitted from MacOS xterm-256color terminfo
// Helps Terminal.app/iTerm
#ifdef __APPLE__
    const auto term_prog = parser.vars().get(L"TERM_PROGRAM");
    if (!term_prog.missing_or_empty() &&
        (term_prog->as_string() == L"Apple_Terminal" || term_prog->as_string() == L"iTerm.app")) {
        const auto term = parser.vars().get(L"TERM");
        if (!term.missing_or_empty() && (term->as_string() == L"xterm-256color")) {
            enter_italics_mode = sitm_esc;
            exit_italics_mode = ritm_esc;
            enter_dim_mode = dim_esc;
        }
    }
#endif

    // Variables used for parsing the argument list.
    int argc = builtin_count_args(argv);

    // Some code passes variables to set_color that don't exist, like $fish_user_whatever. As a
    // hack, quietly return failure.
    if (argc <= 1) {
        return EXIT_FAILURE;
    }

    const wchar_t *bgcolor = nullptr;
    bool bold = false, underline = false, italics = false, dim = false, reverse = false;

    // Parse options to obtain the requested operation and the modifiers.
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case 'b': {
                bgcolor = w.woptarg;
                break;
            }
            case 'h': {
                builtin_print_help(parser, streams, argv[0]);
                return STATUS_CMD_OK;
            }
            case 'o': {
                bold = true;
                break;
            }
            case 'i': {
                italics = true;
                break;
            }
            case 'd': {
                dim = true;
                break;
            }
            case 'r': {
                reverse = true;
                break;
            }
            case 'u': {
                underline = true;
                break;
            }
            case 'c': {
                print_colors(streams);
                return STATUS_CMD_OK;
            }
            case ':': {
                // We don't error here because "-b" is the only option that requires an argument,
                // and we don't error for missing colors.
                return STATUS_INVALID_ARGS;
            }
            case '?': {
                return STATUS_INVALID_ARGS;
            }
            default: {
                DIE("unexpected retval from wgetopt_long");
            }
        }
    }

    // Remaining arguments are foreground color.
    std::vector<rgb_color_t> fgcolors;
    for (; w.woptind < argc; w.woptind++) {
        rgb_color_t fg = rgb_color_t(argv[w.woptind]);
        if (fg.is_none()) {
            streams.err.append_format(_(L"%ls: Unknown color '%ls'\n"), argv[0], argv[w.woptind]);
            return STATUS_INVALID_ARGS;
        }
        fgcolors.push_back(fg);
    }

    // #1323: We may have multiple foreground colors. Choose the best one. If we had no foreground
    // color, we'll get none(); if we have at least one we expect not-none.
    const rgb_color_t fg = best_color(fgcolors, output_get_color_support());
    assert(fgcolors.empty() || !fg.is_none());

    const rgb_color_t bg = rgb_color_t(bgcolor ? bgcolor : L"");
    if (bgcolor && bg.is_none()) {
        streams.err.append_format(_(L"%ls: Unknown color '%ls'\n"), argv[0], bgcolor);
        return STATUS_INVALID_ARGS;
    }

    // Test if we have at least basic support for setting fonts, colors and related bits - otherwise
    // just give up...
    if (cur_term == nullptr || !exit_attribute_mode) {
        return STATUS_CMD_ERROR;
    }
    outputter_t outp;

    if (bold && enter_bold_mode) {
        writembs_nofail(outp, tparm(const_cast<char *>(enter_bold_mode)));
    }

    if (underline && enter_underline_mode) {
        writembs_nofail(outp, enter_underline_mode);
    }

    if (italics && enter_italics_mode) {
        writembs_nofail(outp, enter_italics_mode);
    }

    if (dim && enter_dim_mode) {
        writembs_nofail(outp, enter_dim_mode);
    }

    if (reverse && enter_reverse_mode) {
        writembs_nofail(outp, enter_reverse_mode);
    } else if (reverse && enter_standout_mode) {
        writembs_nofail(outp, enter_standout_mode);
    }

    if (bgcolor != nullptr && bg.is_normal()) {
        writembs_nofail(outp, tparm(const_cast<char *>(exit_attribute_mode)));
    }

    if (!fg.is_none()) {
        if (fg.is_normal() || fg.is_reset()) {
            writembs_nofail(outp, tparm(const_cast<char *>(exit_attribute_mode)));
        } else {
            if (!outp.write_color(fg, true /* is_fg */)) {
                // We need to do *something* or the lack of any output messes up
                // when the cartesian product here would make "foo" disappear:
                //  $ echo (set_color foo)bar
                outp.set_color(rgb_color_t::reset(), rgb_color_t::none());
            }
        }
    }

    if (bgcolor != nullptr && !bg.is_normal() && !bg.is_reset()) {
        outp.write_color(bg, false /* not is_fg */);
    }

    // Output the collected string.
    streams.out.append(str2wcstring(outp.contents()));

    return STATUS_CMD_OK;
}
