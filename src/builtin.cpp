// Functions for executing builtin functions.
//
// How to add a new builtin function:
//
// 1). Create a function in builtin.c with the following signature:
//
//     <tt>static int builtin_NAME(parser_t &parser, io_streams_t &streams, wchar_t **argv)</tt>
//
// where NAME is the name of the builtin, and args is a zero-terminated list of arguments.
//
// 2). Add a line like { L"NAME", &builtin_NAME, N_(L"Bla bla bla") }, to the builtin_data_t
// variable. The description is used by the completion system. Note that this array is sorted.
//
// 3). Create a file doc_src/NAME.txt, containing the manual for the builtin in Doxygen-format.
// Check the other builtin manuals for proper syntax.
//
// 4). Use 'git add doc_src/NAME.txt' to start tracking changes to the documentation file.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include <algorithm>
#include <memory>
#include <string>

#include "builtin.h"
#include "builtin_argparse.h"
#include "builtin_bg.h"
#include "builtin_bind.h"
#include "builtin_block.h"
#include "builtin_builtin.h"
#include "builtin_cd.h"
#include "builtin_command.h"
#include "builtin_commandline.h"
#include "builtin_complete.h"
#include "builtin_contains.h"
#include "builtin_disown.h"
#include "builtin_echo.h"
#include "builtin_emit.h"
#include "builtin_exit.h"
#include "builtin_fg.h"
#include "builtin_functions.h"
#include "builtin_history.h"
#include "builtin_jobs.h"
#include "builtin_math.h"
#include "builtin_printf.h"
#include "builtin_pwd.h"
#include "builtin_random.h"
#include "builtin_read.h"
#include "builtin_realpath.h"
#include "builtin_return.h"
#include "builtin_set.h"
#include "builtin_set_color.h"
#include "builtin_source.h"
#include "builtin_status.h"
#include "builtin_string.h"
#include "builtin_test.h"
#include "builtin_ulimit.h"
#include "builtin_wait.h"
#include "common.h"
#include "complete.h"
#include "exec.h"
#include "fallback.h"  // IWYU pragma: keep
#include "intern.h"
#include "io.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "parser.h"
#include "proc.h"
#include "reader.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

bool builtin_data_t::operator<(const wcstring &other) const {
    return wcscmp(this->name, other.c_str()) < 0;
}

bool builtin_data_t::operator<(const builtin_data_t *other) const {
    return wcscmp(this->name, other->name) < 0;
}

/// Counts the number of arguments in the specified null-terminated array
int builtin_count_args(const wchar_t *const *argv) {
    int argc;
    for (argc = 1; argv[argc] != NULL;) {
        argc++;
    }

    assert(argv[argc] == NULL);
    return argc;
}

/// This function works like wperror, but it prints its result into the streams.err string instead
/// to stderr. Used by the builtin commands.
void builtin_wperror(const wchar_t *s, io_streams_t &streams) {
    char *err = strerror(errno);
    if (s != NULL) {
        streams.err.append(s);
        streams.err.append(L": ");
    }
    if (err != NULL) {
        const wcstring werr = str2wcstring(err);
        streams.err.append(werr);
        streams.err.push_back(L'\n');
    }
}

static const wchar_t *const short_options = L"+:h";
static const struct woption long_options[] = {{L"help", no_argument, NULL, 'h'},
                                              {NULL, 0, NULL, 0}};

