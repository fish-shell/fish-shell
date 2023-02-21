//! The rusty version of iothreads from the cpp code, to be consumed by native rust code. This isn't
//! ported directly from the cpp code so we can use rust threads instead of using pthreads.

use crate::flog::FLOG;
use std::thread::{self, ThreadId};

// We don't want to use a full-blown Lazy<T> for the cached main thread id, but we can't use
// AtomicU64 since std::thread::ThreadId::as_u64() is a nightly-only feature (issue #67939,
// thread_id_value). We also can't safely transmute `ThreadId` to `NonZeroU64` because there's no
// guarantee that's what the underlying type will always be on all platforms and in all cases,
// `ThreadId` isn't marked `#[repr(transparent)]`. We could generate our own thread-local value, but
// `#[thread_local]` is nightly-only while the stable `thread_local!()` macro doesn't generate
// efficient/fast/low-overhead code.

/// The thread id of the main thread, as set by [`init()`] at startup.
static mut MAIN_THREAD_ID: Option<ThreadId> = None;

/// Initialize some global static variables. Must be called at startup from the main thread.
pub fn init() {
    unsafe {
        if MAIN_THREAD_ID.is_some() {
            panic!("threads::init() must only be called once (at startup)!");
        }
        MAIN_THREAD_ID = Some(thread::current().id());
    }
}

#[inline(always)]
fn main_thread_id() -> ThreadId {
    #[cold]
    fn init_not_called() -> ! {
        panic!("threads::init() was not called at startup!");
    }

    match unsafe { MAIN_THREAD_ID } {
        None => init_not_called(),
        Some(id) => id,
    }
}

#[inline(always)]
pub fn assert_is_main_thread() {
    #[cold]
    fn not_main_thread() -> ! {
        panic!("Function is not running on the main thread!");
    }

    if thread::current().id() != main_thread_id() {
        not_main_thread();
    }
}

#[inline(always)]
pub fn assert_is_background_thread() {
    #[cold]
    fn not_background_thread() -> ! {
        panic!("Function is not allowed to be called on the main thread!");
    }

    if thread::current().id() == main_thread_id() {
        not_background_thread();
    }
}

/// The rusty version of `iothreads::make_detached_pthread()`. We will probably need a
/// `spawn_scoped` version of the same to handle some more advanced borrow cases safely, and maybe
/// an unsafe version that doesn't do any lifetime checking akin to
/// `spawn_unchecked()`[std::thread::Builder::spawn_unchecked], which is a nightly-only feature.
///
/// Returns a boolean indicating whether or not the thread was successfully launched. Failure here
/// is not dependent on the passed callback and implies a system error (likely insufficient
/// resources).
pub fn spawn<F: FnOnce() + Send + 'static>(callback: F) -> bool {
    // The spawned thread inherits our signal mask. Temporarily block signals, spawn the thread, and
    // then restore it. But we must not block SIGBUS, SIGFPE, SIGILL, or SIGSEGV; that's undefined
    // (#7837). Conservatively don't try to mask SIGKILL or SIGSTOP either; that's ignored on Linux
    // but maybe has an effect elsewhere.
    let saved_set = unsafe {
        let mut new_set: libc::sigset_t = std::mem::zeroed();
        let new_set = &mut new_set as *mut _;
        libc::sigfillset(new_set);
        libc::sigdelset(new_set, libc::SIGILL); // bad jump
        libc::sigdelset(new_set, libc::SIGFPE); // divide-by-zero
        libc::sigdelset(new_set, libc::SIGBUS); // unaligned memory access
        libc::sigdelset(new_set, libc::SIGSEGV); // bad memory access
        libc::sigdelset(new_set, libc::SIGSTOP); // unblockable
        libc::sigdelset(new_set, libc::SIGKILL); // unblockable

        let mut saved_set: libc::sigset_t = std::mem::zeroed();
        let result = libc::pthread_sigmask(libc::SIG_BLOCK, new_set, &mut saved_set as *mut _);
        assert_eq!(result, 0, "Failed to override thread signal mask!");
        saved_set
    };

    // Spawn a thread. If this fails, it means there's already a bunch of threads; it is very
    // unlikely that they are all on the verge of exiting, so one is likely to be ready to handle
    // extant requests. So we can ignore failure with some confidence.
    // We don't have to port the PTHREAD_CREATE_DETACHED logic. Rust threads are detached
    // automatically if the returned join handle is dropped.

    let result = match std::thread::Builder::new().spawn(|| callback()) {
        Ok(handle) => {
            let id = handle.thread().id();
            FLOG!(iothread, "rust thread", id, "spawned");
            // Drop the handle to detach the thread
            drop(handle);
            true
        }
        Err(e) => {
            eprintln!("rust thread spawn failure: {e}");
            false
        }
    };

    // Restore our sigmask
    unsafe {
        let result = libc::pthread_sigmask(
            libc::SIG_SETMASK,
            &saved_set as *const _,
            std::ptr::null_mut(),
        );
        assert_eq!(result, 0, "Failed to restore thread signal mask!");
    };

    result
}

