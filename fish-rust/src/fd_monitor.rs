use std::os::fd::{AsRawFd, RawFd};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use self::fd_monitor::{new_fd_event_signaller, FdEventSignaller, ItemWakeReason};
use crate::fd_readable_set::FdReadableSet;
use crate::fds::AutoCloseFd;
use crate::ffi::void_ptr;
use crate::flog::FLOG;
use crate::threads::assert_is_background_thread;
use crate::wutil::perror;
use cxx::SharedPtr;

#[cxx::bridge]
mod fd_monitor {
    /// Reason for waking an item
    #[repr(u8)]
    #[cxx_name = "item_wake_reason_t"]
    enum ItemWakeReason {
        /// The fd became readable (or was HUP'd)
        Readable,
        /// The requested timeout was hit
        Timeout,
        /// The item was "poked" (woken up explicitly)
        Poke,
    }

    unsafe extern "C++" {
        include!("fds.h");

        /// An event signaller implemented using a file descriptor, so it can plug into
        /// [`select()`](libc::select).
        ///
        /// This is like a binary semaphore. A call to [`post()`](FdEventSignaller::post) will
        /// signal an event, making the fd readable.  Multiple calls to `post()` may be coalesced.
        /// On Linux this uses [`eventfd()`](libc::eventfd), on other systems this uses a pipe.
        /// [`try_consume()`](FdEventSignaller::try_consume) may be used to consume the event.
        /// Importantly this is async signal safe. Of course it is `CLO_EXEC` as well.
        #[rust_name = "FdEventSignaller"]
        type fd_event_signaller_t = crate::ffi::fd_event_signaller_t;
        #[rust_name = "new_fd_event_signaller"]
        fn ffi_new_fd_event_signaller_t() -> SharedPtr<FdEventSignaller>;
    }
    extern "Rust" {
        #[cxx_name = "fd_monitor_item_id_t"]
        type FdMonitorItemId;
    }

    extern "Rust" {
        #[cxx_name = "fd_monitor_item_t"]
        type FdMonitorItem;

        #[cxx_name = "make_fd_monitor_item_t"]
        fn new_fd_monitor_item_ffi(
            fd: i32,
            timeout_usecs: u64,
            callback: *const u8,
            param: *const u8,
        ) -> Box<FdMonitorItem>;
    }

    extern "Rust" {
        #[cxx_name = "fd_monitor_t"]
        type FdMonitor;

        #[cxx_name = "make_fd_monitor_t"]
        fn new_fd_monitor_ffi() -> Box<FdMonitor>;

        #[cxx_name = "add_item"]
        fn add_item_ffi(
            &mut self,
            fd: i32,
            timeout_usecs: u64,
            callback: *const u8,
            param: *const u8,
        ) -> u64;

        #[cxx_name = "poke_item"]
        fn poke_item_ffi(&self, item_id: u64);

        #[cxx_name = "add"]
        pub fn add_ffi(&mut self, item: Box<FdMonitorItem>) -> u64;
    }
}

// TODO: Remove once we're no longer using the FFI variant of FdEventSignaller
unsafe impl Sync for FdEventSignaller {}
unsafe impl Send for FdEventSignaller {}

/// Each item added to fd_monitor_t is assigned a unique ID, which is not recycled. Items may have
/// their callback triggered immediately by passing the ID. Zero is a sentinel.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct FdMonitorItemId(u64);

type FfiCallback = extern "C" fn(*mut AutoCloseFd, u8, void_ptr);

/// The callback type used by [`FdMonitorItem`]. It is passed a mutable reference to the
/// `FdMonitorItem`'s [`FdMonitorItem::fd`] and [the reason](ItemWakeupReason) for the wakeup. The
/// callback may close the fd, in which case the `FdMonitorItem` is removed from [`FdMonitor`]'s
/// set.
///
/// As capturing C++ closures can't be safely used via ffi interop and cxx bridge doesn't support
/// passing typed `fn(...)` pointers from C++ to rust, we have a separate variant of the type that
/// uses the C abi to invoke a callback. This will be removed when the dependent C++ code (currently
/// only `src/io.cpp`) is ported to rust
enum FdMonitorCallback {
    None,
    Native(Box<dyn Fn(&mut AutoCloseFd, ItemWakeReason) + Send + Sync>),
    Ffi(FfiCallback /* fn ptr */, void_ptr /* param */),
}

