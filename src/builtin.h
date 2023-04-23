// Prototypes for functions for executing builtin functions.
#ifndef FISH_BUILTIN_H
#define FISH_BUILTIN_H

#include <vector>

#include "common.h"
#include "complete.h"
#include "maybe.h"
#include "wutil.h"

class parser_t;
class proc_status_t;
class output_stream_t;
struct io_streams_t;
using completion_list_t = std::vector<completion_t>;

/// Data structure to describe a builtin.
struct builtin_data_t {
    // Name of the builtin.
    const wchar_t *name;
    // Function pointer to the builtin implementation.
    maybe_t<int> (*func)(parser_t &parser, io_streams_t &streams, const wchar_t **argv);
    // Description of what the builtin does.
    const wchar_t *desc;
};

/// The default prompt for the read command.
#define DEFAULT_READ_PROMPT L"set_color green; echo -n read; set_color normal; echo -n \"> \""

/// Error message on missing argument.
#define BUILTIN_ERR_MISSING _(L"%ls: %ls: option requires an argument\n")

/// Error message on missing man page.
#define BUILTIN_ERR_MISSING_HELP                                                            \
    _(L"fish: %ls: missing man page\nDocumentation may not be installed.\n`help %ls` will " \
      L"show an online version\n")

/// Error message on invalid combination of options.
#define BUILTIN_ERR_COMBO _(L"%ls: invalid option combination\n")

/// Error message on invalid combination of options.
#define BUILTIN_ERR_COMBO2 _(L"%ls: invalid option combination, %ls\n")
#define BUILTIN_ERR_COMBO2_EXCLUSIVE _(L"%ls: %ls %ls: options cannot be used together\n")

/// Error message on multiple scope levels for variables.
#define BUILTIN_ERR_GLOCAL _(L"%ls: scope can be only one of: universal function global local\n")

/// Error message for specifying both export and unexport to set/read.
#define BUILTIN_ERR_EXPUNEXP _(L"%ls: cannot both export and unexport\n")

/// Error message for unknown switch.
#define BUILTIN_ERR_UNKNOWN _(L"%ls: %ls: unknown option\n")

/// Error message for unexpected args.
#define BUILTIN_ERR_ARG_COUNT0 _(L"%ls: missing argument\n")
#define BUILTIN_ERR_ARG_COUNT1 _(L"%ls: expected %d arguments; got %d\n")
#define BUILTIN_ERR_ARG_COUNT2 _(L"%ls: %ls: expected %d arguments; got %d\n")
#define BUILTIN_ERR_MIN_ARG_COUNT1 _(L"%ls: expected >= %d arguments; got %d\n")
#define BUILTIN_ERR_MAX_ARG_COUNT1 _(L"%ls: expected <= %d arguments; got %d\n")

/// Error message for invalid variable name.
#define BUILTIN_ERR_VARNAME _(L"%ls: %ls: invalid variable name. See `help identifiers`\n")

/// Error message for invalid bind mode name.
#define BUILTIN_ERR_BIND_MODE _(L"%ls: %ls: invalid mode name. See `help identifiers`\n")

/// Error message when too many arguments are supplied to a builtin.
#define BUILTIN_ERR_TOO_MANY_ARGUMENTS _(L"%ls: too many arguments\n")

/// Error message when integer expected
#define BUILTIN_ERR_NOT_NUMBER _(L"%ls: %ls: invalid integer\n")

/// Command that requires a subcommand was invoked without a recognized subcommand.
#define BUILTIN_ERR_MISSING_SUBCMD _(L"%ls: missing subcommand\n")
#define BUILTIN_ERR_INVALID_SUBCMD _(L"%ls: %ls: invalid subcommand\n")

/// The send stuff to foreground message.
#define FG_MSG _(L"Send job %d (%ls) to foreground\n")

bool builtin_exists(const wcstring &cmd);

proc_status_t builtin_run(parser_t &parser, const std::vector<wcstring> &argv,
                          io_streams_t &streams);

std::vector<wcstring> builtin_get_names();
wcstring_list_ffi_t builtin_get_names_ffi();
void builtin_get_names(completion_list_t *list);
const wchar_t *builtin_get_desc(const wcstring &name);

wcstring builtin_help_get(parser_t &parser, const wchar_t *cmd);

void builtin_print_help(parser_t &parser, const io_streams_t &streams, const wchar_t *name,
                        const wcstring &error_message = {});
int builtin_count_args(const wchar_t *const *argv);

void builtin_unknown_option(parser_t &parser, io_streams_t &streams, const wchar_t *cmd,
                            const wchar_t *opt, bool print_hints = true);

void builtin_missing_argument(parser_t &parser, io_streams_t &streams, const wchar_t *cmd,
                              const wchar_t *opt, bool print_hints = true);

void builtin_print_error_trailer(parser_t &parser, output_stream_t &b, const wchar_t *cmd);

void builtin_wperror(const wchar_t *program_name, io_streams_t &streams);

struct help_only_cmd_opts_t {
    bool print_help = false;
};
int parse_help_only_cmd_opts(help_only_cmd_opts_t &opts, int *optind, int argc,
                             const wchar_t **argv, parser_t &parser, io_streams_t &streams);

/// An enum of the builtins implemented in Rust.
enum class RustBuiltin : int32_t {
    Abbr,
    Argparse,
    Bg,
    Block,
    Builtin,
    Cd,
    Contains,
    Command,
    Echo,
    Emit,
    Exit,
    Math,
    Printf,
    Pwd,
    Random,
    Realpath,
    Return,
    SetColor,
    Status,
    Test,
    Type,
    Wait,
};
#endif
