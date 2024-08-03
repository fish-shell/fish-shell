use std::os::unix::prelude::*;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex, Weak};
use std::time::{Duration, Instant};

use crate::common::exit_without_destructors;
use crate::fd_readable_set::FdReadableSet;
use crate::fds::AutoCloseFd;
use crate::flog::FLOG;
use crate::threads::assert_is_background_thread;
use crate::wutil::perror;
use errno::errno;
use libc::{c_void, EAGAIN, EINTR, EWOULDBLOCK};

#[cfg(not(HAVE_EVENTFD))]
use crate::fds::{make_autoclose_pipes, make_fd_nonblocking};
#[cfg(HAVE_EVENTFD)]
use libc::{EFD_CLOEXEC, EFD_NONBLOCK};

/// Reason for waking an item
#[derive(PartialEq, Eq)]
pub enum ItemWakeReason {
    /// The fd became readable (or was HUP'd)
    Readable,
    /// The requested timeout was hit
    Timeout,
    /// The item was "poked" (woken up explicitly)
    Poke,
}

/// An event signaller implemented using a file descriptor, so it can plug into
/// [`select()`](libc::select).
///
/// This is like a binary semaphore. A call to [`post()`](FdEventSignaller::post) will
/// signal an event, making the fd readable.  Multiple calls to `post()` may be coalesced.
/// On Linux this uses [`eventfd()`](libc::eventfd), on other systems this uses a pipe.
/// [`try_consume()`](FdEventSignaller::try_consume) may be used to consume the event.
/// Importantly this is async signal safe. Of course it is `CLO_EXEC` as well.
pub struct FdEventSignaller {
    // Always the read end of the fd; maybe the write end as well.
    fd: OwnedFd,
    #[cfg(not(HAVE_EVENTFD))]
    write: OwnedFd,
}

impl FdEventSignaller {
    /// The default constructor will abort on failure (fd exhaustion).
    /// This should only be used during startup.
    pub fn new() -> Self {
        #[cfg(HAVE_EVENTFD)]
        {
            // Note we do not want to use EFD_SEMAPHORE because we are binary (not counting) semaphore.
            let fd = unsafe { libc::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK) };
            if fd < 0 {
                perror("eventfd");
                exit_without_destructors(1);
            }
            Self {
                fd: unsafe { OwnedFd::from_raw_fd(fd) },
            }
        }
        #[cfg(not(HAVE_EVENTFD))]
        {
            // Implementation using pipes.
            let Ok(pipes) = make_autoclose_pipes() else {
                exit_without_destructors(1);
            };
            make_fd_nonblocking(pipes.read.as_raw_fd()).unwrap();
            make_fd_nonblocking(pipes.write.as_raw_fd()).unwrap();
            Self {
                fd: pipes.read,
                write: pipes.write,
            }
        }
    }

    /// Return the fd to read from, for notification.
    pub fn read_fd(&self) -> RawFd {
        self.fd.as_raw_fd()
    }

    /// If an event is signalled, consume it; otherwise return.
    /// This does not block.
    /// This retries on EINTR.
    pub fn try_consume(&self) -> bool {
        // If we are using eventfd, we want to read a single uint64.
        // If we are using pipes, read a lot; note this may leave data on the pipe if post has been
        // called many more times. In no case do we care about the data which is read.
        #[cfg(HAVE_EVENTFD)]
        let mut buff = [0_u64; 1];
        #[cfg(not(HAVE_EVENTFD))]
        let mut buff = [0_u8; 1024];
        let mut ret;
        loop {
            ret = unsafe {
                libc::read(
                    self.read_fd(),
                    &mut buff as *mut _ as *mut c_void,
                    std::mem::size_of_val(&buff),
                )
            };
            if ret >= 0 || errno().0 != EINTR {
                break;
            }
        }
        if ret < 0 && ![EAGAIN, EWOULDBLOCK].contains(&errno().0) {
            perror("read");
        }
        ret > 0
    }

    /// Mark that an event has been received. This may be coalesced.
    /// This retries on EINTR.
    pub fn post(&self) {
        // eventfd writes uint64; pipes write 1 byte.
        #[cfg(HAVE_EVENTFD)]
        let c = 1_u64;
        #[cfg(not(HAVE_EVENTFD))]
        let c = 1_u8;
        let mut ret;
        loop {
            let bytes = c.to_ne_bytes();
            ret = nix::unistd::write(unsafe { BorrowedFd::borrow_raw(self.write_fd()) }, &bytes);

            match ret {
                Ok(_) => break,
                Err(nix::Error::EINTR) => continue,
                Err(_) => break,
            }
        }

        if let Err(err) = ret {
            // EAGAIN occurs if either the pipe buffer is full or the eventfd overflows (very unlikely).
            if ![nix::Error::EAGAIN, nix::Error::EWOULDBLOCK].contains(&err) {
                perror("write");
            }
        }
    }

    /// Perform a poll to see if an event is received.
    /// If `wait` is set, wait until it is readable; this does not consume the event
    /// but guarantees that the next call to wait() will not block.
    /// Return true if readable, false if not readable, or not interrupted by a signal.
    pub fn poll(&self, wait: bool /* = false */) -> bool {
        let mut timeout = libc::timeval {
            tv_sec: 0,
            tv_usec: 0,
        };
        let mut fds: libc::fd_set = unsafe { std::mem::zeroed() };
        unsafe { libc::FD_ZERO(&mut fds) };
        unsafe { libc::FD_SET(self.read_fd(), &mut fds) };
        let res = unsafe {
            libc::select(
                self.read_fd() + 1,
                &mut fds,
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                if wait {
                    std::ptr::null_mut()
                } else {
                    &mut timeout
                },
            )
        };
        res > 0
    }

    /// Return the fd to write to.
    fn write_fd(&self) -> RawFd {
        #[cfg(HAVE_EVENTFD)]
        return self.fd.as_raw_fd();
        #[cfg(not(HAVE_EVENTFD))]
        return self.write.as_raw_fd();
    }
}

