use crate::common::{get_by_sorted_name, shell_modes, str2wcstring, Named};
use crate::curses;
use crate::env::{EnvMode, Environment, CURSES_INITIALIZED};
use crate::event;
use crate::flog::FLOG;
use crate::input_common::{
    CharEvent, CharEventType, CharInputStyle, InputEventQueuer, ReadlineCmd, R_END_INPUT_FUNCTIONS,
};
use crate::parser::Parser;
use crate::proc::job_reap;
use crate::reader::{
    reader_reading_interrupted, reader_reset_interrupted, reader_schedule_prompt_repaint,
};
use crate::signal::signal_clear_cancel;
#[cfg(test)]
use crate::tests::prelude::*;
use crate::threads::assert_is_main_thread;
use crate::wchar::prelude::*;
use errno::{set_errno, Errno};
use once_cell::sync::{Lazy, OnceCell};
use std::cell::RefCell;
use std::collections::VecDeque;
use std::ffi::CString;
use std::os::fd::RawFd;
use std::rc::Rc;
use std::sync::{
    atomic::{AtomicU32, Ordering},
    Arc, Mutex, MutexGuard,
};

pub const FISH_BIND_MODE_VAR: &wstr = L!("fish_bind_mode");
pub const DEFAULT_BIND_MODE: &wstr = L!("default");

/// A name for our own key mapping for nul.
pub const NUL_MAPPING_NAME: &wstr = L!("nul");

#[derive(Debug, Clone)]
pub struct InputMappingName {
    pub seq: WString,
    pub mode: WString,
}

/// Struct representing a keybinding. Returned by input_get_mappings.
#[derive(Debug, Clone)]
struct InputMapping {
    /// Character sequence which generates this event.
    seq: WString,
    /// Commands that should be evaluated by this mapping.
    commands: Vec<WString>,
    /// We wish to preserve the user-specified order. This is just an incrementing value.
    specification_order: u32,
    /// Mode in which this command should be evaluated.
    mode: WString,
    /// New mode that should be switched to after command evaluation.
    /// TODO: should be an Option, instead of empty string to mean none.
    sets_mode: WString,
}

impl InputMapping {
    /// Create a new mapping.
    fn new(
        seq: WString,
        commands: Vec<WString>,
        mode: WString,
        sets_mode: WString,
    ) -> InputMapping {
        static LAST_INPUT_MAP_SPEC_ORDER: AtomicU32 = AtomicU32::new(0);
        let specification_order = 1 + LAST_INPUT_MAP_SPEC_ORDER.fetch_add(1, Ordering::Relaxed);
        InputMapping {
            seq,
            commands,
            specification_order,
            mode,
            sets_mode,
        }
    }

    /// \return true if this is a generic mapping, i.e. acts as a fallback.
    fn is_generic(&self) -> bool {
        self.seq.is_empty()
    }
}

/// A struct representing the mapping from a terminfo key name to a terminfo character sequence.
#[derive(Debug)]
struct TerminfoMapping {
    // name of key
    name: &'static wstr,

    // character sequence generated on keypress, or none if there was no mapping.
    seq: Option<Box<[u8]>>,
}

/// Input function metadata. This list should be kept in sync with the key code list in
/// input_common.rs.
struct InputFunctionMetadata {
    name: &'static wstr,
    code: ReadlineCmd,
}

impl Named for InputFunctionMetadata {
    fn name(&self) -> &'static wstr {
        self.name
    }
}

/// Helper to create a new InputFunctionMetadata struct.
const fn make_md(name: &'static wstr, code: ReadlineCmd) -> InputFunctionMetadata {
    InputFunctionMetadata { name, code }
}

