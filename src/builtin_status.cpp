// Implementation of the status builtin.
#include "config.h"  // IWYU pragma: keep

#include "builtin_status.h"

#include <cstddef>
#include <cwchar>
#include <string>

#include "builtin.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "future_feature_flags.h"
#include "io.h"
#include "parser.h"
#include "proc.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

enum status_cmd_t {
    STATUS_CURRENT_CMD = 1,
    STATUS_BASENAME,
    STATUS_DIRNAME,
    STATUS_FEATURES,
    STATUS_FILENAME,
    STATUS_FISH_PATH,
    STATUS_FUNCTION,
    STATUS_IS_BLOCK,
    STATUS_IS_BREAKPOINT,
    STATUS_IS_COMMAND_SUB,
    STATUS_IS_FULL_JOB_CTRL,
    STATUS_IS_INTERACTIVE,
    STATUS_IS_INTERACTIVE_JOB_CTRL,
    STATUS_IS_LOGIN,
    STATUS_IS_NO_JOB_CTRL,
    STATUS_LINE_NUMBER,
    STATUS_SET_JOB_CONTROL,
    STATUS_STACK_TRACE,
    STATUS_TEST_FEATURE,
    STATUS_UNDEF
};

// Must be sorted by string, not enum or random.
const enum_map<status_cmd_t> status_enum_map[] = {
    {STATUS_BASENAME, L"basename"},
    {STATUS_BASENAME, L"current-basename"},
    {STATUS_CURRENT_CMD, L"current-command"},
    {STATUS_DIRNAME, L"current-dirname"},
    {STATUS_FILENAME, L"current-filename"},
    {STATUS_FUNCTION, L"current-function"},
    {STATUS_LINE_NUMBER, L"current-line-number"},
    {STATUS_DIRNAME, L"dirname"},
    {STATUS_FEATURES, L"features"},
    {STATUS_FILENAME, L"filename"},
    {STATUS_FISH_PATH, L"fish-path"},
    {STATUS_FUNCTION, L"function"},
    {STATUS_IS_BLOCK, L"is-block"},
    {STATUS_IS_BREAKPOINT, L"is-breakpoint"},
    {STATUS_IS_COMMAND_SUB, L"is-command-substitution"},
    {STATUS_IS_FULL_JOB_CTRL, L"is-full-job-control"},
    {STATUS_IS_INTERACTIVE, L"is-interactive"},
    {STATUS_IS_INTERACTIVE_JOB_CTRL, L"is-interactive-job-control"},
    {STATUS_IS_LOGIN, L"is-login"},
    {STATUS_IS_NO_JOB_CTRL, L"is-no-job-control"},
    {STATUS_SET_JOB_CONTROL, L"job-control"},
    {STATUS_LINE_NUMBER, L"line-number"},
    {STATUS_STACK_TRACE, L"print-stack-trace"},
    {STATUS_STACK_TRACE, L"stack-trace"},
    {STATUS_TEST_FEATURE, L"test-feature"},
    {STATUS_UNDEF, nullptr}};
#define status_enum_map_len (sizeof status_enum_map / sizeof *status_enum_map)

#define CHECK_FOR_UNEXPECTED_STATUS_ARGS(status_cmd)                                        \
    if (!args.empty()) {                                                                    \
        const wchar_t *subcmd_str = enum_to_str(status_cmd, status_enum_map);               \
        if (!subcmd_str) subcmd_str = L"default";                                           \
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT2, cmd, subcmd_str, 0, args.size()); \
        retval = STATUS_INVALID_ARGS;                                                       \
        break;                                                                              \
    }

/// Values that may be returned from the test-feature option to status.
enum { TEST_FEATURE_ON, TEST_FEATURE_OFF, TEST_FEATURE_NOT_RECOGNIZED };

static maybe_t<job_control_t> job_control_str_to_mode(const wchar_t *mode, wchar_t *cmd,
                                                      io_streams_t &streams) {
    if (std::wcscmp(mode, L"full") == 0) {
        return job_control_t::all;
    } else if (std::wcscmp(mode, L"interactive") == 0) {
        return job_control_t::interactive;
    } else if (std::wcscmp(mode, L"none") == 0) {
        return job_control_t::none;
    }
    streams.err.append_format(L"%ls: Invalid job control mode '%ls'\n", cmd, mode);
    return none();
}

struct status_cmd_opts_t {
    int level{1};
    maybe_t<job_control_t> new_job_control_mode{};
    const wchar_t *feature_name{};
    status_cmd_t status_cmd{STATUS_UNDEF};
    bool print_help{false};
};

