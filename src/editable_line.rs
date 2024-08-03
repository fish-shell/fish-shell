use crate::future::IsSomeAnd;
use crate::highlight::HighlightSpec;
use crate::wchar::prelude::*;

/// An edit action that can be undone.
#[derive(Clone, Eq, PartialEq)]
pub struct Edit {
    /// When undoing the edit we use this to restore the previous cursor position.
    pub cursor_position_before_edit: usize,
    pub cursor_position_before_undo: Option<usize>,

    /// The span of text that is replaced by this edit.
    pub range: std::ops::Range<usize>,

    /// The strings that are removed and added by this edit, respectively.
    pub old: WString,
    pub replacement: WString,

    /// edit_t is only for contiguous changes, so to restore a group of arbitrary changes to the
    /// command line we need to have a group id as forcibly coalescing changes is not enough.
    group_id: Option<usize>,
}

impl Edit {
    pub fn new(range: std::ops::Range<usize>, replacement: WString) -> Self {
        Self {
            cursor_position_before_edit: 0,
            cursor_position_before_undo: None,
            range,
            old: WString::new(),
            replacement,
            group_id: None,
        }
    }
}

/// Modify a string and its syntax highlighting according to the given edit.
/// Currently exposed for testing only.
pub fn apply_edit(target: &mut WString, colors: &mut Vec<HighlightSpec>, edit: &Edit) {
    let range = &edit.range;
    target.replace_range(range.clone(), &edit.replacement);

    // Now do the same to highlighting.
    let last_color = edit
        .range
        .start
        .checked_sub(1)
        .map(|i| colors[i])
        .unwrap_or_default();
    colors.splice(
        range.clone(),
        std::iter::repeat(last_color).take(edit.replacement.len()),
    );
}

/// The history of all edits to some command line.
#[derive(Clone, Default)]
pub struct UndoHistory {
    /// The stack of edits that can be undone or redone atomically.
    pub edits: Vec<Edit>,

    /// The position in the undo stack that corresponds to the current
    /// state of the input line.
    /// Invariants:
    ///     edits_applied - 1 is the index of the next edit to undo.
    ///     edits_applied     is the index of the next edit to redo.
    ///
    /// For example, if nothing was undone, edits_applied is edits.size().
    /// If every single edit was undone, edits_applied is 0.
    pub edits_applied: usize,

    /// Whether we allow the next edit to be grouped together with the
    /// last one.
    may_coalesce: bool,

    /// Whether to be more aggressive in coalescing edits. Ideally, it would be "force coalesce"
    /// with guaranteed atomicity but as `edit_t` is strictly for contiguous changes, that guarantee
    /// can't be made at this time.
    try_coalesce: bool,
}

impl UndoHistory {
    /// Empty the history.
    pub fn clear(&mut self) {
        self.edits.clear();
        self.edits_applied = 0;
        self.may_coalesce = false;
    }
}

/// Helper class for storing a command line.
#[derive(Clone, Default)]
pub struct EditableLine {
    /// The command line.
    text: WString,
    /// Syntax highlighting.
    colors: Vec<HighlightSpec>,
    /// The current position of the cursor in the command line.
    position: usize,

    /// The history of all edits.
    undo_history: UndoHistory,
    /// The nesting level for atomic edits, so that recursive invocations of start_edit_group()
    /// are not ended by one end_edit_group() call.
    edit_group_level: Option<usize>,
    /// Monotonically increasing edit group, ignored when edit_group_level_ is -1. Allowed to wrap.
    edit_group_id: usize,
}

impl EditableLine {
    pub fn text(&self) -> &wstr {
        &self.text
    }

    pub fn colors(&self) -> &[HighlightSpec] {
        &self.colors
    }
    pub fn set_colors(&mut self, colors: Vec<HighlightSpec>) {
        assert_eq!(colors.len(), self.len());
        self.colors = colors;
    }