/// A static mapping of all readline commands as strings to their ReadlineCmd equivalent.
/// Keep this list sorted alphabetically!
#[rustfmt::skip]
const INPUT_FUNCTION_METADATA: &[InputFunctionMetadata] = &[
    // NULL makes it unusable - this is specially inserted when we detect mouse input
    make_md(L!(""), ReadlineCmd::DisableMouseTracking),
    make_md(L!("accept-autosuggestion"), ReadlineCmd::AcceptAutosuggestion),
    make_md(L!("and"), ReadlineCmd::FuncAnd),
    make_md(L!("backward-bigword"), ReadlineCmd::BackwardBigword),
    make_md(L!("backward-char"), ReadlineCmd::BackwardChar),
    make_md(L!("backward-delete-char"), ReadlineCmd::BackwardDeleteChar),
    make_md(L!("backward-jump"), ReadlineCmd::BackwardJump),
    make_md(L!("backward-jump-till"), ReadlineCmd::BackwardJumpTill),
    make_md(L!("backward-kill-bigword"), ReadlineCmd::BackwardKillBigword),
    make_md(L!("backward-kill-line"), ReadlineCmd::BackwardKillLine),
    make_md(L!("backward-kill-path-component"), ReadlineCmd::BackwardKillPathComponent),
    make_md(L!("backward-kill-word"), ReadlineCmd::BackwardKillWord),
    make_md(L!("backward-word"), ReadlineCmd::BackwardWord),
    make_md(L!("begin-selection"), ReadlineCmd::BeginSelection),
    make_md(L!("begin-undo-group"), ReadlineCmd::BeginUndoGroup),
    make_md(L!("beginning-of-buffer"), ReadlineCmd::BeginningOfBuffer),
    make_md(L!("beginning-of-history"), ReadlineCmd::BeginningOfHistory),
    make_md(L!("beginning-of-line"), ReadlineCmd::BeginningOfLine),
    make_md(L!("cancel"), ReadlineCmd::Cancel),
    make_md(L!("cancel-commandline"), ReadlineCmd::CancelCommandline),
    make_md(L!("capitalize-word"), ReadlineCmd::CapitalizeWord),
    make_md(L!("clear-screen"), ReadlineCmd::ClearScreenAndRepaint),
    make_md(L!("complete"), ReadlineCmd::Complete),
    make_md(L!("complete-and-search"), ReadlineCmd::CompleteAndSearch),
    make_md(L!("delete-char"), ReadlineCmd::DeleteChar),
    make_md(L!("delete-or-exit"), ReadlineCmd::DeleteOrExit),
    make_md(L!("down-line"), ReadlineCmd::DownLine),
    make_md(L!("downcase-word"), ReadlineCmd::DowncaseWord),
    make_md(L!("end-of-buffer"), ReadlineCmd::EndOfBuffer),
    make_md(L!("end-of-history"), ReadlineCmd::EndOfHistory),
    make_md(L!("end-of-line"), ReadlineCmd::EndOfLine),
    make_md(L!("end-selection"), ReadlineCmd::EndSelection),
    make_md(L!("end-undo-group"), ReadlineCmd::EndUndoGroup),
    make_md(L!("execute"), ReadlineCmd::Execute),
    make_md(L!("exit"), ReadlineCmd::Exit),
    make_md(L!("expand-abbr"), ReadlineCmd::ExpandAbbr),
    make_md(L!("force-repaint"), ReadlineCmd::ForceRepaint),
    make_md(L!("forward-bigword"), ReadlineCmd::ForwardBigword),
    make_md(L!("forward-char"), ReadlineCmd::ForwardChar),
    make_md(L!("forward-jump"), ReadlineCmd::ForwardJump),
    make_md(L!("forward-jump-till"), ReadlineCmd::ForwardJumpTill),
    make_md(L!("forward-single-char"), ReadlineCmd::ForwardSingleChar),
    make_md(L!("forward-word"), ReadlineCmd::ForwardWord),
    make_md(L!("history-pager"), ReadlineCmd::HistoryPager),
    make_md(L!("history-pager-delete"), ReadlineCmd::HistoryPagerDelete),
    make_md(L!("history-prefix-search-backward"), ReadlineCmd::HistoryPrefixSearchBackward),
    make_md(L!("history-prefix-search-forward"), ReadlineCmd::HistoryPrefixSearchForward),
    make_md(L!("history-search-backward"), ReadlineCmd::HistorySearchBackward),
    make_md(L!("history-search-forward"), ReadlineCmd::HistorySearchForward),
    make_md(L!("history-token-search-backward"), ReadlineCmd::HistoryTokenSearchBackward),
    make_md(L!("history-token-search-forward"), ReadlineCmd::HistoryTokenSearchForward),
    make_md(L!("insert-line-over"), ReadlineCmd::InsertLineOver),
    make_md(L!("insert-line-under"), ReadlineCmd::InsertLineUnder),
    make_md(L!("kill-bigword"), ReadlineCmd::KillBigword),
    make_md(L!("kill-inner-line"), ReadlineCmd::KillInnerLine),
    make_md(L!("kill-line"), ReadlineCmd::KillLine),
    make_md(L!("kill-selection"), ReadlineCmd::KillSelection),
    make_md(L!("kill-whole-line"), ReadlineCmd::KillWholeLine),
    make_md(L!("kill-word"), ReadlineCmd::KillWord),
    make_md(L!("nextd-or-forward-word"), ReadlineCmd::NextdOrForwardWord),
    make_md(L!("or"), ReadlineCmd::FuncOr),
    make_md(L!("pager-toggle-search"), ReadlineCmd::PagerToggleSearch),
    make_md(L!("prevd-or-backward-word"), ReadlineCmd::PrevdOrBackwardWord),
    make_md(L!("redo"), ReadlineCmd::Redo),
    make_md(L!("repaint"), ReadlineCmd::Repaint),
    make_md(L!("repaint-mode"), ReadlineCmd::RepaintMode),
    make_md(L!("repeat-jump"), ReadlineCmd::RepeatJump),
    make_md(L!("repeat-jump-reverse"), ReadlineCmd::ReverseRepeatJump),
    make_md(L!("self-insert"), ReadlineCmd::SelfInsert),
    make_md(L!("self-insert-notfirst"), ReadlineCmd::SelfInsertNotFirst),
    make_md(L!("suppress-autosuggestion"), ReadlineCmd::SuppressAutosuggestion),
    make_md(L!("swap-selection-start-stop"), ReadlineCmd::SwapSelectionStartStop),
    make_md(L!("togglecase-char"), ReadlineCmd::TogglecaseChar),
    make_md(L!("togglecase-selection"), ReadlineCmd::TogglecaseSelection),
    make_md(L!("transpose-chars"), ReadlineCmd::TransposeChars),
    make_md(L!("transpose-words"), ReadlineCmd::TransposeWords),
    make_md(L!("undo"), ReadlineCmd::Undo),
    make_md(L!("up-line"), ReadlineCmd::UpLine),
    make_md(L!("upcase-word"), ReadlineCmd::UpcaseWord),
    make_md(L!("yank"), ReadlineCmd::Yank),
    make_md(L!("yank-pop"), ReadlineCmd::YankPop),
];
assert_sorted_by_name!(INPUT_FUNCTION_METADATA);

const fn _assert_sizes_match() {
    let input_function_count = R_END_INPUT_FUNCTIONS;
    assert!(
        INPUT_FUNCTION_METADATA.len() == input_function_count,
        concat!(
            "input_function_metadata size mismatch with input_common. ",
            "Did you forget to update input_function_metadata?"
        )
    );
}
const _: () = _assert_sizes_match();

// Keep this function for debug purposes
// See 031b265
#[allow(dead_code)]
pub fn describe_char(c: i32) -> WString {
    if c > 0 && (c as usize) < R_END_INPUT_FUNCTIONS {
        return sprintf!("%02x (%ls)", c, INPUT_FUNCTION_METADATA[c as usize].name);
    }
    return sprintf!("%02x", c);
}

/// The input mapping set is the set of mappings from character sequences to commands.
#[derive(Debug, Default)]
pub struct InputMappingSet {
    mapping_list: Vec<InputMapping>,
    preset_mapping_list: Vec<InputMapping>,
    all_mappings_cache: RefCell<Option<Arc<Box<[InputMapping]>>>>,
}

