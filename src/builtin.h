// Prototypes for functions for executing builtin functions.
#ifndef FISH_BUILTIN_H
#define FISH_BUILTIN_H

#include <stddef.h>
#include <vector>

#include "common.h"

class completion_t;
class parser_t;
class output_stream_t;
struct io_streams_t;

/// Data structure to describe a builtin.
struct builtin_data_t {
    // Name of the builtin.
    const wchar_t *name;
    // Function pointer tothe builtin implementation.
    int (*func)(parser_t &parser, io_streams_t &streams, wchar_t **argv);
    // Description of what the builtin does.
    const wchar_t *desc;

    bool operator<(const wcstring &) const;
    bool operator<(const builtin_data_t *) const;
};

/// The default prompt for the read command.
#define DEFAULT_READ_PROMPT L"set_color green; echo -n read; set_color normal; echo -n \"> \""

/// The mode name to pass to history and input.
#define READ_MODE_NAME L"fish_read"

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
#define BUILTIN_ERR_ARG_COUNT1 _(L"%ls: expected %d args, got %d\n")
#define BUILTIN_ERR_ARG_COUNT2 _(L"%ls: %ls expected %d args, got %d\n")

/// Error message for invalid character in variable name.
#define BUILTIN_ERR_VARCHAR                                                                 \
    _(L"%ls: Invalid character '%lc' in variable name. Only alphanumerical characters and " \
      L"underscores are valid in a variable name.\n")

/// Error message for invalid (empty) variable name.
#define BUILTIN_ERR_VARNAME_ZERO _(L"%ls: Variable name can not be the empty string\n")

/// Error message when too many arguments are supplied to a builtin.
#define BUILTIN_ERR_TOO_MANY_ARGUMENTS _(L"%ls: Too many arguments\n")

/// Error message when number expected
#define BUILTIN_ERR_NOT_NUMBER _(L"%ls: Argument '%ls' is not a number\n")

/// The send stuff to foreground message.
#define FG_MSG _(L"Send job %d, '%ls' to foreground\n")

void builtin_init();
void builtin_destroy();
bool builtin_exists(const wcstring &cmd);

int builtin_run(parser_t &parser, const wchar_t *const *argv, io_streams_t &streams);

wcstring_list_t builtin_get_names();
void builtin_get_names(std::vector<completion_t> *list);
wcstring builtin_get_desc(const wcstring &b);

/// Support for setting and removing transient command lines. This is used by
/// 'complete -C' in order to make the commandline builtin operate on the string
/// to complete instead of operating on whatever is to be completed. It's also
/// used by completion wrappers, to allow a command to appear as the command
/// being wrapped for the purposes of completion.
///
/// Instantiating an instance of builtin_commandline_scoped_transient_t pushes
/// the command as the new transient commandline. The destructor removes it. It
/// will assert if construction/destruction does not happen in a stack-like
/// (LIFO) order.
class builtin_commandline_scoped_transient_t {
    size_t token;

   public:
    explicit builtin_commandline_scoped_transient_t(const wcstring &cmd);
    ~builtin_commandline_scoped_transient_t();
};

wcstring builtin_help_get(parser_t &parser, const wchar_t *cmd);

int builtin_function(parser_t &parser, io_streams_t &streams, const wcstring_list_t &c_args,
                     const wcstring &contents, int definition_line_offset, wcstring *out_err);

void builtin_print_help(parser_t &parser, io_streams_t &streams, const wchar_t *cmd,
                        output_stream_t &b);
int builtin_count_args(const wchar_t *const *argv);
bool builtin_is_valid_varname(const wchar_t *varname, wcstring &errstr, const wchar_t *cmd);

void builtin_unknown_option(parser_t &parser, io_streams_t &streams, const wchar_t *cmd,
                            const wchar_t *opt);

void builtin_missing_argument(parser_t &parser, io_streams_t &streams, const wchar_t *cmd,
                              const wchar_t *opt);

void builtin_wperror(const wchar_t *s, io_streams_t &streams);
#endif
