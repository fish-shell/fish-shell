use libc::c_int;
use std::os::unix::prelude::*;

/// Returns `true` if the fd is or becomes readable within the given timeout.
/// This returns `false` if the waiting is interrupted by a signal.
pub fn is_fd_readable(fd: i32, timeout_usec: u64) -> bool {
    FdReadableSet::is_fd_readable(fd, timeout_usec)
}

/// Returns whether an fd is readable.
pub fn poll_fd_readable(fd: i32) -> bool {
    FdReadableSet::poll_fd_readable(fd)
}

/// A modest wrapper around select() or poll().
/// This allows accumulating a set of fds and then seeing if they are readable.
/// This only handles readability.
/// Apple's `man poll`: "The poll() system call currently does not support devices."
#[cfg(target_os = "macos")]
pub struct FdReadableSet {
    // The underlying fdset and nfds value to pass to select().
    fdset_: libc::fd_set,
    nfds_: c_int,
}

#[allow(dead_code)]
const kUsecPerMsec: u64 = 1000;
#[allow(dead_code)]
const kUsecPerSec: u64 = 1000 * kUsecPerMsec;

#[cfg(target_os = "macos")]
impl FdReadableSet {
    /// Construct an empty set.
    pub fn new() -> FdReadableSet {
        FdReadableSet {
            fdset_: unsafe { std::mem::zeroed() },
            nfds_: 0,
        }
    }

    /// Reset back to an empty set.
    pub fn clear(&mut self) {
        self.nfds_ = 0;
        unsafe {
            libc::FD_ZERO(&mut self.fdset_);
        }
    }

    /// Add an fd to the set. The fd is ignored if negative (for convenience).
    pub fn add(&mut self, fd: RawFd) {
        if fd >= (libc::FD_SETSIZE as RawFd) {
            //FLOGF(error, "fd %d too large for select()", fd);
            return;
        }
        if fd >= 0 {
            unsafe { libc::FD_SET(fd, &mut self.fdset_) };
            self.nfds_ = std::cmp::max(self.nfds_, fd + 1);
        }
    }

    /// Returns `true` if the given `fd` is marked as set, in our set. Returns `false` if `fd` is
    /// negative.
    pub fn test(&self, fd: RawFd) -> bool {
        fd >= 0 && unsafe { libc::FD_ISSET(fd, &self.fdset_) }
    }

    /// Call `select()` or `poll()`, according to FISH_READABLE_SET_USE_POLL. Note this
    /// destructively modifies the set. Returns the result of `select()` or `poll()`.
    pub fn check_readable(&mut self, timeout_usec: u64) -> c_int {
        let null = std::ptr::null_mut();
        if timeout_usec == Self::kNoTimeout {
            unsafe {
                return libc::select(
                    self.nfds_,
                    &mut self.fdset_,
                    null,
                    null,
                    std::ptr::null_mut(),
                );
            }
        } else {
            let mut tvs = libc::timeval {
                tv_sec: (timeout_usec / kUsecPerSec) as libc::time_t,
                tv_usec: (timeout_usec % kUsecPerSec) as libc::suseconds_t,
            };
            unsafe {
                return libc::select(self.nfds_, &mut self.fdset_, null, null, &mut tvs);
            }
        }
    }

    /// Check if a single fd is readable, with a given timeout.
    /// Returns `true` if readable, `false` otherwise.
    pub fn is_fd_readable(fd: RawFd, timeout_usec: u64) -> bool {
        if fd < 0 {
            return false;
        }
        let mut s = Self::new();
        s.add(fd);
        let res = s.check_readable(timeout_usec);
        return res > 0 && s.test(fd);
    }

    /// Check if a single fd is readable, without blocking.
    /// Returns `true` if readable, `false` if not.
    pub fn poll_fd_readable(fd: RawFd) -> bool {
        return Self::is_fd_readable(fd, 0);
    }

    /// A special timeout value which may be passed to indicate no timeout.
    pub const kNoTimeout: u64 = u64::MAX;
}

#[cfg(not(target_os = "macos"))]
pub struct FdReadableSet {
    pollfds_: Vec<libc::pollfd>,
}

