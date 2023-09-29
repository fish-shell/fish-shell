//! A wrapper around the system's curses/ncurses library, exposing some lower-level functionality
//! that's not directly exposed in any of the popular ncurses crates.
//!
//! In addition to exposing the C library ffi calls, we also shim around some functionality that's
//! only made available via the the ncurses headers to C code via macro magic, such as polyfilling
//! missing capability strings to shoe-in missing support for certain terminal sequences.
//!
//! This is intentionally very bare bones and only implements the subset of curses functionality
//! used by fish

use self::sys::*;
use std::ffi::{CStr, CString};
use std::sync::Arc;
use std::sync::Mutex;

/// The [`Term`] singleton, providing a façade around the system curses library. Initialized via a
/// successful call to [`setup()`] and surfaced to the outside world via [`term()`].
///
/// It isn't guaranteed that fish will ever be able to successfully call `setup()`, so this must
/// remain an `Option` instead of returning `Term` by default and just panicking if [`term()`] was
/// called before `setup()`.
///
/// We can't just use an AtomicPtr<Arc<Term>> here because there's a race condition when the old Arc
/// gets dropped - we would obtain the current (non-null) value of `TERM` in [`term()`] but there's
/// no guarantee that a simultaneous call to [`setup()`] won't result in this refcount being
/// decremented to zero and the memory being reclaimed before we can clone it, since we can only
/// atomically *read* the value of the pointer, not clone the `Arc` it points to.
pub static TERM: Mutex<Option<Arc<Term>>> = Mutex::new(None);

/// Returns a reference to the global [`Term`] singleton or `None` if not preceded by a successful
/// call to [`curses::setup()`].
pub fn term() -> Option<Arc<Term>> {
    TERM.lock()
        .expect("Mutex poisoned!")
        .as_ref()
        .map(Arc::clone)
}

/// Convert a nul-terminated pointer, which must not be itself null, to a CString.
fn ptr_to_cstr(ptr: *const libc::c_char) -> CString {
    assert!(!ptr.is_null());
    unsafe { CStr::from_ptr(ptr).to_owned() }
}

/// Convert a nul-terminated pointer to a CString, or None if the pointer is null.
fn try_ptr_to_cstr(ptr: *const libc::c_char) -> Option<CString> {
    if ptr.is_null() {
        None
    } else {
        Some(ptr_to_cstr(ptr))
    }
}

/// Private module exposing system curses ffi.
mod sys {
    pub const OK: i32 = 0;
    pub const ERR: i32 = -1;

    /// tputs callback argument type and the callback type itself.
    /// N.B. The C++ had a check for TPUTS_USES_INT_ARG for the first parameter of tputs
    /// which was to support OpenIndiana, which used a char.
    pub type tputs_arg = libc::c_int;
    pub type putc_t = extern "C" fn(tputs_arg) -> libc::c_int;

    extern "C" {
        /// The ncurses `cur_term` TERMINAL pointer.
        pub static mut cur_term: *const core::ffi::c_void;

        /// setupterm(3) is a low-level call to begin doing any sort of `term.h`/`curses.h` work.
        /// It's called internally by ncurses's `initscr()` and `newterm()`, but the C++ code called
        /// it directly from [`initialize_curses_using_fallbacks()`].
        pub fn setupterm(
            term: *const libc::c_char,
            filedes: libc::c_int,
            errret: *mut libc::c_int,
        ) -> libc::c_int;

        /// Frees the `cur_term` TERMINAL  pointer.
        pub fn del_curterm(term: *const core::ffi::c_void) -> libc::c_int;

        /// Checks for the presence of a termcap flag identified by the first two characters of
        /// `id`.
        pub fn tgetflag(id: *const libc::c_char) -> libc::c_int;

        /// Checks for the presence and value of a number capability in the termcap/termconf
        /// database. A return value of `-1` indicates not found.
        pub fn tgetnum(id: *const libc::c_char) -> libc::c_int;

        pub fn tgetstr(
            id: *const libc::c_char,
            area: *mut *mut libc::c_char,
        ) -> *const libc::c_char;

        pub fn tparm(str: *const libc::c_char, ...) -> *const libc::c_char;

        pub fn tputs(str: *const libc::c_char, affcnt: libc::c_int, putc: putc_t) -> libc::c_int;
    }
}
pub use sys::tputs_arg as TputsArg;

