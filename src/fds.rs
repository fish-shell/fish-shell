use crate::common::wcs2zstring;
use crate::flog::flog;
use crate::prelude::*;
use crate::signal::signal_check_cancel;
use crate::wutil::perror;
use cfg_if::cfg_if;
use libc::{EINTR, F_GETFD, F_GETFL, F_SETFD, F_SETFL, FD_CLOEXEC, O_NONBLOCK, c_int};
use nix::fcntl::FcntlArg;
use nix::fcntl::OFlag;
use std::ffi::CStr;
use std::fs::File;
use std::io;
use std::mem::ManuallyDrop;
use std::ops::{Deref, DerefMut};
use std::os::fd::{AsRawFd, FromRawFd, IntoRawFd, OwnedFd};
use std::os::unix::prelude::*;

localizable_consts!(
    pub PIPE_ERROR
    "An error occurred while setting up pipe"
);

/// The first "high fd", which is considered outside the range of valid user-specified redirections
/// (like >&5).
pub const FIRST_HIGH_FD: RawFd = 10;

/// A sentinel value indicating no timeout.
pub const NO_TIMEOUT: u64 = u64::MAX;

/// Helper type returned from make_autoclose_pipes.
pub struct AutoClosePipes {
    /// Read end of the pipe.
    pub read: OwnedFd,

    /// Write end of the pipe.
    pub write: OwnedFd,
}

/// Construct a pair of connected pipes, set to close-on-exec.
/// Return None on fd exhaustion.
pub fn make_autoclose_pipes() -> nix::Result<AutoClosePipes> {
    #[allow(unused_mut, unused_assignments)]
    let mut already_cloexec = false;
    cfg_if!(
        if #[cfg(have_pipe2)] {
            let pipes = match nix::unistd::pipe2(OFlag::O_CLOEXEC) {
                Ok(pipes) => {
                    already_cloexec = true;
                    pipes
                }
                Err(err) => {
                    flog!(warning, PIPE_ERROR.localize());
                    perror("pipe2");
                    return Err(err);
                }
            };
        } else {
            let pipes = match nix::unistd::pipe() {
                Ok(pipes) => pipes,
                Err(err) => {
                    flog!(warning, PIPE_ERROR.localize());
                    perror("pipe");
                    return Err(err);
                }
            };
        }
    );

    let readp = pipes.0;
    let writep = pipes.1;

    // Ensure our fds are out of the user range.
    let readp = heightenize_fd(readp, already_cloexec)?;
    let writep = heightenize_fd(writep, already_cloexec)?;

    Ok(AutoClosePipes {
        read: readp,
        write: writep,
    })
}

/// If the given fd is in the "user range", move it to a new fd in the "high range".
/// zsh calls this movefd().
/// `input_has_cloexec` describes whether the input has CLOEXEC already set, so we can avoid
/// setting it again.
/// Return the fd, which always has CLOEXEC set; or an invalid fd on failure, in
/// which case an error will have been printed, and the input fd closed.
fn heightenize_fd(fd: OwnedFd, input_has_cloexec: bool) -> nix::Result<OwnedFd> {
    let raw_fd = fd.as_raw_fd();

    if raw_fd >= FIRST_HIGH_FD {
        if !input_has_cloexec {
            set_cloexec(raw_fd, true);
        }
        return Ok(fd);
    }

    // Here we are asking the kernel to give us a cloexec fd.
    let newfd = match nix::fcntl::fcntl(&fd, FcntlArg::F_DUPFD_CLOEXEC(FIRST_HIGH_FD)) {
        Ok(newfd) => newfd,
        Err(err) => {
            perror("fcntl");
            return Err(err);
        }
    };

    Ok(unsafe { OwnedFd::from_raw_fd(newfd) })
}

/// Sets CLO_EXEC on a given fd according to the value of `should_set`.
pub fn set_cloexec(fd: RawFd, should_set: bool /* = true */) -> c_int {
    // Note we don't want to overwrite existing flags like O_NONBLOCK which may be set. So fetch the
    // existing flags and modify them.
    let flags = unsafe { libc::fcntl(fd, F_GETFD, 0) };
    if flags < 0 {
        return -1;
    }
    let mut new_flags = flags;
    if should_set {
        new_flags |= FD_CLOEXEC;
    } else {
        new_flags &= !FD_CLOEXEC;
    }
    if flags == new_flags {
        0
    } else {
        unsafe { libc::fcntl(fd, F_SETFD, new_flags) }
    }
}

