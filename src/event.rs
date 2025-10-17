//! Functions for handling event triggers
//!
//! Because most of these functions can be called by signal handler, it is important to make it well
//! defined when these functions produce output or perform memory allocations, since such functions
//! may not be safely called by signal handlers.

use std::sync::atomic::{AtomicBool, AtomicU32, Ordering};
use std::sync::{Arc, Mutex};

use crate::common::{ScopeGuard, escape};
use crate::flog::FLOG;
use crate::io::{IoChain, IoStreams};
use crate::job_group::MaybeJobId;
use crate::parser::{Block, Parser};
use crate::proc::Pid;
use crate::signal::{Signal, signal_check_cancel, signal_handle};
use crate::termsize;
use crate::wchar::prelude::*;

pub enum event_type_t {
    any,
    signal,
    variable,
    process_exit,
    job_exit,
    caller_exit,
    generic,
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub enum EventDescription {
    /// Matches any event type (not always any event, as the function name may limit the choice as
    /// well).
    Any,
    /// An event triggered by a signal.
    Signal { signal: Signal },
    /// An event triggered by a variable update.
    Variable { name: WString },
    /// An event triggered by a process exit.
    ProcessExit {
        /// Process ID. Use [`None`] to match any pid.
        pid: Option<Pid>,
    },
    /// An event triggered by a job exit.
    JobExit {
        /// pid requested by the event, or [`None`] for all.
        pid: Option<Pid>,
        /// `internal_job_id` of the job to match.
        /// If this is 0, we match either all jobs (`pid == ANY_PID`) or no jobs (otherwise).
        internal_job_id: u64,
    },
    /// An event triggered by a job exit, triggering the 'caller'-style events only.
    CallerExit {
        /// Internal job ID.
        caller_id: u64,
    },
    /// A generic event.
    Generic {
        /// The parameter describing this generic event.
        param: WString,
    },
}

impl EventDescription {
    fn str_param1(&self) -> Option<&wstr> {
        match self {
            EventDescription::Any
            | EventDescription::Signal { .. }
            | EventDescription::ProcessExit { .. }
            | EventDescription::JobExit { .. }
            | EventDescription::CallerExit { .. } => None,
            EventDescription::Variable { name } => Some(name),
            EventDescription::Generic { param } => Some(param),
        }
    }

    fn name(&self) -> &'static wstr {
        match self {
            EventDescription::Any => L!("any"),
            EventDescription::Signal { .. } => L!("signal"),
            EventDescription::Variable { .. } => L!("variable"),
            EventDescription::ProcessExit { .. } => L!("process-exit"),
            EventDescription::JobExit { .. } => L!("job-exit"),
            EventDescription::CallerExit { .. } => L!("caller-exit"),
            EventDescription::Generic { .. } => L!("generic"),
        }
    }

    fn matches_filter(&self, filter: &wstr) -> bool {
        if filter.is_empty() {
            return true;
        }

        match self {
            EventDescription::Any => false,
            EventDescription::ProcessExit { .. }
            | EventDescription::JobExit { .. }
            | EventDescription::CallerExit { .. }
                if filter == "exit" =>
            {
                true
            }
            _ => filter == self.name(),
        }
    }
}

impl From<&EventDescription> for event_type_t {
    fn from(desc: &EventDescription) -> Self {
        match desc {
            EventDescription::Any => event_type_t::any,
            EventDescription::Signal { .. } => event_type_t::signal,
            EventDescription::Variable { .. } => event_type_t::variable,
            EventDescription::ProcessExit { .. } => event_type_t::process_exit,
            EventDescription::JobExit { .. } => event_type_t::job_exit,
            EventDescription::CallerExit { .. } => event_type_t::caller_exit,
            EventDescription::Generic { .. } => event_type_t::generic,
        }
    }
}

#[derive(Debug)]
pub struct EventHandler {
    /// Properties of the event to match.
    pub desc: EventDescription,
    /// Name of the function to invoke.
    pub function_name: WString,
    /// A flag set when an event handler is removed from the global list.
    /// Once set, this is never cleared.
    pub removed: AtomicBool,
    /// A flag set when an event handler is first fired.
    pub fired: AtomicBool,
}

impl EventHandler {
    pub fn new(desc: EventDescription, name: Option<WString>) -> Self {
        Self {
            desc,
            function_name: name.unwrap_or_default(),
            removed: AtomicBool::new(false),
            fired: AtomicBool::new(false),
        }
    }