    pub fn position(&self) -> usize {
        self.position
    }
    pub fn set_position(&mut self, position: usize) {
        assert!(position <= self.len());
        self.position = position;
    }

    // Gets the length of the text.
    pub fn len(&self) -> usize {
        self.text.len()
    }

    pub fn is_empty(&self) -> bool {
        self.text.is_empty()
    }

    pub fn at(&self, idx: usize) -> char {
        self.text.char_at(idx)
    }

    pub fn line_at_cursor(&self) -> &wstr {
        let start = self.text[0..self.position()]
            .as_char_slice()
            .iter()
            .rposition(|&c| c == '\n')
            .map(|newline| newline + 1)
            .unwrap_or(0);
        let end = self.text[self.position()..]
            .as_char_slice()
            .iter()
            .position(|&c| c == '\n')
            .map(|pos| self.position() + pos)
            .unwrap_or(self.len());
        // Remove any traililng newline
        self.text[start..end].trim_matches('\n')
    }

    pub fn clear(&mut self) {
        if self.is_empty() {
            return;
        }
        self.push_edit(
            Edit::new(0..self.len(), L!("").to_owned()),
            /*allow_coalesce=*/ false,
        );
    }

    /// Modify the commandline according to @edit. Most modifications to the
    /// text should pass through this function.
    pub fn push_edit(&mut self, mut edit: Edit, allow_coalesce: bool) {
        let range = &edit.range;
        let is_insertion = range.is_empty();
        // Coalescing insertion does not create a new undo entry but adds to the last insertion.
        if allow_coalesce && is_insertion && self.want_to_coalesce_insertion_of(&edit.replacement) {
            assert!(range.start == self.position());
            let last_edit = self.undo_history.edits.last_mut().unwrap();
            last_edit.replacement.push_utfstr(&edit.replacement);
            apply_edit(&mut self.text, &mut self.colors, &edit);
            self.set_position(self.position() + edit.replacement.len());

            assert!(self.undo_history.may_coalesce);
            return;
        }

        // Assign a new group id or propagate the old one if we're in a logical grouping of edits
        if self.edit_group_level.is_some() {
            edit.group_id = Some(self.edit_group_id);
        }

        let edit_does_nothing = range.is_empty() && edit.replacement.is_empty();
        if edit_does_nothing {
            return;
        }
        if self.undo_history.edits_applied != self.undo_history.edits.len() {
            // After undoing some edits, the user is making a new edit;
            // we are about to create a new edit branch.
            // Discard all edits that were undone because we only support
            // linear undo/redo, they will be unreachable.
            self.undo_history
                .edits
                .truncate(self.undo_history.edits_applied);
        }
        edit.cursor_position_before_edit = self.position();
        edit.old = self.text[range.clone()].to_owned();
        apply_edit(&mut self.text, &mut self.colors, &edit);
        self.set_position(cursor_position_after_edit(&edit));
        assert_eq!(
            self.undo_history.edits_applied,
            self.undo_history.edits.len()
        );
        self.undo_history.may_coalesce =
            is_insertion && (self.undo_history.try_coalesce || edit.replacement.len() == 1);
        self.undo_history.edits_applied += 1;
        self.undo_history.edits.push(edit);
    }

    /// Undo the most recent edit that was not yet undone. Returns true on success.
    pub fn undo(&mut self) -> bool {
        let mut did_undo = false;
        let mut last_group_id = None;
        let position_before_undo = self.position();
        let end = self.undo_history.edits_applied;
        while self.undo_history.edits_applied != 0 {
            let edit = &self.undo_history.edits[self.undo_history.edits_applied - 1];
            if did_undo
                && edit
                    .group_id
                    .is_none_or(|group_id| Some(group_id) != last_group_id)
            {
                // We've restored all the edits in this logical undo group
                break;
            }
            last_group_id = edit.group_id;
            self.undo_history.edits_applied -= 1;
            let range = &edit.range;
            let mut inverse = Edit::new(
                range.start..range.start + edit.replacement.len(),
                L!("").to_owned(),
            );
            inverse.replacement = edit.old.clone();
            let old_position = edit.cursor_position_before_edit;
            apply_edit(&mut self.text, &mut self.colors, &inverse);
            self.set_position(old_position);
            did_undo = true;
        }
        if did_undo {
            let edit = &mut self.undo_history.edits[end - 1];
            edit.cursor_position_before_undo = Some(position_before_undo);
        }

        self.end_edit_group();
        self.undo_history.may_coalesce = false;
        did_undo
    }