/// Wide character version of open() that also sets the close-on-exec flag (atomically when
/// possible).
pub fn wopen_cloexec(
    pathname: &wstr,
    flags: OFlag,
    mode: nix::sys::stat::Mode,
) -> nix::Result<File> {
    open_cloexec(wcs2zstring(pathname).as_c_str(), flags, mode)
}

/// Narrow versions of wopen_cloexec().
pub fn open_cloexec(path: &CStr, flags: OFlag, mode: nix::sys::stat::Mode) -> nix::Result<File> {
    // Port note: the C++ version of this function had a fallback for platforms where
    // O_CLOEXEC is not supported, using fcntl. In 2023, this is no longer needed.
    // We retry this in case of signals,
    // if we get EINTR and it's not a SIGINT, we continue.
    // If it is that's our cancel signal, so we abort.
    loop {
        let ret = nix::fcntl::open(path, flags | OFlag::O_CLOEXEC, mode);
        let ret = ret.map(File::from);
        match ret {
            Ok(file) => {
                return Ok(file);
            }
            Err(err) => {
                if err != nix::Error::EINTR || signal_check_cancel() != 0 {
                    return ret;
                }
            }
        }
    }
}

/// POSIX specifies an open option O_SEARCH for opening directories for later
/// `fchdir` or `openat`, not for `readdir`. The read permission is not checked,
/// and the x permission is checked on opening. Not all platforms have this,
/// so we fall back to O_PATH or O_RDONLY according to the platform.
pub use o_search::BEST_O_SEARCH;

mod o_search {
    use super::OFlag;
    /// On macOS we have O_SEARCH, which is defined as O_EXEC | O_DIRECTORY,
    /// where O_EXEC is 0x40000000. This is only on macOS 12.0+ or later; however
    /// prior macOS versions ignores O_EXEC so it is treated the same as O_RDONLY.
    #[cfg(apple)]
    pub const BEST_O_SEARCH: OFlag = OFlag::from_bits_truncate(libc::O_DIRECTORY | 0x40000000);

    /// On FreeBSD, we have O_SEARCH = 0x00040000.
    #[cfg(target_os = "freebsd")]
    pub const BEST_O_SEARCH: OFlag = OFlag::from_bits_truncate(0x00040000);

    /// On Linux we can use O_PATH, it has nearly the same semantics. we can use the fd for openat / fchdir, with only requiring
    /// x permission on the directory.
    #[cfg(any(target_os = "linux", target_os = "android"))]
    pub const BEST_O_SEARCH: OFlag = OFlag::from_bits_truncate(libc::O_PATH);

    /// Fall back to O_RDONLY.
    #[cfg(not(any(
        apple,
        target_os = "linux",
        target_os = "android",
        target_os = "freebsd",
    )))]
    pub const BEST_O_SEARCH: OFlag = OFlag::O_RDONLY;
}

/// Wide character version of open_dir() that also sets the close-on-exec flag (atomically when
/// possible).
pub fn wopen_dir(pathname: &wstr, flags: OFlag) -> nix::Result<OwnedFd> {
    open_dir(wcs2zstring(pathname).as_c_str(), flags)
}

/// Narrow version of wopen_dir().
pub fn open_dir(path: &CStr, flags: OFlag) -> nix::Result<OwnedFd> {
    let mode = nix::sys::stat::Mode::empty();
    open_cloexec(path, flags | OFlag::O_DIRECTORY, mode).map(OwnedFd::from)
}

/// Close a file descriptor `fd`, retrying on EINTR.
pub fn exec_close(fd: RawFd) {
    assert!(fd >= 0, "Invalid fd");
    while unsafe { libc::close(fd) } == -1 {
        if errno::errno().0 != EINTR {
            perror("close");
            break;
        }
    }
}

/// Mark an fd as nonblocking
pub fn make_fd_nonblocking(fd: RawFd) -> Result<(), io::Error> {
    let flags = unsafe { libc::fcntl(fd, F_GETFL, 0) };
    let nonblocking = (flags & O_NONBLOCK) == O_NONBLOCK;
    if !nonblocking {
        match unsafe { libc::fcntl(fd, F_SETFL, flags | O_NONBLOCK) } {
            -1 => return Err(io::Error::last_os_error()),
            _ => return Ok(()),
        };
    }
    Ok(())
}

