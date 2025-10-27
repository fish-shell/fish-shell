//! The rusty version of iothreads from the cpp code, to be consumed by native rust code. This isn't
//! ported directly from the cpp code so we can use rust threads instead of using pthreads.

use crate::flog::{FLOG, FloggableDebug};
use crate::reader::Reader;
use std::marker::PhantomData;
use std::mem::MaybeUninit;
use std::num::NonZeroU64;
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::sync::{Arc, Mutex, OnceLock};
use std::time::{Duration, Instant};

impl FloggableDebug for std::thread::ThreadId {}

/// The thread id of the main thread, as set by [`init()`] at startup.
static MAIN_THREAD_ID: OnceLock<usize> = OnceLock::new();
/// Used to bypass thread assertions when testing.
const THREAD_ASSERTS_CFG_FOR_TESTING: bool = cfg!(test);
/// This allows us to notice when we've forked.
static IS_FORKED_PROC: AtomicBool = AtomicBool::new(false);

/// Maximum number of threads for the IO thread pool.
const IO_MAX_THREADS: usize = 1024;

/// How long an idle [`ThreadPool`] thread will wait for work (against the condition variable)
/// before exiting.
const IO_WAIT_FOR_WORK_DURATION: Duration = Duration::from_millis(500);

/// The iothreads [`ThreadPool`] singleton. Used to lift I/O off of the main thread and used for
/// completions, etc.
static IO_THREAD_POOL: OnceLock<Mutex<ThreadPool>> = OnceLock::new();

/// The event signaller singleton used for completions and queued main thread requests.
static NOTIFY_SIGNALLER: once_cell::sync::Lazy<crate::fd_monitor::FdEventSignaller> =
    once_cell::sync::Lazy::new(crate::fd_monitor::FdEventSignaller::new);

/// A [`ThreadPool`] work request.
type WorkItem = Box<dyn FnOnce() + 'static + Send>;

// A helper type to allow us to (temporarily) send an object to another thread.
struct ForceSend<T>(T);

// Safety: only used on main thread.
unsafe impl<T> Send for ForceSend<T> {}

#[allow(clippy::type_complexity)]
type DebounceCallback = ForceSend<Box<dyn FnOnce(&mut Reader) + 'static>>;

/// The queue of [`WorkItem`]s to be executed on the main thread. This is read from in
/// `iothread_service_main()`.
///
/// Since the queue is synchronized, items don't need to implement `Send`.
static MAIN_THREAD_QUEUE: Mutex<Vec<DebounceCallback>> = Mutex::new(Vec::new());

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

    IO_THREAD_POOL
        .set(Mutex::new(ThreadPool::new(1, IO_MAX_THREADS)))
        .expect("IO_THREAD_POOL has already been initialized!");
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

/// Data behind an [`Arc`] to share between the [`ThreadPool`] and [`WorkerThread`] instances.
#[derive(Default)]
struct ThreadPoolShared {
    /// The mutex to access shared state between [`ThreadPool`] and [`WorkerThread`] instances. This
    /// is accessed both standalone and via [`cond_var`](Self::cond_var).
    mutex: Mutex<ThreadPoolProtected>,
    /// The condition variable used to wake up waiting threads. This is tied to [`mutex`](Self::mutex).
    cond_var: std::sync::Condvar,
}

pub struct ThreadPool {
    /// The data which needs to be shared with worker threads.
    shared: Arc<ThreadPoolShared>,
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
    pub fn new(soft_min_threads: usize, max_threads: usize) -> Self {
        ThreadPool {
            shared: Default::default(),
            soft_min_threads,
            max_threads,
        }
    }

