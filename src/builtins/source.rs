use super::prelude::*;
use crate::{
    builtins::Error, err_fmt, err_raw, err_str, fds::wopen_cloexec, nix::isatty, parser::Block,
    reader::reader_read, wutil::fstat,
};
use fish_common::{FilenameRef, escape};
use fish_widestring::str2wcstring;
use nix::{fcntl::OFlag, sys::stat::Mode, sys::uio::pread};
use std::os::fd::{AsRawFd as _, BorrowedFd, RawFd};

/// Inspect the shebang of a seekable file for an interpreter that is known to be a shell other
/// than fish. This is only a heuristic, used to improve the error message when a file fails to
/// parse; it must not cause files that would otherwise parse to be rejected.
fn foreign_shell_from_shebang(fd: RawFd) -> Option<&'static str> {
    // Only read regular files. Pipes and terminals are not seekable, and pread() would either
    // fail or consume data that reader_read() still needs.
    let md = fstat(fd).ok()?;
    if !md.is_file() {
        return None;
    }

    let mut buf = [0_u8; 512];
    let amt = pread(unsafe { BorrowedFd::borrow_raw(fd) }, &mut buf, 0).ok()?;
    let contents = &buf[..amt];

    // The shebang is the first line, starting with "#!".
    let line = contents.split(|&b| b == b'\n').next()?;
    let line = line.strip_suffix(b"\r").unwrap_or(line);
    let rest = line.strip_prefix(b"#!")?;
    let text = std::str::from_utf8(rest).ok()?.trim();

    // Parse the interpreter path, unwrapping `env` invocations such as
    // `#!/usr/bin/env bash` or `#!/usr/bin/env -S bash -e`.
    let mut words = text.split_whitespace();
    let mut interp = words.next()?;
    if interp.rsplit('/').next() == Some("env") {
        interp = words.find(|word| !word.starts_with('-') && !word.contains('='))?;
    }
    let name = interp.rsplit('/').next()?;

    match name {
        "bash" => Some("bash"),
        "sh" => Some("sh"),
        "zsh" => Some("zsh"),
        "dash" => Some("dash"),
        "ksh" => Some("ksh"),
        "mksh" => Some("mksh"),
        "ash" => Some("ash"),
        "csh" => Some("csh"),
        "tcsh" => Some("tcsh"),
        _ => None,
    }
}

/// The  source builtin, sometimes called `.`. Evaluates the contents of a file in the current
/// context.
pub fn source(parser: &mut Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let argc = args.len();

    let opts = HelpOnlyCmdOpts::parse(args, parser, streams)?;
    let cmd = match args.first() {
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
    let foreign_shell;
    let optind = opts.optind;

    if argc == optind || args[optind] == "-" {
        if streams.is_stdin_closed() {
            err_str!(Error::STDIN_CLOSED).cmd(cmd).finish(streams);
            return Err(STATUS_CMD_ERROR);
        }
        // Either a bare `source` which means to implicitly read from stdin or an explicit `-`.
        if argc == optind && isatty(streams.stdin_fd()) {
            // Don't implicitly read from the terminal.
            err_str!("missing filename argument or input redirection")
                .cmd(cmd)
                .finish(streams);
            return Err(STATUS_CMD_ERROR);
        }
        func_filename = FilenameRef::new(L!("-").to_owned());
        fd = streams.stdin_fd();
        // stdin may be a pipe or terminal, which is not necessarily seekable.
        foreign_shell = None;
    } else {
        match wopen_cloexec(args[optind], OFlag::O_RDONLY, Mode::empty()) {
            Ok(file) => {
                opened_file = file;
            }
            Err(err) => {
                let esc = escape(args[optind]);
                err_fmt!("Error encountered while sourcing file '%s':", &esc)
                    .append_to_msg('\n')
                    .append_to_msg(&err_raw!(str2wcstring(err.desc())).cmd(cmd).to_string())
                    .cmd(cmd)
                    .finish(streams);

                return Err(STATUS_CMD_ERROR);
            }
        }

        fd = opened_file.as_raw_fd();
        foreign_shell = foreign_shell_from_shebang(fd);

        func_filename = FilenameRef::new(args[optind].to_owned());
    }

    assert!(fd >= 0, "Should have a valid fd");

    let sb = parser.push_block(Block::source_block(func_filename.clone()));
    let _filename_push = parser
        .current_filename
        .scoped_replace(Some(func_filename.clone()));

    // Construct argv for the sourced file from our remaining args.
    // This is slightly subtle. If this is a bare `source` with no args then `argv + optind` already
    // points to the end of argv. Otherwise we want to skip the file name to get to the args if any.
    let remaining_args = &args[optind + if argc == optind { 0 } else { 1 }..];
    let argv_list = remaining_args.iter().map(|&arg| arg.to_owned()).collect();
    parser.vars().set_argv(argv_list, parser.is_repainting());

    let retval = reader_read(parser, fd, streams.io_chain);

    parser.pop_block(sb);

    match retval {
        Ok(_) => BuiltinResult::from_dynamic(parser.last_status()),
        Err(err) => {
            let esc = escape(&func_filename);
            let mut error = err_fmt!(
                "Error while reading file '%s'",
                if esc == "-" { L!("<stdin>") } else { &esc }
            )
            .cmd(cmd);
            if let Some(shell) = foreign_shell {
                error = error.append_to_msg('\n').append_to_msg(&wgettext_fmt!(
                    "This file appears to target %s, not fish. Try running it with %s instead.",
                    shell,
                    shell
                ));
            }
            error.finish(streams);
            Err(err)
        }
    }
}