#[cfg(not(target_os = "macos"))]
impl FdReadableSet {
    /// Construct an empty set.
    pub fn new() -> FdReadableSet {
        FdReadableSet {
            pollfds_: Vec::new(),
        }
    }

    /// Reset back to an empty set.
    pub fn clear(&mut self) {
        self.pollfds_.clear();
    }

    #[inline]
    fn pollfd_get_fd(pollfd: &libc::pollfd) -> RawFd {
        pollfd.fd
    }

    /// Add an fd to the set. The fd is ignored if negative (for convenience). The fd is also
    /// ignored if it's already in the set.
    pub fn add(&mut self, fd: RawFd) {
        if fd < 0 {
            return;
        }
        let pos = match self.pollfds_.binary_search_by_key(&fd, Self::pollfd_get_fd) {
            Ok(_) => return,
            Err(pos) => pos,
        };

        self.pollfds_.insert(
            pos,
            libc::pollfd {
                fd,
                events: libc::POLLIN,
                revents: 0,
            },
        );
    }

    /// Returns `true` if the given `fd` has input available to read or has been HUP'd.
    /// Returns `false` if `fd` is negative or was not found in the set.
    pub fn test(&self, fd: RawFd) -> bool {
        // If a pipe is widowed with no data, Linux sets POLLHUP but not POLLIN, so test for both.
        if let Ok(pos) = self.pollfds_.binary_search_by_key(&fd, Self::pollfd_get_fd) {
            let pollfd = &self.pollfds_[pos];
            debug_assert_eq!(pollfd.fd, fd);
            return pollfd.revents & (libc::POLLIN | libc::POLLHUP) != 0;
        }
        return false;
    }

    /// Convert from usecs to poll-friendly msecs.
    fn usec_to_poll_msec(timeout_usec: u64) -> c_int {
        let mut timeout_msec: u64 = timeout_usec / kUsecPerMsec;
        // Round to nearest, down for halfway.
        if (timeout_usec % kUsecPerMsec) > kUsecPerMsec / 2 {
            timeout_msec += 1;
        }
        if timeout_usec == FdReadableSet::kNoTimeout || timeout_msec > c_int::MAX as u64 {
            // Negative values mean wait forever in poll-speak.
            return -1;
        }
        return timeout_msec as c_int;
    }

    fn do_poll(fds: &mut [libc::pollfd], timeout_usec: u64) -> c_int {
        let count = fds.len();
        assert!(count <= libc::nfds_t::MAX as usize, "count too big");
        return unsafe {
            libc::poll(
                fds.as_mut_ptr(),
                count as libc::nfds_t,
                Self::usec_to_poll_msec(timeout_usec),
            )
        };
    }

    /// Call select() or poll(), according to FISH_READABLE_SET_USE_POLL. Note this destructively
    /// modifies the set. Return the result of select() or poll().
    ///
    /// TODO: Change to [`Duration`](std::time::Duration) once FFI usage is done.
    pub fn check_readable(&mut self, timeout_usec: u64) -> c_int {
        if self.pollfds_.is_empty() {
            return 0;
        }
        return Self::do_poll(&mut self.pollfds_, timeout_usec);
    }

    /// Check if a single fd is readable, with a given timeout.
    /// Return true if `fd` is our set and is readable, `false` otherwise.
    pub fn is_fd_readable(fd: RawFd, timeout_usec: u64) -> bool {
        if fd < 0 {
            return false;
        }
        let mut pfd = libc::pollfd {
            fd,
            events: libc::POLLIN,
            revents: 0,
        };
        let ret = Self::do_poll(std::slice::from_mut(&mut pfd), timeout_usec);
        return ret > 0 && (pfd.revents & libc::POLLIN) != 0;
    }

    /// Check if a single fd is readable, without blocking.
    /// Return true if readable, false if not.
    pub fn poll_fd_readable(fd: RawFd) -> bool {
        return Self::is_fd_readable(fd, 0);
    }

    /// A special timeout value which may be passed to indicate no timeout.
    pub const kNoTimeout: u64 = u64::MAX;
}
