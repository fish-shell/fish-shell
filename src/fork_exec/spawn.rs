//! Wrappers around posix_spawn.

use super::blocked_signals_for_job;
use crate::proc::Job;
use crate::redirection::Dup2List;
use crate::signal::signals_to_default;
use crate::{exec::is_thompson_shell_script, libc::_PATH_BSHELL};
use errno::Errno;
use libc::{c_char, posix_spawn_file_actions_t, posix_spawnattr_t};
use std::ffi::{CStr, CString};
use std::mem::MaybeUninit;
use std::sync::atomic::Ordering;

// The posix_spawn family of functions is unusual in that it returns errno codes directly in the return value, not via errno.
// This converts to an error if nonzero.
fn check_fail(res: i32) -> Result<(), Errno> {
    match res {
        0 => Ok(()),
        err => Err(Errno(err)),
    }
}

/// Basic RAII wrapper around posix_spawnattr_t.
struct Attr(posix_spawnattr_t);

impl Attr {
    fn new() -> Result<Self, Errno> {
        unsafe {
            let mut attr = MaybeUninit::uninit();
            check_fail(libc::posix_spawnattr_init(attr.as_mut_ptr()))?;
            Ok(Self(attr.assume_init()))
        }
    }

    fn set_flags(&mut self, flags: libc::c_short) -> Result<(), Errno> {
        unsafe { check_fail(libc::posix_spawnattr_setflags(&mut self.0, flags)) }
    }

    fn set_pgroup(&mut self, pgroup: libc::pid_t) -> Result<(), Errno> {
        unsafe { check_fail(libc::posix_spawnattr_setpgroup(&mut self.0, pgroup)) }
    }

    fn set_sigdefault(&mut self, sigs: &libc::sigset_t) -> Result<(), Errno> {
        unsafe { check_fail(libc::posix_spawnattr_setsigdefault(&mut self.0, sigs)) }
    }

    fn set_sigmask(&mut self, sigs: &libc::sigset_t) -> Result<(), Errno> {
        unsafe { check_fail(libc::posix_spawnattr_setsigmask(&mut self.0, sigs)) }
    }
}

impl Drop for Attr {
    fn drop(&mut self) {
        unsafe {
            let _ = libc::posix_spawnattr_destroy(&mut self.0);
        }
    }
}

/// Basic RAII wrapper around posix_spawn_file_actions_t;
struct FileActions(posix_spawn_file_actions_t);

impl FileActions {
    fn new() -> Result<Self, Errno> {
        unsafe {
            let mut actions = MaybeUninit::uninit();
            check_fail(libc::posix_spawn_file_actions_init(actions.as_mut_ptr()))?;
            Ok(Self(actions.assume_init()))
        }
    }

    fn add_close(&mut self, fd: libc::c_int) -> Result<(), Errno> {
        unsafe { check_fail(libc::posix_spawn_file_actions_addclose(&mut self.0, fd)) }
    }

    fn add_dup2(&mut self, src: libc::c_int, target: libc::c_int) -> Result<(), Errno> {
        unsafe {
            check_fail(libc::posix_spawn_file_actions_adddup2(
                &mut self.0,
                src,
                target,
            ))
        }
    }
}

impl Drop for FileActions {
    fn drop(&mut self) {
        unsafe {
            let _ = libc::posix_spawn_file_actions_destroy(&mut self.0);
        }
    }
}

/// A RAII type which wraps up posix_spawn's data structures.
pub struct PosixSpawner {
    attr: Attr,
    actions: FileActions,
}

impl PosixSpawner {
    pub fn new(j: &Job, dup2s: &Dup2List) -> Result<PosixSpawner, Errno> {
        let mut attr = Attr::new()?;
        let mut actions = FileActions::new()?;

        // desired_pgid tracks the pgroup for the process. If it is none, the pgroup is left unchanged.
        // If it is zero, create a new pgroup from the pid. If it is >0, join that pgroup.
        let desired_pgid = if let Some(pgid) = j.get_pgid() {
            Some(pgid.get())
        } else if j.processes()[0].leads_pgrp {
            Some(0)
        } else {
            None
        };

        // Set our flags.
        let mut flags: i32 = 0;
        flags |= libc::POSIX_SPAWN_SETSIGDEF;
        flags |= libc::POSIX_SPAWN_SETSIGMASK;
        if desired_pgid.is_some() {
            flags |= libc::POSIX_SPAWN_SETPGROUP;
        }
        attr.set_flags(flags.try_into().expect("Flags should fit in c_short"))?;

        if let Some(desired_pgid) = desired_pgid {
            attr.set_pgroup(desired_pgid)?;
        }

        // Everybody gets default handlers.
        attr.set_sigdefault(&signals_to_default)?;

        // Reset the sigmask.
        let mut sigmask = MaybeUninit::uninit();
        unsafe { libc::sigemptyset(sigmask.as_mut_ptr()) };
        let mut sigmask = unsafe { sigmask.assume_init() };
        blocked_signals_for_job(j, &mut sigmask);
        attr.set_sigmask(&sigmask)?;

        // Apply our dup2s.
        for act in dup2s.get_actions() {
            if act.target < 0 {
                actions.add_close(act.src)?;
            } else {
                actions.add_dup2(act.src, act.target)?;
            }
        }
        Ok(PosixSpawner { attr, actions })
    }

    // Attempt to spawn a new process.
    pub(crate) fn spawn(
        &mut self,
        cmd: *const c_char,
        argv: *const *mut c_char,
        envp: *const *mut c_char,
    ) -> Result<libc::pid_t, Errno> {
        let mut pid = -1;
        let spawned = check_fail(unsafe {
            libc::posix_spawn(&mut pid, cmd, &self.actions.0, &self.attr.0, argv, envp)
        });
        if spawned.is_ok() {
            return Ok(pid);
        }
        let spawn_err = spawned.unwrap_err();

        // The shebang wasn't introduced until UNIX Seventh Edition, so if
        // the kernel won't run the binary we hand it off to the interpreter
        // after performing a binary safety check, recommended by POSIX: a
        // line needs to exist before the first \0 with a lowercase letter.
        let cmdcstr = unsafe { CStr::from_ptr(cmd) };
        if spawn_err.0 == libc::ENOEXEC && is_thompson_shell_script(cmdcstr) {
            // Create a new argv with /bin/sh prepended.
            let mut argv2 = vec![_PATH_BSHELL.load(Ordering::Relaxed) as *mut c_char];

            // The command to call should use the full path,
            // not what we would pass as argv0.
            let cmd2: CString = CString::new(cmdcstr.to_bytes()).unwrap();
            argv2.push(cmd2.as_ptr() as *mut c_char);
            for i in 1.. {
                let ptr = unsafe { argv.offset(i).read() };
                if ptr.is_null() {
                    break;
                }
                argv2.push(ptr);
            }
            argv2.push(std::ptr::null_mut());
            check_fail(unsafe {
                libc::posix_spawn(
                    &mut pid,
                    _PATH_BSHELL.load(Ordering::Relaxed),
                    &self.actions.0,
                    &self.attr.0,
                    argv2.as_ptr(),
                    envp,
                )
            })?;
            return Ok(pid);
        }
        Err(spawn_err)
    }
}
