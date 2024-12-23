#[cfg(not(target_has_atomic = "64"))]
use portable_atomic::AtomicU64;
use std::collections::HashMap;
use std::os::unix::prelude::*;
#[cfg(target_has_atomic = "64")]
use std::sync::atomic::AtomicU64;
use std::sync::atomic::Ordering;
use std::sync::{Arc, Mutex};
use std::time::Duration;

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

/// Each item added to FdMonitor is assigned a unique ID, which is not recycled. Items may have
/// their callback triggered immediately by passing the ID. Zero is a sentinel.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
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
    /// Invoke this item's callback because the fd is readable.
    /// Returns the [`ItemAction`] to indicate whether the item should be removed from the [`FdMonitor`] set.
    fn service_readable(&mut self) -> ItemAction {
        (self.callback)(&mut self.fd, ItemWakeReason::Readable)
    }

    /// Invoke this item's callback because the item was poked.
    /// Returns the [`ItemAction`] to indicate whether the item should be removed from the [`FdMonitor`] set.
    fn service_poke(&mut self) -> ItemAction {
        (self.callback)(&mut self.fd, ItemWakeReason::Poke)
    }

    pub fn new(fd: AutoCloseFd, callback: Callback) -> Self {
        FdMonitorItem {
            fd,
            callback,
            item_id: FdMonitorItemId(0),
        }
    }
}

/// A thread-safe class which can monitor a set of fds, invoking a callback when any becomes
/// readable (or has been HUP'd).
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
    /// The map of items. This may be modified by the main thread with the mutex locked.
    items: HashMap<FdMonitorItemId, FdMonitorItem>,
    /// List of IDs for items that need to be poked (explicitly woken up).
    pokelist: Vec<FdMonitorItemId>,
    /// Whether the background thread is running.
    running: bool,
    /// Used to signal that the background thread should terminate.
    terminate: bool,
}

/// The background half of the fd monitor, running on its own thread.
struct BackgroundFdMonitor {
    /// Our self-signaller. When this is written to, it means there are new items pending, new items
    /// in the poke list, or terminate has been set.
    change_signaller: Arc<FdEventSignaller>,
    /// The data shared between the background thread and the `FdMonitor` instance.
    data: Arc<Mutex<SharedData>>,
}

