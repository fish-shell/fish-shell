//! Utility for transferring the tty to a child process in a scoped way,
//! and reclaiming it after.

use crate::common::{self, safe_write_loop};
use crate::env::Environment;
use crate::env_dispatch::MIDNIGHT_COMMANDER_SID;
use crate::flog::{flog, flogf};
use crate::global_safety::RelaxedAtomicBool;
use crate::job_group::JobGroup;
use crate::proc::JobGroupRef;
use crate::terminal::TerminalCommand::{
    self, ApplicationKeypadModeDisable, ApplicationKeypadModeEnable, DecrstBracketedPaste,
    DecrstColorThemeReporting, DecrstFocusReporting, DecsetBracketedPaste,
    DecsetColorThemeReporting, DecsetFocusReporting, KittyKeyboardProgressiveEnhancementsDisable,
    KittyKeyboardProgressiveEnhancementsEnable, ModifyOtherKeysDisable, ModifyOtherKeysEnable,
};
use crate::terminal::{Output, Outputter};
use crate::threads::assert_is_main_thread;
use crate::wchar::prelude::*;
use crate::wchar_ext::ToWString;
use crate::wutil::{perror, wcstoi};
use libc::{EINVAL, ENOTTY, EPERM, STDIN_FILENO, WNOHANG};
use once_cell::sync::OnceCell;
use std::mem::MaybeUninit;
use std::sync::atomic::{AtomicBool, AtomicPtr, Ordering};

/// Whether kitty keyboard protocol support is present in the TTY.
static KITTY_KEYBOARD_SUPPORTED: OnceCell<bool> = OnceCell::new();

/// Set that the TTY supports the kitty keyboard protocol.
pub fn maybe_set_kitty_keyboard_capability() {
    KITTY_KEYBOARD_SUPPORTED.get_or_init(|| true);
}

pub(crate) static SCROLL_CONTENT_UP_SUPPORTED: OnceCell<bool> = OnceCell::new();
pub(crate) const SCROLL_CONTENT_UP_TERMINFO_CODE: &str = "indn";

// Get the support capability for kitty keyboard protocol.
pub fn get_scroll_content_up_capability() -> Option<bool> {
    SCROLL_CONTENT_UP_SUPPORTED.get().copied()
}

pub fn maybe_set_scroll_content_up_capability() {
    SCROLL_CONTENT_UP_SUPPORTED.get_or_init(|| {
        flog!(reader, "SCROLL UP is supported");
        true
    });
}

pub static TERMINAL_OS_NAME: OnceCell<Option<WString>> = OnceCell::new();
pub(crate) const XTGETTCAP_QUERY_OS_NAME: &str = "query-os-name";

pub static XTVERSION: OnceCell<WString> = OnceCell::new();

pub fn xtversion() -> Option<&'static wstr> {
    XTVERSION.get().as_ref().map(|s| s.as_utfstr())
}

// Facts that affect how we communicate with the TTY.
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum TtyQuirks {
    None,
    // Running Midnight Commander which can't parse CSI yet.
    PreCsiMidnightCommander,
    // Running in iTerm2 before 3.5.12, which causes issues when using the kitty keyboard protocol.
    PreKittyIterm2,
    // Whether we are running under WezTerm.
    Wezterm,
}

impl TtyQuirks {
    // Create a new TtyQuirks instance with the current environment.
    fn detect(vars: &dyn Environment, xtversion: &wstr) -> Self {
        use TtyQuirks::*;
        if vars.get(MIDNIGHT_COMMANDER_SID).is_some()
            && vars.get(L!("__mc_kitty_keyboard")).is_none()
        {
            PreCsiMidnightCommander
        } else if get_iterm2_version(xtversion).is_some_and(|v| v < (3, 5, 12)) {
            PreKittyIterm2
        } else if xtversion.starts_with(L!("WezTerm ")) {
            Wezterm
        } else {
            None
        }
    }
}