/// Access the singleton input mapping set.
pub fn input_mappings() -> MutexGuard<'static, InputMappingSet> {
    static INPUT_MAPPINGS: Lazy<Mutex<InputMappingSet>> =
        Lazy::new(|| Mutex::new(InputMappingSet::default()));
    INPUT_MAPPINGS.lock().unwrap()
}

/// Terminfo map list.
static TERMINFO_MAPPINGS: OnceCell<Box<[TerminfoMapping]>> = OnceCell::new();

/// Return the current bind mode.
fn input_get_bind_mode(vars: &dyn Environment) -> WString {
    if let Some(mode) = vars.get(FISH_BIND_MODE_VAR) {
        mode.as_string()
    } else {
        DEFAULT_BIND_MODE.to_owned()
    }
}

/// Set the current bind mode.
fn input_set_bind_mode(parser: &Parser, bm: &wstr) {
    // Only set this if it differs to not execute variable handlers all the time.
    // modes may not be empty - empty is a sentinel value meaning to not change the mode
    assert!(!bm.is_empty());
    if input_get_bind_mode(parser.vars()) != bm {
        // Must send events here - see #6653.
        parser.set_var_and_fire(FISH_BIND_MODE_VAR, EnvMode::GLOBAL, vec![bm.to_owned()]);
    }
}

/// Returns the arity of a given input function.
fn input_function_arity(function: ReadlineCmd) -> usize {
    match function {
        ReadlineCmd::ForwardJump
        | ReadlineCmd::BackwardJump
        | ReadlineCmd::ForwardJumpTill
        | ReadlineCmd::BackwardJumpTill => 1,
        _ => 0,
    }
}

/// Inserts an input mapping at the correct position. We sort them in descending order by length, so
/// that we test longer sequences first.
fn input_mapping_insert_sorted(ml: &mut Vec<InputMapping>, new_mapping: InputMapping) {
    let new_mapping_len = new_mapping.seq.len();
    let pos = ml
        .binary_search_by(|m| m.seq.len().cmp(&new_mapping_len).reverse())
        .unwrap_or_else(|e| e);
    ml.insert(pos, new_mapping);
}

impl InputMappingSet {
    /// Adds an input mapping.
    pub fn add(
        &mut self,
        sequence: WString,
        commands: Vec<WString>,
        mode: WString,
        sets_mode: WString,
        user: bool,
    ) {
        // Clear cached mappings.
        self.all_mappings_cache = RefCell::new(None);

        // Update any existing mapping with this sequence.
        // FIXME: this makes adding multiple bindings quadratic.
        let ml = if user {
            &mut self.mapping_list
        } else {
            &mut self.preset_mapping_list
        };
        for m in ml.iter_mut() {
            if m.seq == sequence && m.mode == mode {
                m.commands = commands;
                m.sets_mode = sets_mode;
                return;
            }
        }

        // Add a new mapping, using the next order.
        let new_mapping = InputMapping::new(sequence, commands, mode, sets_mode);
        input_mapping_insert_sorted(ml, new_mapping);
    }

    // Like add(), but takes a single command.
    pub fn add1(
        &mut self,
        sequence: WString,
        command: WString,
        mode: WString,
        sets_mode: WString,
        user: bool,
    ) {
        self.add(sequence, vec![command], mode, sets_mode, user);
    }
}

/// Set up arrays used by readch to detect escape sequences for special keys and perform related
/// initializations for our input subsystem.
pub fn init_input() {
    assert_is_main_thread();
    if TERMINFO_MAPPINGS.get().is_some() {
        return;
    }
    TERMINFO_MAPPINGS.set(create_input_terminfo()).unwrap();

    let mut input_mapping = input_mappings();

    // If we have no keybindings, add a few simple defaults.
    if input_mapping.preset_mapping_list.is_empty() {
        // Helper for adding.
        let mut add = |seq: &str, cmd: &str| {
            let mode = DEFAULT_BIND_MODE.to_owned();
            let sets_mode = DEFAULT_BIND_MODE.to_owned();
            input_mapping.add1(seq.into(), cmd.into(), mode, sets_mode, false);
        };

        add("", "self-insert");
        add("\n", "execute");
        add("\r", "execute");
        add("\t", "complete");
        add("\x03", "cancel-commandline");
        add("\x04", "exit");
        add("\x05", "bind");
        // ctrl-s
        add("\x13", "pager-toggle-search");
        // ctrl-u
        add("\x15", "backward-kill-line");
        // del/backspace
        add("\x7f", "backward-delete-char");
        // Arrows - can't have functions, so *-or-search isn't available.
        add("\x1B[A", "up-line");
        add("\x1B[B", "down-line");
        add("\x1B[C", "forward-char");
        add("\x1B[D", "backward-char");
        // emacs-style ctrl-p/n/b/f
        add("\x10", "up-line");
        add("\x0e", "down-line");
        add("\x02", "backward-char");
        add("\x06", "forward-char");
    }
}

/// CommandHandler is used to run commands. When a character is encountered that
/// would invoke a fish command, it is unread and CharEventType::CheckExit is returned.
/// Note the handler is not stored.
pub type CommandHandler<'a> = dyn FnMut(&[WString]) + 'a;

pub struct Inputter {
    in_fd: RawFd,
    queue: VecDeque<CharEvent>,
    // We need a parser to evaluate bindings.
    parser: Rc<Parser>,
    input_function_args: Vec<char>,
    function_status: bool,

    // Transient storage to avoid repeated allocations.
    event_storage: Vec<CharEvent>,
}

impl InputEventQueuer for Inputter {
    fn get_queue(&self) -> &VecDeque<CharEvent> {
        &self.queue
    }

    fn get_queue_mut(&mut self) -> &mut VecDeque<CharEvent> {
        &mut self.queue
    }

