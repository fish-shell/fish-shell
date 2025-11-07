// Support for exposing the terminal size.
use crate::common::assert_sync;
use crate::env::{EnvMode, EnvVar, Environment};
use crate::flog::FLOG;
use crate::parser::Parser;
use crate::wchar::prelude::*;
use crate::wutil::fish_wcstoi;
use std::mem::MaybeUninit;
use std::num::NonZeroU16;
use std::sync::Mutex;
use std::sync::atomic::{AtomicBool, AtomicU32, Ordering};

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct Termsize {
    /// Width of the terminal, in columns.
    width: NonZeroU16,

    /// Height of the terminal, in rows.
    height: NonZeroU16,
}

// A counter which is incremented every SIGWINCH, or when the tty is otherwise invalidated.
static TTY_TERMSIZE_GEN_COUNT: AtomicU32 = AtomicU32::new(0);

/// Convert an environment variable to an int.
/// The int must be >0 and <USHRT_MAX (from struct winsize).
fn var_to_int(var: Option<EnvVar>) -> Option<NonZeroU16> {
    var.and_then(|v| fish_wcstoi(&v.as_string()).ok())
        .and_then(|i| u16::try_from(i).ok())
        .and_then(NonZeroU16::new)
}

/// Return a termsize from ioctl, or None on error or if not supported.
fn read_termsize_from_tty() -> Option<Termsize> {
    // Note: historically we've supported libc::winsize not existing.
    let winsize = {
        let mut winsize = MaybeUninit::<libc::winsize>::uninit();
        if unsafe { libc::ioctl(0, libc::TIOCGWINSZ, winsize.as_mut_ptr()) } < 0 {
            return None;
        }
        unsafe { winsize.assume_init() }
    };
    let width = NonZeroU16::new(winsize.ws_col).unwrap_or_else(|| {
        FLOG!(
            term_support,
            L!("Terminal has 0 columns, falling back to default width")
        );
        Termsize::DEFAULT_WIDTH
    });
    let height = NonZeroU16::new(winsize.ws_row).unwrap_or_else(|| {
        FLOG!(
            term_support,
            L!("Terminal has 0 rows, falling back to default height")
        );
        Termsize::DEFAULT_HEIGHT
    });
    Some(Termsize::new(width, height))
}

impl Termsize {
    /// Default width and height.
    pub const DEFAULT_WIDTH: NonZeroU16 = NonZeroU16::new(80).unwrap();
    pub const DEFAULT_HEIGHT: NonZeroU16 = NonZeroU16::new(24).unwrap();

    /// Construct from width and height.
    pub fn new(width: NonZeroU16, height: NonZeroU16) -> Self {
        Self { width, height }
    }

    pub fn width_u16(&self) -> NonZeroU16 {
        self.width
    }
    pub fn height_u16(&self) -> NonZeroU16 {
        self.height
    }

    pub fn width(&self) -> usize {
        usize::from(self.width.get())
    }
    pub fn height(&self) -> usize {
        usize::from(self.height.get())
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
    pub(crate) const fn defaults() -> Self {
        Self {
            last_from_tty: None,
            last_from_env: None,
            last_tty_gen_count: u32::MAX,
        }
    }

    /// Return the current termsize from this data.
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

    /// A function used for accessing the termsize from the tty.
    tty_size_reader: fn() -> Option<Termsize>,
}

impl TermsizeContainer {
    /// Return the termsize without applying any updates.
    /// Return the default termsize if none.
    pub fn last(&self) -> Termsize {
        self.data.lock().unwrap().current()
    }

    /// Initialize our termsize, using the given environment stack.
    /// This will prefer to use COLUMNS and LINES, but will fall back to the tty size reader.
    /// This does not change any variables in the environment.
    pub fn initialize(&self, vars: &dyn Environment) -> Termsize {
        let width = var_to_int(vars.getf(L!("COLUMNS"), EnvMode::GLOBAL));
        let height = var_to_int(vars.getf(L!("LINES"), EnvMode::GLOBAL));
        let mut data = self.data.lock().unwrap();
        if let (Some(width), Some(height)) = (width, height) {
            data.mark_override_from_env(Termsize { width, height });
        } else {
            data.last_tty_gen_count = TTY_TERMSIZE_GEN_COUNT.load(Ordering::Relaxed);
            data.last_from_tty = (self.tty_size_reader)();
        }
        data.current()
    }