// Helper to determine which keyboard protocols to enable.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
enum ProtocolKind {
    KittyKeyboard, // Kitty keyboard support, producing CSI-u style encoding.
    Other,         // Other protocols (e.g., modifyOtherKeys)
    WorkAroundWezTerm,
    None, // No protocols
}

// Commands to emit to enable or disable TTY protocols. Each of these contains
// the full serialized command sequence as bytes. It's structured in this awkward
// way so that we can use it from a signal handler - no need to allocate or deallocate
// as Kitty support is discovered through tty queries.
struct ProtocolBytes {
    kitty_keyboard: Box<[u8]>,
    other: Box<[u8]>,
    wezterm_workaround: Box<[u8]>,
    none: Box<[u8]>,
}

// The combined set of TTY protocols.
// This is created once at startup and then leaked, so it may be used
// from the SIGTERM handler.
struct TtyProtocolsSet {
    // TTY quirks.
    quirks: TtyQuirks,
    // Variants to enable or disable tty protocols.
    enablers: ProtocolBytes,
    disablers: ProtocolBytes,
}

impl TtyProtocolsSet {
    // Get commands to enable or disable TTY protocols
    // and the KITTY_KEYBOARD_SUPPORTED global variable.
    // THIS IS USED FROM A SIGNAL HANDLER.
    fn safe_get_commands(&self, enable: bool) -> &[u8] {
        let protocol = self.quirks.safe_get_supported_protocol();
        let cmds = if enable {
            &self.enablers
        } else {
            &self.disablers
        };
        match protocol {
            ProtocolKind::KittyKeyboard => &cmds.kitty_keyboard,
            ProtocolKind::Other => &cmds.other,
            ProtocolKind::WorkAroundWezTerm => &cmds.wezterm_workaround,
            ProtocolKind::None => &cmds.none,
        }
    }
}

// Serialize a sequence of terminal commands into a byte array.
fn serialize_commands<'a>(cmds: impl Iterator<Item = TerminalCommand<'a>>) -> Box<[u8]> {
    let mut out = Outputter::new_buffering();
    for cmd in cmds {
        out.write_command(cmd);
    }
    out.contents().into()
}

impl TtyQuirks {
    // Determine which keyboard protocol.
    // This is used from a signal handler.
    fn safe_get_supported_protocol(&self) -> ProtocolKind {
        use TtyQuirks::{PreCsiMidnightCommander, PreKittyIterm2, Wezterm};
        if *self == PreCsiMidnightCommander {
            return ProtocolKind::None;
        }
        if *self == PreKittyIterm2 {
            return ProtocolKind::Other;
        }
        match KITTY_KEYBOARD_SUPPORTED.get() {
            Some(&true) => ProtocolKind::KittyKeyboard,
            Some(&false) => {
                if *self == Wezterm {
                    ProtocolKind::WorkAroundWezTerm
                } else {
                    ProtocolKind::Other
                }
            }
            None => ProtocolKind::None,
        }
    }

    // Return the protocols set to enable or disable TTY protocols.
    fn get_protocols(self) -> TtyProtocolsSet {
        let on_chain = vec![
            DecsetFocusReporting,
            DecsetBracketedPaste,
            DecsetColorThemeReporting,
        ];
        let off_chain = vec![
            DecrstFocusReporting,
            DecrstBracketedPaste,
            DecrstColorThemeReporting,
        ];

        let on_chain = || on_chain.clone().into_iter();
        let off_chain = || off_chain.clone().into_iter();

        let enablers = ProtocolBytes {
            kitty_keyboard: serialize_commands(
                on_chain().chain([KittyKeyboardProgressiveEnhancementsEnable]),
            ),
            other: serialize_commands(on_chain().chain([
                ModifyOtherKeysEnable,       // XTerm's modifyOtherKeys
                ApplicationKeypadModeEnable, // set application keypad mode, so the keypad keys send unique codes
            ])),
            wezterm_workaround: serialize_commands(on_chain().chain([ApplicationKeypadModeEnable])),
            none: serialize_commands(on_chain()),
        };
        let disablers = ProtocolBytes {
            kitty_keyboard: serialize_commands(
                off_chain().chain([KittyKeyboardProgressiveEnhancementsDisable]),
            ),
            other: serialize_commands(
                off_chain().chain([ModifyOtherKeysDisable, ApplicationKeypadModeDisable]),
            ),
            wezterm_workaround: serialize_commands(
                off_chain().chain([ApplicationKeypadModeDisable]),
            ),
            none: serialize_commands(off_chain()),
        };
        TtyProtocolsSet {
            quirks: self,
            enablers,
            disablers,
        }
    }
}

