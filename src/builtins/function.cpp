// Implementation of the function builtin.
#include "config.h"  // IWYU pragma: keep

#include "function.h"

#include <unistd.h>

#include <cerrno>
#include <cwchar>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../builtin.h"
#include "../common.h"
#include "../complete.h"
#include "../env.h"
#include "../event.h"
#include "../fallback.h"  // IWYU pragma: keep
#include "../function.h"
#include "../io.h"
#include "../maybe.h"
#include "../null_terminated_array.h"
#include "../parser.h"
#include "../parser_keywords.h"
#include "../parse_tree.h"
#include "../proc.h"
#include "../signal.h"
#include "../wait_handle.h"
#include "../wgetopt.h"
#include "../wutil.h"  // IWYU pragma: keep

namespace {
struct function_cmd_opts_t {
    bool print_help = false;
    bool shadow_scope = true;
    wcstring description;
    std::vector<event_description_t> events;
    wcstring_list_t named_arguments;
    wcstring_list_t inherit_vars;
    wcstring_list_t wrap_targets;
};
}  // namespace

// This command is atypical in using the "-" (RETURN_IN_ORDER) option for flag parsing.
// This is needed due to the semantics of the -a/--argument-names flag.
static const wchar_t *const short_options = L"-:a:d:e:hj:p:s:v:w:SV:";
static const struct woption long_options[] = {
    {L"description", required_argument, nullptr, 'd'},
    {L"on-signal", required_argument, nullptr, 's'},
    {L"on-job-exit", required_argument, nullptr, 'j'},
    {L"on-process-exit", required_argument, nullptr, 'p'},
    {L"on-variable", required_argument, nullptr, 'v'},
    {L"on-event", required_argument, nullptr, 'e'},
    {L"wraps", required_argument, nullptr, 'w'},
    {L"help", no_argument, nullptr, 'h'},
    {L"argument-names", required_argument, nullptr, 'a'},
    {L"no-scope-shadowing", no_argument, nullptr, 'S'},
    {L"inherit-variable", required_argument, nullptr, 'V'},
    {}};

/// \return the internal_job_id for a pid, or 0 if none.
/// This looks through both active and finished jobs.
static internal_job_id_t job_id_for_pid(pid_t pid, parser_t &parser) {
    if (const auto *job = parser.job_get_from_pid(pid)) {
        return job->internal_job_id;
    }
    if (wait_handle_ref_t wh = parser.get_wait_handles().get_by_pid(pid)) {
        return wh->internal_job_id;
    }
    return 0;
}