/// Mark an fd as blocking
pub fn make_fd_blocking(fd: RawFd) -> Result<(), io::Error> {
    let flags = unsafe { libc::fcntl(fd, F_GETFL, 0) };
    let nonblocking = (flags & O_NONBLOCK) == O_NONBLOCK;
    if nonblocking {
        match unsafe { libc::fcntl(fd, F_SETFL, flags & !O_NONBLOCK) } {
            -1 => return Err(io::Error::last_os_error()),
            _ => return Ok(()),
        };
    }
    Ok(())
}

/// A helper type for a File that does not close on drop.
/// Note the underlying file is never dropped; this is equivalent to mem::forget.
pub struct BorrowedFdFile(ManuallyDrop<File>);

impl Deref for BorrowedFdFile {
    type Target = File;

    #[inline]
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl DerefMut for BorrowedFdFile {
    #[inline]
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl FromRawFd for BorrowedFdFile {
    // Note this does NOT take ownership.
    unsafe fn from_raw_fd(fd: RawFd) -> Self {
        Self(ManuallyDrop::new(unsafe { File::from_raw_fd(fd) }))
    }
}

impl AsRawFd for BorrowedFdFile {
    #[inline]
    fn as_raw_fd(&self) -> RawFd {
        self.0.as_raw_fd()
    }
}

impl IntoRawFd for BorrowedFdFile {
    #[inline]
    fn into_raw_fd(self) -> RawFd {
        ManuallyDrop::into_inner(self.0).into_raw_fd()
    }
}

impl BorrowedFdFile {
    /// Return a BorrowedFdFile from stdin.
    pub fn stdin() -> Self {
        unsafe { Self::from_raw_fd(libc::STDIN_FILENO) }
    }
}

impl Clone for BorrowedFdFile {
    // BorrowedFdFile may be cloned: this just shares the borrowed fd.
    // It does NOT duplicate the underlying fd.
    fn clone(&self) -> Self {
        // Safety: just re-borrow the same fd.
        unsafe { Self::from_raw_fd(self.as_raw_fd()) }
    }
}

impl std::io::Read for BorrowedFdFile {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.deref_mut().read(buf)
    }
    fn read_vectored(&mut self, bufs: &mut [io::IoSliceMut<'_>]) -> io::Result<usize> {
        self.deref_mut().read_vectored(bufs)
    }
    fn read_to_end(&mut self, buf: &mut Vec<u8>) -> io::Result<usize> {
        self.deref_mut().read_to_end(buf)
    }
    fn read_to_string(&mut self, buf: &mut String) -> io::Result<usize> {
        self.deref_mut().read_to_string(buf)
    }
}

#[cfg(test)]
mod tests {
    use super::{BorrowedFdFile, FIRST_HIGH_FD, make_autoclose_pipes};
    use crate::tests::prelude::*;
    use libc::{F_GETFD, FD_CLOEXEC};
    use std::os::fd::{AsRawFd, FromRawFd};

    #[test]
    #[serial]
    fn test_pipes() {
        let _cleanup = test_init();
        // Here we just test that each pipe has CLOEXEC set and is in the high range.
        // Note pipe creation may fail due to fd exhaustion; don't fail in that case.
        let mut pipes = vec![];
        for _i in 0..10 {
            if let Ok(pipe) = make_autoclose_pipes() {
                pipes.push(pipe);
            }
        }
        for pipe in pipes {
            for fd in [&pipe.read, &pipe.write] {
                let fd = fd.as_raw_fd();
                assert!(fd >= FIRST_HIGH_FD);
                let flags = unsafe { libc::fcntl(fd, F_GETFD, 0) };
                assert!(flags >= 0);
                assert_ne!(flags & FD_CLOEXEC, 0);
            }
        }
    }

    #[test]
    fn test_borrowed_fd_file_does_not_close() {
        let file = std::fs::File::open("/dev/null").unwrap();
        let fd = file.as_raw_fd();
        let borrowed = unsafe { BorrowedFdFile::from_raw_fd(fd) };
        #[allow(clippy::drop_non_drop)]
        drop(borrowed);
        let flags = unsafe { libc::fcntl(fd, libc::F_GETFD, 0) };
        assert!(flags >= 0);
        drop(file);
        let flags = unsafe { libc::fcntl(fd, libc::F_GETFD, 0) };
        assert!(flags < 0);
    }
}
