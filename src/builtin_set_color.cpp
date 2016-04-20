// Functions used for implementing the set_color builtin.
#include "config.h"

#if HAVE_NCURSES_H
#include <ncurses.h>
#elif HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif
#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory>
#include <string>
#include <vector>

#include "builtin.h"
#include "color.h"
#include "common.h"
#include "io.h"
#include "output.h"
#include "proc.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

class parser_t;

/// Error message for invalid path operations.
#define BUILTIN_SET_PATH_ERROR L"%ls: Warning: path component %ls may not be valid in %ls.\n"

/// Hint for invalid path operation with a colon.
#define BUILTIN_SET_PATH_HINT L"%ls: Did you mean 'set %ls $%ls %ls'?\n"

/// Error for mismatch between index count and elements.
#define BUILTIN_SET_ARG_COUNT \
    L"%ls: The number of variable indexes does not match the number of values\n"

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

/// set_color builtin.
int builtin_set_color(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wgetopter_t w;
    // Variables used for parsing the argument list.
    const struct woption long_options[] = {{L"background", required_argument, 0, 'b'},
                                           {L"help", no_argument, 0, 'h'},
                                           {L"bold", no_argument, 0, 'o'},
                                           {L"underline", no_argument, 0, 'u'},
                                           {L"version", no_argument, 0, 'v'},
                                           {L"print-colors", no_argument, 0, 'c'},
                                           {0, 0, 0, 0}};

    const wchar_t *short_options = L"b:hvocu";

    int argc = builtin_count_args(argv);

    // Some code passes variables to set_color that don't exist, like $fish_user_whatever. As a
    // hack, quietly return failure.
    if (argc <= 1) {
        return EXIT_FAILURE;
    }

    const wchar_t *bgcolor = NULL;
    bool bold = false, underline = false;
    int errret;

    // Parse options to obtain the requested operation and the modifiers.
    w.woptind = 0;
    while (1) {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1) {
            break;
        }

        switch (c) {
            case 0: {
                break;
            }
            case 'b': {
                bgcolor = w.woptarg;
                break;
            }
            case 'h': {
                builtin_print_help(parser, streams, argv[0], streams.out);
                return STATUS_BUILTIN_OK;
            }
            case 'o': {
                bold = true;
                break;
            }
            case 'u': {
                underline = true;
                break;
            }
            case 'c': {
                print_colors(streams);
                return STATUS_BUILTIN_OK;
            }
            case '?': {
                return STATUS_BUILTIN_ERROR;
            }
        }
    }

    // Remaining arguments are foreground color.
    std::vector<rgb_color_t> fgcolors;
    for (; w.woptind < argc; w.woptind++) {
        rgb_color_t fg = rgb_color_t(argv[w.woptind]);
        if (fg.is_none()) {
            streams.err.append_format(_(L"%ls: Unknown color '%ls'\n"), argv[0], argv[w.woptind]);
            return STATUS_BUILTIN_ERROR;
        }
        fgcolors.push_back(fg);
    }

    if (fgcolors.empty() && bgcolor == NULL && !bold && !underline) {
        streams.err.append_format(_(L"%ls: Expected an argument\n"), argv[0]);
        return STATUS_BUILTIN_ERROR;
    }

    // #1323: We may have multiple foreground colors. Choose the best one. If we had no foreground
    // color, we'll get none(); if we have at least one we expect not-none.
    const rgb_color_t fg = best_color(fgcolors, output_get_color_support());
    assert(fgcolors.empty() || !fg.is_none());

    const rgb_color_t bg = rgb_color_t(bgcolor ? bgcolor : L"");
    if (bgcolor && bg.is_none()) {
        streams.err.append_format(_(L"%ls: Unknown color '%ls'\n"), argv[0], bgcolor);
        return STATUS_BUILTIN_ERROR;
    }

    // Make sure that the term exists.
    if (cur_term == NULL && setupterm(0, STDOUT_FILENO, &errret) == ERR) {
        streams.err.append_format(_(L"%ls: Could not set up terminal\n"), argv[0]);
        return STATUS_BUILTIN_ERROR;
    }

    // Test if we have at least basic support for setting fonts, colors and related bits - otherwise
    // just give up...
    if (!exit_attribute_mode) {
        return STATUS_BUILTIN_ERROR;
    }

    // Save old output function so we can restore it.
    int (*const saved_writer_func)(char) = output_get_writer();

    // Set our output function, which writes to a std::string.
    builtin_set_color_output.clear();
    output_set_writer(set_color_builtin_outputter);

    if (bold) {
        if (enter_bold_mode) writembs(tparm(enter_bold_mode));
    }

    if (underline) {
        if (enter_underline_mode) writembs(enter_underline_mode);
    }

    if (bgcolor != NULL) {
        if (bg.is_normal()) {
            write_color(rgb_color_t::black(), false /* not is_fg */);
            writembs(tparm(exit_attribute_mode));
        }
    }

    if (!fg.is_none()) {
        if (fg.is_normal() || fg.is_reset()) {
            write_color(rgb_color_t::black(), true /* is_fg */);
            writembs(tparm(exit_attribute_mode));
        } else {
            write_color(fg, true /* is_fg */);
        }
    }

    if (bgcolor != NULL) {
        if (!bg.is_normal() && !bg.is_reset()) {
            write_color(bg, false /* not is_fg */);
        }
    }

    // Restore saved writer function.
    output_set_writer(saved_writer_func);

    // Output the collected string.
    streams.out.append(str2wcstring(builtin_set_color_output));
    builtin_set_color_output.clear();

    return STATUS_BUILTIN_OK;
}