static int parse_cmd_opts(function_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, const wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    const wchar_t *cmd = L"function";
    int opt;
    wgetopter_t w;
    bool handling_named_arguments = false;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        if (opt != 'a' && opt != 1) handling_named_arguments = false;
        switch (opt) {
            case 1: {
                if (handling_named_arguments) {
                    opts.named_arguments.push_back(w.woptarg);
                    break;
                } else {
                    streams.err.append_format(_(L"%ls: %ls: unexpected positional argument"), cmd,
                                              w.woptarg);
                    return STATUS_INVALID_ARGS;
                }
            }
            case 'd': {
                opts.description = w.woptarg;
                break;
            }
            case 's': {
                int sig = wcs2sig(w.woptarg);
                if (sig == -1) {
                    streams.err.append_format(_(L"%ls: Unknown signal '%ls'"), cmd, w.woptarg);
                    return STATUS_INVALID_ARGS;
                }
                opts.events.push_back(event_description_t::signal(sig));
                break;
            }
            case 'v': {
                if (!valid_var_name(w.woptarg)) {
                    streams.err.append_format(BUILTIN_ERR_VARNAME, cmd, w.woptarg);
                    return STATUS_INVALID_ARGS;
                }

                opts.events.push_back(event_description_t::variable(w.woptarg));
                break;
            }
            case 'e': {
                opts.events.push_back(event_description_t::generic(w.woptarg));
                break;
            }
            case 'j':
            case 'p': {
                event_description_t e(event_type_t::any);

                if ((opt == 'j') && (wcscasecmp(w.woptarg, L"caller") == 0)) {
                    internal_job_id_t caller_id =
                        parser.libdata().is_subshell ? parser.libdata().caller_id : 0;
                    if (caller_id == 0) {
                        streams.err.append_format(
                            _(L"%ls: calling job for event handler not found"), cmd);
                        return STATUS_INVALID_ARGS;
                    }
                    e.type = event_type_t::caller_exit;
                    e.param1.caller_id = caller_id;
                } else if ((opt == 'p') && (wcscasecmp(w.woptarg, L"%self") == 0)) {
                    e.type = event_type_t::process_exit;
                    e.param1.pid = getpid();
                } else {
                    pid_t pid = fish_wcstoi(w.woptarg);
                    if (errno || pid < 0) {
                        streams.err.append_format(_(L"%ls: %ls: invalid process id"), cmd,
                                                  w.woptarg);
                        return STATUS_INVALID_ARGS;
                    }
                    if (opt == 'p') {
                        e.type = event_type_t::process_exit;
                        e.param1.pid = pid;
                    } else {
                        e.type = event_type_t::job_exit;
                        e.param1.jobspec = {pid, job_id_for_pid(pid, parser)};
                    }
                }
                opts.events.push_back(e);
                break;
            }
            case 'a': {
                handling_named_arguments = true;
                opts.named_arguments.push_back(w.woptarg);
                break;
            }
            case 'S': {
                opts.shadow_scope = false;
                break;
            }
            case 'w': {
                opts.wrap_targets.push_back(w.woptarg);
                break;
            }
            case 'V': {
                if (!valid_var_name(w.woptarg)) {
                    streams.err.append_format(BUILTIN_ERR_VARNAME, cmd, w.woptarg);
                    return STATUS_INVALID_ARGS;
                }
                opts.inherit_vars.push_back(w.woptarg);
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

static int validate_function_name(int argc, const wchar_t *const *argv, wcstring &function_name,
                                  const wchar_t *cmd, io_streams_t &streams) {
    if (argc < 2) {
        // This is currently impossible but let's be paranoid.
        streams.err.append_format(_(L"%ls: function name required"), cmd);
        return STATUS_INVALID_ARGS;
    }

    function_name = argv[1];
    if (!valid_func_name(function_name)) {
        streams.err.append_format(_(L"%ls: %ls: invalid function name"), cmd,
                                  function_name.c_str());
        return STATUS_INVALID_ARGS;
    }

    if (parser_keywords_is_reserved(function_name)) {
        streams.err.append_format(_(L"%ls: %ls: cannot use reserved keyword as function name"), cmd,
                                  function_name.c_str());
        return STATUS_INVALID_ARGS;
    }

    return STATUS_CMD_OK;
}

/// Define a function. Calls into `function.cpp` to perform the heavy lifting of defining a
/// function.
maybe_t<int> builtin_function(parser_t &parser, io_streams_t &streams,
                              const wcstring_list_t &c_args, const parsed_source_ref_t &source,
                              const ast::block_statement_t &func_node) {
    assert(source && "Missing source in builtin_function");
    // The wgetopt function expects 'function' as the first argument. Make a new wcstring_list with
    // that property. This is needed because this builtin has a different signature than the other
    // builtins.
    wcstring_list_t args = {L"function"};
    args.insert(args.end(), c_args.begin(), c_args.end());

    null_terminated_array_t<wchar_t> argv_array(args);
    const wchar_t **argv = argv_array.get();
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);

    // A valid function name has to be the first argument.
    wcstring function_name;
    int retval = validate_function_name(argc, argv, function_name, cmd, streams);
    if (retval != STATUS_CMD_OK) return retval;
    argv++;
    argc--;

    function_cmd_opts_t opts;
    int optind;
    retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_CMD_OK;
    }

    if (argc != optind) {
        if (!opts.named_arguments.empty()) {
            for (int i = optind; i < argc; i++) {
                if (!valid_var_name(argv[i])) {
                    streams.err.append_format(BUILTIN_ERR_VARNAME, cmd, argv[i]);
                    return STATUS_INVALID_ARGS;
                }
                opts.named_arguments.push_back(argv[i]);
            }
        } else {
            streams.err.append_format(_(L"%ls: %ls: unexpected positional argument"), cmd,
                                      argv[optind]);
            return STATUS_INVALID_ARGS;
        }
    }

    // We have what we need to actually define the function.
    auto props = std::make_shared<function_properties_t>();
    props->shadow_scope = opts.shadow_scope;
    props->named_arguments = std::move(opts.named_arguments);
    props->parsed_source = source;
    props->func_node = &func_node;
    props->description = opts.description;
    props->definition_file = parser.libdata().current_filename;

    // Populate inherit_vars.
    for (const wcstring &name : opts.inherit_vars) {
        if (auto var = parser.vars().get(name)) {
            props->inherit_vars[name] = var->as_list();
        }
    }

    // Add the function itself.
    function_add(function_name, props);

    // Handle wrap targets by creating the appropriate completions.
    for (const wcstring &wt : opts.wrap_targets) {
        complete_add_wrapper(function_name, wt);
    }

    // Add any event handlers.
    for (const event_description_t &ed : opts.events) {
        event_add_handler(std::make_shared<event_handler_t>(ed, function_name));
    }

    // If there is an --on-process-exit or --on-job-exit event handler for some pid, and that
    // process has already exited, run it immediately (#7210).
    for (const event_description_t &ed : opts.events) {
        if (ed.type == event_type_t::process_exit) {
            pid_t pid = ed.param1.pid;
            if (pid == EVENT_ANY_PID) continue;
            wait_handle_ref_t wh = parser.get_wait_handles().get_by_pid(pid);
            if (wh && wh->completed) {
                event_fire(parser, event_t::process_exit(pid, wh->status));
            }
        } else if (ed.type == event_type_t::job_exit) {
            pid_t pid = ed.param1.jobspec.pid;
            if (pid == EVENT_ANY_PID) continue;
            wait_handle_ref_t wh = parser.get_wait_handles().get_by_pid(pid);
            if (wh && wh->completed) {
                event_fire(parser, event_t::job_exit(pid, wh->internal_job_id));
            }
        }
    }

    return STATUS_CMD_OK;
}