    /// Return the fd corresponding to stdin.
    fn get_in_fd(&self) -> RawFd {
        self.in_fd
    }

    fn prepare_to_select(&mut self) {
        // Fire any pending events and reap stray processes, including printing exit status messages.
        event::fire_delayed(&self.parser);
        if job_reap(&self.parser, true) {
            reader_schedule_prompt_repaint();
        }
    }

    fn select_interrupted(&mut self) {
        // Readline commands may be bound to \cc which also sets the cancel flag.
        // See #6937, #8125.
        signal_clear_cancel();

        // Fire any pending events and reap stray processes, including printing exit status messages.
        let parser = &self.parser;
        event::fire_delayed(parser);
        if job_reap(parser, true) {
            reader_schedule_prompt_repaint();
        }

        // Tell the reader an event occurred.
        if reader_reading_interrupted() != 0 {
            let vintr = shell_modes().c_cc[libc::VINTR];
            if vintr != 0 {
                self.push_front(CharEvent::from_char(vintr.into()));
            }
            return;
        }
        self.push_front(CharEvent::from_check_exit());
    }

    fn uvar_change_notified(&mut self) {
        self.parser.sync_uvars_and_fire(true /* always */);
    }
}

impl Inputter {
    /// Construct from a parser, and the fd from which to read.
    pub fn new(parser: Rc<Parser>, in_fd: RawFd) -> Inputter {
        Inputter {
            in_fd,
            queue: VecDeque::new(),
            parser,
            input_function_args: Vec::new(),
            function_status: false,
            event_storage: Vec::new(),
        }
    }

    fn function_push_arg(&mut self, arg: char) {
        self.input_function_args.push(arg);
    }

    pub fn function_pop_arg(&mut self) -> char {
        self.input_function_args
            .pop()
            .expect("function_pop_arg underflow")
    }

    fn function_push_args(&mut self, code: ReadlineCmd) {
        let arity = input_function_arity(code);
        assert!(
            self.event_storage.is_empty(),
            "event_storage should be empty"
        );
        let mut skipped = std::mem::take(&mut self.event_storage);
        for _ in 0..arity {
            // Skip and queue up any function codes. See issue #2357.
            let arg: char;
            loop {
                let evt = self.readch();
                if evt.is_char() {
                    arg = evt.get_char();
                    break;
                }
                skipped.push(evt);
            }
            self.function_push_arg(arg);
        }

        // Push the function codes back into the input stream.
        self.insert_front(skipped.drain(..));
        self.event_storage = skipped;
        self.event_storage.clear();
    }

    /// Perform the action of the specified binding. allow_commands controls whether fish commands
    /// should be executed, or should be deferred until later.
    fn mapping_execute(
        &mut self,
        m: &InputMapping,
        command_handler: &mut Option<&mut CommandHandler>,
    ) {
        // has_functions: there are functions that need to be put on the input queue
        // has_commands: there are shell commands that need to be evaluated
        let mut has_commands = false;
        let mut has_functions = false;
        for cmd in &m.commands {
            if input_function_get_code(cmd).is_some() {
                has_functions = true;
            } else {
                has_commands = true;
            }
            if has_functions && has_commands {
                break;
            }
        }

        // !has_functions && !has_commands: only set bind mode
        if !has_commands && !has_functions {
            if !m.sets_mode.is_empty() {
                input_set_bind_mode(&self.parser, &m.sets_mode);
            }
            return;
        }

        if has_commands && command_handler.is_none() {
            // We don't want to run commands yet. Put the characters back and return check_exit.
            self.insert_front(m.seq.chars().map(CharEvent::from_char));
            self.push_front(CharEvent::from_check_exit());
            return; // skip the input_set_bind_mode
        } else if has_functions && !has_commands {
            // Functions are added at the head of the input queue.
            for cmd in m.commands.iter().rev() {
                let code = input_function_get_code(cmd).unwrap();
                self.function_push_args(code);
                self.push_front(CharEvent::from_readline_seq(code, m.seq.clone()));
            }
        } else if has_commands && !has_functions {
            // Execute all commands.
            //
            // FIXME(snnw): if commands add stuff to input queue (e.g. commandline -f execute), we won't
            // see that until all other commands have also been run.
            let command_handler = command_handler.as_mut().unwrap();
            command_handler(&m.commands);
            self.push_front(CharEvent::from_check_exit());
        } else {
            // Invalid binding, mixed commands and functions.  We would need to execute these one by
            // one.
            self.push_front(CharEvent::from_check_exit());
        }
        // Empty bind mode indicates to not reset the mode (#2871)
        if !m.sets_mode.is_empty() {
            input_set_bind_mode(&self.parser, &m.sets_mode);
        }
    }

    /// Enqueue a char event to the queue of unread characters that input_readch will return before
    /// actually reading from fd 0.
    pub fn queue_char(&mut self, ch: CharEvent) {
        if ch.is_readline() {
            self.function_push_args(ch.get_readline());
        }
        self.queue.push_back(ch);
    }

    /// Enqueue a readline command. Convenience cover over queue_char().
    pub fn queue_readline(&mut self, cmd: ReadlineCmd) {
        self.queue_char(CharEvent::from_readline(cmd));
    }

    /// Sets the return status of the most recently executed input function.
    pub fn function_set_status(&mut self, status: bool) {
        self.function_status = status;
    }
}

/// A struct which allows accumulating input events, or returns them to the queue.
/// This contains a list of events which have been dequeued, and a current index into that list.
struct EventQueuePeeker<'q> {
    /// The list of events which have been dequeued.
    peeked: Vec<CharEvent>,

    /// If set, then some previous timed event timed out.
    had_timeout: bool,

    /// The current index. This never exceeds peeked.len().
    idx: usize,

    /// The queue from which to read more events.
    event_queue: &'q mut Inputter,
}