/// Each item added to fd_monitor_t is assigned a unique ID, which is not recycled. Items may have
/// their callback triggered immediately by passing the ID. Zero is a sentinel.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct FdMonitorItemId(u64);

impl From<FdMonitorItemId> for u64 {
    fn from(value: FdMonitorItemId) -> Self {
        value.0
    }
}

impl From<u64> for FdMonitorItemId {
    fn from(value: u64) -> Self {
        FdMonitorItemId(value)
    }
}

/// The callback type used by [`FdMonitorItem`]. It is passed a mutable reference to the
/// `FdMonitorItem`'s [`FdMonitorItem::fd`] and [the reason](ItemWakeupReason) for the wakeup.
/// It should return an [`ItemAction`] to indicate whether the item should be removed from the
/// [`FdMonitor`] set.
pub type Callback = Box<dyn Fn(&mut AutoCloseFd, ItemWakeReason) -> ItemAction + Send + Sync>;

/// An item containing an fd and callback, which can be monitored to watch when it becomes readable
/// and invoke the callback.
pub struct FdMonitorItem {
    /// The fd to monitor
    fd: AutoCloseFd,
    /// A callback to be invoked when the fd is readable, or for another reason given by the wake reason.
    /// If the fd is invalid on return from the function, then the item is removed from the [`FdMonitor`] set.
    callback: Callback,
    /// The timeout associated with waiting on this item or `None` to wait indefinitely. A timeout
    /// of `0` is not supported.
    timeout: Option<Duration>,
    /// The last time we were called or the time of initialization.
    last_time: Option<Instant>,
    /// The id for this item, assigned by [`FdMonitor`].
    item_id: FdMonitorItemId,
}

/// A value returned by the callback to indicate what to do with the item.
#[derive(PartialEq, Eq)]
pub enum ItemAction {
    Remove,
    Retain,
}

impl FdMonitorItem {
    /// Return the duration until the timeout should trigger or `None`. A return of `0` means we are
    /// at or past the timeout.
    fn remaining_time(&self, now: &Instant) -> Option<Duration> {
        let last_time = self.last_time.expect("Should always have a last_time!");
        let timeout = self.timeout?;
        assert!(now >= &last_time, "Steady clock went backwards or bug!");
        let since = *now - last_time;
        Some(if since >= timeout {
            Duration::ZERO
        } else {
            timeout - since
        })
    }

    /// Invoke this item's callback if its value (when its value is set in the fd or has timed out).
    /// Returns `true` if the item should be retained or `false` if it should be removed from the
    /// set.
    fn service_item(&mut self, fds: &FdReadableSet, now: &Instant) -> ItemAction {
        let mut result = ItemAction::Retain;
        let readable = fds.test(self.fd.as_raw_fd());
        let timed_out = !readable && self.remaining_time(now) == Some(Duration::ZERO);
        if readable || timed_out {
            self.last_time = Some(*now);
            let reason = if readable {
                ItemWakeReason::Readable
            } else {
                ItemWakeReason::Timeout
            };
            result = (self.callback)(&mut self.fd, reason);
        }
        result
    }

    /// Invoke this item's callback with a poke, if its id is present in the sorted poke list.
    fn maybe_poke_item(&mut self, pokelist: &[FdMonitorItemId]) -> ItemAction {
        if self.item_id.0 == 0 || pokelist.binary_search(&self.item_id).is_err() {
            // Not pokeable or not in the poke list.
            return ItemAction::Retain;
        }

        (self.callback)(&mut self.fd, ItemWakeReason::Poke)
    }

