use super::input::{CharEvent, CharInputStyle, ImplicitEvent, InputEventQueuer, KeyEvent};
use crate::{
    env::Environment,
    flog::{FloggableDebug, flog},
    global_safety::RelaxedAtomicBool,
    key::{self, Key, Modifiers, canonicalize_raw_escapes, ctrl},
    prelude::*,
    reader::{Reader, reader_reset_interrupted},
    threads::assert_is_main_thread,
};
use fish_common::{FilenameRef, Named, assert_sorted_by_name, escape, get_by_sorted_name};
use std::{
    mem,
    sync::{
        Mutex, MutexGuard,
        atomic::{AtomicU32, Ordering},
    },
};

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) enum KeyMatchQuality {
    BaseLayoutModuloShift,
    BaseLayout,
    ModuloShift,
    Exact,
}

impl FloggableDebug for KeyMatchQuality {}

pub(crate) fn match_key_event_to_key(event: &KeyEvent, key: &Key) -> Option<KeyMatchQuality> {
    if &event.key == key {
        return Some(KeyMatchQuality::Exact);
    }

    let shifted_evt = apply_shift(event.key, false, event.shifted_codepoint);
    let shifted_key = apply_shift(*key, true, '\0');
    if shifted_evt.is_some() && shifted_evt == shifted_key {
        return Some(KeyMatchQuality::ModuloShift);
    }

    if event.base_layout_codepoint != '\0' {
        let mut base_layout_key = event.key;
        base_layout_key.codepoint = event.base_layout_codepoint;
        if base_layout_key == *key {
            return Some(KeyMatchQuality::BaseLayout);
        }
        let shifted_base_layout_key = apply_shift(base_layout_key, true, '\0');
        if shifted_base_layout_key.is_some() && shifted_base_layout_key == shifted_key {
            return Some(KeyMatchQuality::BaseLayoutModuloShift);
        }
    }

    None
}

fn apply_shift(mut key: Key, do_ascii: bool, shifted_codepoint: char) -> Option<Key> {
    if !key.modifiers.shift {
        return Some(key);
    }
    if shifted_codepoint != '\0' {
        key.codepoint = shifted_codepoint;
    } else if do_ascii && key.codepoint.is_ascii_lowercase() {
        // For backwards compatibility, we convert the "bind shift-a" notation to "bind A".
        // This enables us to match "A" events which are the legacy encoding for keys that
        // generate text -- until we request kitty's "Report all keys as escape codes".
        // We do not currently convert non-ASCII key notation such as "bind shift-ä".
        key.codepoint = key.codepoint.to_ascii_uppercase();
    } else {
        return None;
    }
    key.modifiers.shift = false;
    Some(key)
}

pub const FISH_BIND_MODE_VAR: &wstr = L!("fish_bind_mode");
pub const DEFAULT_BIND_MODE: &wstr = L!("default");

#[derive(Debug, Clone)]
pub struct BindingName {
    pub seq: Vec<Key>,
    pub mode: WString,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum KeyNameStyle {
    Plain,
    RawEscapeSequence,
}

/// Struct representing a keybinding.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Binding {
    /// Character sequence which triggers this binding.
    seq: Vec<Key>,
    /// Commands that should be evaluated by this binding.
    pub commands: Vec<WString>,
    /// We wish to preserve the user-specified order. This is just an incrementing value.
    specification_order: u32,
    /// Mode in which this command should be evaluated.
    pub mode: WString,
    /// New mode that should be switched to after command evaluation, or None to leave the mode unchanged.
    pub sets_mode: Option<WString>,
    /// Perhaps this binding was created using a raw escape sequence.
    pub key_name_style: KeyNameStyle,
    /// The file from which the binding was created, or None if not from a file.
    pub definition_file: Option<FilenameRef>,
}

impl Binding {
    /// Create a new binding.
    fn new(
        seq: Vec<Key>,
        commands: Vec<WString>,
        mode: WString,
        sets_mode: Option<WString>,
        key_name_style: KeyNameStyle,
        definition_file: Option<FilenameRef>,
    ) -> Binding {
        static LAST_INPUT_MAP_SPEC_ORDER: AtomicU32 = AtomicU32::new(0);
        let specification_order = 1 + LAST_INPUT_MAP_SPEC_ORDER.fetch_add(1, Ordering::Relaxed);
        assert!(
            sets_mode.is_none() || !mode.is_empty(),
            "sets_mode set but mode is empty"
        );
        Binding {
            seq,
            commands,
            specification_order,
            mode,
            sets_mode,
            key_name_style,
            definition_file,
        }
    }

