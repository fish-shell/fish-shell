use super::prelude::*;

use crate::builtins::commandline;
use crate::io::{IoChain, OutputStream, StringOutputStream};
use crate::wutil::fish_wcstoi;

pub fn fish_is_nth_token(
    parser: &Parser,
    streams: &mut IoStreams,
    argv: &mut [&wstr],
) -> BuiltinResult {
    let cmd = argv[0];
    let print_hints = false;

    const SHORTOPTS: &wstr = L!("h");
    const LONGOPTS: &[WOption] = &[wopt(L!("help"), ArgType::NoArgument, 'h')];

    let mut w = WGetopter::new(SHORTOPTS, LONGOPTS, argv);
    while let Some(opt) = w.next_opt() {
        match opt {
            'h' => {
                builtin_print_help(parser, streams, cmd);
                return Ok(SUCCESS);
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, argv[w.wopt_index - 1], print_hints);
                return Err(STATUS_INVALID_ARGS);
            }
            ';' => {
                builtin_unexpected_argument(
                    parser,
                    streams,
                    cmd,
                    argv[w.wopt_index - 1],
                    print_hints,
                );
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, argv[w.wopt_index - 1], print_hints);
                return Err(STATUS_INVALID_ARGS);
            }
            _ => unreachable!(),
        }
    }

    let arg_index = w.wopt_index;
    let arg_count = argv.len().saturating_sub(arg_index);
    if arg_count != 1 {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_ARG_COUNT1, cmd, 1, arg_count));
        return Err(STATUS_INVALID_ARGS);
    }

    let n_str = argv[arg_index];
    let requested = match fish_wcstoi(n_str) {
        Ok(value) => value,
        Err(_) => {
            streams
                .err
                .append(wgettext_fmt!("%ls: %ls: invalid integer\n", cmd, n_str));
            return Err(STATUS_INVALID_ARGS);
        }
    };

    let mut subcommand_argv: [&wstr; 2] = [L!("commandline"), L!("-pxc")];
    let mut out_stream = OutputStream::String(StringOutputStream::new());
    let mut err_stream = OutputStream::Null;
    let io_chain = IoChain::new();
    let mut sub_streams = IoStreams::new(&mut out_stream, &mut err_stream, &io_chain);
    let status = commandline::commandline(parser, &mut sub_streams, &mut subcommand_argv);
    drop(sub_streams);

    status?;

    let tokens_output = out_stream.contents();

    let mut count: usize = 0;
    if !tokens_output.is_empty() {
        let chars = tokens_output.as_char_slice();
        let mut start = 0usize;
        for (idx, ch) in chars.iter().enumerate() {
            if *ch == '\n' {
                if idx > start {
                    let token = &tokens_output[start..idx];
                    if token.chars().next().is_some_and(|first| first != '-') {
                        count += 1;
                    }
                }
                start = idx + 1;
            }
        }

        if start < chars.len() {
            let token = &tokens_output[start..];
            if token.chars().next().is_some_and(|first| first != '-') {
                count += 1;
            }
        }
    }

    if requested < 0 {
        return Err(STATUS_CMD_ERROR);
    }

    if count == requested as usize {
        Ok(SUCCESS)
    } else {
        Err(STATUS_CMD_ERROR)
    }
}
