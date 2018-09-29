// Functions used for implementing the set_color builtin.
#include "config.h"

#include <stddef.h>
#include <stdlib.h>

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
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

class parser_t;

static void print_colors(io_streams_t &streams) {
    const wcstring_list_t result = rgb_color_t::named_color_names();
    size_t i;
    for (i = 0; i < result.size(); i++) {
        streams.out.append(result.at(i));
        streams.out.push_back(L'\n');
    }
}

static std::string builtin_set_color_output;
/// Function we set as the output writer.
static int set_color_builtin_outputter(char c) {
    ASSERT_IS_MAIN_THREAD();
    builtin_set_color_output.push_back(c);
    return 0;
}

static const wchar_t *const short_options = L":b:hvoidrcu";
static const struct woption long_options[] = {{L"background", required_argument, NULL, 'b'},
                                              {L"help", no_argument, NULL, 'h'},
                                              {L"bold", no_argument, NULL, 'o'},
                                              {L"underline", no_argument, NULL, 'u'},
                                              {L"italics", no_argument, NULL, 'i'},
                                              {L"dim", no_argument, NULL, 'd'},
                                              {L"reverse", no_argument, NULL, 'r'},
                                              {L"version", no_argument, NULL, 'v'},
                                              {L"print-colors", no_argument, NULL, 'c'},
                                              {NULL, 0, NULL, 0}};

#if __APPLE__
static char sitm_esc[] = "\x1B[3m";
static char ritm_esc[] = "\x1B[23m";
static char dim_esc[] = "\x1B[2m";
#endif

/// set_color builtin.
int builtin_set_color(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    // By the time this is called we should have initialized the curses subsystem.
    assert(curses_initialized);

    // Hack in missing italics and dim capabilities omitted from MacOS xterm-256color terminfo
    // Helps Terminal.app/iTerm
    #if __APPLE__
    const auto term_prog = env_get(L"TERM_PROGRAM");
    if (!term_prog.missing_or_empty() && (term_prog->as_string() == L"Apple_Terminal"
        || term_prog->as_string() == L"iTerm.app")) {
        const auto term = env_get(L"TERM");
        if (!term.missing_or_empty() && (term->as_string() == L"xterm-256color")) {
            enter_italics_mode = sitm_esc;
            exit_italics_mode = ritm_esc;
            enter_dim_mode = dim_esc;
        }
    }
    #endif

    // Variables used for parsing the argument list.
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);

    // Some code passes variables to set_color that don't exist, like $fish_user_whatever. As a
    // hack, quietly return failure.
    if (argc <= 1) {
        return EXIT_FAILURE;
    }

    const wchar_t *bgcolor = NULL;
    bool bold = false, underline = false, italics = false, dim = false, reverse = false;

    // Parse options to obtain the requested operation and the modifiers.
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 'b': {
                bgcolor = w.woptarg;
                break;
            }
            case 'h': {
                builtin_print_help(parser, streams, argv[0], streams.out);
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
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            case '?': {
                return STATUS_INVALID_ARGS;
            }
            default: {
                DIE("unexpected retval from wgetopt_long");
                break;
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

    if (fgcolors.empty() && bgcolor == NULL && !bold && !underline && !italics && !dim &&
        !reverse) {
        streams.err.append_format(_(L"%ls: Expected an argument\n"), argv[0]);
        return STATUS_INVALID_ARGS;
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
    if (cur_term == NULL || !exit_attribute_mode) {
        return STATUS_CMD_ERROR;
    }

    // Save old output function so we can restore it.
    int (*const saved_writer_func)(char) = output_get_writer();

    // Set our output function, which writes to a std::string.
    builtin_set_color_output.clear();
    output_set_writer(set_color_builtin_outputter);

    if (bold && enter_bold_mode) {
        writembs_nofail(tparm(enter_bold_mode));
    }

    if (underline && enter_underline_mode) {
        writembs_nofail(enter_underline_mode);
    }

    if (italics && enter_italics_mode) {
        writembs_nofail(enter_italics_mode);
    }

    if (dim && enter_dim_mode) {
        writembs_nofail(enter_dim_mode);
    }

    if (reverse && enter_reverse_mode) {
        writembs_nofail(enter_reverse_mode);
    } else if (reverse && enter_standout_mode) {
        writembs_nofail(enter_standout_mode);
    }

    if (bgcolor != NULL && bg.is_normal()) {
        write_color(rgb_color_t::black(), false /* not is_fg */);
        writembs_nofail(tparm(exit_attribute_mode));
    }

    if (!fg.is_none()) {
        if (fg.is_normal() || fg.is_reset()) {
            write_color(rgb_color_t::black(), true /* is_fg */);
            writembs_nofail(tparm(exit_attribute_mode));
        } else {
            if (!write_color(fg, true /* is_fg */)) {
                // We need to do *something* or the lack of any output messes up
                // when the cartesian product here would make "foo" disappear:
                //  $ echo (set_color foo)bar
                set_color(rgb_color_t::reset(), rgb_color_t::none());
            }
        }
    }

    if (bgcolor != NULL && !bg.is_normal() && !bg.is_reset()) {
        write_color(bg, false /* not is_fg */);
    }

    // Restore saved writer function.
    output_set_writer(saved_writer_func);

    // Output the collected string.
    streams.out.append(str2wcstring(builtin_set_color_output));
    builtin_set_color_output.clear();

    return STATUS_CMD_OK;
}
