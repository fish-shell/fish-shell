// Prototypes for functions for executing builtin functions.
#ifndef FISH_BUILTIN_H
#define FISH_BUILTIN_H

#include <stddef.h>

#include <vector>

#include "common.h"

class completion_t;
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
    maybe_t<int> (*func)(parser_t &parser, io_streams_t &streams, wchar_t **argv);
    // Description of what the builtin does.
    const wchar_t *desc;

    bool operator<(const wcstring &) const;
    bool operator<(const builtin_data_t *) const;
};

/// The default prompt for the read command.
#define DEFAULT_READ_PROMPT L"set_color green; echo -n read; set_color normal; echo -n \"> \""

enum { COMMAND_NOT_BUILTIN, BUILTIN_REGULAR, BUILTIN_FUNCTION };

/// Error message on missing argument.
#define BUILTIN_ERR_MISSING _(L"%ls: Expected argument for option %ls\n")

/// Error message on invalid combination of options.
#define BUILTIN_ERR_COMBO _(L"%ls: Invalid combination of options\n")

/// Error message on invalid combination of options.
#define BUILTIN_ERR_COMBO2 _(L"%ls: Invalid combination of options,\n%ls\n")

/// Error message on multiple scope levels for variables.
#define BUILTIN_ERR_GLOCAL \
    _(L"%ls: Variable scope can only be one of universal, global and local\n")

/// Error message for specifying both export and unexport to set/read.
#define BUILTIN_ERR_EXPUNEXP _(L"%ls: Variable can't be both exported and unexported\n")

/// Error message for unknown switch.
#define BUILTIN_ERR_UNKNOWN _(L"%ls: Unknown option '%ls'\n")

/// Error message for unexpected args.
#define BUILTIN_ERR_ARG_COUNT0 _(L"%ls: Expected an argument\n")
#define BUILTIN_ERR_ARG_COUNT1 _(L"%ls: Expected %d args, got %d\n")
#define BUILTIN_ERR_ARG_COUNT2 _(L"%ls %ls: Expected %d args, got %d\n")
#define BUILTIN_ERR_MIN_ARG_COUNT1 _(L"%ls: Expected at least %d args, got %d\n")
#define BUILTIN_ERR_MAX_ARG_COUNT1 _(L"%ls: Expected at most %d args, got %d\n")

/// Error message for invalid variable name.
#define BUILTIN_ERR_VARNAME _(L"%ls: Variable name '%ls' is not valid. See `help identifiers`.\n")

/// Error message for invalid bind mode name.
#define BUILTIN_ERR_BIND_MODE _(L"%ls: mode name '%ls' is not valid. See `help identifiers`.\n")

/// Error message when too many arguments are supplied to a builtin.
#define BUILTIN_ERR_TOO_MANY_ARGUMENTS _(L"%ls: Too many arguments\n")

/// Error message when integer expected
#define BUILTIN_ERR_NOT_NUMBER _(L"%ls: Argument '%ls' is not a valid integer\n")

/// Command that requires a subcommand was invoked without a recognized subcommand.
#define BUILTIN_ERR_MISSING_SUBCMD _(L"%ls: Expected a subcommand to follow the command\n")
#define BUILTIN_ERR_INVALID_SUBCMD _(L"%ls: Subcommand '%ls' is not valid\n")

/// The send stuff to foreground message.
#define FG_MSG _(L"Send job %d, '%ls' to foreground\n")

void builtin_init();
bool builtin_exists(const wcstring &cmd);

proc_status_t builtin_run(parser_t &parser, wchar_t **argv, io_streams_t &streams);

wcstring_list_t builtin_get_names();
void builtin_get_names(completion_list_t *list);
const wchar_t *builtin_get_desc(const wcstring &name);

wcstring builtin_help_get(parser_t &parser, const wchar_t *cmd);

void builtin_print_help(parser_t &parser, const io_streams_t &streams, const wchar_t *name,
                        wcstring *error_message = nullptr);
int builtin_count_args(const wchar_t *const *argv);

void builtin_unknown_option(parser_t &parser, io_streams_t &streams, const wchar_t *cmd,
                            const wchar_t *opt);

void builtin_missing_argument(parser_t &parser, io_streams_t &streams, const wchar_t *cmd,
                              const wchar_t *opt, bool print_hints = true);

void builtin_print_error_trailer(parser_t &parser, output_stream_t &b, const wchar_t *cmd);

void builtin_wperror(const wchar_t *s, io_streams_t &streams);

struct help_only_cmd_opts_t {
    bool print_help = false;
};
int parse_help_only_cmd_opts(help_only_cmd_opts_t &opts, int *optind, int argc, wchar_t **argv,
                             parser_t &parser, io_streams_t &streams);
#endif