/// An item containing an fd and callback, which can be monitored to watch when it becomes readable
/// and invoke the callback.
pub struct FdMonitorItem {
    /// The fd to monitor
    fd: AutoCloseFd,
    /// A callback to be invoked when the fd is readable, or when we are timed out. If we time out,
    /// then timed_out will be true. If the fd is invalid on return from the function, then the item
    /// is removed from the [`FdMonitor`] set.
    callback: FdMonitorCallback,
    /// The timeout associated with waiting on this item or `None` to wait indefinitely. A timeout
    /// of `0` is not supported.
    timeout: Option<Duration>,
    /// The last time we were called or the time of initialization.
    last_time: Option<Instant>,
    /// The id for this item, assigned by [`FdMonitor`].
    item_id: FdMonitorItemId,
}

/// Unlike C++, rust's `Vec` has `Vec::retain()` instead of `std::remove_if(...)` with the inverse
/// logic. It's hard to keep track of which bool means what across the different layers, so be more
/// explicit.
#[derive(PartialEq, Eq)]
enum ItemAction {
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
            match &self.callback {
                FdMonitorCallback::None => panic!("Callback not assigned!"),
                FdMonitorCallback::Native(callback) => (callback)(&mut self.fd, reason),
                FdMonitorCallback::Ffi(callback, param) => {
                    // Safety: identical objects are generated on both sides by cxx bridge as
                    // integers of the same size (minimum size to fit the enum).
                    let reason = unsafe { std::mem::transmute(reason) };
                    (callback)(&mut self.fd as *mut _, reason, *param)
                }
            }
            if !self.fd.is_valid() {
                result = ItemAction::Remove;
            }
        }
        return result;
    }

    /// Invoke this item's callback with a poke, if its id is present in the sorted poke list.
    fn maybe_poke_item(&mut self, pokelist: &[FdMonitorItemId]) -> ItemAction {
        if self.item_id.0 == 0 || pokelist.binary_search(&self.item_id).is_err() {
            // Not pokeable or not in the poke list.
            return ItemAction::Retain;
        }

        match &self.callback {
            FdMonitorCallback::None => panic!("Callback not assigned!"),
            FdMonitorCallback::Native(callback) => (callback)(&mut self.fd, ItemWakeReason::Poke),
            FdMonitorCallback::Ffi(callback, param) => {
                // Safety: identical objects are generated on both sides by cxx bridge as
                // integers of the same size (minimum size to fit the enum).
                let reason = unsafe { std::mem::transmute(ItemWakeReason::Poke) };
                (callback)(&mut self.fd as *mut _, reason, *param)
            }
        }
        // Return `ItemAction::Remove` if the callback closed the fd
        match self.fd.is_valid() {
            true => ItemAction::Retain,
            false => ItemAction::Remove,
        }
    }

    fn new() -> Self {
        Self {
            callback: FdMonitorCallback::None,
            fd: AutoCloseFd::empty(),
            timeout: None,
            last_time: None,
            item_id: FdMonitorItemId(0),
        }
    }

    fn set_callback_ffi(&mut self, callback: *const u8, param: *const u8) {
        // Safety: we are just marshalling our function pointers with identical definitions on both
        // sides of the ffi bridge as void pointers to keep cxx bridge happy. Whether we invoke the
        // raw function as a void pointer or as a typed fn that helps us keep track of what we're
        // doing is unsafe in all cases, so might as well make the best of it.
        let callback = unsafe { std::mem::transmute(callback) };
        self.callback = FdMonitorCallback::Ffi(callback, param.into());
    }
}

// cxx bridge does not support "static member functions" in C++ or rust, so we need a top-level fn.
fn new_fd_monitor_ffi() -> Box<FdMonitor> {
    Box::new(FdMonitor::new())
}

