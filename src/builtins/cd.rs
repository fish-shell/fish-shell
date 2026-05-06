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
use errno::Errno;
use libc::{EACCES, ELOOP, ENOENT, ENOTDIR, EPERM};
use nix::unistd::fchdir;

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
            fchdir(&fd)
                .map_err(|_|
                // nix::Result::Err contains nix::errno::Errno, which does not offer an API for
                // converting to a raw int.
                errno::errno().0)
                .map(|()| fd)
        });

        let _fd = match res {
            Ok(fd) => fd,
            Err(err) => {
                // Some errors we skip and only report if nothing worked.
                // ENOENT in particular is very low priority
                // - if in another directory there was a *file* by the correct name
                // we prefer *that* error because it's more specific
                if err == ENOENT {
                    let tmp = wreadlink(&norm_dir);
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
        };

        if try_chdir(vec![dir], parser, deref_symlink) {
            return Ok(SUCCESS);
        }
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