int parse_help_only_cmd_opts(struct help_only_cmd_opts_t &opts, int *optind, int argc,
                             wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {  //!OCLINT(too few branches)
            case 'h': {
                opts.print_help = true;
                break;
            }
            case ':': {
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            case '?': {
                builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            default: {
                DIE("unexpected retval from wgetopt_long");
                break;
            }
        }
    }

    *optind = w.woptind;
    return STATUS_CMD_OK;
}

/// Count the number of times the specified character occurs in the specified string.
static int count_char(const wchar_t *str, wchar_t c) {
    int res = 0;
    for (; *str; str++) {
        res += (*str == c);
    }
    return res;
}

/// Obtain help/usage information for the specified builtin from manpage in subshell
///
/// @param  name
///    builtin name to get up help for
///
/// @return
///    A wcstring with a formatted manpage.
///
wcstring builtin_help_get(parser_t &parser, io_streams_t &streams, const wchar_t *name) {
    UNUSED(parser);
    // This won't ever work if no_exec is set.
    if (no_exec) return wcstring();

    wcstring_list_t lst;
    wcstring out;
    const wcstring name_esc = escape_string(name, 1);
    wcstring cmd = format_string(L"__fish_print_help %ls", name_esc.c_str());
    if (!streams.out_is_redirected && isatty(STDOUT_FILENO)) {
        // since we're using a subshell, __fish_print_help can't tell we're in
        // a terminal. Tell it ourselves.
        int cols = common_get_width();
        cmd = format_string(L"__fish_print_help --tty-width %d %ls", cols, name_esc.c_str());
    }
    if (exec_subshell(cmd, lst, false /* don't apply exit status */) >= 0) {
        for (size_t i = 0; i < lst.size(); i++) {
            out.append(lst.at(i));
            out.push_back(L'\n');
        }
    }
    return out;
}

/// Process and print for the specified builtin. If @c b is `sb_err`, also print the line
/// information.
///
/// If @c b is the buffer representing standard error, and the help message is about to be printed
/// to an interactive screen, it may be shortened to fit the screen.
///
void builtin_print_help(parser_t &parser, io_streams_t &streams, const wchar_t *cmd,
                        output_stream_t &b) {
    bool is_stderr = &b == &streams.err;
    if (is_stderr) {
        b.append(parser.current_line());
    }

    const wcstring h = builtin_help_get(parser, streams, cmd);

    if (!h.size()) return;

    wchar_t *str = wcsdup(h.c_str());
    if (str) {
        bool is_short = false;
        if (is_stderr) {
            // Interactive mode help to screen - only print synopsis if the rest won't fit.
            int screen_height, lines;

            screen_height = common_get_height();
            lines = count_char(str, L'\n');
            if (!shell_is_interactive() || (lines > 2 * screen_height / 3)) {
                wchar_t *pos;
                int cut = 0;
                int i;

                is_short = true;

                // First move down 4 lines.
                pos = str;
                for (i = 0; (i < 4) && pos && *pos; i++) {
                    pos = wcschr(pos + 1, L'\n');
                }

                if (pos && *pos) {
                    // Then find the next empty line.
                    for (; *pos; pos++) {
                        if (*pos != L'\n') {
                            continue;
                        }

                        int is_empty = 1;
                        wchar_t *pos2;
                        for (pos2 = pos + 1; *pos2; pos2++) {
                            if (*pos2 == L'\n') break;

                            if (*pos2 != L'\t' && *pos2 != L' ') {
                                is_empty = 0;
                                break;
                            }
                        }
                        if (is_empty) {
                            // And cut it.
                            *(pos2 + 1) = L'\0';
                            cut = 1;
                            break;
                        }
                    }
                }

                // We did not find a good place to cut message to shorten it - so we make sure we
                // don't print anything.
                if (!cut) {
                    *str = 0;
                }
            }
        }

        b.append(str);
        if (is_short) {
            b.append_format(_(L"%ls: Type 'help %ls' for related documentation\n\n"), cmd, cmd);
        }

        free(str);
    }
}

/// Perform error reporting for encounter with unknown option.
void builtin_unknown_option(parser_t &parser, io_streams_t &streams, const wchar_t *cmd,
                            const wchar_t *opt) {
    streams.err.append_format(BUILTIN_ERR_UNKNOWN, cmd, opt);
    builtin_print_help(parser, streams, cmd, streams.err);
}

/// Perform error reporting for encounter with missing argument.
void builtin_missing_argument(parser_t &parser, io_streams_t &streams, const wchar_t *cmd,
                              const wchar_t *opt) {
    streams.err.append_format(BUILTIN_ERR_MISSING, cmd, opt);
    builtin_print_help(parser, streams, cmd, streams.err);
}

/// A generic bultin that only supports showing a help message. This is only a placeholder that
/// prints the help message. Useful for commands that live in the parser.
static int builtin_generic(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    help_only_cmd_opts_t opts;
    int optind;
    int retval = parse_help_only_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    // Hackish - if we have no arguments other than the command, we are a "naked invocation" and we
    // just print help.
    if (argc == 1) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_INVALID_ARGS;
    }

    return STATUS_CMD_ERROR;
}

