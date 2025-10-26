use super::{ThreadPool, assert_is_main_thread};
use crate::reader::Reader;
/// Support for debounced background execution of functions.
use std::num::NonZeroU64;
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

/// A debounced function to execute.
pub(super) type WorkItem = Box<dyn FnOnce() + 'static + Send>;

// A helper type to allow us to (temporarily) send an object to another thread.
pub(super) struct ForceSend<T>(pub T);

// Safety: only used on main thread.
unsafe impl<T> Send for ForceSend<T> {}

/// The callback to be invoked on the main thread after a debounced function completes.
#[allow(clippy::type_complexity)]
pub(super) type DebounceCallback = ForceSend<Box<dyn FnOnce(&mut Reader) + 'static>>;

impl ThreadPool {
    /// Return a Debounce instance using this thread pool.
    pub fn debouncer(self: &Arc<Self>, timeout: Duration) -> Debounce {
        Debounce::new(Arc::clone(self), timeout)
    }
}

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
pub struct Debounce {
    /// The thread pool to use for background execution.
    pool: Arc<ThreadPool>,
    /// The timeout after which a running thread is considered abandoned.
    timeout: Duration,
    /// The data shared between [`Debounce`] instances.
    data: Arc<Mutex<DebounceData>>,
}

/// The data shared between a [`Debounce`] and its thread.
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
    pub fn new(pool: Arc<ThreadPool>, timeout: Duration) -> Self {
        Self {
            pool,
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
        let pool = Arc::clone(&self.pool);
        let work_item = Box::new(move || {
            let result = handler();
            let callback: DebounceCallback = ForceSend(Box::new(move |ctx| {
                let completion = completion_wrapper;
                (completion.0)(ctx, result);
            }));
            pool.completion_queue.lock().unwrap().push(callback);
            pool.event_signaller.post();
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
            self.pool.perform(move || {
                while debounce.run_next(active_token) {
                    // Keep thread alive/busy.
                }
            });
        }

        active_token
    }
}