// The global tty protocols. This is set once at startup and not changed thereafter.
// This is an AtomicPtr and not a OnceLock, etc. so that it can be used from a signal handler.
static TTY_PROTOCOLS: AtomicPtr<TtyProtocolsSet> = AtomicPtr::new(std::ptr::null_mut());

// Get the TTY protocols, without initializing it.
fn tty_protocols() -> Option<&'static TtyProtocolsSet> {
    // Safety: TTY_PROTOCOLS is never modified after initialization.
    unsafe { TTY_PROTOCOLS.load(Ordering::Acquire).as_ref() }
}

// Initialize serialized commands for enabling/disabling TTY protocols in signal handlers.
pub fn initialize_tty_protocols(vars: &dyn Environment) {
    // Default missing query responses.
    KITTY_KEYBOARD_SUPPORTED.get_or_init(|| false);
    SCROLL_CONTENT_UP_SUPPORTED.get_or_init(|| false);
    TERMINAL_OS_NAME.get_or_init(|| None);
    let xtversion = XTVERSION.get_or_init(WString::new);

    use std::sync::atomic::Ordering::{Acquire, Release};
    // Standard lazy-init pattern from rust-atomics-and-locks.
    let mut p = TTY_PROTOCOLS.load(Acquire);
    if p.is_null() {
        // Try to swap in a new TTY protocols set.
        p = Box::into_raw(Box::new(TtyQuirks::detect(vars, xtversion).get_protocols()));
        if let Err(_e) = TTY_PROTOCOLS.compare_exchange(std::ptr::null_mut(), p, Release, Acquire) {
            // Safety: p comes from Box::into_raw right above,
            // and wasn't shared with any other thread.
            drop(unsafe { Box::from_raw(p) });
        }
    }
}

// A marker of the current state of the tty protocols.
static TTY_PROTOCOLS_ACTIVE: AtomicBool = AtomicBool::new(false);

// A marker that the tty has been closed (SIGHUP, etc) and so we should not try to write to it.
static TTY_INVALID: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

// Enable or disable TTY protocols by writing the appropriate commands to the tty.
// Return true if we emitted any bytes to the tty.
// Note this does NOT intialize the TTY protocls if not already initialized.
fn set_tty_protocols_active(on_write: fn(), enable: bool) {
    assert_is_main_thread();
    // Have protocols at all? We require someone else to have initialized them.
    let Some(protocols) = tty_protocols() else {
        return;
    };
    // Already set?
    // Note we don't need atomic swaps as this is only called on the main thread.
    // Also note we (logically) set and clear this even if we got SIGHUP.
    if TTY_PROTOCOLS_ACTIVE.load(Ordering::Relaxed) == enable {
        return;
    }
    if enable {
        TTY_PROTOCOLS_ACTIVE.store(true, Ordering::Release);
    }

    // Did we get SIGHUP?
    if TTY_INVALID.load() {
        return;
    }

    // Write the commands to the tty, ignoring errors.
    let commands = protocols.safe_get_commands(enable);
    let _ = common::write_loop(&libc::STDOUT_FILENO, commands);
    if !enable {
        TTY_PROTOCOLS_ACTIVE.store(false, Ordering::Relaxed);
    }

    // Flog any terminal protocol changes of interest.
    let mode = if enable { "Enabling" } else { "Disabling" };
    match protocols.quirks.safe_get_supported_protocol() {
        ProtocolKind::KittyKeyboard => flog!(reader, mode, "kitty keyboard protocol"),
        ProtocolKind::Other => flog!(reader, mode, "other extended keys"),
        ProtocolKind::WorkAroundWezTerm => flog!(reader, mode, "wezterm; no modifyOtherKeys"),
        ProtocolKind::None => (),
    };
    (on_write)();
}