// cxx bridge does not support "static member functions" in C++ or rust, so we need a top-level fn.
fn new_fd_monitor_item_ffi(
    fd: RawFd,
    timeout_usecs: u64,
    callback: *const u8,
    param: *const u8,
) -> Box<FdMonitorItem> {
    // Safety: we are just marshalling our function pointers with identical definitions on both
    // sides of the ffi bridge as void pointers to keep cxx bridge happy. Whether we invoke the
    // raw function as a void pointer or as a typed fn that helps us keep track of what we're
    // doing is unsafe in all cases, so might as well make the best of it.
    let callback = unsafe { std::mem::transmute(callback) };
    let mut item = FdMonitorItem::new();
    item.fd.reset(fd);
    item.callback = FdMonitorCallback::Ffi(callback, param.into());
    if timeout_usecs != FdReadableSet::kNoTimeout {
        item.timeout = Some(Duration::from_micros(timeout_usecs));
    }
    return Box::new(item);
}

/// A thread-safe class which can monitor a set of fds, invoking a callback when any becomes
/// readable (or has been HUP'd) or when per-item-configurable timeouts are reached.
pub struct FdMonitor {
    /// Our self-signaller. When this is written to, it means there are new items pending, new items
    /// in the poke list, or terminate has been set.
    change_signaller: SharedPtr<FdEventSignaller>,
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
    change_signaller: SharedPtr<FdEventSignaller>,
    /// The data shared between the background thread and the `FdMonitor` instance.
    data: Arc<Mutex<SharedData>>,
}

impl FdMonitor {
    pub fn add_ffi(&self, item: Box<FdMonitorItem>) -> u64 {
        self.add(*item).0
    }

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
                change_signaller: SharedPtr::clone(&self.change_signaller),
                items: Vec::new(),
            };
            crate::threads::spawn(move || {
                background_monitor.run();
            });
        }

        item_id
    }

    /// Avoid requiring a separate UniquePtr for each item C++ wants to add to the set by giving an
    /// all-in-one entry point that can initialize the item on our end and insert it to the set.
    fn add_item_ffi(
        &mut self,
        fd: RawFd,
        timeout_usecs: u64,
        callback: *const u8,
        param: *const u8,
    ) -> u64 {
        // Safety: we are just marshalling our function pointers with identical definitions on both
        // sides of the ffi bridge as void pointers to keep cxx bridge happy. Whether we invoke the
        // raw function as a void pointer or as a typed fn that helps us keep track of what we're
        // doing is unsafe in all cases, so might as well make the best of it.
        let callback = unsafe { std::mem::transmute(callback) };
        let mut item = FdMonitorItem::new();
        item.fd.reset(fd);
        item.callback = FdMonitorCallback::Ffi(callback, param.into());
        if timeout_usecs != FdReadableSet::kNoTimeout {
            item.timeout = Some(Duration::from_micros(timeout_usecs));
        }
        let item_id = self.add(item).0;
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

    fn poke_item_ffi(&self, item_id: u64) {
        self.poke_item(FdMonitorItemId(item_id))
    }

    pub fn new() -> Self {
        Self {
            data: Arc::new(Mutex::new(SharedData {
                pending: Vec::new(),
                pokelist: Vec::new(),
                running: false,
                terminate: false,
            })),
            change_signaller: new_fd_event_signaller(),
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
                self.poke(&mut pokelist);
                pokelist.clear();
            }
            fds.clear();

            // Our change_signaller is special-cased
            let change_signal_fd = self.change_signaller.read_fd().into();
            fds.add(change_signal_fd);

            let mut now = Instant::now();
            // Use Duration::MAX to represent no timeout for comparison purposes.
            let mut timeout = Duration::MAX;

            for item in &mut self.items {
                fds.add(item.fd.as_raw_fd());
                if !item.last_time.is_some() {
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
                if item.service_item(&fds, &now) == ItemAction::Remove {
                    FLOG!(fd_monitor, "Removing fd", fd);
                    return ItemAction::Remove;
                }
                return ItemAction::Retain;
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
                self.change_signaller.try_consume();
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
            let action = item.maybe_poke_item(&*pokelist);
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
