// Implementation of the status builtin.
#include "config.h"  // IWYU pragma: keep

#include <stddef.h>
#include <wchar.h>

#include <string>

#include "builtin.h"
#include "builtin_status.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "parser.h"
#include "proc.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

enum status_cmd_t {
    STATUS_IS_LOGIN = 1,
    STATUS_IS_INTERACTIVE,
    STATUS_IS_BLOCK,
    STATUS_IS_COMMAND_SUB,
    STATUS_IS_FULL_JOB_CTRL,
    STATUS_IS_INTERACTIVE_JOB_CTRL,
    STATUS_IS_NO_JOB_CTRL,
    STATUS_CURRENT_FILENAME,
    STATUS_CURRENT_FUNCTION,
    STATUS_CURRENT_LINE_NUMBER,
    STATUS_SET_JOB_CONTROL,
    STATUS_PRINT_STACK_TRACE,
    STATUS_UNDEF
};

// Must be sorted by string, not enum or random.
const enum_map<status_cmd_t> status_enum_map[] = {
    {STATUS_CURRENT_FILENAME, L"current-filename"},
    {STATUS_CURRENT_FUNCTION, L"current-function"},
    {STATUS_CURRENT_LINE_NUMBER, L"current-line-number"},
    {STATUS_IS_BLOCK, L"is-block"},
    {STATUS_IS_COMMAND_SUB, L"is-command-substitution"},
    {STATUS_IS_FULL_JOB_CTRL, L"is-full-job-control"},
    {STATUS_IS_INTERACTIVE, L"is-interactive"},
    {STATUS_IS_INTERACTIVE_JOB_CTRL, L"is-interactive-job-control"},
    {STATUS_IS_LOGIN, L"is-login"},
    {STATUS_IS_NO_JOB_CTRL, L"is-no-job-control"},
    {STATUS_SET_JOB_CONTROL, L"job-control"},
    {STATUS_PRINT_STACK_TRACE, L"print-stack-trace"},
    {STATUS_UNDEF, NULL}};
#define status_enum_map_len (sizeof status_enum_map / sizeof *status_enum_map)

#define CHECK_FOR_UNEXPECTED_STATUS_ARGS(status_cmd)                                        \
    if (args.size() != 0) {                                                                 \
        const wchar_t *subcmd_str = enum_to_str(status_cmd, status_enum_map);               \
        if (!subcmd_str) subcmd_str = L"default";                                           \
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT2, cmd, subcmd_str, 0, args.size()); \
        retval = STATUS_INVALID_ARGS;                                                       \
        break;                                                                              \
    }

int job_control_str_to_mode(const wchar_t *mode, wchar_t *cmd, io_streams_t &streams) {
    if (wcscmp(mode, L"full") == 0) {
        return JOB_CONTROL_ALL;
    } else if (wcscmp(mode, L"interactive") == 0) {
        return JOB_CONTROL_INTERACTIVE;
    } else if (wcscmp(mode, L"none") == 0) {
        return JOB_CONTROL_NONE;
    }
    streams.err.append_format(L"%ls: Invalid job control mode '%ls'\n", cmd, mode);
    return -1;
}

struct status_cmd_opts_t {
    bool print_help = false;
    status_cmd_t status_cmd = STATUS_UNDEF;
    int new_job_control_mode = -1;
};

/// Note: Do not add new flags that represent subcommands. We're encouraging people to switch to
/// the non-flag subcommand form. While these flags are deprecated they must be supported at
/// least until fish 3.0 and possibly longer to avoid breaking everyones config.fish and other
/// scripts.
static const wchar_t *short_options = L":cbilfnhj:t";
static const struct woption long_options[] = {{L"help", no_argument, NULL, 'h'},
                                              {L"is-command-substitution", no_argument, NULL, 'c'},
                                              {L"is-block", no_argument, NULL, 'b'},
                                              {L"is-interactive", no_argument, NULL, 'i'},
                                              {L"is-login", no_argument, NULL, 'l'},
                                              {L"is-full-job-control", no_argument, NULL, 1},
                                              {L"is-interactive-job-control", no_argument, NULL, 2},
                                              {L"is-no-job-control", no_argument, NULL, 3},
                                              {L"current-filename", no_argument, NULL, 'f'},
                                              {L"current-line-number", no_argument, NULL, 'n'},
                                              {L"job-control", required_argument, NULL, 'j'},
                                              {L"print-stack-trace", no_argument, NULL, 't'},
                                              {NULL, 0, NULL, 0}};

/// Remember the status subcommand and disallow selecting more than one status subcommand.
static bool set_status_cmd(wchar_t *const cmd, status_cmd_opts_t &opts, status_cmd_t sub_cmd,
                           io_streams_t &streams) {
    if (opts.status_cmd != STATUS_UNDEF) {
        wchar_t err_text[1024];
        const wchar_t *subcmd_str1 = enum_to_str(opts.status_cmd, status_enum_map);
        const wchar_t *subcmd_str2 = enum_to_str(sub_cmd, status_enum_map);
        swprintf(err_text, sizeof(err_text) / sizeof(wchar_t),
                 _(L"you cannot do both '%ls' and '%ls' in the same invocation"), subcmd_str1,
                 subcmd_str2);
        streams.err.append_format(BUILTIN_ERR_COMBO2, cmd, err_text);
        return false;
    }

    opts.status_cmd = sub_cmd;
    return true;
}