#[test]
/// Verify that spawing a thread normally via [`std::thread::spawn()`] causes the calling thread's
/// sigmask to be inherited by the newly spawned thread.
fn std_thread_inherits_sigmask() {
    // First change our own thread mask
    let (saved_set, t1_set) = unsafe {
        let mut new_set: libc::sigset_t = std::mem::zeroed();
        let new_set = &mut new_set as *mut _;
        libc::sigemptyset(new_set);
        libc::sigaddset(new_set, libc::SIGILL); // mask bad jump

        let mut saved_set: libc::sigset_t = std::mem::zeroed();
        let result = libc::pthread_sigmask(libc::SIG_BLOCK, new_set, &mut saved_set as *mut _);
        assert_eq!(result, 0, "Failed to set thread mask!");

        // Now get the current set that includes the masked SIGILL
        let mut t1_set: libc::sigset_t = std::mem::zeroed();
        let mut empty_set = std::mem::zeroed();
        let empty_set = &mut empty_set as *mut _;
        libc::sigemptyset(empty_set);
        let result = libc::pthread_sigmask(libc::SIG_UNBLOCK, empty_set, &mut t1_set as *mut _);
        assert_eq!(result, 0, "Failed to get own altered thread mask!");

        (saved_set, t1_set)
    };

    // Launch a new thread that can access existing variables
    let t2_set = std::thread::scope(|_| {
        unsafe {
            // Set a new thread sigmask and verify that the old one is what we expect it to be
            let mut new_set: libc::sigset_t = std::mem::zeroed();
            let new_set = &mut new_set as *mut _;
            libc::sigemptyset(new_set);
            let mut saved_set2: libc::sigset_t = std::mem::zeroed();
            let result = libc::pthread_sigmask(libc::SIG_BLOCK, new_set, &mut saved_set2 as *mut _);
            assert_eq!(result, 0, "Failed to get existing sigmask for new thread");
            saved_set2
        }
    });

    // Compare the sigset_t values
    unsafe {
        let t1_sigset_slice = std::slice::from_raw_parts(
            &t1_set as *const _ as *const u8,
            core::mem::size_of::<libc::sigset_t>(),
        );
        let t2_sigset_slice = std::slice::from_raw_parts(
            &t2_set as *const _ as *const u8,
            core::mem::size_of::<libc::sigset_t>(),
        );

        assert_eq!(t1_sigset_slice, t2_sigset_slice);
    };

    // Restore the thread sigset so we don't affect `cargo test`'s multithreaded test harnesses
    unsafe {
        let result = libc::pthread_sigmask(
            libc::SIG_SETMASK,
            &saved_set as *const _,
            core::ptr::null_mut(),
        );
        assert_eq!(result, 0, "Failed to restore sigmask!");
    }
}
