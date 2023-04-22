use crate::common::wcs2zstring;
use crate::ffi;
use crate::wchar::{wstr, L};
use crate::wutil::perror;
use libc::EINTR;
use libc::O_CLOEXEC;
use nix::unistd;
use std::ffi::CStr;
use std::io::{Read, Write};
use std::os::unix::io::{AsRawFd, FromRawFd, RawFd};

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
        unsafe {
            match libc::read(self.as_raw_fd(), buf.as_mut_ptr() as *mut _, buf.len()) {
                -1 => Err(std::io::Error::from_raw_os_error(errno::errno().0)),
                bytes => Ok(bytes as usize),
            }
        }
    }
}

impl Write for AutoCloseFd {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        unsafe {
            match libc::write(self.as_raw_fd(), buf.as_ptr() as *const _, buf.len()) {
                -1 => Err(std::io::Error::from_raw_os_error(errno::errno().0)),
                bytes => Ok(bytes as usize),
            }
        }
    }

    fn flush(&mut self) -> std::io::Result<()> {
        // We don't buffer anything so this is a no-op.
        Ok(())
    }
}

#[cxx::bridge]
mod autoclose_fd_t {
    extern "Rust" {
        #[cxx_name = "autoclose_fd_t2"]
        type AutoCloseFd;

        #[cxx_name = "valid"]
        fn is_valid(&self) -> bool;
        fn close(&mut self);
        fn fd(&self) -> i32;
    }
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
    let pipes = ffi::make_pipes_ffi();

    let readp = AutoCloseFd::new(pipes.read);
    let writep = AutoCloseFd::new(pipes.write);
    if !readp.is_valid() || !writep.is_valid() {
        None
    } else {
        Some(AutoClosePipes {
            read: readp,
            write: writep,
        })
    }
}

/// Wide character version of open() that also sets the close-on-exec flag (atomically when
/// possible).
pub fn wopen_cloexec(pathname: &wstr, flags: i32, mode: libc::c_int) -> RawFd {
    open_cloexec(wcs2zstring(pathname).as_c_str(), flags, mode)
}

/// Narrow versions of wopen_cloexec.
pub fn open_cloexec(path: &CStr, flags: i32, mode: libc::c_int) -> RawFd {
    unsafe { libc::open(path.as_ptr(), flags | O_CLOEXEC, mode) }
}

/// Close a file descriptor \p fd, retrying on EINTR.
pub fn exec_close(fd: RawFd) {
    assert!(fd >= 0, "Invalid fd");
    while unsafe { libc::close(fd) } == -1 {
        if errno::errno().0 != EINTR {
            perror("close");
            break;
        }
    }
}
