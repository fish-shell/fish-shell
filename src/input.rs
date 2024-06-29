use crate::common::{escape, get_by_sorted_name, shell_modes, str2wcstring, Named};
use crate::curses;
use crate::env::{Environment, CURSES_INITIALIZED};
use crate::event;
use crate::flog::FLOG;
use crate::input_common::{
    CharEvent, CharInputStyle, InputData, InputEventQueuer, ReadlineCmd, R_END_INPUT_FUNCTIONS,
};
use crate::key::{self, canonicalize_raw_escapes, ctrl, Key, Modifiers};
use crate::proc::job_reap;
use crate::reader::{
    reader_reading_interrupted, reader_reset_interrupted, reader_schedule_prompt_repaint, Reader,
};
use crate::signal::signal_clear_cancel;
use crate::threads::{assert_is_main_thread, iothread_service_main};
use crate::wchar::prelude::*;
use once_cell::sync::{Lazy, OnceCell};
use std::ffi::CString;
use std::sync::{
    atomic::{AtomicU32, Ordering},
    Mutex, MutexGuard,
};

pub const FISH_BIND_MODE_VAR: &wstr = L!("fish_bind_mode");
pub const DEFAULT_BIND_MODE: &wstr = L!("default");

/// A name for our own key mapping for nul.
pub const NUL_MAPPING_NAME: &wstr = L!("nul");

#[derive(Debug, Clone)]
pub struct InputMappingName {
    pub seq: Vec<Key>,
    pub mode: WString,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum KeyNameStyle {
    Plain,
    RawEscapeSequence,
    Terminfo(WString),
}

/// Struct representing a keybinding. Returned by input_get_mappings.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct InputMapping {
    /// Character sequence which generates this event.
    seq: Vec<Key>,
    /// Commands that should be evaluated by this mapping.
    pub commands: Vec<WString>,
    /// We wish to preserve the user-specified order. This is just an incrementing value.
    specification_order: u32,
    /// Mode in which this command should be evaluated.
    mode: WString,
    /// New mode that should be switched to after command evaluation, or None to leave the mode unchanged.
    sets_mode: Option<WString>,
    /// Perhaps this binding was created using a raw escape sequence or terminfo.
    key_name_style: KeyNameStyle,
}

impl InputMapping {
    /// Create a new mapping.
    fn new(
        seq: Vec<Key>,
        commands: Vec<WString>,
        mode: WString,
        sets_mode: Option<WString>,
        key_name_style: KeyNameStyle,
    ) -> InputMapping {
        static LAST_INPUT_MAP_SPEC_ORDER: AtomicU32 = AtomicU32::new(0);
        let specification_order = 1 + LAST_INPUT_MAP_SPEC_ORDER.fetch_add(1, Ordering::Relaxed);
        assert!(
            sets_mode.is_none() || !mode.is_empty(),
            "sets_mode set but mode is empty"
        );
        InputMapping {
            seq,
            commands,
            specification_order,
            mode,
            sets_mode,
            key_name_style,
        }
    }

