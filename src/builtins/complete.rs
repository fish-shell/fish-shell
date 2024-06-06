use super::prelude::*;
use crate::common::{unescape_string, ScopeGuard, UnescapeFlags, UnescapeStringStyle};
use crate::complete::{complete_add_wrapper, complete_remove_wrapper, CompletionRequestOptions};
use crate::highlight::colorize;
use crate::highlight::highlight_shell;
use crate::nix::isatty;
use crate::parse_constants::ParseErrorList;
use crate::parse_util::parse_util_detect_errors_in_argument_list;
use crate::parse_util::{parse_util_detect_errors, parse_util_token_extent};
use crate::reader::{commandline_get_state, completion_apply_to_command_line};
use crate::wcstringutil::string_suffixes_string;
use crate::{
    common::str2wcstring,
    complete::{
        complete_add, complete_print, complete_remove, complete_remove_all, CompleteFlags,
        CompleteOptionType, CompletionMode,
    },
};
use libc::STDOUT_FILENO;

// builtin_complete_* are a set of rather silly looping functions that make sure that all the proper
// combinations of complete_add or complete_remove get called. This is needed since complete allows
// you to specify multiple switches on a single commandline, like 'complete -s a -s b -s c', but the
// complete_add function only accepts one short switch and one long switch.

/// Silly function.
fn builtin_complete_add2(
    cmd: &wstr,
    cmd_is_path: bool,
    short_opt: &wstr,
    gnu_opts: &[WString],
    old_opts: &[WString],
    result_mode: CompletionMode,
    condition: &[WString],
    comp: &wstr,
    desc: &wstr,
    flags: CompleteFlags,
) {
    for short_opt in short_opt.chars() {
        complete_add(
            cmd.to_owned(),
            cmd_is_path,
            WString::from(&[short_opt][..]),
            CompleteOptionType::Short,
            result_mode,
            condition.to_vec(),
            comp.to_owned(),
            desc.to_owned(),
            flags,
        );
    }

    for gnu_opt in gnu_opts {
        complete_add(
            cmd.to_owned(),
            cmd_is_path,
            gnu_opt.to_owned(),
            CompleteOptionType::DoubleLong,
            result_mode,
            condition.to_vec(),
            comp.to_owned(),
            desc.to_owned(),
            flags,
        );
    }

    for old_opt in old_opts {
        complete_add(
            cmd.to_owned(),
            cmd_is_path,
            old_opt.to_owned(),
            CompleteOptionType::SingleLong,
            result_mode,
            condition.to_vec(),
            comp.to_owned(),
            desc.to_owned(),
            flags,
        );
    }

    if old_opts.is_empty() && gnu_opts.is_empty() && short_opt.is_empty() {
        complete_add(
            cmd.to_owned(),
            cmd_is_path,
            WString::new(),
            CompleteOptionType::ArgsOnly,
            result_mode,
            condition.to_vec(),
            comp.to_owned(),
            desc.to_owned(),
            flags,
        );
    }
}

/// Sily function.
fn builtin_complete_add(
    cmds: &[WString],
    paths: &[WString],
    short_opt: &wstr,
    gnu_opt: &[WString],
    old_opt: &[WString],
    result_mode: CompletionMode,
    condition: &[WString],
    comp: &wstr,
    desc: &wstr,
    flags: CompleteFlags,
) {
    for cmd in cmds {
        builtin_complete_add2(
            cmd,
            false, /* not path */
            short_opt,
            gnu_opt,
            old_opt,
            result_mode,
            condition,
            comp,
            desc,
            flags,
        );
    }
    for path in paths {
        builtin_complete_add2(
            path,
            true, /* is path */
            short_opt,
            gnu_opt,
            old_opt,
            result_mode,
            condition,
            comp,
            desc,
            flags,
        );
    }
}

