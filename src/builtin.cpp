// Functions for executing builtin functions.
//
// How to add a new builtin function:
//
// 1). Create a function in builtin.c with the following signature:
//
//     <tt>static maybe_t<int> builtin_NAME(parser_t &parser, io_streams_t &streams, wchar_t
//     **argv)</tt>
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

#include <cerrno>
#include <cstring>
#include <cwchar>
#include <deque>
#include <memory>
#include <string>

#include "builtins/argparse.h"
#include "builtins/bind.h"
#include "builtins/cd.h"
#include "builtins/commandline.h"
#include "builtins/complete.h"
#include "builtins/disown.h"
#include "builtins/eval.h"
#include "builtins/fg.h"
#include "builtins/functions.h"
#include "builtins/history.h"
#include "builtins/jobs.h"
#include "builtins/path.h"
#include "builtins/read.h"
#include "builtins/set.h"
#include "builtins/set_color.h"
#include "builtins/shared.rs.h"
#include "builtins/source.h"
#include "builtins/status.h"
#include "builtins/string.h"
#include "builtins/test.h"
#include "builtins/ulimit.h"
#include "complete.h"
#include "cxx.h"
#include "cxxgen.h"
#include "fallback.h"  // IWYU pragma: keep
#include "ffi.h"
#include "flog.h"
#include "io.h"
#include "null_terminated_array.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "parser.h"
#include "proc.h"
#include "reader.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

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
void builtin_wperror(const wchar_t *program_name, io_streams_t &streams) {
    char *err = std::strerror(errno);
    if (program_name != nullptr) {
        streams.err.append(program_name);
        streams.err.append(L": ");
    }
    if (err != nullptr) {
        const wcstring werr = str2wcstring(err);
        streams.err.append(werr);
        streams.err.push(L'\n');
    }
}

static const wchar_t *const short_options = L"+:h";
static const struct woption long_options[] = {{L"help", no_argument, 'h'}, {}};

int parse_help_only_cmd_opts(struct help_only_cmd_opts_t &opts, int *optind, int argc,
                             const wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    const wchar_t *cmd = argv[0];
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
                        const wcstring &error_message) {
    // This won't ever work if no_exec is set.
    if (no_exec()) return;
    const wcstring name_esc = escape_string(name);
    wcstring cmd = format_string(L"__fish_print_help %ls ", name_esc.c_str());
    io_chain_t ios;
    if (!error_message.empty()) {
        cmd.append(escape_string(error_message));
        // If it's an error, redirect the output of __fish_print_help to stderr
        ios.push_back(std::make_shared<io_fd_t>(STDOUT_FILENO, STDERR_FILENO));
    }
    auto res = parser.eval(cmd, ios);
    if (res.status.normal_exited() && res.status.exit_code() == 2) {
        streams.err.append_format(BUILTIN_ERR_MISSING_HELP, name_esc.c_str(), name_esc.c_str());
    }
}

/// Perform error reporting for encounter with unknown option.
void builtin_unknown_option(parser_t &parser, io_streams_t &streams, const wchar_t *cmd,
                            const wchar_t *opt, bool print_hints) {
    streams.err.append_format(BUILTIN_ERR_UNKNOWN, cmd, opt);
    if (print_hints) {
        builtin_print_error_trailer(parser, streams.err, cmd);
    }
}

