//! Functions for handling event triggers
//!
//! Because most of these functions can be called by signal handler, it is important to make it well
//! defined when these functions produce output or perform memory allocations, since such functions
//! may not be safely called by signal handlers.

use autocxx::WithinUniquePtr;
use cxx::{CxxVector, CxxWString, UniquePtr};
use libc::pid_t;
use std::num::NonZeroU32;
use std::pin::Pin;
use std::sync::atomic::{AtomicBool, AtomicU32, Ordering};
use std::sync::{Arc, Mutex};
use widestring_suffix::widestrs;

use crate::builtins::shared::io_streams_t;
use crate::common::{
    escape, escape_string, scoped_push, EscapeFlags, EscapeStringStyle, ScopeGuard,
};
use crate::ffi::{self, block_t, parser_t, signal_check_cancel, signal_handle, Repin};
use crate::flog::FLOG;
use crate::io::IoChain;
use crate::job_group::{JobId, MaybeJobId};
use crate::parser::{Block, Parser};
use crate::signal::{signal_check_cancel, signal_handle, Signal};
use crate::termsize;
use crate::wchar::{wstr, WString, L};
use crate::wchar_ext::ToWString;
use crate::wchar_ffi::{wcharz_t, AsWstr, WCharFromFFI, WCharToFFI};
use crate::wutil::sprintf;

#[cxx::bridge]
mod event_ffi {
    extern "C++" {
        include!("wutil.h");
        include!("parser.h");
        include!("io.h");
        type wcharz_t = crate::ffi::wcharz_t;
        type parser_t = crate::ffi::parser_t;
        type io_streams_t = crate::ffi::io_streams_t;
    }

    enum event_type_t {
        any,
        signal,
        variable,
        process_exit,
        job_exit,
        caller_exit,
        generic,
    }

    struct event_description_t {
        typ: event_type_t,
        signal: i32,
        pid: i32,
        internal_job_id: u64,
        caller_id: u64,
        str_param1: UniquePtr<CxxWString>,
    }

    extern "Rust" {
        type EventHandler;
        type Event;

        fn new_event_generic(desc: wcharz_t) -> Box<Event>;
        fn new_event_variable_erase(name: &CxxWString) -> Box<Event>;
        fn new_event_variable_set(name: &CxxWString) -> Box<Event>;
        fn new_event_process_exit(pid: i32, status: i32) -> Box<Event>;
        fn new_event_job_exit(pgid: i32, jid: u64) -> Box<Event>;
        fn new_event_caller_exit(internal_job_id: u64, job_id: i32) -> Box<Event>;
        #[cxx_name = "clone"]
        fn clone_ffi(self: &Event) -> Box<Event>;

        #[cxx_name = "event_add_handler"]
        fn event_add_handler_ffi(desc: &event_description_t, name: &CxxWString);
        #[cxx_name = "event_remove_function_handlers"]
        fn event_remove_function_handlers_ffi(name: &CxxWString) -> usize;
        #[cxx_name = "event_get_function_handler_descs"]
        fn event_get_function_handler_descs_ffi(name: &CxxWString) -> Vec<event_description_t>;

        fn desc(self: &EventHandler) -> event_description_t;
        fn function_name(self: &EventHandler) -> UniquePtr<CxxWString>;
        fn set_removed(self: &mut EventHandler);

        fn event_fire_generic_ffi(
            parser: Pin<&mut parser_t>,
            name: &CxxWString,
            arguments: &CxxVector<wcharz_t>,
        );
        #[cxx_name = "event_get_desc"]
        fn event_get_desc_ffi(parser: &parser_t, evt: &Event) -> UniquePtr<CxxWString>;
        #[cxx_name = "event_fire_delayed"]
        fn event_fire_delayed_ffi(parser: Pin<&mut parser_t>);
        #[cxx_name = "event_fire"]
        fn event_fire_ffi(parser: Pin<&mut parser_t>, event: &Event);
        #[cxx_name = "event_print"]
        fn event_print_ffi(streams: Pin<&mut io_streams_t>, type_filter: &CxxWString);

        #[cxx_name = "event_enqueue_signal"]
        fn enqueue_signal(signal: i32);
        #[cxx_name = "event_is_signal_observed"]
        fn is_signal_observed(sig: i32) -> bool;
    }
}

pub use event_ffi::{event_description_t, event_type_t};