    /// Return true if a handler is "one shot": it fires at most once.
    fn is_one_shot(&self) -> bool {
        match self.desc {
            EventDescription::ProcessExit { pid } => pid.is_some(),
            EventDescription::JobExit { pid, .. } => pid.is_some(),
            EventDescription::CallerExit { .. } => true,
            EventDescription::Signal { .. }
            | EventDescription::Variable { .. }
            | EventDescription::Generic { .. }
            | EventDescription::Any => false,
        }
    }

    /// Tests if this event handler matches an event that has occurred.
    fn matches(&self, event: &Event) -> bool {
        match (&self.desc, &event.desc) {
            (EventDescription::Any, _) => true,
            (
                EventDescription::Signal { signal },
                EventDescription::Signal { signal: ev_signal },
            ) => signal == ev_signal,
            (EventDescription::Variable { name }, EventDescription::Variable { name: ev_name }) => {
                name == ev_name
            }
            (
                EventDescription::ProcessExit { pid },
                EventDescription::ProcessExit { pid: ev_pid },
            ) => pid.is_none() || pid == ev_pid,
            (
                EventDescription::JobExit {
                    pid,
                    internal_job_id,
                },
                EventDescription::JobExit {
                    internal_job_id: ev_internal_job_id,
                    ..
                },
            ) => pid.is_none() || internal_job_id == ev_internal_job_id,
            (
                EventDescription::CallerExit { caller_id },
                EventDescription::CallerExit {
                    caller_id: ev_caller_id,
                },
            ) => caller_id == ev_caller_id,
            (
                EventDescription::Generic { param },
                EventDescription::Generic { param: ev_param },
            ) => param == ev_param,
            (_, _) => false,
        }
    }
}
type EventHandlerList = Vec<Arc<EventHandler>>;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Event {
    desc: EventDescription,
    arguments: Vec<WString>,
}

impl Event {
    pub fn generic(desc: WString) -> Self {
        Self {
            desc: EventDescription::Generic { param: desc },
            arguments: vec![],
        }
    }

    pub fn variable_erase(name: WString) -> Self {
        Self {
            desc: EventDescription::Variable { name: name.clone() },
            arguments: vec!["VARIABLE".into(), "ERASE".into(), name],
        }
    }

    pub fn variable_set(name: WString) -> Self {
        Self {
            desc: EventDescription::Variable { name: name.clone() },
            arguments: vec!["VARIABLE".into(), "SET".into(), name],
        }
    }

    pub fn process_exit(pid: Pid, status: i32) -> Self {
        Self {
            desc: EventDescription::ProcessExit { pid: Some(pid) },
            arguments: vec![
                "PROCESS_EXIT".into(),
                pid.to_string().into(),
                status.to_string().into(),
            ],
        }
    }

    pub fn job_exit(pgid: Pid, jid: u64) -> Self {
        Self {
            desc: EventDescription::JobExit {
                pid: Some(pgid),
                internal_job_id: jid,
            },
            arguments: vec![
                "JOB_EXIT".into(),
                pgid.to_string().into(),
                "0".into(), // historical
            ],
        }
    }

    pub fn caller_exit(internal_job_id: u64, job_id: MaybeJobId) -> Self {
        Self {
            desc: EventDescription::CallerExit {
                caller_id: internal_job_id,
            },
            arguments: vec![
                "JOB_EXIT".into(),
                job_id.to_wstring(),
                "0".into(), // historical
            ],
        }
    }

    /// Test if specified event is blocked.
    fn is_blocked(&self, parser: &Parser) -> bool {
        for block in parser.blocks_iter_rev() {
            if block.event_blocks {
                return true;
            }
        }

        parser.global_event_blocks.load(Ordering::Relaxed) != 0
    }
}

/// All the signals we are interested in are in the 1-32 range (with 32 being the typical SIGRTMAX),
/// but we can expand it to 64 just to be safe. All code checks if a signal value is within bounds
/// before handling it.
const SIGNAL_COUNT: usize = 64;

struct PendingSignals {
    /// A counter that is incremented each time a pending signal is received.
    counter: AtomicU32,
    /// List of pending signals.
    received: [AtomicBool; SIGNAL_COUNT],
    /// The last counter visible in `acquire_pending()`.
    /// This is not accessed from a signal handler.
    last_counter: Mutex<u32>,
}

