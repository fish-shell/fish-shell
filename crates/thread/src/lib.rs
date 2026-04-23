use core::{cell::OnceCell, marker::PhantomData};
use std::{
    cell::LazyCell,
    sync::atomic::{AtomicUsize, Ordering},
};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ThreadId(usize);

/// Get the calling thread's fish-specific thread id.
///
/// This thread id is internal to the `threads` module for low-level purposes and should not be
/// leaked to other modules; general purpose code that needs a thread id should use rust's native
/// thread id functionality.
///
/// We use our own implementation because Rust's own `Thread::id()` allocates via `Arc`, is fairly
/// slow, and uses a `Mutex` on 32-bit platforms (or anywhere without an atomic 64-bit CAS).
#[inline(always)]
pub fn thread_id() -> ThreadId {
    static THREAD_COUNTER: AtomicUsize = AtomicUsize::new(1);
    // It would be faster and much nicer to use #[thread_local] here, but that's nightly only.
    // This is still faster than going through Thread::thread_id(); it's something like 15ns
    // for each `Thread::thread_id()` call vs 1-2 ns with `#[thread_local]` and 2-4ns with
    // `thread_local!`.
    thread_local! {
        static THREAD_ID: ThreadId = ThreadId(THREAD_COUNTER.fetch_add(1, Ordering::Relaxed));
    }
    let id = THREAD_ID.with(|id| *id);
    // This assertion is only here to reduce hair loss in case someone runs into a known linker bug;
    // as it's not here to catch logic errors in our own code, it can be elided in release mode.
    debug_assert_ne!(id, ThreadId(0), "TLS storage not initialized!");
    id
}

/// A `Sync` and `Send` wrapper for non-`Sync`/`Send` types.
/// Only allows access from one thread.
pub struct SingleThreaded<T> {
    thread_id: OnceCell<ThreadId>,
    data: T,
    // Make type !Send and !Sync by default
    _marker: PhantomData<*const ()>,
}

// Can be shared across threads as long as T is 'static.
unsafe impl<T: 'static> Send for SingleThreaded<T> {}
unsafe impl<T: 'static> Sync for SingleThreaded<T> {}

impl<T> SingleThreaded<T> {
    pub const fn new(value: T) -> Self {
        Self {
            thread_id: OnceCell::new(),
            data: value,
            _marker: PhantomData,
        }
    }

    pub fn get(&self) -> &T {
        assert!(thread_id() == *self.thread_id.get_or_init(thread_id));
        &self.data
    }
}

impl<T> std::ops::Deref for SingleThreaded<T> {
    type Target = T;
    fn deref(&self) -> &Self::Target {
        self.get()
    }
}

pub struct SingleThreadedLazyCell<T, F = fn() -> T>(SingleThreaded<LazyCell<T, F>>);

impl<T, F: FnOnce() -> T> SingleThreadedLazyCell<T, F> {
    pub const fn new(f: F) -> Self {
        Self(SingleThreaded::new(LazyCell::new(f)))
    }
}

impl<T, F: FnOnce() -> T> std::ops::Deref for SingleThreadedLazyCell<T, F> {
    type Target = T;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

pub struct SingleThreadedOnceCell<T>(SingleThreaded<OnceCell<T>>);

impl<T> SingleThreadedOnceCell<T> {
    pub const fn new() -> Self {
        Self(SingleThreaded::new(OnceCell::new()))
    }
}

impl<T> std::ops::Deref for SingleThreadedOnceCell<T> {
    type Target = OnceCell<T>;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}