impl FdMonitor {
    /// Add an item to the monitor. Returns the [`FdMonitorItemId`] assigned to the item.
    pub fn add(&self, mut item: FdMonitorItem) -> FdMonitorItemId {
        assert!(item.fd.is_valid());
        assert!(
            item.item_id == FdMonitorItemId(0),
            "Item should not already have an id!"
        );

        let item_id = self.last_id.fetch_add(1, Ordering::Relaxed) + 1;
        let item_id = FdMonitorItemId(item_id);
        let start_thread = {
            // Lock around a local region
            let mut data = self.data.lock().expect("Mutex poisoned!");

            // Assign an id and add the item.
            item.item_id = item_id;
            let old_value = data.items.insert(item_id, item);
            assert!(old_value.is_none(), "Item ID {} already exists!", item_id.0);

            // Start the thread if it hasn't already been started
            let already_started = data.running;
            data.running = true;
            !already_started
        };

        if start_thread {
            FLOG!(fd_monitor, "Thread starting");
            let background_monitor = BackgroundFdMonitor {
                data: Arc::clone(&self.data),
                change_signaller: Arc::clone(&self.change_signaller),
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
                items: HashMap::new(),
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
    fn run(self) {
        assert_is_background_thread();

        let mut fds = FdReadableSet::new();
        let mut item_ids: Vec<FdMonitorItemId> = Vec::new();
        let mut pokelist: Vec<FdMonitorItemId> = Vec::new();

        loop {
            // Our general flow is that a client thread adds an item for us to monitor,
            // and we use select() or poll() to wait on it. However, the client thread
            // may then reclaim the item. We are currently blocked in select():
            // how then do we stop waiting on it?
            //
            // The safest, slowest approach is:
            //  - The background thread waits on select() for the set of active file descriptors.
            //  - The client thread records a request to remove an item.
            //  - The client thread wakes up the background thread via change_signaller.
            //  - The background thread check for any pending removals, and removes and returns them.
            //  - The client thread accepts the removed item and continues on.
            // However this means a round-trip from the client thread to this background thread,
            // plus additional blocking system calls. This slows down the client thread.
            //
            // A second possibility is that:
            //  - The background thread waits on select() for the set of active file descriptors.
            //  - The client thread directly removes an item (protected by the mutex).
            //  - After select() returns the set of active file descriptors, we only invoke callbacks
            //    for items whose file descriptors are still in the set.
            // However this risks the ABA problem: if the client thread reclaims an item, closes its
            // fd, and then adds a new item which happens to get the same fd, we might falsely
            // trigger the callback of the new item even though its fd is not readable.
            //
            // So we use the following approach:
            // - The background thread creates a snapshotted list of active ItemIDs.
            // - The background thread waits in select() on the set of active file descriptors,
            //   without holding the lock.
            // - The client thread directly removes an item (protected by the mutex).
            // - After select() returns the set of active file descriptors, we only invoke callbacks
            //   for items whose file descriptors are marked active, and whose ItemID was snapshotted.
            // This avoids the ABA problem because ItemIDs are never recycled. It does have a race where
            // we might select() on a file descriptor that has been closed or recycled. Thus we must be
            // prepared to handle EBADF. This race is otherwise considered benign.

            // Construct the set of fds to monitor.
            // Our change_signaller is special-cased.
            fds.clear();
            let change_signal_fd = self.change_signaller.read_fd();
            fds.add(change_signal_fd);

            // Grab the lock and snapshot the item_ids.
            let mut data = self.data.lock().expect("Mutex poisoned!");
            item_ids.clear();
            item_ids.reserve(data.items.len());
            for (item_id, item) in &data.items {
                let fd = item.fd.as_raw_fd();
                fds.add(fd);
                item_ids.push(*item_id);
            }

            // Sort it to avoid the non-determinism of the hash table.
            item_ids.sort();

            // If we have no items, then we wish to allow the thread to exit, but after a time, so
            // we aren't spinning up and tearing down the thread repeatedly. Set a timeout of 256
            // msec; if nothing becomes readable by then we will exit. We refer to this as the
            // wait-lap.
            let is_wait_lap = item_ids.is_empty();
            let timeout = if is_wait_lap {
                Some(Duration::from_millis(256))
            } else {
                None
            };

            // Call select().
            // We must release and then re-acquire the lock around select() to avoid deadlock.
            // Note that while we are waiting in select(), the client thread may add or remove items;
            // in particular it may even close file descriptors that we are waiting on. That is why
            // we handle EBADF. Note that even if the file descriptor is recycled, we don't invoke
            // a callback for it unless its ItemID is still present.
            drop(data);
            let ret = fds.check_readable(
                timeout
                    .map(|duration| duration.as_micros() as u64)
                    .unwrap_or(FdReadableSet::kNoTimeout),
            );
            if ret < 0 && !matches!(errno().0, libc::EINTR | libc::EBADF) {
                // Surprising error
                perror("select");
            }

            // Re-acquire the lock.
            data = self.data.lock().expect("Mutex poisoned!");

            // For each item id that we snapshotted, if the corresponding item is still in our
            // set of active items and its fd was readable, then service it.
            for item_id in &item_ids {
                let Some(item) = data.items.get_mut(item_id) else {
                    // Item was removed while we were waiting.
                    // Note there is no risk of an ABA problem because ItemIDs are never recycled.
                    continue;
                };
                let fd = item.fd.as_raw_fd();
                if !fds.test(fd) {
                    // Not readable.
                    continue;
                }
                let action = item.service_readable();
                if action == ItemAction::Remove {
                    FLOG!(fd_monitor, "Removing fd", fd);
                    data.items.remove(item_id);
                }
            }

            pokelist.clear();
            std::mem::swap(&mut pokelist, &mut data.pokelist);
            for item_id in pokelist.drain(..) {
                if let Some(item) = data.items.get_mut(&item_id) {
                    let action = item.service_poke();
                    if action == ItemAction::Remove {
                        FLOG!(fd_monitor, "Removing fd", item.fd.as_raw_fd());
                        data.items.remove(&item_id);
                    }
                }
            }

            // Handle any changes if the change signaller was set. Alternatively, this may be the
            // wait lap, in which case we might want to commit to exiting.
            let change_signalled = fds.test(change_signal_fd);
            if change_signalled || is_wait_lap {
                // Clear the change signaller before processing incoming changes
                self.change_signaller.try_consume();

                if data.terminate
                    || (is_wait_lap
                        && data.items.is_empty()
                        && data.pokelist.is_empty()
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
}

/// In ordinary usage, we never invoke the destructor. This is used in the tests to not leave stale
/// fds arounds; this is why it's very hacky!
impl Drop for FdMonitor {
    fn drop(&mut self) {
        self.data.lock().expect("Mutex poisoned!").terminate = true;
        self.change_signaller.post();

        // Safety: see note above.
        while self.data.lock().expect("Mutex poisoned!").running {
            std::thread::sleep(Duration::from_millis(5));
        }
    }
}
