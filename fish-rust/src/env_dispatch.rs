use crate::common::ToCString;
use crate::curses::{self, Term};
use crate::env::{setenv_lock, unsetenv_lock, EnvMode, EnvStack, Environment};
use crate::env::{CURSES_INITIALIZED, READ_BYTE_LIMIT, TERM_HAS_XN};
use crate::ffi::is_interactive_session;
use crate::flog::FLOG;
use crate::function;
use crate::output::ColorSupport;
use crate::wchar::prelude::*;
use crate::wutil::fish_wcstoi;
use std::borrow::Cow;
use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::ptr;
use std::sync::atomic::{AtomicBool, Ordering};

#[cxx::bridge]
mod env_dispatch_ffi {
    extern "Rust" {
        fn env_dispatch_init_ffi();
        fn term_supports_setting_title() -> bool;
        fn use_posix_spawn() -> bool;
    }
}

/// List of all locale environment variable names that might trigger (re)initializing of the locale
/// subsystem. These are only the variables we're possibly interested in.
#[rustfmt::skip]
const LOCALE_VARIABLES: [&wstr; 10] = [
    L!("LANG"),       L!("LANGUAGE"), L!("LC_ALL"),
    L!("LC_COLLATE"), L!("LC_CTYPE"), L!("LC_MESSAGES"),
    L!("LC_NUMERIC"), L!("LC_TIME"),  L!("LOCPATH"),
    L!("fish_allow_singlebyte_locale"),
];

#[rustfmt::skip]
const CURSES_VARIABLES: [&wstr; 3] = [
    L!("TERM"), L!("TERMINFO"), L!("TERMINFO_DIRS")
];

/// Whether to use `posix_spawn()` when possible.
static USE_POSIX_SPAWN: AtomicBool = AtomicBool::new(false);

/// Whether we think we can set the terminal title or not.
static CAN_SET_TERM_TITLE: AtomicBool = AtomicBool::new(false);

/// The variable dispatch table. This is set at startup and cannot be modified after.
static VAR_DISPATCH_TABLE: once_cell::sync::Lazy<VarDispatchTable> =
    once_cell::sync::Lazy::new(|| {
        let mut table = VarDispatchTable::default();

        for name in LOCALE_VARIABLES {
            table.add_anon(name, handle_locale_change);
        }

        for name in CURSES_VARIABLES {
            table.add_anon(name, handle_curses_change);
        }

        table.add(L!("TZ"), handle_tz_change);
        table.add_anon(L!("fish_term256"), handle_fish_term_change);
        table.add_anon(L!("fish_term24bit"), handle_fish_term_change);
        table.add_anon(L!("fish_escape_delay_ms"), update_wait_on_escape_ms);
        table.add_anon(
            L!("fish_sequence_key_delay_ms"),
            update_wait_on_sequence_key_ms,
        );
        table.add_anon(L!("fish_emoji_width"), guess_emoji_width);
        table.add_anon(L!("fish_ambiguous_width"), handle_change_ambiguous_width);
        table.add_anon(L!("LINES"), handle_term_size_change);
        table.add_anon(L!("COLUMNS"), handle_term_size_change);
        table.add_anon(L!("fish_complete_path"), handle_complete_path_change);
        table.add_anon(L!("fish_function_path"), handle_function_path_change);
        table.add_anon(L!("fish_read_limit"), handle_read_limit_change);
        table.add_anon(L!("fish_history"), handle_fish_history_change);
        table.add_anon(
            L!("fish_autosuggestion_enabled"),
            handle_autosuggestion_change,
        );
        table.add_anon(
            L!("fish_use_posix_spawn"),
            handle_fish_use_posix_spawn_change,
        );
        table.add_anon(L!("fish_trace"), handle_fish_trace);
        table.add_anon(
            L!("fish_cursor_selection_mode"),
            handle_fish_cursor_selection_mode_change,
        );

        table
    });

type NamedEnvCallback = fn(name: &wstr, env: &EnvStack);
type AnonEnvCallback = fn(env: &EnvStack);

enum EnvCallback {
    Named(NamedEnvCallback),
    Anon(AnonEnvCallback),
}

