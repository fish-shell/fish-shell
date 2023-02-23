#![allow(clippy::extra_unused_lifetimes, clippy::needless_lifetimes)]
use crate::ffi::{self, parser_t, readline_cmd_t};
use crate::threads::assert_is_main_thread;
use crate::wchar_ffi::WCharToFFI;
use crate::{
    env::flags,
    ffi::Repin,
    wchar::{wstr, WString, L},
    wutil::wgettext_fmt,
};
use errno::set_errno;
use libc::{EILSEQ, ENOENT};
use once_cell::sync::Lazy;
use std::sync::{
    atomic::{AtomicU32, Ordering},
    Arc, Mutex, MutexGuard,
};

#[cxx::bridge]
mod input_ffi {
    extern "C++" {}

    extern "Rust" {
        type GlobalMappings<'a>;

        #[cxx_name = "input_mappings"]
        unsafe fn input_mappings_ffi<'a>() -> Box<GlobalMappings<'a>>;
    }
}

const DEFAULT_BIND_MODE: &wstr = L!("default");
const FISH_BIND_MODE_VAR: &wstr = L!("fish_bind_mode");
const k_nul_mapping_name: &wstr = L!("nul");

const input_function_count: usize = 100; // TODO
static terminfo_mappings: Lazy<Vec<TermInfoMapping>> = Lazy::new(|| Vec::new());
static last_input_map_spec_order: AtomicU32 = AtomicU32::new(0);
static MAPPINGS: Lazy<Arc<Mutex<MappingsSet>>> = Lazy::new(Default::default);
const input_function_metadata: [FuntionMetadata; 0] = [];

pub fn with_mappings_mut<R>(cb: impl FnOnce(&mut MappingsSet) -> R) -> R {
    let mut mappings = MAPPINGS.lock().unwrap();
    cb(&mut mappings)
}

/// Struct representing a keybinding. Returned by input_get_mappings.
#[derive(Default, Clone)]
pub struct Mapping {
    /// Character sequence which generates this event.
    seq: WString,
    /// Commands that should be evaluated by this mapping.
    commands: Vec<WString>,
    /// We wish to preserve the user-specified order. This is just an incrementing value.
    specification_order: u32,
    /// Mode in which this command should be evaluated.
    mode: WString,
    /// New mode that should be switched to after command evaluation.
    sets_mode: WString,
}
pub type MappingList = Vec<Mapping>;

