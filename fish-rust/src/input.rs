use crate::wchar::{wstr, WString, L};
use once_cell::sync::Lazy;
use std::sync::{
    atomic::{AtomicU32, Ordering},
    Arc, Mutex,
};

const DEFAULT_BIND_MODE: &wstr = L!("default");

static s_last_input_map_spec_order: AtomicU32 = AtomicU32::new(0);
static MAPPINGS: Lazy<Mutex<input_mapping_set_t>> = Lazy::new(Default::default);

pub fn with_mappings_mut<R>(cb: impl FnOnce(&mut input_mapping_set_t) -> R) -> R {
    let mut mappings = MAPPINGS.lock().unwrap();
    cb(&mut mappings)
}

/// Struct representing a keybinding. Returned by input_get_mappings.
#[derive(Default, Clone)]
pub struct input_mapping_t {
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

impl input_mapping_t {
    fn new(seq: WString, commands: Vec<WString>, mode: WString, sets_mode: WString) -> Self {
        let specification_order = s_last_input_map_spec_order.fetch_add(1, Ordering::SeqCst);

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

/// A struct representing the mapping from a terminfo key name to a terminfo character sequence.
struct terminfo_mapping_t {
    // name of key
    name: WString,

    // character sequence generated on keypress, or none if there was no mapping.
    seq: Option<String>,
}

impl terminfo_mapping_t {
    fn new(name: WString) -> Self {
        Self { name, seq: None }
    }

    fn new_with_seq(name: WString, seq: String) -> Self {
        Self {
            name,
            seq: Some(seq),
        }
    }
}

#[derive(Default)]
pub struct input_mapping_name_t {
    seq: WString,
    mode: WString,
}

pub type MappingList = Vec<input_mapping_t>;

/// The input mapping set is the set of mappings from character sequences to commands.
#[derive(Default)]
pub struct input_mapping_set_t {
    mappings: MappingList,
    preset_mappings: MappingList,
    all_mappings_cache: Option<Arc<MappingList>>,
}

impl input_mapping_set_t {
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
    pub fn get_names(&self, user: bool) -> Vec<input_mapping_name_t> {
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
            result.push(input_mapping_name_t {
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
        let new_mapping = input_mapping_t::new(sequence, commands, mode, sets_mode);
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

/// Helper function to compare the lengths of sequences.
fn length_is_greater_than(m1: &input_mapping_t, m2: &input_mapping_t) -> bool {
    return m1.seq.len() > m2.seq.len();
}

/// Inserts an input mapping at the correct position. We sort them in descending order by length, so
/// that we test longer sequences first.
pub fn input_mapping_insert_sorted(ml: &mut MappingList, new_mapping: input_mapping_t) {
    let mut loc = 0usize;
    for m in ml.iter() {
        if m.seq.len() > new_mapping.seq.len() {
            loc += 1;
        }
    }

    ml.insert(loc, new_mapping);
}

pub fn init_mappings() {
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
