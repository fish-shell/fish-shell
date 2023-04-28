use std::borrow::Cow;
use std::num::NonZeroI32;

use crate::ffi;
use crate::topic_monitor::{generation_t, invalid_generations, topic_monitor_principal, topic_t};
use crate::wchar::wstr;
use crate::wchar_ffi::c_str;
use libc::{c_int, SIG_DFL};
use widestring::U32CStr;
use widestring_suffix::widestrs;

/// A sigint_detector_t can be used to check if a SIGINT (or SIGHUP) has been delivered.
pub struct sigchecker_t {
    topic: topic_t,
    gen: generation_t,
}

impl sigchecker_t {
    /// Create a new checker for the given topic.
    pub fn new(topic: topic_t) -> sigchecker_t {
        let mut res = sigchecker_t { topic, gen: 0 };
        // Call check() to update our generation.
        res.check();
        res
    }

    /// Create a new checker for SIGHUP and SIGINT.
    pub fn new_sighupint() -> sigchecker_t {
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

#[deprecated(note = "Use [`Signal::parse()`] instead.")]
/// Get the integer signal value representing the specified signal.
pub fn wcs2sig(s: &wstr) -> Option<usize> {
    let sig = ffi::wcs2sig(c_str!(s));

    sig.0.try_into().ok()
}

#[deprecated(note = "Use [`Signal::name()`] instead.")]
/// Get string representation of a signal.
pub fn sig2wcs(sig: i32) -> &'static wstr {
    let s = ffi::sig2wcs(ffi::c_int(sig));
    let s = unsafe { U32CStr::from_ptr_str(s) };

    wstr::from_ucstr(s).expect("signal name should be valid utf-32")
}

#[deprecated(note = "Use [`Signal::desc()`] instead.")]
/// Returns a description of the specified signal.
pub fn signal_get_desc(sig: i32) -> &'static wstr {
    let s = ffi::signal_get_desc(ffi::c_int(sig));
    let s = unsafe { U32CStr::from_ptr_str(s) };

    wstr::from_ucstr(s).expect("signal description should be valid utf-32")
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, PartialOrd, Ord)]
/// A wrapper around the system signal code.
pub struct Signal(NonZeroI32);

impl Signal {
    pub const SIGHUP: Signal = Signal::new(libc::SIGHUP);
    pub const SIGINT: Signal = Signal::new(libc::SIGINT);
    pub const SIGQUIT: Signal = Signal::new(libc::SIGQUIT);
    pub const SIGILL: Signal = Signal::new(libc::SIGILL);
    pub const SIGTRAP: Signal = Signal::new(libc::SIGTRAP);
    pub const SIGABRT: Signal = Signal::new(libc::SIGABRT);
    /// Available on BSD and macOS only.
    #[cfg(any(feature = "bsd", target_os = "macos"))]
    pub const SIGEMT: Signal = Signal::new(libc::SIGEMT);
    pub const SIGFPE: Signal = Signal::new(libc::SIGFPE);
    pub const SIGKILL: Signal = Signal::new(libc::SIGKILL);
    pub const SIGBUS: Signal = Signal::new(libc::SIGBUS);
    pub const SIGSEGV: Signal = Signal::new(libc::SIGSEGV);
    pub const SIGSYS: Signal = Signal::new(libc::SIGSYS);
    pub const SIGPIPE: Signal = Signal::new(libc::SIGPIPE);
    pub const SIGALRM: Signal = Signal::new(libc::SIGALRM);
    pub const SIGTERM: Signal = Signal::new(libc::SIGTERM);
    pub const SIGURG: Signal = Signal::new(libc::SIGURG);
    pub const SIGSTOP: Signal = Signal::new(libc::SIGSTOP);
    pub const SIGTSTP: Signal = Signal::new(libc::SIGTSTP);
    pub const SIGCONT: Signal = Signal::new(libc::SIGCONT);
    pub const SIGCHLD: Signal = Signal::new(libc::SIGCHLD);
    pub const SIGTTIN: Signal = Signal::new(libc::SIGTTIN);
    pub const SIGTTOU: Signal = Signal::new(libc::SIGTTOU);
    pub const SIGIO: Signal = Signal::new(libc::SIGIO);
    pub const SIGXCPU: Signal = Signal::new(libc::SIGXCPU);
    pub const SIGXFSZ: Signal = Signal::new(libc::SIGXFSZ);
    pub const SIGVTALRM: Signal = Signal::new(libc::SIGVTALRM);
    pub const SIGPROF: Signal = Signal::new(libc::SIGPROF);
    pub const SIGWINCH: Signal = Signal::new(libc::SIGWINCH);
    /// Available on BSD and macOS only.
    #[cfg(any(feature = "bsd", target_os = "macos"))]
    pub const SIGINFO: Signal = Signal::new(libc::SIGINFO);
    pub const SIGUSR1: Signal = Signal::new(libc::SIGUSR1);
    pub const SIGUSR2: Signal = Signal::new(libc::SIGUSR2);
    /// Available on BSD only.
    #[cfg(any(target_os = "freebsd"))]
    pub const SIGTHR: Signal = Signal::new(32); // Not exposed by libc crate
    /// Available on BSD only.
    #[cfg(any(target_os = "freebsd"))]
    pub const SIGLIBRT: Signal = Signal::new(33); // Not exposed by libc crate
    #[cfg(target_os = "linux")]
    /// Available on Linux only.
    pub const SIGSTKFLT: Signal = Signal::new(libc::SIGSTKFLT);
    #[cfg(target_os = "linux")]
    /// Available on Linux only.
    pub const SIGPWR: Signal = Signal::new(libc::SIGPWR);