impl Mapping {
    fn new(seq: WString, commands: Vec<WString>, mode: WString, sets_mode: WString) -> Self {
        let specification_order = last_input_map_spec_order.fetch_add(1, Ordering::SeqCst);

        Self {
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

#[derive(Default)]
pub struct MappingName {
    seq: WString,
    mode: WString,
}

/// The input mapping set is the set of mappings from character sequences to commands.
#[derive(Default)]
pub struct MappingsSet {
    mappings: MappingList,
    preset_mappings: MappingList,
    all_mappings_cache: Option<Arc<MappingList>>,
}

impl MappingsSet {
    /// Erase all bindings with matching mode.
    fn clear(&mut self, mode: WString, user: bool) {
        self.all_mappings_cache = Default::default();
        let ml = if user {
            &mut self.mappings
        } else {
            &mut self.preset_mappings
        };

        ml.retain(|m| mode != m.mode);
    }

    /// Erase all bindings.
    fn clear_all(&mut self, user: bool) {
        self.all_mappings_cache = Default::default();
        let ml = if user {
            &mut self.mappings
        } else {
            &mut self.preset_mappings
        };
        ml.clear();
    }

    /// Erase binding for specified key sequence.
    fn erase(&mut self, sequence: WString, mode: WString, user: bool) -> bool {
        // Clear cached mappings.
        self.all_mappings_cache = Default::default();

        let mut result = false;
        let ml = if user {
            &mut self.mappings
        } else {
            &mut self.preset_mappings
        };

        for (i, it) in ml.iter().enumerate() {
            if sequence == it.seq && mode == it.mode {
                ml.remove(i);
                result = true;
                break;
            }
        }

        result
    }

    /// Gets the command bound to the specified key sequence in the specified mode. Returns true if
    /// it exists, false if not.
    fn get(&self, sequence: WString, mode: WString, user: bool) -> bool {
        let mut result = false;
        let ml = if user {
            &self.mappings
        } else {
            &self.preset_mappings
        };
        for m in ml {
            if sequence == m.seq && mode == m.mode {
                // *out_cmds = m.commands;
                // *out_sets_mode = m.sets_mode;
                result = true;
                break;
            }
        }
        return result;
    }

    /// Returns all mapping names and modes.
    pub fn get_names(&self, user: bool) -> Vec<MappingName> {
        // Sort the mappings by the user specification order, so we can return them in the same order
        // that the user specified them in.
        let mut local_list = if user {
            self.mappings.clone()
        } else {
            self.preset_mappings.clone()
        };

        local_list.sort_by_key(|a| a.specification_order);

        let mut result = Vec::with_capacity(local_list.len());

        for m in local_list {
            result.push(MappingName {
                seq: m.seq.clone(),
                mode: m.mode.clone(),
            });
        }

        result
    }

    /// Add a key mapping from the specified sequence to the specified command.
    ///
    /// \param sequence the sequence to bind
    /// \param command an input function that will be run whenever the key sequence occurs
    fn add(
        &mut self,
        sequence: WString,
        commands: Vec<WString>,
        mode: WString,
        sets_mode: WString,
        user: bool,
    ) {
        // Clear cached mappings.
        self.all_mappings_cache = Default::default();

        // Remove existing mappings with this sequence.
        let ml = if user {
            &mut self.mappings
        } else {
            &mut self.preset_mappings
        };

        for m in ml.iter_mut() {
            if m.seq == sequence && m.mode == mode {
                m.commands = commands;
                m.sets_mode = sets_mode;
                return;
            }
        }

        // Add a new mapping, using the next order.
        let new_mapping = Mapping::new(sequence, commands, mode, sets_mode);
        input_mapping_insert_sorted(ml, new_mapping);
    }

    fn add_command(
        &mut self,
        sequence: WString,
        command: WString,
        mode: WString,
        sets_mode: WString,
        user: bool,
    ) {
        self.add(sequence, vec![command], mode, sets_mode, user);
    }

    /// \return a snapshot of the list of input mappings.
    pub fn all_mappings(&mut self) -> Arc<MappingList> {
        // Populate the cache if needed.
        if let Some(ref all_mappings_cache) = self.all_mappings_cache {
            Arc::clone(all_mappings_cache)
        } else {
            let all_mappings = Arc::new(self.preset_mappings.clone());
            self.all_mappings_cache = Some(Arc::clone(&all_mappings));
            Arc::clone(&all_mappings)
        }
    }
}

/// A struct representing the mapping from a terminfo key name to a terminfo character sequence.
#[derive(Debug)]
struct TermInfoMapping {
    // name of key
    name: WString,

    // character sequence generated on keypress, or none if there was no mapping.
    seq: Option<WString>,
}

/// Input function metadata. This list should be kept in sync with the key code list in
/// input_common.h.
struct FuntionMetadata {
    name: WString,
    code: readline_cmd_t,
}

/// Helper function to compare the lengths of sequences.
fn length_is_greater_than(m1: &Mapping, m2: &Mapping) -> bool {
    return m1.seq.len() > m2.seq.len();
}

/// Inserts an input mapping at the correct position. We sort them in descending order by length, so
/// that we test longer sequences first.
pub fn input_mapping_insert_sorted(ml: &mut MappingList, new_mapping: Mapping) {
    let mut loc = 0usize;
    for m in ml.iter() {
        if m.seq.len() > new_mapping.seq.len() {
            loc += 1;
        }
    }

    ml.insert(loc, new_mapping);
}

/// Set up arrays used by readch to detect escape sequences for special keys and perform related
/// initializations for our input subsystem.
pub fn init_input() {
    assert_is_main_thread();
    // if (s_terminfo_mappings.is_set()) return;
    // s_terminfo_mappings = create_input_terminfo();

    with_mappings_mut(|input_mapping| {
        // If we have no keybindings, add a few simple defaults.
        if input_mapping.preset_mappings.is_empty() {
            input_mapping.add_command(
                WString::from_str(""),
                WString::from_str("self-insert"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
            input_mapping.add_command(
                WString::from_str("\n"),
                WString::from_str("execute"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
            input_mapping.add_command(
                WString::from_str("\r"),
                WString::from_str("execute"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
            input_mapping.add_command(
                WString::from_str("\t"),
                WString::from_str("complete"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
            input_mapping.add_command(
                WString::from_str("\x03"),
                WString::from_str("cancel-commandline"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
            input_mapping.add_command(
                WString::from_str("\x04"),
                WString::from_str("exit"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
            input_mapping.add_command(
                WString::from_str("\x05"),
                WString::from_str("bind"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
            // ctrl-s
            input_mapping.add_command(
                WString::from_str("\x13"),
                WString::from_str("pager-toggle-search"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
            // ctrl-u
            input_mapping.add_command(
                WString::from_str("\x15"),
                WString::from_str("backward-kill-line"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
            // del/backspace
            input_mapping.add_command(
                WString::from_str("\x7f"),
                WString::from_str("backward-delete-char"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
            // Arrows - can't have functions, so *-or-search isn't available.
            input_mapping.add_command(
                WString::from_str("\x1B[A"),
                WString::from_str("up-line"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
            input_mapping.add_command(
                WString::from_str("\x1B[B"),
                WString::from_str("down-line"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
            input_mapping.add_command(
                WString::from_str("\x1B[C"),
                WString::from_str("forward-char"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
            input_mapping.add_command(
                WString::from_str("\x1B[D"),
                WString::from_str("backward-char"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
            // emacs-style ctrl-p/n/b/f
            input_mapping.add_command(
                WString::from_str("\x10"),
                WString::from_str("up-line"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
            input_mapping.add_command(
                WString::from_str("\x0e"),
                WString::from_str("down-line"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
            input_mapping.add_command(
                WString::from_str("\x02"),
                WString::from_str("backward-char"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
            input_mapping.add_command(
                WString::from_str("\x06"),
                WString::from_str("forward-char"),
                DEFAULT_BIND_MODE.into(),
                DEFAULT_BIND_MODE.into(),
                false,
            );
        }
    });
}

fn describe_char(c: usize) -> WString {
    if c < input_function_count {
        return wgettext_fmt!("%02x (%ls)", c, input_function_metadata[c].name);
    }
    return wgettext_fmt!("%02x", c);
}

pub struct Inputter{
}

impl Inputter {
    // /// Construct from a parser, and the fd from which to read.
    // explicit inputter_t(parser_t &parser, int in = STDIN_FILENO);

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
     fn read_char() {
        // Clear the interrupted flag.
        ffi::reader_reset_interrupted();
    // Search for sequence in mapping tables.

    }

    /// Enqueue a char event to the queue of unread characters that input_readch will return before
    /// actually reading from fd 0.
    // void queue_char(const char_event_t &ch);

    /// Sets the return status of the most recently executed input function.
    // void function_set_status(bool status) { function_status_ = status; }

    /// Pop an argument from the function argument stack.
    // wchar_t function_pop_arg();

    // Called right before potentially blocking in select().
    // void prepare_to_select() override;

    // Called when select() is interrupted by a signal.
    // void select_interrupted() override;

    // Called when we are notified of a uvar change.
    // void uvar_change_notified() override;

    // void function_push_arg(wchar_t arg);
    // void function_push_args(readline_cmd_t code);
    // void mapping_execute(const input_mapping_t &m, const command_handler_t &command_handler);
    // void mapping_execute_matching_or_generic(const command_handler_t &command_handler);
    // maybe_t<input_mapping_t> find_mapping(event_queue_peeker_t *peeker);
    // char_event_t read_characters_no_readline();

    // We need a parser to evaluate bindings.
    // const std::shared_ptr<parser_t> parser_;

    // std::vector<wchar_t> input_function_args_{};
    // bool function_status_{false};

    // Transient storage to avoid repeated allocations.
    // std::vector<char_event_t> event_storage_{};
}

/// Return the current bind mode.
fn get_bind_mode(parser: &mut parser_t) -> WString {
    parser.pin().vars().unpin().get_or_default(
        FISH_BIND_MODE_VAR,
        DEFAULT_BIND_MODE,
        flags::ENV_DEFAULT,
    )
}

/// Set the current bind mode.
fn input_set_bind_mode(parser: &mut parser_t, bm: &wstr) {
    // Only set this if it differs to not execute variable handlers all the time.
    // modes may not be empty - empty is a sentinel value meaning to not change the mode
    assert!(!bm.is_empty());

    if get_bind_mode(parser) != bm {
        // Must send events here - see #6653.
        parser
            .pin()
            .set_var_and_fire(&FISH_BIND_MODE_VAR.to_ffi(), flags::ENV_GLOBAL, bm.to_ffi());
    }
}

fn input_mappings_ffi<'a>() -> Box<GlobalMappings<'a>> {
    let mappings = MAPPINGS.lock().unwrap();
    Box::new(GlobalMappings { g: mappings })
}

/// Returns the arity of a given input function.
fn input_function_arity(function: readline_cmd_t) -> i32 {
    match function {
        readline_cmd_t::forward_jump
        | readline_cmd_t::backward_jump
        | readline_cmd_t::forward_jump_till
        | readline_cmd_t::backward_jump_till => 1,
        _ => 0,
    }
}

/// Return the sequence for the terminfo variable of the specified name.
///
/// If no terminfo variable of the specified name could be found, return false and set errno to
/// ENOENT. If the terminfo variable does not have a value, return false and set errno to EILSEQ.
fn input_terminfo_get_sequence(name: &wstr) -> Option<WString> {
    // assert(s_terminfo_mappings.is_set());
    for m in terminfo_mappings.iter() {
        if name == m.name {
            // Found the mapping.
            if let Some(ref seq) = m.seq {
                return Some(seq.clone());
            } else {
                set_errno(EILSEQ);
                return None;
            }
        }
    }
    set_errno(ENOENT);
    None
}

/// Return the name of the terminfo variable with the specified sequence, in out_name. Returns true
/// if found, false if not found.
fn input_terminfo_get_name(seq: &wstr) -> Option<WString> {
    // assert(terminfo_mappings.is_set());
    for m in terminfo_mappings.iter() {
        if let Some(ref m_seq) = m.seq {
            if m_seq == seq {
                return Some(m.name.clone());
            }
        }
    }

    None
}

/// Return a list of all known terminfo names.
fn input_terminfo_get_names(skip_null: bool) -> Vec<WString> {
    // assert!(s_terminfo_mappings.is_set()); // TODO
    let mut result = Vec::with_capacity(terminfo_mappings.len());
    for m in terminfo_mappings.iter() {
        if (skip_null && m.seq.is_none()) {
            continue;
        }
        result.push(m.name.clone());
    }
    result
}

/// Returns the input function code for the given input function name.
fn input_function_get_names() -> Vec<WString> {
    // The list and names of input functions are hard-coded and never change
    let mut result = Vec::new();
    for md in &input_function_metadata {
        if !md.name.is_empty() {
            result.push(md.name.clone());
        }
    }

    result
}

/// Returns a list of all existing input function names.
fn input_function_get_code(name: &wstr) -> Option<readline_cmd_t> {
    // `input_function_metadata` is required to be kept in asciibetical order, making it OK to do
    // a binary search for the matching name.
    return input_function_metadata
        .as_slice()
        .binary_search_by(|md| md.name.as_utfstr().cmp(name))
        .ok()
        .map(|i| &input_function_metadata[i])
        .map(|md| md.code.clone());
}

pub struct GlobalMappings<'a> {
    g: MutexGuard<'a, MappingsSet>,
}

impl<'a> GlobalMappings<'a> {}
