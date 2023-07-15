use crate::parse_util::parse_util_unescape_wildcards;
use crate::wchar::prelude::*;
use crate::wildcard::wildcard_match;
use libc::c_int;
use std::io::Write;
use std::os::unix::prelude::*;
use std::sync::atomic::{AtomicI32, Ordering};

#[rustfmt::skip::macros(category)]
#[widestrs]
pub mod categories {
    use super::wstr;
    use std::sync::atomic::AtomicBool;

    pub struct category_t {
        pub name: &'static wstr,
        pub description: &'static wstr,
        pub enabled: AtomicBool,
    }

    /// Macro to declare a static variable identified by $var,
    /// with the given name and description, and optionally enabled by default.
    macro_rules! declare_category {
        (
            ($var:ident, $name:expr, $description:expr, $enabled:expr)
        ) => {
            pub static $var: category_t = category_t {
                name: $name,
                description: $description,
                enabled: AtomicBool::new($enabled),
            };
        };
        (
            ($var:ident, $name:expr, $description:expr)
        ) => {
            declare_category!(($var, $name, $description, false));
        };
    }

    /// Macro to extract the variable name for a category.
    macro_rules! category_name {
        (($var:ident, $name:expr, $description:expr, $enabled:expr)) => {
            $var
        };
        (($var:ident, $name:expr, $description:expr)) => {
            $var
        };
    }

    macro_rules! categories {
        (
            // A repetition of categories, separated by semicolons.
            $($cats:tt);*

            // Allow trailing semicolon.
            $(;)?
        ) => {
            // Declare each category.
            $(
                declare_category!($cats);
            )*

            // Define a function which gives you a Vector of all categories.
            pub fn all_categories() -> Vec<&'static category_t> {
                vec![
                    $(
                        & category_name!($cats),
                    )*
                ]
            }
        };
    }

    categories!(
        (error, "error"L, "Serious unexpected errors (on by default)"L, true);

        (debug, "debug"L, "Debugging aid (on by default)"L, true);

        (warning, "warning"L, "Warnings (on by default)"L, true);

        (warning_path, "warning-path"L, "Warnings about unusable paths for config/history (on by default)"L, true);

        (config, "config"L, "Finding and reading configuration"L);

        (event, "event"L, "Firing events"L);

        (exec, "exec"L, "Errors reported by exec (on by default)"L, true);

        (exec_job_status, "exec-job-status"L, "Jobs changing status"L);

        (exec_job_exec, "exec-job-exec"L, "Jobs being executed"L);

        (exec_fork, "exec-fork"L, "Calls to fork()"L);

        (output_invalid, "output-invalid"L, "Trying to print invalid output"L);
        (ast_construction, "ast-construction"L, "Parsing fish AST"L);

        (proc_job_run, "proc-job-run"L, "Jobs getting started or continued"L);

        (proc_termowner, "proc-termowner"L, "Terminal ownership events"L);

        (proc_internal_proc, "proc-internal-proc"L, "Internal (non-forked) process events"L);

        (proc_reap_internal, "proc-reap-internal"L, "Reaping internal (non-forked) processes"L);

        (proc_reap_external, "proc-reap-external"L, "Reaping external (forked) processes"L);
        (proc_pgroup, "proc-pgroup"L, "Process groups"L);

        (env_locale, "env-locale"L, "Changes to locale variables"L);

        (env_export, "env-export"L, "Changes to exported variables"L);

        (env_dispatch, "env-dispatch"L, "Reacting to variables"L);

        (uvar_file, "uvar-file"L, "Writing/reading the universal variable store"L);
        (uvar_notifier, "uvar-notifier"L, "Notifications about universal variable changes"L);

        (topic_monitor, "topic-monitor"L, "Internal details of the topic monitor"L);
        (char_encoding, "char-encoding"L, "Character encoding issues"L);

        (history, "history"L, "Command history events"L);
        (history_file, "history-file"L, "Reading/Writing the history file"L);

        (profile_history, "profile-history"L, "History performance measurements"L);

        (iothread, "iothread"L, "Background IO thread events"L);
        (fd_monitor, "fd-monitor"L, "FD monitor events"L);

        (term_support, "term-support"L, "Terminal feature detection"L);

        (reader, "reader"L, "The interactive reader/input system"L);
        (reader_render, "reader-render"L, "Rendering the command line"L);
        (complete, "complete"L, "The completion system"L);
        (path, "path"L, "Searching/using paths"L);

        (screen, "screen"L, "Screen repaints"L);
    );
}