    // Signals aliased to other signals
    /// Available on Linux only. Use [`Signal::SIGIO`] instead.
    #[cfg(target_os = "linux")]
    pub const SIGPOLL: Signal = Signal::SIGIO;
    /// Available on Linux only. Use [`Signal::SIGSYS`] instead.
    #[cfg(target_os = "linux")]
    #[deprecated(note = "Use SIGSYS instead")]
    pub const SIGUNUSED: Signal = Signal::SIGSYS;
    /// Available on Linux only. Alias for [`Signal::SIGABRT`].
    #[cfg(target_os = "linux")]
    pub const SIGIOT: Signal = Signal::SIGABRT;
}

impl Signal {
    #[widestrs]
    const UNKNOWN_SIG_NAME: &'static wstr = "SIG???"L;

    /// Creates a new `Signal` to represent the passed system signal code `sig`.
    /// Panics if `sig` is zero.
    pub const fn new(sig: i32) -> Self {
        match NonZeroI32::new(sig) {
            None => panic!("Invalid zero signal value!"),
            Some(result) => Signal(result),
        }
    }

    #[widestrs]
    pub const fn name(&self) -> &'static wstr {
        match *self {
            Signal::SIGHUP => "SIGHUP"L,
            Signal::SIGINT => "SIGINT"L,
            Signal::SIGQUIT => "SIGQUIT"L,
            Signal::SIGILL => "SIGILL"L,
            Signal::SIGTRAP => "SIGTRAP"L,
            Signal::SIGABRT => "SIGABRT"L,
            #[cfg(any(feature = "bsd", target_os = "macos"))]
            Signal::SIGEMT => "SIGEMT"L,
            Signal::SIGFPE => "SIGFPE"L,
            Signal::SIGKILL => "SIGKILL"L,
            Signal::SIGBUS => "SIGBUS"L,
            Signal::SIGSEGV => "SIGSEGV"L,
            Signal::SIGSYS => "SIGSYS"L,
            Signal::SIGPIPE => "SIGPIPE"L,
            Signal::SIGALRM => "SIGALRM"L,
            Signal::SIGTERM => "SIGTERM"L,
            Signal::SIGURG => "SIGURG"L,
            Signal::SIGSTOP => "SIGSTOP"L,
            Signal::SIGTSTP => "SIGTSTP"L,
            Signal::SIGCONT => "SIGCONT"L,
            Signal::SIGCHLD => "SIGCHLD"L,
            Signal::SIGTTIN => "SIGTTIN"L,
            Signal::SIGTTOU => "SIGTTOU"L,
            Signal::SIGIO => "SIGIO"L,
            Signal::SIGXCPU => "SIGXCPU"L,
            Signal::SIGXFSZ => "SIGXFSZ"L,
            Signal::SIGVTALRM => "SIGVTALRM"L,
            Signal::SIGPROF => "SIGPROF"L,
            Signal::SIGWINCH => "SIGWINCH"L,
            #[cfg(any(feature = "bsd", target_os = "macos"))]
            Signal::SIGINFO => "SIGINFO"L,
            Signal::SIGUSR1 => "SIGUSR1"L,
            Signal::SIGUSR2 => "SIGUSR2"L,
            #[cfg(any(target_os = "freebsd"))]
            Signal::SIGTHR => "SIGTHR"L,
            #[cfg(any(target_os = "freebsd"))]
            Signal::SIGLIBRT => "SIGLIBRT"L,
            #[cfg(target_os = "linux")]
            Signal::SIGSTKFLT => "SIGSTKFLT"L,
            #[cfg(target_os = "linux")]
            Signal::SIGPWR => "SIGPWR"L,
            Signal(_) => Self::UNKNOWN_SIG_NAME,
        }
    }

    pub fn code(&self) -> i32 {
        self.0.into()
    }

    #[widestrs]
    pub const fn desc(&self) -> &'static wstr {
        match *self {
            Signal::SIGHUP => "Terminal hung up"L,
            Signal::SIGINT => "Quit request from job control (^C)"L,
            Signal::SIGQUIT => "Quit request from job control with core dump (^\\)"L,
            Signal::SIGILL => "Illegal instruction"L,
            Signal::SIGTRAP => "Trace or breakpoint trap"L,
            Signal::SIGABRT => "Abort"L,
            #[cfg(any(feature = "bsd", target_os = "macos"))]
            Signal::SIGEMT => "Emulator trap"L,
            Signal::SIGFPE => "Floating point exception"L,
            Signal::SIGKILL => "Forced quit"L,
            Signal::SIGBUS => "Misaligned address error"L,
            Signal::SIGSEGV => "Address boundary error"L,
            Signal::SIGSYS => "Bad system call"L,
            Signal::SIGPIPE => "Broken pipe"L,
            Signal::SIGALRM => "Timer expired"L,
            Signal::SIGTERM => "Polite quit request"L,
            Signal::SIGURG => "Urgent socket condition"L,
            Signal::SIGSTOP => "Forced stop"L,
            Signal::SIGTSTP => "Stop request from job control (^Z)"L,
            Signal::SIGCONT => "Continue previously stopped process"L,
            Signal::SIGCHLD => "Child process status changed"L,
            Signal::SIGTTIN => "Stop from terminal input"L,
            Signal::SIGTTOU => "Stop from terminal output"L,
            Signal::SIGIO => "I/O on asynchronous file descriptior is possible"L,
            Signal::SIGXCPU => "CPU time limit exceeded"L,
            Signal::SIGXFSZ => "File size limit exceeded"L,
            Signal::SIGVTALRM => "Virtual timer expired"L,
            Signal::SIGPROF => "Profiling timer expired"L,
            Signal::SIGWINCH => "Window size change"L,
            #[cfg(any(feature = "bsd", target_os = "macos"))]
            Signal::SIGINFO => "Information request"L,
            Signal::SIGUSR1 => "User-defined signal 1"L,
            Signal::SIGUSR2 => "User-defined signal 2"L,
            #[cfg(any(target_os = "freebsd"))]
            Signal::SIGTHR => "Thread interrupt"L,
            #[cfg(any(target_os = "freebsd"))]
            Signal::SIGLIBRT => "Real-time library interrupt"L,
            #[cfg(target_os = "linux")]
            Signal::SIGSTKFLT => "Stack fault"L,
            #[cfg(target_os = "linux")]
            Signal::SIGPWR => "Power failure"L,
            Signal(_) => "Unknown"L,
        }
    }

    /// Parses a string into the equivalent [`Signal`] sharing the same name.
    ///
    /// Accepts both `SIGABC` and `ABC` to match against `Signal::SIGABC`. If the signal name is not
    /// recognized, `None` is returned.
    pub fn parse(name: &str) -> Option<Signal> {
        let mut chars = name.chars();
        let name = loop {
            match chars.next() {
                None => break Cow::Borrowed(name),
                Some(c) if !c.is_ascii() => return None,
                Some(c) if !c.is_ascii_uppercase() => break Cow::Owned(name.to_ascii_uppercase()),
                _ => (),
            };
        };

        let name = name.strip_prefix("SIG").unwrap_or(name.as_ref());
        match name {
            "HUP" => Some(Signal::SIGHUP),
            "INT" => Some(Signal::SIGINT),
            "QUIT" => Some(Signal::SIGQUIT),
            "ILL" => Some(Signal::SIGILL),
            "TRAP" => Some(Signal::SIGTRAP),
            "ABRT" => Some(Signal::SIGABRT),
            #[cfg(any(feature = "bsd", target_os = "macos"))]
            "EMT" => Some(Signal::SIGEMT),
            "FPE" => Some(Signal::SIGFPE),
            "KILL" => Some(Signal::SIGKILL),
            "BUS" => Some(Signal::SIGBUS),
            "SEGV" => Some(Signal::SIGSEGV),
            "SYS" => Some(Signal::SIGSYS),
            "PIPE" => Some(Signal::SIGPIPE),
            "ALRM" => Some(Signal::SIGALRM),
            "TERM" => Some(Signal::SIGTERM),
            "URG" => Some(Signal::SIGURG),
            "STOP" => Some(Signal::SIGSTOP),
            "TSTP" => Some(Signal::SIGTSTP),
            "CONT" => Some(Signal::SIGCONT),
            "CHLD" => Some(Signal::SIGCHLD),
            "TTIN" => Some(Signal::SIGTTIN),
            "TTOU" => Some(Signal::SIGTTOU),
            "IO" => Some(Signal::SIGIO),
            "XCPU" => Some(Signal::SIGXCPU),
            "XFSZ" => Some(Signal::SIGXFSZ),
            "VTALRM" => Some(Signal::SIGVTALRM),
            "PROF" => Some(Signal::SIGPROF),
            "WINCH" => Some(Signal::SIGWINCH),
            #[cfg(any(feature = "bsd", target_os = "macos"))]
            "INFO" => Some(Signal::SIGINFO),
            "USR1" => Some(Signal::SIGUSR1),
            "USR2" => Some(Signal::SIGUSR2),
            #[cfg(any(target_os = "freebsd"))]
            "THR" => Some(Signal::SIGTHR),
            #[cfg(any(target_os = "freebsd"))]
            "LIBRT" => Some(Signal::SIGLIBRT),
            #[cfg(target_os = "linux")]
            "STKFLT" => Some(Signal::SIGSTKFLT),
            #[cfg(target_os = "linux")]
            "PWR" => Some(Signal::SIGPWR),
            _ => None,
        }
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

    // static std::once_flag s_inter_once;
    // if (interactive) std::call_once(s_inter_once, set_interactive_handlers);
}

