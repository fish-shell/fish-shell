//! A thread pool for handling operations related to the interactive reader
//! which might block, such as file I/O or completions.

use crate::threads::ThreadPool;
use std::sync::{Arc, OnceLock};

/// The maximum number of I/O threads in this pool.
const IO_MAX_THREADS: usize = 16;

/// The iothreads [`ThreadPool`] singleton. Used to lift I/O off of the main thread and used for
/// completions, etc.
static THREAD_POOL: OnceLock<Arc<ThreadPool>> = OnceLock::new();

/// Returns a reference to the singleton thread pool.
#[inline]
pub fn thread_pool() -> &'static Arc<ThreadPool> {
    // Soft max of 1 so we keep a single thread hanging around for a time to
    // amortize thread creation costs as the user ctypes, etc.
    THREAD_POOL.get_or_init(|| ThreadPool::new(1, IO_MAX_THREADS))
}

/// Return the read fd of the singleton ThreadPool event signaller.
/// This may be used with `poll()` or `select()` to multiplex iothread completions with other events.
pub fn completion_port() -> i32 {
    thread_pool().event_signaller_read_port()
}

/// Invoke completions for the given reader.
pub fn invoke_completions(reader: &mut crate::reader::Reader) {
    thread_pool().invoke_completions(reader);
}
