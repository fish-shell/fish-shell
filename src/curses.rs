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

    /// tputs callback argument type and the callback type itself.
    /// N.B. The C++ had a check for TPUTS_USES_INT_ARG for the first parameter of tputs
    /// which was to support OpenIndiana, which used a char.
    pub type tputs_arg = libc::c_int;
    pub type putc_t = extern "C" fn(tputs_arg) -> libc::c_int;

    extern "C" {
        #[cfg(not(have_nc_cur_term))]
        pub static mut cur_term: *const core::ffi::c_void;
        #[cfg(have_nc_cur_term)]
        pub fn have_nc_cur_term() -> *const core::ffi::c_void;

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

        /// Sets the `cur_term` TERMINAL  pointer.
        pub fn set_curterm(term: *const core::ffi::c_void) -> *const core::ffi::c_void;

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

/// The ncurses `cur_term` TERMINAL pointer.
fn get_curterm() -> *const core::ffi::c_void {
    #[cfg(have_nc_cur_term)]
    unsafe {
        sys::have_nc_cur_term()
    }
    #[cfg(not(have_nc_cur_term))]
    unsafe {
        sys::cur_term
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
    pub exit_standout_mode: Option<CString>,
    pub enter_blink_mode: Option<CString>,
    pub enter_protected_mode: Option<CString>,
    pub enter_shadow_mode: Option<CString>,
    pub exit_shadow_mode: Option<CString>,
    pub enter_secure_mode: Option<CString>,
    pub enter_alt_charset_mode: Option<CString>,
    pub exit_alt_charset_mode: Option<CString>,
    pub set_a_foreground: Option<CString>,
    pub set_foreground: Option<CString>,
    pub set_a_background: Option<CString>,
    pub set_background: Option<CString>,
    pub exit_attribute_mode: Option<CString>,
    pub set_title: Option<CString>,
    pub clear_screen: Option<CString>,
    pub cursor_up: Option<CString>,
    pub cursor_down: Option<CString>,
    pub cursor_left: Option<CString>,
    pub cursor_right: Option<CString>,
    pub parm_left_cursor: Option<CString>,
    pub parm_right_cursor: Option<CString>,
    pub clr_eol: Option<CString>,
    pub clr_eos: Option<CString>,

    // Number capabilities
    pub max_colors: Option<usize>,
    pub init_tabs: Option<usize>,

    // Flag/boolean capabilities
    pub eat_newline_glitch: bool,
    pub auto_right_margin: bool,

    // Keys.
    pub key_a1: Option<CString>,
    pub key_a3: Option<CString>,
    pub key_b2: Option<CString>,
    pub key_backspace: Option<CString>,
    pub key_beg: Option<CString>,
    pub key_btab: Option<CString>,
    pub key_c1: Option<CString>,
    pub key_c3: Option<CString>,
    pub key_cancel: Option<CString>,
    pub key_catab: Option<CString>,
    pub key_clear: Option<CString>,
    pub key_close: Option<CString>,
    pub key_command: Option<CString>,
    pub key_copy: Option<CString>,
    pub key_create: Option<CString>,
    pub key_ctab: Option<CString>,
    pub key_dc: Option<CString>,
    pub key_dl: Option<CString>,
    pub key_down: Option<CString>,
    pub key_eic: Option<CString>,
    pub key_end: Option<CString>,
    pub key_enter: Option<CString>,
    pub key_eol: Option<CString>,
    pub key_eos: Option<CString>,
    pub key_exit: Option<CString>,
    pub key_f0: Option<CString>,
    pub key_f1: Option<CString>,
    pub key_f2: Option<CString>,
    pub key_f3: Option<CString>,
    pub key_f4: Option<CString>,
    pub key_f5: Option<CString>,
    pub key_f6: Option<CString>,
    pub key_f7: Option<CString>,
    pub key_f8: Option<CString>,
    pub key_f9: Option<CString>,
    pub key_f10: Option<CString>,
    pub key_f11: Option<CString>,
    pub key_f12: Option<CString>,
    pub key_f13: Option<CString>,
    pub key_f14: Option<CString>,
    pub key_f15: Option<CString>,
    pub key_f16: Option<CString>,
    pub key_f17: Option<CString>,
    pub key_f18: Option<CString>,
    pub key_f19: Option<CString>,
    pub key_f20: Option<CString>,
    // Note key_f21 through key_f63 are available but no actual keyboard supports them.
    pub key_find: Option<CString>,
    pub key_help: Option<CString>,
    pub key_home: Option<CString>,
    pub key_ic: Option<CString>,
    pub key_il: Option<CString>,
    pub key_left: Option<CString>,
    pub key_ll: Option<CString>,
    pub key_mark: Option<CString>,
    pub key_message: Option<CString>,
    pub key_move: Option<CString>,
    pub key_next: Option<CString>,
    pub key_npage: Option<CString>,
    pub key_open: Option<CString>,
    pub key_options: Option<CString>,
    pub key_ppage: Option<CString>,
    pub key_previous: Option<CString>,
    pub key_print: Option<CString>,
    pub key_redo: Option<CString>,
    pub key_reference: Option<CString>,
    pub key_refresh: Option<CString>,
    pub key_replace: Option<CString>,
    pub key_restart: Option<CString>,
    pub key_resume: Option<CString>,
    pub key_right: Option<CString>,
    pub key_save: Option<CString>,
    pub key_sbeg: Option<CString>,
    pub key_scancel: Option<CString>,
    pub key_scommand: Option<CString>,
    pub key_scopy: Option<CString>,
    pub key_screate: Option<CString>,
    pub key_sdc: Option<CString>,
    pub key_sdl: Option<CString>,
    pub key_select: Option<CString>,
    pub key_send: Option<CString>,
    pub key_seol: Option<CString>,
    pub key_sexit: Option<CString>,
    pub key_sf: Option<CString>,
    pub key_sfind: Option<CString>,
    pub key_shelp: Option<CString>,
    pub key_shome: Option<CString>,
    pub key_sic: Option<CString>,
    pub key_sleft: Option<CString>,
    pub key_smessage: Option<CString>,
    pub key_smove: Option<CString>,
    pub key_snext: Option<CString>,
    pub key_soptions: Option<CString>,
    pub key_sprevious: Option<CString>,
    pub key_sprint: Option<CString>,
    pub key_sr: Option<CString>,
    pub key_sredo: Option<CString>,
    pub key_sreplace: Option<CString>,
    pub key_sright: Option<CString>,
    pub key_srsume: Option<CString>,
    pub key_ssave: Option<CString>,
    pub key_ssuspend: Option<CString>,
    pub key_stab: Option<CString>,
    pub key_sundo: Option<CString>,
    pub key_suspend: Option<CString>,
    pub key_undo: Option<CString>,
    pub key_up: Option<CString>,
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
            exit_standout_mode: get_str_cap("se"),
            enter_blink_mode: get_str_cap("mb"),
            enter_protected_mode: get_str_cap("mp"),
            enter_shadow_mode: get_str_cap("ZM"),
            exit_shadow_mode: get_str_cap("ZU"),
            enter_secure_mode: get_str_cap("mk"),
            enter_alt_charset_mode: get_str_cap("as"),
            exit_alt_charset_mode: get_str_cap("ae"),
            set_a_foreground: get_str_cap("AF"),
            set_foreground: get_str_cap("Sf"),
            set_a_background: get_str_cap("AB"),
            set_background: get_str_cap("Sb"),
            exit_attribute_mode: get_str_cap("me"),
            set_title: get_str_cap("ts"),
            clear_screen: get_str_cap("cl"),
            cursor_up: get_str_cap("up"),
            cursor_down: get_str_cap("do"),
            cursor_left: get_str_cap("le"),
            cursor_right: get_str_cap("nd"),
            parm_left_cursor: get_str_cap("LE"),
            parm_right_cursor: get_str_cap("RI"),
            clr_eol: get_str_cap("ce"),
            clr_eos: get_str_cap("cd"),

            // Number capabilities
            max_colors: get_num_cap("Co"),
            init_tabs: get_num_cap("it"),

            // Flag/boolean capabilities
            eat_newline_glitch: get_flag_cap("xn"),
            auto_right_margin: get_flag_cap("am"),

            // Keys. See `man terminfo` for these strings.
            key_a1: get_str_cap("K1"),
            key_a3: get_str_cap("K3"),
            key_b2: get_str_cap("K2"),
            key_backspace: get_str_cap("kb"),
            key_beg: get_str_cap("@1"),
            key_btab: get_str_cap("kB"),
            key_c1: get_str_cap("K4"),
            key_c3: get_str_cap("K5"),
            key_cancel: get_str_cap("@2"),
            key_catab: get_str_cap("ka"),
            key_clear: get_str_cap("kC"),
            key_close: get_str_cap("@3"),
            key_command: get_str_cap("@4"),
            key_copy: get_str_cap("@5"),
            key_create: get_str_cap("@6"),
            key_ctab: get_str_cap("kt"),
            key_dc: get_str_cap("kD"),
            key_dl: get_str_cap("kL"),
            key_down: get_str_cap("kd"),
            key_eic: get_str_cap("kM"),
            key_end: get_str_cap("@7"),
            key_enter: get_str_cap("@8"),
            key_eol: get_str_cap("kE"),
            key_eos: get_str_cap("kS"),
            key_exit: get_str_cap("@9"),
            key_f0: get_str_cap("k0"),
            key_f1: get_str_cap("k1"),
            key_f10: get_str_cap("k;"),
            key_f11: get_str_cap("F1"),
            key_f12: get_str_cap("F2"),
            key_f13: get_str_cap("F3"),
            key_f14: get_str_cap("F4"),
            key_f15: get_str_cap("F5"),
            key_f16: get_str_cap("F6"),
            key_f17: get_str_cap("F7"),
            key_f18: get_str_cap("F8"),
            key_f19: get_str_cap("F9"),
            key_f2: get_str_cap("k2"),
            key_f20: get_str_cap("FA"),
            // Note key_f21 through key_f63 are available but no actual keyboard supports them.
            key_f3: get_str_cap("k3"),
            key_f4: get_str_cap("k4"),
            key_f5: get_str_cap("k5"),
            key_f6: get_str_cap("k6"),
            key_f7: get_str_cap("k7"),
            key_f8: get_str_cap("k8"),
            key_f9: get_str_cap("k9"),
            key_find: get_str_cap("@0"),
            key_help: get_str_cap("%1"),
            key_home: get_str_cap("kh"),
            key_ic: get_str_cap("kI"),
            key_il: get_str_cap("kA"),
            key_left: get_str_cap("kl"),
            key_ll: get_str_cap("kH"),
            key_mark: get_str_cap("%2"),
            key_message: get_str_cap("%3"),
            key_move: get_str_cap("%4"),
            key_next: get_str_cap("%5"),
            key_npage: get_str_cap("kN"),
            key_open: get_str_cap("%6"),
            key_options: get_str_cap("%7"),
            key_ppage: get_str_cap("kP"),
            key_previous: get_str_cap("%8"),
            key_print: get_str_cap("%9"),
            key_redo: get_str_cap("%0"),
            key_reference: get_str_cap("&1"),
            key_refresh: get_str_cap("&2"),
            key_replace: get_str_cap("&3"),
            key_restart: get_str_cap("&4"),
            key_resume: get_str_cap("&5"),
            key_right: get_str_cap("kr"),
            key_save: get_str_cap("&6"),
            key_sbeg: get_str_cap("&9"),
            key_scancel: get_str_cap("&0"),
            key_scommand: get_str_cap("*1"),
            key_scopy: get_str_cap("*2"),
            key_screate: get_str_cap("*3"),
            key_sdc: get_str_cap("*4"),
            key_sdl: get_str_cap("*5"),
            key_select: get_str_cap("*6"),
            key_send: get_str_cap("*7"),
            key_seol: get_str_cap("*8"),
            key_sexit: get_str_cap("*9"),
            key_sf: get_str_cap("kF"),
            key_sfind: get_str_cap("*0"),
            key_shelp: get_str_cap("#1"),
            key_shome: get_str_cap("#2"),
            key_sic: get_str_cap("#3"),
            key_sleft: get_str_cap("#4"),
            key_smessage: get_str_cap("%a"),
            key_smove: get_str_cap("%b"),
            key_snext: get_str_cap("%c"),
            key_soptions: get_str_cap("%d"),
            key_sprevious: get_str_cap("%e"),
            key_sprint: get_str_cap("%f"),
            key_sr: get_str_cap("kR"),
            key_sredo: get_str_cap("%g"),
            key_sreplace: get_str_cap("%h"),
            key_sright: get_str_cap("%i"),
            key_srsume: get_str_cap("%j"),
            key_ssave: get_str_cap("!1"),
            key_ssuspend: get_str_cap("!2"),
            key_stab: get_str_cap("kT"),
            key_sundo: get_str_cap("!3"),
            key_suspend: get_str_cap("&7"),
            key_undo: get_str_cap("&8"),
            key_up: get_str_cap("ku"),
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
        let _ = sys::del_curterm(get_curterm());

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
            let _ = sys::del_curterm(get_curterm());
            let _ = sys::set_curterm(core::ptr::null());
        }
        *term = None;
    }
}

/// Return a nonempty String capability from termcap, or None if missing or empty.
/// Panics if the given code string does not contain exactly two bytes.
fn get_str_cap(code: &str) -> Option<CString> {
    let code = to_cstr_code(code);
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
fn get_num_cap(code: &str) -> Option<usize> {
    let code = to_cstr_code(code);
    match unsafe { sys::tgetnum(code.as_ptr()) } {
        -1 => None,
        n => Some(usize::try_from(n).unwrap()),
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
pub fn tparm0(cap: &CStr) -> Option<CString> {
    // Take the lock because tparm races with del_curterm, etc.
    let _term: std::sync::MutexGuard<Option<Arc<Term>>> = TERM.lock().unwrap();
    assert!(!cap.to_bytes().is_empty());
    let cap_ptr = cap.as_ptr() as *mut libc::c_char;
    // Safety: we're trusting tparm here.
    unsafe { try_ptr_to_cstr(tparm(cap_ptr)) }
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