    /// Return true if this is a generic binding, i.e. acts as a fallback.
    fn is_generic(&self) -> bool {
        self.seq.is_empty()
    }
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

macro_rules! define_readline_cmds {
    {
        $( ( $name:literal, $readline_cmd:ident ), )*
    } => {
        #[derive(Debug, Copy, Clone, PartialEq, Eq)]
        #[repr(u8)]
        pub enum ReadlineCmd {
            $( $readline_cmd , )*
        }

        /// Helper to create a new InputFunctionMetadata struct.
        const fn make_md(name: &'static wstr, code: ReadlineCmd) -> InputFunctionMetadata {
            InputFunctionMetadata { name, code }
        }

        /// A static mapping of all readline commands as strings to their ReadlineCmd equivalent.
        const INPUT_FUNCTION_METADATA: &[InputFunctionMetadata] = {
            use ReadlineCmd::*;
            &[
                $(
                    make_md(L!($name), $readline_cmd),
                )*
            ]
        };
        assert_sorted_by_name!(INPUT_FUNCTION_METADATA);
    }
}

define_readline_cmds! {
    ("accept-autosuggestion", AcceptAutosuggestion),
    ("and", FuncAnd),
    ("backward-bigword", BackwardBigword),
    ("backward-bigword-end", BackwardBigwordEnd),
    ("backward-char", BackwardChar),
    ("backward-char-passive", BackwardCharPassive),
    ("backward-delete-char", BackwardDeleteChar),
    ("backward-jump", BackwardJump),
    ("backward-jump-till", BackwardJumpTill),
    ("backward-kill-bigword", BackwardKillBigword),
    ("backward-kill-line", BackwardKillLine),
    ("backward-kill-path-component", BackwardKillPathComponent),
    ("backward-kill-token", BackwardKillToken),
    ("backward-kill-word", BackwardKillWord),
    ("backward-path-component", BackwardPathComponent),
    ("backward-token", BackwardToken),
    ("backward-word", BackwardWord),
    ("backward-word-end", BackwardWordEnd),
    ("begin-selection", BeginSelection),
    ("begin-undo-group", BeginUndoGroup),
    ("beginning-of-buffer", BeginningOfBuffer),
    ("beginning-of-history", BeginningOfHistory),
    ("beginning-of-line", BeginningOfLine),
    ("cancel", Cancel),
    ("cancel-commandline", CancelCommandline),
    ("capitalize-word", CapitalizeWord),
    ("clear-commandline", ClearCommandline),
    ("clear-screen", ClearScreenAndRepaint),
    ("complete", Complete),
    ("complete-and-search", CompleteAndSearch),
    ("delete-char", DeleteChar),
    ("delete-or-exit", DeleteOrExit),
    ("down-line", DownLine),
    ("downcase-selection", DowncaseSelection),
    ("downcase-word", DowncaseWord),
    ("end-of-buffer", EndOfBuffer),
    ("end-of-history", EndOfHistory),
    ("end-of-line", EndOfLine),
    ("end-selection", EndSelection),
    ("end-undo-group", EndUndoGroup),
    ("execute", Execute),
    ("exit", Exit),
    ("expand-abbr", ExpandAbbr),
    ("force-repaint", ForceRepaint),
    ("forward-bigword", ForwardBigwordEmacs),
    ("forward-bigword-end", ForwardBigwordEnd),
    ("forward-bigword-vi", ForwardBigwordVi),
    ("forward-char", ForwardChar),
    ("forward-char-passive", ForwardCharPassive),
    ("forward-jump", ForwardJump),
    ("forward-jump-till", ForwardJumpTill),
    ("forward-path-component", ForwardPathComponent),
    ("forward-single-char", ForwardSingleChar),
    ("forward-token", ForwardToken),
    ("forward-word", ForwardWordEmacs),
    ("forward-word-end", ForwardWordEnd),
    ("forward-word-vi", ForwardWordVi),
    ("get-key", GetKey),
    ("history-delete", HistoryDelete),
    ("history-last-token-search-backward", HistoryLastTokenSearchBackward),
    ("history-last-token-search-forward", HistoryLastTokenSearchForward),
    ("history-pager", HistoryPager),
    ("history-pager-delete", HistoryPagerDelete),
    ("history-prefix-search-backward", HistoryPrefixSearchBackward),
    ("history-prefix-search-forward", HistoryPrefixSearchForward),
    ("history-search-backward", HistorySearchBackward),
    ("history-search-forward", HistorySearchForward),
    ("history-token-search-backward", HistoryTokenSearchBackward),
    ("history-token-search-forward", HistoryTokenSearchForward),
    ("insert-line-over", InsertLineOver),
    ("insert-line-under", InsertLineUnder),
    ("jump-till-matching-bracket", JumpTillMatchingBracket),
    ("jump-to-matching-bracket", JumpToMatchingBracket),
    ("kill-a-bigword", KillABigWord),
    ("kill-a-word", KillAWord),
    ("kill-bigword", KillBigwordEmacs),
    ("kill-bigword-vi", KillBigwordVi),
    ("kill-inner-bigword", KillInnerBigWord),
    ("kill-inner-line", KillInnerLine),
    ("kill-inner-word", KillInnerWord),
    ("kill-line", KillLine),
    ("kill-path-component", KillPathComponent),
    ("kill-selection", KillSelection),
    ("kill-token", KillToken),
    ("kill-whole-line", KillWholeLine),
    ("kill-word", KillWordEmacs),
    ("kill-word-vi", KillWordVi),
    ("nextd-or-forward-word", NextdOrForwardWordEmacs),
    ("or", FuncOr),
    ("pager-toggle-search", PagerToggleSearch),
    ("prevd-or-backward-word", PrevdOrBackwardWord),
    ("redo", Redo),
    ("repaint", Repaint),
    ("repaint-mode", RepaintMode),
    ("repeat-jump", RepeatJump),
    ("repeat-jump-reverse", ReverseRepeatJump),
    ("scrollback-push", ScrollbackPush),
    ("self-insert", SelfInsert),
    ("self-insert-notfirst", SelfInsertNotFirst),
    ("suppress-autosuggestion", SuppressAutosuggestion),
    ("swap-selection-start-stop", SwapSelectionStartStop),
    ("togglecase-char", TogglecaseChar),
    ("togglecase-selection", TogglecaseSelection),
    ("transpose-chars", TransposeChars),
    ("transpose-words", TransposeWords),
    ("undo", Undo),
    ("up-line", UpLine),
    ("upcase-selection", UpcaseSelection),
    ("upcase-word", UpcaseWord),
    ("yank", Yank),
    ("yank-pop", YankPop),
}

/// The set of bindings from character sequences to commands.
#[derive(Debug, Default)]
pub struct BindingSet {
    bindings: Vec<Binding>,
    preset_bindings: Vec<Binding>,
}

impl BindingSet {
    const fn new() -> Self {
        Self {
            bindings: Vec::new(),
            preset_bindings: Vec::new(),
        }
    }
}

/// Access the singleton binding set.
pub fn bindings() -> MutexGuard<'static, BindingSet> {
    static BINDINGS: Mutex<BindingSet> = Mutex::new(BindingSet::new());
    BINDINGS.lock().unwrap()
}

