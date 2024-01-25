//! A wrapper around the system's curses/ncurses library, exposing some lower-level functionality
//! that's not directly exposed in any of the popular ncurses crates.
//!
//! In addition to exposing the C library ffi calls, we also shim around some functionality that's
//! only made available via the the ncurses headers to C code via macro magic, such as polyfilling
//! missing capability strings to shoe-in missing support for certain terminal sequences.
//!
//! This is intentionally very bare bones and only implements the subset of curses functionality
//! used by fish

use crate::common::ToCString;
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
    fn new(db: terminfo::Database) -> Self {
        Term {
            // String capabilities
            enter_bold_mode: get_str_cap(&db, "md"),
            enter_italics_mode: get_str_cap(&db, "ZH"),
            exit_italics_mode: get_str_cap(&db, "ZR"),
            enter_dim_mode: get_str_cap(&db, "mh"),
            enter_underline_mode: get_str_cap(&db, "us"),
            exit_underline_mode: get_str_cap(&db, "ue"),
            enter_reverse_mode: get_str_cap(&db, "mr"),
            enter_standout_mode: get_str_cap(&db, "so"),
            exit_standout_mode: get_str_cap(&db, "se"),
            enter_blink_mode: get_str_cap(&db, "mb"),
            enter_protected_mode: get_str_cap(&db, "mp"),
            enter_shadow_mode: get_str_cap(&db, "ZM"),
            exit_shadow_mode: get_str_cap(&db, "ZU"),
            enter_secure_mode: get_str_cap(&db, "mk"),
            enter_alt_charset_mode: get_str_cap(&db, "as"),
            exit_alt_charset_mode: get_str_cap(&db, "ae"),
            set_a_foreground: get_str_cap(&db, "AF"),
            set_foreground: get_str_cap(&db, "Sf"),
            set_a_background: get_str_cap(&db, "AB"),
            set_background: get_str_cap(&db, "Sb"),
            exit_attribute_mode: get_str_cap(&db, "me"),
            set_title: get_str_cap(&db, "ts"),
            clear_screen: get_str_cap(&db, "cl"),
            cursor_up: get_str_cap(&db, "up"),
            cursor_down: get_str_cap(&db, "do"),
            cursor_left: get_str_cap(&db, "le"),
            cursor_right: get_str_cap(&db, "nd"),
            parm_left_cursor: get_str_cap(&db, "LE"),
            parm_right_cursor: get_str_cap(&db, "RI"),
            clr_eol: get_str_cap(&db, "ce"),
            clr_eos: get_str_cap(&db, "cd"),

            // Number capabilities
            max_colors: get_num_cap(&db, "Co"),
            init_tabs: get_num_cap(&db, "it"),

            // Flag/boolean capabilities
            eat_newline_glitch: get_flag_cap(&db, "xn"),
            auto_right_margin: get_flag_cap(&db, "am"),

            // Keys. See `man terminfo` for these strings.
            key_a1: get_str_cap(&db, "K1"),
            key_a3: get_str_cap(&db, "K3"),
            key_b2: get_str_cap(&db, "K2"),
            key_backspace: get_str_cap(&db, "kb"),
            key_beg: get_str_cap(&db, "@1"),
            key_btab: get_str_cap(&db, "kB"),
            key_c1: get_str_cap(&db, "K4"),
            key_c3: get_str_cap(&db, "K5"),
            key_cancel: get_str_cap(&db, "@2"),
            key_catab: get_str_cap(&db, "ka"),
            key_clear: get_str_cap(&db, "kC"),
            key_close: get_str_cap(&db, "@3"),
            key_command: get_str_cap(&db, "@4"),
            key_copy: get_str_cap(&db, "@5"),
            key_create: get_str_cap(&db, "@6"),
            key_ctab: get_str_cap(&db, "kt"),
            key_dc: get_str_cap(&db, "kD"),
            key_dl: get_str_cap(&db, "kL"),
            key_down: get_str_cap(&db, "kd"),
            key_eic: get_str_cap(&db, "kM"),
            key_end: get_str_cap(&db, "@7"),
            key_enter: get_str_cap(&db, "@8"),
            key_eol: get_str_cap(&db, "kE"),
            key_eos: get_str_cap(&db, "kS"),
            key_exit: get_str_cap(&db, "@9"),
            key_f0: get_str_cap(&db, "k0"),
            key_f1: get_str_cap(&db, "k1"),
            key_f10: get_str_cap(&db, "k;"),
            key_f11: get_str_cap(&db, "F1"),
            key_f12: get_str_cap(&db, "F2"),
            key_f13: get_str_cap(&db, "F3"),
            key_f14: get_str_cap(&db, "F4"),
            key_f15: get_str_cap(&db, "F5"),
            key_f16: get_str_cap(&db, "F6"),
            key_f17: get_str_cap(&db, "F7"),
            key_f18: get_str_cap(&db, "F8"),
            key_f19: get_str_cap(&db, "F9"),
            key_f2: get_str_cap(&db, "k2"),
            key_f20: get_str_cap(&db, "FA"),
            // Note key_f21 through key_f63 are available but no actual keyboard supports them.
            key_f3: get_str_cap(&db, "k3"),
            key_f4: get_str_cap(&db, "k4"),
            key_f5: get_str_cap(&db, "k5"),
            key_f6: get_str_cap(&db, "k6"),
            key_f7: get_str_cap(&db, "k7"),
            key_f8: get_str_cap(&db, "k8"),
            key_f9: get_str_cap(&db, "k9"),
            key_find: get_str_cap(&db, "@0"),
            key_help: get_str_cap(&db, "%1"),
            key_home: get_str_cap(&db, "kh"),
            key_ic: get_str_cap(&db, "kI"),
            key_il: get_str_cap(&db, "kA"),
            key_left: get_str_cap(&db, "kl"),
            key_ll: get_str_cap(&db, "kH"),
            key_mark: get_str_cap(&db, "%2"),
            key_message: get_str_cap(&db, "%3"),
            key_move: get_str_cap(&db, "%4"),
            key_next: get_str_cap(&db, "%5"),
            key_npage: get_str_cap(&db, "kN"),
            key_open: get_str_cap(&db, "%6"),
            key_options: get_str_cap(&db, "%7"),
            key_ppage: get_str_cap(&db, "kP"),
            key_previous: get_str_cap(&db, "%8"),
            key_print: get_str_cap(&db, "%9"),
            key_redo: get_str_cap(&db, "%0"),
            key_reference: get_str_cap(&db, "&1"),
            key_refresh: get_str_cap(&db, "&2"),
            key_replace: get_str_cap(&db, "&3"),
            key_restart: get_str_cap(&db, "&4"),
            key_resume: get_str_cap(&db, "&5"),
            key_right: get_str_cap(&db, "kr"),
            key_save: get_str_cap(&db, "&6"),
            key_sbeg: get_str_cap(&db, "&9"),
            key_scancel: get_str_cap(&db, "&0"),
            key_scommand: get_str_cap(&db, "*1"),
            key_scopy: get_str_cap(&db, "*2"),
            key_screate: get_str_cap(&db, "*3"),
            key_sdc: get_str_cap(&db, "*4"),
            key_sdl: get_str_cap(&db, "*5"),
            key_select: get_str_cap(&db, "*6"),
            key_send: get_str_cap(&db, "*7"),
            key_seol: get_str_cap(&db, "*8"),
            key_sexit: get_str_cap(&db, "*9"),
            key_sf: get_str_cap(&db, "kF"),
            key_sfind: get_str_cap(&db, "*0"),
            key_shelp: get_str_cap(&db, "#1"),
            key_shome: get_str_cap(&db, "#2"),
            key_sic: get_str_cap(&db, "#3"),
            key_sleft: get_str_cap(&db, "#4"),
            key_smessage: get_str_cap(&db, "%a"),
            key_smove: get_str_cap(&db, "%b"),
            key_snext: get_str_cap(&db, "%c"),
            key_soptions: get_str_cap(&db, "%d"),
            key_sprevious: get_str_cap(&db, "%e"),
            key_sprint: get_str_cap(&db, "%f"),
            key_sr: get_str_cap(&db, "kR"),
            key_sredo: get_str_cap(&db, "%g"),
            key_sreplace: get_str_cap(&db, "%h"),
            key_sright: get_str_cap(&db, "%i"),
            key_srsume: get_str_cap(&db, "%j"),
            key_ssave: get_str_cap(&db, "!1"),
            key_ssuspend: get_str_cap(&db, "!2"),
            key_stab: get_str_cap(&db, "kT"),
            key_sundo: get_str_cap(&db, "!3"),
            key_suspend: get_str_cap(&db, "&7"),
            key_undo: get_str_cap(&db, "&8"),
            key_up: get_str_cap(&db, "ku"),
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
pub fn setup<F>(term: Option<&CStr>, configure: F) -> Option<Arc<Term>>
where
    F: Fn(&mut Term),
{
    // For now, use the same TERM lock when using `cur_term` to prevent any race conditions in
    // curses itself. We might split this to another lock in the future.
    let mut global_term = TERM.lock().expect("Mutex poisoned!");

    let res = if let Some(term) = term {
        terminfo::Database::from_name(term.to_str().unwrap())
    } else {
        // For historical reasons getting "None" means to get it from the environment.
        terminfo::Database::from_env()
    };

    // Safely store the new Term instance or replace the old one. We have the lock so it's safe to
    // drop the old TERM value and have its refcount decremented - no one will be cloning it.
    if let Ok(result) = res {
        // Create a new `Term` instance, prepopulate the capabilities we care about, and allow the
        // caller to override any as needed.
        let mut term = Term::new(result);
        (configure)(&mut term);

        let term = Arc::new(term);
        *global_term = Some(term.clone());
        Some(term)
    } else {
        *global_term = None;
        None
    }
}

/// Return a nonempty String capability from termcap, or None if missing or empty.
/// Panics if the given code string does not contain exactly two bytes.
fn get_str_cap(db: &terminfo::Database, code: &str) -> Option<CString> {
    db.raw(code).map(|cap| match cap {
        terminfo::Value::True => "1".to_string().as_bytes().to_cstring(),
        terminfo::Value::Number(n) => n.to_string().as_bytes().to_cstring(),
        terminfo::Value::String(s) => s.clone().to_cstring(),
    })
}

/// Return a number capability from termcap, or None if missing.
/// Panics if the given code string does not contain exactly two bytes.
fn get_num_cap(db: &terminfo::Database, code: &str) -> Option<usize> {
    match db.raw(code) {
        Some(terminfo::Value::Number(n)) if *n >= 0 => Some(*n as usize),
        _ => None,
    }
}

/// Return a flag capability from termcap, or false if missing.
/// Panics if the given code string does not contain exactly two bytes.
fn get_flag_cap(db: &terminfo::Database, code: &str) -> bool {
    db.raw(code)
        .map(|cap| matches!(cap, terminfo::Value::True))
        .unwrap_or(false)
}

/// Covers over tparm().
pub fn tparm0(cap: &CStr) -> Option<CString> {
    assert!(!cap.to_bytes().is_empty());
    let cap = cap.to_bytes();
    terminfo::expand!(cap).ok().map(|x| x.to_cstring())
}

/// Covers over tparm().
pub fn tparm1(cap: &CStr, param1: i32) -> Option<CString> {
    assert!(!cap.to_bytes().is_empty());
    let cap = cap.to_bytes();
    terminfo::expand!(cap; param1).ok().map(|x| x.to_cstring())
}
