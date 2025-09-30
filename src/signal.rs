use std::mem::MaybeUninit;
use std::num::NonZeroI32;

use crate::common::exit_without_destructors;
use crate::event::{enqueue_signal, is_signal_observed};
use crate::nix::getpid;
use crate::reader::{reader_handle_sigint, reader_sighup, safe_restore_term_mode};
use crate::termsize::TermsizeContainer;
use crate::topic_monitor::{topic_monitor_principal, Generation, GenerationsList, Topic};
use crate::tty_handoff::{safe_deactivate_tty_protocols, safe_mark_tty_invalid};
use crate::wchar::prelude::*;
use crate::wutil::{fish_wcstoi, perror};
use errno::{errno, set_errno};
use once_cell::sync::Lazy;
use std::sync::atomic::{AtomicI32, Ordering};

/// Store the "main" pid. This allows us to reliably determine if we are in a forked child.
static MAIN_PID: AtomicI32 = AtomicI32::new(0);

/// It's possible that we receive a signal after we have forked, but before we have reset the signal
/// handlers (or even run the pthread_atfork calls). In that event we will do something dumb like
/// swallow SIGINT. Ensure that doesn't happen. Check if we are the main fish process; if not, reset
/// and re-raise the signal. Return whether we re-raised the signal.
fn reraise_if_forked_child(sig: i32) -> bool {
    // Don't use is_forked_child: it relies on atfork handlers which may have not yet run.
    if getpid() == MAIN_PID.load(Ordering::Relaxed) {
        return false;
    }

    // Safety: signal() and raise() are async-signal-safe.
    unsafe {
        libc::signal(sig, libc::SIG_DFL);
        libc::raise(sig);
    }
    true
}

/// The cancellation signal we have received.
/// Of course this is modified from a signal handler.
static CANCELLATION_SIGNAL: AtomicI32 = AtomicI32::new(0);

/// Set the cancellation signal to zero.
/// In generally this should only be done in interactive sessions.
pub fn signal_clear_cancel() {
    CANCELLATION_SIGNAL.store(0, Ordering::Relaxed);
}

/// Return the most recent cancellation signal received by the fish process.
/// Currently only SIGINT is considered a cancellation signal.
/// This is thread safe.
pub fn signal_check_cancel() -> i32 {
    CANCELLATION_SIGNAL.load(Ordering::Relaxed)
}

/// The single signal handler. By centralizing signal handling we ensure that we can never install
/// the "wrong" signal handler (see #5969).
extern "C" fn fish_signal_handler(
    sig: i32,
    _info: *mut libc::siginfo_t,
    _context: *mut libc::c_void,
) {
    // Ensure we preserve errno.
    let saved_errno = errno();

    // Check if we are a forked child.
    if reraise_if_forked_child(sig) {
        set_errno(saved_errno);
        return;
    }

    // Check if fish script cares about this.
    let observed = is_signal_observed(sig);
    if observed {
        enqueue_signal(sig);
    }

    // Do some signal-specific stuff.
    match sig {
        libc::SIGWINCH => {
            // Respond to a winch signal by telling the termsize container.
            TermsizeContainer::handle_winch();
        }
        libc::SIGHUP => {
            // Exit unless the signal was trapped.
            if !observed {
                reader_sighup();
                safe_mark_tty_invalid();
            }
            topic_monitor_principal().post(Topic::sighupint);
        }
        libc::SIGTERM => {
            // Handle sigterm. The only thing we do is restore the front process ID and disable protocols, then die.
            if !observed {
                safe_restore_term_mode();
                safe_deactivate_tty_protocols();
                // Safety: signal() and raise() are async-signal-safe.
                unsafe {
                    libc::signal(libc::SIGTERM, libc::SIG_DFL);
                    libc::raise(libc::SIGTERM);
                }
            }
        }
        libc::SIGINT => {
            // Cancel unless the signal was trapped.
            if !observed {
                CANCELLATION_SIGNAL.store(libc::SIGINT, Ordering::Relaxed);
            }
            reader_handle_sigint();
            topic_monitor_principal().post(Topic::sighupint);
        }
        libc::SIGCHLD => {
            // A child process stopped or exited.
            topic_monitor_principal().post(Topic::sigchld);
        }
        libc::SIGALRM => {
            // We have a sigalarm handler that does nothing. This is used in the signal torture
            // test, to verify that we behave correctly when receiving lots of irrelevant signals.
        }
        _ => {}
    }

    set_errno(saved_errno);
}