impl PendingSignals {
    /// Mark a signal as pending. This may be called from a signal handler. We expect only one
    /// signal handler to execute at once. Also note that these may be coalesced.
    pub fn mark(&self, sig: libc::c_int) {
        if let Some(received) = self.received.get(usize::try_from(sig).unwrap()) {
            received.store(true, Ordering::Relaxed);
            self.counter.fetch_add(1, Ordering::Relaxed);
        }
    }

    /// Return the list of signals that were set as the bits in a u64, clearing them.
    pub fn acquire_pending(&self) -> u64 {
        let mut current = self
            .last_counter
            .lock()
            .expect("mutex should not be poisoned");

        // Check the counter first. If it hasn't changed, no signals have been received.
        let count = self.counter.load(Ordering::Acquire);
        if count == *current {
            return 0;
        }

        // The signal count has changed. Store the new counter and fetch all set signals.
        *current = count;
        let mut result = 0;
        for (i, received) in self.received.iter().enumerate() {
            if received.load(Ordering::Relaxed) {
                result |= 1_u64 << i;
                received.store(false, Ordering::Relaxed);
            }
        }

        result
    }
}

// Required until inline const is stabilized.
#[allow(clippy::declare_interior_mutable_const)]
const ATOMIC_BOOL_FALSE: AtomicBool = AtomicBool::new(false);
#[allow(clippy::declare_interior_mutable_const)]
const ATOMIC_U32_0: AtomicU32 = AtomicU32::new(0);

static PENDING_SIGNALS: PendingSignals = PendingSignals {
    counter: AtomicU32::new(0),
    received: [ATOMIC_BOOL_FALSE; SIGNAL_COUNT],
    last_counter: Mutex::new(0),
};

/// List of event handlers. **While this is locked to allow safely accessing/modifying the vector,
/// note that it does NOT provide exclusive access to the [`EventHandler`] objects which are shared
/// references (in an `Arc<T>`).**
static EVENT_HANDLERS: Mutex<EventHandlerList> = Mutex::new(Vec::new());

/// Tracks the number of registered event handlers for each signal.
/// This is inspected by a signal handler. We assume no values in here overflow.
static OBSERVED_SIGNALS: [AtomicU32; SIGNAL_COUNT] = [ATOMIC_U32_0; SIGNAL_COUNT];

/// List of events that have been sent but have not yet been delivered because they are blocked.
///
/// This was part of profile_item_t accessed as parser.libdata().blocked_events and has been
/// temporarily moved here. There was no mutex around this in the cpp code. TODO: Move it back.
static BLOCKED_EVENTS: Mutex<Vec<Event>> = Mutex::new(Vec::new());

fn inc_signal_observed(sig: Signal) {
    if let Some(sig) = OBSERVED_SIGNALS.get(usize::from(sig)) {
        sig.fetch_add(1, Ordering::Relaxed);
    }
}

fn dec_signal_observed(sig: Signal) {
    if let Some(sig) = OBSERVED_SIGNALS.get(usize::from(sig)) {
        sig.fetch_sub(1, Ordering::Relaxed);
    }
}

/// Returns whether an event listener is registered for the given signal. This is safe to call from
/// a signal handler.
pub fn is_signal_observed(sig: libc::c_int) -> bool {
    // We are in a signal handler!
    OBSERVED_SIGNALS
        .get(usize::try_from(sig).unwrap())
        .is_some_and(|s| s.load(Ordering::Relaxed) > 0)
}

pub fn get_desc(parser: &Parser, evt: &Event) -> WString {
    let s = match &evt.desc {
        EventDescription::Signal { signal } => {
            format!("signal handler for {} ({})", signal.name(), signal.desc(),)
        }
        EventDescription::Variable { name } => format!("handler for variable '{name}'"),
        EventDescription::ProcessExit { pid: None } => "exit handler for any process".to_string(),
        EventDescription::ProcessExit { pid: Some(pid) } => {
            format!("exit handler for process {pid}")
        }
        EventDescription::JobExit { pid, .. } => {
            if let Some(pid) = pid {
                if let Some(job) = parser.job_get_from_pid(*pid) {
                    format!("exit handler for job {}, '{}'", job.job_id(), job.command())
                } else {
                    format!("exit handler for job with pid {pid}")
                }
            } else {
                "exit handler for any job".to_string()
            }
        }
        EventDescription::CallerExit { .. } => {
            "exit handler for command substitution caller".to_string()
        }
        EventDescription::Generic { param } => format!("handler for generic event '{param}'"),
        EventDescription::Any => unreachable!(),
    };

    WString::from_str(&s)
}

