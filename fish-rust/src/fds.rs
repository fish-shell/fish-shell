use crate::common::wcs2zstring;
use crate::flog::FLOG;
#[cfg(test)]
use crate::tests::prelude::*;
use crate::wchar::prelude::*;
use crate::wutil::perror;
use libc::{
    c_int, FD_CLOEXEC, F_DUPFD_CLOEXEC, F_GETFD, F_GETFL, F_SETFD, F_SETFL, O_CLOEXEC, O_NONBLOCK,
};
use nix::errno::Errno;
use nix::unistd;
use std::ffi::CStr;
use std::io::{self, Read, Write};
use std::os::unix::prelude::*;

pub const PIPE_ERROR: &str = "An error occurred while setting up pipe";

/// The first "high fd", which is considered outside the range of valid user-specified redirections
/// (like >&5).
pub const FIRST_HIGH_FD: RawFd = 10;

/// A sentinel value indicating no timeout.
pub const NO_TIMEOUT: u64 = u64::MAX;

/// A helper type for managing and automatically closing a file descriptor
///
/// This was implemented in rust as a port of the existing C++ code but it didn't take its place
/// (yet) and there's still the original cpp implementation in `src/fds.h`, so its name is
/// disambiguated because some code uses a mix of both for interop purposes.
pub struct AutoCloseFd {
    fd_: RawFd,
}

impl Read for AutoCloseFd {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        unistd::read(self.as_raw_fd(), buf).map_err(std::io::Error::from)
    }
}

impl Write for AutoCloseFd {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        unistd::write(self.as_raw_fd(), buf).map_err(std::io::Error::from)
    }

    fn flush(&mut self) -> std::io::Result<()> {
        // We don't buffer anything so this is a no-op.
        Ok(())
    }
}

fn new_autoclose_fd(fd: i32) -> Box<AutoCloseFd> {
    Box::new(AutoCloseFd::new(fd))
}

impl AutoCloseFd {
    // Closes the fd if not already closed.
    pub fn close(&mut self) {
        if self.fd_ != -1 {
            _ = unistd::close(self.fd_);
            self.fd_ = -1;
        }
    }

    // Returns the fd.
    pub fn fd(&self) -> RawFd {
        self.fd_
    }

    // Returns the fd, transferring ownership to the caller.
    pub fn acquire(&mut self) -> RawFd {
        let temp = self.fd_;
        self.fd_ = -1;
        temp
    }

    // Resets to a new fd, taking ownership.
    pub fn reset(&mut self, fd: RawFd) {
        if fd == self.fd_ {
            return;
        }
        self.close();
        self.fd_ = fd;
    }

    // Returns if this has a valid fd.
    pub fn is_valid(&self) -> bool {
        self.fd_ >= 0
    }

    // Create a new AutoCloseFd instance taking ownership of the passed fd
    pub fn new(fd: RawFd) -> Self {
        AutoCloseFd { fd_: fd }
    }

    // Create a new AutoCloseFd without an open fd
    pub fn empty() -> Self {
        AutoCloseFd { fd_: -1 }
    }
}

impl FromRawFd for AutoCloseFd {
    unsafe fn from_raw_fd(fd: RawFd) -> Self {
        AutoCloseFd { fd_: fd }
    }
}

impl AsRawFd for AutoCloseFd {
    fn as_raw_fd(&self) -> RawFd {
        self.fd()
    }
}

impl Default for AutoCloseFd {
    fn default() -> AutoCloseFd {
        AutoCloseFd { fd_: -1 }
    }
}

impl Drop for AutoCloseFd {
    fn drop(&mut self) {
        self.close()
    }
}

/// Helper type returned from make_autoclose_pipes.
#[derive(Default)]
pub struct AutoClosePipes {
    /// Read end of the pipe.
    pub read: AutoCloseFd,

    /// Write end of the pipe.
    pub write: AutoCloseFd,
}