/// Implementation of the builtin count command, used to count the number of arguments sent to it.
static int builtin_count(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    int argc = builtin_count_args(argv);
    streams.out.append_format(L"%d\n", argc - 1);
    return argc - 1 == 0 ? STATUS_CMD_ERROR : STATUS_CMD_OK;
}

/// This function handles both the 'continue' and the 'break' builtins that are used for loop
/// control.
static int builtin_break_continue(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    int is_break = (wcscmp(argv[0], L"break") == 0);
    int argc = builtin_count_args(argv);

    if (argc != 1) {
        streams.err.append_format(BUILTIN_ERR_UNKNOWN, argv[0], argv[1]);

        builtin_print_help(parser, streams, argv[0], streams.err);
        return STATUS_INVALID_ARGS;
    }

    // Find the index of the enclosing for or while loop. Recall that incrementing loop_idx goes
    // 'up' to outer blocks.
    size_t loop_idx;
    for (loop_idx = 0; loop_idx < parser.block_count(); loop_idx++) {
        const block_t *b = parser.block_at_index(loop_idx);
        if (b->type() == WHILE || b->type() == FOR) break;
    }

    if (loop_idx >= parser.block_count()) {
        streams.err.append_format(_(L"%ls: Not inside of loop\n"), argv[0]);
        builtin_print_help(parser, streams, argv[0], streams.err);
        return STATUS_CMD_ERROR;
    }

    // Skip blocks interior to the loop (but not the loop itself)
    size_t block_idx = loop_idx;
    while (block_idx--) {
        parser.block_at_index(block_idx)->skip = true;
    }

    // Mark the loop's status
    block_t *loop_block = parser.block_at_index(loop_idx);
    loop_block->loop_status = is_break ? LOOP_BREAK : LOOP_CONTINUE;
    return STATUS_CMD_OK;
}

/// Implementation of the builtin breakpoint command, used to launch the interactive debugger.
static int builtin_breakpoint(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    if (argv[1] != NULL) {
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT1, cmd, 0, builtin_count_args(argv) - 1);
        return STATUS_INVALID_ARGS;
    }

    // If we're not interactive then we can't enter the debugger. So treat this command as a no-op.
    if (!shell_is_interactive()) {
        return STATUS_CMD_ERROR;
    }

    // Ensure we don't allow creating a breakpoint at an interactive prompt. There may be a simpler
    // or clearer way to do this but this works.
    const block_t *block1 = parser.block_at_index(1);
    if (!block1 || block1->type() == BREAKPOINT) {
        streams.err.append_format(_(L"%ls: Command not valid at an interactive prompt\n"), cmd);
        return STATUS_ILLEGAL_CMD;
    }

    const breakpoint_block_t *bpb = parser.push_block<breakpoint_block_t>();
    reader_read(STDIN_FILENO, streams.io_chain ? *streams.io_chain : io_chain_t());
    parser.pop_block(bpb);
    return proc_get_last_status();
}

int builtin_true(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    UNUSED(streams);
    if (argv[1] != NULL) {
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT1, argv[0], 0, builtin_count_args(argv) - 1);
        return STATUS_INVALID_ARGS;
    }
    return STATUS_CMD_OK;
}

int builtin_false(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    UNUSED(streams);
    if (argv[1] != NULL) {
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT1, argv[0], 0, builtin_count_args(argv) - 1);
        return STATUS_INVALID_ARGS;
    }
    return STATUS_CMD_ERROR;
}

// END OF BUILTIN COMMANDS
// Below are functions for handling the builtin commands.
// THESE MUST BE SORTED BY NAME! Completion lookup uses binary search.