/// Set all signal handlers to SIG_DFL.
/// This is called after fork - it should be async signal safe.
pub fn signal_reset_handlers() {
    let mut act: libc::sigaction = unsafe { std::mem::zeroed() };
    unsafe { libc::sigemptyset(&mut act.sa_mask) };
    act.sa_flags = 0;
    act.sa_sigaction = libc::SIG_DFL;

    for data in SIGNAL_TABLE.iter() {
        if data.signal == libc::SIGHUP {
            let mut oact = MaybeUninit::uninit();
            unsafe { libc::sigaction(libc::SIGHUP, std::ptr::null(), oact.as_mut_ptr()) };
            let oact = unsafe { oact.assume_init() };
            if oact.sa_sigaction == libc::SIG_IGN {
                continue;
            }
        }
        unsafe {
            libc::sigaction(data.signal.code(), &act, std::ptr::null_mut());
        };
    }
}

// Wrapper around sigaction.
fn sigaction(sig: i32, act: &libc::sigaction, oact: *mut libc::sigaction) -> libc::c_int {
    // Note: historically many call sites have ignored return value of sigaction here.
    unsafe { libc::sigaction(sig, act, oact) }
}

fn set_interactive_handlers() {
    let signal_handler: usize = fish_signal_handler as usize;
    let mut act: libc::sigaction = unsafe { std::mem::zeroed() };
    let mut oact: libc::sigaction = unsafe { std::mem::zeroed() };
    act.sa_flags = 0;
    oact.sa_flags = 0;
    unsafe { libc::sigemptyset(&mut act.sa_mask) };

    let nullptr = std::ptr::null_mut();

    // Interactive mode. Ignore interactive signals.  We are a shell, we know what is best for
    // the user.
    act.sa_sigaction = libc::SIG_IGN;
    sigaction(libc::SIGTSTP, &act, nullptr);
    sigaction(libc::SIGTTOU, &act, nullptr);

    // We don't ignore SIGTTIN because we might send it to ourself.
    act.sa_sigaction = signal_handler;
    act.sa_flags = libc::SA_SIGINFO;
    sigaction(libc::SIGTTIN, &act, nullptr);

    // SIGTERM restores the terminal controlling process before dying.
    act.sa_sigaction = signal_handler;
    act.sa_flags = libc::SA_SIGINFO;
    sigaction(libc::SIGTERM, &act, nullptr);

    unsafe { libc::sigaction(libc::SIGHUP, nullptr, &mut oact) };
    if oact.sa_sigaction == libc::SIG_DFL {
        act.sa_sigaction = signal_handler;
        act.sa_flags = libc::SA_SIGINFO;
        sigaction(libc::SIGHUP, &act, nullptr);
    }

    // SIGALARM as part of our signal torture test
    act.sa_sigaction = signal_handler;
    act.sa_flags = libc::SA_SIGINFO;
    sigaction(libc::SIGALRM, &act, nullptr);

    act.sa_sigaction = signal_handler;
    act.sa_flags = libc::SA_SIGINFO;
    sigaction(libc::SIGWINCH, &act, nullptr);
}

/// Set signal handlers to fish default handlers.
pub fn signal_set_handlers(interactive: bool) {
    // Mark our main pid.
    MAIN_PID.store(getpid(), Ordering::Relaxed);

    use libc::SIG_IGN;
    let nullptr = std::ptr::null_mut();
    let mut act: libc::sigaction = unsafe { std::mem::zeroed() };

    act.sa_flags = 0;
    unsafe { libc::sigemptyset(&mut act.sa_mask) };

    // Ignore SIGPIPE. We'll detect failed writes and deal with them appropriately. We don't want
    // this signal interrupting other syscalls or terminating us.
    act.sa_sigaction = SIG_IGN;
    sigaction(libc::SIGPIPE, &act, nullptr);

    // Ignore SIGQUIT.
    act.sa_sigaction = SIG_IGN;
    sigaction(libc::SIGQUIT, &act, nullptr);

    // Apply our SIGINT handler.
    act.sa_sigaction = fish_signal_handler as usize;
    act.sa_flags = libc::SA_SIGINFO;
    sigaction(libc::SIGINT, &act, nullptr);

    // Whether or not we're interactive we want SIGCHLD to not interrupt restartable syscalls.
    act.sa_sigaction = fish_signal_handler as usize;
    act.sa_flags = libc::SA_SIGINFO | libc::SA_RESTART;
    if sigaction(libc::SIGCHLD, &act, nullptr) != 0 {
        perror("sigaction");
        exit_without_destructors(1);
    }

    if interactive {
        set_interactive_handlers();
    }

    if cfg!(feature = "tsan") {
        // Work around the following TSAN bug:
        // The structure containing signal information for a thread is lazily allocated by TSAN.
        // It is possible for the same thread to receive two allocations, if the signal handler
        // races with other allocation paths (e.g. a blocking call). This results in the first signal
        // being potentially dropped.
        // The workaround is to send ourselves a SIGCHLD signal now, to force the allocation to happen.
        // As no child is associated with this signal, it is OK if it is dropped, so long as the
        // allocation happens.
        unsafe { libc::kill(getpid(), libc::SIGCHLD) };
    }
}