#[derive(Default)]
struct VarDispatchTable {
    table: HashMap<&'static wstr, EnvCallback>,
}

// TODO: Delete this after input_common is ported (and pass the input_function function directly).
fn update_wait_on_escape_ms(vars: &EnvStack) {
    let fish_escape_delay_ms = vars.get_unless_empty(L!("fish_escape_delay_ms"));
    let var = crate::env::environment::env_var_to_ffi(fish_escape_delay_ms);
    crate::ffi::update_wait_on_escape_ms_ffi(var);
}
fn update_wait_on_sequence_key_ms(vars: &EnvStack) {
    let fish_sequence_key_delay_ms = vars.get_unless_empty(L!("fish_sequence_key_delay_ms"));
    let var = crate::env::environment::env_var_to_ffi(fish_sequence_key_delay_ms);
    crate::ffi::update_wait_on_sequence_key_ms_ffi(var);
}

impl VarDispatchTable {
    /// Add a callback for the variable `name`. We must not already be observing this variable.
    pub fn add(&mut self, name: &'static wstr, callback: NamedEnvCallback) {
        let prev = self.table.insert(name, EnvCallback::Named(callback));
        assert!(prev.is_none(), "Already observing {}", name);
    }

    /// Add an callback for the variable `name`. We must not already be observing this variable.
    pub fn add_anon(&mut self, name: &'static wstr, callback: AnonEnvCallback) {
        let prev = self.table.insert(name, EnvCallback::Anon(callback));
        assert!(prev.is_none(), "Already observing {}", name);
    }

    pub fn dispatch(&self, key: &wstr, vars: &EnvStack) {
        match self.table.get(key) {
            Some(EnvCallback::Named(named)) => (named)(key, vars),
            Some(EnvCallback::Anon(anon)) => (anon)(vars),
            None => (),
        }
    }
}

fn handle_timezone(var_name: &wstr, vars: &EnvStack) {
    let var = vars.get_unless_empty(var_name).map(|v| v.as_string());
    FLOG!(
        env_dispatch,
        "handle_timezone() current timezone var:",
        var_name,
        "=>",
        var.as_ref()
            .map(|v| v.as_utfstr())
            .unwrap_or(L!("MISSING/EMPTY")),
    );
    if let Some(value) = var {
        setenv_lock(var_name, &value, true);
    } else {
        unsetenv_lock(var_name);
    }

    extern "C" {
        fn tzset();
    }

    unsafe {
        tzset();
    }
}

/// Update the value of [`FISH_EMOJI_WIDTH`].
fn guess_emoji_width(vars: &EnvStack) {
    use crate::fallback::FISH_EMOJI_WIDTH;

    if let Some(width_str) = vars.get(L!("fish_emoji_width")) {
        // The only valid values are 1 or 2; we default to 1 if it was an invalid int.
        let new_width = fish_wcstoi(&width_str.as_string()).unwrap_or(1).clamp(1, 2);
        FISH_EMOJI_WIDTH.store(new_width, Ordering::Relaxed);
        FLOG!(
            term_support,
            "Overriding default fish_emoji_width w/",
            new_width
        );
        return;
    }

    let term = vars
        .get(L!("TERM_PROGRAM"))
        .map(|v| v.as_string())
        .unwrap_or_else(WString::new);
    // The format and contents of $TERM_PROGRAM_VERSION depend on $TERM_PROGRAM. Under
    // Apple_Terminal, this is an integral value in the hundreds corresponding to the
    // CFBundleVersion of Terminal.app; under iTerm, this is the version number which can contain
    // multiple periods (e.g 3.4.19). Currently we only care about Apple_Terminal but the C++ code
    // used wcstod() to parse at least the major.minor value of cases like the latter.
    //
    // TODO: Move this inside the Apple_Terminal branch and use i32::FromStr (i.e. str::parse())
    // instead.
    let version = vars
        .get(L!("TERM_PROGRAM_VERSION"))
        .map(|v| v.as_string())
        .and_then(|v| {
            let mut consumed = 0;
            crate::wutil::wcstod::wcstod(&v, '.', &mut consumed).ok()
        })
        .unwrap_or(0.0);

    if term == "Apple_Terminal" && version as i32 >= 400 {
        // Apple Terminal on High Sierra
        FISH_EMOJI_WIDTH.store(2, Ordering::Relaxed);
        FLOG!(term_support, "default emoji width: 2 for", term);
    } else if term == "iTerm.app" {
        // iTerm2 now defaults to Unicode 9 sizes for anything after macOS 10.12
        FISH_EMOJI_WIDTH.store(2, Ordering::Relaxed);
        FLOG!(term_support, "default emoji width 2 for iTerm2");
    } else {
        // Default to whatever the system's wcwidth gives for U+1F603, but only if it's at least
        // 1 and at most 2.
        let width = crate::fallback::wcwidth('😃').clamp(1, 2);
        FISH_EMOJI_WIDTH.store(width, Ordering::Relaxed);
        FLOG!(term_support, "default emoji width:", width);
    }
}

