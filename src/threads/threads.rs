//! Support for thread pools and thread management.
use crate::flog::{FLOG, FloggableDebug};
use std::marker::PhantomData;
use std::mem::MaybeUninit;
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::sync::{Arc, Mutex, OnceLock};
use std::time::Duration;

impl FloggableDebug for std::thread::ThreadId {}

/// The thread id of the main thread, as set by [`init()`] at startup.
static MAIN_THREAD_ID: OnceLock<usize> = OnceLock::new();
/// Used to bypass thread assertions when testing.
const THREAD_ASSERTS_CFG_FOR_TESTING: bool = cfg!(test);
/// This allows us to notice when we've forked.
static IS_FORKED_PROC: AtomicBool = AtomicBool::new(false);

/// How long an idle [`ThreadPool`] thread will wait for work (against the condition variable)
/// before exiting.
const IO_WAIT_FOR_WORK_DURATION: Duration = Duration::from_millis(500);

/// A [`ThreadPool`] work request.
type WorkItem = Box<dyn FnOnce() + 'static + Send>;

/// Initialize some global static variables. Must be called at startup from the main thread.
pub fn init() {
    MAIN_THREAD_ID
        .set(thread_id())
        .expect("threads::init() must only be called once (at startup)!");

    extern "C" fn child_post_fork() {
        IS_FORKED_PROC.store(true, Ordering::Relaxed);
    }
    unsafe {
        let result = libc::pthread_atfork(None, None, Some(child_post_fork));
        assert_eq!(result, 0, "pthread_atfork() failure: {}", errno::errno());
    }
}

#[inline(always)]
fn main_thread_id() -> usize {
    #[cold]
    fn init_not_called() -> ! {
        panic!("threads::init() was not called at startup!");
    }

    match MAIN_THREAD_ID.get() {
        None => init_not_called(),
        Some(id) => *id,
    }
}

/// Get the calling thread's fish-specific thread id.
///
/// This thread id is internal to the `threads` module for low-level purposes and should not be
/// leaked to other modules; general purpose code that needs a thread id should use rust's native
/// thread id functionality.
///
/// We use our own implementation because Rust's own `Thread::id()` allocates via `Arc`, is fairly
/// slow, and uses a `Mutex` on 32-bit platforms (or anywhere without an atomic 64-bit CAS).
#[inline(always)]
fn thread_id() -> usize {
    static THREAD_COUNTER: AtomicUsize = AtomicUsize::new(1);
    // It would be faster and much nicer to use #[thread_local] here, but that's nightly only.
    // This is still faster than going through Thread::thread_id(); it's something like 15ns
    // for each `Thread::thread_id()` call vs 1-2 ns with `#[thread_local]` and 2-4ns with
    // `thread_local!`.
    thread_local! {
        static THREAD_ID: usize = THREAD_COUNTER.fetch_add(1, Ordering::Relaxed);
    }
    let id = THREAD_ID.with(|id| *id);
    // This assertion is only here to reduce hair loss in case someone runs into a known linker bug;
    // as it's not here to catch logic errors in our own code, it can be elided in release mode.
    debug_assert_ne!(id, 0, "TLS storage not initialized!");
    id
}

#[inline(always)]
pub fn is_main_thread() -> bool {
    thread_id() == main_thread_id()
}

#[inline(always)]
pub fn assert_is_main_thread() {
    #[cold]
    fn not_main_thread() -> ! {
        panic!("Function is not running on the main thread!");
    }

    if !is_main_thread() && !THREAD_ASSERTS_CFG_FOR_TESTING {
        not_main_thread();
    }
}

#[inline(always)]
pub fn assert_is_background_thread() {
    #[cold]
    fn not_background_thread() -> ! {
        panic!("Function is not allowed to be called on the main thread!");
    }

    if is_main_thread() && !THREAD_ASSERTS_CFG_FOR_TESTING {
        not_background_thread();
    }
}