/// Construct a pair of connected pipes, set to close-on-exec.
/// \return None on fd exhaustion.
pub fn make_autoclose_pipes() -> Option<AutoClosePipes> {
    let mut pipes: [c_int; 2] = [-1, -1];

    let already_cloexec = false;
    #[cfg(HAVE_PIPE2)]
    {
        if unsafe { libc::pipe2(&mut pipes[0], O_CLOEXEC) } < 0 {
            FLOG!(warning, PIPE_ERROR);
            perror("pipe2");
            return None;
        }
        already_cloexec = true;
    }
    #[cfg(not(HAVE_PIPE2))]
    if unsafe { libc::pipe(&mut pipes[0]) } < 0 {
        FLOG!(warning, PIPE_ERROR);
        perror("pipe2");
        return None;
    }

    let readp = AutoCloseFd::new(pipes[0]);
    let writep = AutoCloseFd::new(pipes[1]);

    // Ensure our fds are out of the user range.
    let readp = heightenize_fd(readp, already_cloexec);
    if !readp.is_valid() {
        return None;
    }

    let writep = heightenize_fd(writep, already_cloexec);
    if !writep.is_valid() {
        return None;
    }

    Some(AutoClosePipes {
        read: readp,
        write: writep,
    })
}

/// If the given fd is in the "user range", move it to a new fd in the "high range".
/// zsh calls this movefd().
/// \p input_has_cloexec describes whether the input has CLOEXEC already set, so we can avoid
/// setting it again.
/// \return the fd, which always has CLOEXEC set; or an invalid fd on failure, in
/// which case an error will have been printed, and the input fd closed.
fn heightenize_fd(fd: AutoCloseFd, input_has_cloexec: bool) -> AutoCloseFd {
    // Check if the fd is invalid or already in our high range.
    if !fd.is_valid() {
        return fd;
    }
    if fd.fd() >= FIRST_HIGH_FD {
        if !input_has_cloexec {
            set_cloexec(fd.fd(), true);
        }
        return fd;
    }
    // Here we are asking the kernel to give us a cloexec fd.
    let newfd = unsafe { libc::fcntl(fd.fd(), F_DUPFD_CLOEXEC, FIRST_HIGH_FD) };
    if newfd < 0 {
        perror("fcntl");
        return AutoCloseFd::default();
    }
    return AutoCloseFd::new(newfd);
}

/// Sets CLO_EXEC on a given fd according to the value of \p should_set.
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
pub fn wopen_cloexec(pathname: &wstr, flags: i32, mode: libc::c_int) -> RawFd {
    open_cloexec(wcs2zstring(pathname).as_c_str(), flags, mode)
}

/// Narrow versions of wopen_cloexec.
pub fn open_cloexec(path: &CStr, flags: i32, mode: libc::c_int) -> RawFd {
    // Port note: the C++ version of this function had a fallback for platforms where
    // O_CLOEXEC is not supported, using fcntl. In 2023, this is no longer needed.
    unsafe { libc::open(path.as_ptr(), flags | O_CLOEXEC, mode) }
}

/// Close a file descriptor \p fd, retrying on EINTR.
pub fn exec_close(fd: RawFd) {
    assert!(fd >= 0, "Invalid fd");

    while let Err(err) = unistd::close(fd) {
        if err != Errno::EINTR {
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
        Errno::result(unsafe { libc::fcntl(fd, F_SETFL, flags | O_NONBLOCK) })?;
    }
    Ok(())
}

/// Mark an fd as blocking
pub fn make_fd_blocking(fd: RawFd) -> Result<(), io::Error> {
    let flags = unsafe { libc::fcntl(fd, F_GETFL, 0) };
    let nonblocking = (flags & O_NONBLOCK) == O_NONBLOCK;
    if nonblocking {
        Errno::result(unsafe { libc::fcntl(fd, F_SETFL, flags & !O_NONBLOCK) })?;
    }
    Ok(())
}

#[test]
#[serial]
fn test_pipes() {
    test_init();
    // Here we just test that each pipe has CLOEXEC set and is in the high range.
    // Note pipe creation may fail due to fd exhaustion; don't fail in that case.
    let mut pipes = vec![];
    for _i in 0..10 {
        if let Some(pipe) = make_autoclose_pipes() {
            pipes.push(pipe);
        }
    }
    for pipe in pipes {
        for fd in [pipe.read.fd(), pipe.write.fd()] {
            assert!(fd >= FIRST_HIGH_FD);
            let flags = unsafe { libc::fcntl(fd, F_GETFD, 0) };
            assert!(flags >= 0);
            assert!(flags & FD_CLOEXEC != 0);
        }
    }
}