/// React to modifying the given variable.
pub fn env_dispatch_var_change(key: &wstr, vars: &EnvStack) {
    use once_cell::sync::Lazy;

    // We want to ignore variable changes until the dispatch table is explicitly initialized.
    if let Some(dispatch_table) = Lazy::get(&VAR_DISPATCH_TABLE) {
        dispatch_table.dispatch(key, vars);
    }
}

fn handle_fish_term_change(vars: &EnvStack) {
    update_fish_color_support(vars);
    crate::ffi::reader_schedule_prompt_repaint();
}

fn handle_change_ambiguous_width(vars: &EnvStack) {
    let new_width = vars
        .get(L!("fish_ambiguous_width"))
        .map(|v| v.as_string())
        // We use the default value of 1 if it was an invalid int.
        .and_then(|fish_ambiguous_width| fish_wcstoi(&fish_ambiguous_width).ok())
        .unwrap_or(1)
        // Clamp in case of negative values.
        .max(0);
    crate::fallback::FISH_AMBIGUOUS_WIDTH.store(new_width, Ordering::Relaxed);
}

fn handle_term_size_change(vars: &EnvStack) {
    crate::termsize::handle_columns_lines_var_change(vars);
}

fn handle_fish_history_change(vars: &EnvStack) {
    let fish_history = vars.get(L!("fish_history"));
    let var = crate::env::env_var_to_ffi(fish_history);
    crate::ffi::reader_change_history(&crate::ffi::history_session_id(var));
}

fn handle_fish_cursor_selection_mode_change(vars: &EnvStack) {
    use crate::reader::CursorSelectionMode;

    let inclusive = vars
        .get(L!("fish_cursor_selection_mode"))
        .as_ref()
        .map(|v| v.as_string())
        .map(|v| v == "inclusive")
        .unwrap_or(false);
    let mode = if inclusive {
        CursorSelectionMode::Inclusive
    } else {
        CursorSelectionMode::Exclusive
    };

    let mode = mode as u8;
    crate::ffi::reader_change_cursor_selection_mode(mode);
}

fn handle_autosuggestion_change(vars: &EnvStack) {
    // TODO: This was a call to reader_set_autosuggestion_enabled(vars) and
    // reader::check_autosuggestion_enabled() should be private to the `reader` module.
    crate::ffi::reader_set_autosuggestion_enabled_ffi(crate::reader::check_autosuggestion_enabled(
        vars,
    ));
}

fn handle_function_path_change(_: &EnvStack) {
    function::invalidate_path();
}

fn handle_complete_path_change(_: &EnvStack) {
    crate::ffi::complete_invalidate_path();
}

fn handle_tz_change(var_name: &wstr, vars: &EnvStack) {
    handle_timezone(var_name, vars);
}

fn handle_locale_change(vars: &EnvStack) {
    init_locale(vars);
    // We need to re-guess emoji width because the locale might have changed to a multibyte one.
    guess_emoji_width(vars);
}

fn handle_curses_change(vars: &EnvStack) {
    guess_emoji_width(vars);
    init_curses(vars);
}