pub fn signal_set_handlers_once(interactive: bool) {
    static NONINTER_ONCE: std::sync::Once = std::sync::Once::new();
    NONINTER_ONCE.call_once(|| signal_set_handlers(false));

    static INTER_ONCE: std::sync::Once = std::sync::Once::new();
    if interactive {
        INTER_ONCE.call_once(set_interactive_handlers);
    }
}

/// Mark that a signal is being handled.
pub fn signal_handle(sig: Signal) {
    let sig = sig.code();
    let mut act: libc::sigaction = unsafe { std::mem::zeroed() };

    // These should always be handled.
    if sig == libc::SIGINT
        || sig == libc::SIGQUIT
        || sig == libc::SIGTSTP
        || sig == libc::SIGTTIN
        || sig == libc::SIGTTOU
        || sig == libc::SIGCHLD
    {
        return;
    }

    act.sa_flags = 0;
    unsafe { libc::sigemptyset(&mut act.sa_mask) };
    act.sa_flags = libc::SA_SIGINFO;
    act.sa_sigaction = fish_signal_handler as usize;
    sigaction(sig, &act, std::ptr::null_mut());
}

pub static signals_to_default: Lazy<libc::sigset_t> = Lazy::new(|| {
    let mut set = MaybeUninit::uninit();
    unsafe { libc::sigemptyset(set.as_mut_ptr()) };
    for data in SIGNAL_TABLE.iter() {
        // If SIGHUP is being ignored (e.g., because were were run via `nohup`) don't reset it.
        // We don't special case other signals because if they're being ignored that shouldn't
        // affect processes we spawn. They should get the default behavior for those signals.
        if data.signal == libc::SIGHUP {
            let mut act = MaybeUninit::uninit();
            unsafe { libc::sigaction(data.signal.code(), std::ptr::null(), act.as_mut_ptr()) };
            let act = unsafe { act.assume_init() };
            if act.sa_sigaction == libc::SIG_IGN {
                continue;
            }
        }
        unsafe { libc::sigaddset(set.as_mut_ptr(), data.signal.code()) };
    }
    unsafe { set.assume_init() }
});

/// Ensure we did not inherit any blocked signals. See issue #3964.
pub fn signal_unblock_all() {
    unsafe {
        let mut iset = MaybeUninit::uninit();
        libc::sigemptyset(iset.as_mut_ptr());
        libc::sigprocmask(libc::SIG_SETMASK, iset.as_ptr(), std::ptr::null_mut());
    }
}

/// A Sigchecker can be used to check if a SIGINT (or SIGHUP) has been delivered.
pub struct SigChecker {
    topic: Topic,
    gen: Generation,
}

impl SigChecker {
    /// Create a new checker for the given topic.
    pub fn new(topic: Topic) -> Self {
        let mut res = SigChecker { topic, gen: 0 };
        // Call check() to update our generation.
        res.check();
        res
    }

    /// Create a new checker for SIGHUP and SIGINT.
    pub fn new_sighupint() -> Self {
        Self::new(Topic::sighupint)
    }

    /// Check if a sigint has been delivered since the last call to check(), or since the detector
    /// was created.
    pub fn check(&mut self) -> bool {
        let tm = topic_monitor_principal();
        let gen = tm.generation_for_topic(self.topic);
        let changed = self.gen != gen;
        self.gen = gen;
        changed
    }

    /// Wait until a sigint is delivered.
    pub fn wait(&self) {
        let tm = topic_monitor_principal();
        let gens = GenerationsList::invalid();
        gens.set(self.topic, self.gen);
        tm.check(&gens, true /* wait */);
    }
}

/// Struct describing an entry for the lookup table used to convert between signal names and signal
/// ids, etc.
struct LookupEntry {
    signal: Signal,
    name: &'static wstr,
    desc: LocalizableString, // Note: this needs to be localized via gettext before presenting it to the user.
}

