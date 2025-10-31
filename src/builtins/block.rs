use std::sync::atomic::Ordering;

// Implementation of the block builtin.
use super::prelude::*;

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
enum Scope {
    #[default]
    Unset,
    Global,
    Local,
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
) -> Result<(Options, usize), ErrorCode> {
    let cmd = args[0];

    const SHORT_OPTS: &wstr = L!("eghl");
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
            ';' => {
                builtin_unexpected_argument(parser, streams, cmd, args[w.wopt_index - 1], false);
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
pub fn block(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let cmd = args[0];

    let (opts, _) = parse_options(args, parser, streams)?;

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return Ok(SUCCESS);
    }

    if opts.erase {
        if opts.scope != Scope::Unset {
            streams.err.append(wgettext_fmt!(
                "%s: Can not specify scope when removing block\n",
                cmd
            ));
            return Err(STATUS_INVALID_ARGS);
        }

        if parser.global_event_blocks.load(Ordering::Relaxed) == 0 {
            streams
                .err
                .append(wgettext_fmt!("%s: No blocks defined\n", cmd));
            return Err(STATUS_CMD_ERROR);
        }
        parser.global_event_blocks.fetch_sub(1, Ordering::Relaxed);
        return Ok(SUCCESS);
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
        parser.block_at_index_mut(block_idx).unwrap().event_blocks |= true;
    } else {
        parser.global_event_blocks.fetch_add(1, Ordering::Relaxed);
    }

    return Ok(SUCCESS);
}