fn handle_fish_use_posix_spawn_change(vars: &EnvStack) {
    // Note that if the variable is missing or empty we default to true (if allowed).
    if !allow_use_posix_spawn() {
        USE_POSIX_SPAWN.store(false, Ordering::Relaxed);
    } else if let Some(var) = vars.get(L!("fish_use_posix_spawn")) {
        let use_posix_spawn =
            var.is_empty() || crate::wcstringutil::bool_from_string(&var.as_string());
        USE_POSIX_SPAWN.store(use_posix_spawn, Ordering::Relaxed);
    } else {
        USE_POSIX_SPAWN.store(true, Ordering::Relaxed);
    }
}

/// Allow the user to override the limits on how much data the `read` command will process. This is
/// primarily intended for testing, but could also be used directly by users in special situations.
fn handle_read_limit_change(vars: &EnvStack) {
    let read_byte_limit = vars
        .get_unless_empty(L!("fish_read_limit"))
        .map(|v| v.as_string())
        .and_then(|v| {
            // We use fish_wcstoul() to support leading/trailing whitespace
            match (crate::wutil::fish_wcstoul(&v).ok())
                // wcstoul() returns a u64 but want a usize. Handle overflow on 32-bit platforms.
                .and_then(|_u64| usize::try_from(_u64).ok())
            {
                Some(v) => Some(v),
                None => {
                    // We intentionally warn here even in non-interactive mode.
                    FLOG!(warning, "Ignoring invalid $fish_read_limit");
                    None
                }
            }
        });

    // Clippy should recognize comments in an empty match branch as a valid pattern!
    #[allow(clippy::single_match)]
    match read_byte_limit {
        Some(new_limit) => READ_BYTE_LIMIT.store(new_limit, Ordering::Relaxed),
        None => {
            // TODO: reset READ_BYTE_LIMIT to the default value on receiving an invalid value
            // instead of persisting the previous value, which may or may not have been the
            // default.
        }
    }
}

fn handle_fish_trace(vars: &EnvStack) {
    let enabled = vars.get_unless_empty(L!("fish_trace")).is_some();
    crate::trace::trace_set_enabled(enabled);
}

pub fn env_dispatch_init(vars: &EnvStack) {
    use once_cell::sync::Lazy;

    run_inits(vars);
    // env_dispatch_var_change() purposely supresses change notifications until the dispatch table
    // was initialized elsewhere (either explicitly as below or via deref of VAR_DISPATCH_TABLE).
    Lazy::force(&VAR_DISPATCH_TABLE);
}

pub fn env_dispatch_init_ffi() {
    let vars = EnvStack::principal();
    env_dispatch_init(vars);
}

/// Runs the subset of dispatch functions that need to be called at startup.
fn run_inits(vars: &EnvStack) {
    init_locale(vars);
    init_curses(vars);
    guess_emoji_width(vars);
    update_wait_on_escape_ms(vars);
    update_wait_on_sequence_key_ms(vars);
    handle_read_limit_change(vars);
    handle_fish_use_posix_spawn_change(vars);
    handle_fish_trace(vars);
}

