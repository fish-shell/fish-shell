// Support for exposing the terminal size.
use crate::common::assert_sync;
use crate::env::{EnvMode, EnvStackRefFFI, EnvVar, Environment};
use crate::ffi::{parser_t, Repin};
use crate::flog::FLOG;
use crate::wchar::prelude::*;
use crate::wchar_ffi::WCharToFFI;
use crate::wutil::fish_wcstoi;
use std::sync::atomic::{AtomicBool, AtomicU32, Ordering};
use std::sync::Mutex;

#[cxx::bridge]
mod termsize_ffi {
    #[cxx_name = "termsize_t"]
    #[derive(Copy, Clone, Debug, PartialEq, Eq)]
    pub struct Termsize {
        /// Width of the terminal, in columns.
        // TODO: Change to u32
        pub width: isize,

        /// Height of the terminal, in rows.
        // TODO: Change to u32
        pub height: isize,
    }

    extern "Rust" {
        pub fn termsize_default() -> Termsize;
        pub fn termsize_last() -> Termsize;
        pub fn termsize_initialize_ffi(vars: *const u8) -> Termsize;
        pub fn termsize_invalidate_tty();
        pub fn termsize_update_ffi(parser: *mut u8) -> Termsize;
        pub fn termsize_handle_winch();
    }
}
pub use termsize_ffi::Termsize;

// A counter which is incremented every SIGWINCH, or when the tty is otherwise invalidated.
static TTY_TERMSIZE_GEN_COUNT: AtomicU32 = AtomicU32::new(0);

/// Convert an environment variable to an int, or return a default value.
/// The int must be >0 and <USHRT_MAX (from struct winsize).
fn var_to_int_or(var: Option<EnvVar>, default: isize) -> isize {
    let val: WString = var.map(|v| v.as_string()).unwrap_or_default();
    if !val.is_empty() {
        if let Ok(proposed) = fish_wcstoi(&val) {
            if proposed > 0 && proposed <= u16::MAX as i32 {
                return proposed as isize;
            }
        }
    }
    default
}

/// \return a termsize from ioctl, or None on error or if not supported.
fn read_termsize_from_tty() -> Option<Termsize> {
    let mut ret: Option<Termsize> = None;
    // Note: historically we've supported libc::winsize not existing.
    let mut winsize: libc::winsize = unsafe { std::mem::zeroed() };
    if unsafe { libc::ioctl(0, libc::TIOCGWINSZ, &mut winsize as *mut libc::winsize) } >= 0 {
        // 0 values are unusable, fall back to the default instead.
        if winsize.ws_col == 0 {
            FLOG!(
                term_support,
                L!("Terminal has 0 columns, falling back to default width")
            );
            winsize.ws_col = Termsize::DEFAULT_WIDTH as u16;
        }
        if winsize.ws_row == 0 {
            FLOG!(
                term_support,
                L!("Terminal has 0 rows, falling back to default height")
            );
            winsize.ws_row = Termsize::DEFAULT_HEIGHT as u16;
        }
        ret = Some(Termsize::new(
            winsize.ws_col as isize,
            winsize.ws_row as isize,
        ));
    }
    ret
}

impl Termsize {
    /// Default width and height.
    pub const DEFAULT_WIDTH: isize = 80;
    pub const DEFAULT_HEIGHT: isize = 24;

    /// Construct from width and height.
    pub fn new(width: isize, height: isize) -> Self {
        Self { width, height }
    }

    /// Return a default-sized termsize.
    pub fn defaults() -> Self {
        Self::new(Self::DEFAULT_WIDTH, Self::DEFAULT_HEIGHT)
    }
}

struct TermsizeData {
    // The last termsize returned by TIOCGWINSZ, or none if none.
    last_from_tty: Option<Termsize>,
    // The last termsize seen from the environment (COLUMNS/LINES), or none if none.
    last_from_env: Option<Termsize>,
    // The last-seen tty-invalidation generation count.
    // Set to a huge value so it's initially stale.
    last_tty_gen_count: u32,
}

