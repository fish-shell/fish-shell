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
use crate::fds::{make_autoclose_pipes, AutoCloseFd, AutoClosePipes};
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
        self.length_read.fetch_add(amt as usize, Ordering::Relaxed);
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
        if ret < 0 {
            Err(errno().0)
        } else {
            Ok(ret)
        }
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
    use crate::common::{is_windows_subsystem_for_linux as is_wsl, WSL};
    let close_it = |read_fd: OwnedFd| {
        drop(read_fd);
        None
    };
    let result = do_something_bad_during_select(close_it);

    // WSLv1 does not error out with EBADF if the fd is closed mid-select.
    // This is OK because we do not _depend_ on this behavior; the only
    // true requirement is that we don't panic in the handling code above.
    assert!(
        is_wsl(WSL::V1) || matches!(result, Err(libc::EBADF) | Ok(1)),
        "select/poll should have failed with EBADF or marked readable"
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
        matches!(result, Err(libc::EBADF) | Ok(0)),
        "select/poll should have failed with EBADF or timed out"
    );
    // Ensure these stay alive until after thread is joined.
    drop(pipe_read);
    drop(pipe_write);
}