/// Updates our idea of whether we support term256 and term24bit (see issue #10222).
fn update_fish_color_support(vars: &EnvStack) {
    // Detect or infer term256 support. If fish_term256 is set, we respect it. Otherwise, infer it
    // from $TERM or use terminfo.

    let term = vars
        .get(L!("TERM"))
        .map(|v| v.as_string())
        .unwrap_or_else(WString::new);
    let max_colors = curses::term().and_then(|term| term.max_colors);
    let mut supports_256color = false;
    let mut supports_24bit = false;

    if let Some(fish_term256) = vars.get(L!("fish_term256")).map(|v| v.as_string()) {
        // $fish_term256
        supports_256color = crate::wcstringutil::bool_from_string(&fish_term256);
        FLOG!(
            term_support,
            "256-color support determined by $fish_term256:",
            supports_256color
        );
    } else if term.find(L!("256color")).is_some() {
        // TERM contains "256color": 256 colors explicitly supported.
        supports_256color = true;
        FLOG!(term_support, "256-color support enabled for TERM", term);
    } else if term.find(L!("xterm")).is_some() {
        // Assume that all "xterm" terminals can handle 256
        supports_256color = true;
        FLOG!(term_support, "256-color support enabled for TERM", term);
    }
    // See if terminfo happens to identify 256 colors
    else if let Some(max_colors) = max_colors {
        supports_256color = max_colors >= 256;
        FLOG!(
            term_support,
            "256-color support:",
            max_colors,
            "per termcap/terminfo entry for",
            term
        );
    }

    if let Some(fish_term24bit) = vars.get(L!("fish_term24bit")).map(|v| v.as_string()) {
        // $fish_term24bit
        supports_24bit = crate::wcstringutil::bool_from_string(&fish_term24bit);
        FLOG!(
            term_support,
            "$fish_term24bit preference: 24-bit color",
            if supports_24bit {
                "enabled"
            } else {
                "disabled"
            }
        );
    } else if vars.get(L!("STY")).is_some() || term.starts_with(L!("eterm")) {
        // Screen and emacs' ansi-term swallow true-color sequences, so we ignore them unless
        // force-enabled.
        supports_24bit = false;
        FLOG!(
            term_support,
            "True-color support: disabled for eterm/screen"
        );
    } else if max_colors.unwrap_or(0) > 32767 {
        // $TERM wins, xterm-direct reports 32767 colors and we assume that's the minimum as xterm
        // is weird when it comes to color.
        supports_24bit = true;
        FLOG!(
            term_support,
            "True-color support: enabled per termcap/terminfo for",
            term,
            "with",
            max_colors.unwrap(),
            "colors"
        );
    } else if let Some(ct) = vars.get(L!("COLORTERM")).map(|v| v.as_string()) {
        // If someone sets $COLORTERM, that's the sort of color they want.
        if ct == "truecolor" || ct == "24bit" {
            supports_24bit = true;
        }
        FLOG!(
            term_support,
            "True-color support",
            if supports_24bit {
                "enabled"
            } else {
                "disabled"
            },
            "per $COLORTERM",
            ct
        );
    } else if vars.get(L!("KONSOLE_VERSION")).is_some()
        || vars.get(L!("KONSOLE_PROFILE_NAME")).is_some()
    {
        // All Konsole versions that use $KONSOLE_VERSION are new enough to support this, so no
        // check is needed.
        supports_24bit = true;
        FLOG!(term_support, "True-color support: enabled for Konsole");
    } else if let Some(it) = vars.get(L!("ITERM_SESSION_ID")).map(|v| v.as_string()) {
        // Supporting versions of iTerm include a colon here.
        // We assume that if this is iTerm it can't also be st, so having this check inside is okay.
        if !it.contains(':') {
            supports_24bit = true;
            FLOG!(term_support, "True-color support: enabled for iTerm");
        }
    } else if term.starts_with("st-") {
        supports_24bit = true;
        FLOG!(term_support, "True-color support: enabling for st");
    } else if let Some(vte) = vars.get(L!("VTE_VERSION")).map(|v| v.as_string()) {
        if fish_wcstoi(&vte).unwrap_or(0) > 3600 {
            supports_24bit = true;
            FLOG!(
                term_support,
                "True-color support: enabled for VTE version",
                vte
            );
        }
    }

    let mut color_support = ColorSupport::default();
    color_support.set(ColorSupport::TERM_256COLOR, supports_256color);
    color_support.set(ColorSupport::TERM_24BIT, supports_24bit);
    crate::output::set_color_support(color_support);
}