/// Note: Do not add new flags that represent subcommands. We're encouraging people to switch to
/// the non-flag subcommand form. While these flags are deprecated they must be supported at
/// least until fish 3.0 and possibly longer to avoid breaking everyones config.fish and other
/// scripts.
static const wchar_t *const short_options = L":L:cbilfnhj:t";
static const struct woption long_options[] = {
    {L"help", no_argument, nullptr, 'h'},
    {L"current-filename", no_argument, nullptr, 'f'},
    {L"current-line-number", no_argument, nullptr, 'n'},
    {L"filename", no_argument, nullptr, 'f'},
    {L"fish-path", no_argument, nullptr, STATUS_FISH_PATH},
    {L"is-block", no_argument, nullptr, 'b'},
    {L"is-command-substitution", no_argument, nullptr, 'c'},
    {L"is-full-job-control", no_argument, nullptr, STATUS_IS_FULL_JOB_CTRL},
    {L"is-interactive", no_argument, nullptr, 'i'},
    {L"is-interactive-job-control", no_argument, nullptr, STATUS_IS_INTERACTIVE_JOB_CTRL},
    {L"is-login", no_argument, nullptr, 'l'},
    {L"is-no-job-control", no_argument, nullptr, STATUS_IS_NO_JOB_CTRL},
    {L"job-control", required_argument, nullptr, 'j'},
    {L"level", required_argument, nullptr, 'L'},
    {L"line", no_argument, nullptr, 'n'},
    {L"line-number", no_argument, nullptr, 'n'},
    {L"print-stack-trace", no_argument, nullptr, 't'},
    {nullptr, 0, nullptr, 0}};

/// Remember the status subcommand and disallow selecting more than one status subcommand.
static bool set_status_cmd(wchar_t *const cmd, status_cmd_opts_t &opts, status_cmd_t sub_cmd,
                           io_streams_t &streams) {
    if (opts.status_cmd != STATUS_UNDEF) {
        wchar_t err_text[1024];
        const wchar_t *subcmd_str1 = enum_to_str(opts.status_cmd, status_enum_map);
        const wchar_t *subcmd_str2 = enum_to_str(sub_cmd, status_enum_map);
        std::swprintf(err_text, sizeof(err_text) / sizeof(wchar_t),
                      _(L"you cannot do both '%ls' and '%ls' in the same invocation"), subcmd_str1,
                      subcmd_str2);
        streams.err.append_format(BUILTIN_ERR_COMBO2, cmd, err_text);
        return false;
    }

    opts.status_cmd = sub_cmd;
    return true;
}

/// Print the features and their values.
static void print_features(io_streams_t &streams) {
    for (const auto &md : features_t::metadata) {
        int set = feature_test(md.flag);
        streams.out.append_format(L"%ls\t%s\t%ls\t%ls\n", md.name, set ? "on" : "off", md.groups,
                                  md.description);
    }
}