    pub fn new(fd: AutoCloseFd, timeout: Option<Duration>, callback: Callback) -> Self {
        FdMonitorItem {
            fd,
            timeout,
            callback,
            item_id: FdMonitorItemId(0),
            last_time: None,
        }
    }
}

/// A thread-safe class which can monitor a set of fds, invoking a callback when any becomes
/// readable (or has been HUP'd) or when per-item-configurable timeouts are reached.
pub struct FdMonitor {
    /// Our self-signaller. When this is written to, it means there are new items pending, new items
    /// in the poke list, or terminate has been set.
    change_signaller: Arc<FdEventSignaller>,
    /// The data shared between the background thread and the `FdMonitor` instance.
    data: Arc<Mutex<SharedData>>,
    /// The last ID assigned or `0` if none.
    last_id: AtomicU64,
}

// We don't want to manually implement `Sync` for `FdMonitor` but we do want to make sure that it's
// always using interior mutability correctly and therefore automatically `Sync`.
const _: () = {
    // It is sufficient to declare the generic function pointers; calling them too would require
    // using `const fn` with Send/Sync constraints which wasn't stabilized until rustc 1.61.0
    fn assert_sync<T: Sync>() {}
    let _ = assert_sync::<FdMonitor>;
};

/// Data shared between the `FdMonitor` instance and its associated `BackgroundFdMonitor`.
struct SharedData {
    /// Pending items. This is set by the main thread with the mutex locked, then the background
    /// thread grabs them.
    pending: Vec<FdMonitorItem>,
    /// List of IDs for items that need to be poked (explicitly woken up).
    pokelist: Vec<FdMonitorItemId>,
    /// Whether the background thread is running.
    running: bool,
    /// Used to signal that the background thread should terminate.
    terminate: bool,
}

/// The background half of the fd monitor, running on its own thread.
struct BackgroundFdMonitor {
    /// The list of items to monitor. This is only accessed from the background thread.
    /// This doesn't need to be in any particular order.
    items: Vec<FdMonitorItem>,
    /// Our self-signaller. When this is written to, it means there are new items pending, new items
    /// in the poke list, or terminate has been set.
    change_signaller: Weak<FdEventSignaller>,
    /// The data shared between the background thread and the `FdMonitor` instance.
    data: Arc<Mutex<SharedData>>,
}

impl FdMonitor {
    /// Add an item to the monitor. Returns the [`FdMonitorItemId`] assigned to the item.
    pub fn add(&self, mut item: FdMonitorItem) -> FdMonitorItemId {
        assert!(item.fd.is_valid());
        assert!(item.timeout != Some(Duration::ZERO), "Invalid timeout!");
        assert!(
            item.item_id == FdMonitorItemId(0),
            "Item should not already have an id!"
        );

        let item_id = self.last_id.fetch_add(1, Ordering::Relaxed) + 1;
        let item_id = FdMonitorItemId(item_id);
        let start_thread = {
            // Lock around a local region
            let mut data = self.data.lock().expect("Mutex poisoned!");

            // Assign an id and add the item to pending
            item.item_id = item_id;
            data.pending.push(item);

            // Start the thread if it hasn't already been started
            let already_started = data.running;
            data.running = true;
            !already_started
        };

        if start_thread {
            FLOG!(fd_monitor, "Thread starting");
            let background_monitor = BackgroundFdMonitor {
                data: Arc::clone(&self.data),
                change_signaller: Arc::downgrade(&self.change_signaller),
                items: Vec::new(),
            };
            crate::threads::spawn(move || {
                background_monitor.run();
            });
        }

        // Tickle our signaller.
        self.change_signaller.post();

        item_id
    }

    /// Mark that the item with the given ID needs to be woken up explicitly.
    pub fn poke_item(&self, item_id: FdMonitorItemId) {
        assert!(item_id.0 > 0, "Invalid item id!");
        let needs_notification = {
            let mut data = self.data.lock().expect("Mutex poisoned!");
            let needs_notification = data.pokelist.is_empty();
            // Insert it, sorted. But not if it already exists.
            if let Err(pos) = data.pokelist.binary_search(&item_id) {
                data.pokelist.insert(pos, item_id);
            };
            needs_notification
        };

        if needs_notification {
            self.change_signaller.post();
        }
    }

    pub fn new() -> Self {
        Self {
            data: Arc::new(Mutex::new(SharedData {
                pending: Vec::new(),
                pokelist: Vec::new(),
                running: false,
                terminate: false,
            })),
            change_signaller: Arc::new(FdEventSignaller::new()),
            last_id: AtomicU64::new(0),
        }
    }
}

