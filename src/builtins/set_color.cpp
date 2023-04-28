// Functions used for implementing the set_color builtin.
#include "config.h"

#include "set_color.h"

#include <unistd.h>

#include <cstdlib>

#if HAVE_CURSES_H
#include <curses.h>  // IWYU pragma: keep
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

#include <string>
#include <vector>

#include "../builtin.h"
#include "../color.h"
#include "../common.h"
#include "../env.h"
#include "../fallback.h"  // IWYU pragma: keep
#include "../io.h"
#include "../maybe.h"
#include "../output.h"
#include "../parser.h"
#include "../wgetopt.h"
#include "../wutil.h"  // IWYU pragma: keep

static void print_modifiers(outputter_t &outp, bool bold, bool underline, bool italics, bool dim,
                            bool reverse, rgb_color_t bg) {
    if (bold && enter_bold_mode) {
        // These casts are needed to work with different curses implementations.
        writembs_nofail(outp, fish_tparm(const_cast<char *>(enter_bold_mode)));
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
    if (!bg.is_none() && bg.is_normal()) {
        writembs_nofail(outp, fish_tparm(const_cast<char *>(exit_attribute_mode)));
    }
}

static void print_colors(io_streams_t &streams, std::vector<wcstring> args, bool bold,
                         bool underline, bool italics, bool dim, bool reverse, rgb_color_t bg) {
    outputter_t outp;
    if (args.empty()) args = rgb_color_t::named_color_names();
    for (const auto &color_name : args) {
        if (!streams.out_is_redirected && isatty(STDOUT_FILENO)) {
            print_modifiers(outp, bold, underline, italics, dim, reverse, bg);
            rgb_color_t color = rgb_color_t(color_name);
            outp.set_color(color, rgb_color_t::none());
            if (!bg.is_none()) {
                outp.write_color(bg, false /* not is_fg */);
            }
        }
        outp.writestr(color_name);
        if (!bg.is_none()) {
            // If we have a background, stop it after the color
            // or it goes to the end of the line and looks ugly.
            writembs_nofail(outp, fish_tparm(const_cast<char *>(exit_attribute_mode)));
        }
        outp.writech(L'\n');
    }  // conveniently, 'normal' is always the last color so we don't need to reset here

    streams.out.append(str2wcstring(outp.contents()));
}

static const wchar_t *const short_options = L":b:hoidrcu";
static const struct woption long_options[] = {{L"background", required_argument, 'b'},
                                              {L"help", no_argument, 'h'},
                                              {L"bold", no_argument, 'o'},
                                              {L"underline", no_argument, 'u'},
                                              {L"italics", no_argument, 'i'},
                                              {L"dim", no_argument, 'd'},
                                              {L"reverse", no_argument, 'r'},
                                              {L"print-colors", no_argument, 'c'},
                                              {}};

/// set_color builtin.
maybe_t<int> builtin_set_color(parser_t &parser, io_streams_t &streams, const wchar_t **argv) {
    // By the time this is called we should have initialized the curses subsystem.
    assert(curses_initialized);

    // Variables used for parsing the argument list.
    int argc = builtin_count_args(argv);

    // Some code passes variables to set_color that don't exist, like $fish_user_whatever. As a
    // hack, quietly return failure.
    if (argc <= 1) {
        return EXIT_FAILURE;
    }

    const wchar_t *bgcolor = nullptr;
    bool bold = false, underline = false, italics = false, dim = false, reverse = false,
         print = false;

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
                print = true;
                break;
            }
            case ':': {
                // We don't error here because "-b" is the only option that requires an argument,
                // and we don't error for missing colors.
                return STATUS_INVALID_ARGS;
            }
            case '?': {
                builtin_unknown_option(parser, streams, L"set_color", argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            default: {
                DIE("unexpected retval from wgetopt_long");
            }
        }
    }

    rgb_color_t bg = rgb_color_t(bgcolor ? bgcolor : L"");
    if (bgcolor && bg.is_none()) {
        streams.err.append_format(_(L"%ls: Unknown color '%ls'\n"), argv[0], bgcolor);
        return STATUS_INVALID_ARGS;
    }

    if (print) {
        // Hack: Explicitly setting a background of "normal" crashes
        // for --print-colors. Because it's not interesting in terms of display,
        // just skip it.
        if (bgcolor && bg.is_special()) {
            bg = rgb_color_t(L"");
        }
        std::vector<wcstring> args(argv + w.woptind, argv + argc);
        print_colors(streams, args, bold, underline, italics, dim, reverse, bg);
        return STATUS_CMD_OK;
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

    // Test if we have at least basic support for setting fonts, colors and related bits - otherwise
    // just give up...
    if (cur_term == nullptr || !exit_attribute_mode) {
        return STATUS_CMD_ERROR;
    }
    outputter_t outp;

    print_modifiers(outp, bold, underline, italics, dim, reverse, bg);

    if (bgcolor != nullptr && bg.is_normal()) {
        writembs_nofail(outp, fish_tparm(const_cast<char *>(exit_attribute_mode)));
    }

    if (!fg.is_none()) {
        if (fg.is_normal() || fg.is_reset()) {
            writembs_nofail(outp, fish_tparm(const_cast<char *>(exit_attribute_mode)));
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