fn builtin_complete_remove_cmd(
    cmd: &WString,
    cmd_is_path: bool,
    short_opt: &wstr,
    gnu_opt: &[WString],
    old_opt: &[WString],
) {
    let mut removed = false;
    for s in short_opt.chars() {
        complete_remove(
            cmd.to_owned(),
            cmd_is_path,
            wstr::from_char_slice(&[s]),
            CompleteOptionType::Short,
        );
        removed = true;
    }

    for opt in old_opt {
        complete_remove(
            cmd.to_owned(),
            cmd_is_path,
            opt,
            CompleteOptionType::SingleLong,
        );
        removed = true;
    }

    for opt in gnu_opt {
        complete_remove(
            cmd.to_owned(),
            cmd_is_path,
            opt,
            CompleteOptionType::DoubleLong,
        );
        removed = true;
    }

    if !removed {
        // This means that all loops were empty.
        complete_remove_all(cmd.to_owned(), cmd_is_path);
    }
}

fn builtin_complete_remove(
    cmds: &[WString],
    paths: &[WString],
    short_opt: &wstr,
    gnu_opt: &[WString],
    old_opt: &[WString],
) {
    for cmd in cmds {
        builtin_complete_remove_cmd(cmd, false /* not path */, short_opt, gnu_opt, old_opt);
    }

    for path in paths {
        builtin_complete_remove_cmd(path, true /* is path */, short_opt, gnu_opt, old_opt);
    }
}

fn builtin_complete_print(cmd: &wstr, streams: &mut IoStreams, parser: &Parser) {
    let repr = complete_print(cmd);

    // colorize if interactive
    if !streams.out_is_redirected && isatty(STDOUT_FILENO) {
        let mut colors = vec![];
        highlight_shell(&repr, &mut colors, &parser.context(), false, None);
        streams
            .out
            .append(str2wcstring(&colorize(&repr, &colors, parser.vars())));
    } else {
        streams.out.append(repr);
    }
}

/// Values used for long-only options.
const OPT_ESCAPE: char = '\x01';

