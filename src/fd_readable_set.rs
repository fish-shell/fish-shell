use libc::c_int;
use std::os::unix::prelude::*;
use std::time::Duration;

pub enum Timeout {
    Duration(Duration),
    Forever,
}

impl Timeout {
    pub const ZERO: Timeout = Timeout::Duration(Duration::ZERO);

    /// Convert from usecs to poll-friendly msecs.
    #[allow(unused)]
    fn as_poll_msecs(&self) -> c_int {
        match self {
            // Negative values mean wait forever in poll-speak.
            Timeout::Forever => -1 as c_int,
            Timeout::Duration(duration) => {
                assert!(
                    duration.as_millis() < c_int::MAX as _,
                    "Timeout too long but not forever!"
                );
                duration.as_millis() as c_int
            }
        }
    }
}

/// Returns `true` if the fd is or becomes readable within the given timeout.
/// This returns `false` if the waiting is interrupted by a signal.
pub fn is_fd_readable(fd: i32, timeout: Timeout) -> bool {
    FdReadableSet::is_fd_readable(fd, timeout)
}

/// Returns whether an fd is readable.
pub fn poll_fd_readable(fd: i32) -> bool {
    FdReadableSet::poll_fd_readable(fd)
}

/// A modest wrapper around select() or poll().
/// This allows accumulating a set of fds and then seeing if they are readable.
/// This only handles readability.
/// Apple's `man poll`: "The poll() system call currently does not support devices."
#[cfg(apple)]
pub struct FdReadableSet {
    // The underlying fdset and nfds value to pass to select().
    fdset_: libc::fd_set,
    nfds_: c_int,
}

#[cfg(apple)]
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

    /// Call `select()`. Note this destructively modifies the set. Returns the result of
    /// `select()`.
    pub fn check_readable(&mut self, timeout: Timeout) -> c_int {
        let null = std::ptr::null_mut();
        let mut tvs;
        let timeout = match timeout {
            Timeout::Forever => std::ptr::null_mut(),
            Timeout::Duration(duration) => {
                tvs = libc::timeval {
                    tv_sec: duration.as_secs() as libc::time_t,
                    tv_usec: duration.subsec_micros() as libc::suseconds_t,
                };
                &mut tvs
            }
        };
        unsafe {
            return libc::select(self.nfds_, &mut self.fdset_, null, null, timeout);
        }
    }

    /// Check if a single fd is readable, with a given timeout.
    /// Returns `true` if readable, `false` otherwise.
    pub fn is_fd_readable(fd: RawFd, timeout: Timeout) -> bool {
        if fd < 0 {
            return false;
        }
        let mut s = Self::new();
        s.add(fd);
        let res = s.check_readable(timeout);
        return res > 0 && s.test(fd);
    }

    /// Check if a single fd is readable, without blocking.
    /// Returns `true` if readable, `false` if not.
    pub fn poll_fd_readable(fd: RawFd) -> bool {
        return Self::is_fd_readable(fd, Timeout::ZERO);
    }
}

#[cfg(not(apple))]
pub struct FdReadableSet {
    pollfds_: Vec<libc::pollfd>,
}

#[cfg(not(apple))]
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

    fn do_poll(fds: &mut [libc::pollfd], timeout: Timeout) -> c_int {
        let count = fds.len();
        assert!(count <= libc::nfds_t::MAX as usize, "count too big");
        return unsafe {
            libc::poll(
                fds.as_mut_ptr(),
                count as libc::nfds_t,
                timeout.as_poll_msecs(),
            )
        };
    }

    /// Call poll(). Note this destructively modifies the set. Return the result of poll().
    pub fn check_readable(&mut self, timeout: Timeout) -> c_int {
        if self.pollfds_.is_empty() {
            return 0;
        }
        return Self::do_poll(&mut self.pollfds_, timeout);
    }

    /// Check if a single fd is readable, with a given timeout.
    /// Return true if `fd` is our set and is readable, `false` otherwise.
    pub fn is_fd_readable(fd: RawFd, timeout: Timeout) -> bool {
        if fd < 0 {
            return false;
        }
        let mut pfd = libc::pollfd {
            fd,
            events: libc::POLLIN,
            revents: 0,
        };
        let ret = Self::do_poll(std::slice::from_mut(&mut pfd), timeout);
        return ret > 0 && (pfd.revents & libc::POLLIN) != 0;
    }

    /// Check if a single fd is readable, without blocking.
    /// Return true if readable, false if not.
    pub fn poll_fd_readable(fd: RawFd) -> bool {
        return Self::is_fd_readable(fd, Timeout::ZERO);
    }
}