impl EventQueuePeeker<'_> {
    fn new(event_queue: &mut Inputter) -> EventQueuePeeker {
        EventQueuePeeker {
            peeked: Vec::new(),
            had_timeout: false,
            idx: 0,
            event_queue,
        }
    }

    /// \return the next event.
    fn next(&mut self) -> CharEvent {
        assert!(
            self.idx <= self.peeked.len(),
            "Index must not be larger than dequeued event count"
        );
        if self.idx == self.peeked.len() {
            let event = self.event_queue.readch();
            self.peeked.push(event);
        }
        let res = self.peeked[self.idx].clone();
        self.idx += 1;
        res
    }

    /// Check if the next event is the given character. This advances the index on success only.
    /// If \p escaped is set, then return false if this (or any other) character had a timeout.
    fn next_is_char(&mut self, c: char, escaped: bool) -> bool {
        assert!(
            self.idx <= self.peeked.len(),
            "Index must not be larger than dequeued event count"
        );
        // See if we had a timeout already.
        if escaped && self.had_timeout {
            return false;
        }
        // Grab a new event if we have exhausted what we have already peeked.
        // Use either readch or readch_timed, per our param.
        if self.idx == self.peeked.len() {
            let newevt: CharEvent;
            if !escaped {
                if let Some(mevt) = self.event_queue.readch_timed_sequence_key() {
                    newevt = mevt;
                } else {
                    self.had_timeout = true;
                    return false;
                }
            } else if let Some(mevt) = self.event_queue.readch_timed_esc() {
                newevt = mevt;
            } else {
                self.had_timeout = true;
                return false;
            }
            self.peeked.push(newevt);
        }
        // Now we have peeked far enough; check the event.
        // If it matches the char, then increment the index.
        if self.peeked[self.idx].maybe_char() == Some(c) {
            self.idx += 1;
            return true;
        }
        false
    }

    /// \return the current index.
    fn len(&self) -> usize {
        self.idx
    }

    /// Consume all events up to the current index.
    /// Remaining events are returned to the queue.
    fn consume(mut self) {
        // Note this deliberately takes 'self' by value.
        self.event_queue.insert_front(self.peeked.drain(self.idx..));
        self.peeked.clear();
        self.idx = 0;
    }

    /// Test if any of our peeked events are readline or check_exit.
    fn char_sequence_interrupted(&self) -> bool {
        self.peeked
            .iter()
            .any(|evt| evt.is_readline() || evt.is_check_exit())
    }

    /// Reset our index back to 0.
    fn restart(&mut self) {
        self.idx = 0;
    }
}

impl Drop for EventQueuePeeker<'_> {
    fn drop(&mut self) {
        assert!(
            self.idx == 0,
            "Events left on the queue - missing restart or consume?",
        );
        self.event_queue.insert_front(self.peeked.drain(self.idx..));
    }
}

/// Try reading a mouse-tracking CSI sequence, using the given \p peeker.
/// Events are left on the peeker and the caller must restart or consume it.
/// \return true if matched, false if not.
fn have_mouse_tracking_csi(peeker: &mut EventQueuePeeker) -> bool {
    // Maximum length of any CSI is NPAR (which is nominally 16), although this does not account for
    // user input intermixed with pseudo input generated by the tty emulator.
    // Check for the CSI first.
    if !peeker.next_is_char('\x1b', false) || !peeker.next_is_char('[', true /* escaped */) {
        return false;
    }

    let mut next = peeker.next().maybe_char();
    let length;
    if next == Some('M') {
        // Generic X10 or modified VT200 sequence. It doesn't matter which, they're both 6 chars
        // (although in mode 1005, the characters may be unicode and not necessarily just one byte
        // long) reporting the button that was clicked and its location.
        length = 6;
    } else if next == Some('<') {
        // Extended (SGR/1006) mouse reporting mode, with semicolon-separated parameters for button
        // code, Px, and Py, ending with 'M' for button press or 'm' for button release.
        loop {
            next = peeker.next().maybe_char();
            if next == Some('M') || next == Some('m') {
                // However much we've read, we've consumed the CSI in its entirety.
                length = peeker.len();
                break;
            }
            if peeker.len() >= 16 {
                // This is likely a malformed mouse-reporting CSI but we can't do anything about it.
                return false;
            }
        }
    } else if next == Some('t') {
        // VT200 button released in mouse highlighting mode at valid text location. 5 chars.
        length = 5;
    } else if next == Some('T') {
        // VT200 button released in mouse highlighting mode past end-of-line. 9 characters.
        length = 9;
    } else {
        return false;
    }

    // Consume however many characters it takes to prevent the mouse tracking sequence from reaching
    // the prompt, dependent on the class of mouse reporting as detected above.
    while peeker.len() < length {
        let _ = peeker.next();
    }
    true
}

/// \return true if a given \p peeker matches a given sequence of char events given by \p str.
fn try_peek_sequence(peeker: &mut EventQueuePeeker, str: &wstr) -> bool {
    assert!(!str.is_empty(), "Empty string passed to try_peek_sequence");
    let mut prev = '\0';
    for c in str.chars() {
        // If we just read an escape, we need to add a timeout for the next char,
        // to distinguish between the actual escape key and an "alt"-modifier.
        let escaped = prev == '\x1B';
        if !peeker.next_is_char(c, escaped) {
            return false;
        }
        prev = c;
    }
    true
}

