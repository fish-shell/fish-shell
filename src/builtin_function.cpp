// Implementation of the function builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include "builtin.h"
#include "builtin_function.h"
#include "common.h"
#include "complete.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "function.h"
#include "io.h"
#include "parser.h"
#include "parser_keywords.h"
#include "proc.h"
#include "signal.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

struct function_cmd_opts_t {
    bool print_help = false;
    bool shadow_scope = true;
    wcstring description = L"";
    std::vector<event_t> events;
    wcstring_list_t named_arguments;
    wcstring_list_t inherit_vars;
    wcstring_list_t wrap_targets;
};

// This command is atypical in using the "+" (REQUIRE_ORDER) option for flag parsing.
// This is needed due to the semantics of the -a/--argument-names flag.
static const wchar_t *const short_options = L"+:a:d:e:hj:p:s:v:w:SV:";
static const struct woption long_options[] = {{L"description", required_argument, NULL, 'd'},
                                              {L"on-signal", required_argument, NULL, 's'},
                                              {L"on-job-exit", required_argument, NULL, 'j'},
                                              {L"on-process-exit", required_argument, NULL, 'p'},
                                              {L"on-variable", required_argument, NULL, 'v'},
                                              {L"on-event", required_argument, NULL, 'e'},
                                              {L"wraps", required_argument, NULL, 'w'},
                                              {L"help", no_argument, NULL, 'h'},
                                              {L"argument-names", required_argument, NULL, 'a'},
                                              {L"no-scope-shadowing", no_argument, NULL, 'S'},
                                              {L"inherit-variable", required_argument, NULL, 'V'},
                                              {NULL, 0, NULL, 0}};

static int parse_cmd_opts(function_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    const wchar_t *cmd = L"function";
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
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
                opts.events.push_back(event_t::signal_event(sig));
                break;
            }
            case 'v': {
                if (!valid_var_name(w.woptarg)) {
                    streams.err.append_format(BUILTIN_ERR_VARNAME, cmd, w.woptarg);
                    return STATUS_INVALID_ARGS;
                }

                opts.events.push_back(event_t::variable_event(w.woptarg));
                break;
            }
            case 'e': {
                opts.events.push_back(event_t::generic_event(w.woptarg));
                break;
            }
            case 'j':
            case 'p': {
                pid_t pid;
                event_t e(EVENT_ANY);

                if ((opt == 'j') && (wcscasecmp(w.woptarg, L"caller") == 0)) {
                    job_id_t job_id = -1;

                    if (is_subshell) {
                        size_t block_idx = 0;

                        // Find the outermost substitution block.
                        for (block_idx = 0;; block_idx++) {
                            const block_t *b = parser.block_at_index(block_idx);
                            if (b == NULL || b->type() == SUBST) break;
                        }

                        // Go one step beyond that, to get to the caller.
                        const block_t *caller_block = parser.block_at_index(block_idx + 1);
                        if (caller_block != NULL && caller_block->job != NULL) {
                            job_id = caller_block->job->job_id;
                        }
                    }

                    if (job_id == -1) {
                        streams.err.append_format(
                            _(L"%ls: Cannot find calling job for event handler"), cmd);
                        return STATUS_INVALID_ARGS;
                    }
                    e.type = EVENT_JOB_ID;
                    e.param1.job_id = job_id;
                } else if ((opt == 'p') && (wcscasecmp(w.woptarg, L"%self") == 0)) {
                    pid = getpid();
                    e.type = EVENT_EXIT;
                    e.param1.pid = pid;
                } else {
                    pid = fish_wcstoi(w.woptarg);
                    if (errno || pid < 0) {
                        streams.err.append_format(_(L"%ls: Invalid process id '%ls'"), cmd,
                                                  w.woptarg);
                        return STATUS_INVALID_ARGS;
                    }

                    e.type = EVENT_EXIT;
                    e.param1.pid = (opt == 'j' ? -1 : 1) * abs(pid);
                }
                opts.events.push_back(e);
                break;
            }
            case 'a': {
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
                break;
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
        streams.err.append_format(_(L"%ls: Expected function name"), cmd);
        return STATUS_INVALID_ARGS;
    }

    function_name = argv[1];
    if (!valid_func_name(function_name)) {
        streams.err.append_format(_(L"%ls: Illegal function name '%ls'"), cmd,
                                  function_name.c_str());
        return STATUS_INVALID_ARGS;
    }

    if (parser_keywords_is_reserved(function_name)) {
        streams.err.append_format(
            _(L"%ls: The name '%ls' is reserved,\nand can not be used as a function name"), cmd,
            function_name.c_str());
        return STATUS_INVALID_ARGS;
    }

    return STATUS_CMD_OK;
}

/// Define a function. Calls into `function.cpp` to perform the heavy lifting of defining a
/// function.
int builtin_function(parser_t &parser, io_streams_t &streams, const wcstring_list_t &c_args,
                     const parsed_source_ref_t &source, tnode_t<grammar::job_list> body) {
    assert(source && "Missing source in builtin_function");
    // The wgetopt function expects 'function' as the first argument. Make a new wcstring_list with
    // that property. This is needed because this builtin has a different signature than the other
    // builtins.
    wcstring_list_t args = {L"function"};
    args.insert(args.end(), c_args.begin(), c_args.end());

    null_terminated_array_t<wchar_t> argv_array(args);
    wchar_t **argv = argv_array.get();
    wchar_t *cmd = argv[0];
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
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_CMD_OK;
    }

    if (argc != optind) {
        if (opts.named_arguments.size()) {
            for (int i = optind; i < argc; i++) {
                opts.named_arguments.push_back(argv[i]);
            }
        } else {
            streams.err.append_format(_(L"%ls: Unexpected positional argument '%ls'"), cmd,
                                      argv[optind]);
            return STATUS_INVALID_ARGS;
        }
    }

    // We have what we need to actually define the function.
    function_data_t d;
    d.name = function_name;
    if (!opts.description.empty()) d.description = opts.description;
    // d.description = opts.description;
    d.events.swap(opts.events);
    d.props.shadow_scope = opts.shadow_scope;
    d.props.named_arguments = std::move(opts.named_arguments);
    d.inherit_vars = std::move(opts.inherit_vars);

    for (size_t i = 0; i < d.events.size(); i++) {
        event_t &e = d.events.at(i);
        e.function_name = d.name;
    }

    d.props.parsed_source = source;
    d.props.body_node = body;
    function_add(std::move(d), parser);

    // Handle wrap targets by creating the appropriate completions.
    for (const wcstring &wt : opts.wrap_targets) complete_add_wrapper(function_name, wt);
    return STATUS_CMD_OK;
}