    /// Return true if this is a generic mapping, i.e. acts as a fallback.
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
    make_md(L!(""), ReadlineCmd::FocusIn),
    make_md(L!(""), ReadlineCmd::FocusOut),
    make_md(L!("accept-autosuggestion"), ReadlineCmd::AcceptAutosuggestion),
    make_md(L!("and"), ReadlineCmd::FuncAnd),
    make_md(L!("backward-bigword"), ReadlineCmd::BackwardBigword),
    make_md(L!("backward-char"), ReadlineCmd::BackwardChar),
    make_md(L!("backward-char-passive"), ReadlineCmd::BackwardCharPassive),
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
    make_md(L!("forward-char-passive"), ReadlineCmd::ForwardCharPassive),
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
    make_md(L!("jump-till-matching-bracket"), ReadlineCmd::JumpTillMatchingBracket),
    make_md(L!("jump-to-matching-bracket"), ReadlineCmd::JumpToMatchingBracket),
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
        sequence: Vec<Key>,
        key_name_style: KeyNameStyle,
        commands: Vec<WString>,
        mode: WString,
        sets_mode: Option<WString>,
        user: bool,
    ) {
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
        let new_mapping = InputMapping::new(sequence, commands, mode, sets_mode, key_name_style);
        input_mapping_insert_sorted(ml, new_mapping);
    }

    // Like add(), but takes a single command.
    pub fn add1(
        &mut self,
        sequence: Vec<Key>,
        key_name_style: KeyNameStyle,
        command: WString,
        mode: WString,
        sets_mode: Option<WString>,
        user: bool,
    ) {
        self.add(
            sequence,
            key_name_style,
            vec![command],
            mode,
            sets_mode,
            user,
        );
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
        let mut add = |key: Vec<Key>, cmd: &str| {
            let mode = DEFAULT_BIND_MODE.to_owned();
            let sets_mode = Some(DEFAULT_BIND_MODE.to_owned());
            input_mapping.add1(key, KeyNameStyle::Plain, cmd.into(), mode, sets_mode, false);
        };

        add(vec![], "self-insert");
        add(vec![Key::from_raw(key::Enter)], "execute");
        add(vec![Key::from_raw(key::Tab)], "complete");
        add(vec![ctrl('c')], "cancel-commandline");
        add(vec![ctrl('d')], "exit");
        add(vec![ctrl('e')], "bind");
        add(vec![ctrl('s')], "pager-toggle-search");
        add(vec![ctrl('u')], "backward-kill-line");
        add(vec![Key::from_raw(key::Backspace)], "backward-delete-char");
        // Arrows - can't have functions, so *-or-search isn't available.
        add(vec![Key::from_raw(key::Up)], "up-line");
        add(vec![Key::from_raw(key::Down)], "down-line");
        add(vec![Key::from_raw(key::Right)], "forward-char");
        add(vec![Key::from_raw(key::Left)], "backward-char");
        // Emacs style
        add(vec![ctrl('p')], "up-line");
        add(vec![ctrl('n')], "down-line");
        add(vec![ctrl('b')], "backward-char");
        add(vec![ctrl('f')], "forward-char");

        let mut add_raw = |escape_sequence: &str, cmd: &str| {
            let mode = DEFAULT_BIND_MODE.to_owned();
            let sets_mode = Some(DEFAULT_BIND_MODE.to_owned());
            input_mapping.add1(
                canonicalize_raw_escapes(escape_sequence.chars().map(Key::from_raw).collect()),
                KeyNameStyle::RawEscapeSequence,
                cmd.into(),
                mode,
                sets_mode,
                false,
            );
        };
        add_raw("\x1B[A", "up-line");
        add_raw("\x1B[B", "down-line");
        add_raw("\x1B[C", "forward-char");
        add_raw("\x1B[D", "backward-char");
    }
}

impl<'a> InputEventQueuer for Reader<'a> {
    fn get_input_data(&self) -> &InputData {
        &self.data.input_data
    }

    fn get_input_data_mut(&mut self) -> &mut InputData {
        &mut self.data.input_data
    }

    fn prepare_to_select(&mut self) {
        // Fire any pending events and reap stray processes, including printing exit status messages.
        event::fire_delayed(self.parser);
        if job_reap(self.parser, true) {
            reader_schedule_prompt_repaint();
        }
    }

    fn select_interrupted(&mut self) {
        // Readline commands may be bound to \cc which also sets the cancel flag.
        // See #6937, #8125.
        signal_clear_cancel();

        // Fire any pending events and reap stray processes, including printing exit status messages.
        let parser = self.parser;
        event::fire_delayed(parser);
        if job_reap(parser, true) {
            reader_schedule_prompt_repaint();
        }

        // Tell the reader an event occurred.
        if reader_reading_interrupted(self) != 0 {
            let vintr = shell_modes().c_cc[libc::VINTR];
            if vintr != 0 {
                self.push_front(CharEvent::from_key(Key::from_single_byte(vintr)));
            }
            return;
        }
        self.push_front(CharEvent::from_check_exit());
    }