impl LookupEntry {
    const fn new(signal: i32, name: &'static wstr, desc: LocalizableString) -> Self {
        Self {
            signal: Signal::new(signal),
            name,
            desc,
        }
    }
}

macro_rules! signal_entry {
    ($name:ident, $desc:literal) => {
        LookupEntry::new(
            libc::$name,
            L!(stringify!($name)),
            localizable_string!($desc),
        )
    };
    ($name:ident, $desc:expr) => {
        LookupEntry::new(libc::$name, L!(stringify!($name)), $desc)
    };
}

// Lookup table used to convert between signal names and signal ids, etc.
#[rustfmt::skip]
const SIGNAL_TABLE : &[LookupEntry] = &[
    signal_entry!(SIGHUP, "Terminal hung up"),
    signal_entry!(SIGINT, "Quit request from job control (^C)"),
    signal_entry!(SIGQUIT, "Quit request from job control with core dump (^\\)"),
    signal_entry!(SIGILL, "Illegal instruction"),
    signal_entry!(SIGTRAP, "Trace or breakpoint trap"),
    signal_entry!(SIGABRT, "Abort"),
    signal_entry!(SIGBUS, "Misaligned address error"),
    signal_entry!(SIGFPE, "Floating point exception"),
    signal_entry!(SIGKILL, "Forced quit"),
    signal_entry!(SIGUSR1, "User defined signal 1"),
    signal_entry!(SIGUSR2, "User defined signal 2"),
    signal_entry!(SIGSEGV, "Address boundary error"),
    signal_entry!(SIGPIPE, "Broken pipe"),
    signal_entry!(SIGALRM, "Timer expired"),
    signal_entry!(SIGTERM, "Polite quit request"),
    signal_entry!(SIGCHLD, "Child process status changed"),
    signal_entry!(SIGCONT, "Continue previously stopped process"),
    signal_entry!(SIGSTOP, "Forced stop"),
    signal_entry!(SIGTSTP, "Stop request from job control (^Z)"),
    signal_entry!(SIGTTIN, "Stop from terminal input"),
    signal_entry!(SIGTTOU, "Stop from terminal output"),
    signal_entry!(SIGURG, "Urgent socket condition"),
    signal_entry!(SIGXCPU, "CPU time limit exceeded"),
    signal_entry!(SIGXFSZ, "File size limit exceeded"),
    signal_entry!(SIGVTALRM, "Virtual timer expired"),
    signal_entry!(SIGPROF, "Profiling timer expired"),
    signal_entry!(SIGWINCH, "Window size change"),
    signal_entry!(SIGIO, "I/O on asynchronous file descriptor is possible"),
    signal_entry!(SIGSYS, "Bad system call"),
    signal_entry!(SIGIOT, "Abort (Alias for SIGABRT)"),

    #[cfg(any(apple, bsd))]
    signal_entry!(SIGEMT, SIGEMT_DESC),

    #[cfg(any(apple, bsd))]
    signal_entry!(SIGINFO, SIGINFO_DESC),

    #[cfg(target_os = "linux")]
    signal_entry!(SIGSTKFLT, SIGSTKFLT_DESC),

    #[cfg(target_os = "linux")]
    signal_entry!(SIGIOT, SIGIOT_DESC),

    #[cfg(target_os = "linux")]
    signal_entry!(SIGPWR, SIGPWR_DESC),

    // TODO: determine whether SIGWIND is defined on any platform.
    //signal_entry!(SIGWIND, "Window size change"),
];

localizable_consts!(
    #[allow(dead_code)]
    SIGEMT_DESC "Unused signal"

    #[allow(dead_code)]
    SIGINFO_DESC "Information request"

    #[allow(dead_code)]
    SIGSTKFLT_DESC "Stack fault"

    #[allow(dead_code)]
    SIGIOT_DESC "Abort (Alias for SIGABRT)"

    #[allow(dead_code)]
    SIGPWR_DESC "Power failure"
);

// Return true if two strings are equal, ignoring ASCII case.
fn equals_ascii_icase(left: &wstr, right: &wstr) -> bool {
    if left.len() != right.len() {
        return false;
    }
    for (lc, rc) in left.chars().zip(right.chars()) {
        if !lc.eq_ignore_ascii_case(&rc) {
            return false;
        }
    }
    true
}

/// Test if \c name is a string describing the signal named \c canonical.
fn match_signal_name(canonical: &wstr, mut name: &wstr) -> bool {
    // Skip the "SIG" prefix if it exists.
    if name.char_count() >= 3 && equals_ascii_icase(name.slice_to(3), L!("sig")) {
        name = name.slice_from(3)
    }
    equals_ascii_icase(canonical.slice_from(3), name)
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, PartialOrd, Ord)]
/// A wrapper around the system signal code.
pub struct Signal(NonZeroI32);

