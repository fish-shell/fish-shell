use super::ThreadPool;
use crate::fd_monitor::FdEventSignaller;
use crate::fd_readable_set;
/// Support for debounced background execution of functions.
use std::num::NonZeroU64;
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

/// A debounced function to execute.
pub(super) type WorkItem = Box<dyn FnOnce() + 'static + Send>;

/// `Debounce` is a simple struct which executes one function on a background thread while enqueuing
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
pub struct Debounce<R> {
    /// The thread pool to use for background execution.
    pool: Arc<ThreadPool>,
    /// The timeout after which a running thread is considered abandoned.
    timeout: Duration,
    /// The data shared between a [`Debounce`] instance and its thread.
    data: Arc<Mutex<DebounceData<R>>>,
    /// An event signaller used to indicate that a result has arrived.
    /// Note the usage order here matters:
    ///   1. To enqueue a result, first store it, then post to the event signaller.
    ///   2. To service a result, first consume the event signaller, then process the result.
    event_signaller: Arc<FdEventSignaller>,
}

/// The data shared between a [`Debounce`] and its thread.
struct DebounceData<R> {
    /// The (one or none) next enqueued request, overwritten each time a new call to
    /// [`Debounce::perform()`] is made.
    next_req: Option<WorkItem>,
    /// The non-zero token of the current non-abandoned thread or `None` if no thread is running.
    active_token: Option<NonZeroU64>,
    /// The next token to use when spawning a thread.
    next_token: NonZeroU64,
    /// The start time of the most recently spawned thread or request (if any).
    start_time: Instant,
    /// The most recent result, at most one. This is overwritten by the most recent completing request.
    result: Option<R>,
}

impl<R: Send + 'static> Debounce<R> {
    pub fn new(
        pool: &Arc<ThreadPool>,
        event_signaller: &Arc<FdEventSignaller>,
        timeout: Duration,
    ) -> Self {
        Self {
            pool: Arc::clone(pool),
            timeout,
            event_signaller: Arc::clone(event_signaller),
            data: Arc::new(Mutex::new(DebounceData {
                next_req: None,
                active_token: None,
                next_token: NonZeroU64::new(1).unwrap(),
                start_time: Instant::now(),
                result: None,
            })),
        }
    }

    /// Run an iteration in the background with the given thread token. Returns `true` if we handled
    /// a request or `false` if there were no requests to handle (in which case the debounce thread
    /// exits).
    ///
    /// Note that this method is called from a background thread.
    fn run_next(data: &Mutex<DebounceData<R>>, token: NonZeroU64) -> bool {
        let request = {
            let mut data = data.lock().expect("Mutex poisoned!");
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
    pub fn perform_void(&self, handler: impl FnOnce() + 'static + Send) -> NonZeroU64 {
        self.perform_inner(Box::new(handler))
    }

    /// Enqueue `handler` to be performed on a background thread with a function to
    /// If a function is already enqueued, this overwrites it and that function
    /// will not be executed.
    ///
    /// The result is a token which is only of interest to the test suite.
    pub fn perform<Handler>(&self, handler: Handler) -> NonZeroU64
    where
        Handler: FnOnce() -> R + 'static + Send,
    {
        let data = Arc::clone(&self.data);
        let event_signaller = Arc::clone(&self.event_signaller);
        let work_item = Box::new(move || {
            let result = handler();
            // Store the result and signal its availability.
            data.lock().unwrap().result = Some(result);
            event_signaller.post();
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
            let data = Arc::clone(&self.data);
            self.pool.perform(move || {
                while Self::run_next(&data, active_token) {
                    // Keep thread alive/busy.
                }
            });
        }

        active_token
    }

    /// Take the result if available.
    pub fn take_result(&mut self) -> Option<R> {
        self.data.lock().unwrap().result.take()
    }

    /// Take the result, waiting up to `timeout` for a result to be available.
    pub fn take_result_with_timeout(&mut self, timeout: Duration) -> Option<R> {
        let timeout = fd_readable_set::Timeout::Duration(timeout);
        if fd_readable_set::is_fd_readable(self.event_signaller.read_fd(), timeout) {
            self.take_result()
        } else {
            None
        }
    }
}