    fn uvar_change_notified(&mut self) {
        self.parser.sync_uvars_and_fire(true /* always */);
    }

    fn ioport_notified(&mut self) {
        iothread_service_main(self);
    }

    fn paste_start_buffering(&mut self) {
        self.input_data.paste_buffer = Some(vec![]);
        self.push_front(CharEvent::from_readline(ReadlineCmd::BeginUndoGroup));
    }

    fn paste_commit(&mut self) {
        self.push_front(CharEvent::from_readline(ReadlineCmd::EndUndoGroup));
        let Some(buffer) = self.input_data.paste_buffer.take() else {
            return;
        };
        self.push_front(CharEvent::Command(sprintf!(
            "__fish_paste %s",
            escape(&str2wcstring(&buffer))
        )));
    }
}

/// A struct which allows accumulating input events, or returns them to the queue.
/// This contains a list of events which have been dequeued, and a current index into that list.
pub struct EventQueuePeeker<'q, Queuer: InputEventQueuer + ?Sized> {
    /// The list of events which have been dequeued.
    peeked: Vec<CharEvent>,

    /// If set, then some previous timed event timed out.
    had_timeout: bool,

    /// The current index. This never exceeds peeked.len().
    idx: usize,
    /// The current index within a the raw characters within a single key event.
    subidx: usize,

    /// The queue from which to read more events.
    event_queue: &'q mut Queuer,
}

impl<'q, Queuer: InputEventQueuer + ?Sized> EventQueuePeeker<'q, Queuer> {
    pub fn new(event_queue: &'q mut Queuer) -> Self {
        EventQueuePeeker {
            peeked: Vec::new(),
            had_timeout: false,
            idx: 0,
            subidx: 0,
            event_queue,
        }
    }

    /// Return the next event.
    fn next(&mut self) -> CharEvent {
        assert!(self.subidx == 0);
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
        self.subidx = 0;
        res
    }

