use super::prelude::*;
use crate::parser::{Block, BlockType};
use crate::reader::reader_read;
use libc::STDIN_FILENO;

/// Implementation of the builtin breakpoint command, used to launch the interactive debugger.
pub fn breakpoint(parser: &Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> BuiltinResult {
    let cmd = argv[0];
    if argv.len() != 1 {
        streams.err.append(wgettext_fmt!(
            BUILTIN_ERR_ARG_COUNT1,
            cmd,
            0,
            argv.len() - 1
        ));
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
            .is_none_or(|b| b.typ() == BlockType::breakpoint)
        {
            streams.err.append(wgettext_fmt!(
                "%s: Command not valid at an interactive prompt\n",
                cmd,
            ));
            return Err(STATUS_ILLEGAL_CMD);
        }
    }

    let bpb = parser.push_block(Block::breakpoint_block());
    let io_chain = &streams.io_chain;
    reader_read(parser, STDIN_FILENO, io_chain)?;
    parser.pop_block(bpb);
    BuiltinResult::from_dynamic(parser.get_last_status())
}