/// Try to initialize the terminfo/curses subsystem using our fallback terminal name. Do not set
/// `$TERM` to our fallback. We're only doing this in the hope of getting a functional shell.
/// If we launch an external command that uses `$TERM`, it should get the same value we were given,
/// if any.
fn initialize_curses_using_fallbacks(vars: &EnvStack) {
    // xterm-256color is the most used terminal type by a massive margin, especially counting
    // terminals that are mostly compatible.
    const FALLBACKS: [&str; 4] = ["xterm-256color", "xterm", "ansi", "dumb"];

    let current_term = vars
        .get_unless_empty(L!("TERM"))
        .map(|v| v.as_string())
        .unwrap_or(Default::default());

    for term in FALLBACKS {
        // If $TERM is already set to the fallback name we're about to use, there's no point in
        // seeing if the fallback name can be used.
        if current_term == term {
            continue;
        }

        // `term` here is one of our hard-coded strings above; we can unwrap because we can
        // guarantee it doesn't contain any interior NULs.
        let term_cstr = CString::new(term).unwrap();
        let success = curses::setup(Some(&term_cstr), libc::STDOUT_FILENO, |term| {
            apply_term_hacks(vars, term)
        })
        .is_some();
        if is_interactive_session() {
            if success {
                FLOG!(warning, wgettext!("Using fallback terminal type"), term);
            } else {
                FLOG!(
                    warning,
                    wgettext!("Could not set up terminal using the fallback terminal type"),
                    term,
                );
            }
        }

        if success {
            break;
        }
    }
}

/// Apply any platform- or environment-specific hacks to our curses [`Term`] instance.
fn apply_term_hacks(vars: &EnvStack, term: &mut Term) {
    if cfg!(target_os = "macos") {
        // Hack in missing italics and dim capabilities omitted from macOS xterm-256color terminfo.
        // Improves the user experience under Terminal.app and iTerm.
        let term_prog = vars
            .get(L!("TERM_PROGRAM"))
            .map(|v| v.as_string())
            .unwrap_or(WString::new());
        if term_prog == "Apple_Terminal" || term_prog == "iTerm.app" {
            if let Some(term_val) = vars.get(L!("TERM")).map(|v| v.as_string()) {
                if term_val == "xterm-256color" {
                    const SITM_ESC: &[u8] = b"\x1B[3m";
                    const RITM_ESC: &[u8] = b"\x1B[23m";
                    const DIM_ESC: &[u8] = b"\x1B[2m";

                    if term.enter_italics_mode.is_none() {
                        term.enter_italics_mode = Some(SITM_ESC.to_cstring());
                    }
                    if term.exit_italics_mode.is_none() {
                        term.exit_italics_mode = Some(RITM_ESC.to_cstring());
                    }
                    if term.enter_dim_mode.is_none() {
                        term.enter_dim_mode = Some(DIM_ESC.to_cstring());
                    }
                }
            }
        }
    }
}

/// Apply any platform- or environment-specific hacks that don't involve a `Term` instance.
fn apply_non_term_hacks(vars: &EnvStack) {
    // Midnight Commander tries to extract the last line of the prompt, and does so in a way that is
    // broken if you do '\r' after it like we normally do.
    // See https://midnight-commander.org/ticket/4258.
    if vars.get(L!("MC_SID")).is_some() {
        crate::ffi::screen_set_midnight_commander_hack();
    }
}

/// This is a pretty lame heuristic for detecting terminals that do not support setting the title.
/// If we recognise the terminal name as that of a virtual terminal, we assume it supports setting
/// the title. If we recognise it as that of a console, we assume it does not support setting the
/// title. Otherwise we check the ttyname and see if we believe it is a virtual terminal.
///
/// One situation in which this breaks down is with screen, since screen supports setting the
/// terminal title if the underlying terminal does so, but will print garbage on terminals that
/// don't. Since we can't see the underlying terminal below screen there is no way to fix this.
fn does_term_support_setting_title(vars: &EnvStack) -> bool {
    #[rustfmt::skip]
    const TITLE_TERMS: &[&wstr] = &[
        L!("xterm"), L!("screen"),    L!("tmux"),    L!("nxterm"),
        L!("rxvt"),  L!("alacritty"), L!("wezterm"),
    ];

    let Some(term) = vars.get_unless_empty(L!("TERM")).map(|v| v.as_string()) else {
        return false;
    };
    let term: &wstr = term.as_ref();

    if curses::term().map(|term| term.set_title.is_some()) == Some(true) {
        return true;
    }

    let recognized = TITLE_TERMS.contains(&term)
        || term.starts_with(L!("xterm-"))
        || term.starts_with(L!("screen-"))
        || term.starts_with(L!("tmux-"));
    if !recognized {
        if [
            L!("linux"),
            L!("dumb"),
            L!("vt100"), // NetBSD
            L!("wsvt25"),
        ]
        .contains(&term)
        {
            return false;
        }

        let mut buf = [b'\0'; libc::PATH_MAX as usize];
        let retval =
            unsafe { libc::ttyname_r(libc::STDIN_FILENO, buf.as_mut_ptr().cast(), buf.len()) };
        let buf = &buf[..buf.iter().position(|c| *c == b'\0').unwrap()];
        if retval != 0
            || buf.windows(b"tty".len()).any(|w| w == b"tty")
            || buf.windows(b"/vc/".len()).any(|w| w == b"/vc/")
        {
            return false;
        }
    }

    true
}