    /// Check if the next event is the given character. This advances the index on success only.
    /// If `escaped` is set, then return false if this (or any other) character had a timeout.
    fn next_is_char(&mut self, style: &KeyNameStyle, key: Key, escaped: bool) -> bool {
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
            let newevt = if escaped {
                FLOG!(reader, "reading timed escape");
                match self.event_queue.readch_timed_esc() {
                    Some(evt) => evt,
                    None => {
                        self.had_timeout = true;
                        return false;
                    }
                }
            } else {
                FLOG!(reader, "readch timed sequence key");
                match self.event_queue.readch_timed_sequence_key() {
                    Some(evt) => evt,
                    None => {
                        self.had_timeout = true;
                        return false;
                    }
                }
            };
            FLOG!(reader, format!("adding peeked {:?}", newevt));
            self.peeked.push(newevt);
        }
        // Now we have peeked far enough; check the event.
        // If it matches the char, then increment the index.
        let evt = &self.peeked[self.idx];
        let Some(kevt) = evt.get_key() else {
            return false;
        };
        if kevt.seq == L!("\x1b") && key.modifiers == Modifiers::ALT {
            self.idx += 1;
            self.subidx = 0;
            FLOG!(reader, "matched delayed escape prefix in alt sequence");
            return self.next_is_char(style, Key::from_raw(key.codepoint), true);
        }
        if *style == KeyNameStyle::Plain {
            if kevt.key == key {
                assert!(self.subidx == 0);
                self.idx += 1;
                FLOG!(reader, "matched full key", key);
                return true;
            }
            return false;
        }
        let actual_seq = kevt.seq.as_char_slice();
        if !actual_seq.is_empty() {
            let seq_char = actual_seq[self.subidx];
            if Key::from_single_char(seq_char) == key {
                self.subidx += 1;
                if self.subidx == actual_seq.len() {
                    self.idx += 1;
                    self.subidx = 0;
                }
                FLOG!(
                    reader,
                    format!(
                        "matched char {} with offset {} within raw sequence of length {}",
                        key,
                        self.subidx,
                        actual_seq.len()
                    )
                );
                return true;
            }
            if key.modifiers == Modifiers::ALT && seq_char == '\x1b' {
                if self.subidx + 1 == actual_seq.len() {
                    self.idx += 1;
                    self.subidx = 0;
                    FLOG!(reader, "matched escape prefix in raw escape sequence");
                    return self.next_is_char(style, Key::from_raw(key.codepoint), true);
                } else if actual_seq
                    .get(self.subidx + 1)
                    .cloned()
                    .map(|c| Key::from_single_char(c).codepoint)
                    == Some(key.codepoint)
                {
                    self.subidx += 2;
                    if self.subidx == actual_seq.len() {
                        self.idx += 1;
                        self.subidx = 0;
                    }
                    FLOG!(reader, format!("matched {key} against raw escape sequence"));
                    return true;
                }
            }
        }
        false
    }

    /// Consume all events up to the current index.
    /// Remaining events are returned to the queue.
    fn consume(mut self) {
        // Note this deliberately takes 'self' by value.
        self.event_queue.insert_front(self.peeked.drain(self.idx..));
        self.peeked.clear();
        self.idx = 0;
        self.subidx = 0;
    }

    /// Test if any of our peeked events are readline or check_exit.
    fn char_sequence_interrupted(&self) -> bool {
        self.peeked
            .iter()
            .any(|evt| evt.is_readline_or_command() || evt.is_check_exit())
    }

    /// Reset our index back to 0.
    pub fn restart(&mut self) {
        self.idx = 0;
        self.subidx = 0;
    }

    /// Return true if this `peeker` matches a given sequence of char events given by `str`.
    fn try_peek_sequence(&mut self, style: &KeyNameStyle, seq: &[Key]) -> bool {
        assert!(
            !seq.is_empty(),
            "Empty sequence passed to try_peek_sequence"
        );
        let mut prev = Key::from_raw(key::Invalid);
        for key in seq {
            // If we just read an escape, we need to add a timeout for the next char,
            // to distinguish between the actual escape key and an "alt"-modifier.
            let escaped = *style != KeyNameStyle::Plain && prev == Key::from_raw(key::Escape);
            if !self.next_is_char(style, *key, escaped) {
                return false;
            }
            prev = *key;
        }
        if self.subidx != 0 {
            FLOG!(
                reader,
                "legacy binding matched prefix of key encoding but did not consume all of it"
            );
            return false;
        }
        true
    }

    /// Return the first mapping that matches from the given mapping set, walking first over the
    /// user's mapping list, then the preset list.
    /// Return none if nothing matches, or if we may have matched a longer sequence but it was
    /// interrupted by a readline event.
    pub fn find_mapping(
        &mut self,
        vars: &dyn Environment,
        ip: &InputMappingSet,
    ) -> Option<InputMapping> {
        let mut generic: Option<&InputMapping> = None;
        let bind_mode = input_get_bind_mode(vars);
        let mut escape: Option<&InputMapping> = None;

        let ml = ip.mapping_list.iter().chain(ip.preset_mapping_list.iter());
        for m in ml {
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

            // FLOG!(reader, "trying mapping", format!("{:?}", m));
            if self.try_peek_sequence(&m.key_name_style, &m.seq) {
                // A binding for just escape should also be deferred
                // so escape sequences take precedence.
                if m.seq == vec![Key::from_raw(key::Escape)] {
                    if escape.is_none() {
                        escape = Some(m);
                    }
                } else {
                    return Some(m.clone());
                }
            }
            self.restart();
        }
        if self.char_sequence_interrupted() {
            // We might have matched a longer sequence, but we were interrupted, e.g. by a signal.
            FLOG!(reader, "torn sequence, rearranging events");
            return None;
        }

        if escape.is_some() {
            // We need to reconsume the escape.
            self.next();
            return escape.cloned();
        }

        if generic.is_some() {
            generic.cloned()
        } else {
            None
        }
    }
}

