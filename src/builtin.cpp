// Functions for executing builtin functions.
//
// How to add a new builtin function:
//
// 1). Create a function in builtin.c with the following signature:
//
//     <tt>static maybe_t<int> builtin_NAME(parser_t &parser, io_streams_t &streams, wchar_t **argv)</tt>
//
// where NAME is the name of the builtin, and args is a zero-terminated list of arguments.
//
// 2). Add a line like { L"NAME", &builtin_NAME, N_(L"Bla bla bla") }, to the builtin_data_t
// variable. The description is used by the completion system. Note that this array is sorted.
//
// 3). Create a file doc_src/NAME.rst, containing the manual for the builtin in
// reStructuredText-format. Check the other builtin manuals for proper syntax.
//
// 4). Use 'git add doc_src/NAME.txt' to start tracking changes to the documentation file.
#include "config.h"  // IWYU pragma: keep

#include "builtin.h"

#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <memory>
#include <string>

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
#include "builtin_eval.h"
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
#include "builtin_type.h"
#include "builtin_ulimit.h"
#include "builtin_wait.h"
#include "common.h"
#include "complete.h"
#include "exec.h"
#include "fallback.h"  // IWYU pragma: keep
#include "flog.h"
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
    return std::wcscmp(this->name, other.c_str()) < 0;
}

bool builtin_data_t::operator<(const builtin_data_t *other) const {
    return std::wcscmp(this->name, other->name) < 0;
}

/// Counts the number of arguments in the specified null-terminated array
int builtin_count_args(const wchar_t *const *argv) {
    int argc;
    for (argc = 1; argv[argc] != nullptr;) {
        argc++;
    }

    assert(argv[argc] == nullptr);
    return argc;
}

/// This function works like wperror, but it prints its result into the streams.err string instead
/// to stderr. Used by the builtin commands.
void builtin_wperror(const wchar_t *s, io_streams_t &streams) {
    char *err = std::strerror(errno);
    if (s != nullptr) {
        streams.err.append(s);
        streams.err.append(L": ");
    }
    if (err != nullptr) {
        const wcstring werr = str2wcstring(err);
        streams.err.append(werr);
        streams.err.push_back(L'\n');
    }
}

static const wchar_t *const short_options = L"+:h";
static const struct woption long_options[] = {{L"help", no_argument, nullptr, 'h'},
                                              {nullptr, 0, nullptr, 0}};

int parse_help_only_cmd_opts(struct help_only_cmd_opts_t &opts, int *optind, int argc,
                             wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
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
            }
        }
    }

    *optind = w.woptind;
    return STATUS_CMD_OK;
}

/// Display help/usage information for the specified builtin or function from manpage
///
/// @param  name
///    builtin or function name to get up help for
///
/// Process and print help for the specified builtin or function.
void builtin_print_help(parser_t &parser, const io_streams_t &streams, const wchar_t *name,
                        wcstring *error_message) {
    UNUSED(streams);
    // This won't ever work if no_exec is set.
    if (no_exec()) return;
    const wcstring name_esc = escape_string(name, ESCAPE_ALL);
    wcstring cmd = format_string(L"__fish_print_help %ls ", name_esc.c_str());
    io_chain_t ios;
    if (error_message) {
        cmd.append(escape_string(*error_message, ESCAPE_ALL));
        // If it's an error, redirect the output of __fish_print_help to stderr
        ios.push_back(std::make_shared<io_fd_t>(STDOUT_FILENO, STDERR_FILENO));
    }
    parser.eval(cmd, ios);
    // ignore the exit status of __fish_print_help
}

/// Perform error reporting for encounter with unknown option.
void builtin_unknown_option(parser_t &parser, io_streams_t &streams, const wchar_t *cmd,
                            const wchar_t *opt) {
    streams.err.append_format(BUILTIN_ERR_UNKNOWN, cmd, opt);
    builtin_print_error_trailer(parser, streams.err, cmd);
}

/// Perform error reporting for encounter with missing argument.
void builtin_missing_argument(parser_t &parser, io_streams_t &streams, const wchar_t *cmd,
                              const wchar_t *opt, bool print_hints) {
    if (opt[0] == L'-' && opt[1] != L'-') {
        opt += std::wcslen(opt) - 1;
    }
    streams.err.append_format(BUILTIN_ERR_MISSING, cmd, opt);
    if (print_hints) {
        builtin_print_error_trailer(parser, streams.err, cmd);
    }
}