pub fn signal_clear_cancel() {
    todo!()
}

pub fn signal_check_cancel() -> Option<Signal> {
    todo!()
}

#[test]
fn signal_name() {
    let sig = Signal::SIGINT;
    assert_eq!(sig.name(), "SIGINT");
}

#[test]
fn parse_signal() {
    assert_eq!(Signal::parse("SIGHUP"), Some(Signal::SIGHUP));
    assert_eq!(Signal::parse("sigwinch"), Some(Signal::SIGWINCH));
    assert_eq!(Signal::parse("TSTP"), Some(Signal::SIGTSTP));
    assert_eq!(Signal::parse("TstP"), Some(Signal::SIGTSTP));
    assert_eq!(Signal::parse("sigCONT"), Some(Signal::SIGCONT));
    assert_eq!(Signal::parse("SIGFOO"), None);
    assert_eq!(Signal::parse(""), None);
    assert_eq!(Signal::parse("SIG"), None);
    assert_eq!(Signal::parse("سلام"), None);
}

#[test]
#[cfg(any(target_os = "freebsd", target_os = "netbsd", target_os = "openbsd"))]
/// Verify bsd feature is detected on the known BSDs, which gives us greater confidence it'll work
/// for the unknown ones too. We don't need to do this for Linux and macOS because we're using
/// rust's native OS targeting for those.
fn bsd_signals() {
    assert_eq!(Signal::SIGEMT.code(), libc::SIGEMT);
    assert_eq!(Signal::SIGINFO.code(), libc::SIGINFO);
}