    /// Redo the most recent undo. Returns true on success.
    pub fn redo(&mut self) -> bool {
        let mut did_redo = false;

        let mut last_group_id = None;
        while let Some(edit) = self.undo_history.edits.get(self.undo_history.edits_applied) {
            if did_redo
                && edit
                    .group_id
                    .is_none_or(|group_id| Some(group_id) != last_group_id)
            {
                // We've restored all the edits in this logical undo group
                break;
            }
            last_group_id = edit.group_id;
            self.undo_history.edits_applied += 1;
            apply_edit(&mut self.text, &mut self.colors, edit);
            edit.cursor_position_before_undo
                .map(|pos| self.set_position(pos));
            did_redo = true;
        }

        self.end_edit_group();
        did_redo
    }

    /// Start a logical grouping of command line edits that should be undone/redone together.
    pub fn begin_edit_group(&mut self) {
        if self.edit_group_level.is_some() {
            return;
        }
        self.edit_group_level = Some(1);
        // Indicate that the next change must trigger the creation of a new history item
        self.undo_history.may_coalesce = false;
        // Indicate that future changes should be coalesced into the same edit if possible.
        self.undo_history.try_coalesce = true;
        // Assign a logical edit group id to future edits in this group
        self.edit_group_id += 1;
    }

    /// End a logical grouping of command line edits that should be undone/redone together.
    pub fn end_edit_group(&mut self) {
        match self.edit_group_level.as_mut() {
            Some(edit_group_level) => {
                *edit_group_level -= 1;
                if *edit_group_level > 0 {
                    return;
                }
            }
            None => {
                // Prevent unbalanced end_edit_group() calls from breaking everything.
                return;
            }
        }

        self.edit_group_level = None;
        self.undo_history.try_coalesce = false;
        self.undo_history.may_coalesce = false;
    }

    /// Whether we want to append this string to the previous edit.
    fn want_to_coalesce_insertion_of(&self, s: &wstr) -> bool {
        // The previous edit must support coalescing.
        if !self.undo_history.may_coalesce {
            return false;
        }
        // Only consolidate single character inserts.
        if s.len() != 1 {
            return false;
        }
        // Make an undo group after every space.
        if s.as_char_slice()[0] == ' ' && !self.undo_history.try_coalesce {
            return false;
        }
        let last_edit = self.undo_history.edits.last().unwrap();
        // Don't add to the last edit if it deleted something.
        if !last_edit.range.is_empty() {
            return false;
        }
        // Must not have moved the cursor!
        if cursor_position_after_edit(last_edit) != self.position() {
            return false;
        }
        true
    }
}

/// Returns the number of characters left of the cursor that are removed by the
/// deletion in the given edit.
fn chars_deleted_left_of_cursor(edit: &Edit) -> usize {
    if edit.cursor_position_before_edit > edit.range.start {
        return std::cmp::min(
            edit.range.len(),
            edit.cursor_position_before_edit - edit.range.start,
        );
    }
    0
}

/// Compute the position of the cursor after the given edit.
fn cursor_position_after_edit(edit: &Edit) -> usize {
    let cursor = edit.cursor_position_before_edit + edit.replacement.len();
    let removed = chars_deleted_left_of_cursor(edit);
    cursor.saturating_sub(removed)
}