/// The safe wrapper around curses functionality, initialized by a successful call to [`setup()`]
/// and obtained thereafter by calls to [`term()`].
///
/// An extant `Term` instance means the curses `TERMINAL *cur_term` pointer is non-null. Any
/// functionality that is normally performed using `cur_term` should be done via `Term` instead.
pub struct Term {
    // String capabilities. Any Some value is confirmed non-empty.
    pub enter_bold_mode: Option<CString>,
    pub enter_italics_mode: Option<CString>,
    pub exit_italics_mode: Option<CString>,
    pub enter_dim_mode: Option<CString>,
    pub enter_underline_mode: Option<CString>,
    pub exit_underline_mode: Option<CString>,
    pub enter_reverse_mode: Option<CString>,
    pub enter_standout_mode: Option<CString>,
    pub set_a_foreground: Option<CString>,
    pub set_foreground: Option<CString>,
    pub set_a_background: Option<CString>,
    pub set_background: Option<CString>,
    pub exit_attribute_mode: Option<CString>,
    pub set_title: Option<CString>,

    // Number capabilities
    pub max_colors: Option<i32>,

    // Flag/boolean capabilities
    pub eat_newline_glitch: bool,
}

impl Term {
    /// Initialize a new `Term` instance, prepopulating the values of all the curses string
    /// capabilities we care about in the process.
    fn new() -> Self {
        Term {
            // String capabilities
            enter_bold_mode: get_str_cap("md"),
            enter_italics_mode: get_str_cap("ZH"),
            exit_italics_mode: get_str_cap("ZR"),
            enter_dim_mode: get_str_cap("mh"),
            enter_underline_mode: get_str_cap("us"),
            exit_underline_mode: get_str_cap("ue"),
            enter_reverse_mode: get_str_cap("mr"),
            enter_standout_mode: get_str_cap("so"),
            set_a_foreground: get_str_cap("AF"),
            set_foreground: get_str_cap("Sf"),
            set_a_background: get_str_cap("AB"),
            set_background: get_str_cap("Sb"),
            exit_attribute_mode: get_str_cap("me"),
            set_title: get_str_cap("ts"),

            // Number capabilities
            max_colors: get_num_cap("Co"),

            // Flag/boolean capabilities
            eat_newline_glitch: get_flag_cap("xn"),
        }
    }
}

/// Calls the curses `setupterm()` function with the provided `$TERM` value `term` (or a null
/// pointer in case `term` is null) for the file descriptor `fd`. Returns a reference to the newly
/// initialized [`Term`] singleton on success or `None` if this failed.
///
/// The `configure` parameter may be set to a callback that takes an `&mut Term` reference to
/// override any capabilities before the `Term` is permanently made immutable.
///
/// Note that the `errret` parameter is provided to the function, meaning curses will not write
/// error output to stderr in case of failure.
///
/// Any existing references from `curses::term()` will be invalidated by this call!
pub fn setup<F>(term: Option<&CStr>, fd: i32, configure: F) -> Option<Arc<Term>>
where
    F: Fn(&mut Term),
{
    // For now, use the same TERM lock when using `cur_term` to prevent any race conditions in
    // curses itself. We might split this to another lock in the future.
    let mut global_term = TERM.lock().expect("Mutex poisoned!");

    let result = unsafe {
        // If cur_term is already initialized for a different $TERM value, calling setupterm() again
        // will leak memory. Call del_curterm() first to free previously allocated resources.
        let _ = sys::del_curterm(cur_term);

        let mut err = 0;
        if let Some(term) = term {
            sys::setupterm(term.as_ptr(), fd, &mut err)
        } else {
            sys::setupterm(core::ptr::null(), fd, &mut err)
        }
    };

    // Safely store the new Term instance or replace the old one. We have the lock so it's safe to
    // drop the old TERM value and have its refcount decremented - no one will be cloning it.
    if result == sys::OK {
        // Create a new `Term` instance, prepopulate the capabilities we care about, and allow the
        // caller to override any as needed.
        let mut term = Term::new();
        (configure)(&mut term);

        let term = Arc::new(term);
        *global_term = Some(term.clone());
        Some(term)
    } else {
        *global_term = None;
        None
    }
}

