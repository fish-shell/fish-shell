use std::sync::atomic::Ordering;

// Implementation of the block builtin.
use super::shared::{
    builtin_missing_argument, builtin_print_help, STATUS_CMD_ERROR, STATUS_CMD_OK,
    STATUS_INVALID_ARGS,
};
use crate::io::IoStreams;
use crate::parser::Parser;
use crate::wchar::WString;
use crate::{
    builtins::shared::builtin_unknown_option,
    ffi::Repin,
    wchar::{wstr, L},
    wgetopt::{wgetopter_t, wopt, woption, woption_argument_t},
    wutil::wgettext_fmt,
};
use libc::c_int;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Scope {
    Unset,
    Global,
    Local,
}

impl Default for Scope {
    fn default() -> Self {
        Self::Unset
    }
}

#[derive(Debug, Clone, Copy, Default)]
struct Options {
    scope: Scope,
    erase: bool,
    print_help: bool,
}

fn parse_options(
    args: &mut [WString],
    parser: &Parser,
    streams: &mut IoStreams<'_>,
) -> Result<(Options, usize), Option<c_int>> {
    const SHORT_OPTS: &wstr = L!(":eghl");
    const LONG_OPTS: &[woption] = &[
        wopt(L!("erase"), woption_argument_t::no_argument, 'e'),
        wopt(L!("local"), woption_argument_t::no_argument, 'l'),
        wopt(L!("global"), woption_argument_t::no_argument, 'g'),
        wopt(L!("help"), woption_argument_t::no_argument, 'h'),
    ];

    let mut opts = Options::default();

    let mut w = wgetopter_t::new(SHORT_OPTS, LONG_OPTS, args);
    while let Some(c) = w.wgetopt_long() {
        match c {
            'h' => {
                opts.print_help = true;
            }
            'g' => {
                opts.scope = Scope::Global;
            }
            'l' => {
                opts.scope = Scope::Local;
            }
            'e' => {
                opts.erase = true;
            }
            ':' => {
                builtin_missing_argument(parser, streams, w.cmd(), &w.argv()[w.woptind - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, w.cmd(), &w.argv()[w.woptind - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            _ => {
                panic!("unexpected retval from wgetopt_long");
            }
        }
    }

    Ok((opts, w.woptind))
}

/// The block builtin, used for temporarily blocking events.
pub fn block(parser: &Parser, streams: &mut IoStreams<'_>, args: &mut [WString]) -> Option<c_int> {
    let opts = match parse_options(args, parser, streams) {
        Ok((opts, _)) => opts,
        Err(err @ Some(_)) if err != STATUS_CMD_OK => return err,
        Err(err) => panic!("Illogical exit code from parse_options(): {err:?}"),
    };

    let cmd = &args[0];

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    if opts.erase {
        if opts.scope != Scope::Unset {
            streams.err.append(&wgettext_fmt!(
                "%ls: Can not specify scope when removing block\n",
                cmd
            ));
            return STATUS_INVALID_ARGS;
        }

        if parser.global_event_blocks.load(Ordering::Relaxed) == 0 {
            streams
                .err
                .append(&wgettext_fmt!("%ls: No blocks defined\n", cmd));
            return STATUS_CMD_ERROR;
        }
        parser.global_event_blocks.fetch_sub(1, Ordering::Relaxed);
        return STATUS_CMD_OK;
    }

    let mut block_idx = 0;
    let blocks = parser.blocks();
    let mut block = blocks.get(block_idx);

    match opts.scope {
        Scope::Local => {
            // If this is the outermost block, then we're global
            if block_idx + 1 >= parser.blocks_size() {
                block = None;
            }
        }
        Scope::Global => {
            block = None;
        }
        Scope::Unset => {
            loop {
                block = if let Some(block) = block.as_mut() {
                    if !block.is_function_call() {
                        break;
                    }
                    // Set it in function scope
                    block_idx += 1;
                    blocks.get(block_idx)
                } else {
                    break;
                }
            }
        }
    }

    if block.is_some() {
        let mut blocks = parser.blocks_mut();
        blocks[block_idx].event_blocks += 1;
    } else {
        parser.global_event_blocks.fetch_add(1, Ordering::Relaxed);
    }

    return STATUS_CMD_OK;
}