impl BackgroundFdMonitor {
    /// Starts monitoring the fd set and listening for new fds to add to the set. Takes ownership
    /// over its instance so that this method cannot be called again.
    fn run(mut self) {
        assert_is_background_thread();

        let mut pokelist: Vec<FdMonitorItemId> = Vec::new();
        let mut fds = FdReadableSet::new();

        loop {
            // Poke any items that need it
            if !pokelist.is_empty() {
                self.poke(&pokelist);
                pokelist.clear();
            }
            fds.clear();

            // Our change_signaller is special-cased
            let change_signal_fd = self.change_signaller.upgrade().unwrap().read_fd();
            fds.add(change_signal_fd);

            let mut now = Instant::now();
            // Use Duration::MAX to represent no timeout for comparison purposes.
            let mut timeout = Duration::MAX;

            for item in &mut self.items {
                fds.add(item.fd.as_raw_fd());
                if item.last_time.is_none() {
                    item.last_time = Some(now);
                }
                timeout = timeout.min(item.timeout.unwrap_or(Duration::MAX));
            }

            // If we have no items, then we wish to allow the thread to exit, but after a time, so
            // we aren't spinning up and tearing down the thread repeatedly. Set a timeout of 256
            // msec; if nothing becomes readable by then we will exit. We refer to this as the
            // wait-lap.
            let is_wait_lap = self.items.is_empty();
            if is_wait_lap {
                assert!(
                    timeout == Duration::MAX,
                    "Should not have a timeout on wait lap!"
                );
                timeout = Duration::from_millis(256);
            }

            // Don't leave Duration::MAX as an actual timeout value
            let timeout = match timeout {
                Duration::MAX => None,
                timeout => Some(timeout),
            };

            // Call select()
            let ret = fds.check_readable(
                timeout
                    .map(|duration| duration.as_micros() as u64)
                    .unwrap_or(FdReadableSet::kNoTimeout),
            );
            if ret < 0 && errno::errno().0 != libc::EINTR {
                // Surprising error
                perror("select");
            }

            // Update the value of `now` after waiting on `fds.check_readable()`; it's used in the
            // servicer closure.
            now = Instant::now();

            // A predicate which services each item in turn, returning true if it should be removed
            let servicer = |item: &mut FdMonitorItem| {
                let fd = item.fd.as_raw_fd();
                let action = item.service_item(&fds, &now);
                if action == ItemAction::Remove {
                    FLOG!(fd_monitor, "Removing fd", fd);
                }
                action
            };

            // Service all items that are either readable or have timed out, and remove any which
            // say to do so.

            self.items
                .retain_mut(|item| servicer(item) == ItemAction::Retain);

            // Handle any changes if the change signaller was set. Alternatively, this may be the
            // wait lap, in which case we might want to commit to exiting.
            let change_signalled = fds.test(change_signal_fd);
            if change_signalled || is_wait_lap {
                // Clear the change signaller before processing incoming changes
                self.change_signaller.upgrade().unwrap().try_consume();
                let mut data = self.data.lock().expect("Mutex poisoned!");

                // Move from `pending` to the end of `items`
                self.items.extend(&mut data.pending.drain(..));

                // Grab any poke list
                assert!(
                    pokelist.is_empty(),
                    "poke list should be empty or else we're dropping pokes!"
                );
                std::mem::swap(&mut pokelist, &mut data.pokelist);

                if data.terminate
                    || (is_wait_lap
                        && self.items.is_empty()
                        && pokelist.is_empty()
                        && !change_signalled)
                {
                    // Maybe terminate is set. Alternatively, maybe we had no items, waited a bit,
                    // and still have no items. It's important to do this while holding the lock,
                    // otherwise we race with new items being added.
                    assert!(
                        data.running,
                        "Thread should be running because we're that thread"
                    );
                    FLOG!(fd_monitor, "Thread exiting");
                    data.running = false;
                    break;
                }
            }
        }
    }

    /// Poke items in the poke list, removing any items that close their fd in their callback. The
    /// poke list is consumed after this. This is only called from the background thread.
    fn poke(&mut self, pokelist: &[FdMonitorItemId]) {
        self.items.retain_mut(|item| {
            let action = item.maybe_poke_item(pokelist);
            if action == ItemAction::Remove {
                FLOG!(fd_monitor, "Removing fd", item.fd.as_raw_fd());
            }
            return action == ItemAction::Retain;
        });
    }
}

/// In ordinary usage, we never invoke the destructor. This is used in the tests to not leave stale
/// fds arounds; this is why it's very hacky!
impl Drop for FdMonitor {
    fn drop(&mut self) {
        // Safety: this is a port of the C++ code and we are running in the destructor. The C++ code
        // had no way to bubble back any errors encountered here, and the pthread mutex the C++ code
        // uses does not have a concept of mutex poisoning.
        self.data.lock().expect("Mutex poisoned!").terminate = true;
        self.change_signaller.post();

        // Safety: see note above.
        while self.data.lock().expect("Mutex poisoned!").running {
            std::thread::sleep(Duration::from_millis(5));
        }
    }
}
