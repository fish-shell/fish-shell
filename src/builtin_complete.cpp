// Functions used for implementing the complete builtin.
#include "config.h"  // IWYU pragma: keep

#include <stddef.h>
#include <wchar.h>

#include <memory>
#include <string>
#include <vector>

#include "builtin.h"
#include "common.h"
#include "complete.h"
#include "env.h"
#include "fallback.h"  // IWYU pragma: keep
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
static void builtin_complete_add2(const wchar_t *cmd, int cmd_type, const wchar_t *short_opt,
                                  const wcstring_list_t &gnu_opt, const wcstring_list_t &old_opt,
                                  int result_mode, const wchar_t *condition, const wchar_t *comp,
                                  const wchar_t *desc, int flags) {
    size_t i;
    const wchar_t *s;

    for (s = short_opt; *s; s++) {
        complete_add(cmd, cmd_type, wcstring(1, *s), option_type_short, result_mode, condition,
                     comp, desc, flags);
    }

    for (i = 0; i < gnu_opt.size(); i++) {
        complete_add(cmd, cmd_type, gnu_opt.at(i), option_type_double_long, result_mode, condition,
                     comp, desc, flags);
    }

    for (i = 0; i < old_opt.size(); i++) {
        complete_add(cmd, cmd_type, old_opt.at(i), option_type_single_long, result_mode, condition,
                     comp, desc, flags);
    }

    if (old_opt.empty() && gnu_opt.empty() && wcslen(short_opt) == 0) {
        complete_add(cmd, cmd_type, wcstring(), option_type_args_only, result_mode, condition, comp,
                     desc, flags);
    }
}

/// Silly function.
static void builtin_complete_add(const wcstring_list_t &cmd, const wcstring_list_t &path,
                                 const wchar_t *short_opt, wcstring_list_t &gnu_opt,
                                 wcstring_list_t &old_opt, int result_mode,
                                 const wchar_t *condition, const wchar_t *comp, const wchar_t *desc,
                                 int flags) {
    for (size_t i = 0; i < cmd.size(); i++) {
        builtin_complete_add2(cmd.at(i).c_str(), COMMAND, short_opt, gnu_opt, old_opt, result_mode,
                              condition, comp, desc, flags);
    }

    for (size_t i = 0; i < path.size(); i++) {
        builtin_complete_add2(path.at(i).c_str(), PATH, short_opt, gnu_opt, old_opt, result_mode,
                              condition, comp, desc, flags);
    }
}

static void builtin_complete_remove_cmd(const wcstring &cmd, int cmd_type, const wchar_t *short_opt,
                                        const wcstring_list_t &gnu_opt,
                                        const wcstring_list_t &old_opt) {
    bool removed = false;
    size_t i;
    for (i = 0; short_opt[i] != L'\0'; i++) {
        complete_remove(cmd, cmd_type, wcstring(1, short_opt[i]), option_type_short);
        removed = true;
    }

    for (i = 0; i < old_opt.size(); i++) {
        complete_remove(cmd, cmd_type, old_opt.at(i), option_type_single_long);
        removed = true;
    }

    for (i = 0; i < gnu_opt.size(); i++) {
        complete_remove(cmd, cmd_type, gnu_opt.at(i), option_type_double_long);
        removed = true;
    }

    if (!removed) {
        // This means that all loops were empty.
        complete_remove_all(cmd, cmd_type);
    }
}

static void builtin_complete_remove(const wcstring_list_t &cmd, const wcstring_list_t &path,
                                    const wchar_t *short_opt, const wcstring_list_t &gnu_opt,
                                    const wcstring_list_t &old_opt) {
    for (size_t i = 0; i < cmd.size(); i++) {
        builtin_complete_remove_cmd(cmd.at(i), COMMAND, short_opt, gnu_opt, old_opt);
    }

    for (size_t i = 0; i < path.size(); i++) {
        builtin_complete_remove_cmd(path.at(i), PATH, short_opt, gnu_opt, old_opt);
    }
}

/// The complete builtin. Used for specifying programmable tab-completions. Calls the functions in
// complete.cpp for any heavy lifting.
int builtin_complete(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    ASSERT_IS_MAIN_THREAD();
    static int recursion_level = 0;

    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    int result_mode = SHARED;
    int remove = 0;
    wcstring short_opt;
    wcstring_list_t gnu_opt, old_opt;
    const wchar_t *comp = L"", *desc = L"", *condition = L"";
    bool do_complete = false;
    wcstring do_complete_param;
    wcstring_list_t cmd_to_complete;
    wcstring_list_t path;
    wcstring_list_t wrap_targets;
    bool preserve_order = false;

    static const wchar_t *const short_options = L":a:c:p:s:l:o:d:frxeuAn:C::w:hk";
    static const struct woption long_options[] = {{L"exclusive", no_argument, NULL, 'x'},
                                                  {L"no-files", no_argument, NULL, 'f'},
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
                result_mode |= EXCLUSIVE;
                break;
            }
            case 'f': {
                result_mode |= NO_FILES;
                break;
            }
            case 'r': {
                result_mode |= NO_COMMON;
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
                const wchar_t *arg = w.woptarg ? w.woptarg : reader_get_buffer();
                if (arg == NULL) {
                    // This corresponds to using 'complete -C' in non-interactive mode.
                    // See #2361.
                    builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1]);
                    return STATUS_INVALID_ARGS;
                }
                do_complete_param = arg;
                break;
            }
            case 'h': {
                builtin_print_help(parser, streams, cmd, streams.out);
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

    if (w.woptind != argc) {
        streams.err.append_format(BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    if (condition && wcslen(condition)) {
        const wcstring condition_string = condition;
        parse_error_list_t errors;
        if (parse_util_detect_errors(condition_string, &errors,
                                     false /* do not accept incomplete */)) {
            streams.err.append_format(L"%ls: Condition '%ls' contained a syntax error", cmd,
                                      condition);
            for (size_t i = 0; i < errors.size(); i++) {
                streams.err.append_format(L"\n%s: ", cmd);
                streams.err.append(errors.at(i).describe(condition_string));
            }
            return STATUS_CMD_ERROR;
        }
    }

    if (comp && wcslen(comp)) {
        wcstring prefix;
        prefix.append(cmd);
        prefix.append(L": ");

        wcstring err_text;
        if (parser.detect_errors_in_argument_list(comp, &err_text, prefix.c_str())) {
            streams.err.append_format(L"%ls: Completion '%ls' contained a syntax error\n", cmd,
                                      comp);
            streams.err.append(err_text);
            streams.err.push_back(L'\n');
            return STATUS_CMD_ERROR;
        }
    }

    if (do_complete) {
        const wchar_t *token;

        parse_util_token_extent(do_complete_param.c_str(), do_complete_param.size(), &token, 0, 0,
                                0);

        // Create a scoped transient command line, so that bulitin_commandline will see our
        // argument, not the reader buffer.
        builtin_commandline_scoped_transient_t temp_buffer(do_complete_param);

        if (recursion_level < 1) {
            recursion_level++;

            std::vector<completion_t> comp;
            complete(do_complete_param, &comp, COMPLETION_REQUEST_DEFAULT);

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

            recursion_level--;
        }
    } else if (cmd_to_complete.empty() && path.empty()) {
        // No arguments specified, meaning we print the definitions of all specified completions
        // to stdout.
        streams.out.append(complete_print());
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