impl<Queue: InputEventQueuer + ?Sized> Drop for EventQueuePeeker<'_, Queue> {
    fn drop(&mut self) {
        assert!(
            self.idx == 0 && self.subidx == 0,
            "Events left on the queue - missing restart or consume?",
        );
        self.event_queue.insert_front(self.peeked.drain(self.idx..));
    }
}

/// Support for reading keys from the terminal for the Reader.
impl<'a> Reader<'a> {
    /// Read a key from our fd.
    pub fn read_char(&mut self) -> CharEvent {
        // Clear the interrupted flag.
        reader_reset_interrupted();

        // Search for sequence in mapping tables.
        loop {
            let evt = self.readch();
            match evt {
                CharEvent::Readline(ref readline_event) => match readline_event.cmd {
                    ReadlineCmd::SelfInsert | ReadlineCmd::SelfInsertNotFirst => {
                        // Typically self-insert is generated by the generic (empty) binding.
                        // However if it is generated by a real sequence, then insert that sequence.
                        let seq = readline_event.seq.chars().map(CharEvent::from_char);
                        self.insert_front(seq);
                        // Issue #1595: ensure we only insert characters, not readline functions. The
                        // common case is that this will be empty.
                        let mut res = self.read_characters_no_readline();

                        // Hackish: mark the input style.
                        if readline_event.cmd == ReadlineCmd::SelfInsertNotFirst {
                            if let CharEvent::Key(kevt) = &mut res {
                                kevt.input_style = CharInputStyle::NotFirst;
                            }
                        }
                        return res;
                    }
                    ReadlineCmd::FuncAnd | ReadlineCmd::FuncOr => {
                        // If previous function has bad status, skip all functions that follow us.
                        let fs = self.get_function_status();
                        if (!fs && readline_event.cmd == ReadlineCmd::FuncAnd)
                            || (fs && readline_event.cmd == ReadlineCmd::FuncOr)
                        {
                            self.drop_leading_readline_events();
                        }
                    }
                    _ => {
                        return evt;
                    }
                },
                CharEvent::Command(_) => {
                    return evt;
                }
                CharEvent::Eof => {
                    // If we have EOF, we need to immediately quit.
                    // There's no need to go through the input functions.
                    return evt;
                }
                CharEvent::CheckExit => {
                    // Allow the reader to check for exit conditions.
                    return evt;
                }
                CharEvent::Key(ref kevt) => {
                    FLOG!(
                        reader,
                        "Read char",
                        kevt.key,
                        format!(
                            "-- {:?} -- {:?}",
                            kevt.key,
                            kevt.seq.chars().map(u32::from).collect::<Vec<_>>()
                        )
                    );
                    self.push_front(evt);
                    self.mapping_execute_matching_or_generic();
                }
            }
        }
    }

    fn mapping_execute_matching_or_generic(&mut self) {
        let vars = self.parser.vars_ref();
        let mut peeker = EventQueuePeeker::new(self);
        // Check for ordinary mappings.
        let ip = input_mappings();
        if let Some(mapping) = peeker.find_mapping(&*vars, &ip) {
            FLOG!(
                reader,
                format!("Found mapping {:?} from {:?}", &mapping, &peeker.peeked)
            );
            peeker.consume();
            self.mapping_execute(&mapping);
            return;
        }
        std::mem::drop(ip);
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
        let mut saved_events = std::mem::take(&mut self.get_input_data_mut().event_storage);
        assert!(saved_events.is_empty(), "event_storage should be empty");

        let evt_to_return: CharEvent;
        loop {
            let evt = self.readch();
            if evt.is_readline_or_command() {
                saved_events.push(evt);
            } else {
                evt_to_return = evt;
                break;
            }
        }

        // Restore any readline functions
        self.insert_front(saved_events.drain(..));
        saved_events.clear();
        self.get_input_data_mut().event_storage = saved_events;
        evt_to_return
    }

