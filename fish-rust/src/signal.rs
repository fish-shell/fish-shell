use std::borrow::Cow;
use std::num::NonZeroI32;

use crate::ffi;
use crate::topic_monitor::{generation_t, invalid_generations, topic_monitor_principal, topic_t};
use crate::wchar::wstr;
use crate::wchar_ffi::c_str;
use widestring::U32CStr;

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

#[derive(Clone, Copy, Eq, PartialEq, PartialOrd, Ord)]
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
    #[cfg(any(target_os = "freebsd", target_os = "macos"))]
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
    #[cfg(any(target_os = "freebsd", target_os = "macos"))]
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
    const UNKNOWN_SIG_NAME: &'static str = "SIG???";

    /// Creates a new `Signal` to represent the passed system signal code `sig`.
    /// Panics if `sig` is zero.
    pub const fn new(sig: i32) -> Self {
        match NonZeroI32::new(sig) {
            None => panic!("Invalid zero signal value!"),
            Some(result) => Signal(result),
        }
    }

    pub const fn name(&self) -> &'static str {
        match *self {
            Signal::SIGHUP => "SIGHUP",
            Signal::SIGINT => "SIGINT",
            Signal::SIGQUIT => "SIGQUIT",
            Signal::SIGILL => "SIGILL",
            Signal::SIGTRAP => "SIGTRAP",
            Signal::SIGABRT => "SIGABRT",
            #[cfg(any(target_os = "freebsd", target_os = "macos"))]
            Signal::SIGEMT => "SIGEMT",
            Signal::SIGFPE => "SIGFPE",
            Signal::SIGKILL => "SIGKILL",
            Signal::SIGBUS => "SIGBUS",
            Signal::SIGSEGV => "SIGSEGV",
            Signal::SIGSYS => "SIGSYS",
            Signal::SIGPIPE => "SIGPIPE",
            Signal::SIGALRM => "SIGALRM",
            Signal::SIGTERM => "SIGTERM",
            Signal::SIGURG => "SIGURG",
            Signal::SIGSTOP => "SIGSTOP",
            Signal::SIGTSTP => "SIGTSTP",
            Signal::SIGCONT => "SIGCONT",
            Signal::SIGCHLD => "SIGCHLD",
            Signal::SIGTTIN => "SIGTTIN",
            Signal::SIGTTOU => "SIGTTOU",
            Signal::SIGIO => "SIGIO",
            Signal::SIGXCPU => "SIGXCPU",
            Signal::SIGXFSZ => "SIGXFSZ",
            Signal::SIGVTALRM => "SIGVTALRM",
            Signal::SIGPROF => "SIGPROF",
            Signal::SIGWINCH => "SIGWINCH",
            #[cfg(any(target_os = "freebsd", target_os = "macos"))]
            Signal::SIGINFO => "SIGINFO",
            Signal::SIGUSR1 => "SIGUSR1",
            Signal::SIGUSR2 => "SIGUSR2",
            #[cfg(any(target_os = "freebsd"))]
            Signal::SIGTHR => "SIGTHR",
            #[cfg(any(target_os = "freebsd"))]
            Signal::SIGLIBRT => "SIGLIBRT",
            #[cfg(target_os = "linux")]
            Signal::SIGSTKFLT => "SIGSTKFLT",
            #[cfg(target_os = "linux")]
            Signal::SIGPWR => "SIGPWR",
            Signal(_) => Self::UNKNOWN_SIG_NAME,
        }
    }

    pub fn code(&self) -> i32 {
        self.0.into()
    }

    pub const fn desc(&self) -> &'static str {
        match *self {
            Signal::SIGHUP => "Terminal hung up",
            Signal::SIGINT => "Quit request from job control (^C)",
            Signal::SIGQUIT => "Quit request from job control with core dump (^\\)",
            Signal::SIGILL => "Illegal instruction",
            Signal::SIGTRAP => "Trace or breakpoint trap",
            Signal::SIGABRT => "Abort",
            #[cfg(any(target_os = "freebsd", target_os = "macos"))]
            Signal::SIGEMT => "Emulator trap",
            Signal::SIGFPE => "Floating point exception",
            Signal::SIGKILL => "Forced quit",
            Signal::SIGBUS => "Misaligned address error",
            Signal::SIGSEGV => "Address boundary error",
            Signal::SIGSYS => "Bad system call",
            Signal::SIGPIPE => "Broken pipe",
            Signal::SIGALRM => "Timer expired",
            Signal::SIGTERM => "Polite quit request",
            Signal::SIGURG => "Urgent socket condition",
            Signal::SIGSTOP => "Forced stop",
            Signal::SIGTSTP => "Stop request from job control (^Z)",
            Signal::SIGCONT => "Continue previously stopped process",
            Signal::SIGCHLD => "Child process status changed",
            Signal::SIGTTIN => "Stop from terminal input",
            Signal::SIGTTOU => "Stop from terminal output",
            Signal::SIGIO => "I/O on asynchronous file descriptior is possible",
            Signal::SIGXCPU => "CPU time limit exceeded",
            Signal::SIGXFSZ => "File size limit exceeded",
            Signal::SIGVTALRM => "Virtual timer expired",
            Signal::SIGPROF => "Profiling timer expired",
            Signal::SIGWINCH => "Window size change",
            #[cfg(any(target_os = "freebsd", target_os = "macos"))]
            Signal::SIGINFO => "Information request",
            Signal::SIGUSR1 => "User-defined signal 1",
            Signal::SIGUSR2 => "User-defined signal 2",
            #[cfg(any(target_os = "freebsd"))]
            Signal::SIGTHR => "Thread interrupt",
            #[cfg(any(target_os = "freebsd"))]
            Signal::SIGLIBRT => "Real-time library interrupt",
            #[cfg(target_os = "linux")]
            Signal::SIGSTKFLT => "Stack fault",
            #[cfg(target_os = "linux")]
            Signal::SIGPWR => "Power failure",
            Signal(_) => "Unknown",
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
            #[cfg(any(target_os = "freebsd", target_os = "macos"))]
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
            #[cfg(any(target_os = "freebsd", target_os = "macos"))]
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

impl std::fmt::Debug for Signal {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_fmt(format_args!("{}({})", self.name(), self.code()))
    }
}

impl std::fmt::Display for Signal {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let name = self.name();
        if name != Self::UNKNOWN_SIG_NAME {
            f.write_str(name)
        } else {
            f.write_fmt(format_args!("Unrecognized Signal {}", self.code()))
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
        value.code() as usize
    }
}

impl From<Signal> for NonZeroI32 {
    fn from(value: Signal) -> Self {
        value.0
    }
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