// Helper to check if TTY protocols are active.
pub fn get_tty_protocols_active() -> bool {
    TTY_PROTOCOLS_ACTIVE.load(Ordering::Relaxed)
}

// Called from a signal handler to deactivate TTY protocols before exiting.
// Only async-signal-safe code can be run here.
pub fn safe_deactivate_tty_protocols() {
    // Safety: TTY_PROTOCOLS is never modified after initialization.
    let protocols = unsafe { TTY_PROTOCOLS.load(Ordering::Acquire).as_ref() };
    let Some(protocols) = protocols else {
        // No protocols set, nothing to do.
        return;
    };
    if !TTY_PROTOCOLS_ACTIVE.load(Ordering::Acquire) {
        return;
    }

    // Did we get SIGHUP?
    if TTY_INVALID.load() {
        return;
    }

    let commands = protocols.safe_get_commands(false);
    // Safety: just writing data to stdout.
    let _ = safe_write_loop(&libc::STDOUT_FILENO, commands);
    TTY_PROTOCOLS_ACTIVE.store(false, Ordering::Release);
}

// Called from a signal handler to mark the tty as invalid (e.g. SIGHUP).
// This suppresses any further attempts to write protocols to the tty,
pub fn safe_mark_tty_invalid() {
    TTY_INVALID.store(true);
}

// Allows transferring the tty to a job group, while it runs, in a scoped fashion.
// This has several responsibilities:
//   - Invoking tcsetpgrp() to transfer the tty to the job group.
//     Note this is complex because it is inherently "racey."
//   - Saving tty modes if a job stops. That is, if a job is running and
//     then it stops in the background, we want to record the tty modes
//     it has in the job, so that we can restore them when the job is resumed.
//   - Managing enabling and disabling terminal protocols (bracketed paste, etc).
//  Note it only ever makes sense to run this on the main thread.
pub struct TtyHandoff {
    // The job group which owns the tty, or empty if none.
    owner: Option<JobGroupRef>,
    // Whether terminal protocols were initially enabled.
    // reclaim() restores the state to this.
    tty_protocols_initial: bool,
    // The state of terminal protocols that we set.
    // Note we track this separately from TTY_PROTOCOLS_ACTIVE. We undo the changes
    // we make.
    tty_protocols_applied: bool,
    // Whether reclaim was called, restoring the tty to its pre-scoped value.
    reclaimed: bool,
    // Called after writing to the TTY.
    on_write: fn(),
}

impl TtyHandoff {
    pub fn new(on_write: fn()) -> Self {
        let protocols_active = get_tty_protocols_active();
        TtyHandoff {
            owner: None,
            tty_protocols_initial: protocols_active,
            tty_protocols_applied: protocols_active,
            reclaimed: false,
            on_write,
        }
    }

    /// Mark terminal modes as enabled.
    /// Return true if something was written to the tty.
    pub fn enable_tty_protocols(&mut self) {
        if self.tty_protocols_applied {
            return; // Already enabled.
        }
        self.tty_protocols_applied = true;
        set_tty_protocols_active(self.on_write, true);
    }

    /// Mark terminal modes as disabled.
    /// Return true if something was written to the tty.
    pub fn disable_tty_protocols(&mut self) {
        if !self.tty_protocols_applied {
            return; // Already disabled.
        };
        self.tty_protocols_applied = false;
        set_tty_protocols_active(self.on_write, false);
    }

    /// Transfer to the given job group, if it wants to own the terminal.
    #[allow(clippy::wrong_self_convention)]
    pub fn to_job_group(&mut self, jg: &JobGroupRef) {
        assert!(self.owner.is_none(), "Terminal already transferred");
        if Self::try_transfer(jg) {
            self.owner = Some(jg.clone());
        }
    }

    /// Reclaim the tty if we transferred it.
    /// Returns true if data was written to the tty, as part of
    /// re-enabling terminal protocols.
    pub fn reclaim(mut self) {
        self.reclaim_impl()
    }