/// \return the first mapping that matches, walking first over the user's mapping list, then the
/// preset list.
/// \return none if nothing matches, or if we may have matched a longer sequence but it was
/// interrupted by a readline event.
impl Inputter {
    fn find_mapping(vars: &dyn Environment, peeker: &mut EventQueuePeeker) -> Option<InputMapping> {
        let mut generic: Option<&InputMapping> = None;
        let bind_mode = input_get_bind_mode(vars);
        let mut escape: Option<&InputMapping> = None;

        let ml = input_mappings().all_mappings();
        for m in ml.iter() {
            if m.mode != bind_mode {
                continue;
            }

            // Defer generic mappings until the end.
            if m.is_generic() {
                if generic.is_none() {
                    generic = Some(m);
                }
                continue;
            }

            if try_peek_sequence(peeker, &m.seq) {
                // A binding for just escape should also be deferred
                // so escape sequences take precedence.
                if m.seq == "\x1B" {
                    if escape.is_none() {
                        escape = Some(m);
                    }
                } else {
                    return Some(m.clone());
                }
            }
            peeker.restart();
        }
        if peeker.char_sequence_interrupted() {
            // We might have matched a longer sequence, but we were interrupted, e.g. by a signal.
            FLOG!(reader, "torn sequence, rearranging events");
            return None;
        }

        if escape.is_some() {
            // We need to reconsume the escape.
            peeker.next();
            return escape.cloned();
        }

        if generic.is_some() {
            generic.cloned()
        } else {
            None
        }
    }

    fn mapping_execute_matching_or_generic(
        &mut self,
        command_handler: &mut Option<&mut CommandHandler>,
    ) {
        let vars = self.parser.vars_ref();
        let mut peeker = EventQueuePeeker::new(self);
        // Check for mouse-tracking CSI before mappings to prevent the generic mapping handler from
        // taking over.
        if have_mouse_tracking_csi(&mut peeker) {
            // fish recognizes but does not actually support mouse reporting. We never turn it on, and
            // it's only ever enabled if a program we spawned enabled it and crashed or forgot to turn
            // it off before exiting. We swallow the events to prevent garbage from piling up at the
            // prompt, but don't do anything further with the received codes. To prevent this from
            // breaking user interaction with the tty emulator, wasting CPU, and adding latency to the
            // event queue, we turn off mouse reporting here.
            //
            // Since this is only called when we detect an incoming mouse reporting payload, we know the
            // terminal emulator supports the xterm ANSI extensions for mouse reporting and can safely
            // issue this without worrying about termcap.
            FLOG!(reader, "Disabling mouse tracking");

            // We can't/shouldn't directly manipulate stdout from `input.cpp`, so request the execution
            // of a helper function to disable mouse tracking.
            // writembs(outputter_t::stdoutput(), "\x1B[?1000l");
            peeker.consume();
            self.push_front(CharEvent::from_readline(ReadlineCmd::DisableMouseTracking));
            return;
        }
        peeker.restart();

        // Check for ordinary mappings.
        if let Some(mapping) = Self::find_mapping(&*vars, &mut peeker) {
            peeker.consume();
            self.mapping_execute(&mapping, command_handler);
            return;
        }
        peeker.restart();

        if peeker.char_sequence_interrupted() {
            // This may happen if we received a signal in the middle of an escape sequence or other
            // multi-char binding. Move these non-char events to the front of the queue, handle them
            // first, and then later we'll return and try the sequence again. See #8628.
            peeker.consume();
            self.promote_interruptions_to_front();
            return;
        }

        FLOG!(reader, "no generic found, ignoring char...");
        let _ = peeker.next();
        peeker.consume();
    }

    /// Helper function. Picks through the queue of incoming characters until we get to one that's not a
    /// readline function.
    fn read_characters_no_readline(&mut self) -> CharEvent {
        assert!(
            self.event_storage.is_empty(),
            "event_storage should be empty"
        );
        let mut saved_events = std::mem::take(&mut self.event_storage);

        let evt_to_return: CharEvent;
        loop {
            let evt = self.readch();
            if evt.is_readline() {
                saved_events.push(evt);
            } else {
                evt_to_return = evt;
                break;
            }
        }

        // Restore any readline functions
        self.insert_front(saved_events.drain(..));
        self.event_storage = saved_events;
        self.event_storage.clear();
        evt_to_return
    }

    /// Read a character from stdin. Try to convert some escape sequences into character constants,
    /// but do not permanently block the escape character.
    ///
    /// This is performed in the same way vim does it, i.e. if an escape character is read, wait for
    /// more input for a short time (a few milliseconds). If more input is available, it is assumed
    /// to be an escape sequence for a special character (such as an arrow key), and readch attempts
    /// to parse it. If no more input follows after the escape key, it is assumed to be an actual
    /// escape key press, and is returned as such.
    ///
    /// \p command_handler is used to run commands. If empty (in the std::function sense), when a
    /// character is encountered that would invoke a fish command, it is unread and
    /// char_event_type_t::check_exit is returned. Note the handler is not stored.
    pub fn read_char(&mut self, mut command_handler: Option<&mut CommandHandler>) -> CharEvent {
        // Clear the interrupted flag.
        reader_reset_interrupted();

        // Search for sequence in mapping tables.
        loop {
            let evt = self.readch();
            match evt.evt {
                CharEventType::Readline(cmd) => match cmd {
                    ReadlineCmd::SelfInsert | ReadlineCmd::SelfInsertNotFirst => {
                        // Typically self-insert is generated by the generic (empty) binding.
                        // However if it is generated by a real sequence, then insert that sequence.
                        let seq = evt.seq.chars().map(CharEvent::from_char);
                        self.insert_front(seq);
                        // Issue #1595: ensure we only insert characters, not readline functions. The
                        // common case is that this will be empty.
                        let mut res = self.read_characters_no_readline();

                        // Hackish: mark the input style.
                        res.input_style = if cmd == ReadlineCmd::SelfInsertNotFirst {
                            CharInputStyle::NotFirst
                        } else {
                            CharInputStyle::Normal
                        };
                        return res;
                    }
                    ReadlineCmd::FuncAnd | ReadlineCmd::FuncOr => {
                        // If previous function has bad status, we want to skip all functions that
                        // follow us.
                        // TODO: this line is too tricky.
                        if (cmd == ReadlineCmd::FuncAnd) != self.function_status {
                            self.drop_leading_readline_events();
                        }
                    }
                    _ => {
                        return evt;
                    }
                },
                CharEventType::Eof => {
                    // If we have EOF, we need to immediately quit.
                    // There's no need to go through the input functions.
                    return evt;
                }
                CharEventType::CheckExit => {
                    // Allow the reader to check for exit conditions.
                    return evt;
                }
                CharEventType::Char(_) => {
                    self.push_front(evt);
                    self.mapping_execute_matching_or_generic(&mut command_handler);
                    // Regarding allow_commands, we're in a loop, but if a fish command is executed,
                    // check_exit is unread, so the next pass through the loop we'll break out and return
                    // it.
                }
            }
        }
    }
}