// Initialize the curses subsystem
fn init_curses(vars: &EnvStack) {
    for var_name in CURSES_VARIABLES {
        if let Some(value) = vars
            .getf_unless_empty(var_name, EnvMode::EXPORT)
            .map(|v| v.as_string())
        {
            FLOG!(term_support, "curses var", var_name, "=", value);
            setenv_lock(var_name, &value, true);
        } else {
            FLOG!(term_support, "curses var", var_name, "is missing or empty");
            unsetenv_lock(var_name);
        }
    }

    if curses::setup(None, libc::STDOUT_FILENO, |term| {
        apply_term_hacks(vars, term)
    })
    .is_none()
    {
        if is_interactive_session() {
            let term = vars.get_unless_empty(L!("TERM")).map(|v| v.as_string());
            FLOG!(warning, wgettext!("Could not set up terminal."));
            if let Some(term) = term {
                FLOG!(warning, wgettext!("TERM environment variable set to"), term);
                FLOG!(
                    warning,
                    wgettext!("Check that this terminal type is supported on this system.")
                );
            } else {
                FLOG!(warning, wgettext!("TERM environment variable not set."));
            }
        }

        initialize_curses_using_fallbacks(vars);
    }

    // Configure hacks that apply regardless of whether we successfully init curses or not.
    apply_non_term_hacks(vars);

    // Store some global variables that reflect the term's capabilities
    CAN_SET_TERM_TITLE.store(does_term_support_setting_title(vars), Ordering::Relaxed);
    if let Some(term) = curses::term() {
        TERM_HAS_XN.store(term.eat_newline_glitch, Ordering::Relaxed);
    }

    update_fish_color_support(vars);
    // Invalidate the cached escape sequences since they may no longer be valid.
    crate::ffi::screen_clear_layout_cache_ffi();
    CURSES_INITIALIZED.store(true, Ordering::Relaxed);
}