/// Add an event handler.
pub fn add_handler(eh: EventHandler) {
    if let EventDescription::Signal { signal } = eh.desc {
        signal_handle(signal);
        inc_signal_observed(signal);
    }

    EVENT_HANDLERS
        .lock()
        .expect("event handler list should not be poisoned")
        .push(Arc::new(eh));
}

/// Remove handlers where `pred` returns true. Simultaneously update our `signal_observed` array.
fn remove_handlers_if(pred: impl Fn(&EventHandler) -> bool) -> usize {
    let mut handlers = EVENT_HANDLERS
        .lock()
        .expect("event handler list should not be poisoned");

    let mut removed = 0;
    for i in (0..handlers.len()).rev() {
        let handler = &handlers[i];
        if pred(handler) {
            handler.removed.store(true, Ordering::Relaxed);
            if let EventDescription::Signal { signal } = handler.desc {
                dec_signal_observed(signal);
            }
            handlers.remove(i);
            removed += 1;
        }
    }

    removed
}

/// Remove all events for the given function name.
pub fn remove_function_handlers(name: &wstr) -> usize {
    remove_handlers_if(|h| h.function_name == name)
}

/// Return all event handlers for the given function.
pub fn get_function_handlers(name: &wstr) -> EventHandlerList {
    EVENT_HANDLERS
        .lock()
        .expect("event handler list should not be poisoned")
        .iter()
        .filter(|h| h.function_name == name)
        .cloned()
        .collect()
}

/// Perform the specified event. Since almost all event firings will not be matched by even a single
/// event handler, we make sure to optimize the 'no matches' path. This means that nothing is
/// allocated/initialized unless needed.
fn fire_internal(parser: &Parser, event: &Event) {
    // Suppress fish_trace during events.
    let _saved = parser.push_scope(|s| {
        s.is_event = true;
        s.suppress_fish_trace = true;
    });

    // Capture the event handlers that match this event.
    let fire: Vec<_> = EVENT_HANDLERS
        .lock()
        .expect("event handler list should not be poisoned")
        .iter()
        .filter(|h| h.matches(event))
        .cloned()
        .collect();

    // Iterate over our list of matching events. Fire the ones that are still present.
    let mut fired_one_shot = false;
    for handler in fire {
        // A previous handler may have erased this one.
        if handler.removed.load(Ordering::Relaxed) {
            continue;
        };

        // Construct a buffer to evaluate, starting with the function name and then all the
        // arguments.
        let mut buffer = handler.function_name.clone();
        for arg in &event.arguments {
            buffer.push(' ');
            buffer.push_utfstr(&escape(arg));
        }

        // Event handlers are not part of the main flow of code, so they are marked as
        // non-interactive.
        let _non_interactive = parser.push_scope(|s| s.is_interactive = false);
        let saved_statuses = parser.get_last_statuses();
        let _cleanup = ScopeGuard::new((), |()| {
            parser.set_last_statuses(saved_statuses);
        });

        FLOG!(
            event,
            "Firing event '",
            event.desc.str_param1().unwrap_or(L!("")),
            "' to handler '",
            handler.function_name,
            "'"
        );

        let b = parser.push_block(Block::event_block(event.clone()));
        parser.eval(&buffer, &IoChain::new());
        parser.pop_block(b);

        handler.fired.store(true, Ordering::Relaxed);
        fired_one_shot |= handler.is_one_shot();
    }

    if fired_one_shot {
        remove_handlers_if(|h| h.fired.load(Ordering::Relaxed) && h.is_one_shot());
    }
}