/// The complete builtin. Used for specifying programmable tab-completions. Calls the functions in
/// complete.cpp for any heavy lifting.
pub fn complete(parser: &Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> Option<c_int> {
    let cmd = argv[0];
    let argc = argv.len();
    let mut result_mode = CompletionMode::default();
    let mut remove = false;
    let mut short_opt = WString::new();
    // todo!("these whould be Vec<&wstr>");
    let mut gnu_opt = vec![];
    let mut old_opt = vec![];
    let mut subcommand = vec![];
    let mut comp = WString::new();
    let mut desc = WString::new();
    let mut condition = vec![];
    let mut do_complete = false;
    let mut do_complete_param = None;
    let mut cmd_to_complete = vec![];
    let mut path = vec![];
    let mut wrap_targets = vec![];
    let mut preserve_order = false;
    let mut unescape_output = true;

    const short_options: &wstr = L!(":a:c:p:s:l:o:d:fFrxeuAn:C::w:hk");
    const long_options: &[WOption] = &[
        wopt(L!("exclusive"), ArgType::NoArgument, 'x'),
        wopt(L!("no-files"), ArgType::NoArgument, 'f'),
        wopt(L!("force-files"), ArgType::NoArgument, 'F'),
        wopt(L!("require-parameter"), ArgType::NoArgument, 'r'),
        wopt(L!("path"), ArgType::RequiredArgument, 'p'),
        wopt(L!("command"), ArgType::RequiredArgument, 'c'),
        wopt(L!("short-option"), ArgType::RequiredArgument, 's'),
        wopt(L!("long-option"), ArgType::RequiredArgument, 'l'),
        wopt(L!("old-option"), ArgType::RequiredArgument, 'o'),
        wopt(L!("subcommand"), ArgType::RequiredArgument, 'S'),
        wopt(L!("description"), ArgType::RequiredArgument, 'd'),
        wopt(L!("arguments"), ArgType::RequiredArgument, 'a'),
        wopt(L!("erase"), ArgType::NoArgument, 'e'),
        wopt(L!("unauthoritative"), ArgType::NoArgument, 'u'),
        wopt(L!("authoritative"), ArgType::NoArgument, 'A'),
        wopt(L!("condition"), ArgType::RequiredArgument, 'n'),
        wopt(L!("wraps"), ArgType::RequiredArgument, 'w'),
        wopt(L!("do-complete"), ArgType::OptionalArgument, 'C'),
        wopt(L!("help"), ArgType::NoArgument, 'h'),
        wopt(L!("keep-order"), ArgType::NoArgument, 'k'),
        wopt(L!("escape"), ArgType::NoArgument, OPT_ESCAPE),
    ];

    let mut have_x = false;

    let mut w = WGetopter::new(short_options, long_options, argv);
    while let Some(opt) = w.next_opt() {
        match opt {
            'x' => {
                result_mode.no_files = true;
                result_mode.requires_param = true;
                // Needed to print an error later;
                have_x = true;
            }
            'f' => {
                result_mode.no_files = true;
            }
            'F' => {
                result_mode.force_files = true;
            }
            'r' => {
                result_mode.requires_param = true;
            }
            'k' => {
                preserve_order = true;
            }
            'p' | 'c' => {
                if let Some(tmp) = unescape_string(
                    w.woptarg.unwrap(),
                    UnescapeStringStyle::Script(UnescapeFlags::SPECIAL),
                ) {
                    if opt == 'p' {
                        path.push(tmp);
                    } else {
                        cmd_to_complete.push(tmp);
                    }
                } else {
                    streams.err.append(wgettext_fmt!(
                        "%ls: Invalid token '%ls'\n",
                        cmd,
                        w.woptarg.unwrap()
                    ));
                    return STATUS_INVALID_ARGS;
                }
            }
            'd' => {
                desc = w.woptarg.unwrap().to_owned();
            }
            'u' => {
                // This option was removed in commit 1911298 and is now a no-op.
            }
            'A' => {
                // This option was removed in commit 1911298 and is now a no-op.
            }
            's' => {
                let arg = w.woptarg.unwrap();
                short_opt.extend(arg.chars());
                if arg.is_empty() {
                    streams
                        .err
                        .append(wgettext_fmt!("%ls: -s requires a non-empty string\n", cmd,));
                    return STATUS_INVALID_ARGS;
                }
            }
            'l' => {
                let arg = w.woptarg.unwrap();
                gnu_opt.push(arg.to_owned());
                if arg.is_empty() {
                    streams
                        .err
                        .append(wgettext_fmt!("%ls: -l requires a non-empty string\n", cmd,));
                    return STATUS_INVALID_ARGS;
                }
            }
            'o' => {
                let arg = w.woptarg.unwrap();
                old_opt.push(arg.to_owned());
                if arg.is_empty() {
                    streams
                        .err
                        .append(wgettext_fmt!("%ls: -o requires a non-empty string\n", cmd,));
                    return STATUS_INVALID_ARGS;
                }
            }
            'S' => {
                let arg = w.woptarg.unwrap();
                subcommand.push(arg.to_owned());
                if arg.is_empty() {
                    streams
                        .err
                        .append(wgettext_fmt!("%ls: -S requires a non-empty string\n", cmd,));
                    return STATUS_INVALID_ARGS;
                }
            }
            'a' => {
                comp = w.woptarg.unwrap().to_owned();
            }
            'e' => remove = true,
            'n' => {
                condition.push(w.woptarg.unwrap().to_owned());
            }
            'w' => {
                wrap_targets.push(w.woptarg.unwrap().to_owned());
            }
            'C' => {
                do_complete = true;
                if let Some(s) = w.woptarg {
                    do_complete_param = Some(s.to_owned());
                }
            }
            OPT_ESCAPE => {
                unescape_output = false;
            }
            'h' => {
                builtin_print_help(parser, streams, cmd);
                return STATUS_CMD_OK;
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, argv[w.wopt_index - 1], true);
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, argv[w.wopt_index - 1], true);
                return STATUS_INVALID_ARGS;
            }
            _ => panic!("unexpected retval from WGetopter"),
        }
    }

    if result_mode.no_files && result_mode.force_files {
        if !have_x {
            streams.err.append(wgettext_fmt!(
                BUILTIN_ERR_COMBO2,
                "complete",
                "'--no-files' and '--force-files'"
            ));
        } else {
            // The reason for us not wanting files is `-x`,
            // which is short for `-rf`.
            streams.err.append(wgettext_fmt!(
                BUILTIN_ERR_COMBO2,
                "complete",
                "'--exclusive' and '--force-files'"
            ));
        }
        return STATUS_INVALID_ARGS;
    }

    if w.wopt_index != argc {
        // Use one left-over arg as the do-complete argument
        // to enable `complete -C "git check"`.
        if do_complete && do_complete_param.is_none() && argc == w.wopt_index + 1 {
            do_complete_param = Some(argv[argc - 1].to_owned());
        } else if !do_complete && cmd_to_complete.is_empty() && argc == w.wopt_index + 1 {
            // Or use one left-over arg as the command to complete
            cmd_to_complete.push(argv[argc - 1].to_owned());
        } else {
            streams
                .err
                .append(wgettext_fmt!(BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }
    }

    for condition_string in &condition {
        let mut errors = ParseErrorList::new();
        if parse_util_detect_errors(condition_string, Some(&mut errors), false).is_err() {
            for error in errors {
                let prefix = cmd.to_owned() + L!(": -n '") + &condition_string[..] + L!("': ");
                streams.err.append(error.describe_with_prefix(
                    condition_string,
                    &prefix,
                    parser.is_interactive(),
                    false,
                ));
                streams.err.push('\n');
            }
            return STATUS_CMD_ERROR;
        }
    }

    if !comp.is_empty() {
        let mut prefix = WString::new();
        prefix.push_utfstr(cmd);
        prefix.push_str(": ");

        if let Err(err_text) = parse_util_detect_errors_in_argument_list(&comp, &prefix) {
            streams.err.append(wgettext_fmt!(
                "%ls: %ls: contains a syntax error\n",
                cmd,
                comp
            ));
            streams.err.append(err_text);
            streams.err.push('\n');
            return STATUS_CMD_ERROR;
        }
    }

    if do_complete {
        let have_do_complete_param = do_complete_param.is_some();
        let do_complete_param = match do_complete_param {
            None => {
                // No argument given, try to use the current commandline.
                let commandline_state = commandline_get_state(true);
                if !commandline_state.initialized {
                    // This corresponds to using 'complete -C' in non-interactive mode.
                    // See #2361    .
                    builtin_missing_argument(parser, streams, cmd, L!("-C"), true);
                    return STATUS_INVALID_ARGS;
                }
                commandline_state.text
            }
            Some(param) => param,
        };

        let mut token = 0..0;
        parse_util_token_extent(
            &do_complete_param,
            do_complete_param.len(),
            &mut token,
            None,
        );

        // Create a scoped transient command line, so that builtin_commandline will see our
        // argument, not the reader buffer.
        parser
            .libdata_mut()
            .transient_commandlines
            .push(do_complete_param.clone());
        let _remove_transient = ScopeGuard::new((), |()| {
            parser.libdata_mut().transient_commandlines.pop();
        });

        // Prevent accidental recursion (see #6171).
        if !parser.libdata().builtin_complete_current_commandline {
            if !have_do_complete_param {
                parser.libdata_mut().builtin_complete_current_commandline = true;
            }

            let (mut comp, _needs_load) = crate::complete::complete(
                &do_complete_param,
                CompletionRequestOptions::normal(),
                &parser.context(),
            );

            // Apply the same sort and deduplication treatment as pager completions
            crate::complete::sort_and_prioritize(&mut comp, CompletionRequestOptions::default());

            for next in comp {
                // Make a fake commandline, and then apply the completion to it.
                let faux_cmdline = &do_complete_param[token.clone()];
                let mut tmp_cursor = faux_cmdline.len();
                let mut faux_cmdline_with_completion = completion_apply_to_command_line(
                    &next.completion,
                    next.flags,
                    faux_cmdline,
                    &mut tmp_cursor,
                    false,
                );

                // completion_apply_to_command_line will append a space unless COMPLETE_NO_SPACE
                // is set. We don't want to set COMPLETE_NO_SPACE because that won't close
                // quotes. What we want is to close the quote, but not append the space. So we
                // just look for the space and clear it.
                if !next.flags.contains(CompleteFlags::NO_SPACE)
                    && string_suffixes_string(L!(" "), &faux_cmdline_with_completion)
                {
                    faux_cmdline_with_completion.truncate(faux_cmdline_with_completion.len() - 1);
                }

                if unescape_output {
                    // The input data is meant to be something like you would have on the command
                    // line, e.g. includes backslashes. The output should be raw, i.e. unescaped. So
                    // we need to unescape the command line. See #1127.
                    faux_cmdline_with_completion = unescape_string(
                        &faux_cmdline_with_completion,
                        UnescapeStringStyle::Script(UnescapeFlags::default()),
                    )
                    .expect("Unescaping commandline to complete failed");
                }

                // Append any description.
                if !next.description.is_empty() {
                    faux_cmdline_with_completion
                        .reserve(faux_cmdline_with_completion.len() + 2 + next.description.len());
                    faux_cmdline_with_completion.push('\t');
                    faux_cmdline_with_completion.push_utfstr(&next.description);
                }
                faux_cmdline_with_completion.push('\n');
                streams.out.append(faux_cmdline_with_completion);
            }

            parser.libdata_mut().builtin_complete_current_commandline = false;
        }
    } else if path.is_empty()
        && gnu_opt.is_empty()
        && short_opt.is_empty()
        && old_opt.is_empty()
        && !remove
        && comp.is_empty()
        && desc.is_empty()
        && condition.is_empty()
        && wrap_targets.is_empty()
        && !result_mode.no_files
        && !result_mode.force_files
        && !result_mode.requires_param
    {
        // No arguments that would add or remove anything specified, so we print the definitions of
        // all matching completions.
        if cmd_to_complete.is_empty() {
            builtin_complete_print(L!(""), streams, parser);
        } else {
            for cmd in cmd_to_complete {
                builtin_complete_print(&cmd, streams, parser);
            }
        }
    } else {
        let mut flags = CompleteFlags::AUTO_SPACE;
        // HACK: Don't escape tildes because at the beginning of a token they probably mean
        // $HOME, for example as produced by a recursive call to "complete -C".
        flags |= CompleteFlags::DONT_ESCAPE_TILDES;
        if preserve_order {
            flags |= CompleteFlags::DONT_SORT;
        }

        if remove {
            builtin_complete_remove(&cmd_to_complete, &path, &short_opt, &gnu_opt, &old_opt);
        } else {
            builtin_complete_add(
                &cmd_to_complete,
                &path,
                &short_opt,
                &gnu_opt,
                &old_opt,
                result_mode,
                &condition,
                &comp,
                &desc,
                flags,
            );
        }

        // Handle wrap targets (probably empty). We only wrap commands, not paths.
        for wrap_target in wrap_targets {
            for i in &cmd_to_complete {
                if remove {
                    complete_remove_wrapper(i.clone(), &wrap_target);
                } else {
                    complete_add_wrapper(i.clone(), wrap_target.clone());
                }
            }
        }
    }

    STATUS_CMD_OK
}
