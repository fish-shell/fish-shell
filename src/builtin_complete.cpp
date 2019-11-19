// Functions used for implementing the complete builtin.
#include "config.h"  // IWYU pragma: keep

#include <cstddef>
#include <cwchar>
#include <memory>
#include <string>
#include <vector>

#include "builtin.h"
#include "color.h"
#include "common.h"
#include "complete.h"
#include "env.h"
#include "fallback.h"  // IWYU pragma: keep
#include "highlight.h"
#include "io.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "parser.h"
#include "reader.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

// builtin_complete_* are a set of rather silly looping functions that make sure that all the proper
// combinations of complete_add or complete_remove get called. This is needed since complete allows
// you to specify multiple switches on a single commandline, like 'complete -s a -s b -s c', but the
// complete_add function only accepts one short switch and one long switch.

/// Silly function.
static void builtin_complete_add2(const wchar_t *cmd, bool cmd_is_path, const wchar_t *short_opt,
                                  const wcstring_list_t &gnu_opts, const wcstring_list_t &old_opts,
                                  completion_mode_t result_mode, const wchar_t *condition,
                                  const wchar_t *comp, const wchar_t *desc, int flags) {
    for (const wchar_t *s = short_opt; *s; s++) {
        complete_add(cmd, cmd_is_path, wcstring{*s}, option_type_short, result_mode, condition,
                     comp, desc, flags);
    }

    for (const wcstring &gnu_opt : gnu_opts) {
        complete_add(cmd, cmd_is_path, gnu_opt, option_type_double_long, result_mode, condition,
                     comp, desc, flags);
    }

    for (const wcstring &old_opt : old_opts) {
        complete_add(cmd, cmd_is_path, old_opt, option_type_single_long, result_mode, condition,
                     comp, desc, flags);
    }

    if (old_opts.empty() && gnu_opts.empty() && short_opt[0] == L'\0') {
        complete_add(cmd, cmd_is_path, wcstring(), option_type_args_only, result_mode, condition,
                     comp, desc, flags);
    }
}

/// Silly function.
static void builtin_complete_add(const wcstring_list_t &cmds, const wcstring_list_t &paths,
                                 const wchar_t *short_opt, wcstring_list_t &gnu_opt,
                                 wcstring_list_t &old_opt, completion_mode_t result_mode,
                                 const wchar_t *condition, const wchar_t *comp, const wchar_t *desc,
                                 int flags) {
    for (const wcstring &cmd : cmds) {
        builtin_complete_add2(cmd.c_str(), false /* not path */, short_opt, gnu_opt, old_opt,
                              result_mode, condition, comp, desc, flags);
    }

    for (const wcstring &path : paths) {
        builtin_complete_add2(path.c_str(), true /* is path */, short_opt, gnu_opt, old_opt,
                              result_mode, condition, comp, desc, flags);
    }
}

static void builtin_complete_remove_cmd(const wcstring &cmd, bool cmd_is_path,
                                        const wchar_t *short_opt, const wcstring_list_t &gnu_opt,
                                        const wcstring_list_t &old_opt) {
    bool removed = false;
    for (const wchar_t *s = short_opt; *s; s++) {
        complete_remove(cmd, cmd_is_path, wcstring{*s}, option_type_short);
        removed = true;
    }

    for (const wcstring &opt : old_opt) {
        complete_remove(cmd, cmd_is_path, opt, option_type_single_long);
        removed = true;
    }

    for (const wcstring &opt : gnu_opt) {
        complete_remove(cmd, cmd_is_path, opt, option_type_double_long);
        removed = true;
    }

    if (!removed) {
        // This means that all loops were empty.
        complete_remove_all(cmd, cmd_is_path);
    }
}

static void builtin_complete_remove(const wcstring_list_t &cmds, const wcstring_list_t &paths,
                                    const wchar_t *short_opt, const wcstring_list_t &gnu_opt,
                                    const wcstring_list_t &old_opt) {
    for (const wcstring &cmd : cmds) {
        builtin_complete_remove_cmd(cmd, false /* not path */, short_opt, gnu_opt, old_opt);
    }

    for (const wcstring &path : paths) {
        builtin_complete_remove_cmd(path, true /* is path */, short_opt, gnu_opt, old_opt);
    }
}