    /// Release the tty, meaning no longer restore anything in Drop - similar to `mem::forget`.
    pub fn release(mut self) {
        self.reclaimed = true;
    }

    /// Implementation of reclaim, factored out for use in Drop.
    fn reclaim_impl(&mut self) {
        assert!(!self.reclaimed, "Terminal already reclaimed");
        self.reclaimed = true;
        if self.owner.is_some() {
            flog!(proc_pgroup, "fish reclaiming terminal");
            if unsafe { libc::tcsetpgrp(STDIN_FILENO, libc::getpgrp()) } == -1 {
                flog!(
                    warning,
                    "Could not return shell to foreground:",
                    errno::errno()
                );
                perror("tcsetpgrp");
            }
            self.owner = None;
        }
        // Restore the terminal protocols. Note this does nothing if they were unchanged.
        if self.tty_protocols_initial {
            self.enable_tty_protocols();
        } else {
            self.disable_tty_protocols();
        }
    }

    /// Save the current tty modes into the owning job group, if we are transferred.
    pub fn save_tty_modes(&mut self) {
        if let Some(ref mut owner) = self.owner {
            let mut tmodes = MaybeUninit::uninit();
            if unsafe { libc::tcgetattr(STDIN_FILENO, tmodes.as_mut_ptr()) } == 0 {
                owner.tmodes.replace(Some(unsafe { tmodes.assume_init() }));
            } else if errno::errno().0 != ENOTTY {
                perror("tcgetattr");
            }
        }
    }