/// Print the backtrace and call for help that we use at the end of error messages.
void builtin_print_error_trailer(parser_t &parser, output_stream_t &b, const wchar_t *cmd) {
    b.append(L"\n");
    const wcstring stacktrace = parser.current_line();
    // Don't print two empty lines if we don't have a stacktrace.
    if (!stacktrace.empty()) {
        b.append(stacktrace);
        b.append(L"\n");
    }
    b.append_format(_(L"(Type 'help %ls' for related documentation)\n"), cmd);
}

/// A generic bultin that only supports showing a help message. This is only a placeholder that
/// prints the help message. Useful for commands that live in the parser.
static maybe_t<int> builtin_generic(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    help_only_cmd_opts_t opts;
    int optind;
    int retval = parse_help_only_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    // Hackish - if we have no arguments other than the command, we are a "naked invocation" and we
    // just print help.
    if (argc == 1 || wcscmp(cmd, L"time") == 0) {
        builtin_print_help(parser, streams, cmd);
        return STATUS_INVALID_ARGS;
    }

    return STATUS_CMD_ERROR;
}

// How many bytes we read() at once.
// Since this is just for counting, it can be massive.
#define COUNT_CHUNK_SIZE (512 * 256)
/// Implementation of the builtin count command, used to count the number of arguments sent to it.
static maybe_t<int> builtin_count(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    int argc = 0;

    // Count the newlines coming in via stdin like `wc -l`.
    if (streams.stdin_is_directly_redirected) {
        char buf[COUNT_CHUNK_SIZE];
        while (true) {
            long n = read_blocked(streams.stdin_fd, buf, COUNT_CHUNK_SIZE);
            // Ignore all errors for now.
            if (n <= 0) break;
            for (int i = 0; i < n; i++) {
                if (buf[i] == L'\n') {
                    argc++;
                }
            }
        }
    }

    // Always add the size of argv.
    // That means if you call `something | count a b c`, you'll get the count of something _plus 3_.
    argc += builtin_count_args(argv) - 1;
    streams.out.append_format(L"%d\n", argc);
    return argc == 0 ? STATUS_CMD_ERROR : STATUS_CMD_OK;
}

/// This function handles both the 'continue' and the 'break' builtins that are used for loop
/// control.
static maybe_t<int> builtin_break_continue(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    int is_break = (std::wcscmp(argv[0], L"break") == 0);
    int argc = builtin_count_args(argv);

    if (argc != 1) {
        wcstring error_message = format_string(BUILTIN_ERR_UNKNOWN, argv[0], argv[1]);
        builtin_print_help(parser, streams, argv[0], &error_message);
        return STATUS_INVALID_ARGS;
    }

    // Paranoia: ensure we have a real loop.
    bool has_loop = false;
    for (const auto &b : parser.blocks()) {
        if (b.type() == block_type_t::while_block || b.type() == block_type_t::for_block) {
            has_loop = true;
            break;
        }
        if (b.is_function_call()) break;
    }
    if (!has_loop) {
        wcstring error_message = format_string(_(L"%ls: Not inside of loop\n"), argv[0]);
        builtin_print_help(parser, streams, argv[0], &error_message);
        return STATUS_CMD_ERROR;
    }

    // Mark the status in the libdata.
    parser.libdata().loop_status = is_break ? loop_status_t::breaks : loop_status_t::continues;
    return STATUS_CMD_OK;
}

/// Implementation of the builtin breakpoint command, used to launch the interactive debugger.
static maybe_t<int> builtin_breakpoint(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    if (argv[1] != nullptr) {
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT1, cmd, 0, builtin_count_args(argv) - 1);
        return STATUS_INVALID_ARGS;
    }

    // If we're not interactive then we can't enter the debugger. So treat this command as a no-op.
    if (!parser.is_interactive()) {
        return STATUS_CMD_ERROR;
    }

    // Ensure we don't allow creating a breakpoint at an interactive prompt. There may be a simpler
    // or clearer way to do this but this works.
    const block_t *block1 = parser.block_at_index(1);
    if (!block1 || block1->type() == block_type_t::breakpoint) {
        streams.err.append_format(_(L"%ls: Command not valid at an interactive prompt\n"), cmd);
        return STATUS_ILLEGAL_CMD;
    }

    const block_t *bpb = parser.push_block(block_t::breakpoint_block());
    reader_read(parser, STDIN_FILENO, streams.io_chain ? *streams.io_chain : io_chain_t());
    parser.pop_block(bpb);
    return parser.get_last_status();
}