/// FLOG formats values. By default we would like to use Display, and fall back to Debug.
/// However that would require specialization. So instead we make two "separate" traits, bring them both in scope,
/// and let Rust figure it out.
/// Clients can opt a Debug type into Floggable by implementing FloggableDebug:
///    impl FloggableDebug for MyType {}
pub trait FloggableDisplay {
    /// Return a string representation of this thing.
    fn to_flog_str(&self) -> String;
}

impl<T: std::fmt::Display> FloggableDisplay for T {
    fn to_flog_str(&self) -> String {
        self.to_string()
    }
}

pub trait FloggableDebug: std::fmt::Debug {
    fn to_flog_str(&self) -> String {
        format!("{:?}", self)
    }
}

/// Write to our FLOG file.
pub fn flog_impl(s: &str) {
    let fd = get_flog_file_fd();
    if fd < 0 {
        return;
    }
    let mut file = unsafe { std::fs::File::from_raw_fd(fd) };
    let _ = file.write(s.as_bytes());
    // Ensure the file is not closed.
    file.into_raw_fd();
}

macro_rules! FLOG {
    ($category:ident, $($elem:expr),+ $(,)*) => {
        if crate::flog::categories::$category.enabled.load(std::sync::atomic::Ordering::Relaxed) {
            #[allow(unused_imports)]
            use crate::flog::{FloggableDisplay, FloggableDebug};
            let mut vs = vec![format!("{}:", crate::flog::categories::$category.name)];
            $(
                {
                   vs.push($elem.to_flog_str())
                }
            )+
            // We don't use locking here so we have to append our own newline to avoid multiple writes.
            let mut v = vs.join(" ");
            v.push('\n');
            crate::flog::flog_impl(&v);
        }
    };
}

macro_rules! FLOGF {
    ($category:ident, $fmt: expr, $($elem:expr),+ $(,)*) => {
        crate::flog::FLOG!($category, sprintf!($fmt, $($elem),*));
    }
}

macro_rules! should_flog {
    ($category:ident) => {
        crate::flog::categories::$category
            .enabled
            .load(std::sync::atomic::Ordering::Relaxed)
    };
}

pub(crate) use {should_flog, FLOG, FLOGF};

/// For each category, if its name matches the wildcard, set its enabled to the given sense.
fn apply_one_wildcard(wc_esc: &wstr, sense: bool) {
    let wc = parse_util_unescape_wildcards(wc_esc);
    let mut match_found = false;
    for cat in categories::all_categories() {
        if wildcard_match(cat.name, &wc, false) {
            cat.enabled.store(sense, Ordering::Relaxed);
            match_found = true;
        }
    }
    if !match_found {
        eprintln!("Failed to match debug category: {wc_esc}");
    }
}

/// Set the active flog categories according to the given wildcard \p wc.
pub fn activate_flog_categories_by_pattern(wc_ptr: &wstr) {
    let mut wc: WString = wc_ptr.into();
    // Normalize underscores to dashes, allowing the user to be sloppy.
    for c in wc.as_char_slice_mut() {
        if *c == '_' {
            *c = '-';
        }
    }
    for s in wc.split(',') {
        if s.starts_with('-') {
            apply_one_wildcard(s.slice_from(1), false);
        } else {
            apply_one_wildcard(s, true);
        }
    }
}

/// The flog output fd. Defaults to stderr. A value < 0 disables flog.
static FLOG_FD: AtomicI32 = AtomicI32::new(libc::STDERR_FILENO);

pub fn set_flog_file_fd(fd: c_int) {
    FLOG_FD.store(fd, Ordering::Relaxed);
}

pub fn get_flog_file_fd() -> c_int {
    FLOG_FD.load(Ordering::Relaxed)
}
