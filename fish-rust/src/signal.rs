use std::num::NonZeroI32;

use crate::common::{exit_without_destructors, restore_term_foreground_process_group_for_exit};
use crate::event::{enqueue_signal, is_signal_observed};
use crate::termsize::termsize_handle_winch;
use crate::topic_monitor::{generation_t, invalid_generations, topic_monitor_principal, topic_t};
use crate::wchar::{wstr, WExt, L};
use crate::wchar_ffi::{AsWstr, WCharToFFI};
use crate::wutil::{fish_wcstoi, wgettext, wgettext_str, wperror};
use cxx::{CxxWString, UniquePtr};
use errno::{errno, set_errno};
use std::sync::atomic::{AtomicI32, Ordering};
use crate::wchar::wstr;
use crate::wchar_ffi::c_str;
use libc::SIG_DFL;
use widestring::U32CStr;
use widestring_suffix::widestrs;

#[cxx::bridge]
mod signal_ffi {
    extern "Rust" {
        fn signal_set_handlers(interactive: bool);
        fn signal_set_handlers_once(interactive: bool);
        #[cxx_name = "signal_handle"]
        fn signal_handle_ffi(sig: i32);
        fn signal_unblock_all();

        #[cxx_name = "sig2wcs"]
        fn sig2wcs_ffi(sig: i32) -> UniquePtr<CxxWString>;

        #[cxx_name = "wcs2sig"]
        fn wcs2sig_ffi(sig: &CxxWString) -> i32;

        #[cxx_name = "signal_get_desc"]
        fn signal_get_desc_ffi(sig: i32) -> UniquePtr<CxxWString>;

        fn signal_check_cancel() -> i32;
        fn signal_clear_cancel();
        fn signal_reset_handlers();

    }
}

fn sig2wcs_ffi(sig: i32) -> UniquePtr<CxxWString> {
    Signal::new(sig).name().to_ffi()
}

fn wcs2sig_ffi(sig: &CxxWString) -> i32 {
    if let Some(sig) = Signal::parse(sig.as_wstr()) {
        sig.code()
    } else {
        -1
    }
}

fn signal_get_desc_ffi(sig: i32) -> UniquePtr<CxxWString> {
    Signal::new(sig).desc().to_ffi()
}

fn signal_handle_ffi(sig: i32) {
    signal_handle(Signal::new(sig));
}

// This is extern "C" for FFI purposes, as this is used after fork().
#[no_mangle]
pub extern "C" fn get_signals_with_handlers_ffi(set: *mut libc::sigset_t) {
    get_signals_with_handlers(unsafe { &mut *set });
}

/// Store the "main" pid. This allows us to reliably determine if we are in a forked child.
static MAIN_PID: AtomicI32 = AtomicI32::new(0);