impl InputMappingSet {
    /// Returns all mapping names and modes.
    pub fn get_names(&self, user: bool) -> Vec<InputMappingName> {
        // Sort the mappings by the user specification order, so we can return them in the same order
        // that the user specified them in.
        let mut local_list = if user {
            self.mapping_list.clone()
        } else {
            self.preset_mapping_list.clone()
        };
        local_list.sort_unstable_by_key(|m| m.specification_order);
        local_list
            .into_iter()
            .map(|m| InputMappingName {
                seq: m.seq,
                mode: m.mode,
            })
            .collect()
    }

    /// Erase all bindings.
    pub fn clear(&mut self, mode: Option<&wstr>, user: bool) {
        // Clear cached mappings.
        self.all_mappings_cache = RefCell::new(None);

        let ml = if user {
            &mut self.mapping_list
        } else {
            &mut self.preset_mapping_list
        };
        let should_erase = |m: &InputMapping| mode.is_none() || mode.unwrap() == m.mode;
        ml.retain(|m| !should_erase(m));
    }

    /// Erase binding for specified key sequence.
    pub fn erase(&mut self, sequence: &wstr, mode: &wstr, user: bool) -> bool {
        // Clear cached mappings.
        self.all_mappings_cache = RefCell::new(None);

        let ml = if user {
            &mut self.mapping_list
        } else {
            &mut self.preset_mapping_list
        };
        let mut result = false;
        for (idx, m) in ml.iter().enumerate() {
            if m.seq == sequence && m.mode == mode {
                ml.remove(idx);
                result = true;
                break;
            }
        }
        result
    }

    /// Gets the command bound to the specified key sequence in the specified mode. Returns true if
    /// it exists, false if not.
    pub fn get(
        &self,
        sequence: &wstr,
        mode: &wstr,
        out_cmds: &mut Vec<WString>,
        user: bool,
        out_sets_mode: &mut WString,
    ) -> bool {
        let mut result = false;
        let ml = if user {
            &self.mapping_list
        } else {
            &self.preset_mapping_list
        };
        for m in ml {
            if m.seq == sequence && m.mode == mode {
                *out_cmds = m.commands.clone();
                *out_sets_mode = m.sets_mode.clone();
                result = true;
                break;
            }
        }
        result
    }

    /// \return a snapshot of the list of input mappings.
    fn all_mappings(&self) -> Arc<Box<[InputMapping]>> {
        // Populate the cache if needed.
        let mut cache = self.all_mappings_cache.borrow_mut();
        if cache.is_none() {
            let mut all_mappings =
                Vec::with_capacity(self.mapping_list.len() + self.preset_mapping_list.len());
            all_mappings.extend(self.mapping_list.iter().cloned());
            all_mappings.extend(self.preset_mapping_list.iter().cloned());
            *cache = Some(Arc::new(all_mappings.into_boxed_slice()));
        }
        Arc::clone(cache.as_ref().unwrap())
    }
}

/// Create a list of terminfo mappings.
fn create_input_terminfo() -> Box<[TerminfoMapping]> {
    assert!(CURSES_INITIALIZED.load(Ordering::Relaxed));
    let Some(term) = curses::term() else {
        // setupterm() failed so we can't reference any key definitions.
        return Box::new([]);
    };

    // Helper to convert an Option<CString> to an Option<Box<[u8]>>.
    // The nul-terminator is NOT included.
    fn opt_cstr_to_bytes(opt: &Option<CString>) -> Option<Box<[u8]>> {
        opt.clone().map(|s| s.into_bytes().into())
    }

    macro_rules! terminfo_add {
        ($key:ident) => {
            TerminfoMapping {
                name: &L!(stringify!($key))[4..],
                seq: opt_cstr_to_bytes(&term.$key),
            }
        };
    }
    #[rustfmt::skip]
    return Box::new([
        terminfo_add!(key_a1), terminfo_add!(key_a3), terminfo_add!(key_b2),
        terminfo_add!(key_backspace), terminfo_add!(key_beg), terminfo_add!(key_btab),
        terminfo_add!(key_c1), terminfo_add!(key_c3), terminfo_add!(key_cancel),
        terminfo_add!(key_catab), terminfo_add!(key_clear), terminfo_add!(key_close),
        terminfo_add!(key_command), terminfo_add!(key_copy), terminfo_add!(key_create),
        terminfo_add!(key_ctab), terminfo_add!(key_dc), terminfo_add!(key_dl), terminfo_add!(key_down),
        terminfo_add!(key_eic), terminfo_add!(key_end), terminfo_add!(key_enter),
        terminfo_add!(key_eol), terminfo_add!(key_eos), terminfo_add!(key_exit), terminfo_add!(key_f0),
        terminfo_add!(key_f1), terminfo_add!(key_f2), terminfo_add!(key_f3), terminfo_add!(key_f4),
        terminfo_add!(key_f5), terminfo_add!(key_f6), terminfo_add!(key_f7), terminfo_add!(key_f8),
        terminfo_add!(key_f9), terminfo_add!(key_f10), terminfo_add!(key_f11), terminfo_add!(key_f12),
        terminfo_add!(key_f13), terminfo_add!(key_f14), terminfo_add!(key_f15), terminfo_add!(key_f16),
        terminfo_add!(key_f17), terminfo_add!(key_f18), terminfo_add!(key_f19), terminfo_add!(key_f20),
        // Note key_f21 through key_f63 are available but no actual keyboard supports them.
        terminfo_add!(key_find), terminfo_add!(key_help), terminfo_add!(key_home),
        terminfo_add!(key_ic), terminfo_add!(key_il), terminfo_add!(key_left), terminfo_add!(key_ll),
        terminfo_add!(key_mark), terminfo_add!(key_message), terminfo_add!(key_move),
        terminfo_add!(key_next), terminfo_add!(key_npage), terminfo_add!(key_open),
        terminfo_add!(key_options), terminfo_add!(key_ppage), terminfo_add!(key_previous),
        terminfo_add!(key_print), terminfo_add!(key_redo), terminfo_add!(key_reference),
        terminfo_add!(key_refresh), terminfo_add!(key_replace), terminfo_add!(key_restart),
        terminfo_add!(key_resume), terminfo_add!(key_right), terminfo_add!(key_save),
        terminfo_add!(key_sbeg), terminfo_add!(key_scancel), terminfo_add!(key_scommand),
        terminfo_add!(key_scopy), terminfo_add!(key_screate), terminfo_add!(key_sdc),
        terminfo_add!(key_sdl), terminfo_add!(key_select), terminfo_add!(key_send),
        terminfo_add!(key_seol), terminfo_add!(key_sexit), terminfo_add!(key_sf),
        terminfo_add!(key_sfind), terminfo_add!(key_shelp), terminfo_add!(key_shome),
        terminfo_add!(key_sic), terminfo_add!(key_sleft), terminfo_add!(key_smessage),
        terminfo_add!(key_smove), terminfo_add!(key_snext), terminfo_add!(key_soptions),
        terminfo_add!(key_sprevious), terminfo_add!(key_sprint), terminfo_add!(key_sr),
        terminfo_add!(key_sredo), terminfo_add!(key_sreplace), terminfo_add!(key_sright),
        terminfo_add!(key_srsume), terminfo_add!(key_ssave), terminfo_add!(key_ssuspend),
        terminfo_add!(key_stab), terminfo_add!(key_sundo), terminfo_add!(key_suspend),
        terminfo_add!(key_undo), terminfo_add!(key_up),

        // We introduce our own name for the string containing only the nul character - see
        // #3189. This can typically be generated via control-space.
        TerminfoMapping { name: NUL_MAPPING_NAME, seq: Some(Box::new([0])) },
    ]);
}