impl TermsizeData {
    const fn defaults() -> Self {
        Self {
            last_from_tty: None,
            last_from_env: None,
            last_tty_gen_count: u32::max_value(),
        }
    }

    /// \return the current termsize from this data.
    fn current(&self) -> Termsize {
        // This encapsulates our ordering logic. If we have a termsize from a tty, use it; otherwise use
        // what we have seen from the environment.
        self.last_from_tty
            .or(self.last_from_env)
            .unwrap_or_else(Termsize::defaults)
    }

    /// Mark that our termsize is (for the time being) from the environment, not the tty.
    fn mark_override_from_env(&mut self, ts: Termsize) {
        self.last_from_env = Some(ts);
        self.last_from_tty = None;
        self.last_tty_gen_count = TTY_TERMSIZE_GEN_COUNT.load(Ordering::Relaxed);
    }
}

/// Termsize monitoring is more complicated than one may think.
/// The main source of complexity is the interaction between the environment variables COLUMNS/ROWS,
/// the WINCH signal, and the TIOCGWINSZ ioctl.
/// Our policy is "last seen wins": if COLUMNS or LINES is modified, we respect that until we get a
/// SIGWINCH.
pub struct TermsizeContainer {
    // Our lock-protected data.
    data: Mutex<TermsizeData>,

    // An indication that we are currently in the process of setting COLUMNS and LINES, and so do
    // not react to any changes.
    setting_env_vars: AtomicBool,

    /// A function used for accessing the termsize from the tty. This is only exposed for testing.
    tty_size_reader: fn() -> Option<Termsize>,
}

impl TermsizeContainer {
    /// \return the termsize without applying any updates.
    /// Return the default termsize if none.
    pub fn last(&self) -> Termsize {
        self.data.lock().unwrap().current()
    }

    /// Initialize our termsize, using the given environment stack.
    /// This will prefer to use COLUMNS and LINES, but will fall back to the tty size reader.
    /// This does not change any variables in the environment.
    pub fn initialize(&self, vars: &dyn Environment) -> Termsize {
        let new_termsize = Termsize {
            width: var_to_int_or(vars.getf(L!("COLUMNS"), EnvMode::GLOBAL), -1),
            height: var_to_int_or(vars.getf(L!("LINES"), EnvMode::GLOBAL), -1),
        };

        let mut data = self.data.lock().unwrap();
        if new_termsize.width > 0 && new_termsize.height > 0 {
            data.mark_override_from_env(new_termsize);
        } else {
            data.last_tty_gen_count = TTY_TERMSIZE_GEN_COUNT.load(Ordering::Relaxed);
            data.last_from_tty = (self.tty_size_reader)();
        }
        data.current()
    }

    /// If our termsize is stale, update it, using \p parser to fire any events that may be
    /// registered for COLUMNS and LINES.
    /// This requires a shared reference so it can work from a static.
    /// \return the updated termsize.
    pub fn updating(&self, parser: &mut parser_t) -> Termsize {
        let new_size;
        let prev_size;

        // Take the lock in a local region.
        // Capture the size before and the new size.
        {
            let mut data = self.data.lock().unwrap();
            prev_size = data.current();

            // Critical read of signal-owned variable.
            // This must happen before the TIOCGWINSZ ioctl.
            let tty_gen_count: u32 = TTY_TERMSIZE_GEN_COUNT.load(Ordering::Relaxed);
            if data.last_tty_gen_count != tty_gen_count {
                // Our idea of the size of the terminal may be stale.
                // Apply any updates.
                data.last_tty_gen_count = tty_gen_count;
                data.last_from_tty = (self.tty_size_reader)();
            }
            new_size = data.current();
        }

        // Announce any updates.
        if new_size != prev_size {
            self.set_columns_lines_vars(new_size, parser);
        }
        new_size
    }

    fn set_columns_lines_vars(&self, val: Termsize, parser: &mut parser_t) {
        let saved = self.setting_env_vars.swap(true, Ordering::Relaxed);
        parser.pin().set_var_and_fire(
            &L!("COLUMNS").to_ffi(),
            EnvMode::GLOBAL.bits(),
            val.width.to_wstring().to_ffi(),
        );
        parser.pin().set_var_and_fire(
            &L!("LINES").to_ffi(),
            EnvMode::GLOBAL.bits(),
            val.height.to_wstring().to_ffi(),
        );
        self.setting_env_vars.store(saved, Ordering::Relaxed);
    }