    /// Perform the action of the specified binding.
    fn mapping_execute(&mut self, m: &InputMapping) {
        let has_command = m
            .commands
            .iter()
            .any(|cmd| input_function_get_code(cmd).is_none());
        if has_command {
            self.push_front(CharEvent::from_check_exit());
        }
        for cmd in m.commands.iter().rev() {
            let evt = match input_function_get_code(cmd) {
                Some(code) => {
                    self.function_push_args(code);
                    // At this point, the sequence is only used for reinserting the keys into
                    // the event queue for self-insert. Modifiers make no sense here so drop them.
                    CharEvent::from_readline_seq(
                        code,
                        m.seq
                            .iter()
                            .filter(|key| key.modifiers.is_none())
                            .map(|key| key.codepoint)
                            .collect(),
                    )
                }
                None => CharEvent::Command(cmd.clone()),
            };
            self.push_front(evt);
        }
        // Missing bind mode indicates to not reset the mode (#2871)
        if let Some(mode) = m.sets_mode.as_ref() {
            self.push_front(CharEvent::Command(sprintf!(
                "set --global %s %s",
                FISH_BIND_MODE_VAR,
                escape(mode)
            )));
        }
    }

    fn function_push_arg(&mut self, arg: char) {
        self.get_input_data_mut().input_function_args.push(arg);
    }

    pub fn function_pop_arg(&mut self) -> Option<char> {
        self.get_input_data_mut().input_function_args.pop()
    }

    fn function_push_args(&mut self, code: ReadlineCmd) {
        let arity = input_function_arity(code);
        let mut skipped = std::mem::take(&mut self.get_input_data_mut().event_storage);
        assert!(skipped.is_empty(), "event_storage should be empty");

        for _ in 0..arity {
            // Skip and queue up any function codes. See issue #2357.
            let arg: char;
            loop {
                let evt = self.readch();
                if let Some(kevt) = evt.get_key() {
                    if let Some(c) = kevt.key.codepoint_text() {
                        // TODO forward the whole key
                        arg = c;
                        break;
                    }
                }
                skipped.push(evt);
            }
            self.function_push_arg(arg);
        }

        // Push the function codes back into the input stream.
        self.insert_front(skipped.drain(..));
        skipped.clear();
        self.get_input_data_mut().event_storage = skipped;
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
        let ml = if user {
            &mut self.mapping_list
        } else {
            &mut self.preset_mapping_list
        };
        let should_erase = |m: &InputMapping| mode.is_none() || mode.unwrap() == m.mode;
        ml.retain(|m| !should_erase(m));
    }

    /// Erase binding for specified key sequence.
    pub fn erase(&mut self, sequence: &[Key], mode: &wstr, user: bool) -> bool {
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
    pub fn get<'a>(
        &'a self,
        sequence: &[Key],
        mode: &wstr,
        out_cmds: &mut &'a [WString],
        user: bool,
        out_sets_mode: &mut Option<&'a wstr>,
        out_key_name_style: &mut KeyNameStyle,
    ) -> bool {
        let ml = if user {
            &self.mapping_list
        } else {
            &self.preset_mapping_list
        };
        for m in ml {
            if m.seq == sequence && m.mode == mode {
                *out_cmds = &m.commands;
                *out_sets_mode = m.sets_mode.as_deref();
                *out_key_name_style = m.key_name_style.clone();
                return true;
            }
        }
        false
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

/// Possible errors from from input_terminfo_get_sequence.
pub enum GetSequenceError {
    /// The mapping was not found.
    NotFound,
    /// The terminfo variable does not have a value.
    NoSeq,
}

/// Return the sequence for the terminfo variable of the specified name.
///
/// If no terminfo variable of the specified name could be found, return false and set errno to
/// ENOENT. If the terminfo variable does not have a value, return false and set errno to EILSEQ.
pub fn input_terminfo_get_sequence(name: &wstr) -> Result<WString, GetSequenceError> {
    let mappings = TERMINFO_MAPPINGS
        .get()
        .expect("TERMINFO_MAPPINGS not initialized");
    for m in mappings.iter() {
        if name == m.name {
            // Found the mapping.
            return if let Some(seq) = &m.seq {
                Ok(str2wcstring(seq))
            } else {
                Err(GetSequenceError::NoSeq)
            };
        }
    }
    Err(GetSequenceError::NotFound)
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
