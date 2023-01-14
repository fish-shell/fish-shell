use crate::ffi;
use nix::unistd;
use std::os::unix::io::RawFd;

/// A helper type for managing and automatically closing a file descriptor
pub struct autoclose_fd_t {
    fd_: RawFd,
}

impl autoclose_fd_t {
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

    // \return if this has a valid fd.
    pub fn valid(&self) -> bool {
        self.fd_ >= 0
    }

    // Construct, taking ownership of an fd.
    pub fn new(fd: RawFd) -> autoclose_fd_t {
        autoclose_fd_t { fd_: fd }
    }
}

impl Default for autoclose_fd_t {
    fn default() -> autoclose_fd_t {
        autoclose_fd_t { fd_: -1 }
    }
}

impl Drop for autoclose_fd_t {
    fn drop(&mut self) {
        self.close()
    }
}

/// Helper type returned from make_autoclose_pipes.
#[derive(Default)]
pub struct autoclose_pipes_t {
    /// Read end of the pipe.
    pub read: autoclose_fd_t,

    /// Write end of the pipe.
    pub write: autoclose_fd_t,
}

/// Construct a pair of connected pipes, set to close-on-exec.
/// \return None on fd exhaustion.
pub fn make_autoclose_pipes() -> Option<autoclose_pipes_t> {
    let pipes = ffi::make_pipes_ffi();

    let readp = autoclose_fd_t::new(pipes.read);
    let writep = autoclose_fd_t::new(pipes.write);
    if !readp.valid() || !writep.valid() {
        None
    } else {
        Some(autoclose_pipes_t {
            read: readp,
            write: writep,
        })
    }
}