impl Signal {
    /// Creates a new `Signal` to represent the passed system signal code `sig`.
    /// Panics if `sig` is zero.
    pub const fn new(sig: i32) -> Self {
        match NonZeroI32::new(sig) {
            None => panic!("Invalid zero signal value!"),
            Some(result) => Signal(result),
        }
    }

    /// Return the LookupEntry for ourself.
    fn get_lookup_entry(&self) -> Option<&'static LookupEntry> {
        SIGNAL_TABLE
            .iter()
            .find(|entry| entry.signal == self.code())
    }

    /// Get string representation of a signal.
    /// Previously sig2wcs().
    pub fn name(&self) -> &'static wstr {
        match self.get_lookup_entry() {
            Some(entry) => entry.name,
            None => wgettext!("Unknown"),
        }
    }

    /// Returns a description of the specified signal.
    /// Previously signal_get_desc().
    pub fn desc(&self) -> &'static wstr {
        match self.get_lookup_entry() {
            Some(entry) => entry.desc.localize(),
            None => wgettext!("Unknown"),
        }
    }

    pub fn code(&self) -> i32 {
        self.0.into()
    }
    /// Parses a string into the equivalent [`Signal`] sharing the same name.
    /// Accepts both `SIGABC` and `ABC` to match against `Signal::SIGABC`. If the signal name is not
    /// recognized, `None` is returned.
    /// This also accepts integer codes via fish_wcstoi().
    /// Previously sig2wcs().
    pub fn parse(name: &wstr) -> Option<Signal> {
        for entry in SIGNAL_TABLE.iter() {
            if match_signal_name(entry.name, name) {
                return Some(entry.signal);
            }
        }

        if let Ok(num) = fish_wcstoi(name) {
            if num > 0 {
                return Some(Signal::new(num));
            }
        }
        None
    }
}

// Allow signals to be compared against i32.
impl PartialEq<i32> for Signal {
    fn eq(&self, other: &i32) -> bool {
        self.code() == *other
    }
}

impl From<Signal> for i32 {
    fn from(value: Signal) -> Self {
        value.code()
    }
}

impl From<Signal> for usize {
    fn from(value: Signal) -> Self {
        usize::try_from(value.code()).unwrap()
    }
}

impl From<Signal> for NonZeroI32 {
    fn from(value: Signal) -> Self {
        value.0
    }
}

// Need to use add_test for wgettext support.

#[test]
fn test_signal_name() {
    let sig = Signal::new(libc::SIGINT);
    assert_eq!(sig.name(), "SIGINT");
}

#[rustfmt::skip]
#[test]
fn test_signal_parse() {
    assert_eq!(Signal::parse(L!("SIGHUP")), Some(Signal::new(libc::SIGHUP)));
    assert_eq!(Signal::parse(L!("sigwinch")), Some(Signal::new(libc::SIGWINCH)));
    assert_eq!(Signal::parse(L!("TSTP")), Some(Signal::new(libc::SIGTSTP)));
    assert_eq!(Signal::parse(L!("TstP")), Some(Signal::new(libc::SIGTSTP)));
    assert_eq!(Signal::parse(L!("sigCONT")), Some(Signal::new(libc::SIGCONT)));
    assert_eq!(Signal::parse(L!("SIGFOO")), None);
    assert_eq!(Signal::parse(L!("")), None);
    assert_eq!(Signal::parse(L!("SIG")), None);
    assert_eq!(Signal::parse(L!("سلام")), None);

    assert_eq!(Signal::parse(&libc::SIGINT.to_wstring()), Some(Signal::new(libc::SIGINT)));
    assert_eq!(Signal::parse(L!("0")), None);
    assert_eq!(Signal::parse(L!("-0")), None);
    assert_eq!(Signal::parse(L!("-1")), None);
}

#[test]
#[cfg(any(target_os = "freebsd", target_os = "netbsd", target_os = "openbsd"))]
/// Verify bsd feature is detected on the known BSDs, which gives us greater confidence it'll work
/// for the unknown ones too. We don't need to do this for Linux and macOS because we're using
/// rust's native OS targeting for those.
fn bsd_signals() {
    assert_eq!(Signal::parse(L!("SIGEMT")), Some(Signal::new(libc::SIGEMT)));
    assert_eq!(
        Signal::parse(L!("SIGINFO")),
        Some(Signal::new(libc::SIGINFO))
    );
}