/// Return the sequence for the terminfo variable of the specified name.
///
/// If no terminfo variable of the specified name could be found, return false and set errno to
/// ENOENT. If the terminfo variable does not have a value, return false and set errno to EILSEQ.
pub fn input_terminfo_get_sequence(name: &wstr, out_seq: &mut WString) -> bool {
    // TODO: stop using errno for this.
    let mappings = TERMINFO_MAPPINGS
        .get()
        .expect("TERMINFO_MAPPINGS not initialized");
    for m in mappings.iter() {
        if name == m.name {
            // Found the mapping.
            if m.seq.is_none() {
                set_errno(Errno(libc::EILSEQ));
                return false;
            } else {
                *out_seq = str2wcstring(m.seq.as_ref().unwrap());
                return true;
            }
        }
    }
    set_errno(Errno(libc::ENOENT));
    false
}

/// Return the name of the terminfo variable with the specified sequence.
pub fn input_terminfo_get_name(seq: &wstr) -> Option<WString> {
    let mappings = TERMINFO_MAPPINGS
        .get()
        .expect("TERMINFO_MAPPINGS not initialized");
    for m in mappings.iter() {
        if m.seq.is_some() && seq == str2wcstring(m.seq.as_ref().unwrap()) {
            return Some(m.name.to_owned());
        }
    }
    None
}

/// Return a list of all known terminfo names.
pub fn input_terminfo_get_names(skip_null: bool) -> Vec<WString> {
    let mappings = TERMINFO_MAPPINGS
        .get()
        .expect("TERMINFO_MAPPINGS not initialized");
    let mut result = Vec::with_capacity(mappings.len());
    for m in mappings.iter() {
        if skip_null && m.seq.is_none() {
            continue;
        }
        result.push(m.name.to_owned());
    }
    result
}

/// Returns a list of all existing input function names.
pub fn input_function_get_names() -> Vec<&'static wstr> {
    // Note: the C++ cached this, but we don't to save memory.
    INPUT_FUNCTION_METADATA
        .iter()
        .filter(|&md| !md.name.is_empty())
        .map(|md| md.name)
        .collect()
}

pub fn input_function_get_code(name: &wstr) -> Option<ReadlineCmd> {
    // `input_function_metadata` is required to be kept in asciibetical order, making it OK to do
    // a binary search for the matching name.
    get_by_sorted_name(name, INPUT_FUNCTION_METADATA).map(|md| md.code)
}

#[test]
#[serial]
fn test_input() {
    test_init();
    use crate::env::EnvStack;
    let parser = Parser::new(Arc::pin(EnvStack::new()), false);
    let mut input = Inputter::new(parser, libc::STDIN_FILENO);
    // Ensure sequences are order independent. Here we add two bindings where the first is a prefix
    // of the second, and then emit the second key list. The second binding should be invoked, not
    // the first!
    let prefix_binding = WString::from_str("qqqqqqqa");
    let desired_binding = prefix_binding.clone() + "a";

    let default_mode = || DEFAULT_BIND_MODE.to_owned();

    {
        let mut input_mapping = input_mappings();
        input_mapping.add1(
            prefix_binding,
            WString::from_str("up-line"),
            default_mode(),
            default_mode(),
            true,
        );
        input_mapping.add1(
            desired_binding.clone(),
            WString::from_str("down-line"),
            default_mode(),
            default_mode(),
            true,
        );
    }

    // Push the desired binding to the queue.
    for c in desired_binding.chars() {
        input.queue_char(CharEvent::from_char(c));
    }

    // Now test.
    let evt = input.read_char(None);
    if !evt.is_readline() {
        panic!("Event is not a readline");
    } else if evt.get_readline() != ReadlineCmd::DownLine {
        panic!("Expected to read char down_line");
    }
}
