use crate::common::PROGRAM_NAME;
use crate::fds::{make_fd_nonblocking, set_cloexec};
use crate::flog::{FLOG, FLOGF};
use crate::universal_notifier::UniversalNotifier;
use crate::wchar::prelude::*;
use libc::{c_char, c_int};
use std::ffi::CString;
use std::os::fd::{BorrowedFd, RawFd};

unsafe extern "C" {
    unsafe fn notify_register_file_descriptor(
        name: *const c_char,
        fd: *mut c_int,
        flags: c_int,
        token: *mut c_int,
    ) -> u32;

    unsafe fn notify_post(name: *const c_char) -> u32;

    unsafe fn notify_cancel(token: c_int) -> c_int;
}

const NOTIFY_STATUS_OK: u32 = 0;
const NOTIFY_TOKEN_INVALID: c_int = -1;

/// A notifier based on notifyd.
pub struct NotifydNotifier {
    // The file descriptor to watch for readability.
    // Note that we should NOT use AutocloseFd, as notify_cancel() takes responsibility for
    // closing it.
    notify_fd: RawFd,
    token: c_int,
    name: CString,
}

impl NotifydNotifier {
    pub fn new() -> Option<Self> {
        // Per notify(3), the user.uid.%d style is only accessible to processes with that uid.
        let program_name = *PROGRAM_NAME.get().unwrap_or(&L!("fish"));
        let local_name = format!(
            "user.uid.{}.{}.uvars",
            unsafe { libc::getuid() },
            program_name
        );
        let name = CString::new(local_name).ok()?;

        let mut notify_fd = -1;
        let mut token = -1;
        let status = unsafe {
            notify_register_file_descriptor(name.as_ptr(), &mut notify_fd, 0, &mut token)
        };
        if status != NOTIFY_STATUS_OK || notify_fd < 0 {
            FLOGF!(
                warning,
                "notify_register_file_descriptor() failed with status %u.",
                status
            );
            FLOG!(
                warning,
                "Universal variable notifications may not be received."
            );
            return None;
        }
        // Mark us for non-blocking reads, and CLO_EXEC.
        let _ = make_fd_nonblocking(notify_fd);
        let _ = set_cloexec(notify_fd, true);

        // Serious hack: notify_fd is likely the read end of a pipe. The other end is owned by
        // libnotify, which does not mark it as CLO_EXEC (it should!). The next fd is probably
        // notify_fd + 1. Do it ourselves. If the implementation changes and some other FD gets
        // marked as CLO_EXEC, that's probably a good thing.
        let _ = set_cloexec(notify_fd + 1, true);

        Some(Self {
            notify_fd,
            token,
            name,
        })
    }
}

impl Drop for NotifydNotifier {
    fn drop(&mut self) {
        if self.token != NOTIFY_TOKEN_INVALID {
            // Note this closes notify_fd.
            unsafe {
                notify_cancel(self.token);
            }
        }
    }
}

impl UniversalNotifier for NotifydNotifier {
    fn post_notification(&self) {
        FLOG!(uvar_notifier, "posting notification");
        let status = unsafe { notify_post(self.name.as_ptr()) };
        if status != NOTIFY_STATUS_OK {
            FLOGF!(
                warning,
                "notify_post() failed with status %u. Uvar notifications may not be sent.",
                status,
            );
        }
    }

    fn notification_fd(&self) -> Option<RawFd> {
        Some(self.notify_fd)
    }

    fn notification_fd_became_readable(&self, fd: RawFd) -> bool {
        // notifyd notifications come in as 32 bit values. We don't care about the value. We set
        // ourselves as non-blocking, so just read until we can't read any more.
        assert!(fd == self.notify_fd);
        let mut read_something = false;
        let mut buff: [u8; 64] = [0; 64];
        loop {
            let res =
                nix::unistd::read(unsafe { BorrowedFd::borrow_raw(self.notify_fd) }, &mut buff);

            if let Ok(amt_read) = res {
                read_something |= amt_read > 0;
            }

            match res {
                Ok(amt_read) if amt_read != buff.len() => break,
                Err(_) => break,
                _ => continue,
            }
        }
        FLOGF!(
            uvar_notifier,
            "notify fd %s readable",
            if read_something { "was" } else { "was not" },
        );
        read_something
    }
}

#[test]
fn test_notifyd_notifiers() {
    let mut notifiers = Vec::new();
    for _ in 0..16 {
        notifiers.push(NotifydNotifier::new().expect("failed to create notifier"));
    }
    let notifiers = notifiers
        .iter()
        .map(|n| n as &dyn UniversalNotifier)
        .collect::<Vec<_>>();
    super::test_helpers::test_notifiers(&notifiers, None);
}