static int parse_cmd_opts(status_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 1: {
                if (!set_status_cmd(cmd, opts, STATUS_IS_FULL_JOB_CTRL, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 2: {
                if (!set_status_cmd(cmd, opts, STATUS_IS_INTERACTIVE_JOB_CTRL, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 3: {
                if (!set_status_cmd(cmd, opts, STATUS_IS_NO_JOB_CTRL, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 'c': {
                if (!set_status_cmd(cmd, opts, STATUS_IS_COMMAND_SUB, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 'b': {
                if (!set_status_cmd(cmd, opts, STATUS_IS_BLOCK, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 'i': {
                if (!set_status_cmd(cmd, opts, STATUS_IS_INTERACTIVE, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 'l': {
                if (!set_status_cmd(cmd, opts, STATUS_IS_LOGIN, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 'f': {
                if (!set_status_cmd(cmd, opts, STATUS_CURRENT_FILENAME, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 'n': {
                if (!set_status_cmd(cmd, opts, STATUS_CURRENT_LINE_NUMBER, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 'j': {
                if (!set_status_cmd(cmd, opts, STATUS_SET_JOB_CONTROL, streams)) {
                    return STATUS_CMD_ERROR;
                }
                opts.new_job_control_mode = job_control_str_to_mode(w.woptarg, cmd, streams);
                if (opts.new_job_control_mode == -1) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 't': {
                if (!set_status_cmd(cmd, opts, STATUS_PRINT_STACK_TRACE, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
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

/// The status builtin. Gives various status information on fish.
int builtin_status(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    status_cmd_opts_t opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    // If a status command hasn't already been specified via a flag check the first word.
    // Note that this can be simplified after we eliminate allowing subcommands as flags.
    if (optind < argc) {
        status_cmd_t subcmd = str_to_enum(argv[optind], status_enum_map, status_enum_map_len);
        if (subcmd != STATUS_UNDEF) {
            if (!set_status_cmd(cmd, opts, subcmd, streams)) {
                return STATUS_CMD_ERROR;
            }
            optind++;
        }
    }

    // Every argument that we haven't consumed already is an argument for a subcommand.
    const wcstring_list_t args(argv + optind, argv + argc);

    switch (opts.status_cmd) {
        case STATUS_UNDEF: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            if (is_login) {
                streams.out.append_format(_(L"This is a login shell\n"));
            } else {
                streams.out.append_format(_(L"This is not a login shell\n"));
            }

            streams.out.append_format(
                _(L"Job control: %ls\n"),
                job_control_mode == JOB_CONTROL_INTERACTIVE
                    ? _(L"Only on interactive jobs")
                    : (job_control_mode == JOB_CONTROL_NONE ? _(L"Never") : _(L"Always")));
            streams.out.append(parser.stack_trace());
            break;
        }
        case STATUS_SET_JOB_CONTROL: {
            if (opts.new_job_control_mode != -1) {
                // Flag form was used.
                CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            } else {
                if (args.size() != 1) {
                    const wchar_t *subcmd_str = enum_to_str(opts.status_cmd, status_enum_map);
                    streams.err.append_format(BUILTIN_ERR_ARG_COUNT2, cmd, subcmd_str, 1,
                                              args.size());
                    return STATUS_INVALID_ARGS;
                }
                opts.new_job_control_mode = job_control_str_to_mode(args[0].c_str(), cmd, streams);
                if (opts.new_job_control_mode == -1) {
                    return STATUS_CMD_ERROR;
                }
            }
            job_control_mode = opts.new_job_control_mode;
            break;
        }
        case STATUS_CURRENT_FILENAME: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            const wchar_t *fn = parser.current_filename();

            if (!fn) fn = _(L"Standard input");
            streams.out.append_format(L"%ls\n", fn);
            break;
        }
        case STATUS_CURRENT_FUNCTION: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            const wchar_t *fn = parser.get_function_name();

            if (!fn) fn = _(L"Not a function");
            streams.out.append_format(L"%ls\n", fn);
            break;
        }
        case STATUS_CURRENT_LINE_NUMBER: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            streams.out.append_format(L"%d\n", parser.get_lineno());
            break;
        }
        case STATUS_IS_INTERACTIVE: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            retval = !is_interactive_session;
            break;
        }
        case STATUS_IS_COMMAND_SUB: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            retval = !is_subshell;
            break;
        }
        case STATUS_IS_BLOCK: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            retval = !is_block;
            break;
        }
        case STATUS_IS_LOGIN: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            retval = !is_login;
            break;
        }
        case STATUS_IS_FULL_JOB_CTRL: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            retval = job_control_mode != JOB_CONTROL_ALL;
            break;
        }
        case STATUS_IS_INTERACTIVE_JOB_CTRL: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            retval = job_control_mode != JOB_CONTROL_INTERACTIVE;
            break;
        }
        case STATUS_IS_NO_JOB_CTRL: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            retval = job_control_mode != JOB_CONTROL_NONE;
            break;
        }
        case STATUS_PRINT_STACK_TRACE: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            streams.out.append(parser.stack_trace());
            break;
        }
    }

    return retval;
}
