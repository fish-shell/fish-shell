use std::sync::atomic::Ordering;

// Implementation of the block builtin.
use super::prelude::*;

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
    parser: &Parser,
    streams: &mut IoStreams,
) -> Result<(Options, usize), Option<c_int>> {
    let cmd = args[0];

    const SHORT_OPTS: &wstr = L!(":eghl");
    const LONG_OPTS: &[WOption] = &[
        wopt(L!("erase"), ArgType::NoArgument, 'e'),
        wopt(L!("local"), ArgType::NoArgument, 'l'),
        wopt(L!("global"), ArgType::NoArgument, 'g'),
        wopt(L!("help"), ArgType::NoArgument, 'h'),
    ];

    let mut opts = Options::default();

    let mut w = WGetopter::new(SHORT_OPTS, LONG_OPTS, args);
    while let Some(c) = w.next_opt() {
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
                builtin_missing_argument(parser, streams, cmd, args[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, args[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            _ => {
                panic!("unexpected retval from WGetopter");
            }
        }
    }

    Ok((opts, w.wopt_index))
}

/// The block builtin, used for temporarily blocking events.
pub fn block(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> Option<c_int> {
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

        if parser.global_event_blocks.load(Ordering::Relaxed) == 0 {
            streams
                .err
                .append(wgettext_fmt!("%ls: No blocks defined\n", cmd));
            return STATUS_CMD_ERROR;
        }
        parser.global_event_blocks.fetch_sub(1, Ordering::Relaxed);
        return STATUS_CMD_OK;
    }

    let mut block_idx = 0;
    let have_block = {
        let mut block = parser.block_at_index(block_idx);

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
                        parser.block_at_index(block_idx)
                    } else {
                        break;
                    }
                }
            }
        }
        block.is_some()
    };

    if have_block {
        parser.block_at_index_mut(block_idx).unwrap().event_blocks += 1;
    } else {
        parser.global_event_blocks.fetch_add(1, Ordering::Relaxed);
    }

    return STATUS_CMD_OK;
}