static int parse_cmd_opts(status_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case STATUS_IS_FULL_JOB_CTRL: {
                if (!set_status_cmd(cmd, opts, STATUS_IS_FULL_JOB_CTRL, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case STATUS_IS_INTERACTIVE_JOB_CTRL: {
                if (!set_status_cmd(cmd, opts, STATUS_IS_INTERACTIVE_JOB_CTRL, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case STATUS_IS_NO_JOB_CTRL: {
                if (!set_status_cmd(cmd, opts, STATUS_IS_NO_JOB_CTRL, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case STATUS_FISH_PATH: {
                if (!set_status_cmd(cmd, opts, STATUS_FISH_PATH, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 'L': {
                opts.level = fish_wcstoi(w.woptarg);
                if (opts.level < 0 || errno == ERANGE) {
                    streams.err.append_format(_(L"%ls: Invalid level value '%ls'\n"), argv[0],
                                              w.woptarg);
                    return STATUS_INVALID_ARGS;
                } else if (errno) {
                    streams.err.append_format(BUILTIN_ERR_NOT_NUMBER, argv[0], w.woptarg);
                    return STATUS_INVALID_ARGS;
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
                if (!set_status_cmd(cmd, opts, STATUS_FILENAME, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 'n': {
                if (!set_status_cmd(cmd, opts, STATUS_LINE_NUMBER, streams)) {
                    return STATUS_CMD_ERROR;
                }
                break;
            }
            case 'j': {
                if (!set_status_cmd(cmd, opts, STATUS_SET_JOB_CONTROL, streams)) {
                    return STATUS_CMD_ERROR;
                }
                auto job_mode = job_control_str_to_mode(w.woptarg, cmd, streams);
                if (!job_mode) {
                    return STATUS_CMD_ERROR;
                }
                opts.new_job_control_mode = job_mode;
                break;
            }
            case 't': {
                if (!set_status_cmd(cmd, opts, STATUS_STACK_TRACE, streams)) {
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
            }
        }
    }

    *optind = w.woptind;
    return STATUS_CMD_OK;
}

/// The status builtin. Gives various status information on fish.
maybe_t<int> builtin_status(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    status_cmd_opts_t opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd);
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
        } else {
            streams.err.append_format(BUILTIN_ERR_INVALID_SUBCMD, cmd, argv[1]);
            return STATUS_INVALID_ARGS;
        }
    }

    // Every argument that we haven't consumed already is an argument for a subcommand.
    const wcstring_list_t args(argv + optind, argv + argc);

    switch (opts.status_cmd) {
        case STATUS_UNDEF: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            if (get_login()) {
                streams.out.append_format(_(L"This is a login shell\n"));
            } else {
                streams.out.append_format(_(L"This is not a login shell\n"));
            }

            auto job_control_mode = get_job_control_mode();
            streams.out.append_format(
                _(L"Job control: %ls\n"),
                job_control_mode == job_control_t::interactive
                    ? _(L"Only on interactive jobs")
                    : (job_control_mode == job_control_t::none ? _(L"Never") : _(L"Always")));
            streams.out.append(parser.stack_trace());
            break;
        }
        case STATUS_SET_JOB_CONTROL: {
            if (opts.new_job_control_mode) {
                // Flag form was used.
                CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            } else {
                if (args.size() != 1) {
                    const wchar_t *subcmd_str = enum_to_str(opts.status_cmd, status_enum_map);
                    streams.err.append_format(BUILTIN_ERR_ARG_COUNT2, cmd, subcmd_str, 1,
                                              args.size());
                    return STATUS_INVALID_ARGS;
                }
                auto new_mode = job_control_str_to_mode(args[0].c_str(), cmd, streams);
                if (!new_mode) {
                    return STATUS_CMD_ERROR;
                }
                opts.new_job_control_mode = new_mode;
            }
            assert(opts.new_job_control_mode && "Should have a new mode");
            set_job_control_mode(*opts.new_job_control_mode);
            break;
        }
        case STATUS_FEATURES: {
            print_features(streams);
            break;
        }
        case STATUS_TEST_FEATURE: {
            if (args.size() != 1) {
                const wchar_t *subcmd_str = enum_to_str(opts.status_cmd, status_enum_map);
                streams.err.append_format(BUILTIN_ERR_ARG_COUNT2, cmd, subcmd_str, 1, args.size());
                return STATUS_INVALID_ARGS;
            }
            auto metadata = features_t::metadata_for(args.front().c_str());
            if (!metadata) {
                retval = TEST_FEATURE_NOT_RECOGNIZED;
            } else {
                retval = feature_test(metadata->flag) ? TEST_FEATURE_ON : TEST_FEATURE_OFF;
            }
            break;
        }
        case STATUS_BASENAME:
        case STATUS_DIRNAME:
        case STATUS_FILENAME: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            auto res = parser.current_filename();
            wcstring fn = res ? res : L"";
            if (!fn.empty() && opts.status_cmd == STATUS_DIRNAME) {
                fn = wdirname(fn);
            } else if (!fn.empty() && opts.status_cmd == STATUS_BASENAME) {
                fn = wbasename(fn);
            } else if (fn.empty()) {
                fn = _(L"Standard input");
            }
            streams.out.append_format(L"%ls\n", fn.c_str());
            break;
        }
        case STATUS_FUNCTION: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            const wchar_t *fn = parser.get_function_name(opts.level);

            if (!fn) fn = _(L"Not a function");
            streams.out.append_format(L"%ls\n", fn);
            break;
        }
        case STATUS_LINE_NUMBER: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            // TBD is how to interpret the level argument when fetching the line number.
            // See issue #4161.
            // streams.out.append_format(L"%d\n", parser.get_lineno(opts.level));
            streams.out.append_format(L"%d\n", parser.get_lineno());
            break;
        }
        case STATUS_IS_INTERACTIVE: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            retval = session_interactivity() == session_interactivity_t::not_interactive ? 1 : 0;
            break;
        }
        case STATUS_IS_COMMAND_SUB: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            retval = parser.libdata().is_subshell ? 0 : 1;
            break;
        }
        case STATUS_IS_BLOCK: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            retval = parser.libdata().is_block ? 0 : 1;
            break;
        }
        case STATUS_IS_BREAKPOINT: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            retval = parser.libdata().is_breakpoint ? 0 : 1;
            break;
        }
        case STATUS_IS_LOGIN: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            retval = !get_login();
            break;
        }
        case STATUS_IS_FULL_JOB_CTRL: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            retval = get_job_control_mode() != job_control_t::all;
            break;
        }
        case STATUS_IS_INTERACTIVE_JOB_CTRL: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            retval = get_job_control_mode() != job_control_t::interactive;
            break;
        }
        case STATUS_IS_NO_JOB_CTRL: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            retval = get_job_control_mode() != job_control_t::none;
            break;
        }
        case STATUS_STACK_TRACE: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            streams.out.append(parser.stack_trace());
            break;
        }
        case STATUS_CURRENT_CMD: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            // HACK: Go via the deprecated variable to get the command.
            const auto var = parser.vars().get(L"_");
            if (!var.missing_or_empty()) {
                streams.out.append(var->as_string());
                streams.out.push_back(L'\n');
            } else {
                streams.out.append(program_name);
                streams.out.push_back(L'\n');
            }
            break;
        }
        case STATUS_FISH_PATH: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(opts.status_cmd)
            streams.out.append(str2wcstring(get_executable_path("fish")));
            streams.out.push_back(L'\n');
            break;
        }
    }

    return retval;
}
