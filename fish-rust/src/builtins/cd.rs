// Implementation of the cd builtin.

use super::prelude::*;
use crate::{
    env::{EnvMode, Environment},
    fds::{wopen_cloexec, AutoCloseFd},
    path::path_apply_cdpath,
    set_errno,
    wutil::{normalize_path, wperror, wreadlink},
};
use libc::{fchdir, O_RDONLY};
use nix::errno::Errno;
use std::sync::Arc;

// The cd builtin. Changes the current directory to the one specified or to $HOME if none is
// specified. The directory can be relative to any directory in the CDPATH variable.
pub fn cd(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> Option<c_int> {
    let cmd = args[0];

    let opts = match HelpOnlyCmdOpts::parse(args, parser, streams) {
        Ok(opts) => opts,
        Err(err @ Some(_)) if err != STATUS_CMD_OK => return err,
        Err(err) => panic!("Illogical exit code from parse_options(): {err:?}"),
    };

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
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
                    .append(wgettext_fmt!("%ls: Could not find home directory\n", cmd));
                return STATUS_CMD_ERROR;
            }
        }
    };

    // Stop `cd ""` from crashing
    if dir_in.is_empty() {
        streams.err.append(wgettext_fmt!(
            "%ls: Empty directory '%ls' does not exist\n",
            cmd,
            dir_in
        ));
        if !parser.is_interactive() {
            streams.err.append(parser.current_line());
        };
        return STATUS_CMD_ERROR;
    }

    let pwd = vars.get_pwd_slash();

    let dirs = path_apply_cdpath(dir_in, &pwd, vars);
    if dirs.is_empty() {
        streams.err.append(wgettext_fmt!(
            "%ls: The directory '%ls' does not exist\n",
            cmd,
            dir_in
        ));

        if !parser.is_interactive() {
            streams.err.append(parser.current_line());
        }

        return STATUS_CMD_ERROR;
    }

    let mut best_errno = Errno::from_i32(0);
    let mut broken_symlink = WString::new();
    let mut broken_symlink_target = WString::new();

    for dir in dirs {
        let norm_dir = normalize_path(&dir, true);

        Errno::clear();

        // We need to keep around the fd for this directory, in the parser.
        let dir_fd = Arc::new(AutoCloseFd::new(wopen_cloexec(&norm_dir, O_RDONLY, 0)));

        if !(dir_fd.is_valid() && unsafe { fchdir(dir_fd.fd()) } == 0) {
            // Some errors we skip and only report if nothing worked.
            // ENOENT in particular is very low priority
            // - if in another directory there was a *file* by the correct name
            // we prefer *that* error because it's more specific
            match Errno::last() {
                Errno::ENOENT => {
                    let tmp = wreadlink(&norm_dir);
                    // clippy doesn't like this is_some/unwrap pair, but using if let is harder to read IMO
                    #[allow(clippy::unnecessary_unwrap)]
                    if broken_symlink.is_empty() && tmp.is_some() {
                        broken_symlink = norm_dir;
                        broken_symlink_target = tmp.unwrap();
                    } else if best_errno == Errno::from_i32(0) {
                        best_errno = Errno::last();
                    }
                    continue;
                }
                Errno::ENOTDIR => {
                    best_errno = Errno::ENOTDIR;
                    continue;
                }
                _ => {}
            }

            best_errno = Errno::last();
            break;
        }

        // Stash the fd for the cwd in the parser.
        parser.libdata_mut().cwd_fd = Some(dir_fd);

        parser.set_var_and_fire(L!("PWD"), EnvMode::EXPORT | EnvMode::GLOBAL, vec![norm_dir]);
        return STATUS_CMD_OK;
    }

    match best_errno {
        Errno::ENOTDIR => {
            streams.err.append(wgettext_fmt!(
                "%ls: '%ls' is not a directory\n",
                cmd,
                dir_in
            ));
        }
        _ if !broken_symlink.is_empty() => {
            streams.err.append(wgettext_fmt!(
                "%ls: '%ls' is a broken symbolic link to '%ls'\n",
                cmd,
                broken_symlink,
                broken_symlink_target
            ));
        }
        Errno::ELOOP => {
            streams.err.append(wgettext_fmt!(
                "%ls: Too many levels of symbolic links: '%ls'\n",
                cmd,
                dir_in
            ));
        }
        Errno::ENOENT => {
            streams.err.append(wgettext_fmt!(
                "%ls: The directory '%ls' does not exist\n",
                cmd,
                dir_in
            ));
        }
        Errno::EACCES | Errno::EPERM => {
            streams.err.append(wgettext_fmt!(
                "%ls: Permission denied: '%ls'\n",
                cmd,
                dir_in
            ));
        }
        _ => {
            set_errno(best_errno);
            wperror(L!("cd"));
            streams.err.append(wgettext_fmt!(
                "%ls: Unknown error trying to locate directory '%ls'\n",
                cmd,
                dir_in
            ));
        }
    }

    if !parser.is_interactive() {
        streams.err.append(parser.current_line());
    }

    return STATUS_CMD_ERROR;
}