    /// Note that COLUMNS and/or LINES global variables changed.
    fn handle_columns_lines_var_change(&self, vars: &dyn Environment) {
        // Do nothing if we are the ones setting it.
        if self.setting_env_vars.load(Ordering::Relaxed) {
            return;
        }
        // Construct a new termsize from COLUMNS and LINES, then set it in our data.
        let new_termsize = Termsize {
            width: vars
                .getf(L!("COLUMNS"), EnvMode::GLOBAL)
                .map(|v| v.as_string())
                .and_then(|v| fish_wcstoi(&v).ok().map(|h| h as isize))
                .unwrap_or(Termsize::DEFAULT_WIDTH),
            height: vars
                .getf(L!("LINES"), EnvMode::GLOBAL)
                .map(|v| v.as_string())
                .and_then(|v| fish_wcstoi(&v).ok().map(|h| h as isize))
                .unwrap_or(Termsize::DEFAULT_HEIGHT),
        };

        // Store our termsize as an environment override.
        self.data
            .lock()
            .unwrap()
            .mark_override_from_env(new_termsize);
    }

    /// Note that COLUMNS and/or LINES global variables changed.
    fn handle_columns_lines_var_change_ffi(&self, vars: &dyn Environment) {
        // Do nothing if we are the ones setting it.
        if self.setting_env_vars.load(Ordering::Relaxed) {
            return;
        }
        // Construct a new termsize from COLUMNS and LINES, then set it in our data.
        let new_termsize = Termsize {
            width: var_to_int_or(
                vars.getf(L!("COLUMNS"), EnvMode::GLOBAL),
                Termsize::DEFAULT_WIDTH,
            ),
            height: var_to_int_or(
                vars.getf(L!("LINES"), EnvMode::GLOBAL),
                Termsize::DEFAULT_HEIGHT,
            ),
        };

        // Store our termsize as an environment override.
        self.data
            .lock()
            .unwrap()
            .mark_override_from_env(new_termsize);
    }

    /// Note that a WINCH signal is received.
    /// Naturally this may be called from within a signal handler.
    pub fn handle_winch() {
        TTY_TERMSIZE_GEN_COUNT.fetch_add(1, Ordering::Relaxed);
    }

    pub fn invalidate_tty() {
        TTY_TERMSIZE_GEN_COUNT.fetch_add(1, Ordering::Relaxed);
    }
}

pub static SHARED_CONTAINER: TermsizeContainer = TermsizeContainer {
    data: Mutex::new(TermsizeData::defaults()),
    setting_env_vars: AtomicBool::new(false),
    tty_size_reader: read_termsize_from_tty,
};

const _: () = assert_sync::<TermsizeContainer>();

/// Helper to return the default termsize.
pub fn termsize_default() -> Termsize {
    Termsize::defaults()
}

/// Convenience helper to return the last known termsize.
pub fn termsize_last() -> Termsize {
    return SHARED_CONTAINER.last();
}

/// Called when the COLUMNS or LINES variables are changed.
pub fn handle_columns_lines_var_change(vars: &dyn Environment) {
    SHARED_CONTAINER.handle_columns_lines_var_change(vars);
}

/// Called to initialize the termsize.
/// The pointer is to a Box<EnvStackRefFFI>, but has the wrong type to satisfy cxx.
#[allow(clippy::borrowed_box)]
pub fn termsize_initialize_ffi(vars_ptr: *const u8) -> Termsize {
    assert!(!vars_ptr.is_null());
    let vars: &Box<EnvStackRefFFI> = unsafe { &*(vars_ptr.cast()) };
    SHARED_CONTAINER.initialize(&*vars.0)
}