/// Return the current bind mode.
pub fn input_get_bind_mode(vars: &dyn Environment) -> WString {
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

/// Inserts a binding at the correct position. We sort them in descending order by length, so
/// that we test longer sequences first.
fn binding_insert_sorted(ml: &mut Vec<Binding>, new_binding: Binding) {
    let binding_len = new_binding.seq.len();
    let pos = ml
        .binary_search_by(|m| m.seq.len().cmp(&binding_len).reverse())
        .unwrap_or_else(|e| e);
    ml.insert(pos, new_binding);
}

impl BindingSet {
    /// Adds a binding.
    #[allow(clippy::too_many_arguments)]
    pub fn add(
        &mut self,
        sequence: Vec<Key>,
        key_name_style: KeyNameStyle,
        commands: Vec<WString>,
        mode: WString,
        sets_mode: Option<WString>,
        user: bool,
        definition_file: Option<FilenameRef>,
    ) {
        // Update any existing binding with this sequence.
        // FIXME: this makes adding multiple bindings quadratic.
        let ml = if user {
            &mut self.bindings
        } else {
            &mut self.preset_bindings
        };
        for m in ml.iter_mut() {
            if m.seq == sequence && m.mode == mode {
                m.commands = commands;
                m.sets_mode = sets_mode;
                m.definition_file = definition_file;
                return;
            }
        }

        // Add a new binding, using the next order.
        let new_binding = Binding::new(
            sequence,
            commands,
            mode,
            sets_mode,
            key_name_style,
            definition_file,
        );
        binding_insert_sorted(ml, new_binding);
    }

    // Like add(), but takes a single command.
    #[allow(clippy::too_many_arguments)]
    fn add1(
        &mut self,
        sequence: Vec<Key>,
        key_name_style: KeyNameStyle,
        command: WString,
        mode: WString,
        sets_mode: Option<WString>,
        user: bool,
        definition_file: Option<FilenameRef>,
    ) {
        self.add(
            sequence,
            key_name_style,
            vec![command],
            mode,
            sets_mode,
            user,
            definition_file,
        );
    }
}

/// Set up arrays used by readch to detect escape sequences for special keys and perform related
/// initializations for our input subsystem.
pub fn init_input() {
    assert_is_main_thread();

    static DONE: RelaxedAtomicBool = RelaxedAtomicBool::new(false);
    if DONE.swap(true) {
        return;
    }

    let mut bindings = bindings();

    // If we have no keybindings, add a few simple defaults.
    if bindings.preset_bindings.is_empty() {
        // Helper for adding.
        let mut add = |key: Vec<Key>, cmd: &str| {
            let mode = DEFAULT_BIND_MODE.to_owned();
            let sets_mode = Some(DEFAULT_BIND_MODE.to_owned());
            bindings.add1(
                key,
                KeyNameStyle::Plain,
                cmd.into(),
                mode,
                sets_mode,
                false,
                None,
            );
        };

        add(vec![], "self-insert");
        add(vec![Key::from_raw(key::ENTER)], "execute");
        add(vec![Key::from_raw(key::TAB)], "complete");
        add(vec![ctrl('c')], "cancel-commandline");
        add(vec![ctrl('d')], "exit");
        add(vec![ctrl('e')], "bind");
        add(vec![ctrl('s')], "pager-toggle-search");
        add(vec![ctrl('u')], "backward-kill-line");
        add(vec![Key::from_raw(key::BACKSPACE)], "backward-delete-char");
        // Arrows - can't have functions, so *-or-search isn't available.
        add(vec![Key::from_raw(key::UP)], "up-line");
        add(vec![Key::from_raw(key::DOWN)], "down-line");
        add(vec![Key::from_raw(key::RIGHT)], "forward-char");
        add(vec![Key::from_raw(key::LEFT)], "backward-char");
        // Emacs style
        add(vec![ctrl('p')], "up-line");
        add(vec![ctrl('n')], "down-line");
        add(vec![ctrl('b')], "backward-char");
        add(vec![ctrl('f')], "forward-char");

        let mut add_raw = |escape_sequence: &str, cmd: &str| {
            let mode = DEFAULT_BIND_MODE.to_owned();
            let sets_mode = Some(DEFAULT_BIND_MODE.to_owned());
            bindings.add1(
                canonicalize_raw_escapes(escape_sequence.chars().map(Key::from_raw).collect()),
                KeyNameStyle::RawEscapeSequence,
                cmd.into(),
                mode,
                sets_mode,
                false,
                None,
            );
        };
        add_raw("\x1B[A", "up-line");
        add_raw("\x1B[B", "down-line");
        add_raw("\x1B[C", "forward-char");
        add_raw("\x1B[D", "backward-char");
    }
}

/// A struct which allows accumulating input events, or returns them to the queue.
/// This contains a list of events which have been dequeued, and a current index into that list.
struct EventQueuePeeker<'q, Queuer: InputEventQueuer + ?Sized> {
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
    fn new(event_queue: &'q mut Queuer) -> Self {
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
        assert_eq!(self.subidx, 0);
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
    fn next_is_char(
        &mut self,
        style: &KeyNameStyle,
        key: Key,
        escaped: bool,
    ) -> Option<KeyMatchQuality> {
        assert!(
            self.idx <= self.peeked.len(),
            "Index must not be larger than dequeued event count"
        );
        // See if we had a timeout already.
        if escaped && self.had_timeout {
            return None;
        }
        // Grab a new event if we have exhausted what we have already peeked.
        // Use either readch or readch_timed, per our param.
        if self.idx == self.peeked.len() {
            let newevt = if escaped {
                flog!(reader, "reading timed escape");
                match self.event_queue.readch_timed_esc() {
                    Some(evt) => evt,
                    None => {
                        self.had_timeout = true;
                        return None;
                    }
                }
            } else {
                flog!(reader, "readch timed sequence key");
                match self.event_queue.readch_timed_sequence_key() {
                    Some(evt) => evt,
                    None => {
                        self.had_timeout = true;
                        return None;
                    }
                }
            };
            flog!(reader, format!("adding peeked {:?}", newevt));
            self.peeked.push(newevt);
        }
        // Now we have peeked far enough; check the event.
        // If it matches the char, then increment the index.
        let evt = &self.peeked[self.idx];
        let kevt = evt.get_key()?;
        if kevt.seq == L!("\x1b") && key.modifiers == Modifiers::ALT {
            self.idx += 1;
            self.subidx = 0;
            flog!(reader, "matched delayed escape prefix in alt sequence");
            return self.next_is_char(style, Key::from_raw(key.codepoint), true);
        }
        if *style == KeyNameStyle::Plain {
            let result = match_key_event_to_key(&kevt.key, &key);
            if let Some(key_match) = &result {
                assert_eq!(self.subidx, 0);
                self.idx += 1;
                flog!(reader, "matched full key", key, "kind", key_match);
            }
            return result;
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
                flog!(
                    reader,
                    format!(
                        "matched char {} with offset {} within raw sequence of length {}",
                        key,
                        self.subidx,
                        actual_seq.len()
                    )
                );
                return Some(KeyMatchQuality::Exact);
            }
            if key.modifiers == Modifiers::ALT && seq_char == '\x1b' {
                if self.subidx + 1 == actual_seq.len() {
                    self.idx += 1;
                    self.subidx = 0;
                    flog!(reader, "matched escape prefix in raw escape sequence");
                    return self.next_is_char(style, Key::from_raw(key.codepoint), true);
                } else if actual_seq
                    .get(self.subidx + 1)
                    .copied()
                    .map(|c| Key::from_single_char(c).codepoint)
                    == Some(key.codepoint)
                {
                    self.subidx += 2;
                    if self.subidx == actual_seq.len() {
                        self.idx += 1;
                        self.subidx = 0;
                    }
                    flog!(reader, format!("matched {key} against raw escape sequence"));
                    return Some(KeyMatchQuality::Exact);
                }
            }
        }
        None
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
        self.peeked.iter().any(|evt| {
            evt.is_readline_or_command()
                || matches!(evt, CharEvent::Implicit(ImplicitEvent::CheckExit))
        })
    }

    /// Reset our index back to 0.
    fn restart(&mut self) {
        self.idx = 0;
        self.subidx = 0;
    }

    /// Return true if this `peeker` matches a given sequence of char events given by `str`.
    fn try_peek_sequence(
        &mut self,
        style: &KeyNameStyle,
        seq: &[Key],
        quality: &mut Vec<KeyMatchQuality>,
    ) -> bool {
        assert!(
            !seq.is_empty(),
            "Empty sequence passed to try_peek_sequence"
        );
        let mut prev = None;
        for key in seq {
            // If we just read an escape, we need to add a timeout for the next char,
            // to distinguish between the actual escape key and an "alt"-modifier.
            let escaped = *style != KeyNameStyle::Plain && prev == Some(Key::from_raw(key::ESCAPE));
            let Some(spec) = self.next_is_char(style, *key, escaped) else {
                return false;
            };
            quality.push(spec);
            prev = Some(*key);
        }
        if self.subidx != 0 {
            flog!(
                reader,
                "legacy binding matched prefix of key encoding but did not consume all of it"
            );
            return false;
        }
        true
    }

    /// Return the first binding that matches from the given binding set, walking first over the
    /// user's binding list, then the preset list.
    /// Return none if nothing matches, or if we may have matched a longer sequence but it was
    /// interrupted by a readline event.
    fn find_binding<'a>(&mut self, ip: &'a BindingSet) -> Option<Binding> {
        let bind_mode = self.event_queue.get_bind_mode();

        struct MatchedBinding<'a> {
            binding: &'a Binding,
            quality: Vec<KeyMatchQuality>,
            idx: usize,
            subidx: usize,
        }

        let mut deferred: Option<MatchedBinding<'a>> = None;

        let ml = ip.bindings.iter().chain(ip.preset_bindings.iter());
        let mut quality = vec![];
        for m in ml {
            if m.mode != bind_mode {
                continue;
            }

            // Defer generic bindings until the end.
            if m.is_generic() {
                if deferred.is_none() {
                    deferred = Some(MatchedBinding {
                        binding: m,
                        quality: vec![],
                        idx: self.idx,
                        subidx: self.subidx,
                    });
                }
                continue;
            }

            // flog!(reader, "trying binding", format!("{:?}", m));
            if self.try_peek_sequence(&m.key_name_style, &m.seq, &mut quality) {
                // // A binding for just escape should also be deferred
                // // so escape sequences take precedence.
                let is_escape = m.seq == vec![Key::from_raw(key::ESCAPE)];
                let is_perfect_match = quality
                    .iter()
                    .all(|key_match| *key_match == KeyMatchQuality::Exact);
                if !is_escape && is_perfect_match {
                    return Some(m.clone());
                }
                if deferred
                    .as_ref()
                    .is_none_or(|matched| !is_escape && quality >= matched.quality)
                {
                    deferred = Some(MatchedBinding {
                        binding: m,
                        quality: mem::take(&mut quality),
                        idx: self.idx,
                        subidx: self.subidx,
                    });
                }
            }
            quality.clear();
            self.restart();
        }
        if self.char_sequence_interrupted() {
            // We might have matched a longer sequence, but we were interrupted, e.g. by a signal.
            flog!(reader, "torn sequence, rearranging events");
            return None;
        }

        deferred
            .map(|matched| {
                self.idx = matched.idx;
                self.subidx = matched.subidx;
                matched.binding
            })
            .cloned()
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

        // Search for sequence in binding tables.
        loop {
            let evt = self.readch();
            match evt {
                CharEvent::Readline(ref readline_event) => match readline_event.cmd {
                    ReadlineCmd::SelfInsert
                    | ReadlineCmd::SelfInsertNotFirst
                    | ReadlineCmd::GetKey => {
                        // Typically self-insert is generated by the generic (empty) binding.
                        // However if it is generated by a real sequence, then insert that sequence.
                        let seq = readline_event.seq.chars().map(CharEvent::from_char);
                        self.insert_front(seq);
                        // Issue #1595: ensure we only insert characters, not readline functions. The
                        // common case is that this will be empty.
                        let mut res = self.read_character_matching(|evt| {
                            use CharEvent::*;
                            use ImplicitEvent::*;
                            match evt {
                                Key(_) => true,
                                Implicit(Eof) => true,
                                Readline(_) | Command(_) | Implicit(_) | QueryResult(_) => false,
                            }
                        });

                        // Hackish: mark the input style.
                        if readline_event.cmd == ReadlineCmd::SelfInsertNotFirst {
                            if let CharEvent::Key(kevt) = &mut res {
                                kevt.input_style = CharInputStyle::NotFirst;
                            }
                        }
                        if readline_event.cmd == ReadlineCmd::GetKey {
                            if let CharEvent::Key(kevt) = res {
                                return CharEvent::Command(sprintf!(
                                    "set -g fish_key %s",
                                    escape(
                                        &kevt
                                            .key
                                            .codepoint_text()
                                            .map(|c| WString::from_chars(vec![c]))
                                            .unwrap_or_default()
                                    )
                                ));
                            }
                        }
                        return res;
                    }
                    ReadlineCmd::FuncAnd | ReadlineCmd::FuncOr => {
                        // If previous function has bad status, skip all functions that follow us.
                        let fs = self.function_status();
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
                CharEvent::Key(ref kevt) => {
                    flog!(
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
                    self.binding_execute_matching_or_generic();
                }
                CharEvent::Implicit(_) | CharEvent::QueryResult(_) => {
                    return evt;
                }
            }
        }
    }

    fn binding_execute_matching_or_generic(&mut self) {
        let mut peeker = EventQueuePeeker::new(self);
        // Check for ordinary bindings.
        let bindings = bindings();
        if let Some(binding) = peeker.find_binding(&bindings) {
            flog!(
                reader,
                format!("Found binding {:?} from {:?}", &binding, &peeker.peeked)
            );
            peeker.consume();
            self.binding_execute(&binding);
            return;
        }
        std::mem::drop(bindings);
        peeker.restart();

        if peeker.char_sequence_interrupted() {
            // This may happen if we received a signal in the middle of an escape sequence or other
            // multi-char binding. Move these non-char events to the front of the queue, handle them
            // first, and then later we'll return and try the sequence again. See #8628.
            peeker.consume();
            self.promote_interruptions_to_front();
            return;
        }

        flog!(reader, "no generic found, ignoring char...");
        let _ = peeker.next();
        peeker.consume();
    }

    /// Pick through the queue of incoming characters until we get to one that matches.
    fn read_character_matching(&mut self, predicate: impl Fn(&CharEvent) -> bool) -> CharEvent {
        let mut saved_events = std::mem::take(&mut self.get_input_data_mut().event_storage);
        assert!(saved_events.is_empty(), "event_storage should be empty");

        let evt_to_return: CharEvent = loop {
            let evt = self.readch();
            if (predicate)(&evt) {
                break evt;
            }
            saved_events.push(evt);
        };

        // Restore any readline functions
        self.insert_front(saved_events.drain(..));
        saved_events.clear();
        self.get_input_data_mut().event_storage = saved_events;
        evt_to_return
    }

    /// Perform the action of the specified binding.
    fn binding_execute(&mut self, m: &Binding) {
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

impl BindingSet {
    /// Returns all binding names and modes.
    pub fn get_names(&self, user: bool) -> Vec<BindingName> {
        // Sort the bindings by the user specification order, so we can return them in the same order
        // that the user specified them in.
        let mut local_list = if user {
            self.bindings.clone()
        } else {
            self.preset_bindings.clone()
        };
        local_list.sort_unstable_by_key(|m| m.specification_order);
        local_list
            .into_iter()
            .map(|m| BindingName {
                seq: m.seq,
                mode: m.mode,
            })
            .collect()
    }

    /// Erase all bindings.
    pub fn clear(&mut self, mode: Option<&wstr>, user: bool) {
        let ml = if user {
            &mut self.bindings
        } else {
            &mut self.preset_bindings
        };
        let should_erase = |m: &Binding| mode.is_none_or(|x| x == m.mode);
        ml.retain(|m| !should_erase(m));
    }

    /// Erase binding for specified key sequence.
    pub fn erase(&mut self, sequence: &[Key], mode: &wstr, user: bool) -> bool {
        let ml = if user {
            &mut self.bindings
        } else {
            &mut self.preset_bindings
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

    /// Returns the command bound to the specified bind mode.
    ///
    /// If bind_mode is None, then binds from all modes are returned.
    pub fn get<'a>(
        &'a self,
        sequence: &[Key],
        bind_mode: Option<&wstr>,
        user: bool,
    ) -> Vec<&'a Binding> {
        let ml = if user {
            &self.bindings
        } else {
            &self.preset_bindings
        };

        let ml = ml.iter().filter(|binding| binding.seq == sequence);
        let mut bindings: Vec<_>;
        if let Some(mode) = bind_mode {
            bindings = ml.filter(|binding| binding.mode == mode).collect();
            assert!(bindings.len() <= 1);
        } else {
            bindings = ml.collect();
            bindings.sort_unstable_by_key(|binding| binding.specification_order);
        }
        bindings
    }
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

#[cfg(test)]
pub(super) mod mock {
    use crate::input::{InputData, InputEventQueuer};

    pub struct MockInputEventQueuer {
        pub input_data: InputData,
        pub pending_input: &'static [u8],
    }

    impl MockInputEventQueuer {
        pub fn new() -> Self {
            MockInputEventQueuer {
                input_data: InputData::new(i32::MAX, None), // value doesn't matter since we don't read from it
                pending_input: b"",
            }
        }
    }

    impl InputEventQueuer for MockInputEventQueuer {
        fn get_input_data(&self) -> &InputData {
            &self.input_data
        }
        fn get_input_data_mut(&mut self) -> &mut InputData {
            &mut self.input_data
        }
        fn read_sequence_byte(&mut self, buffer: &mut Vec<u8>) -> Option<u8> {
            let (head, tail) = self.pending_input.split_at_checked(1)?;
            self.pending_input = tail;
            let head = head[0];
            buffer.push(head);
            Some(head)
        }
    }
}
#[cfg(test)]
pub(super) use mock::MockInputEventQueuer;

#[cfg(test)]
mod tests {
    use super::{
        BindingSet, EventQueuePeeker, KeyMatchQuality, KeyNameStyle, MockInputEventQueuer,
        match_key_event_to_key,
    };
    use crate::input::{CharEvent, InputEventQueuer as _, KeyEvent};
    use crate::key::{Key, Modifiers};
    use crate::prelude::*;

    #[test]
    fn test_match_key_event_to_key() {
        macro_rules! validate {
            ($evt:expr, $key:expr, $expected:expr) => {
                assert_eq!(match_key_event_to_key(&$evt, &$key), $expected);
            };
        }

        let none = Modifiers::default();
        let shift = Modifiers::SHIFT;
        let ctrl = Modifiers::CTRL;
        let ctrl_shift = Modifiers {
            ctrl: true,
            shift: true,
            ..Default::default()
        };

        let exact = KeyMatchQuality::Exact;
        let modulo_shift = KeyMatchQuality::ModuloShift;
        let base_layout = KeyMatchQuality::BaseLayout;
        let base_layout_modulo_shift = KeyMatchQuality::BaseLayoutModuloShift;

        validate!(KeyEvent::new(none, 'a'), Key::new(none, 'a'), Some(exact));
        validate!(KeyEvent::new(none, 'a'), Key::new(none, 'A'), None);
        validate!(KeyEvent::new(shift, 'a'), Key::new(shift, 'a'), Some(exact));
        validate!(KeyEvent::new(shift, 'a'), Key::new(none, 'A'), None);
        validate!(KeyEvent::new(shift, 'ä'), Key::new(none, 'Ä'), None);
        // For historical reasons we canonicalize notation for ASCII keys like "shift-a" to "A",
        // but not "shift-a" events - those should send a shifted key.
        validate!(
            KeyEvent::new(none, 'A'),
            Key::new(shift, 'a'),
            Some(modulo_shift)
        );
        validate!(KeyEvent::new(none, 'A'), Key::new(shift, 'A'), None);
        validate!(KeyEvent::new(none, 'Ä'), Key::new(none, 'Ä'), Some(exact));
        validate!(KeyEvent::new(none, 'Ä'), Key::new(shift, 'ä'), None);

        // FYI: for codepoints that are not letters with uppercase/lowercase versions, we use
        // the shifted key in the canonical notation, because the unshifted one may depend on the
        // keyboard layout.
        let ctrl_shift_equals = KeyEvent::new_with(ctrl_shift, true, '=', Some('+'), None);
        validate!(ctrl_shift_equals, Key::new(ctrl_shift, '='), Some(exact));
        validate!(ctrl_shift_equals, Key::new(ctrl, '+'), Some(modulo_shift)); // canonical notation
        validate!(ctrl_shift_equals, Key::new(ctrl_shift, '+'), None);
        validate!(ctrl_shift_equals, Key::new(ctrl, '='), None);

        // A event like capslock-shift-ä may or may not include a shifted codepoint.
        //
        // Without a shifted codepoint, we cannot easily match ctrl-Ä.
        let caps_ctrl_shift_ä = KeyEvent::new(ctrl_shift, 'ä');
        validate!(caps_ctrl_shift_ä, Key::new(ctrl_shift, 'ä'), Some(exact)); // canonical notation
        validate!(caps_ctrl_shift_ä, Key::new(ctrl, 'ä'), None);
        validate!(caps_ctrl_shift_ä, Key::new(ctrl, 'Ä'), None); // can't match without shifted key
        validate!(caps_ctrl_shift_ä, Key::new(ctrl_shift, 'Ä'), None);
        // With a shifted codepoint, we can match the alternative notation too.
        let caps_ctrl_shift_ä = KeyEvent::new_with(ctrl_shift, true, 'ä', Some('Ä'), None);
        validate!(caps_ctrl_shift_ä, Key::new(ctrl_shift, 'ä'), Some(exact)); // canonical notation
        validate!(caps_ctrl_shift_ä, Key::new(ctrl, 'ä'), None);
        validate!(caps_ctrl_shift_ä, Key::new(ctrl, 'Ä'), Some(modulo_shift)); // matched via shifted key
        validate!(caps_ctrl_shift_ä, Key::new(ctrl_shift, 'Ä'), None);

        let ctrl_ц = KeyEvent::new_with(ctrl, true, 'ц', None, Some('w'));
        let ctrl_shift_ц = KeyEvent::new_with(ctrl_shift, true, 'ц', Some('Ц'), Some('w'));
        validate!(ctrl_ц, Key::new(ctrl, 'ц'), Some(exact));
        validate!(ctrl_ц, Key::new(ctrl, 'w'), Some(base_layout));
        validate!(ctrl_ц, Key::new(ctrl_shift, 'ц'), None);
        validate!(ctrl_ц, Key::new(ctrl_shift, 'w'), None);
        validate!(
            ctrl_shift_ц,
            Key::new(ctrl, 'W'),
            Some(base_layout_modulo_shift)
        );
        validate!(ctrl_shift_ц, Key::new(ctrl, 'w'), None);

        // Note that "bind ctrl-Ц" will win over "bind ctrl-shift-w".
        // This is because we consider shift transformation to be less magic than base-key
        // transformation.
        validate!(ctrl_shift_ц, Key::new(ctrl, 'Ц'), Some(modulo_shift));
        validate!(ctrl_shift_ц, Key::new(ctrl_shift, 'w'), Some(base_layout));
    }

    #[test]
    fn test_input() {
        let mut input = MockInputEventQueuer::new();
        // Ensure sequences are order independent. Here we add two bindings where the first is a prefix
        // of the second, and then emit the second key list. The second binding should be invoked, not
        // the first!
        let prefix_binding: Vec<Key> = "qqqqqqqa".chars().map(Key::from_raw).collect();
        let mut desired_binding = prefix_binding.clone();
        desired_binding.push(Key::from_raw('a'));

        let bind_mode = || input.get_bind_mode();

        let mut bindings = BindingSet::default();
        bindings.add1(
            prefix_binding,
            KeyNameStyle::Plain,
            L!("up-line").to_owned(),
            bind_mode(),
            None,
            true,
            None,
        );
        bindings.add1(
            desired_binding.clone(),
            KeyNameStyle::Plain,
            L!("down-line").to_owned(),
            bind_mode(),
            None,
            true,
            None,
        );

        // Push the desired binding to the queue.
        for key in desired_binding {
            input
                .input_data
                .queue_char(CharEvent::from_key(KeyEvent::from(key)));
        }

        let mut peeker = EventQueuePeeker::new(&mut input);
        let binding = peeker.find_binding(&bindings);
        assert!(binding.is_some());
        assert_eq!(binding.unwrap().commands, ["down-line"]);
        peeker.restart();
    }
}