    /// If our termsize is stale, update it, using `parser` to fire any events that may be
    /// registered for COLUMNS and LINES.
    /// This requires a shared reference so it can work from a static.
    /// Return the updated termsize.
    fn updating(&self, parser: &Parser) -> Termsize {
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

    fn set_columns_lines_vars(&self, val: Termsize, parser: &Parser) {
        let saved = self.setting_env_vars.swap(true, Ordering::Relaxed);
        parser.set_var_and_fire(
            L!("COLUMNS"),
            EnvMode::GLOBAL,
            vec![val.width().to_wstring()],
        );
        parser.set_var_and_fire(
            L!("LINES"),
            EnvMode::GLOBAL,
            vec![val.height().to_wstring()],
        );
        self.setting_env_vars.store(saved, Ordering::Relaxed);
    }

    /// Note that COLUMNS and/or LINES global variables changed.
    pub(crate) fn handle_columns_lines_var_change(&self, vars: &dyn Environment) {
        // Do nothing if we are the ones setting it.
        if self.setting_env_vars.load(Ordering::Relaxed) {
            return;
        }
        // Construct a new termsize from COLUMNS and LINES, then set it in our data.
        let new_termsize = Termsize {
            width: var_to_int(vars.getf(L!("COLUMNS"), EnvMode::GLOBAL))
                .unwrap_or(Termsize::DEFAULT_WIDTH),
            height: var_to_int(vars.getf(L!("LINES"), EnvMode::GLOBAL))
                .unwrap_or(Termsize::DEFAULT_HEIGHT),
        };

        // Store our termsize as an environment override.
        self.data
            .lock()
            .unwrap()
            .mark_override_from_env(new_termsize);
    }
}

pub static SHARED_CONTAINER: TermsizeContainer = TermsizeContainer {
    data: Mutex::new(TermsizeData::defaults()),
    setting_env_vars: AtomicBool::new(false),
    tty_size_reader: read_termsize_from_tty,
};

const _: () = assert_sync::<TermsizeContainer>();

/// Convenience helper to return the last known termsize.
pub fn termsize_last() -> Termsize {
    SHARED_CONTAINER.last()
}

/// Called when the COLUMNS or LINES variables are changed.
pub fn handle_columns_lines_var_change(vars: &dyn Environment) {
    SHARED_CONTAINER.handle_columns_lines_var_change(vars);
}

pub fn termsize_update(parser: &Parser) -> Termsize {
    SHARED_CONTAINER.updating(parser)
}

/// May be called form a signal handler (WINCH).
pub fn safe_termsize_invalidate_tty() {
    TTY_TERMSIZE_GEN_COUNT.fetch_add(1, Ordering::Relaxed);
}

#[cfg(test)]
mod tests {
    use crate::env::{EnvMode, Environment};
    use crate::termsize::*;
    use crate::tests::prelude::*;
    use std::sync::Mutex;
    use std::sync::atomic::AtomicBool;

    #[test]
    #[serial]
    fn test_termsize() {
        let _cleanup = test_init();
        let env_global = EnvMode::GLOBAL;
        let parser = TestParser::new();
        let vars = parser.vars();

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
            width: NonZeroU16::new(42).unwrap(),
            height: NonZeroU16::new(84).unwrap(),
        });
        assert_eq!(ts.last(), Termsize::defaults());

        // Ok let's tell it. But it still doesn't update right away.
        let handle_winch = safe_termsize_invalidate_tty;
        handle_winch();
        assert_eq!(ts.last(), Termsize::defaults());

        let new_test_termsize = |width, height| {
            Termsize::new(
                NonZeroU16::new(width).unwrap(),
                NonZeroU16::new(height).unwrap(),
            )
        };

        // Ok now we tell it to update.
        ts.updating(&parser);
        assert_eq!(ts.last(), new_test_termsize(42, 84));
        assert_eq!(vars.get(L!("COLUMNS")).unwrap().as_string(), "42");
        assert_eq!(vars.get(L!("LINES")).unwrap().as_string(), "84");

        // Wow someone set COLUMNS and LINES to a weird value.
        // Now the tty's termsize doesn't matter.
        vars.set_one(L!("COLUMNS"), env_global, L!("75").to_owned());
        vars.set_one(L!("LINES"), env_global, L!("150").to_owned());
        ts.handle_columns_lines_var_change(parser.vars());
        assert_eq!(ts.last(), new_test_termsize(75, 150));
        assert_eq!(vars.get(L!("COLUMNS")).unwrap().as_string(), "75");
        assert_eq!(vars.get(L!("LINES")).unwrap().as_string(), "150");

        vars.set_one(L!("COLUMNS"), env_global, L!("33").to_owned());
        ts.handle_columns_lines_var_change(parser.vars());
        assert_eq!(ts.last(), new_test_termsize(33, 150));

        // Oh it got SIGWINCH, now the tty matters again.
        handle_winch();
        assert_eq!(ts.last(), new_test_termsize(33, 150));
        assert_eq!(ts.updating(&parser), stubby_termsize().unwrap());
        assert_eq!(vars.get(L!("COLUMNS")).unwrap().as_string(), "42");
        assert_eq!(vars.get(L!("LINES")).unwrap().as_string(), "84");

        // Test initialize().
        vars.set_one(L!("COLUMNS"), env_global, L!("83").to_owned());
        vars.set_one(L!("LINES"), env_global, L!("38").to_owned());
        ts.initialize(vars);
        assert_eq!(ts.last(), new_test_termsize(83, 38));

        // initialize() even beats the tty reader until a sigwinch.
        let ts2 = TermsizeContainer {
            data: Mutex::new(TermsizeData::defaults()),
            setting_env_vars: AtomicBool::new(false),
            tty_size_reader: stubby_termsize,
        };
        ts.initialize(parser.vars());
        ts2.updating(&parser);
        assert_eq!(ts.last(), new_test_termsize(83, 38));
        handle_winch();
        assert_eq!(ts2.updating(&parser), stubby_termsize().unwrap());
    }
}
