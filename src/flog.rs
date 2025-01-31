use crate::wchar::prelude::*;
use crate::wildcard::wildcard_match;
use crate::wutil::write_to_fd;
use crate::{parse_util::parse_util_unescape_wildcards, wutil::wwrite_to_fd};
use libc::c_int;
use std::sync::atomic::{AtomicI32, Ordering};

#[rustfmt::skip::macros(category)]
pub mod categories {
    use super::wstr;
    use crate::wchar::L;
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
            ($var:ident, $name:literal, $description:literal, $enabled:expr)
        ) => {
            pub static $var: category_t = category_t {
                name: L!($name),
                description: L!($description),
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
        (($var:ident, $name:literal, $description:literal, $enabled:expr)) => {
            $var
        };
        (($var:ident, $name:literal, $description:literal)) => {
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
        (error, "error", "Serious unexpected errors (on by default)", true);

        (debug, "debug", "Debugging aid (on by default)", true);

        (warning, "warning", "Warnings (on by default)", true);

        (warning_path, "warning-path", "Warnings about unusable paths for config/history (on by default)", true);

        (deprecated_test, "deprecated-test", "Warning about using test's zero- or one-argument modes (`test -d $foo`), which will be changed in future.");

        (config, "config", "Finding and reading configuration");

        (event, "event", "Firing events");

        (exec, "exec", "Errors reported by exec (on by default)", true);

        (exec_job_status, "exec-job-status", "Jobs changing status");

        (exec_job_exec, "exec-job-exec", "Jobs being executed");

        (exec_fork, "exec-fork", "Calls to fork()");

        (output_invalid, "output-invalid", "Trying to print invalid output");
        (ast_construction, "ast-construction", "Parsing fish AST");

        (proc_job_run, "proc-job-run", "Jobs getting started or continued");

        (proc_termowner, "proc-termowner", "Terminal ownership events");

        (proc_internal_proc, "proc-internal-proc", "Internal (non-forked) process events");

        (proc_reap_internal, "proc-reap-internal", "Reaping internal (non-forked) processes");

        (proc_reap_external, "proc-reap-external", "Reaping external (forked) processes");
        (proc_pgroup, "proc-pgroup", "Process groups");

        (env_locale, "env-locale", "Changes to locale variables");

        (env_export, "env-export", "Changes to exported variables");

        (env_dispatch, "env-dispatch", "Reacting to variables");

        (uvar_file, "uvar-file", "Writing/reading the universal variable store");
        (uvar_notifier, "uvar-notifier", "Notifications about universal variable changes");

        (topic_monitor, "topic-monitor", "Internal details of the topic monitor");
        (char_encoding, "char-encoding", "Character encoding issues");

        (history, "history", "Command history events");
        (history_file, "history-file", "Reading/Writing the history file", true);

        (profile_history, "profile-history", "History performance measurements");

        (iothread, "iothread", "Background IO thread events");
        (fd_monitor, "fd-monitor", "FD monitor events");

        (term_support, "term-support", "Terminal feature detection");
        (term_protocols, "term-protocols", "Terminal protocol negotiation");

        (reader, "reader", "The interactive reader/input system");
        (reader_render, "reader-render", "Rendering the command line");
        (complete, "complete", "The completion system");
        (path, "path", "Searching/using paths");

        (screen, "screen", "Screen repaints");

        (abbrs, "abbrs", "Abbreviation expansion");

        (refcell, "refcell", "Refcell dynamic borrowing");
        (autoload, "autoload", "autoloading");
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
    let _ = write_to_fd(s.as_bytes(), fd);
}

/// The entry point for flogging.
#[macro_export]
macro_rules! FLOG {
    ($category:ident, $($elem:expr),+ $(,)*) => {
        if $crate::flog::categories::$category.enabled.load(std::sync::atomic::Ordering::Relaxed) {
            #[allow(unused_imports)]
            use $crate::flog::{FloggableDisplay, FloggableDebug};
            let mut vs = vec![format!("{}:", $crate::flog::categories::$category.name)];
            $(
                {
                   vs.push($elem.to_flog_str())
                }
            )+
            // We don't use locking here so we have to append our own newline to avoid multiple writes.
            let mut v = vs.join(" ");
            v.push('\n');
            $crate::flog::flog_impl(&v);
        }
    };
}

#[macro_export]
macro_rules! FLOGF {
    ($category:ident, $fmt: expr, $($elem:expr),+ $(,)*) => {
        $crate::flog::FLOG!($category, $crate::wutil::sprintf!($fmt, $($elem),*))
    }
}

#[macro_export]
macro_rules! should_flog {
    ($category:ident) => {
        $crate::flog::categories::$category
            .enabled
            .load(std::sync::atomic::Ordering::Relaxed)
    };
}

pub use {should_flog, FLOG, FLOGF};

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

/// Set the active flog categories according to the given wildcard `wc`.
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

#[inline]
pub fn get_flog_file_fd() -> c_int {
    FLOG_FD.load(Ordering::Relaxed)
}

pub fn log_extra_to_flog_file(s: &wstr) {
    wwrite_to_fd(s, get_flog_file_fd());
}