// Data about all the builtin commands in fish.
// Functions that are bound to builtin_generic are handled directly by the parser.
// NOTE: These must be kept in sorted order!
static const builtin_data_t builtin_datas[] = {
    {L"[", &builtin_test, N_(L"Test a condition")},
    {L"and", &builtin_generic, N_(L"Execute command if previous command suceeded")},
    {L"argparse", &builtin_argparse, N_(L"Parse options in fish script")},
    {L"begin", &builtin_generic, N_(L"Create a block of code")},
    {L"bg", &builtin_bg, N_(L"Send job to background")},
    {L"bind", &builtin_bind, N_(L"Handle fish key bindings")},
    {L"block", &builtin_block, N_(L"Temporarily block delivery of events")},
    {L"break", &builtin_break_continue, N_(L"Stop the innermost loop")},
    {L"breakpoint", &builtin_breakpoint,
     N_(L"Temporarily halt execution of a script and launch an interactive debug prompt")},
    {L"builtin", &builtin_builtin, N_(L"Run a builtin command instead of a function")},
    {L"case", &builtin_generic, N_(L"Conditionally execute a block of commands")},
    {L"cd", &builtin_cd, N_(L"Change working directory")},
    {L"command", &builtin_command, N_(L"Run a program instead of a function or builtin")},
    {L"commandline", &builtin_commandline, N_(L"Set or get the commandline")},
    {L"complete", &builtin_complete, N_(L"Edit command specific completions")},
    {L"contains", &builtin_contains, N_(L"Search for a specified string in a list")},
    {L"continue", &builtin_break_continue,
     N_(L"Skip the rest of the current lap of the innermost loop")},
    {L"count", &builtin_count, N_(L"Count the number of arguments")},
    {L"disown", &builtin_disown, N_(L"Remove job from job list")},
    {L"echo", &builtin_echo, N_(L"Print arguments")},
    {L"else", &builtin_generic, N_(L"Evaluate block if condition is false")},
    {L"emit", &builtin_emit, N_(L"Emit an event")},
    {L"end", &builtin_generic, N_(L"End a block of commands")},
    {L"exec", &builtin_generic, N_(L"Run command in current process")},
    {L"exit", &builtin_exit, N_(L"Exit the shell")},
    {L"false", &builtin_false, N_(L"Return an unsuccessful result")},
    {L"fg", &builtin_fg, N_(L"Send job to foreground")},
    {L"for", &builtin_generic, N_(L"Perform a set of commands multiple times")},
    {L"function", &builtin_generic, N_(L"Define a new function")},
    {L"functions", &builtin_functions, N_(L"List or remove functions")},
    {L"history", &builtin_history, N_(L"History of commands executed by user")},
    {L"if", &builtin_generic, N_(L"Evaluate block if condition is true")},
    {L"jobs", &builtin_jobs, N_(L"Print currently running jobs")},
    {L"math", &builtin_math, N_(L"Evaluate math expressions")},
    {L"not", &builtin_generic, N_(L"Negate exit status of job")},
    {L"or", &builtin_generic, N_(L"Execute command if previous command failed")},
    {L"printf", &builtin_printf, N_(L"Prints formatted text")},
    {L"pwd", &builtin_pwd, N_(L"Print the working directory")},
    {L"random", &builtin_random, N_(L"Generate random number")},
    {L"read", &builtin_read, N_(L"Read a line of input into variables")},
    {L"realpath", &builtin_realpath, N_(L"Convert path to absolute path without symlinks")},
    {L"return", &builtin_return, N_(L"Stop the currently evaluated function")},
    {L"set", &builtin_set, N_(L"Handle environment variables")},
    {L"set_color", &builtin_set_color, N_(L"Set the terminal color")},
    {L"source", &builtin_source, N_(L"Evaluate contents of file")},
    {L"status", &builtin_status, N_(L"Return status information about fish")},
    {L"string", &builtin_string, N_(L"Manipulate strings")},
    {L"switch", &builtin_generic, N_(L"Conditionally execute a block of commands")},
    {L"test", &builtin_test, N_(L"Test a condition")},
    {L"true", &builtin_true, N_(L"Return a successful result")},
    {L"ulimit", &builtin_ulimit, N_(L"Set or get the shells resource usage limits")},
    {L"wait", &builtin_wait, N_(L"Wait for background processes completed")},
    {L"while", &builtin_generic, N_(L"Perform a command multiple times")}};