/// Resets the curses `cur_term` TERMINAL pointer. Subsequent calls to [`curses::term()`](term())
/// will return `None`.
pub fn reset() {
    let mut term = TERM.lock().expect("Mutex poisoned!");
    if term.is_some() {
        unsafe {
            // Ignore the result of del_curterm() as the only documented error is that
            // `cur_term` was already null.
            let _ = sys::del_curterm(cur_term);
            sys::cur_term = core::ptr::null();
        }
        *term = None;
    }
}

/// Return a nonempty String capability from termcap, or None if missing or empty.
/// Panics if the given code string does not contain exactly two bytes.
fn get_str_cap(code: &str) -> Option<CString> {
    let code = to_cstr_code(code);
    const NULL: *const i8 = core::ptr::null();
    // termcap spec says nul is not allowed in terminal sequences and must be encoded;
    // so the terminating NUL is the end of the string.
    let tstr = unsafe { sys::tgetstr(code.as_ptr(), core::ptr::null_mut()) };
    let result = try_ptr_to_cstr(tstr);
    // Paranoia: do not return empty strings.
    if let Some(s) = &result {
        if s.as_bytes().is_empty() {
            return None;
        }
    }
    result
}

/// Return a number capability from termcap, or None if missing.
/// Panics if the given code string does not contain exactly two bytes.
fn get_num_cap(code: &str) -> Option<i32> {
    let code = to_cstr_code(code);
    match unsafe { sys::tgetnum(code.as_ptr()) } {
        -1 => None,
        n => Some(n),
    }
}

/// Return a flag capability from termcap, or false if missing.
/// Panics if the given code string does not contain exactly two bytes.
fn get_flag_cap(code: &str) -> bool {
    let code = to_cstr_code(code);
    unsafe { sys::tgetflag(code.as_ptr()) != 0 }
}

/// `code` is the two-digit termcap code. See termcap(5) for a reference.
/// Panics if anything other than a two-ascii-character `code` is passed into the function.
const fn to_cstr_code(code: &str) -> [libc::c_char; 3] {
    use libc::c_char;
    let code = code.as_bytes();
    if code.len() != 2 {
        panic!("Invalid termcap code provided");
    }
    [code[0] as c_char, code[1] as c_char, b'\0' as c_char]
}

/// Covers over tparm().
pub fn tparm1(cap: &CStr, param1: i32) -> Option<CString> {
    // Take the lock because tparm races with del_curterm, etc.
    let _term: std::sync::MutexGuard<Option<Arc<Term>>> = TERM.lock().unwrap();
    assert!(!cap.to_bytes().is_empty());
    let cap_ptr = cap.as_ptr() as *mut libc::c_char;
    // Safety: we're trusting tparm here.
    unsafe { try_ptr_to_cstr(tparm(cap_ptr, param1 as libc::c_int)) }
}

/// Wrapper over tputs.
/// The caller is responsible for synchronization.
pub fn tputs(str: &CStr, affcnt: libc::c_int, putc: sys::putc_t) -> libc::c_int {
    let str_ptr = str.as_ptr() as *mut libc::c_char;
    // Safety: we're trusting tputs here.
    unsafe { sys::tputs(str_ptr, affcnt, putc) }
}
