// Implementation of the cd builtin.

use super::prelude::*;
use crate::{
    env::{EnvMode, Environment as _},
    err_fmt, err_raw, err_str,
    fds::{BEST_O_SEARCH, wopen_dir},
    parser::ParserEnvSetMode,
    path::path_apply_cdpath,
    wutil::{normalize_path, wreadlink, wrealpath},
};
use nix::{errno::Errno, unistd::fchdir};

// cd is highlighted specially in src/highlight/highlight.rs - new options also need to be added
// there
const SHORT_OPTIONS: &wstr = L!("hLP");
const LONG_OPTIONS: &[WOption] = &[
    wopt(L!("help"), ArgType::NoArgument, 'h'),
    wopt(L!("no-dereference"), ArgType::NoArgument, 'L'),
    wopt(L!("dereference"), ArgType::NoArgument, 'P'),
];

fn try_chdir(dirs: Vec<WString>, parser: &mut Parser, deref_symlink: bool) -> bool {
    for dir in dirs {
        let norm_dir = normalize_path(&dir, true);
        if let Ok(fd) = wopen_dir(&norm_dir, BEST_O_SEARCH) {
            if fchdir(&fd).is_ok() {
                parser.libdata_mut().cwd_fd = Some(std::sync::Arc::new(fd));

                let mut new_pwd = norm_dir;
                if deref_symlink {
                    if let Some(real_dir) = wrealpath(&new_pwd) {
                        new_pwd = real_dir;
                    }
                }
                parser.set_var_and_fire(
                    L!("PWD"),
                    ParserEnvSetMode::new(EnvMode::EXPORT | EnvMode::GLOBAL),
                    vec![new_pwd],
                );
                return true;
            }
        }
    }
    false
}

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

    // Validate arguments.
    let argc = args.len();
    let mut deref_symlink = false;
    let mut w = WGetopter::new(SHORT_OPTIONS, LONG_OPTIONS, args);
    while let Some(opt) = w.next_opt() {
        match opt {
            'L' => deref_symlink = false,
            'P' => deref_symlink = true,
            'h' => {
                builtin_print_help(parser, streams, cmd);
                return Ok(SUCCESS);
            }
            ';' => {
                builtin_unexpected_argument(parser, streams, cmd, args[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, args[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            _ => panic!("unexpected option {}", opt),
        }
    }

    let optind = w.wopt_index;
    let non_option_argc = argc - optind;
    if non_option_argc > 1 {
        err_fmt!(Error::UNEXP_ARG_COUNT, 1, non_option_argc)
            .cmd(cmd)
            .finish(streams);
        return Err(STATUS_INVALID_ARGS);
    }

    let vars = parser.vars();
    let tmpstr;

    let dir_in: &wstr = if non_option_argc == 1 {
        args[optind]
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

    // If given -P, deref PWD before applying any CDPATH.
    let mut pwd = vars.get_pwd_slash();
    if deref_symlink {
        if let Some(mut real_pwd) = wrealpath(&pwd) {
            if !real_pwd.ends_with('/') {
                real_pwd.push('/');
            }
            pwd = real_pwd;
        }
    }

    let is_relative_to_cwd = [L!("."), L!("..")].contains(&dir_in)
        || dir_in.starts_with(L!("./"))
        || dir_in.starts_with(L!("../"));
    if is_relative_to_cwd && wopen_dir(&normalize_path(&pwd, true), BEST_O_SEARCH).is_err() {
        if let Some(mut real_pwd) = wrealpath(L!(".")) {
            if !real_pwd.ends_with('/') {
                real_pwd.push('/');
            }
            pwd = real_pwd;
        }
    }

    // Walk over candidate directories, respecting CDPATH.
    let dirs = path_apply_cdpath(dir_in, &pwd, vars);
    assert!(
        !dirs.is_empty(),
        "dirs should always contains a least an abs path, or a rel path, or '<PWD>/...'"
    );

    // Because we may have multiple candidate directories, we have to be choosy about
    // which error to report.
    let mut best_errno: Option<Errno> = None;

    // If the error is a broken symlink, keep track of that here.
    // This is a tuple of the path to the symlink, and what the symlink was trying to point to.
    // Note if broken_symlink is set, so is best_errno; however it may not be ENOENT.
    let mut broken_symlink: Option<(WString, WString)> = None;

    for dir in dirs {
        let norm_dir = normalize_path(&dir, true);
        // Try opening the directory and invoking fchdir().
        let res = wopen_dir(&norm_dir, BEST_O_SEARCH).and_then(|fd| {
            fchdir(&fd)?;
            Ok(fd)
        });

        let _fd = match res {
            Ok(fd) => fd,
            Err(err) => {
                // Some errors we skip and only report if nothing worked.
                // ENOENT in particular is very low priority
                // - if in another directory there was a *file* by the correct name
                // we prefer *that* error because it's more specific
                if err == Errno::ENOENT {
                    // If this is the first broken symlink, record it.
                    if broken_symlink.is_none() {
                        if let Some(symlink_dest) = wreadlink(&norm_dir) {
                            broken_symlink = Some((norm_dir, symlink_dest));
                        }
                    }
                    if best_errno.is_none() {
                        best_errno = Some(err);
                    }
                    continue;
                } else if err == Errno::ENOTDIR {
                    best_errno = Some(err);
                    continue;
                }
                // Hard error - stop the search.
                // This most likely indicates that the user tried to cd into a directory that exists, but the cd failed.
                // We don't keep trying further directories in CDPATH: this directory is probably what the user intended and
                // an error is better than a worse directory choice.
                best_errno = Some(err);
                break;
            }
        };

        if try_chdir(vec![dir], parser, deref_symlink) {
            return Ok(SUCCESS);
        }
    }

    // If we got here, we failed to cd to any of the directories.
    // Report the best error, which must exist.
    let best_errno = best_errno.expect("best_errno should be set");
    let mut err = if best_errno == Errno::ENOTDIR {
        err_fmt!("'%s' is not a directory", dir_in)
    } else if let Some((symlink, symlink_dest)) = &broken_symlink {
        err_fmt!(
            "'%s' is a broken symbolic link to '%s'",
            symlink,
            symlink_dest
        )
    } else if best_errno == Errno::ELOOP {
        err_fmt!("Too many levels of symbolic links: '%s'", dir_in)
    } else if best_errno == Errno::ENOENT {
        err_fmt!(DIR_DOES_NOT_EXIST, dir_in)
    } else if best_errno == Errno::EACCES || best_errno == Errno::EPERM {
        err_fmt!("Permission denied: '%s'", dir_in)
    } else {
        Errno::set(best_errno);
        err_raw!(builtin_strerror()).cmd(L!("cd")).finish(streams);
        err_fmt!("Unknown error trying to locate directory '%s'", dir_in)
    };

    if !parser.is_interactive() {
        err = err.stacktrace(parser);
    }
    err.cmd(cmd).finish(streams);

    Err(STATUS_CMD_ERROR)
}