/// Initialize the locale subsystem
fn init_locale(vars: &EnvStack) {
    #[rustfmt::skip]
    const UTF8_LOCALES: &[&str] = &[
        "C.UTF-8", "en_US.UTF-8", "en_GB.UTF-8", "de_DE.UTF-8", "C.utf8", "UTF-8",
    ];

    let old_msg_locale: CString = {
        let old = unsafe { libc::setlocale(libc::LC_MESSAGES, ptr::null()) };
        assert_ne!(old, ptr::null_mut());
        // We have to make a copy because the subsequent setlocale() call to change the locale will
        // invalidate the pointer from this setlocale() call.

        // safety: `old` is not a null-pointer, and should be a reference to the currently set locale
        unsafe { CStr::from_ptr(old.cast()) }.to_owned()
    };

    for var_name in LOCALE_VARIABLES {
        let var = vars
            .getf_unless_empty(var_name, EnvMode::EXPORT)
            .map(|v| v.as_string());
        if let Some(value) = var {
            FLOG!(env_locale, "locale var", var_name, "=", value);
            setenv_lock(var_name, &value, true);
        } else {
            FLOG!(env_locale, "locale var", var_name, "is missing or empty");
            unsetenv_lock(var_name);
        }
    }

    let user_locale = {
        let loc_ptr = unsafe { libc::setlocale(libc::LC_ALL, b"\0".as_ptr().cast()) };
        if loc_ptr.is_null() {
            FLOG!(env_locale, "user has an invalid locale configured");
            None
        } else {
            // safety: setlocale did not return a null-pointer, so it is a valid pointer
            Some(unsafe { CStr::from_ptr(loc_ptr) })
        }
    };

    // Try to get a multibyte-capable encoding.
    // A "C" locale is broken for our purposes: any wchar function will break on it. So we try
    // *really, really, really hard* to not have one.
    let fix_locale = vars
        .get_unless_empty(L!("fish_allow_singlebyte_locale"))
        .map(|v| v.as_string())
        .map(|allow_c| !crate::wcstringutil::bool_from_string(&allow_c))
        .unwrap_or(true);

    if fix_locale && crate::compat::MB_CUR_MAX() == 1 {
        FLOG!(env_locale, "Have singlebyte locale, trying to fix.");
        for locale in UTF8_LOCALES {
            {
                let locale = CString::new(locale.to_owned()).unwrap();
                // this can fail, that is fine
                unsafe { libc::setlocale(libc::LC_CTYPE, locale.as_ptr()) };
            }
            if crate::compat::MB_CUR_MAX() > 1 {
                FLOG!(env_locale, "Fixed locale:", locale);
                break;
            }
        }

        if crate::compat::MB_CUR_MAX() == 1 {
            FLOG!(env_locale, "Failed to fix locale.");
        }
    }

    // We *always* use a C-locale for numbers because we want '.' (except for in printf).
    let loc_ptr = unsafe { libc::setlocale(libc::LC_NUMERIC, b"C\0".as_ptr().cast()) };
    // should never fail, the C locale should always be defined
    assert_ne!(loc_ptr, ptr::null_mut());

    // See that we regenerate our special locale for numbers
    crate::locale::invalidate_numeric_locale();
    crate::common::fish_setlocale();
    FLOG!(
        env_locale,
        "init_locale() setlocale():",
        user_locale
            .map(CStr::to_string_lossy)
            .unwrap_or(Cow::Borrowed("(null)"))
    );

    let new_msg_loc_ptr = unsafe { libc::setlocale(libc::LC_MESSAGES, std::ptr::null()) };
    // should never fail
    assert_ne!(new_msg_loc_ptr, ptr::null_mut());
    // safety: we just asserted it is not a null-pointer.
    let new_msg_locale = unsafe { CStr::from_ptr(new_msg_loc_ptr) };
    FLOG!(
        env_locale,
        "Old LC_MESSAGES locale:",
        old_msg_locale.to_string_lossy()
    );
    FLOG!(
        env_locale,
        "New LC_MESSAGES locale:",
        new_msg_locale.to_string_lossy()
    );

    #[cfg(feature = "gettext")]
    {
        if old_msg_locale.as_c_str() != new_msg_locale {
            // Make change known to GNU gettext.
            extern "C" {
                static mut _nl_msg_cat_cntr: libc::c_int;
            }
            unsafe {
                _nl_msg_cat_cntr += 1;
            }
        }
    }
}

pub fn use_posix_spawn() -> bool {
    USE_POSIX_SPAWN.load(Ordering::Relaxed)
}

/// Whether or not we are running on an OS where we allow ourselves to use `posix_spawn()`.
const fn allow_use_posix_spawn() -> bool {
    #![allow(clippy::if_same_then_else)]
    #![allow(clippy::needless_bool)]
    // OpenBSD's posix_spawn returns status 127 instead of erroring with ENOEXEC when faced with a
    // shebang-less script. Disable posix_spawn on OpenBSD.
    if cfg!(target_os = "openbsd") {
        false
    } else if cfg!(not(target_os = "linux")) {
        true
    } else {
        // The C++ code used __GLIBC_PREREQ(2, 24) && !defined(__UCLIBC__) to determine if we'll use
        // posix_spawn() by default on Linux. Surprise! We don't have to worry about porting that
        // logic here because the libc crate only supports 2.26+ atm.
        // See https://github.com/rust-lang/libc/issues/1412
        true
    }
}

/// Returns true if we think the terminal support setting its title.
pub fn term_supports_setting_title() -> bool {
    CAN_SET_TERM_TITLE.load(Ordering::Relaxed)
}
