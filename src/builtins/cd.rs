// Implementation of the cd builtin.

use super::prelude::*;
use crate::{
    env::{EnvMode, Environment},
    fds::{BEST_O_SEARCH, wopen_dir},
    parser::ParserEnvSetMode,
    path::path_apply_cdpath,
    wutil::{normalize_path, wperror, wreadlink},
};
use errno::Errno;
use libc::{EACCES, ELOOP, ENOENT, ENOTDIR, EPERM};
use nix::unistd::fchdir;
use std::sync::Arc;

// The cd builtin. Changes the current directory to the one specified or to $HOME if none is
// specified. The directory can be relative to any directory in the CDPATH variable.
pub fn cd(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let Some(&cmd) = args.first() else {
        return Err(STATUS_INVALID_ARGS);
    };

    let opts = HelpOnlyCmdOpts::parse(args, parser, streams)?;

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return Ok(SUCCESS);
    }

    let vars = parser.vars();
    let tmpstr;

    let dir_in: &wstr = if args.len() > opts.optind {
        args[opts.optind]
    } else {
        match vars.get_unless_empty(L!("HOME")) {
            Some(v) => {
                tmpstr = v.as_string();
                &tmpstr
            }
            None => {
                streams
                    .err
                    .append(&wgettext_fmt!("%s: Could not find home directory\n", cmd));
                return Err(STATUS_CMD_ERROR);
            }
        }
    };

    // Stop `cd ""` from crashing
    if dir_in.is_empty() {
        streams.err.append(&wgettext_fmt!(
            "%s: Empty directory '%s' does not exist\n",
            cmd,
            dir_in
        ));
        if !parser.is_interactive() {
            streams.err.append(&parser.current_line());
        }
        return Err(STATUS_CMD_ERROR);
    }

    let pwd = vars.get_pwd_slash();

    let dirs = path_apply_cdpath(dir_in, &pwd, vars);
    if dirs.is_empty() {
        streams.err.append(&wgettext_fmt!(
            "%s: The directory '%s' does not exist\n",
            cmd,
            dir_in
        ));

        if !parser.is_interactive() {
            streams.err.append(&parser.current_line());
        }

        return Err(STATUS_CMD_ERROR);
    }

    let mut best_errno = 0;
    let mut broken_symlink = WString::new();
    let mut broken_symlink_target = WString::new();

    for dir in dirs {
        let norm_dir = normalize_path(&dir, true);

        errno::set_errno(Errno(0));

        let res = wopen_dir(&norm_dir, BEST_O_SEARCH).map_err(|err| err as i32);

        let res = res.and_then(|fd| {
            match fchdir(&fd) {
                Ok(()) => Ok(fd),
                // nix::Result::Err contains nix::errno::Errno, which does not offer an API for
                // converting to a raw int.
                Err(_) => Err(errno::errno().0),
            }
        });

        let fd = match res {
            Ok(fd) => fd,
            Err(err) => {
                // Some errors we skip and only report if nothing worked.
                // ENOENT in particular is very low priority
                // - if in another directory there was a *file* by the correct name
                // we prefer *that* error because it's more specific
                if err == ENOENT {
                    let tmp = wreadlink(&norm_dir);
                    // clippy doesn't like this is_some/unwrap pair, but using if let is harder to read IMO
                    #[allow(clippy::unnecessary_unwrap)]
                    if broken_symlink.is_empty() && tmp.is_some() {
                        broken_symlink = norm_dir;
                        broken_symlink_target = tmp.unwrap();
                    } else if best_errno == 0 {
                        best_errno = errno::errno().0;
                    }
                    continue;
                } else if err == ENOTDIR {
                    best_errno = err;
                    continue;
                }
                best_errno = err;
                break;
            }
        };

        // We need to keep around the fd for this directory, in the parser.
        let dir_fd = Arc::new(fd);

        // Stash the fd for the cwd in the parser.
        parser.libdata_mut().cwd_fd = Some(dir_fd);

        parser.set_var_and_fire(
            L!("PWD"),
            ParserEnvSetMode::new(EnvMode::EXPORT | EnvMode::GLOBAL),
            vec![norm_dir],
        );
        return Ok(SUCCESS);
    }

    if best_errno == ENOTDIR {
        streams
            .err
            .append(&wgettext_fmt!("%s: '%s' is not a directory\n", cmd, dir_in));
    } else if !broken_symlink.is_empty() {
        streams.err.append(&wgettext_fmt!(
            "%s: '%s' is a broken symbolic link to '%s'\n",
            cmd,
            broken_symlink,
            broken_symlink_target
        ));
    } else if best_errno == ELOOP {
        streams.err.append(&wgettext_fmt!(
            "%s: Too many levels of symbolic links: '%s'\n",
            cmd,
            dir_in
        ));
    } else if best_errno == ENOENT {
        streams.err.append(&wgettext_fmt!(
            "%s: The directory '%s' does not exist\n",
            cmd,
            dir_in
        ));
    } else if best_errno == EACCES || best_errno == EPERM {
        streams
            .err
            .append(&wgettext_fmt!("%s: Permission denied: '%s'\n", cmd, dir_in));
    } else {
        errno::set_errno(Errno(best_errno));
        wperror(L!("cd"));
        streams.err.append(&wgettext_fmt!(
            "%s: Unknown error trying to locate directory '%s'\n",
            cmd,
            dir_in
        ));
    }

    if !parser.is_interactive() {
        streams.err.append(&parser.current_line());
    }

    Err(STATUS_CMD_ERROR)
}
