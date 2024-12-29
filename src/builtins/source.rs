use std::os::fd::AsRawFd;

use crate::{
    common::{escape, scoped_push_replacer, FilenameRef},
    fds::wopen_cloexec,
    nix::isatty,
    parser::Block,
    reader::reader_read,
};
use nix::{fcntl::OFlag, sys::stat::Mode};

use super::prelude::*;

/// The  source builtin, sometimes called `.`. Evaluates the contents of a file in the current
/// context.
pub fn source(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let argc = args.len();

    let opts = HelpOnlyCmdOpts::parse(args, parser, streams)?;
    let cmd = match args.get(0) {
        Some(&cmd) => cmd,
        None => return Err(STATUS_INVALID_ARGS),
    };

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return Ok(SUCCESS);
    }

    // If we open a file, this ensures it stays open until the end of scope.
    let opened_file;

    // The fd that we read from, either from opened_fd or stdin.
    let fd;
    let func_filename;
    let optind = opts.optind;

    if argc == optind || args[optind] == "-" {
        if streams.stdin_fd < 0 {
            streams
                .err
                .append(wgettext_fmt!("%ls: stdin is closed\n", cmd));
            return Err(STATUS_CMD_ERROR);
        }
        // Either a bare `source` which means to implicitly read from stdin or an explicit `-`.
        if argc == optind && isatty(streams.stdin_fd) {
            // Don't implicitly read from the terminal.
            streams.err.append(wgettext_fmt!(
                "%ls: missing filename argument or input redirection\n",
                cmd
            ));
            return Err(STATUS_CMD_ERROR);
        }
        func_filename = FilenameRef::new(L!("-").to_owned());
        fd = streams.stdin_fd;
    } else {
        match wopen_cloexec(args[optind], OFlag::O_RDONLY, Mode::empty()) {
            Ok(file) => {
                opened_file = file;
            }
            Err(_) => {
                let esc = escape(args[optind]);
                streams.err.append(wgettext_fmt!(
                    "%ls: Error encountered while sourcing file '%ls':\n",
                    cmd,
                    &esc
                ));
                builtin_wperror(cmd, streams);
                return Err(STATUS_CMD_ERROR);
            }
        };

        fd = opened_file.as_raw_fd();

        func_filename = FilenameRef::new(args[optind].to_owned());
    }

    assert!(fd >= 0, "Should have a valid fd");

    let sb = parser.push_block(Block::source_block(func_filename.clone()));
    let _filename_push = scoped_push_replacer(
        |new_value| std::mem::replace(&mut parser.libdata_mut().current_filename, new_value),
        Some(func_filename.clone()),
    );

    // Construct argv for the sourced file from our remaining args.
    // This is slightly subtle. If this is a bare `source` with no args then `argv + optind` already
    // points to the end of argv. Otherwise we want to skip the file name to get to the args if any.
    let remaining_args = &args[optind + if argc == optind { 0 } else { 1 }..];
    let argv_list = remaining_args.iter().map(|&arg| arg.to_owned()).collect();
    parser.vars().set_argv(argv_list);

    let retval = reader_read(parser, fd, streams.io_chain);

    parser.pop_block(sb);

    match retval {
        Ok(_) => BuiltinResult::from_dynamic(parser.get_last_status()),
        Err(err) => {
            let esc = escape(&func_filename);
            streams.err.append(wgettext_fmt!(
                "%ls: Error while reading file '%ls'\n",
                cmd,
                if esc == "-" { L!("<stdin>") } else { &esc }
            ));
            Err(err)
        }
    }
}