const ANY_PID: pid_t = 0;

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub enum EventType {
    /// Matches any event type (not always any event, as the function name may limit the choice as
    /// well).
    Any,
    /// An event triggered by a signal.
    Signal { signal: Signal },
    /// An event triggered by a variable update.
    Variable { name: WString },
    /// An event triggered by a process exit.
    ProcessExit {
        /// Process ID. Use [`ANY_PID`] to match any pid.
        pid: pid_t,
    },
    /// An event triggered by a job exit.
    JobExit {
        /// pid requested by the event, or [`ANY_PID`] for all.
        pid: pid_t,
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

impl EventType {
    fn str_param1(&self) -> Option<&wstr> {
        match self {
            EventType::Any
            | EventType::Signal { .. }
            | EventType::ProcessExit { .. }
            | EventType::JobExit { .. }
            | EventType::CallerExit { .. } => None,
            EventType::Variable { name } => Some(name),
            EventType::Generic { param } => Some(param),
        }
    }

    #[widestrs]
    fn name(&self) -> &'static wstr {
        match self {
            EventType::Any => "any"L,
            EventType::Signal { .. } => "signal"L,
            EventType::Variable { .. } => "variable"L,
            EventType::ProcessExit { .. } => "process-exit"L,
            EventType::JobExit { .. } => "job-exit"L,
            EventType::CallerExit { .. } => "caller-exit"L,
            EventType::Generic { .. } => "generic"L,
        }
    }

    fn matches_filter(&self, filter: &wstr) -> bool {
        if filter.is_empty() {
            return true;
        }

        match self {
            EventType::Any => false,
            EventType::ProcessExit { .. }
            | EventType::JobExit { .. }
            | EventType::CallerExit { .. }
                if filter == L!("exit") =>
            {
                true
            }
            _ => filter == self.name(),
        }
    }
}

impl From<&EventType> for event_type_t {
    fn from(typ: &EventType) -> Self {
        match typ {
            EventType::Any => event_type_t::any,
            EventType::Signal { .. } => event_type_t::signal,
            EventType::Variable { .. } => event_type_t::variable,
            EventType::ProcessExit { .. } => event_type_t::process_exit,
            EventType::JobExit { .. } => event_type_t::job_exit,
            EventType::CallerExit { .. } => event_type_t::caller_exit,
            EventType::Generic { .. } => event_type_t::generic,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct EventDescription {
    // TODO: remove the wrapper struct and just put `EventType` where `EventDescription` is now
    pub typ: EventType,
}

impl From<&event_description_t> for EventDescription {
    fn from(desc: &event_description_t) -> Self {
        EventDescription {
            typ: match desc.typ {
                event_type_t::any => EventType::Any,
                event_type_t::signal => EventType::Signal {
                    signal: Signal::new(desc.signal),
                },
                event_type_t::variable => EventType::Variable {
                    name: desc.str_param1.from_ffi(),
                },
                event_type_t::process_exit => EventType::ProcessExit { pid: desc.pid },
                event_type_t::job_exit => EventType::JobExit {
                    pid: desc.pid,
                    internal_job_id: desc.internal_job_id,
                },
                event_type_t::caller_exit => EventType::CallerExit {
                    caller_id: desc.caller_id,
                },
                event_type_t::generic => EventType::Generic {
                    param: desc.str_param1.from_ffi(),
                },
                _ => panic!("invalid event description"),
            },
        }
    }
}

impl From<&EventDescription> for event_description_t {
    fn from(desc: &EventDescription) -> Self {
        let mut result = event_description_t {
            typ: (&desc.typ).into(),
            signal: Default::default(),
            pid: Default::default(),
            internal_job_id: Default::default(),
            caller_id: Default::default(),
            str_param1: match desc.typ.str_param1() {
                Some(param) => param.to_ffi(),
                None => UniquePtr::null(),
            },
        };
        match desc.typ {
            EventType::Any => (),
            EventType::Signal { signal } => result.signal = signal.code(),
            EventType::Variable { .. } => (),
            EventType::ProcessExit { pid } => result.pid = pid,
            EventType::JobExit {
                pid,
                internal_job_id,
            } => {
                result.pid = pid;
                result.internal_job_id = internal_job_id;
            }
            EventType::CallerExit { caller_id } => result.caller_id = caller_id,
            EventType::Generic { .. } => (),
        }
        result
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
            function_name: name.unwrap_or_else(WString::new),
            removed: AtomicBool::new(false),
            fired: AtomicBool::new(false),
        }
    }

    /// \return true if a handler is "one shot": it fires at most once.
    fn is_one_shot(&self) -> bool {
        match self.desc.typ {
            EventType::ProcessExit { pid } => pid != ANY_PID,
            EventType::JobExit { pid, .. } => pid != ANY_PID,
            EventType::CallerExit { .. } => true,
            EventType::Signal { .. }
            | EventType::Variable { .. }
            | EventType::Generic { .. }
            | EventType::Any => false,
        }
    }

    /// Tests if this event handler matches an event that has occurred.
    fn matches(&self, event: &Event) -> bool {
        match (&self.desc.typ, &event.desc.typ) {
            (EventType::Any, _) => true,
            (EventType::Signal { signal }, EventType::Signal { signal: ev_signal }) => {
                signal == ev_signal
            }
            (EventType::Variable { name }, EventType::Variable { name: ev_name }) => {
                name == ev_name
            }
            (EventType::ProcessExit { pid }, EventType::ProcessExit { pid: ev_pid }) => {
                *pid == ANY_PID || pid == ev_pid
            }
            (
                EventType::JobExit {
                    pid,
                    internal_job_id,
                },
                EventType::JobExit {
                    internal_job_id: ev_internal_job_id,
                    ..
                },
            ) => *pid == ANY_PID || internal_job_id == ev_internal_job_id,
            (
                EventType::CallerExit { caller_id },
                EventType::CallerExit {
                    caller_id: ev_caller_id,
                },
            ) => caller_id == ev_caller_id,
            (EventType::Generic { param }, EventType::Generic { param: ev_param }) => {
                param == ev_param
            }
            (_, _) => false,
        }
    }
}
type EventHandlerList = Vec<Arc<EventHandler>>;

impl EventHandler {
    fn desc(&self) -> event_description_t {
        (&self.desc).into()
    }
    fn function_name(self: &EventHandler) -> UniquePtr<CxxWString> {
        self.function_name.to_ffi()
    }
    fn set_removed(self: &mut EventHandler) {
        self.removed.store(true, Ordering::Relaxed);
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Event {
    desc: EventDescription,
    arguments: Vec<WString>,
}

impl Event {
    pub fn generic(desc: WString) -> Self {
        Self {
            desc: EventDescription {
                typ: EventType::Generic { param: desc },
            },
            arguments: vec![],
        }
    }

    pub fn variable_erase(name: WString) -> Self {
        Self {
            desc: EventDescription {
                typ: EventType::Variable { name: name.clone() },
            },
            arguments: vec!["VARIABLE".into(), "ERASE".into(), name],
        }
    }

    pub fn variable_set(name: WString) -> Self {
        Self {
            desc: EventDescription {
                typ: EventType::Variable { name: name.clone() },
            },
            arguments: vec!["VARIABLE".into(), "SET".into(), name],
        }
    }

    pub fn process_exit(pid: pid_t, status: i32) -> Self {
        Self {
            desc: EventDescription {
                typ: EventType::ProcessExit { pid },
            },
            arguments: vec![
                "PROCESS_EXIT".into(),
                pid.to_string().into(),
                status.to_string().into(),
            ],
        }
    }

    pub fn job_exit(pgid: pid_t, jid: u64) -> Self {
        Self {
            desc: EventDescription {
                typ: EventType::JobExit {
                    pid: pgid,
                    internal_job_id: jid,
                },
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
            desc: EventDescription {
                typ: EventType::CallerExit {
                    caller_id: internal_job_id,
                },
            },
            arguments: vec![
                "JOB_EXIT".into(),
                job_id.to_wstring(),
                "0".into(), // historical
            ],
        }
    }

    /// Test if specified event is blocked.
    fn is_blocked(&self, parser: &mut Parser) -> bool {
        let mut i = 0;
        while let Some(block) = parser.block_at_index(i) {
            i += 1;
            if block.event_blocks != 0 {
                return true;
            }
        }

        parser.global_event_blocks != 0
    }
}

fn new_event_generic(desc: wcharz_t) -> Box<Event> {
    Box::new(Event::generic(desc.into()))
}

fn new_event_variable_erase(name: &CxxWString) -> Box<Event> {
    Box::new(Event::variable_erase(name.from_ffi()))
}

fn new_event_variable_set(name: &CxxWString) -> Box<Event> {
    Box::new(Event::variable_set(name.from_ffi()))
}

fn new_event_process_exit(pid: i32, status: i32) -> Box<Event> {
    Box::new(Event::process_exit(pid, status))
}

fn new_event_job_exit(pgid: i32, jid: u64) -> Box<Event> {
    Box::new(Event::job_exit(pgid, jid))
}

fn new_event_caller_exit(internal_job_id: u64, job_id: i32) -> Box<Event> {
    Box::new(Event::caller_exit(
        internal_job_id,
        MaybeJobId(if job_id == -1 {
            None
        } else {
            Some(JobId::new(
                NonZeroU32::new(u32::try_from(job_id).unwrap()).unwrap(),
            ))
        }),
    ))
}

impl Event {
    fn clone_ffi(&self) -> Box<Event> {
        Box::new(self.clone())
    }
}

fn event_add_handler_ffi(desc: &event_description_t, name: &CxxWString) {
    add_handler(EventHandler::new(desc.into(), Some(name.from_ffi())));
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
        .map_or(false, |s| s.load(Ordering::Relaxed) > 0)
}

pub fn get_desc(parser: &Parser, evt: &Event) -> WString {
    let s = match &evt.desc.typ {
        EventType::Signal { signal } => {
            format!("signal handler for {} ({})", signal.name(), signal.desc(),)
        }
        EventType::Variable { name } => format!("handler for variable '{name}'"),
        EventType::ProcessExit { pid } => format!("exit handler for process {pid}"),
        EventType::JobExit { pid, .. } => {
            if let Some(job) = parser.job_get_from_pid(*pid) {
                format!("exit handler for job {}, '{}'", job.job_id(), job.command())
            } else {
                format!("exit handler for job with pid {pid}")
            }
        }
        EventType::CallerExit { .. } => "exit handler for command substitution caller".to_string(),
        EventType::Generic { param } => format!("handler for generic event '{param}'"),
        EventType::Any => unreachable!(),
    };

    WString::from_str(&s)
}

fn event_get_desc_ffi(parser: &parser_t, evt: &Event) -> UniquePtr<CxxWString> {
    todo!()
    // get_desc(parser, evt).to_ffi()
}

/// Add an event handler.
pub fn add_handler(eh: EventHandler) {
    if let EventType::Signal { signal } = eh.desc.typ {
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
            if let EventType::Signal { signal } = handler.desc.typ {
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

fn event_remove_function_handlers_ffi(name: &CxxWString) -> usize {
    remove_function_handlers(name.as_wstr())
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

fn event_get_function_handler_descs_ffi(name: &CxxWString) -> Vec<event_description_t> {
    get_function_handlers(name.as_wstr())
        .iter()
        .map(|h| event_description_t::from(&h.desc))
        .collect()
}

/// Perform the specified event. Since almost all event firings will not be matched by even a single
/// event handler, we make sure to optimize the 'no matches' path. This means that nothing is
/// allocated/initialized unless needed.
fn fire_internal(parser: &mut Parser, event: &Event) {
    assert!(
        parser.libdata_pod().is_event >= 0,
        "is_event should not be negative"
    );

    // Suppress fish_trace during events.
    let is_event = parser.libdata_pod().is_event;
    let mut parser = scoped_push(
        parser,
        |parser| &mut parser.libdata_pod_mut().is_event,
        is_event + 1,
    );
    let mut parser = scoped_push(
        &mut *parser,
        |parser| &mut parser.libdata_pod_mut().suppress_fish_trace,
        true,
    );

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
        let saved_is_interactive =
            std::mem::replace(&mut parser.libdata_mut().pods.is_interactive, false);
        let saved_statuses = parser.get_last_statuses();
        let mut parser = ScopeGuard::new(&mut *parser, |parser| {
            parser.set_last_statuses(saved_statuses);
            parser.libdata_mut().pods.is_interactive = saved_is_interactive;
        });

        FLOG!(
            event,
            "Firing event '",
            event.desc.typ.str_param1().unwrap_or(L!("")),
            "' to handler '",
            handler.function_name,
            "'"
        );

        let b = (*parser).push_block(Block::event_block(event.clone()));
        (*parser).eval(&buffer, &IoChain::new());
        (*parser).pop_block(b);

        handler.fired.store(true, Ordering::Relaxed);
        fired_one_shot |= handler.is_one_shot();
    }

    if fired_one_shot {
        remove_handlers_if(|h| h.fired.load(Ordering::Relaxed) && h.is_one_shot());
    }
}

/// Fire all delayed events attached to the given parser.
pub fn fire_delayed(parser: &mut Parser) {
    let ld = parser.libdata_pod();

    // Do not invoke new event handlers from within event handlers.
    if ld.is_event != 0 {
        return;
    };
    // Do not invoke new event handlers if we are unwinding (#6649).
    if signal_check_cancel() != 0 {
        return;
    };

    // We unfortunately can't keep this locked until we're done with it because the SIGWINCH handler
    // code might call back into here and we would delay processing of the events, leading to a test
    // failure under CI. (Yes, the `&mut Parser` is a lie.)
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
            termsize::SHARED_CONTAINER.updating(todo!("parser"));
        }
        let event = Event {
            desc: EventDescription {
                typ: EventType::Signal { signal: sig },
            },
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
                blocked_events = Some(BLOCKED_EVENTS.lock().expect("Mutex posioned"));
            }
            blocked_events.as_mut().unwrap().push(event);
        } else {
            // fire_internal() does not access BLOCKED_EVENTS so this call can't deadlock.
            fire_internal(parser, &event);
        }
    }
}

fn event_fire_delayed_ffi(parser: Pin<&mut parser_t>) {
    todo!()
}

/// Enqueue a signal event. Invoked from a signal handler.
pub fn enqueue_signal(signal: libc::c_int) {
    // Beware, we are in a signal handler
    PENDING_SIGNALS.mark(signal);
}

/// Fire the specified event event, executing it on `parser`.
pub fn fire(parser: &mut Parser, event: Event) {
    // Fire events triggered by signals.
    fire_delayed(parser);

    if event.is_blocked(parser) {
        BLOCKED_EVENTS.lock().expect("Mutex poisoned!").push(event);
    } else {
        fire_internal(parser, &event);
    }
}

fn event_fire_ffi(parser: Pin<&mut parser_t>, event: &Event) {
    todo!()
    // fire(parser.unpin(), event.clone())
}

#[widestrs]
const EVENT_FILTER_NAMES: [&wstr; 7] = [
    "signal"L,
    "variable"L,
    "exit"L,
    "process-exit"L,
    "job-exit"L,
    "caller-exit"L,
    "generic"L,
];

/// Print all events. If type_filter is not empty, only output events with that type.
pub fn print(streams: &mut io_streams_t, type_filter: &wstr) {
    let mut tmp = EVENT_HANDLERS
        .lock()
        .expect("event handler list should not be poisoned")
        .clone();

    tmp.sort_by(|e1, e2| e1.desc.typ.cmp(&e2.desc.typ));

    let mut last_type = None;
    for evt in tmp {
        // If we have a filter, skip events that don't match.
        if !evt.desc.typ.matches_filter(type_filter) {
            continue;
        }

        if last_type.as_ref() != Some(&evt.desc.typ) {
            if last_type.is_some() {
                streams.out.append(L!("\n"));
            }

            last_type = Some(evt.desc.typ.clone());
            streams
                .out
                .append(&sprintf!(L!("Event %ls\n"), evt.desc.typ.name()));
        }

        match &evt.desc.typ {
            EventType::Signal { signal } => {
                let name: WString = signal.name().into();
                streams
                    .out
                    .append(&sprintf!(L!("%ls %ls\n"), name, evt.function_name));
            }
            EventType::ProcessExit { .. } | EventType::JobExit { .. } => {}
            EventType::CallerExit { .. } => {
                streams
                    .out
                    .append(&sprintf!(L!("caller-exit %ls\n"), evt.function_name));
            }
            EventType::Variable { name: param } | EventType::Generic { param } => {
                streams
                    .out
                    .append(&sprintf!(L!("%ls %ls\n"), param, evt.function_name));
            }
            EventType::Any => unreachable!(),
        }
    }
}

fn event_print_ffi(streams: Pin<&mut ffi::io_streams_t>, type_filter: &CxxWString) {
    let mut streams = io_streams_t::new(streams);
    print(&mut streams, type_filter.as_wstr());
}

/// Fire a generic event with the specified name.
pub fn fire_generic(parser: &mut Parser, name: WString, arguments: Vec<WString>) {
    fire(
        parser,
        Event {
            desc: EventDescription {
                typ: EventType::Generic { param: name },
            },
            arguments,
        },
    )
}

fn event_fire_generic_ffi(
    parser: Pin<&mut parser_t>,
    name: &CxxWString,
    arguments: &CxxVector<wcharz_t>,
) {
    todo!()
}