    /// Enqueue a new work item onto the thread pool.
    ///
    /// The function `func` will execute on one of the pool's background threads. If `cant_wait` is
    /// set, the thread limit may be disregarded if extant threads are busy.
    ///
    /// Returns the number of threads that were alive when the work item was enqueued.
    pub fn perform<F: FnOnce() + 'static + Send>(&mut self, func: F, cant_wait: bool) -> usize {
        let work_item = Box::new(func);
        self.perform_inner(work_item, cant_wait)
    }

    fn perform_inner(&mut self, f: WorkItem, cant_wait: bool) -> usize {
        enum ThreadAction {
            None,
            Wake,
            Spawn,
        }

        let local_thread_count;
        let thread_action = {
            let mut data = self.shared.mutex.lock().expect("Mutex poisoned!");
            local_thread_count = data.total_threads;
            data.request_queue.push_back(f);
            FLOG!(
                iothread,
                "enqueuing work item (count is ",
                data.request_queue.len(),
                ")"
            );
            if data.waiting_threads >= data.request_queue.len() {
                // There are enough waiting threads, wake one up.
                ThreadAction::Wake
            } else if cant_wait || data.total_threads < self.max_threads {
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
                self.shared.cond_var.notify_one();
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
                    self.shared
                        .mutex
                        .lock()
                        .expect("Mutex poisoned!")
                        .total_threads -= 1;
                }
            }
        }

        local_thread_count
    }

    /// Attempt to spawn a new worker thread.
    fn spawn_thread(&mut self) -> bool {
        let shared = Arc::clone(&self.shared);
        let soft_min_threads = self.soft_min_threads;
        self::spawn(move || {
            let worker = WorkerThread {
                shared,
                soft_min_threads,
            };

            worker.run();
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

pub struct WorkerThread {
    /// The data shared with the [`ThreadPool`].
    shared: Arc<ThreadPoolShared>,
    /// The soft min number of threads for the associated [`ThreadPool`].
    soft_min_threads: usize,
}

impl WorkerThread {
    /// The worker loop entry point for this thread.
    fn run(mut self) {
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
    fn dequeue_work_or_commit_to_exit(&mut self) -> Option<WorkItem> {
        let mut data = self.shared.mutex.lock().expect("Mutex poisoned!");

        // If the queue is empty, check to see if we should wait. We should wait if our exiting
        // would drop us below our soft thread count minimum.
        if data.request_queue.is_empty()
            && data.total_threads == self.soft_min_threads
            && IO_WAIT_FOR_WORK_DURATION > Duration::ZERO
        {
            data.waiting_threads += 1;
            data = self
                .shared
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

        return result;
    }
}

/// Returns a [`MutexGuard`](std::sync::MutexGuard) containing the IO [`ThreadPool`].
fn borrow_io_thread_pool() -> std::sync::MutexGuard<'static, ThreadPool> {
    IO_THREAD_POOL
        .get()
        .unwrap()
        .lock()
        .expect("Mutex poisoned!")
}

/// Enqueues work on the IO thread pool singleton.
pub fn iothread_perform(f: impl FnOnce() + 'static + Send) {
    let mut thread_pool = borrow_io_thread_pool();
    thread_pool.perform(f, false);
}

/// Enqueues priority work on the IO thread pool singleton, disregarding the thread limit.
///
/// It does its best to spawn a thread if all other threads are occupied. This is primarily for
/// cases where deferring creation of a new thread might lead to a deadlock.
pub fn iothread_perform_cant_wait(f: impl FnOnce() + 'static + Send) {
    let mut thread_pool = borrow_io_thread_pool();
    thread_pool.perform(f, true);
}

pub fn iothread_port() -> i32 {
    NOTIFY_SIGNALLER.read_fd()
}

pub fn iothread_service_main_with_timeout(ctx: &mut Reader, timeout: Duration) {
    use crate::fd_readable_set::Timeout;

    if crate::fd_readable_set::is_fd_readable(iothread_port(), Timeout::Duration(timeout)) {
        iothread_service_main(ctx);
    }
}

pub fn iothread_service_main(ctx: &mut Reader) {
    self::assert_is_main_thread();

    // Note: the order here is important. We must consume events before handling requests, as
    // posting uses the opposite order.
    NOTIFY_SIGNALLER.try_consume();

    let queue = std::mem::take(&mut *MAIN_THREAD_QUEUE.lock().expect("Mutex poisoned!"));

    // Perform each completion in order.
    for callback in queue {
        (callback.0)(ctx);
    }
}

/// Does nasty polling via select(), only used for testing.
#[cfg(test)]
pub(crate) fn iothread_drain_all(ctx: &mut Reader) {
    while borrow_io_thread_pool()
        .shared
        .mutex
        .lock()
        .expect("Mutex poisoned!")
        .total_threads
        > 0
    {
        iothread_service_main_with_timeout(ctx, Duration::from_millis(1000));
    }
}

/// `Debounce` is a simple class which executes one function on a background thread while enqueuing
/// at most one more. Subsequent execution requests overwrite the enqueued one. It takes an optional
/// timeout; if a handler does not finish within the timeout then a new thread is spawned to service
/// the remaining request.
///
/// Debounce implementation note: we would like to enqueue at most one request, except if a thread
/// hangs (e.g. on fs access) then we do not want to block indefinitely - such threads are called
/// "abandoned". This is implemented via a monotone uint64 counter, called a token. Every time we
/// spawn a thread, we increment the token. When the thread has completed running a work item, it
/// compares its token to the active token; if they differ then this thread was abandoned.
#[derive(Clone)]
pub struct Debounce {
    timeout: Duration,
    /// The data shared between [`Debounce`] instances.
    data: Arc<Mutex<DebounceData>>,
}

/// The data shared between [`Debounce`] instances.
struct DebounceData {
    /// The (one or none) next enqueued request, overwritten each time a new call to
    /// [`Debounce::perform()`] is made.
    next_req: Option<WorkItem>,
    /// The non-zero token of the current non-abandoned thread or `None` if no thread is running.
    active_token: Option<NonZeroU64>,
    /// The next token to use when spawning a thread.
    next_token: NonZeroU64,
    /// The start time of the most recently spawned thread or request (if any).
    start_time: Instant,
}

impl Debounce {
    pub fn new(timeout: Duration) -> Self {
        Self {
            timeout,
            data: Arc::new(Mutex::new(DebounceData {
                next_req: None,
                active_token: None,
                next_token: NonZeroU64::new(1).unwrap(),
                start_time: Instant::now(),
            })),
        }
    }

    /// Run an iteration in the background with the given thread token. Returns `true` if we handled
    /// a request or `false` if there were no requests to handle (in which case the debounce thread
    /// exits).
    ///
    /// Note that this method is called from a background thread.
    fn run_next(&self, token: NonZeroU64) -> bool {
        let request = {
            let mut data = self.data.lock().expect("Mutex poisoned!");
            if let Some(req) = data.next_req.take() {
                data.start_time = Instant::now();
                req
            } else {
                // There is no pending request. Mark this token as no longer running.
                if Some(token) == data.active_token {
                    data.active_token = None;
                }
                return false;
            }
        };

        // Execute request after unlocking the mutex.
        (request)();
        return true;
    }

    /// Enqueue `handler` to be performed on a background thread. If another function is already
    /// enqueued, this overwrites it and that function will not be executed.
    ///
    /// The result is a token which is only of interest to the test suite.
    pub fn perform(&self, handler: impl FnOnce() + 'static + Send) -> NonZeroU64 {
        self.perform_with_completion(handler, |_ctx, _result| ())
    }

    /// Enqueue `handler` to be performed on a background thread with `completion` to be performed
    /// on the main thread. If a function is already enqueued, this overwrites it and that function
    /// will not be executed.
    ///
    /// If the function executes within the optional timeout then `completion` will be invoked on
    /// the main thread with the result of the evaluated `handler`.
    ///
    /// The result is a token which is only of interest to the test suite.
    pub fn perform_with_completion<H, R, C>(&self, handler: H, completion: C) -> NonZeroU64
    where
        H: FnOnce() -> R + 'static + Send,
        C: FnOnce(&mut Reader, R) + 'static,
        R: 'static + Send,
    {
        assert_is_main_thread();
        let completion_wrapper = ForceSend(completion);
        let work_item = Box::new(move || {
            let result = handler();
            let callback: DebounceCallback = ForceSend(Box::new(move |ctx| {
                let completion = completion_wrapper;
                (completion.0)(ctx, result);
            }));
            MAIN_THREAD_QUEUE.lock().unwrap().push(callback);
            NOTIFY_SIGNALLER.post();
        });
        self.perform_inner(work_item)
    }

    fn perform_inner(&self, work_item: WorkItem) -> NonZeroU64 {
        let mut spawn = false;
        let active_token = {
            let mut data = self.data.lock().expect("Mutex poisoned!");
            data.next_req = Some(work_item);
            // If we have a timeout and our running thread has exceeded it, abandon that thread.
            if data.active_token.is_some()
                && !self.timeout.is_zero()
                && (Instant::now() - data.start_time > self.timeout)
            {
                // Abandon this thread by dissociating its token from this [`Debounce`] instance.
                data.active_token = None;
            }
            if data.active_token.is_none() {
                // We need to spawn a new thread. Mark the current time so that a new request won't
                // immediately abandon us and start a new thread too.
                spawn = true;
                data.active_token = Some(data.next_token);
                data.next_token = data.next_token.checked_add(1).unwrap();
                data.start_time = Instant::now();
            }
            data.active_token.expect("Something should be active now.")
        };

        // Spawn after unlocking the mutex above.
        if spawn {
            // We need to clone the Arc to get it to last for the duration of the 'static lifetime.
            let debounce = self.clone();
            iothread_perform(move || {
                while debounce.run_next(active_token) {
                    // Keep thread alive/busy.
                }
            });
        }

        active_token
    }
}

#[cfg(test)]
mod tests {
    use super::{Debounce, iothread_drain_all, iothread_service_main, spawn, thread_id};
    use crate::global_safety::RelaxedAtomicBool;
    use crate::reader::{Reader, fake_scoped_reader};
    use crate::tests::prelude::*;
    use std::mem::MaybeUninit;
    use std::sync::{
        Arc, Condvar, Mutex,
        atomic::{AtomicI32, AtomicU32, Ordering},
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

    #[test]
    #[serial]
    fn test_debounce() {
        let _cleanup = test_init();
        let parser = TestParser::new();
        // Run 8 functions using a condition variable.
        // Only the first and last should run.
        let db = Debounce::new(Duration::from_secs(0));
        const count: usize = 8;

        struct Context {
            handler_ran: [RelaxedAtomicBool; count],
            completion_ran: [RelaxedAtomicBool; count],
            ready_to_go: Mutex<bool>,
            cv: Condvar,
        }

        let ctx = Arc::new(Context {
            handler_ran: std::array::from_fn(|_i| RelaxedAtomicBool::new(false)),
            completion_ran: std::array::from_fn(|_i| RelaxedAtomicBool::new(false)),
            ready_to_go: Mutex::new(false),
            cv: Condvar::new(),
        });

        // "Enqueue" all functions. Each one waits until ready_to_go.
        for idx in 0..count {
            assert!(!ctx.handler_ran[idx].load());
            let performer = {
                let ctx = ctx.clone();
                move || {
                    let guard = ctx.ready_to_go.lock().unwrap();
                    let _guard = ctx.cv.wait_while(guard, |ready| !*ready).unwrap();
                    ctx.handler_ran[idx].store(true);
                    idx
                }
            };
            let completer = {
                let ctx = ctx.clone();
                move |_ctx: &mut Reader, idx: usize| {
                    ctx.completion_ran[idx].store(true);
                }
            };
            db.perform_with_completion(performer, completer);
        }

        // We're ready to go.
        *ctx.ready_to_go.lock().unwrap() = true;
        ctx.cv.notify_all();

        // Wait until the last completion is done.
        let mut reader = fake_scoped_reader(&parser);
        while !ctx.completion_ran.last().unwrap().load() {
            iothread_service_main(&mut reader);
        }
        iothread_drain_all(&mut reader);

        // Each perform() call may displace an existing queued operation.
        // Each operation waits until all are queued.
        // Therefore we expect the last perform() to have run, and at most one more.
        assert!(ctx.handler_ran.last().unwrap().load());
        assert!(ctx.completion_ran.last().unwrap().load());

        let mut total_ran = 0;
        for idx in 0..count {
            if ctx.handler_ran[idx].load() {
                total_ran += 1;
            }
            assert_eq!(ctx.handler_ran[idx].load(), ctx.completion_ran[idx].load());
        }
        assert!(total_ran <= 2);
    }

    #[test]
    #[serial]
    fn test_debounce_timeout() {
        let _cleanup = test_init();
        // Verify that debounce doesn't wait forever.
        // Use a shared_ptr so we don't have to join our threads.
        let timeout = Duration::from_millis(500);

        struct Data {
            db: Debounce,
            exit_ok: Mutex<bool>,
            cv: Condvar,
            running: AtomicU32,
        }

        let data = Arc::new(Data {
            db: Debounce::new(timeout),
            exit_ok: Mutex::new(false),
            cv: Condvar::new(),
            running: AtomicU32::new(0),
        });

        // Our background handler. Note this just blocks until exit_ok is set.
        let handler = {
            let data = data.clone();
            move || {
                data.running.fetch_add(1, Ordering::Relaxed);
                let guard = data.exit_ok.lock().unwrap();
                let _guard = data.cv.wait_while(guard, |exit_ok| !*exit_ok);
            }
        };

        // Spawn the handler twice. This should not modify the thread token.
        let token1 = data.db.perform(handler.clone());
        let token2 = data.db.perform(handler.clone());
        assert_eq!(token1, token2);

        // Wait 75 msec, then enqueue something else; this should spawn a new thread.
        std::thread::sleep(timeout + timeout / 2);
        assert!(data.running.load(Ordering::Relaxed) == 1);
        let token3 = data.db.perform(handler);
        assert!(token3 > token2);

        // Release all the threads.
        let mut exit_ok = data.exit_ok.lock().unwrap();
        *exit_ok = true;
        data.cv.notify_all();
    }
}