    fn try_transfer(jg: &JobGroup) -> bool {
        if !jg.wants_terminal() {
            // The job doesn't want the terminal.
            return false;
        }

        // Get the pgid; we must have one if we want the terminal.
        let pgid = jg.get_pgid().unwrap();

        // It should never be fish's pgroup.
        let fish_pgrp = crate::nix::getpgrp();
        assert!(
            pgid.as_pid_t() != fish_pgrp,
            "Job should not have fish's pgroup"
        );

        // Ok, we want to transfer to the child.
        // Note it is important to be very careful about calling tcsetpgrp()!
        // fish ignores SIGTTOU which means that it has the power to reassign the tty even if it doesn't
        // own it. This means that other processes may get SIGTTOU and become zombies.
        // Check who own the tty now. There's four cases of interest:
        //   1. There is no tty at all (tcgetpgrp() returns -1). For example running from a pure script.
        //      Of course do not transfer it in that case.
        //   2. The tty is owned by the process. This comes about often, as the process will call
        //      tcsetpgrp() on itself between fork and exec. This is the essential race inherent in
        //      tcsetpgrp(). In this case we want to reclaim the tty, but do not need to transfer it
        //      ourselves since the child won the race.
        //   3. The tty is owned by a different process. This may come about if fish is running in the
        //      background with job control enabled. Do not transfer it.
        //   4. The tty is owned by fish. In that case we want to transfer the pgid.
        let current_owner = unsafe { libc::tcgetpgrp(STDIN_FILENO) };
        if current_owner < 0 {
            // Case 1.
            return false;
        } else if current_owner == pgid.get() {
            // Case 2.
            return true;
        } else if current_owner != pgid.get() && current_owner != fish_pgrp {
            // Case 3.
            return false;
        }
        // Case 4 - we do want to transfer it.

        // The tcsetpgrp(2) man page says that EPERM is thrown if "pgrp has a supported value, but
        // is not the process group ID of a process in the same session as the calling process."
        // Since we _guarantee_ that this isn't the case (the child calls setpgid before it calls
        // SIGSTOP, and the child was created in the same session as us), it seems that EPERM is
        // being thrown because of an caching issue - the call to tcsetpgrp isn't seeing the
        // newly-created process group just yet. On this developer's test machine (WSL running Linux
        // 4.4.0), EPERM does indeed disappear on retry. The important thing is that we can
        // guarantee the process isn't going to exit while we wait (which would cause us to possibly
        // block indefinitely).
        while unsafe { libc::tcsetpgrp(STDIN_FILENO, pgid.as_pid_t()) } != 0 {
            flogf!(proc_termowner, "tcsetpgrp failed: %d", errno::errno().0);

            // Before anything else, make sure that it's even necessary to call tcsetpgrp.
            // Since it usually _is_ necessary, we only check in case it fails so as to avoid the
            // unnecessary syscall and associated context switch, which profiling has shown to have
            // a significant cost when running process groups in quick succession.
            let getpgrp_res = unsafe { libc::tcgetpgrp(STDIN_FILENO) };
            if getpgrp_res < 0 {
                match errno::errno().0 {
                    ENOTTY => {
                        // stdin is not a tty. This may come about if job control is enabled but we are
                        // not a tty - see #6573.
                        return false;
                    }
                    _ => {
                        perror("tcgetpgrp");
                        return false;
                    }
                }
            }
            if getpgrp_res == pgid.get() {
                flogf!(
                    proc_termowner,
                    "Process group %d already has control of terminal",
                    pgid
                );
                return true;
            }

            let pgroup_terminated;
            if errno::errno().0 == EINVAL {
                // OS X returns EINVAL if the process group no longer lives. Probably other OSes,
                // too. Unlike EPERM below, EINVAL can only happen if the process group has
                // terminated.
                pgroup_terminated = true;
            } else if errno::errno().0 == EPERM {
                // Retry so long as this isn't because the process group is dead.
                let mut result: libc::c_int = 0;
                let wait_result = unsafe { libc::waitpid(-pgid.as_pid_t(), &mut result, WNOHANG) };
                if wait_result == -1 {
                    // Note that -1 is technically an "error" for waitpid in the sense that an
                    // invalid argument was specified because no such process group exists any
                    // longer. This is the observed behavior on Linux 4.4.0. a "success" result
                    // would mean processes from the group still exist but is still running in some
                    // state or the other.
                    pgroup_terminated = true;
                } else {
                    // Debug the original tcsetpgrp error (not the waitpid errno) to the log, and
                    // then retry until not EPERM or the process group has exited.
                    flogf!(
                        proc_termowner,
                        "terminal_give_to_job(): EPERM with pgid %d.",
                        pgid
                    );
                    continue;
                }
            } else if errno::errno().0 == ENOTTY {
                // stdin is not a TTY. In general we expect this to be caught via the tcgetpgrp
                // call's EBADF handler above.
                return false;
            } else {
                flogf!(
                    warning,
                    "Could not send job %d ('%s') with pgid %d to foreground",
                    jg.job_id.to_wstring(),
                    jg.command,
                    pgid
                );
                perror("tcsetpgrp");
                return false;
            }

            if pgroup_terminated {
                // All processes in the process group has exited.
                // Since we delay reaping any processes in a process group until all members of that
                // job/group have been started, the only way this can happen is if the very last
                // process in the group terminated and didn't need to access the terminal, otherwise
                // it would have hung waiting for terminal IO (SIGTTIN). We can safely ignore this.
                flogf!(
                    proc_termowner,
                    "tcsetpgrp called but process group %d has terminated.\n",
                    pgid
                );
                return false;
            }

            break;
        }
        true
    }
}

/// The destructor will assert if reclaim() has not been called.
impl Drop for TtyHandoff {
    fn drop(&mut self) {
        if !self.reclaimed {
            self.reclaim_impl();
        }
    }
}

// If we are running under iTerm2, get the version as a tuple of (major, minor, patch).
fn get_iterm2_version(xtversion: &wstr) -> Option<(u32, u32, u32)> {
    // TODO split_once
    let mut xtversion = xtversion.split(' ');
    let name = xtversion.next().unwrap();
    let version = xtversion.next()?;
    if name != "iTerm2" {
        return None;
    }
    let mut parts = version.split('.');
    Some((
        wcstoi(parts.next()?).ok()?,
        wcstoi(parts.next()?).ok()?,
        wcstoi(parts.next()?).ok()?,
    ))
}
