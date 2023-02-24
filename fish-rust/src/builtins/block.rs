// Implementation of the block builtin.
use super::shared::{
    builtin_missing_argument, builtin_print_help, io_streams_t, STATUS_CMD_ERROR, STATUS_CMD_OK,
    STATUS_INVALID_ARGS,
};
use crate::{
    builtins::shared::builtin_unknown_option,
    ffi::{parser_t, Repin},
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
    args: &mut [&wstr],
    parser: &mut parser_t,
    streams: &mut io_streams_t,
) -> Result<(Options, usize), Option<c_int>> {
    let cmd = args[0];

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
                builtin_missing_argument(parser, streams, cmd, args[w.woptind - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, args[w.woptind - 1], false);
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
pub fn block(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    args: &mut [&wstr],
) -> Option<c_int> {
    let cmd = args[0];

    let opts = match parse_options(args, parser, streams) {
        Ok((opts, _)) => opts,
        Err(err @ Some(_)) if err != STATUS_CMD_OK => return err,
        Err(err) => panic!("Illogical exit code from parse_options(): {err:?}"),
    };

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    if opts.erase {
        if opts.scope != Scope::Unset {
            streams.err.append(wgettext_fmt!(
                "%ls: Can not specify scope when removing block\n",
                cmd
            ));
            return STATUS_INVALID_ARGS;
        }

        if parser.ffi_global_event_blocks() == 0 {
            streams
                .err
                .append(wgettext_fmt!("%ls: No blocks defined\n", cmd));
            return STATUS_CMD_ERROR;
        }
        parser.pin().ffi_decr_global_event_blocks();
        return STATUS_CMD_OK;
    }

    let mut block_idx = 0;
    let mut block = unsafe { parser.pin().block_at_index1(block_idx).as_mut() };

    match opts.scope {
        Scope::Local => {
            // If this is the outermost block, then we're global
            if block_idx + 1 >= parser.ffi_blocks_size() {
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
                    unsafe { parser.pin().block_at_index1(block_idx).as_mut() }
                } else {
                    break;
                }
            }
        }
    }

    if let Some(block) = block.as_mut() {
        block.pin().ffi_incr_event_blocks();
    } else {
        parser.pin().ffi_incr_global_event_blocks();
    }

    return STATUS_CMD_OK;
}