#define BUILTIN_COUNT (sizeof builtin_datas / sizeof *builtin_datas)

/// Look up a builtin_data_t for a specified builtin
///
/// @param  name
///    Name of the builtin
///
/// @return
///    Pointer to a builtin_data_t
///
static const builtin_data_t *builtin_lookup(const wcstring &name) {
    const builtin_data_t *array_end = builtin_datas + BUILTIN_COUNT;
    const builtin_data_t *found = std::lower_bound(builtin_datas, array_end, name);
    if (found != array_end && name == found->name) {
        return found;
    }
    return NULL;
}

/// Initialize builtin data.
void builtin_init() {
    for (size_t i = 0; i < BUILTIN_COUNT; i++) {
        const wchar_t *name = builtin_datas[i].name;
        intern_static(name);
        assert((i == 0 || wcscmp(builtin_datas[i - 1].name, name) < 0) &&
               "builtins are not sorted alphabetically");
    }
}

/// Is there a builtin command with the given name?
bool builtin_exists(const wcstring &cmd) { return static_cast<bool>(builtin_lookup(cmd)); }

/// Is the command a keyword we need to special-case the handling of `-h` and `--help`.
static const wchar_t *const help_builtins[] = {L"for", L"while",  L"function", L"if",
                                               L"end", L"switch", L"case"};
static bool cmd_needs_help(const wchar_t *cmd) { return contains(help_builtins, cmd); }

/// Execute a builtin command
int builtin_run(parser_t &parser, int job_pgid, wchar_t **argv, io_streams_t &streams) {
    UNUSED(parser);
    UNUSED(streams);
    if (argv == NULL || argv[0] == NULL) return STATUS_INVALID_ARGS;

    // We can be handed a keyword by the parser as if it was a command. This happens when the user
    // follows the keyword by `-h` or `--help`. Since it isn't really a builtin command we need to
    // handle displaying help for it here.
    if (argv[1] && !argv[2] && parse_util_argument_is_help(argv[1]) && cmd_needs_help(argv[0])) {
        builtin_print_help(parser, streams, argv[0], streams.out);
        return STATUS_CMD_OK;
    }

    if (const builtin_data_t *data = builtin_lookup(argv[0])) {
        // If we are interactive, save the foreground pgroup and restore it after in case the
        // builtin needs to read from the terminal. See #4540.
        bool grab_tty = is_interactive_session && isatty(streams.stdin_fd);
        pid_t pgroup_to_restore = grab_tty ? terminal_acquire_before_builtin(job_pgid) : -1;
        int ret = data->func(parser, streams, argv);
        if (pgroup_to_restore >= 0) {
            tcsetpgrp(STDIN_FILENO, pgroup_to_restore);
        }
        return ret;
    }

    debug(0, UNKNOWN_BUILTIN_ERR_MSG, argv[0]);
    return STATUS_CMD_ERROR;
}

/// Returns a list of all builtin names.
wcstring_list_t builtin_get_names() {
    wcstring_list_t result;
    result.reserve(BUILTIN_COUNT);
    for (size_t i = 0; i < BUILTIN_COUNT; i++) {
        result.push_back(builtin_datas[i].name);
    }
    return result;
}

/// Insert all builtin names into list.
void builtin_get_names(std::vector<completion_t> *list) {
    assert(list != NULL);
    list->reserve(list->size() + BUILTIN_COUNT);
    for (size_t i = 0; i < BUILTIN_COUNT; i++) {
        append_completion(list, builtin_datas[i].name);
    }
}

/// Return a one-line description of the specified builtin.
wcstring builtin_get_desc(const wcstring &name) {
    wcstring result;
    const builtin_data_t *builtin = builtin_lookup(name);
    if (builtin) {
        result = _(builtin->desc);
    }
    return result;
}