/// Called to update termsize.
pub fn termsize_update_ffi(parser_ptr: *mut u8) -> Termsize {
    assert!(!parser_ptr.is_null());
    let parser: &mut parser_t = unsafe { &mut *(parser_ptr as *mut parser_t) };
    SHARED_CONTAINER.updating(parser)
}

/// FFI bridge for WINCH.
pub fn termsize_handle_winch() {
    TermsizeContainer::handle_winch();
}

pub fn termsize_invalidate_tty() {
    TermsizeContainer::invalidate_tty();
}

use crate::ffi_tests::add_test;
add_test!("test_termsize", || {
    let env_global = EnvMode::GLOBAL;
    let parser: &mut parser_t = unsafe { &mut *parser_t::principal_parser_ffi() };
    let vars = parser.get_vars();

    // Use a static variable so we can pretend we're the kernel exposing a terminal size.
    static STUBBY_TERMSIZE: Mutex<Option<Termsize>> = Mutex::new(None);
    fn stubby_termsize() -> Option<Termsize> {
        *STUBBY_TERMSIZE.lock().unwrap()
    }
    let ts = TermsizeContainer {
        data: Mutex::new(TermsizeData::defaults()),
        setting_env_vars: AtomicBool::new(false),
        tty_size_reader: stubby_termsize,
    };

    // Initially default value.
    assert_eq!(ts.last(), Termsize::defaults());

    // Haha we change the value, it doesn't even know.
    *STUBBY_TERMSIZE.lock().unwrap() = Some(Termsize {
        width: 42,
        height: 84,
    });
    assert_eq!(ts.last(), Termsize::defaults());

    // Ok let's tell it. But it still doesn't update right away.
    TermsizeContainer::handle_winch();
    assert_eq!(ts.last(), Termsize::defaults());

    // Ok now we tell it to update.
    ts.updating(parser);
    assert_eq!(ts.last(), Termsize::new(42, 84));
    assert_eq!(vars.get(L!("COLUMNS")).unwrap().as_string(), "42");
    assert_eq!(vars.get(L!("LINES")).unwrap().as_string(), "84");

    // Wow someone set COLUMNS and LINES to a weird value.
    // Now the tty's termsize doesn't matter.
    vars.set_one(L!("COLUMNS"), env_global, L!("75").to_owned());
    vars.set_one(L!("LINES"), env_global, L!("150").to_owned());
    ts.handle_columns_lines_var_change(&*parser.get_vars());
    assert_eq!(ts.last(), Termsize::new(75, 150));
    assert_eq!(vars.get(L!("COLUMNS")).unwrap().as_string(), "75");
    assert_eq!(vars.get(L!("LINES")).unwrap().as_string(), "150");

    vars.set_one(L!("COLUMNS"), env_global, L!("33").to_owned());
    ts.handle_columns_lines_var_change(&*parser.get_vars());
    assert_eq!(ts.last(), Termsize::new(33, 150));

    // Oh it got SIGWINCH, now the tty matters again.
    TermsizeContainer::handle_winch();
    assert_eq!(ts.last(), Termsize::new(33, 150));
    assert_eq!(ts.updating(parser), stubby_termsize().unwrap());
    assert_eq!(vars.get(L!("COLUMNS")).unwrap().as_string(), "42");
    assert_eq!(vars.get(L!("LINES")).unwrap().as_string(), "84");

    // Test initialize().
    vars.set_one(L!("COLUMNS"), env_global, L!("83").to_owned());
    vars.set_one(L!("LINES"), env_global, L!("38").to_owned());
    ts.initialize(&*vars);
    assert_eq!(ts.last(), Termsize::new(83, 38));

    // initialize() even beats the tty reader until a sigwinch.
    let ts2 = TermsizeContainer {
        data: Mutex::new(TermsizeData::defaults()),
        setting_env_vars: AtomicBool::new(false),
        tty_size_reader: stubby_termsize,
    };
    ts.initialize(&*parser.get_vars());
    ts2.updating(parser);
    assert_eq!(ts.last(), Termsize::new(83, 38));
    TermsizeContainer::handle_winch();
    assert_eq!(ts2.updating(parser), stubby_termsize().unwrap());
});
