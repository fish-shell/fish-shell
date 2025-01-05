//! A wrapper around the terminfo library to expose the functionality that fish needs.
//! Note that this is, on the whole, extremely little, and in practice terminfo
//! barely matters anymore. Even the few terminals in use that don't use "xterm-256color"
//! do not differ much.

use crate::common::ToCString;
use crate::FLOGF;
use std::env;
use std::ffi::{CStr, CString};
use std::path::PathBuf;
use std::sync::Arc;
use std::sync::Mutex;

/// The [`Term`] singleton. Initialized via a call to [`setup()`] and surfaced to the outside world via [`term()`].
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
#[allow(dead_code)]
#[derive(Default)]
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
    // Note key_f21 through key_f63 are available but no actual keyboard supports them.
    // key_f13 and up are also basically unused and not supported by key.rs
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
            key_f2: get_str_cap(&db, "k2"),
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

/// Initializes our database with the provided `$TERM` value `term` (or None).
/// Returns a reference to the newly initialized [`Term`] singleton on success or `None` if this failed.
///
/// The `configure` parameter may be set to a callback that takes an `&mut Term` reference to
/// override any capabilities before the `Term` is permanently made immutable.
///
/// Any existing references from `curses::term()` will be invalidated by this call!
pub fn setup<F>(term: Option<&str>, configure: F) -> Option<Arc<Term>>
where
    F: Fn(&mut Term),
{
    // For now, use the same TERM lock when using `cur_term` to prevent any race conditions in
    // curses itself. We might split this to another lock in the future.
    let mut global_term = TERM.lock().expect("Mutex poisoned!");

    let res = if let Some(term) = term {
        terminfo::Database::from_name(term)
    } else {
        // For historical reasons getting "None" means to get it from the environment.
        terminfo::Database::from_env()
    }
    .or_else(|x| {
        // Try some more paths
        let t = if let Some(term) = term {
            term.to_string()
        } else if let Ok(name) = env::var("TERM") {
            name
        } else {
            return Err(x);
        };
        let first_char = t.chars().next().unwrap().to_string();
        for dir in [
            "/run/current-system/sw/share/terminfo", // Nix
            "/usr/pkg/share/terminfo",               // NetBSD
        ] {
            let mut path = PathBuf::from(dir);
            path.push(first_char.clone());
            path.push(t.clone());
            FLOGF!(term_support, "Trying path '%ls'", path.to_str().unwrap());
            if let Ok(db) = terminfo::Database::from_path(path) {
                return Ok(db);
            }
        }
        Err(x)
    });

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

pub fn setup_fallback_term() -> Arc<Term> {
    let mut global_term = TERM.lock().expect("Mutex poisoned!");
    // These values extracted from xterm-256color from ncurses 6.4
    let term = Term {
        enter_bold_mode: Some(CString::new("\x1b[1m").unwrap()),
        enter_italics_mode: Some(CString::new("\x1b[3m").unwrap()),
        exit_italics_mode: Some(CString::new("\x1b[23m").unwrap()),
        enter_dim_mode: Some(CString::new("\x1b[2m").unwrap()),
        enter_underline_mode: Some(CString::new("\x1b[4m").unwrap()),
        exit_underline_mode: Some(CString::new("\x1b[24m").unwrap()),
        enter_reverse_mode: Some(CString::new("\x1b[7m").unwrap()),
        enter_standout_mode: Some(CString::new("\x1b[7m").unwrap()),
        exit_standout_mode: Some(CString::new("\x1b[27m").unwrap()),
        enter_blink_mode: Some(CString::new("\x1b[5m").unwrap()),
        enter_secure_mode: Some(CString::new("\x1b[8m").unwrap()),
        enter_alt_charset_mode: Some(CString::new("\x1b(0").unwrap()),
        exit_alt_charset_mode: Some(CString::new("\x1b(B").unwrap()),
        set_a_foreground: Some(
            CString::new("\x1b[%?%p1%{8}%<%t3%p1%d%e%p1%{16}%<%t9%p1%{8}%-%d%e38;5;%p1%d%;m")
                .unwrap(),
        ),
        set_a_background: Some(
            CString::new("\x1b[%?%p1%{8}%<%t4%p1%d%e%p1%{16}%<%t10%p1%{8}%-%d%e48;5;%p1%d%;m")
                .unwrap(),
        ),
        exit_attribute_mode: Some(CString::new("\x1b(B\x1b[m").unwrap()),
        clear_screen: Some(CString::new("\x1b[H\x1b[2J").unwrap()),
        cursor_up: Some(CString::new("\x1b[A").unwrap()),
        cursor_down: Some(CString::new("\n").unwrap()),
        cursor_left: Some(CString::new("\x08").unwrap()),
        cursor_right: Some(CString::new("\x1b[C").unwrap()),
        parm_left_cursor: Some(CString::new("\x1b[%p1%dD").unwrap()),
        parm_right_cursor: Some(CString::new("\x1b[%p1%dC").unwrap()),
        clr_eol: Some(CString::new("\x1b[K").unwrap()),
        clr_eos: Some(CString::new("\x1b[J").unwrap()),
        max_colors: Some(256),
        init_tabs: Some(8),
        eat_newline_glitch: true,
        auto_right_margin: true,
        key_a1: Some(CString::new("\x1bOw").unwrap()),
        key_a3: Some(CString::new("\x1bOy").unwrap()),
        key_b2: Some(CString::new("\x1bOu").unwrap()),
        key_backspace: Some(CString::new("\x7f").unwrap()),
        key_btab: Some(CString::new("\x1b[Z").unwrap()),
        key_c1: Some(CString::new("\x1bOq").unwrap()),
        key_c3: Some(CString::new("\x1bOs").unwrap()),
        key_dc: Some(CString::new("\x1b[3~").unwrap()),
        key_down: Some(CString::new("\x1bOB").unwrap()),
        key_f1: Some(CString::new("\x1bOP").unwrap()),
        key_home: Some(CString::new("\x1bOH").unwrap()),
        key_ic: Some(CString::new("\x1b[2~").unwrap()),
        key_left: Some(CString::new("\x1bOD").unwrap()),
        key_npage: Some(CString::new("\x1b[6~").unwrap()),
        key_ppage: Some(CString::new("\x1b[5~").unwrap()),
        key_right: Some(CString::new("\x1bOC").unwrap()),
        key_sdc: Some(CString::new("\x1b[3;2~").unwrap()),
        key_send: Some(CString::new("\x1b[1;2F").unwrap()),
        key_sf: Some(CString::new("\x1b[1;2B").unwrap()),
        key_shome: Some(CString::new("\x1b[1;2H").unwrap()),
        key_sic: Some(CString::new("\x1b[2;2~").unwrap()),
        key_sleft: Some(CString::new("\x1b[1;2D").unwrap()),
        key_snext: Some(CString::new("\x1b[6;2~").unwrap()),
        key_sprevious: Some(CString::new("\x1b[5;2~").unwrap()),
        key_sr: Some(CString::new("\x1b[1;2A").unwrap()),
        key_sright: Some(CString::new("\x1b[1;2C").unwrap()),
        key_up: Some(CString::new("\x1bOA").unwrap()),
        ..Default::default()
    };
    let term = Arc::new(term);
    *global_term = Some(term.clone());
    term
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

/// Covers over tparm() with one parameter.
pub fn tparm1(cap: &CStr, param1: i32) -> Option<CString> {
    assert!(!cap.to_bytes().is_empty());
    let cap = cap.to_bytes();
    terminfo::expand!(cap; param1).ok().map(|x| x.to_cstring())
}