/// Perform error reporting for encounter with missing argument.
void builtin_missing_argument(parser_t &parser, io_streams_t &streams, const wchar_t *cmd,
                              const wchar_t *opt, bool print_hints) {
    if (opt[0] == L'-' && opt[1] != L'-') {
        // if c in -qc '-qc' is missing the argument, now opt is just 'c'
        opt += std::wcslen(opt) - 1;
        // now prepend - to output -c
        streams.err.append_format(BUILTIN_ERR_MISSING, cmd, wcstring(L"-").append(opt).c_str());
    } else
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

// END OF BUILTIN COMMANDS
// Below are functions for handling the builtin commands.
// THESE MUST BE SORTED BY NAME! Completion lookup uses binary search.

// Data about all the builtin commands in fish.
// Functions that are bound to builtin_generic are handled directly by the parser.
// NOTE: These must be kept in sorted order!
static constexpr builtin_data_t builtin_datas[] = {
    {L".", &builtin_source, N_(L"Evaluate contents of file")},
    {L":", &builtin_true, N_(L"Return a successful result")},
    {L"[", &builtin_test, N_(L"Test a condition")},
    {L"_", &builtin_gettext, N_(L"Translate a string")},
    {L"abbr", &implemented_in_rust, N_(L"Manage abbreviations")},
    {L"and", &builtin_generic, N_(L"Run command if last command succeeded")},
    {L"argparse", &builtin_argparse, N_(L"Parse options in fish script")},
    {L"begin", &builtin_generic, N_(L"Create a block of code")},
    {L"bg", &implemented_in_rust, N_(L"Send job to background")},
    {L"bind", &builtin_bind, N_(L"Handle fish key bindings")},
    {L"block", &implemented_in_rust, N_(L"Temporarily block delivery of events")},
    {L"break", &builtin_break_continue, N_(L"Stop the innermost loop")},
    {L"breakpoint", &builtin_breakpoint, N_(L"Halt execution and start debug prompt")},
    {L"builtin", &implemented_in_rust, N_(L"Run a builtin specifically")},
    {L"case", &builtin_generic, N_(L"Block of code to run conditionally")},
    {L"cd", &builtin_cd, N_(L"Change working directory")},
    {L"command", &implemented_in_rust, N_(L"Run a command specifically")},
    {L"commandline", &builtin_commandline, N_(L"Set or get the commandline")},
    {L"complete", &builtin_complete, N_(L"Edit command specific completions")},
    {L"contains", &implemented_in_rust, N_(L"Search for a specified string in a list")},
    {L"continue", &builtin_break_continue, N_(L"Skip over remaining innermost loop")},
    {L"count", &builtin_count, N_(L"Count the number of arguments")},
    {L"disown", &builtin_disown, N_(L"Remove job from job list")},
    {L"echo", &implemented_in_rust, N_(L"Print arguments")},
    {L"else", &builtin_generic, N_(L"Evaluate block if condition is false")},
    {L"emit", &implemented_in_rust, N_(L"Emit an event")},
    {L"end", &builtin_generic, N_(L"End a block of commands")},
    {L"eval", &builtin_eval, N_(L"Evaluate a string as a statement")},
    {L"exec", &builtin_generic, N_(L"Run command in current process")},
    {L"exit", &implemented_in_rust, N_(L"Exit the shell")},
    {L"false", &builtin_false, N_(L"Return an unsuccessful result")},
    {L"fg", &builtin_fg, N_(L"Send job to foreground")},
    {L"for", &builtin_generic, N_(L"Perform a set of commands multiple times")},
    {L"function", &builtin_generic, N_(L"Define a new function")},
    {L"functions", &builtin_functions, N_(L"List or remove functions")},
    {L"history", &builtin_history, N_(L"History of commands executed by user")},
    {L"if", &builtin_generic, N_(L"Evaluate block if condition is true")},
    {L"jobs", &builtin_jobs, N_(L"Print currently running jobs")},
    {L"math", &implemented_in_rust, N_(L"Evaluate math expressions")},
    {L"not", &builtin_generic, N_(L"Negate exit status of job")},
    {L"or", &builtin_generic, N_(L"Execute command if previous command failed")},
    {L"path", &builtin_path, N_(L"Handle paths")},
    {L"printf", &implemented_in_rust, N_(L"Prints formatted text")},
    {L"pwd", &implemented_in_rust, N_(L"Print the working directory")},
    {L"random", &implemented_in_rust, N_(L"Generate random number")},
    {L"read", &builtin_read, N_(L"Read a line of input into variables")},
    {L"realpath", &implemented_in_rust, N_(L"Show absolute path sans symlinks")},
    {L"return", &implemented_in_rust, N_(L"Stop the currently evaluated function")},
    {L"set", &builtin_set, N_(L"Handle environment variables")},
    {L"set_color", &builtin_set_color, N_(L"Set the terminal color")},
    {L"source", &builtin_source, N_(L"Evaluate contents of file")},
    {L"status", &builtin_status, N_(L"Return status information about fish")},
    {L"string", &builtin_string, N_(L"Manipulate strings")},
    {L"switch", &builtin_generic, N_(L"Conditionally run blocks of code")},
    {L"test", &builtin_test, N_(L"Test a condition")},
    {L"time", &builtin_generic, N_(L"Measure how long a command or block takes")},
    {L"true", &builtin_true, N_(L"Return a successful result")},
    {L"type", &implemented_in_rust, N_(L"Check if a thing is a thing")},
    {L"ulimit", &builtin_ulimit, N_(L"Get/set resource usage limits")},
    {L"wait", &implemented_in_rust, N_(L"Wait for background processes completed")},
    {L"while", &builtin_generic, N_(L"Perform a command multiple times")},
};
ASSERT_SORTED_BY_NAME(builtin_datas);

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
    return get_by_sorted_name(name.c_str(), builtin_datas);
}

/// Is there a builtin command with the given name?
bool builtin_exists(const wcstring &cmd) { return static_cast<bool>(builtin_lookup(cmd)); }

/// Is the command a keyword we need to special-case the handling of `-h` and `--help`.
static const wchar_t *const help_builtins[] = {L"for", L"while",  L"function", L"if",
                                               L"end", L"switch", L"case"};
static bool cmd_needs_help(const wcstring &cmd) { return contains(help_builtins, cmd); }

/// Returns a list of all builtin names.
std::vector<wcstring> builtin_get_names() {
    std::vector<wcstring> result;
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