/// The complete builtin. Used for specifying programmable tab-completions. Calls the functions in
// complete.cpp for any heavy lifting.
int builtin_complete(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    ASSERT_IS_MAIN_THREAD();

    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    completion_mode_t result_mode{};
    int remove = 0;
    wcstring short_opt;
    wcstring_list_t gnu_opt, old_opt;
    const wchar_t *comp = L"", *desc = L"", *condition = L"";
    bool do_complete = false;
    bool have_do_complete_param = false;
    wcstring do_complete_param;
    wcstring_list_t cmd_to_complete;
    wcstring_list_t path;
    wcstring_list_t wrap_targets;
    bool preserve_order = false;

    static const wchar_t *const short_options = L":a:c:p:s:l:o:d:fFrxeuAn:C::w:hk";
    static const struct woption long_options[] = {{L"exclusive", no_argument, NULL, 'x'},
                                                  {L"no-files", no_argument, NULL, 'f'},
                                                  {L"force-files", no_argument, NULL, 'F'},
                                                  {L"require-parameter", no_argument, NULL, 'r'},
                                                  {L"path", required_argument, NULL, 'p'},
                                                  {L"command", required_argument, NULL, 'c'},
                                                  {L"short-option", required_argument, NULL, 's'},
                                                  {L"long-option", required_argument, NULL, 'l'},
                                                  {L"old-option", required_argument, NULL, 'o'},
                                                  {L"description", required_argument, NULL, 'd'},
                                                  {L"arguments", required_argument, NULL, 'a'},
                                                  {L"erase", no_argument, NULL, 'e'},
                                                  {L"unauthoritative", no_argument, NULL, 'u'},
                                                  {L"authoritative", no_argument, NULL, 'A'},
                                                  {L"condition", required_argument, NULL, 'n'},
                                                  {L"wraps", required_argument, NULL, 'w'},
                                                  {L"do-complete", optional_argument, NULL, 'C'},
                                                  {L"help", no_argument, NULL, 'h'},
                                                  {L"keep-order", no_argument, NULL, 'k'},
                                                  {NULL, 0, NULL, 0}};

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 'x': {
                result_mode.no_files = true;
                result_mode.requires_param = true;
                break;
            }
            case 'f': {
                result_mode.no_files = true;
                break;
            }
            case 'F': {
                result_mode.force_files = true;
            }
            case 'r': {
                result_mode.requires_param = true;
                break;
            }
            case 'k': {
                preserve_order = true;
                break;
            }
            case 'p':
            case 'c': {
                wcstring tmp;
                if (unescape_string(w.woptarg, &tmp, UNESCAPE_SPECIAL)) {
                    if (opt == 'p')
                        path.push_back(tmp);
                    else
                        cmd_to_complete.push_back(tmp);
                } else {
                    streams.err.append_format(_(L"%ls: Invalid token '%ls'\n"), cmd, w.woptarg);
                    return STATUS_INVALID_ARGS;
                }
                break;
            }
            case 'd': {
                desc = w.woptarg;
                break;
            }
            case 'u': {
                // This option was removed in commit 1911298 and is now a no-op.
                break;
            }
            case 'A': {
                // This option was removed in commit 1911298 and is now a no-op.
                break;
            }
            case 's': {
                short_opt.append(w.woptarg);
                if (w.woptarg[0] == '\0') {
                    streams.err.append_format(_(L"%ls: -s requires a non-empty string\n"), cmd);
                    return STATUS_INVALID_ARGS;
                }
                break;
            }
            case 'l': {
                gnu_opt.push_back(w.woptarg);
                if (w.woptarg[0] == '\0') {
                    streams.err.append_format(_(L"%ls: -l requires a non-empty string\n"), cmd);
                    return STATUS_INVALID_ARGS;
                }
                break;
            }
            case 'o': {
                old_opt.push_back(w.woptarg);
                if (w.woptarg[0] == '\0') {
                    streams.err.append_format(_(L"%ls: -o requires a non-empty string\n"), cmd);
                    return STATUS_INVALID_ARGS;
                }
                break;
            }
            case 'a': {
                comp = w.woptarg;
                break;
            }
            case 'e': {
                remove = 1;
                break;
            }
            case 'n': {
                condition = w.woptarg;
                break;
            }
            case 'w': {
                wrap_targets.push_back(w.woptarg);
                break;
            }
            case 'C': {
                do_complete = true;
                have_do_complete_param = w.woptarg != NULL;
                if (have_do_complete_param) do_complete_param = w.woptarg;
                break;
            }
            case 'h': {
                builtin_print_help(parser, streams, cmd);
                return STATUS_CMD_OK;
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

    if (result_mode.no_files && result_mode.force_files) {
        streams.err.append_format(BUILTIN_ERR_COMBO2, L"complete",
                                  L"'--no-files' and '--force-files'");
        return STATUS_INVALID_ARGS;
    }

    if (w.woptind != argc) {
        // Use one left-over arg as the do-complete argument
        // to enable `complete -C "git check"`.
        if (do_complete && !have_do_complete_param && argc == w.woptind + 1) {
            do_complete_param = argv[argc - 1];
            have_do_complete_param = true;
        } else {
            streams.err.append_format(BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd);
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }
    }

    if (condition && std::wcslen(condition)) {
        const wcstring condition_string = condition;
        parse_error_list_t errors;
        if (parse_util_detect_errors(condition_string, &errors,
                                     false /* do not accept incomplete */)) {
            streams.err.append_format(L"%ls: Condition '%ls' contained a syntax error", cmd,
                                      condition);
            for (size_t i = 0; i < errors.size(); i++) {
                streams.err.append_format(L"\n%ls: ", cmd);
                streams.err.append(
                    errors.at(i).describe(condition_string, parser.is_interactive()));
            }
            return STATUS_CMD_ERROR;
        }
    }

    if (comp && std::wcslen(comp)) {
        wcstring prefix;
        prefix.append(cmd);
        prefix.append(L": ");

        if (maybe_t<wcstring> err_text = parse_util_detect_errors_in_argument_list(comp, prefix)) {
            streams.err.append_format(L"%ls: Completion '%ls' contained a syntax error\n", cmd,
                                      comp);
            streams.err.append(*err_text);
            streams.err.push_back(L'\n');
            return STATUS_CMD_ERROR;
        }
    }

    if (do_complete) {
        if (!have_do_complete_param) {
            // No argument given, try to use the current commandline.
            const wchar_t *cmd = reader_get_buffer();
            if (cmd == NULL) {
                // This corresponds to using 'complete -C' in non-interactive mode.
                // See #2361    .
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            do_complete_param = cmd;
        }
        const wchar_t *token;

        parse_util_token_extent(do_complete_param.c_str(), do_complete_param.size(), &token, 0, 0,
                                0);

        // Create a scoped transient command line, so that builtin_commandline will see our
        // argument, not the reader buffer.
        parser.libdata().transient_commandlines.push_back(do_complete_param);
        cleanup_t remove_transient([&] { parser.libdata().transient_commandlines.pop_back(); });

        if (parser.libdata().builtin_complete_current_commandline) {
            // Prevent accidental recursion (see #6171).
        } else if (parser.libdata().builtin_complete_recursion_level >= 24) {
            // Allow a limited number of recursive calls to complete (#3474).
            streams.err.append_format(L"%ls: maximum recursion depth reached\n", cmd);
        } else {
            parser.libdata().builtin_complete_recursion_level++;
            assert(!parser.libdata().builtin_complete_current_commandline);
            if (!have_do_complete_param)
                parser.libdata().builtin_complete_current_commandline = true;

            std::vector<completion_t> comp;
            complete(do_complete_param, &comp, completion_request_t::fuzzy_match, parser.vars(),
                     parser.shared());

            for (size_t i = 0; i < comp.size(); i++) {
                const completion_t &next = comp.at(i);

                // Make a fake commandline, and then apply the completion to it.
                const wcstring faux_cmdline = token;
                size_t tmp_cursor = faux_cmdline.size();
                wcstring faux_cmdline_with_completion = completion_apply_to_command_line(
                    next.completion, next.flags, faux_cmdline, &tmp_cursor, false);

                // completion_apply_to_command_line will append a space unless COMPLETE_NO_SPACE
                // is set. We don't want to set COMPLETE_NO_SPACE because that won't close
                // quotes. What we want is to close the quote, but not append the space. So we
                // just look for the space and clear it.
                if (!(next.flags & COMPLETE_NO_SPACE) &&
                    string_suffixes_string(L" ", faux_cmdline_with_completion)) {
                    faux_cmdline_with_completion.resize(faux_cmdline_with_completion.size() - 1);
                }

                // The input data is meant to be something like you would have on the command
                // line, e.g. includes backslashes. The output should be raw, i.e. unescaped. So
                // we need to unescape the command line. See #1127.
                unescape_string_in_place(&faux_cmdline_with_completion, UNESCAPE_DEFAULT);
                streams.out.append(faux_cmdline_with_completion);

                // Append any description.
                if (!next.description.empty()) {
                    streams.out.push_back(L'\t');
                    streams.out.append(next.description);
                }
                streams.out.push_back(L'\n');
            }

            parser.libdata().builtin_complete_recursion_level--;
            parser.libdata().builtin_complete_current_commandline = false;
        }
    } else if (cmd_to_complete.empty() && path.empty()) {
        // No arguments specified, meaning we print the definitions of all specified completions
        // to stdout.
        const wcstring repr = complete_print();

        // colorize if interactive
        if (!streams.out_is_redirected && isatty(STDOUT_FILENO)) {
            std::vector<highlight_spec_t> colors;
            size_t len = repr.size();
            highlight_shell_no_io(repr, colors, len, nullptr, env_stack_t::globals());
            streams.out.append(str2wcstring(colorize(repr, colors)));
        } else {
            streams.out.append(repr);
        }
    } else {
        int flags = COMPLETE_AUTO_SPACE;
        if (preserve_order) {
            flags |= COMPLETE_DONT_SORT;
        }

        if (remove) {
            builtin_complete_remove(cmd_to_complete, path, short_opt.c_str(), gnu_opt, old_opt);
        } else {
            builtin_complete_add(cmd_to_complete, path, short_opt.c_str(), gnu_opt, old_opt,
                                 result_mode, condition, comp, desc, flags);
        }

        // Handle wrap targets (probably empty). We only wrap commands, not paths.
        for (size_t w = 0; w < wrap_targets.size(); w++) {
            const wcstring &wrap_target = wrap_targets.at(w);
            for (size_t i = 0; i < cmd_to_complete.size(); i++) {
                (remove ? complete_remove_wrapper : complete_add_wrapper)(cmd_to_complete.at(i),
                                                                          wrap_target);
            }
        }
    }

    return STATUS_CMD_OK;
}
