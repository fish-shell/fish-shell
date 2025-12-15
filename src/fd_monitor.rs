use cfg_if::cfg_if;
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
use crate::fd_readable_set::{FdReadableSet, Timeout};
use crate::fds::AutoCloseFd;
use crate::flog::flog;
use crate::threads::assert_is_background_thread;
use crate::wutil::perror;
use errno::errno;
use libc::{EAGAIN, EINTR, EWOULDBLOCK};

cfg_if!(
    if #[cfg(have_eventfd)] {
        use libc::{EFD_CLOEXEC, EFD_NONBLOCK};
    } else {
        use crate::fds::{make_autoclose_pipes, make_fd_nonblocking};
    }
);

/// An event signaller implemented using a file descriptor, so it can plug into
/// [`select()`](libc::select).
///
/// This is like a binary semaphore. A call to [`post()`](FdEventSignaller::post) will
/// signal an event, making the fd readable.  Multiple calls to `post()` may be coalesced.
/// On Linux this uses eventfd, on other systems this uses a pipe.
/// [`try_consume()`](FdEventSignaller::try_consume) may be used to consume the event.
/// Importantly this is async signal safe. Of course it is `CLO_EXEC` as well.
pub struct FdEventSignaller {
    // Always the read end of the fd; maybe the write end as well.
    fd: OwnedFd,
    #[cfg(not(have_eventfd))]
    write: OwnedFd,
}