/// It's possible that we receive a signal after we have forked, but before we have reset the signal
/// handlers (or even run the pthread_atfork calls). In that event we will do something dumb like
/// swallow SIGINT. Ensure that doesn't happen. Check if we are the main fish process; if not, reset
/// and re-raise the signal. \return whether we re-raised the signal.
fn reraise_if_forked_child(sig: i32) -> bool {
    // Don't use is_forked_child: it relies on atfork handlers which may have not yet run.
    // Safety: getpid() is async-signal-safe.
    let pid = unsafe { libc::getpid() };
    if pid == MAIN_PID.load(Ordering::Relaxed) {
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

/// \return the most recent cancellation signal received by the fish process.
/// Currently only SIGINT is considered a cancellation signal.
/// This is thread safe.
pub fn signal_check_cancel() -> i32 {
    CANCELLATION_SIGNAL.load(Ordering::Relaxed)
}

// Declare these as an extern C functions and call them directly,
// in case the autocxx ffi allocates or does something else signal-unfriendly.
extern "C" {
    fn reader_sighup();
    fn reader_handle_sigint();
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
            termsize_handle_winch();
        }
        libc::SIGHUP => {
            // Exit unless the signal was trapped.
            if !observed {
                unsafe { reader_sighup() };
            }
            topic_monitor_principal().post(topic_t::sighupint);
        }
        libc::SIGTERM => {
            // Handle sigterm. The only thing we do is restore the front process ID, then die.
            if !observed {
                restore_term_foreground_process_group_for_exit();
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
            unsafe { reader_handle_sigint() };
            topic_monitor_principal().post(topic_t::sighupint);
        }
        libc::SIGCHLD => {
            // A child process stopped or exited.
            topic_monitor_principal().post(topic_t::sigchld);
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
pub fn signal_reset_handlers() {
    let mut act: libc::sigaction = unsafe { std::mem::zeroed() };
    unsafe { libc::sigemptyset(&mut act.sa_mask) };
    act.sa_flags = 0;
    act.sa_sigaction = libc::SIG_DFL;

    for data in SIGNAL_TABLE.iter() {
        if data.signal == libc::SIGHUP {
            let mut oact: libc::sigaction = unsafe { std::mem::zeroed() };
            unsafe { libc::sigaction(libc::SIGHUP, std::ptr::null(), &mut oact) };
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
    MAIN_PID.store(unsafe { libc::getpid() }, Ordering::Relaxed);

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
        wperror(L!("sigaction"));
        exit_without_destructors(1);
    }

    if interactive {
        set_interactive_handlers();
    }

    if cfg!(feature = "FISH_TSAN_WORKAROUNDS") {
        // Work around the following TSAN bug:
        // The structure containing signal information for a thread is lazily allocated by TSAN.
        // It is possible for the same thread to receive two allocations, if the signal handler
        // races with other allocation paths (e.g. a blocking call). This results in the first signal
        // being potentially dropped.
        // The workaround is to send ourselves a SIGCHLD signal now, to force the allocation to happen.
        // As no child is associated with this signal, it is OK if it is dropped, so long as the
        // allocation happens.
        unsafe { libc::kill(libc::getpid(), libc::SIGCHLD) };
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

pub fn get_signals_with_handlers(set: &mut libc::sigset_t) {
    unsafe { libc::sigemptyset(set) };
    for data in SIGNAL_TABLE.iter() {
        let mut act: libc::sigaction = unsafe { std::mem::zeroed() };
        unsafe { libc::sigaction(data.signal.code(), std::ptr::null(), &mut act) };
        // If SIGHUP is being ignored (e.g., because were were run via `nohup`) don't reset it.
        // We don't special case other signals because if they're being ignored that shouldn't
        // affect processes we spawn. They should get the default behavior for those signals.
        if data.signal == libc::SIGHUP && act.sa_sigaction == libc::SIG_IGN {
            continue;
        }
        if act.sa_sigaction != libc::SIG_DFL {
            unsafe { libc::sigaddset(set, data.signal.code()) };
        }
    }
}

/// Ensure we did not inherit any blocked signals. See issue #3964.
pub fn signal_unblock_all() {
    unsafe {
        let mut iset: libc::sigset_t = std::mem::zeroed();
        libc::sigemptyset(&mut iset);
        libc::sigprocmask(libc::SIG_SETMASK, &iset, std::ptr::null_mut());
    }
}

/// A Sigchecker can be used to check if a SIGINT (or SIGHUP) has been delivered.
pub struct SigChecker {
    topic: topic_t,
    gen: generation_t,
}

impl SigChecker {
    /// Create a new checker for the given topic.
    pub fn new(topic: topic_t) -> Self {
        let mut res = SigChecker { topic, gen: 0 };
        // Call check() to update our generation.
        res.check();
        res
    }

    /// Create a new checker for SIGHUP and SIGINT.
    pub fn new_sighupint() -> Self {
        Self::new(topic_t::sighupint)
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
        let mut gens = invalid_generations();
        *gens.at_mut(self.topic) = self.gen;
        tm.check(&mut gens, true /* wait */);
    }
}

/// Struct describing an entry for the lookup table used to convert between signal names and signal
/// ids, etc.
struct LookupEntry {
    signal: Signal,
    name: &'static wstr,
    desc: &'static wstr, // Note: this needs to be translated via gettext before presenting it to the user.
}

impl LookupEntry {
    const fn new(signal: i32, name: &'static wstr, desc: &'static wstr) -> Self {
        Self {
            signal: Signal::new(signal),
            name,
            desc,
        }
    }
}

// Lookup table used to convert between signal names and signal ids, etc.
#[rustfmt::skip]
#[widestrs]
const SIGNAL_TABLE : &[LookupEntry] = &[
    LookupEntry::new(libc::SIGHUP,    "SIGHUP"L, "Terminal hung up"L),
    LookupEntry::new(libc::SIGINT,    "SIGINT"L, "Quit request from job control (^C)"L),
    LookupEntry::new(libc::SIGQUIT,   "SIGQUIT"L, "Quit request from job control with core dump (^\\)"L),
    LookupEntry::new(libc::SIGILL,    "SIGILL"L, "Illegal instruction"L),
    LookupEntry::new(libc::SIGTRAP,   "SIGTRAP"L, "Trace or breakpoint trap"L),
    LookupEntry::new(libc::SIGABRT,   "SIGABRT"L, "Abort"L),
    LookupEntry::new(libc::SIGBUS,    "SIGBUS"L, "Misaligned address error"L),
    LookupEntry::new(libc::SIGFPE,    "SIGFPE"L, "Floating point exception"L),
    LookupEntry::new(libc::SIGKILL,   "SIGKILL"L, "Forced quit"L),
    LookupEntry::new(libc::SIGUSR1,   "SIGUSR1"L, "User defined signal 1"L),
    LookupEntry::new(libc::SIGUSR2,   "SIGUSR2"L, "User defined signal 2"L),
    LookupEntry::new(libc::SIGSEGV,   "SIGSEGV"L, "Address boundary error"L),
    LookupEntry::new(libc::SIGPIPE,   "SIGPIPE"L, "Broken pipe"L),
    LookupEntry::new(libc::SIGALRM,   "SIGALRM"L, "Timer expired"L),
    LookupEntry::new(libc::SIGTERM,   "SIGTERM"L, "Polite quit request"L),
    LookupEntry::new(libc::SIGCHLD,   "SIGCHLD"L, "Child process status changed"L),
    LookupEntry::new(libc::SIGCONT,   "SIGCONT"L, "Continue previously stopped process"L),
    LookupEntry::new(libc::SIGSTOP,   "SIGSTOP"L, "Forced stop"L),
    LookupEntry::new(libc::SIGTSTP,   "SIGTSTP"L, "Stop request from job control (^Z)"L),
    LookupEntry::new(libc::SIGTTIN,   "SIGTTIN"L, "Stop from terminal input"L),
    LookupEntry::new(libc::SIGTTOU,   "SIGTTOU"L, "Stop from terminal output"L),
    LookupEntry::new(libc::SIGURG,    "SIGURG"L, "Urgent socket condition"L),
    LookupEntry::new(libc::SIGXCPU,   "SIGXCPU"L, "CPU time limit exceeded"L),
    LookupEntry::new(libc::SIGXFSZ,   "SIGXFSZ"L, "File size limit exceeded"L),
    LookupEntry::new(libc::SIGVTALRM, "SIGVTALRM"L, "Virtual timefr expired"L),
    LookupEntry::new(libc::SIGPROF,   "SIGPROF"L, "Profiling timer expired"L),
    LookupEntry::new(libc::SIGWINCH,  "SIGWINCH"L, "Window size change"L),
    LookupEntry::new(libc::SIGIO,     "SIGIO"L, "I/O on asynchronous file descriptor is possible"L),
    LookupEntry::new(libc::SIGSYS,    "SIGSYS"L, "Bad system call"L),
    LookupEntry::new(libc::SIGIOT,    "SIGIOT"L, "Abort (Alias for SIGABRT)"L),

    #[cfg(any(feature = "bsd", target_os = "macos"))]
    LookupEntry::new(libc::SIGEMT,    "SIGEMT"L, "Unused signal"L),

    #[cfg(any(feature = "bsd", target_os = "macos"))]
    LookupEntry::new(libc::SIGINFO,   "SIGINFO"L, "Information request"L),

    #[cfg(target_os = "linux")]
    LookupEntry::new(libc::SIGSTKFLT, "SISTKFLT"L, "Stack fault"L),

    #[cfg(target_os = "linux")]
    LookupEntry::new(libc::SIGIOT,   "SIGIOT"L, "Abort (Alias for SIGABRT)"L),

    #[cfg(target_os = "linux")]
    #[allow(deprecated)]
    LookupEntry::new(libc::SIGUNUSED, "SIGUNUSED"L, "Unused signal"L),

    #[cfg(target_os = "linux")]
    LookupEntry::new(libc::SIGPWR,    "SIGPWR"L, "Power failure"L),

    // TODO: determine whether SIGWIND is defined on any platform.
    //LookupEntry::new(libc::SIGWIND,   "SIGWIND"L, "Window size change"L),
];

// Return true if two strings are equal, ignoring ASCII case.
fn equals_ascii_icase(left: &wstr, right: &wstr) -> bool {
    if left.len() != right.len() {
        return false;
    }
    for (lc, rc) in left.chars().zip(right.chars()) {
        if lc.to_ascii_lowercase() != rc.to_ascii_lowercase() {
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
            Some(entry) => wgettext_str(entry.desc),
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

<<<<<<< HEAD
// Need to use add_test for wgettext support.
use crate::ffi_tests::add_test;
||||||| parent of 618ac805e ([proc] WIP Port postfork)
pub fn signal_set_handlers_once(interactive: bool) {
    todo!()
    // static std::once_flag s_noninter_once;
    // std::call_once(s_noninter_once, signal_set_handlers, false);
=======
pub fn signal_reset_handlers() {
    todo!()
    // unsafe {
    //     let mut act: libc::sigaction = std::mem::zeroed();
    //     libc::sigemptyset(&mut act.sa_mask);
    //     act.sa_flags = 0;
    //     act.sa_handler = SIG_DFL;

    //     for data in signal_table {
    //         if data.signal == SIGHUP {
    //             let mut oact: libc::sigaction = std::mem::zeroed();
    //             libc::sigaction(SIGHUP, std::ptr::null(), &mut oact);
    //             if oact.sa_handler == SIG_IGN {
    //                 continue;
    //             }
    //         }
    //         libc::sigaction(data.signal, &act, std::ptr::null_mut());
    //     }
    // }
}

pub fn get_signals_with_handlers(set: &mut libc::sigset_t) {
    todo!()
}

pub fn signal_set_handlers_once(interactive: bool) {
    todo!()
    // static std::once_flag s_noninter_once;
    // std::call_once(s_noninter_once, signal_set_handlers, false);
>>>>>>> 618ac805e ([proc] WIP Port postfork)

add_test!("test_signal_name", || {
    let sig = Signal::new(libc::SIGINT);
    assert_eq!(sig.name(), "SIGINT");
});

#[rustfmt::skip]
add_test!("test_signal_parse", || {
    use crate::wchar_ext::ToWString;
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
});

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