pub fn is_forked_child() -> bool {
    IS_FORKED_PROC.load(Ordering::Relaxed)
}

#[inline(always)]
pub fn assert_is_not_forked_child() {
    #[cold]
    fn panic_is_forked_child() {
        panic!("Function called from forked child!");
    }

    if is_forked_child() {
        panic_is_forked_child();
    }
}

/// Spawn a new thread to run the given callback.
/// Returns a boolean indicating whether or not the thread was successfully launched. Failure here
/// is not dependent on the passed callback and implies a system error (likely insufficient
/// resources).
pub fn spawn<F: FnOnce() + Send + 'static>(callback: F) -> bool {
    // The spawned thread inherits our signal mask. Temporarily block signals, spawn the thread, and
    // then restore it. But we must not block SIGBUS, SIGFPE, SIGILL, or SIGSEGV; that's undefined
    // (#7837). Conservatively don't try to mask SIGKILL or SIGSTOP either; that's ignored on Linux
    // but maybe has an effect elsewhere.
    let saved_set = unsafe {
        let mut new_set = MaybeUninit::uninit();
        let new_set = new_set.as_mut_ptr();
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
    let result = match std::thread::Builder::new().spawn(callback) {
        Ok(handle) => {
            let thread_id = thread_id();
            FLOG!(iothread, "rust thread", thread_id, "spawned");
            // Drop the handle to detach the thread
            drop(handle);
            true
        }
        Err(e) => {
            eprintf!("rust thread spawn failure: %s\n", e);
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

/// Exits calling onexit handlers if running under ASAN, otherwise does nothing.
///
/// This function is always defined but is a no-op if not running under ASAN. This is to make it
/// more ergonomic to call it in general and also makes it possible to call it via ffi at all.
pub fn asan_maybe_exit(code: i32) {
    if cfg!(feature = "asan") {
        unsafe {
            libc::exit(code);
        }
    }
}

/// Data shared between the thread pool [`ThreadPool`] and worker threads [`WorkerThread`].
#[derive(Default)]
struct ThreadPoolProtected {
    /// The queue of outstanding, unclaimed work requests
    pub request_queue: std::collections::VecDeque<WorkItem>,
    /// The number of threads that exist in the pool
    pub total_threads: usize,
    /// The number of threads waiting for more work (i.e. idle threads)
    pub waiting_threads: usize,
}

pub struct ThreadPool {
    /// The mutex to access shared state between [`ThreadPool`] and [`WorkerThread`] instances. This
    /// is accessed both standalone and via [`cond_var`](Self::cond_var).
    shared: Mutex<ThreadPoolProtected>,
    /// The condition variable used to wake up waiting threads. This is tied to [`mutex`](Self::mutex).
    cond_var: std::sync::Condvar,
    /// The minimum number of threads that will be kept waiting even when idle in the pool.
    soft_min_threads: usize,
    /// The maximum number of threads that will be created to service outstanding work requests, by
    /// default. This may be bypassed.
    max_threads: usize,
}

impl std::fmt::Debug for ThreadPool {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("ThreadPool")
            .field("min_threads", &self.soft_min_threads)
            .field("max_threads", &self.max_threads)
            .finish()
    }
}

impl ThreadPool {
    /// Construct a new `ThreadPool` instance with the specified min and max num of threads.
    pub fn new(soft_min_threads: usize, max_threads: usize) -> Arc<Self> {
        Arc::new(ThreadPool {
            shared: Default::default(),
            cond_var: Default::default(),
            soft_min_threads,
            max_threads,
        })
    }

    /// Enqueue a new work item onto the thread pool.
    ///
    /// The function `func` will execute on one of the pool's background threads.
    ///
    /// Returns the number of threads that were alive when the work item was enqueued.
    pub fn perform<F: FnOnce() + 'static + Send>(self: &Arc<Self>, func: F) -> usize {
        let work_item = Box::new(func);
        enum ThreadAction {
            None,
            Wake,
            Spawn,
        }

        let local_thread_count;
        let thread_action = {
            let mut data = self.shared.lock().expect("Mutex poisoned!");
            local_thread_count = data.total_threads;
            data.request_queue.push_back(work_item);
            FLOG!(
                iothread,
                "enqueuing work item (count is ",
                data.request_queue.len(),
                ")"
            );
            if data.waiting_threads >= data.request_queue.len() {
                // There are enough waiting threads, wake one up.
                ThreadAction::Wake
            } else if data.total_threads < self.max_threads {
                // No threads are idle waiting but we can or must spawn a new thread to service the
                // request.
                data.total_threads += 1;
                ThreadAction::Spawn
            } else {
                // There is no need to do anything because we've reached the max number of threads.
                ThreadAction::None
            }
        };

        // Act only after unlocking the mutex.
        match thread_action {
            ThreadAction::None => (),
            ThreadAction::Wake => {
                // Wake a thread if we decided to do so.
                FLOG!(iothread, "notifying thread ", std::thread::current().id());
                self.cond_var.notify_one();
            }
            ThreadAction::Spawn => {
                // Spawn a thread. If this fails, it means there are already a bunch of worker
                // threads and it is very unlikely that they are all about to exit so one is likely
                // able to handle the incoming request. This means we can ignore the failure with
                // some degree of confidence. (This is also not an error we expect to routinely run
                // into under normal, non-resource-starved circumstances.)
                if self.spawn_thread() {
                    FLOG!(iothread, "pthread spawned");
                } else {
                    // We failed to spawn a thread; decrement the thread count.
                    self.shared.lock().expect("Mutex poisoned!").total_threads -= 1;
                }
            }
        }

        local_thread_count
    }

    /// Attempt to spawn a new worker thread.
    fn spawn_thread(self: &Arc<Self>) -> bool {
        let pool = Arc::clone(self);
        self::spawn(move || {
            pool.run_worker();
        })
    }
}

/// A `Sync` and `Send` wrapper for non-`Sync`/`Send` types.
/// Only allows access from the main thread.
pub struct MainThread<T> {
    data: T,
    // Make type !Send and !Sync by default
    _marker: PhantomData<*const ()>,
}

// Manually implement Send and Sync for MainThread<T> to ensure it can be shared across threads
// as long as T is 'static.
unsafe impl<T: 'static> Send for MainThread<T> {}
unsafe impl<T: 'static> Sync for MainThread<T> {}

impl<T> MainThread<T> {
    pub const fn new(value: T) -> Self {
        Self {
            data: value,
            _marker: PhantomData,
        }
    }

    pub fn get(&self) -> &T {
        assert_is_main_thread();
        &self.data
    }
}

impl ThreadPool {
    /// The worker loop entry point for this thread.
    /// This is run in a background thread.
    fn run_worker(&self) {
        while let Some(work_item) = self.dequeue_work_or_commit_to_exit() {
            FLOG!(
                iothread,
                "pthread ",
                std::thread::current().id(),
                " got work"
            );

            // Perform the work
            work_item();
        }

        FLOG!(
            iothread,
            "pthread ",
            std::thread::current().id(),
            " exiting"
        );
    }

    /// Dequeue a work item (perhaps waiting on the condition variable) or commit to exiting by
    /// reducing the active thread count.
    fn dequeue_work_or_commit_to_exit(&self) -> Option<WorkItem> {
        let mut data = self.shared.lock().expect("Mutex poisoned!");

        // If the queue is empty, check to see if we should wait. We should wait if our exiting
        // would drop us below our soft thread count minimum.
        if data.request_queue.is_empty()
            && data.total_threads == self.soft_min_threads
            && IO_WAIT_FOR_WORK_DURATION > Duration::ZERO
        {
            data.waiting_threads += 1;
            data = self
                .cond_var
                .wait_timeout(data, IO_WAIT_FOR_WORK_DURATION)
                .expect("Mutex poisoned!")
                .0;
            data.waiting_threads -= 1;
        }

        // Now that we've (perhaps) waited, see if there's something on the queue.
        let result = data.request_queue.pop_front();

        // If we are returning None then ensure we balance the thread count increment from when we
        // were created. This has to be done here in this awkward place because we've already
        // committed to exiting - we will never pick up more work. So we need to make sure to
        // decrement the thread count while holding the lock as we have effectively already exited.
        if result.is_none() {
            data.total_threads -= 1;
        }

        result
    }
}

#[cfg(test)]
mod tests {
    use super::{spawn, thread_id};

    use std::mem::MaybeUninit;
    use std::sync::{
        Arc, Condvar, Mutex,
        atomic::{AtomicI32, Ordering},
    };
    use std::time::Duration;

    #[test]
    fn test_thread_ids() {
        let start_thread_id = thread_id();
        assert_eq!(start_thread_id, thread_id());
        let spawned_thread_id = std::thread::spawn(thread_id).join();
        assert_ne!(start_thread_id, spawned_thread_id.unwrap());
    }

    #[test]
    /// Verify that spawning a thread normally via [`std::thread::spawn()`] causes the calling thread's
    /// sigmask to be inherited by the newly spawned thread.
    fn std_thread_inherits_sigmask() {
        // First change our own thread mask
        let (saved_set, t1_set) = unsafe {
            let mut new_set = MaybeUninit::uninit();
            let new_set = new_set.as_mut_ptr();
            libc::sigemptyset(new_set);
            libc::sigaddset(new_set, libc::SIGILL); // mask bad jump

            let mut saved_set: libc::sigset_t = std::mem::zeroed();
            let result = libc::pthread_sigmask(libc::SIG_BLOCK, new_set, &mut saved_set as *mut _);
            assert_eq!(result, 0, "Failed to set thread mask!");

            // Now get the current set that includes the masked SIGILL
            let mut t1_set: libc::sigset_t = std::mem::zeroed();
            let mut empty_set = MaybeUninit::uninit();
            let empty_set = empty_set.as_mut_ptr();
            libc::sigemptyset(empty_set);
            let result = libc::pthread_sigmask(libc::SIG_UNBLOCK, empty_set, &mut t1_set as *mut _);
            assert_eq!(result, 0, "Failed to get own altered thread mask!");

            (saved_set, t1_set)
        };

        // Launch a new thread that can access existing variables
        let t2_set = std::thread::scope(|_| {
            unsafe {
                // Set a new thread sigmask and verify that the old one is what we expect it to be
                let mut new_set = MaybeUninit::uninit();
                let new_set = new_set.as_mut_ptr();
                libc::sigemptyset(new_set);
                let mut saved_set2: libc::sigset_t = std::mem::zeroed();
                let result =
                    libc::pthread_sigmask(libc::SIG_BLOCK, new_set, &mut saved_set2 as *mut _);
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

    #[test]
    fn test_pthread() {
        struct Context {
            val: AtomicI32,
            condvar: Condvar,
        }
        let ctx = Arc::new(Context {
            val: AtomicI32::new(3),
            condvar: Condvar::new(),
        });
        let mutex = Mutex::new(());
        let ctx2 = ctx.clone();
        let made = spawn(move || {
            ctx2.val.fetch_add(2, Ordering::Release);
            ctx2.condvar.notify_one();
            printf!("condvar signalled\n");
        });
        assert!(made);

        let lock = mutex.lock().unwrap();
        let (_lock, timeout) = ctx
            .condvar
            .wait_timeout_while(lock, Duration::from_secs(5), |()| {
                printf!("looping with lock held\n");
                if ctx.val.load(Ordering::Acquire) != 5 {
                    printf!("test_pthread: value did not yet reach goal\n");
                    return true;
                }
                false
            })
            .unwrap();
        if timeout.timed_out() {
            panic!(concat!(
                "Timeout waiting for condition variable to be notified! ",
                "Does the platform support signalling a condvar without the mutex held?"
            ));
        }
    }
}
