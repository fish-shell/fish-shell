//! A thread pool for handling operations related to the interactive reader
//! which might block, such as file I/O or completions.

use super::{Reader, reader};
use crate::fd_monitor::FdEventSignaller;
use crate::threads::ThreadPool;
use crate::threads::debounce::Debounce;
use std::os::unix::io::RawFd;
use std::sync::Arc;
use std::time::Duration;

/// The maximum number of I/O threads in a pool used to service background reader operations.
const IO_MAX_THREADS: usize = 16;

/// A reader callback after some reader I/O operation completes.
pub(super) type Callback = Box<dyn for<'a> FnOnce(&mut Reader<'a>) + Send>;

/// Separate debouncers for various reader operations.
pub(super) struct Debouncers {
    // The event signaller shared by all debouncers.
    pub event_signaller: Arc<FdEventSignaller>,
    // Debounce autosuggestion computations.
    pub autosuggestions: Debounce<reader::AutosuggestionResult>,
    // Debounce syntax highlighting.
    pub highlight: Debounce<reader::HighlightResult>,
    // Debounce history pager computations. This holds a callback, not a single value,
    // both to demonstrate the technique and because the callback can capture local variables.
    pub history_pager: Debounce<Callback>,
}

impl Debouncers {
    pub fn new() -> Self {
        let pool = ThreadPool::new(1, IO_MAX_THREADS);
        // These timeouts control how long until a thread is considered abandoned and
        // any queued work is assigned to a new thread.
        let event_signaller = Arc::new(FdEventSignaller::new());
        const HIGHLIGHT_TIMEOUT: Duration = Duration::from_millis(500);
        const HISTORY_PAGER_TIMEOUT: Duration = Duration::from_millis(500);
        const AUTOSUGGEST_TIMEOUT: Duration = Duration::from_millis(500);
        Self {
            autosuggestions: Debounce::new(&pool, &event_signaller, AUTOSUGGEST_TIMEOUT),
            highlight: Debounce::new(&pool, &event_signaller, HIGHLIGHT_TIMEOUT),
            history_pager: Debounce::new(&pool, &event_signaller, HISTORY_PAGER_TIMEOUT),
            event_signaller,
        }
    }

    /// Return the read fd of the event signaller.
    /// This may be used with `poll()` or `select()` to multiplex iothread completions with other events.
    pub fn event_signaller_read_fd(&self) -> RawFd {
        self.event_signaller.read_fd()
    }
}
