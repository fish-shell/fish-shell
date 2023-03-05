use std::io::Write;
use std::os::fd::AsRawFd;
use std::sync::atomic::{AtomicBool, AtomicU64, AtomicUsize, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use crate::fd_monitor::{FdMonitor, FdMonitorItem, FdMonitorItemId, ItemWakeReason};
use crate::fds::{make_autoclose_pipes, AutoCloseFd};
use crate::ffi_tests::add_test;

/// Helper to make an item which counts how many times its callback was invoked.
///
/// This could be structured differently to avoid the `Mutex` on `writer`, but it's not worth it
/// since this is just used for test purposes.
struct ItemMaker {
    pub did_timeout: AtomicBool,
    pub length_read: AtomicUsize,
    pub pokes: AtomicUsize,
    pub total_calls: AtomicUsize,
    item_id: AtomicU64,
    pub always_exit: bool,
    pub writer: Mutex<AutoCloseFd>,
}

impl ItemMaker {
    pub fn insert_new_into(monitor: &FdMonitor, timeout: Option<Duration>) -> Arc<Self> {
        Self::insert_new_into2(monitor, timeout, |_| {})
    }

    pub fn insert_new_into2<F: Fn(&mut Self)>(
        monitor: &FdMonitor,
        timeout: Option<Duration>,
        config: F,
    ) -> Arc<Self> {
        let pipes = make_autoclose_pipes().expect("fds exhausted!");
        let mut item = FdMonitorItem::new(pipes.read, timeout, None);

        let mut result = ItemMaker {
            did_timeout: false.into(),
            length_read: 0.into(),
            pokes: 0.into(),
            total_calls: 0.into(),
            item_id: 0.into(),
            always_exit: false,
            writer: Mutex::new(pipes.write),
        };

        config(&mut result);

        let result = Arc::new(result);
        let callback = {
            let result = Arc::clone(&result);
            move |fd: &mut AutoCloseFd, reason: ItemWakeReason| {
                result.callback(fd, reason);
            }
        };
        item.set_callback(Box::new(callback));
        let item_id = monitor.add(item);
        result.item_id.store(u64::from(item_id), Ordering::Relaxed);

        result
    }

    fn item_id(&self) -> FdMonitorItemId {
        self.item_id.load(Ordering::Relaxed).into()
    }

    fn callback(&self, fd: &mut AutoCloseFd, reason: ItemWakeReason) {
        let mut was_closed = false;

        match reason {
            ItemWakeReason::Timeout => {
                self.did_timeout.store(true, Ordering::Relaxed);
            }
            ItemWakeReason::Poke => {
                self.pokes.fetch_add(1, Ordering::Relaxed);
            }
            ItemWakeReason::Readable => {
                let mut buf = [0u8; 1024];
                let amt =
                    unsafe { libc::read(fd.as_raw_fd(), buf.as_mut_ptr() as *mut _, buf.len()) };
                assert_ne!(amt, -1, "read error!");
                self.length_read.fetch_add(amt as usize, Ordering::Relaxed);
                was_closed = amt == 0;
            }
            _ => unreachable!(),
        }

        self.total_calls.fetch_add(1, Ordering::Relaxed);
        if self.always_exit || was_closed {
            fd.close();
        }
    }

    /// Write 42 bytes to our write end.
    fn write42(&self) {
        let buf = [0u8; 42];
        let mut writer = self.writer.lock().expect("Mutex poisoned!");
        writer.write_all(&buf).expect("Error writing 42 bytes to pipe!");
    }
}

add_test!("fd_monitor_items", || {
    let monitor = FdMonitor::new();

    // Items which will never receive data or be called.
    let item_never = ItemMaker::insert_new_into(&monitor, None);
    let item_huge_timeout =
        ItemMaker::insert_new_into(&monitor, Some(Duration::from_millis(100_000_000)));

    // Item which should get no data and time out.
    let item0_timeout = ItemMaker::insert_new_into(&monitor, Some(Duration::from_millis(16)));

    // Item which should get exactly 42 bytes then time out.
    let item42_timeout = ItemMaker::insert_new_into(&monitor, Some(Duration::from_millis(16)));

    // Item which should get exactly 42 bytes and not time out.
    let item42_no_timeout = ItemMaker::insert_new_into(&monitor, None);

    // Item which should get 42 bytes then get notified it is closed.
    let item42_then_close = ItemMaker::insert_new_into(&monitor, Some(Duration::from_millis(16)));

    // Item which gets one poke.
    let item_pokee = ItemMaker::insert_new_into(&monitor, None);

    // Item which should get a callback exactly once.
    let item_oneshot =
        ItemMaker::insert_new_into2(&monitor, Some(Duration::from_millis(16)), |item| {
            item.always_exit = true;
        });

    item42_timeout.write42();
    item42_no_timeout.write42();
    item42_then_close.write42();
    item42_then_close
        .writer
        .lock()
        .expect("Mutex poisoned!")
        .close();
    item_oneshot.write42();

    monitor.poke_item(item_pokee.item_id());

    // May need to loop here to ensure our fd_monitor gets scheduled. See #7699.
    for _ in 0..100 {
        std::thread::sleep(Duration::from_millis(84));
        if item0_timeout.did_timeout.load(Ordering::Relaxed) {
            break;
        }
    }

    drop(monitor);

    assert_eq!(item_never.did_timeout.load(Ordering::Relaxed), false);
    assert_eq!(item_never.length_read.load(Ordering::Relaxed), 0);
    assert_eq!(item_never.pokes.load(Ordering::Relaxed), 0);

    assert_eq!(item_huge_timeout.did_timeout.load(Ordering::Relaxed), false);
    assert_eq!(item_huge_timeout.length_read.load(Ordering::Relaxed), 0);
    assert_eq!(item_huge_timeout.pokes.load(Ordering::Relaxed), 0);

    assert_eq!(item0_timeout.length_read.load(Ordering::Relaxed), 0);
    assert_eq!(item0_timeout.did_timeout.load(Ordering::Relaxed), true);
    assert_eq!(item0_timeout.pokes.load(Ordering::Relaxed), 0);

    assert_eq!(item42_timeout.length_read.load(Ordering::Relaxed), 42);
    assert_eq!(item42_timeout.did_timeout.load(Ordering::Relaxed), true);
    assert_eq!(item42_timeout.pokes.load(Ordering::Relaxed), 0);

    assert_eq!(item42_no_timeout.length_read.load(Ordering::Relaxed), 42);
    assert_eq!(item42_no_timeout.did_timeout.load(Ordering::Relaxed), false);
    assert_eq!(item42_no_timeout.pokes.load(Ordering::Relaxed), 0);

    assert_eq!(item42_then_close.did_timeout.load(Ordering::Relaxed), false);
    assert_eq!(item42_then_close.length_read.load(Ordering::Relaxed), 42);
    assert_eq!(item42_then_close.total_calls.load(Ordering::Relaxed), 2);
    assert_eq!(item42_then_close.pokes.load(Ordering::Relaxed), 0);

    assert_eq!(item_oneshot.did_timeout.load(Ordering::Relaxed), false);
    assert_eq!(item_oneshot.length_read.load(Ordering::Relaxed), 42);
    assert_eq!(item_oneshot.total_calls.load(Ordering::Relaxed), 1);
    assert_eq!(item_oneshot.pokes.load(Ordering::Relaxed), 0);

    assert_eq!(item_pokee.did_timeout.load(Ordering::Relaxed), false);
    assert_eq!(item_pokee.length_read.load(Ordering::Relaxed), 0);
    assert_eq!(item_pokee.total_calls.load(Ordering::Relaxed), 1);
    assert_eq!(item_pokee.pokes.load(Ordering::Relaxed), 1);
});
