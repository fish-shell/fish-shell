// Implementation of the cd builtin.

use super::prelude::*;
use crate::{
    env::{EnvMode, Environment as _},
    err_fmt, err_raw, err_str,
    fds::{BEST_O_SEARCH, wopen_dir},
    parser::ParserEnvSetMode,
    path::path_apply_cdpath,
    wutil::{normalize_path, wreadlink},
};
use errno::Errno;
use libc::{EACCES, ELOOP, ENOENT, ENOTDIR, EPERM};
use nix::unistd::fchdir;

// The cd builtin. Changes the current directory to the one specified or to $HOME if none is
// specified. The directory can be relative to any directory in the CDPATH variable.
pub fn cd(parser: &mut Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    localizable_consts! {
        DIR_DOES_NOT_EXIST
        "The directory '%s' does not exist"
    }

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
                err_str!("Could not find home directory")
                    .cmd(cmd)
                    .finish(streams);
                return Err(STATUS_CMD_ERROR);
            }
        }
    };

    // Stop `cd ""` from crashing
    if dir_in.is_empty() {
        let mut err = err_fmt!("Empty directory '%s' does not exist", dir_in).cmd(cmd);
        if !parser.is_interactive() {
            err = err.stacktrace(parser);
        }
        err.finish(streams);
        return Err(STATUS_CMD_ERROR);
    }

    let pwd = vars.get_pwd_slash();

    let dirs = path_apply_cdpath(dir_in, &pwd, vars);
    assert!(
        !dirs.is_empty(),
        "dirs should always contains a least an abs path, or a rel path, or '<PWD>/...'"
    );

    let mut best_errno = 0;
    let mut broken_symlink = WString::new();
    let mut broken_symlink_target = WString::new();

    for dir in dirs {
        let norm_dir = normalize_path(&dir, true);

        errno::set_errno(Errno(0));

        let res = wopen_dir(&norm_dir, BEST_O_SEARCH).map_err(|err| err as i32);

        let res = res.and_then(|fd| {
            fchdir(&fd).map_err(|_|
                // nix::Result::Err contains nix::errno::Errno, which does not offer an API for
                // converting to a raw int.
                errno::errno().0)
        });

        if let Err(err) = res {
            // Some errors we skip and only report if nothing worked.
            // ENOENT in particular is very low priority
            // - if in another directory there was a *file* by the correct name
            // we prefer *that* error because it's more specific
            if err == ENOENT {
                let tmp = wreadlink(&norm_dir);
                // clippy doesn't like this is_some/unwrap pair, but using if let is harder to read IMO
                // TODO: if-let-chains
                if let Some(tmp) = tmp.filter(|_| broken_symlink.is_empty()) {
                    broken_symlink = norm_dir;
                    broken_symlink_target = tmp;
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

        parser.set_var_and_fire(
            L!("PWD"),
            ParserEnvSetMode::new(EnvMode::EXPORT | EnvMode::GLOBAL),
            vec![norm_dir],
        );
        return Ok(SUCCESS);
    }

    let mut err = if best_errno == ENOTDIR {
        err_fmt!("'%s' is not a directory", dir_in)
    } else if !broken_symlink.is_empty() {
        err_fmt!(
            "'%s' is a broken symbolic link to '%s'",
            broken_symlink,
            broken_symlink_target
        )
    } else if best_errno == ELOOP {
        err_fmt!("Too many levels of symbolic links: '%s'", dir_in)
    } else if best_errno == ENOENT {
        err_fmt!(DIR_DOES_NOT_EXIST, dir_in)
    } else if best_errno == EACCES || best_errno == EPERM {
        err_fmt!("Permission denied: '%s'", dir_in)
    } else {
        errno::set_errno(Errno(best_errno));
        err_raw!(builtin_strerror()).cmd(L!("cd")).finish(streams);
        err_fmt!("Unknown error trying to locate directory '%s'", dir_in)
    };

    if !parser.is_interactive() {
        err = err.stacktrace(parser);
    }
    err.cmd(cmd).finish(streams);

    Err(STATUS_CMD_ERROR)
}
