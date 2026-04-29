use super::prelude::*;
use crate::builtins::error::Error;
use crate::parser::{Block, BlockType};
use crate::reader::reader_read;
use crate::{err_fmt, err_str};
use libc::STDIN_FILENO;

/// Implementation of the builtin breakpoint command, used to launch the interactive debugger.
pub fn breakpoint(
    parser: &mut Parser,
    streams: &mut IoStreams,
    argv: &mut [&wstr],
) -> BuiltinResult {
    let cmd = argv[0];
    if argv.len() != 1 {
        err_fmt!(Error::UNEXP_ARG_COUNT, 0, argv.len() - 1)
            .cmd(cmd)
            .finish(streams);
        return Err(STATUS_INVALID_ARGS);
    }

    // If we're not interactive then we can't enter the debugger. So treat this command as a no-op.
    if !parser.is_interactive() {
        return Err(STATUS_CMD_ERROR);
    }

    // Ensure we don't allow creating a breakpoint at an interactive prompt. There may be a simpler
    // or clearer way to do this but this works.
    {
        if parser
            .block_at_index(1)
            .is_none_or(|b| b.typ() == BlockType::Breakpoint)
        {
            err_str!("Command not valid at an interactive prompt")
                .cmd(cmd)
                .finish(streams);
            return Err(STATUS_ILLEGAL_CMD);
        }
    }

    let bpb = parser.push_block(Block::breakpoint_block());
    let io_chain = &streams.io_chain;
    reader_read(parser, STDIN_FILENO, io_chain)?;
    parser.pop_block(bpb);
    BuiltinResult::from_dynamic(parser.last_status())
}