maybe_t<int> builtin_true(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    UNUSED(streams);
    UNUSED(argv);
    return STATUS_CMD_OK;
}

maybe_t<int> builtin_false(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    UNUSED(streams);
    UNUSED(argv);
    return STATUS_CMD_ERROR;
}

maybe_t<int> builtin_gettext(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    UNUSED(streams);
    for (int i = 1; i < builtin_count_args(argv); i++) {
        streams.out.append(_(argv[i]));
    }
    return STATUS_CMD_OK;
}

// END OF BUILTIN COMMANDS
// Below are functions for handling the builtin commands.
// THESE MUST BE SORTED BY NAME! Completion lookup uses binary search.

// Data about all the builtin commands in fish.
// Functions that are bound to builtin_generic are handled directly by the parser.
// NOTE: These must be kept in sorted order!
static const builtin_data_t builtin_datas[] = {
    {L".", &builtin_source, N_(L"Evaluate contents of file")},
    {L":", &builtin_true, N_(L"Return a successful result")},
    {L"[", &builtin_test, N_(L"Test a condition")},
    {L"_", &builtin_gettext, N_(L"Translate a string")},
    {L"and", &builtin_generic, N_(L"Execute command if previous command succeeded")},
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
    {L"eval", &builtin_eval, N_(L"Evaluate a string as a statement")},
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
    {L"time", &builtin_generic, N_(L"Measure how long a command or block takes")},
    {L"true", &builtin_true, N_(L"Return a successful result")},
    {L"type", &builtin_type, N_(L"Check if a thing is a thing")},
    {L"ulimit", &builtin_ulimit, N_(L"Set or get the shells resource usage limits")},
    {L"wait", &builtin_wait, N_(L"Wait for background processes completed")},
    {L"while", &builtin_generic, N_(L"Perform a command multiple times")},
};

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
    return nullptr;
}

/// Initialize builtin data.
void builtin_init() {
    for (size_t i = 0; i < BUILTIN_COUNT; i++) {
        const wchar_t *name = builtin_datas[i].name;
        intern_static(name);
        assert((i == 0 || std::wcscmp(builtin_datas[i - 1].name, name) < 0) &&
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
proc_status_t builtin_run(parser_t &parser, wchar_t **argv, io_streams_t &streams) {
    UNUSED(parser);
    UNUSED(streams);
    if (argv == nullptr || argv[0] == nullptr)
        return proc_status_t::from_exit_code(STATUS_INVALID_ARGS);

    // We can be handed a keyword by the parser as if it was a command. This happens when the user
    // follows the keyword by `-h` or `--help`. Since it isn't really a builtin command we need to
    // handle displaying help for it here.
    if (argv[1] && !argv[2] && parse_util_argument_is_help(argv[1]) && cmd_needs_help(argv[0])) {
        builtin_print_help(parser, streams, argv[0]);
        return proc_status_t::from_exit_code(STATUS_CMD_OK);
    }

    if (const builtin_data_t *data = builtin_lookup(argv[0])) {
        maybe_t<int> ret = data->func(parser, streams, argv);
        if (!ret) {
            return proc_status_t::empty();
        }
        return proc_status_t::from_exit_code(ret.value());
    }

    FLOGF(error, UNKNOWN_BUILTIN_ERR_MSG, argv[0]);
    return proc_status_t::from_exit_code(STATUS_CMD_ERROR);
}

/// Returns a list of all builtin names.
wcstring_list_t builtin_get_names() {
    wcstring_list_t result;
    result.reserve(BUILTIN_COUNT);
    for (const auto &builtin_data : builtin_datas) {
        result.push_back(builtin_data.name);
    }
    return result;
}

/// Insert all builtin names into list.
void builtin_get_names(completion_list_t *list) {
    assert(list != nullptr);
    list->reserve(list->size() + BUILTIN_COUNT);
    for (const auto &builtin_data : builtin_datas) {
        append_completion(list, builtin_data.name);
    }
}

/// Return a one-line description of the specified builtin.
const wchar_t *builtin_get_desc(const wcstring &name) {
    const wchar_t *result = L"";
    const builtin_data_t *builtin = builtin_lookup(name);
    if (builtin) {
        result = _(builtin->desc);
    }
    return result;
}