/// Fire all delayed events attached to the given parser.
pub fn fire_delayed(parser: &Parser) {
    {
        // Do not invoke new event handlers from within event handlers.
        if parser.scope().is_event {
            return;
        };
    }

    // Do not invoke new event handlers if we are unwinding (#6649).
    if signal_check_cancel() != 0 {
        return;
    };

    // We unfortunately can't keep this locked until we're done with it because the SIGWINCH handler
    // code might call back into here and we would delay processing of the events, leading to a test
    // failure under CI.
    let mut to_send = std::mem::take(&mut *BLOCKED_EVENTS.lock().expect("Mutex poisoned!"));

    // Append all signal events to to_send.
    // 'signals' contains a bit set for each signal that has been received.
    let mut signals: u64 = PENDING_SIGNALS.acquire_pending();
    while signals != 0 {
        let sig = signals.trailing_zeros() as i32;
        signals &= !(1_u64 << sig);
        let sig = Signal::new(sig);

        // HACK: The only variables we change in response to a *signal* are $COLUMNS and $LINES.
        // Do that now.
        if sig == libc::SIGWINCH {
            termsize::SHARED_CONTAINER.updating(parser);
        }
        let event = Event {
            desc: EventDescription::Signal { signal: sig },
            arguments: vec![sig.name().into()],
        };
        to_send.push(event);
    }

    // Fire or re-block all events. Don't obtain BLOCKED_EVENTS until we know that we have at least
    // one event that is blocked.
    let mut blocked_events = None;
    for event in to_send {
        if event.is_blocked(parser) {
            if blocked_events.is_none() {
                blocked_events = Some(BLOCKED_EVENTS.lock().expect("Mutex poisoned"));
            }
            blocked_events.as_mut().unwrap().push(event);
        } else {
            // fire_internal() does not access BLOCKED_EVENTS so this call can't deadlock.
            fire_internal(parser, &event);
        }
    }
}

/// Enqueue a signal event. Invoked from a signal handler.
pub fn enqueue_signal(signal: libc::c_int) {
    // Beware, we are in a signal handler
    PENDING_SIGNALS.mark(signal);
}

/// Fire the specified event event, executing it on `parser`.
pub fn fire(parser: &Parser, event: Event) {
    // Fire events triggered by signals.
    fire_delayed(parser);

    if event.is_blocked(parser) {
        BLOCKED_EVENTS.lock().expect("Mutex poisoned!").push(event);
    } else {
        fire_internal(parser, &event);
    }
}

pub const EVENT_FILTER_NAMES: [&wstr; 7] = [
    L!("signal"),
    L!("variable"),
    L!("exit"),
    L!("process-exit"),
    L!("job-exit"),
    L!("caller-exit"),
    L!("generic"),
];

/// Print all events. If type_filter is not empty, only output events with that type.
pub fn print(streams: &mut IoStreams, type_filter: &wstr) {
    let mut tmp = EVENT_HANDLERS
        .lock()
        .expect("event handler list should not be poisoned")
        .clone();

    tmp.sort_by(|e1, e2| e1.desc.cmp(&e2.desc));

    let mut last_type = std::mem::discriminant(&EventDescription::Any);
    for evt in tmp {
        // If we have a filter, skip events that don't match.
        if !evt.desc.matches_filter(type_filter) {
            continue;
        }

        // Print a "Event $TYPE" header for each event type.
        // This compares only the event *type*, not the entire event,
        // so we don't compare variable events for different variables as different.
        //
        // This assumes EventDescription::Any is not a valid value for an event to have
        // - it's marked "unreachable!()" below!
        if last_type != std::mem::discriminant(&evt.desc) {
            if last_type != std::mem::discriminant(&EventDescription::Any) {
                streams.out.append(L!("\n"));
            }

            last_type = std::mem::discriminant(&evt.desc);
            streams.out.append(sprintf!("Event %s\n", evt.desc.name()));
        }

        match &evt.desc {
            EventDescription::Signal { signal } => {
                let name: WString = signal.name().into();
                streams
                    .out
                    .append(sprintf!("%s %s\n", name, evt.function_name));
            }
            EventDescription::ProcessExit { .. } | EventDescription::JobExit { .. } => {}
            EventDescription::CallerExit { .. } => {
                streams
                    .out
                    .append(sprintf!("caller-exit %s\n", evt.function_name));
            }
            EventDescription::Variable { name: param } | EventDescription::Generic { param } => {
                streams
                    .out
                    .append(sprintf!("%s %s\n", param, evt.function_name));
            }
            EventDescription::Any => unreachable!(),
        }
    }
}

/// Fire a generic event with the specified name.
pub fn fire_generic(parser: &Parser, name: WString, arguments: Vec<WString>) {
    fire(
        parser,
        Event {
            desc: EventDescription::Generic { param: name },
            arguments,
        },
    )
}
