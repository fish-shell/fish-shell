use crate::common::ToCString;
use crate::FLOGF;
use std::env;
use std::ffi::{CStr, CString};
use std::path::PathBuf;
use std::sync::Arc;
use std::sync::Mutex;

/// The [`Term`] singleton. Initialized via a call to [`setup()`] and surfaced to the outside world via [`term()`].
///
/// We can't just use an [`AtomicPtr<Arc<Term>>`](std::sync::atomic::AtomicPtr) here because there's a race condition when the old Arc
/// gets dropped - we would obtain the current (non-null) value of `TERM` in [`term()`] but there's
/// no guarantee that a simultaneous call to [`setup()`] won't result in this refcount being
/// decremented to zero and the memory being reclaimed before we can clone it, since we can only
/// atomically *read* the value of the pointer, not clone the `Arc` it points to.
pub static TERM: Mutex<Option<Arc<Term>>> = Mutex::new(None);

/// Returns a reference to the global [`Term`] singleton or `None` if not preceded by a successful
/// call to [`terminal::setup()`](setup).
pub fn term() -> Arc<Term> {
    Arc::clone(TERM.lock().expect("Mutex poisoned!").as_ref().unwrap())
}

/// The safe wrapper around terminfo functionality, initialized by a successful call to [`setup()`]
/// and obtained thereafter by calls to [`term()`].
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
    pub set_a_foreground: Option<CString>,
    pub set_foreground: Option<CString>,
    pub set_a_background: Option<CString>,
    pub set_background: Option<CString>,
    pub exit_attribute_mode: Option<CString>,
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
}

impl Term {
    /// Initialize a new `Term` instance, prepopulating the values of all the terminfo string
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
            set_a_foreground: get_str_cap(&db, "AF"),
            set_foreground: get_str_cap(&db, "Sf"),
            set_a_background: get_str_cap(&db, "AB"),
            set_background: get_str_cap(&db, "Sb"),
            exit_attribute_mode: get_str_cap(&db, "me"),
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
        }
    }
}

/// Initializes our database from $TERM.
/// Returns a reference to the newly initialized [`Term`] singleton on success or `None` if this failed.
///
/// Any existing references from `terminal::term()` will be invalidated by this call!
pub fn setup() {
    let mut global_term = TERM.lock().expect("Mutex poisoned!");

    let res = terminfo::Database::from_env().or_else(|x| {
        // Try some more paths
        let t = if let Ok(name) = env::var("TERM") {
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
        // Create a new `Term` instance, prepopulate the capabilities we care about.
        let term = Arc::new(Term::new(result));
        *global_term = Some(term.clone());
    } else {
        *global_term = None;
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

/// Covers over tparm() with one parameter.
pub fn tparm1(cap: &CStr, param1: i32) -> Option<CString> {
    assert!(!cap.to_bytes().is_empty());
    let cap = cap.to_bytes();
    terminfo::expand!(cap; param1).ok().map(|x| x.to_cstring())
}