impl FdEventSignaller {
    /// The default constructor will abort on failure (fd exhaustion).
    /// This should only be used during startup.
    pub fn new() -> Self {
        cfg_if!(
            if #[cfg(have_eventfd)] {
                // Note we do not want to use EFD_SEMAPHORE because we are binary (not counting) semaphore.
                let fd = unsafe { libc::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK) };
                if fd < 0 {
                    perror("eventfd");
                    exit_without_destructors(1);
                }
                return Self {
                    fd: unsafe { OwnedFd::from_raw_fd(fd) },
                };
            } else {
                // Implementation using pipes.
                let Ok(pipes) = make_autoclose_pipes() else {
                    exit_without_destructors(1);
                };
                make_fd_nonblocking(pipes.read.as_raw_fd()).unwrap();
                make_fd_nonblocking(pipes.write.as_raw_fd()).unwrap();
                return Self {
                    fd: pipes.read,
                    write: pipes.write,
                };
            }
        );
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
        cfg_if!(
            if #[cfg(have_eventfd)] {
                let mut buff = [0_u64; 1];
            } else {
                let mut buff = [0_u8; 1024];
            }
        );
        let mut ret;
        loop {
            ret = unsafe {
                libc::read(
                    self.read_fd(),
                    buff.as_mut_ptr().cast(),
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
        cfg_if!(
            if #[cfg(have_eventfd)] {
                let c = 1_u64;
            } else {
                let c = 1_u8;
            }
        );
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
        let timeout = if wait {
            Timeout::Forever
        } else {
            Timeout::ZERO
        };
        FdReadableSet::is_fd_readable(self.read_fd(), timeout)
    }

    /// Return the fd to write to.
    fn write_fd(&self) -> RawFd {
        cfg_if!(
            if #[cfg(have_eventfd)] {
                return self.fd.as_raw_fd();
            } else {
                return self.write.as_raw_fd();
            }
        );
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
/// `FdMonitorItem`'s [`FdMonitorItem::fd`]. If the fd is closed, the callback will not
/// be invoked again.
pub type Callback = Box<dyn Fn(&mut AutoCloseFd) + Send + Sync>;

/// An item containing an fd and callback, which can be monitored to watch when it becomes readable
/// and invoke the callback.
pub struct FdMonitorItem {
    /// The fd to monitor
    fd: AutoCloseFd,
    /// A callback to be invoked when the fd is readable, or for another reason given by the wake reason.
    /// If the fd is invalid on return from the function, then the item is removed from the [`FdMonitor`] set.
    callback: Callback,
}

impl FdMonitorItem {
    /// Invoke this item's callback because the fd is readable.
    /// If the given fd is closed, it will be removed from the [`FdMonitor`] set.
    fn service(&mut self) {
        (self.callback)(&mut self.fd)
    }
}

/// A thread-safe class which can monitor a set of fds, invoking a callback when any becomes
/// readable (or has been HUP'd).
pub struct FdMonitor {
    /// Our self-signaller, used to wake up the background thread out of select().
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
    /// Note the locking here is very coarse and the lock is held while servicing items.
    /// This means that an item which reads a lot of data may prevent adding other items.
    /// When we do true multithreaded execution, we may want to make the locking more fine-grained (per-item).
    data: Arc<Mutex<SharedData>>,
}

impl FdMonitor {
    /// Add an item to the monitor. Returns the [`FdMonitorItemId`] assigned to the item.
    pub fn add(&self, fd: AutoCloseFd, callback: Callback) -> FdMonitorItemId {
        assert!(fd.is_valid());

        let item_id = self.last_id.fetch_add(1, Ordering::Relaxed) + 1;
        let item_id = FdMonitorItemId(item_id);
        let item: FdMonitorItem = FdMonitorItem { fd, callback };
        let start_thread = {
            // Lock around a local region
            let mut data = self.data.lock().expect("Mutex poisoned!");

            // Assign an id and add the item.
            let old_value = data.items.insert(item_id, item);
            assert!(old_value.is_none(), "Item ID {} already exists!", item_id.0);

            // Start the thread if it hasn't already been started
            let already_started = data.running;
            data.running = true;
            !already_started
        };

        if start_thread {
            flog!(fd_monitor, "Thread starting");
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

    /// Remove an item from the monitor and return its file descriptor.
    /// Note we may remove an item whose fd is currently being waited on in select(); this is
    /// considered benign because the underlying item will no longer be present and so its
    /// callback will not be invoked.
    pub fn remove_item(&self, item_id: FdMonitorItemId) -> AutoCloseFd {
        assert!(item_id.0 > 0, "Invalid item id!");
        let mut data = self.data.lock().expect("Mutex poisoned!");
        let removed = data.items.remove(&item_id).expect("Item ID not found");
        drop(data);
        // Allow it to recompute the wait set.
        self.change_signaller.post();
        removed.fd
    }

    pub fn new() -> Self {
        Self {
            data: Arc::new(Mutex::new(SharedData {
                items: HashMap::new(),
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
            //
            // This avoids the ABA problem because ItemIDs are never recycled. It does have a race where
            // we might select() on a file descriptor that has been closed or recycled. Thus we must be
            // prepared to handle EBADF. This race is otherwise considered benign.

            // Construct the set of fds to monitor.
            // Our change_signaller is special-cased.
            fds.clear();
            let change_signal_fd = self.change_signaller.read_fd();
            fds.add(change_signal_fd);

            // Grab the lock and snapshot the item_ids. Skip items with invalid fds.
            let mut data = self.data.lock().expect("Mutex poisoned!");
            item_ids.clear();
            item_ids.reserve(data.items.len());
            for (item_id, item) in &data.items {
                let fd = item.fd.as_raw_fd();
                if fd >= 0 {
                    fds.add(fd);
                    item_ids.push(*item_id);
                }
            }

            // Sort it to avoid the non-determinism of the hash table.
            item_ids.sort_unstable();

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
            //
            // Note that WSLv1 doesn't throw EBADF if the fd is closed is mid-select.
            drop(data);
            let ret =
                fds.check_readable(timeout.map(Timeout::Duration).unwrap_or(Timeout::Forever));
            // Cygwin reports ret < 0 && errno == 0 as success.
            let err = errno().0;
            if ret < 0 && !matches!(err, libc::EINTR | libc::EBADF) && !(cfg!(cygwin) && err == 0) {
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
                if fds.test(item.fd.as_raw_fd()) {
                    item.service();
                }
            }

            // Handle any changes if the change signaller was set. Alternatively, this may be the
            // wait lap, in which case we might want to commit to exiting.
            let change_signalled = fds.test(change_signal_fd);
            if change_signalled || is_wait_lap {
                // Clear the change signaller before processing incoming changes
                self.change_signaller.try_consume();

                if data.terminate || (is_wait_lap && data.items.is_empty() && !change_signalled) {
                    // Maybe terminate is set. Alternatively, maybe we had no items, waited a bit,
                    // and still have no items. It's important to do this while holding the lock,
                    // otherwise we race with new items being added.
                    assert!(
                        data.running,
                        "Thread should be running because we're that thread"
                    );
                    flog!(fd_monitor, "Thread exiting");
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

#[cfg(test)]
mod tests {
    #[cfg(not(target_has_atomic = "64"))]
    use portable_atomic::AtomicU64;
    use std::fs::File;
    use std::io::Write;
    use std::os::fd::{AsRawFd, IntoRawFd, OwnedFd};
    #[cfg(target_has_atomic = "64")]
    use std::sync::atomic::AtomicU64;
    use std::sync::atomic::{AtomicUsize, Ordering};
    use std::sync::{Arc, Barrier, Mutex};
    use std::thread;
    use std::time::Duration;

    use errno::errno;

    use crate::fd_monitor::{FdEventSignaller, FdMonitor};
    use crate::fd_readable_set::{FdReadableSet, Timeout};
    use crate::fds::{AutoCloseFd, AutoClosePipes, make_autoclose_pipes};
    use crate::tests::prelude::*;

    /// Helper to make an item which counts how many times its callback was invoked.
    ///
    /// This could be structured differently to avoid the `Mutex` on `writer`, but it's not worth it
    /// since this is just used for test purposes.
    struct ItemMaker {
        pub length_read: AtomicUsize,
        pub total_calls: AtomicUsize,
        item_id: AtomicU64,
        pub always_close: bool,
        pub writer: Mutex<Option<File>>,
    }

    impl ItemMaker {
        pub fn insert_new_into(monitor: &FdMonitor) -> Arc<Self> {
            Self::insert_new_into2(monitor, |_| {})
        }

        pub fn insert_new_into2<F: Fn(&mut Self)>(monitor: &FdMonitor, config: F) -> Arc<Self> {
            let pipes = make_autoclose_pipes().expect("fds exhausted!");

            let mut result = ItemMaker {
                length_read: 0.into(),
                total_calls: 0.into(),
                item_id: 0.into(),
                always_close: false,
                writer: Mutex::new(Some(File::from(pipes.write))),
            };

            config(&mut result);

            let result = Arc::new(result);
            let callback = {
                let result = Arc::clone(&result);
                move |fd: &mut AutoCloseFd| result.callback(fd)
            };
            let fd = AutoCloseFd::new(pipes.read.into_raw_fd());
            let item_id = monitor.add(fd, Box::new(callback));
            result.item_id.store(u64::from(item_id), Ordering::Relaxed);

            result
        }

        fn callback(&self, fd: &mut AutoCloseFd) {
            let mut buf = [0u8; 1024];
            let res = nix::unistd::read(&fd, &mut buf);
            let amt = res.expect("read error!");
            self.length_read.fetch_add(amt, Ordering::Relaxed);
            let was_closed = amt == 0;

            self.total_calls.fetch_add(1, Ordering::Relaxed);
            if was_closed || self.always_close {
                fd.close();
            }
        }

        /// Write 42 bytes to our write end.
        fn write42(&self) {
            let buf = [0u8; 42];
            let mut writer = self.writer.lock().expect("Mutex poisoned!");
            writer
                .as_mut()
                .unwrap()
                .write_all(&buf)
                .expect("Error writing 42 bytes to pipe!");
        }
    }

    #[test]
    #[serial]
    fn fd_monitor_items() {
        let _cleanup = test_init();
        let monitor = FdMonitor::new();

        // Item which will never receive data or be called.
        let item_never = ItemMaker::insert_new_into(&monitor);

        // Item which should get exactly 42 bytes.
        let item42 = ItemMaker::insert_new_into(&monitor);

        // Item which should get 42 bytes then get notified it is closed.
        let item42_then_close = ItemMaker::insert_new_into(&monitor);

        // Item which should get a callback exactly once.
        let item_oneshot = ItemMaker::insert_new_into2(&monitor, |item| {
            item.always_close = true;
        });

        item42.write42();
        item42_then_close.write42();
        *item42_then_close.writer.lock().expect("Mutex poisoned!") = None;
        item_oneshot.write42();

        // May need to loop here to ensure our fd_monitor gets scheduled. See #7699.
        for _ in 0..100 {
            std::thread::sleep(Duration::from_millis(84));
            if item_oneshot.total_calls.load(Ordering::Relaxed) > 0 {
                break;
            }
        }

        drop(monitor);

        assert_eq!(item_never.length_read.load(Ordering::Relaxed), 0);

        assert_eq!(item42.length_read.load(Ordering::Relaxed), 42);

        assert_eq!(item42_then_close.length_read.load(Ordering::Relaxed), 42);
        assert_eq!(item42_then_close.total_calls.load(Ordering::Relaxed), 2);

        assert_eq!(item_oneshot.length_read.load(Ordering::Relaxed), 42);
        assert_eq!(item_oneshot.total_calls.load(Ordering::Relaxed), 1);
    }

    #[test]
    fn test_fd_event_signaller() {
        let sema = FdEventSignaller::new();
        assert!(!sema.try_consume());
        assert!(!sema.poll(false));

        // Post once.
        sema.post();
        assert!(sema.poll(false));
        assert!(sema.poll(false));
        assert!(sema.try_consume());
        assert!(!sema.poll(false));
        assert!(!sema.try_consume());

        // Posts are coalesced.
        sema.post();
        sema.post();
        sema.post();
        assert!(sema.poll(false));
        assert!(sema.poll(false));
        assert!(sema.try_consume());
        assert!(!sema.poll(false));
        assert!(!sema.try_consume());
    }

    // A helper function which calls poll() or selects() on a file descriptor in the background,
    // and then invokes the `bad_action` function on the file descriptor while the poll/select is
    // waiting. The function returns Result<i32, i32>: either the number of readable file descriptors
    // or the error code from poll/select.
    #[cfg(test)]
    fn do_something_bad_during_select<F>(bad_action: F) -> Result<i32, i32>
    where
        F: FnOnce(OwnedFd) -> Option<OwnedFd>,
    {
        let AutoClosePipes {
            read: read_fd,
            write: write_fd,
        } = make_autoclose_pipes().expect("Failed to create pipe");
        let raw_read_fd = read_fd.as_raw_fd();

        // Try to ensure that the thread will be scheduled by waiting until it is.
        let barrier = Arc::new(Barrier::new(2));
        let barrier_clone = Arc::clone(&barrier);

        let select_thread = thread::spawn(move || -> Result<i32, i32> {
            let mut fd_set = FdReadableSet::new();
            fd_set.add(raw_read_fd);

            barrier_clone.wait();

            // Timeout after 500 msec.
            // macOS will eagerly return EBADF if the fd is closed; Linux will hit the timeout.
            let timeout = Timeout::Duration(Duration::from_millis(500));
            let ret = fd_set.check_readable(timeout);
            if ret < 0 { Err(errno().0) } else { Ok(ret) }
        });

        barrier.wait();
        thread::sleep(Duration::from_millis(100));
        let read_fd = bad_action(read_fd);

        let result = select_thread.join().expect("Select thread panicked");
        // Ensure these stay alive until after thread is joined.
        drop(read_fd);
        drop(write_fd);
        result
    }

    #[test]
    fn test_close_during_select_ebadf() {
        use crate::common::{WSL, is_windows_subsystem_for_linux as is_wsl};
        let close_it = |read_fd: OwnedFd| {
            drop(read_fd);
            None
        };
        let result = do_something_bad_during_select(close_it);

        // WSLv1 does not error out with EBADF if the fd is closed mid-select.
        // This is OK because we do not _depend_ on this behavior; the only
        // true requirement is that we don't panic in the handling code above.
        assert!(
            is_wsl(WSL::V1) || matches!(result, Err(libc::EBADF) | Ok(0 | 1)),
            "select/poll should have failed with EBADF or timed out or the fd should be ready"
        );
    }

    #[test]
    fn test_dup2_during_select_ebadf() {
        // Make a random file descriptor that we can dup2 stdin to.
        let AutoClosePipes {
            read: pipe_read,
            write: pipe_write,
        } = make_autoclose_pipes().expect("Failed to create pipe");

        let dup2_it = |read_fd: OwnedFd| {
            // We are going to dup2 stdin to this fd, which should cause select/poll to fail.
            assert!(read_fd.as_raw_fd() > 0, "fd should be valid and not stdin");
            unsafe { libc::dup2(pipe_read.as_raw_fd(), read_fd.as_raw_fd()) };
            Some(read_fd)
        };
        let result = do_something_bad_during_select(dup2_it);
        assert!(
            matches!(result, Err(libc::EBADF) | Ok(0 | 1)),
            "select/poll should have failed with EBADF or timed out or the fd should be ready"
        );
        // Ensure these stay alive until after thread is joined.
        drop(pipe_read);
        drop(pipe_write);
    }
}
