//! Pager support.

use std::borrow::Cow;
use std::collections::HashMap;
use std::collections::hash_map::Entry;

use crate::common::{
    EscapeFlags, EscapeStringStyle, escape_string, get_ellipsis_char, get_ellipsis_str,
};
use crate::complete::{CompleteFlags, Completion};
use crate::editable_line::EditableLine;
use crate::highlight::{HighlightRole, HighlightSpec, highlight_shell};
use crate::operation_context::OperationContext;
use crate::prelude::*;
use crate::screen::{CharOffset, Line, ScreenData, wcswidth_rendered, wcwidth_rendered};
use crate::termsize::Termsize;
use fish_wcstringutil::string_fuzzy_match_string;

/// Represents rendering from the pager.
#[derive(Default)]
pub struct PageRendering {
    pub term_width: Option<usize>,
    pub term_height: Option<usize>,
    pub rows: usize,
    pub cols: usize,
    pub row_start: usize,
    pub row_end: usize,
    pub selected_completion_idx: Option<usize>,
    pub screen_data: ScreenData,

    pub remaining_to_disclose: usize,

    pub search_field_shown: bool,
    pub search_field_line: EditableLine,
}

impl PageRendering {
    // Returns a rendering with invalid data, useful to indicate "no rendering".
    pub fn new() -> Self {
        Default::default()
    }
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum SelectionMotion {
    // Visual directions.
    North,
    East,
    South,
    West,
    PageNorth,
    PageSouth,

    // Logical directions.
    Next,
    Prev,

    // Special value that means deselect.
    Deselect,
}

/// The space between adjacent completions.
const PAGER_SPACER_STRING: &wstr = L!("  ");

// How many rows we will show in the "initial" pager.
const PAGER_UNDISCLOSED_MAX_ROWS: usize = 4;

/// The minimum width (in characters) the terminal must to show completions at all.
const PAGER_MIN_WIDTH: usize = 16;

/// Minimum height to show completions
pub const PAGER_MIN_HEIGHT: usize = 4;

/// The maximum number of columns of completion to attempt to fit onto the screen.
const PAGER_MAX_COLS: usize = 6;

/// Width of the search field.
const PAGER_SEARCH_FIELD_WIDTH: usize = 12;

localizable_consts!(
    /// Text we use for the search field.
    SEARCH_FIELD_PROMPT
    "search: "
);

const PAGER_SELECTION_NONE: usize = usize::MAX;

#[derive(Default)]
pub struct Pager {
    pub available_term_width: usize,
    pub available_term_height: usize,

    pub selected_completion_idx: Option<usize>,
    pub suggested_row_start: usize,

    // Fully disclosed means that we show all completions.
    pub fully_disclosed: bool,

    // Whether we show the search field.
    pub search_field_shown: bool,

    // The filtered list of completion infos.
    completion_infos: Vec<PagerComp>,

    // The unfiltered list. Note there's a lot of duplication here.
    unfiltered_completion_infos: Vec<PagerComp>,

    // This tracks if the completion list has been changed since we last rendered. If yes,
    // then we definitely need to re-render.
    have_unrendered_completions: bool,

    prefix: Cow<'static, wstr>,
    highlight_prefix: bool,

    // The text of the search field.
    pub search_field_line: EditableLine,

    // Extra text to display at the bottom of the pager.
    pub extra_progress_text: WString,
}

impl Pager {
    // Returns the index of the completion that should draw selected, using the given number of
    // columns.
    pub fn visual_selected_completion_index(&self, rows: usize, cols: usize) -> Option<usize> {
        // No completions -> no selection.
        if self.completion_infos.is_empty() {
            return None;
        }

        let result = self.selected_completion_idx;
        if result == Some(0) {
            return result;
        }

        if rows == 0 || cols == 0 {
            return None;
        }
        result.map(|mut result| {
            // If the selected completion is beyond the last selection, go left by columns until it's
            // within it. This is how we implement "column memory".
            while result >= self.completion_infos.len() && result >= rows {
                result -= rows;
            }

            // If we are still beyond the last selection, clamp it.
            if result >= self.completion_infos.len() {
                result = self.completion_infos.len() - 1;
            }
            result
        })
    }

    /// Try to print the list of completions lst with the prefix prefix using cols as the number of
    /// columns. Return true if the completion list was printed, false if the terminal is too narrow for
    /// the specified number of columns. Always succeeds if cols is 1.
    fn completion_try_print(
        &self,
        cols: usize,
        prefix: &wstr,
        lst: &[PagerComp],
        rendering: &mut PageRendering,
        suggested_start_row: usize,
    ) -> bool {
        assert!(cols > 0);
        // The calculated preferred width of each column.
        let mut width_by_column = [0; PAGER_MAX_COLS];

        // Skip completions on tiny terminals.
        if self.available_term_width < PAGER_MIN_WIDTH
            || self.available_term_height < PAGER_MIN_HEIGHT
        {
            return true;
        }

        // Compute the effective term width and term height, accounting for disclosure.
        let term_width = self.available_term_width;
        let mut term_height = self.available_term_height
            // we always subtract 1 to make room for a comment row
                - 1 - if self.search_field_shown { 1 } else { 0 };
        if !self.fully_disclosed {
            // We disclose between half and the entirety of the terminal height,
            // but at least 4 rows.
            //
            // We do this so we show a useful amount but don't force fish to
            // THE VERY TOP, which is jarring.
            term_height = std::cmp::min(
                term_height,
                std::cmp::max(term_height / 2, PAGER_UNDISCLOSED_MAX_ROWS),
            );
        }

        let row_count = divide_round_up(lst.len(), cols);

        // We have more to disclose if we are not fully disclosed and there's more rows than we have in
        // our term height.
        if !self.fully_disclosed && row_count > term_height {
            rendering.remaining_to_disclose = row_count - term_height;
        } else {
            rendering.remaining_to_disclose = 0;
        }

        // If we have only one row remaining to disclose, then squelch the comment row. This prevents us
        // from consuming a line to show "...and 1 more row".
        if rendering.remaining_to_disclose == 1 {
            term_height += 1;
            rendering.remaining_to_disclose = 0;
        }

        // Calculate how wide the list would be.
        for (col, col_width) in width_by_column.iter_mut().enumerate() {
            for row in 0..row_count {
                let comp_idx = col * row_count + row;
                if comp_idx >= lst.len() {
                    continue;
                }
                let c = &lst[comp_idx];
                *col_width = std::cmp::max(*col_width, c.preferred_width());
            }
        }

        let print = if cols == 1 {
            // Force fit if one column.
            width_by_column[0] = std::cmp::min(width_by_column[0], term_width);
            true
        } else {
            // Compute total preferred width, plus spacing
            let mut total_width_needed: usize = width_by_column.iter().sum();
            total_width_needed += (cols - 1) * PAGER_SPACER_STRING.len();
            total_width_needed <= term_width
        };
        if !print {
            return false; // no need to continue
        }

        // Determine the starting and stop row.
        let start_row;
        let stop_row;
        if row_count <= term_height {
            // Easy, we can show everything.
            start_row = 0;
            stop_row = row_count;
        } else {
            // We can only show part of the full list. Determine which part based on the
            // suggested_start_row.
            assert!(row_count > term_height);
            let last_starting_row = row_count - term_height;
            start_row = std::cmp::min(suggested_start_row, last_starting_row);
            stop_row = start_row + term_height;
            assert!(start_row <= last_starting_row);
        }

        assert!(stop_row >= start_row);
        assert!(stop_row <= row_count);
        assert!(stop_row - start_row <= term_height);
        // This always printed at the end of the command line.
        self.completion_print(
            cols,
            &width_by_column,
            start_row,
            stop_row,
            prefix,
            lst,
            rendering,
        );

        // Add the progress line. It's a "more to disclose" line if necessary, or a row listing if
        // it's scrollable; otherwise ignore it.
        // We should never have one row remaining to disclose (else we would have just disclosed it)
        let mut progress_text = WString::new();
        assert_ne!(rendering.remaining_to_disclose, 1);
        if rendering.remaining_to_disclose > 1 {
            progress_text = wgettext_fmt!(
                "%sand %u more rows",
                get_ellipsis_str(),
                rendering.remaining_to_disclose
            );
        } else if start_row > 0 || stop_row < row_count {
            // We have a scrollable interface. The +1 here is because we are zero indexed, but want
            // to present things as 1-indexed. We do not add 1 to stop_row or row_count because
            // these are the "past the last value".
            progress_text =
                wgettext_fmt!("rows %u to %u of %u", start_row + 1, stop_row, row_count);
        } else if self.search_field_shown && self.completion_infos.is_empty() {
            // Everything is filtered.
            progress_text = wgettext!("(no matches)").to_owned();
        }
        if !self.extra_progress_text.is_empty() {
            if !progress_text.is_empty() {
                progress_text.push_str(". ");
            }
            progress_text.push_utfstr(&self.extra_progress_text);
        }

        if !progress_text.is_empty() {
            let line = rendering.screen_data.add_line();
            let spec = HighlightSpec::with_both(HighlightRole::pager_progress);
            print_max(
                CharOffset::None,
                &progress_text,
                spec,
                term_width,
                /*has_more=*/ true,
                line,
            );
        }

        if !self.search_field_shown {
            return true;
        }

        // Add the search field.
        let mut search_field_text = self.search_field_line.text().to_owned();
        // Append spaces to make it at least the required width.
        if search_field_text.len() < PAGER_SEARCH_FIELD_WIDTH {
            search_field_text.extend(std::iter::repeat_n(
                ' ',
                PAGER_SEARCH_FIELD_WIDTH - search_field_text.len(),
            ));
        }
        let search_field = rendering.screen_data.insert_line_at_index(0);

        // We limit the width to term_width - 1.
        let mut underline = HighlightSpec::new();
        underline.force_underline = true;

        let mut search_field_remaining = term_width - 1;
        search_field_remaining -= print_max(
            CharOffset::None,
            wgettext!(SEARCH_FIELD_PROMPT),
            HighlightSpec::new(),
            search_field_remaining,
            false,
            search_field,
        );
        search_field_remaining -= print_max(
            CharOffset::None,
            &search_field_text,
            underline,
            search_field_remaining,
            false,
            search_field,
        );
        let _ = search_field_remaining;
        true
    }

    fn measure_completion_infos(&mut self) {
        let prefix_len = wcswidth_rendered(&self.prefix);
        for comp in &mut self.unfiltered_completion_infos {
            let comp_strings = &mut comp.comp;

            let show_prefix = !comp
                .representative
                .flags
                .contains(CompleteFlags::SUPPRESS_PAGER_PREFIX);

            for (j, comp_string) in comp_strings.iter().enumerate() {
                // If there's more than one, append the length of ', '.
                if j >= 1 {
                    comp.comp_width += 2;
                }

                // This can return -1 if it can't calculate the width. So be cautious.
                let comp_width = wcswidth_rendered(comp_string);
                if show_prefix {
                    comp.comp_width += usize::try_from(prefix_len).unwrap_or_default();
                }
                comp.comp_width += usize::try_from(comp_width).unwrap_or_default();
            }

            // This can return -1 if it can't calculate the width. So be cautious.
            let desc_width = wcswidth_rendered(&comp.desc);
            comp.desc_width = usize::try_from(desc_width).unwrap_or_default();
        }
    }

    // Indicates if the given completion info passes any filtering we have.
    fn completion_info_passes_filter(&self, info: &PagerComp) -> bool {
        // If we have no filter, everything passes.
        if !self.search_field_shown || self.search_field_line.is_empty() {
            return true;
        }

        let needle = self.search_field_line.text();

        // Match against the description.
        if string_fuzzy_match_string(needle, &info.desc, false).is_some() {
            return true;
        }

        // Match against the completion strings.
        for candidate in &info.comp {
            if string_fuzzy_match_string(
                needle,
                &(self.prefix.clone().into_owned() + &candidate[..]),
                false,
            )
            .is_some()
            {
                return true;
            }
        }
        false // no match
    }

    /// Print the specified part of the completion list, using the specified column offsets and quoting
    /// style.
    ///
    /// \param cols number of columns to print in
    /// \param width_by_column An array specifying the width of each column
    /// \param row_start The first row to print
    /// \param row_stop the row after the last row to print
    /// \param prefix The string to print before each completion
    /// \param lst The list of completions to print
    #[allow(clippy::too_many_arguments)]
    fn completion_print(
        &self,
        cols: usize,
        width_by_column: &[usize; PAGER_MAX_COLS],
        row_start: usize,
        row_stop: usize,
        prefix: &wstr,
        lst: &[PagerComp],
        rendering: &mut PageRendering,
    ) {
        // Teach the rendering about the rows it printed.
        assert!(row_stop >= row_start);
        rendering.row_start = row_start;
        rendering.row_end = row_stop;

        let rows = divide_round_up(lst.len(), cols);

        let effective_selected_idx = self.visual_selected_completion_index(rows, cols);

        for row in row_start..row_stop {
            for (col, col_width) in width_by_column.iter().copied().enumerate() {
                let idx = col * rows + row;
                if lst.len() <= idx {
                    continue;
                }

                let el = &lst[idx];
                let is_selected = Some(idx) == effective_selected_idx;

                // Print this completion on its own "line".
                let show_prefix = !el
                    .representative
                    .flags
                    .contains(CompleteFlags::SUPPRESS_PAGER_PREFIX);
                let mut line = self.completion_print_item(
                    CharOffset::Pager(idx),
                    show_prefix.then_some(prefix),
                    el,
                    col_width,
                    row % 2 != 0,
                    is_selected,
                );

                // If there's more to come, append two spaces.
                if col + 1 < cols {
                    line.append_str(PAGER_SPACER_STRING, HighlightSpec::new(), CharOffset::None);
                }

                // Append this to the real line.
                rendering
                    .screen_data
                    .create_line(row - row_start)
                    .append_line(&line);
            }
        }
    }

    /// Print the specified item using at the specified amount of space.
    fn completion_print_item(
        &self,
        offset_in_cmdline: CharOffset,
        prefix: Option<&wstr>,
        c: &PagerComp,
        width: usize,
        secondary: bool,
        selected: bool,
    ) -> Line {
        let mut comp_width;
        let mut line_data = Line::new();

        if c.preferred_width() <= width {
            // The entry fits, we give it as much space as it wants.
            comp_width = c.comp_width;
        } else {
            // The completion and description won't fit on the allocated space. Give a maximum of 2/3 of
            // the space to the completion, and whatever is left to the description
            // This expression is an overflow-safe way of calculating (width-4)*2/3
            let width_minus_spacer = width.saturating_sub(4);
            let two_thirds_width =
                (width_minus_spacer / 3) * 2 + ((width_minus_spacer % 3) * 2) / 3;
            comp_width = std::cmp::min(c.comp_width, two_thirds_width);

            // If the description is short, give the completion the remaining space
            let desc_punct_width = c.description_punctuated_width();
            if width > desc_punct_width {
                comp_width = std::cmp::max(comp_width, width - desc_punct_width);
            }

            // The description gets what's left
            assert!(comp_width <= width);
        }

        let modify_role = |role: HighlightRole| {
            let mut base = role as u8;
            if selected {
                base += HighlightRole::pager_selected_background as u8
                    - HighlightRole::pager_background as u8;
            } else if secondary {
                base += HighlightRole::pager_secondary_background as u8
                    - HighlightRole::pager_background as u8;
            }
            unsafe { std::mem::transmute(base) }
        };

        let bg_role = modify_role(HighlightRole::pager_background);
        let bg = HighlightSpec::with_bg(bg_role);
        let prefix_col = HighlightSpec::with_fg_bg(
            modify_role(if self.highlight_prefix {
                HighlightRole::pager_prefix
            } else {
                HighlightRole::pager_completion
            }),
            bg_role,
        );
        let comp_col =
            HighlightSpec::with_fg_bg(modify_role(HighlightRole::pager_completion), bg_role);
        let desc_col =
            HighlightSpec::with_fg_bg(modify_role(HighlightRole::pager_description), bg_role);

        // Print the completion part
        let mut comp_remaining = comp_width;
        for (i, comp) in c.comp.iter().enumerate() {
            if i > 0 {
                comp_remaining -= print_max(
                    offset_in_cmdline,
                    PAGER_SPACER_STRING,
                    bg,
                    comp_remaining,
                    /*has_more=*/ true,
                    &mut line_data,
                );
            }

            if let Some(prefix) = prefix {
                comp_remaining -= print_max(
                    offset_in_cmdline,
                    prefix,
                    prefix_col,
                    comp_remaining,
                    !comp.is_empty(),
                    &mut line_data,
                );
            }
            comp_remaining -= print_max_impl(
                offset_in_cmdline,
                comp,
                |i| {
                    if c.colors.is_empty() {
                        return comp_col; // Not a shell command.
                    }
                    if selected {
                        // Rendered in reverse video, so avoid highlighting.
                        return comp_col;
                    }
                    *c.colors.get(i).unwrap_or(c.colors.last().unwrap())
                },
                comp_remaining,
                i + 1 < c.comp.len(),
                &mut line_data,
            );
        }

        let mut desc_remaining = width - comp_width + comp_remaining;
        if c.desc_width > 0 && desc_remaining > 4 {
            // always have at least two spaces to separate completion and description
            desc_remaining -= print_max(offset_in_cmdline, L!("  "), bg, 2, false, &mut line_data);

            // right-justify the description by adding spaces
            // the 2 here refers to the parenthesis below
            while desc_remaining > c.desc_width + 2 {
                desc_remaining -=
                    print_max(offset_in_cmdline, L!(" "), bg, 1, false, &mut line_data);
            }

            assert!(desc_remaining >= 2);
            let paren_col = HighlightSpec::with_fg_bg(
                if selected {
                    HighlightRole::pager_selected_completion
                } else {
                    HighlightRole::pager_completion
                },
                bg_role,
            );
            desc_remaining -= print_max(
                offset_in_cmdline,
                L!("("),
                paren_col,
                1,
                false,
                &mut line_data,
            );
            desc_remaining -= print_max(
                offset_in_cmdline,
                &c.desc,
                desc_col,
                desc_remaining - 1,
                false,
                &mut line_data,
            );
            desc_remaining -= print_max(
                offset_in_cmdline,
                L!(")"),
                paren_col,
                1,
                false,
                &mut line_data,
            );
            let _ = desc_remaining;
        } else {
            // No description, or it won't fit. Just add spaces.
            print_max(
                offset_in_cmdline,
                &WString::from_iter(std::iter::repeat_n(' ', desc_remaining)),
                bg,
                desc_remaining,
                false,
                &mut line_data,
            );
        }

        line_data
    }

    // Sets the set of completions.
    pub fn set_completions(&mut self, raw_completions: &[Completion], enable_refilter: bool) {
        self.selected_completion_idx = None;
        // Get completion infos out of it.
        self.unfiltered_completion_infos = process_completions_into_infos(raw_completions);

        // Maybe join them.
        if *self.prefix == "-" {
            join_completions(&mut self.unfiltered_completion_infos);
        }

        // Compute their various widths.
        self.measure_completion_infos();

        // Refilter them.
        if enable_refilter {
            self.refilter_completions();
        } else {
            self.completion_infos = self.unfiltered_completion_infos.clone();
        }
        self.have_unrendered_completions = true;
    }

    // Sets the prefix.
    pub fn set_prefix(&mut self, prefix: Cow<'static, wstr>, highlight: bool /* = true */) {
        self.prefix = prefix;
        self.highlight_prefix = highlight;
    }

    // Sets the terminal size.
    pub fn set_term_size(&mut self, ts: &Termsize) {
        self.available_term_width = ts.width();
        self.available_term_height = ts.height();
    }

    // Changes the selected completion in the given direction according to the layout of the given
    // rendering. Returns true if the selection changed.
    pub fn select_next_completion_in_direction(
        &mut self,
        direction: SelectionMotion,
        rendering: &PageRendering,
    ) -> bool {
        // Must have something to select.
        if self.completion_infos.is_empty() {
            return false;
        }

        match self.selected_completion_idx {
            None => {
                // Handle the case of nothing selected yet.
                match direction {
                    SelectionMotion::South | SelectionMotion::PageSouth | SelectionMotion::Next => {
                        self.selected_completion_idx = Some(0)
                    }
                    SelectionMotion::North | SelectionMotion::Prev => {
                        self.selected_completion_idx = Some(self.completion_infos.len() - 1)
                    }
                    SelectionMotion::East
                    | SelectionMotion::West
                    | SelectionMotion::PageNorth
                    | SelectionMotion::Deselect => {
                        // These do nothing.
                        return false;
                    }
                }
            }
            Some(selected_completion_idx) => {
                // Ok, we had something selected already. Select something different.
                let new_selected_completion_idx;
                if !selection_direction_is_cardinal(direction) {
                    // Next, previous, or deselect, all easy.
                    if direction == SelectionMotion::Deselect {
                        new_selected_completion_idx = None;
                    } else if direction == SelectionMotion::Next {
                        let mut new_idx = selected_completion_idx + 1;
                        if new_idx >= self.completion_infos.len() {
                            new_idx = 0;
                        }
                        new_selected_completion_idx = Some(new_idx);
                    } else if direction == SelectionMotion::Prev {
                        if selected_completion_idx == 0 {
                            new_selected_completion_idx = Some(self.completion_infos.len() - 1);
                        } else {
                            new_selected_completion_idx = if selected_completion_idx != 0 {
                                Some(selected_completion_idx - 1)
                            } else {
                                None
                            };
                        }
                    } else {
                        unreachable!("unknown non-cardinal direction");
                    }
                } else {
                    // Cardinal directions. We have a completion index; we wish to compute its row and
                    // column.
                    let mut current_row = self
                        .get_selected_row(rendering)
                        .unwrap_or(PAGER_SELECTION_NONE);
                    let mut current_col = self
                        .get_selected_column(rendering)
                        .unwrap_or(PAGER_SELECTION_NONE);
                    let page_height = std::cmp::max(rendering.term_height.unwrap() - 1, 1);

                    match direction {
                        SelectionMotion::PageNorth => {
                            if current_row > page_height {
                                current_row -= page_height;
                            } else {
                                current_row = 0;
                            }
                        }
                        SelectionMotion::North => {
                            // Go up a whole row. If we cycle, go to the previous column.
                            if current_row > 0 {
                                current_row -= 1;
                            } else {
                                current_row = rendering.rows - 1;
                                if current_col > 0 {
                                    current_col -= 1;
                                } else {
                                    current_col = rendering.cols - 1;
                                }
                            }
                        }
                        SelectionMotion::PageSouth => {
                            if current_row + page_height < rendering.rows {
                                current_row += page_height;
                            } else {
                                current_row = rendering.rows - 1;
                                if current_col * rendering.rows + current_row
                                    >= self.completion_infos.len()
                                {
                                    current_row =
                                        (self.completion_infos.len() - 1) % rendering.rows;
                                }
                            }
                        }
                        SelectionMotion::South => {
                            // Go down, unless we are in the last row.
                            // If we go over the last element, wrap to the first.
                            if current_row + 1 < rendering.rows
                                && current_col * rendering.rows + current_row + 1
                                    < self.completion_infos.len()
                            {
                                current_row += 1;
                            } else {
                                current_row = 0;
                                current_col = (current_col + 1) % rendering.cols;
                            }
                        }
                        SelectionMotion::East => {
                            // Go east, wrapping to the next row. There is no "row memory," so if we run off
                            // the end, wrap.
                            if current_col + 1 < rendering.cols
                                && (current_col + 1) * rendering.rows + current_row
                                    < self.completion_infos.len()
                            {
                                current_col += 1;
                            } else {
                                current_col = 0;
                                current_row = (current_row + 1) % rendering.rows;
                            }
                        }
                        SelectionMotion::West => {
                            // Go west, wrapping to the previous row.
                            if current_col > 0 {
                                current_col -= 1;
                            } else {
                                current_col = rendering.cols - 1;
                                if current_row > 0 {
                                    current_row -= 1;
                                } else {
                                    current_row = rendering.rows - 1;
                                }
                            }
                        }
                        SelectionMotion::Next
                        | SelectionMotion::Prev
                        | SelectionMotion::Deselect => (),
                    }

                    // Compute the new index based on the changed row.
                    new_selected_completion_idx = Some(current_col * rendering.rows + current_row);
                }

                if Some(selected_completion_idx) == new_selected_completion_idx {
                    return false;
                }
                self.selected_completion_idx = new_selected_completion_idx;
            }
        }

        // Update suggested_row_start to ensure the selection is visible. suggested_row_start *
        // rendering.cols is the first suggested visible completion; add the visible completion
        // count to that to get the last one.
        let visible_row_count = rendering.row_end - rendering.row_start;
        if visible_row_count == 0 {
            return true; // this happens if there was no room to draw the pager
        }
        if self.selected_completion_idx.is_none() {
            return true;
        }

        // Ensure our suggested row start is not past the selected row.
        let row_containing_selection = self
            .get_selected_row_given_rows(rendering.rows)
            .unwrap_or(PAGER_SELECTION_NONE);
        if self.suggested_row_start > row_containing_selection {
            self.suggested_row_start = row_containing_selection;
        }

        // Ensure our suggested row start is not too early before it.
        if self.suggested_row_start + visible_row_count <= row_containing_selection {
            // The user moved south past the bottom completion.
            if matches!(direction, SelectionMotion::South | SelectionMotion::East)
                && !self.fully_disclosed
                && rendering.remaining_to_disclose > 0
            {
                self.fully_disclosed = true; // perform disclosure
            } else {
                // Scroll
                self.suggested_row_start = row_containing_selection - visible_row_count + 1;
                // Ensure fully_disclosed is set. I think we can hit this case if the user
                // resizes the window - we don't want to drop back to the disclosed style.
                self.fully_disclosed = true;
            }
        }

        true
    }

    // Returns the currently selected completion for the given rendering.
    pub fn selected_completion(&'_ self, rendering: &PageRendering) -> Option<&'_ Completion> {
        self.visual_selected_completion_index(rendering.rows, rendering.cols)
            .map(|idx| &self.completion_infos[idx].representative)
    }

    pub fn selected_completion_index(&self) -> Option<usize> {
        self.selected_completion_idx
    }
    pub fn set_selected_completion_index(&mut self, mut new_index: Option<usize>) {
        // Current users are off by one at most.
        assert!(new_index.is_none_or(|new_index| new_index <= self.completion_infos.len()));
        if self.completion_infos.is_empty() {
            return;
        }
        if new_index == Some(self.completion_infos.len()) {
            new_index = Some(self.completion_infos.len() - 1);
        }
        self.selected_completion_idx = new_index;
    }

    // Indicates the row and column for the given rendering. Returns -1 if no selection.
    pub fn get_selected_row(&self, rendering: &PageRendering) -> Option<usize> {
        if rendering.rows == 0 {
            return None;
        }

        rendering
            .selected_completion_idx
            .map(|idx| idx % rendering.rows)
    }
    pub fn get_selected_column(&self, rendering: &PageRendering) -> Option<usize> {
        if rendering.rows == 0 {
            return None;
        }
        rendering
            .selected_completion_idx
            .map(|idx| idx / rendering.rows)
    }
    // Indicates the row assuming we render this many rows. Returns -1 if no selection.
    pub fn get_selected_row_given_rows(&self, rows: usize) -> Option<usize> {
        if rows == 0 {
            return None;
        }

        self.selected_completion_idx.map(|idx| idx % rows)
    }

    // Produces a rendering of the completions, at the given term size.
    pub fn render(&self) -> PageRendering {
        // Try to print the completions. Start by trying to print the list in PAGER_MAX_COLS columns,
        // if the completions won't fit, reduce the number of columns by one. Printing a single column
        // never fails.
        let mut rendering = PageRendering::new();
        rendering.term_width = Some(self.available_term_width);
        rendering.term_height = Some(self.available_term_height);
        rendering.search_field_line = self.search_field_line.clone();
        for cols in (1..=PAGER_MAX_COLS).rev() {
            // Initially empty rendering.
            rendering.screen_data.clear_lines();

            // Determine how many rows we would need if we had 'cols' columns. Then determine how many
            // columns we want from that. For example, say we had 19 completions. We can fit them into 6
            // columns, 4 rows, with the last row containing only 1 entry. Or we can fit them into 5
            // columns, 4 rows, the last row containing 4 entries. Since fewer columns with the same
            // number of rows is better, skip cases where we know we can do better.
            let min_rows_required_for_cols = divide_round_up(self.completion_infos.len(), cols);
            let min_cols_required_for_rows =
                divide_round_up(self.completion_infos.len(), min_rows_required_for_cols);

            assert!(min_cols_required_for_rows <= cols);
            if cols > 1 && min_cols_required_for_rows < cols {
                // Next iteration will be better, so skip this one.
                continue;
            }

            rendering.cols = cols;
            rendering.rows = min_rows_required_for_cols;
            rendering.selected_completion_idx =
                self.visual_selected_completion_index(rendering.rows, rendering.cols);

            if self.completion_try_print(
                cols,
                &self.prefix,
                &self.completion_infos,
                &mut rendering,
                self.suggested_row_start,
            ) {
                break;
            }
        }
        rendering
    }

    // Return true if the given rendering needs to be updated.
    pub fn rendering_needs_update(&self, rendering: &PageRendering) -> bool {
        if self.have_unrendered_completions {
            return true;
        }
        // Common case is no pager.
        if self.is_empty() && rendering.screen_data.is_empty() {
            return false;
        }

        (self.is_empty() && !rendering.screen_data.is_empty()) ||    // Do update after clear().
           rendering.term_width != Some(self.available_term_width) ||
           rendering.term_height != Some(self.available_term_height) ||
           rendering.selected_completion_idx !=
               self.visual_selected_completion_index(rendering.rows, rendering.cols) ||
           rendering.search_field_shown != self.search_field_shown ||
           *rendering.search_field_line.text() != *self.search_field_line.text() ||
           rendering.search_field_line.position() != self.search_field_line.position() ||
           (rendering.remaining_to_disclose > 0 && self.fully_disclosed)
    }

    // Updates the rendering.
    pub fn update_rendering(&mut self, rendering: &mut PageRendering) {
        if self.rendering_needs_update(rendering) {
            *rendering = self.render();
            self.have_unrendered_completions = false;
        }
    }

    // Indicates if there are no completions, and therefore nothing to render.
    pub fn is_empty(&self) -> bool {
        self.unfiltered_completion_infos.is_empty()
    }

    // Clears all completions and the prefix.
    pub fn clear(&mut self) {
        self.unfiltered_completion_infos.clear();
        self.completion_infos.clear();
        self.prefix = Cow::Borrowed(L!(""));
        self.highlight_prefix = false;
        self.selected_completion_idx = None;
        self.fully_disclosed = false;
        self.search_field_shown = false;
        self.extra_progress_text.clear();
        self.suggested_row_start = 0;
    }

    // Updates the completions list per the filter.
    pub fn refilter_completions(&mut self) {
        self.completion_infos.clear();
        for comp in &self.unfiltered_completion_infos {
            if self.completion_info_passes_filter(comp) {
                self.completion_infos.push(comp.clone());
            }
        }
    }

    // Sets whether the search field is shown.
    pub fn set_search_field_shown(&mut self, flag: bool) {
        self.search_field_shown = flag;
    }

    // Gets whether the search field shown.
    pub fn is_search_field_shown(&self) -> bool {
        self.search_field_shown
    }

    // Indicates if we are navigating our contents.
    // It's possible we have no visual selection but are still navigating the contents, e.g. every
    // completion is filtered.
    pub fn is_navigating_contents(&self) -> bool {
        self.selected_completion_idx.is_some()
    }

    // Become fully disclosed.
    pub fn set_fully_disclosed(&mut self) {
        self.fully_disclosed = true;
    }

    // Position of the cursor.
    pub fn cursor_position(&self) -> usize {
        let mut result = wgettext!(SEARCH_FIELD_PROMPT).len() + self.search_field_line.position();
        // Clamp it to the right edge.
        if self.available_term_width > 0 && result + 1 > self.available_term_width {
            result = self.available_term_width - 1;
        }
        result
    }
}

/// Data structure describing one or a group of related completions.
#[derive(Clone, Default)]
pub struct PagerComp {
    /// The list of all completion strings this entry applies to.
    pub comp: Vec<WString>,
    /// The description.
    pub desc: WString,
    /// The representative completion.
    pub representative: Completion,
    /// The per-character highlighting, used when this is a full shell command.
    pub colors: Vec<HighlightSpec>,
    /// On-screen width of the completion string.
    pub comp_width: usize,
    /// On-screen width of the description information.
    pub desc_width: usize,
}

impl PagerComp {
    // Our text looks like this:
    // completion  (description)
    // Two spaces separating, plus parens, yields 4 total extra space
    // but only if we have a description of course
    pub fn description_punctuated_width(&self) -> usize {
        self.desc_width + if self.desc_width != 0 { 4 } else { 0 }
    }

    // Returns the preferred width, containing the sum of the
    // width of the completion, separator, description
    pub fn preferred_width(&self) -> usize {
        self.comp_width + self.description_punctuated_width()
    }
}

fn selection_direction_is_cardinal(dir: SelectionMotion) -> bool {
    match dir {
        SelectionMotion::North
        | SelectionMotion::East
        | SelectionMotion::South
        | SelectionMotion::West
        | SelectionMotion::PageNorth
        | SelectionMotion::PageSouth => true,
        SelectionMotion::Next | SelectionMotion::Prev | SelectionMotion::Deselect => false,
    }
}

/// Returns numer / denom, rounding up. As a "courtesy" 0/0 is 0.
fn divide_round_up(numer: usize, denom: usize) -> usize {
    if numer == 0 {
        return 0;
    }
    assert_ne!(denom, 0);
    let has_rem = (numer % denom) != 0;
    numer / denom + if has_rem { 1 } else { 0 }
}

/// Print the specified string, but use at most the specified amount of space. If the whole string
/// can't be fitted, ellipsize it.
///
/// \param str the string to print
/// \param color the color to apply to every printed character
/// \param max the maximum space that may be used for printing
/// \param has_more if this flag is true, this is not the entire string, and the string should be
/// ellipsized even if the string fits but takes up the whole space.
fn print_max_impl(
    offset_in_cmdline: CharOffset,
    s: &wstr,
    color: impl Fn(usize) -> HighlightSpec,
    max: usize,
    has_more: bool,
    line: &mut Line,
) -> usize {
    let mut remaining = max;
    for (i, c) in s.chars().enumerate() {
        let iwidth_c = wcwidth_rendered(c);
        let Ok(width_c) = usize::try_from(iwidth_c) else {
            // skip non-printable characters
            continue;
        };

        if width_c > remaining {
            break;
        }

        let ellipsis = get_ellipsis_char();
        if (width_c == remaining) && (has_more || i + 1 < s.len()) {
            line.append(ellipsis, color(i), offset_in_cmdline);
            let ellipsis_width = wcwidth_rendered(ellipsis);
            remaining = remaining.saturating_sub(usize::try_from(ellipsis_width).unwrap());
            break;
        }

        line.append(c, color(i), offset_in_cmdline);
        remaining = remaining.checked_sub(width_c).unwrap();
    }

    // return how much we consumed
    max.checked_sub(remaining).unwrap()
}

fn print_max(
    offset_in_cmdline: CharOffset,
    s: &wstr,
    color: HighlightSpec,
    max: usize,
    has_more: bool,
    line: &mut Line,
) -> usize {
    print_max_impl(offset_in_cmdline, s, |_| color, max, has_more, line)
}

/// Trim leading and trailing whitespace, and compress other whitespace runs into a single space.
fn mangle_1_completion_description(s: &mut WString) {
    let mut leading = 0;
    let mut trailing = 0;
    let len = s.len();

    // Skip leading spaces.
    while leading < len {
        if !s.char_at(leading).is_whitespace() {
            break;
        }
        leading += 1;
    }

    // Compress runs of spaces to a single space.
    let mut was_space = false;
    while leading < s.len() {
        let wc = s.char_at(leading);
        let is_space = wc.is_whitespace();
        if !is_space {
            // normal character
            s.as_char_slice_mut()[trailing] = wc;
            trailing += 1;
        } else if !was_space {
            // initial space in a run
            s.as_char_slice_mut()[trailing] = ' ';
            trailing += 1;
        } // else non-initial space in a run, do nothing
        was_space = is_space;
        leading += 1;
    }

    // leading is now at len, trailing is the new length of the string. Delete trailing spaces.
    while trailing > 0 && s.char_at(trailing - 1).is_whitespace() {
        trailing -= 1;
    }

    s.truncate(trailing);
}

fn join_completions(comps: &mut Vec<PagerComp>) {
    // A map from description to index in the completion list of the element with that description.
    // The indexes are stored +1.
    let mut desc_table: HashMap<WString, usize> = HashMap::new();

    // Note that we mutate the completion list as we go, so the size changes.
    let mut i = 0;
    while i < comps.len() {
        if comps[i].desc.is_empty() {
            i += 1;
            continue;
        }

        // See if it's in the table.
        match desc_table.entry(comps[i].desc.clone()) {
            Entry::Vacant(entry) => {
                // We're the first with this description.
                entry.insert(i + 1);
                i += 1;
            }
            Entry::Occupied(entry) => {
                // There's a prior completion with this description. Append the new ones to it.
                let prev_idx_plus_one = entry.get();
                // Erase the element at this index.
                let new_comp = comps.remove(i);
                let prior_comp = &mut comps[prev_idx_plus_one - 1];
                prior_comp.comp.extend(new_comp.comp);
            }
        }
    }
}

/// Generate a list of comp_t structures from a list of completions.
fn process_completions_into_infos(lst: &[Completion]) -> Vec<PagerComp> {
    // Make the list of the correct size up-front.
    let mut result = Vec::with_capacity(lst.len());
    for (i, comp) in lst.iter().enumerate() {
        result.push(PagerComp::default());
        let comp_info = &mut result[i];

        // Append the single completion string. We may later merge these into multiple.
        comp_info.comp.push(escape_string(
            &comp.completion,
            EscapeStringStyle::Script(
                EscapeFlags::NO_PRINTABLES | EscapeFlags::NO_QUOTED | EscapeFlags::SYMBOLIC,
            ),
        ));
        // HACK We want to render a full shell command, with syntax highlighting.  Above we
        // escape nonprintables, which might make the rendered command longer than the original
        // completion. In that case we get wrong colors.  However this should only happen in
        // contrived cases, since our symbolic escaping uses a single character to represent
        // newline and tab characters; other nonprintables are extremely rare in a command
        // line.
        // We should probably fix this by first highlighting the original completion, and
        // then writing a variant of escape_string() that adjusts highlighting according
        // so it matches the escaped string.
        if comp.replaces_line() {
            highlight_shell(
                &comp.completion,
                &mut comp_info.colors,
                &OperationContext::empty(),
                false,
                None,
            );
            assert!(comp_info.comp.last().unwrap().len() >= comp_info.colors.len());
        }

        // Append the mangled description.
        comp_info.desc = comp.description.clone();
        mangle_1_completion_description(&mut comp_info.desc);

        // Set the representative completion.
        comp_info.representative = comp.clone();
    }
    result
}

#[cfg(test)]
mod tests {
    use super::{Pager, SelectionMotion};
    use crate::common::get_ellipsis_char;
    use crate::complete::{CompleteFlags, Completion};
    use crate::prelude::*;
    use crate::termsize::Termsize;
    use crate::tests::prelude::*;
    use fish_wcstringutil::StringFuzzyMatch;
    use std::borrow::Cow;
    use std::num::NonZeroU16;

    #[test]
    #[serial]
    fn test_pager_navigation() {
        let _cleanup = test_init();
        // Generate 19 strings of width 10. There's 2 spaces between completions, and our term size is
        // 80; these can therefore fit into 6 columns (6 * 12 - 2 = 70) or 5 columns (58) but not 7
        // columns (7 * 12 - 2 = 82).
        //
        // You can simulate this test by creating 19 files named "file00.txt" through "file_18.txt".
        let mut completions = vec![];
        for _ in 0..19 {
            completions.push(Completion::new(
                L!("abcdefghij").to_owned(),
                "".into(),
                StringFuzzyMatch::exact_match(),
                CompleteFlags::default(),
            ));
        }

        let mut pager = Pager::default();
        pager.set_completions(&completions, true);
        pager.set_term_size(&Termsize::defaults());
        let mut render = pager.render();

        assert_eq!(render.term_width, Some(80));
        assert_eq!(render.term_height, Some(24));

        let rows = 4;
        let cols = 5;

        // We have 19 completions. We can fit into 6 columns with 4 rows or 5 columns with 4 rows; the
        // second one is better and so is what we ought to have picked.
        assert_eq!(render.rows, rows);
        assert_eq!(render.cols, cols);

        // Initially expect to have no completion index.
        assert!(render.selected_completion_idx.is_none());

        // Here are navigation directions and where we expect the selection to be.
        macro_rules! validate {
            ($pager:ident, $render:ident, $dir:expr, $sel:expr) => {
                $pager.select_next_completion_in_direction($dir, &$render);
                $pager.update_rendering(&mut $render);
                assert_eq!(
                    Some($sel),
                    $render.selected_completion_idx,
                    "For command {:?}",
                    $dir
                );
            };
        }

        // Tab completion to get into the list.
        validate!(pager, render, SelectionMotion::Next, 0);
        // Westward motion in upper left goes to the last filled column in the last row.
        validate!(pager, render, SelectionMotion::West, 15);
        // East goes back.
        validate!(pager, render, SelectionMotion::East, 0);
        validate!(pager, render, SelectionMotion::West, 15);
        validate!(pager, render, SelectionMotion::West, 11);
        validate!(pager, render, SelectionMotion::East, 15);
        validate!(pager, render, SelectionMotion::East, 0);
        // "Next" motion goes down the column.
        validate!(pager, render, SelectionMotion::Next, 1);
        validate!(pager, render, SelectionMotion::Next, 2);
        validate!(pager, render, SelectionMotion::West, 17);
        validate!(pager, render, SelectionMotion::East, 2);
        validate!(pager, render, SelectionMotion::East, 6);
        validate!(pager, render, SelectionMotion::East, 10);
        validate!(pager, render, SelectionMotion::East, 14);
        validate!(pager, render, SelectionMotion::East, 18);
        validate!(pager, render, SelectionMotion::West, 14);
        validate!(pager, render, SelectionMotion::East, 18);
        // Eastward motion wraps back to the upper left, westward goes to the prior column.
        validate!(pager, render, SelectionMotion::East, 3);
        validate!(pager, render, SelectionMotion::East, 7);
        validate!(pager, render, SelectionMotion::East, 11);
        validate!(pager, render, SelectionMotion::East, 15);
        // Pages.
        validate!(pager, render, SelectionMotion::PageNorth, 12);
        validate!(pager, render, SelectionMotion::PageSouth, 15);
        validate!(pager, render, SelectionMotion::PageNorth, 12);
        validate!(pager, render, SelectionMotion::East, 16);
        validate!(pager, render, SelectionMotion::PageSouth, 18);
        validate!(pager, render, SelectionMotion::East, 3);
        validate!(pager, render, SelectionMotion::North, 2);
        validate!(pager, render, SelectionMotion::PageNorth, 0);
        validate!(pager, render, SelectionMotion::PageSouth, 3);
    }

    #[test]
    #[serial]
    fn test_pager_layout() {
        let _cleanup = test_init();
        // These tests are woefully incomplete
        // They only test the truncation logic for a single completion

        let rendered_line = |pager: &mut Pager, width: u16| {
            pager.set_term_size(&Termsize::new(
                NonZeroU16::new(width).unwrap(),
                Termsize::DEFAULT_HEIGHT,
            ));
            let rendering = pager.render();
            let sd = &rendering.screen_data;
            assert_eq!(sd.line_count(), 1);
            let line = sd.line(0);
            WString::from(Vec::from_iter((0..line.len()).map(|i| line.char_at(i))))
        };
        let compute_expected = |expected: &wstr| {
            let ellipsis_char = get_ellipsis_char();
            if ellipsis_char != '\u{2026}' {
                // hack: handle the case where ellipsis is not L'\x2026'
                expected.replace(L!("\u{2026}"), wstr::from_char_slice(&[ellipsis_char]))
            } else {
                expected.to_owned()
            }
        };

        macro_rules! validate {
            ($pager:expr, $width:expr, $expected:expr) => {
                assert_eq!(
                    rendered_line($pager, $width),
                    compute_expected($expected),
                    "width {}",
                    $width
                );
            };
        }

        let mut pager = Pager::default();

        // These test cases have equal completions and descriptions
        let c1s = vec![Completion::new(
            L!("abcdefghij").to_owned(),
            L!("1234567890").to_owned(),
            StringFuzzyMatch::exact_match(),
            CompleteFlags::default(),
        )];
        pager.set_completions(&c1s, true);

        validate!(&mut pager, 26, L!("abcdefghij  (1234567890)"));
        validate!(&mut pager, 25, L!("abcdefghij  (1234567890)"));
        validate!(&mut pager, 24, L!("abcdefghij  (1234567890)"));
        validate!(&mut pager, 23, L!("abcdefghij  (12345678…)"));
        validate!(&mut pager, 22, L!("abcdefghij  (1234567…)"));
        validate!(&mut pager, 21, L!("abcdefghij  (123456…)"));
        validate!(&mut pager, 20, L!("abcdefghij  (12345…)"));
        validate!(&mut pager, 19, L!("abcdefghij  (1234…)"));
        validate!(&mut pager, 18, L!("abcdefgh…  (1234…)"));
        validate!(&mut pager, 17, L!("abcdefg…  (1234…)"));
        validate!(&mut pager, 16, L!("abcdefg…  (123…)"));

        // These test cases have heavyweight completions
        let c2s = vec![Completion::new(
            L!("abcdefghijklmnopqrs").to_owned(),
            L!("1").to_owned(),
            StringFuzzyMatch::exact_match(),
            CompleteFlags::default(),
        )];
        pager.set_completions(&c2s, true);
        validate!(&mut pager, 26, L!("abcdefghijklmnopqrs  (1)"));
        validate!(&mut pager, 25, L!("abcdefghijklmnopqrs  (1)"));
        validate!(&mut pager, 24, L!("abcdefghijklmnopqrs  (1)"));
        validate!(&mut pager, 23, L!("abcdefghijklmnopq…  (1)"));
        validate!(&mut pager, 22, L!("abcdefghijklmnop…  (1)"));
        validate!(&mut pager, 21, L!("abcdefghijklmno…  (1)"));
        validate!(&mut pager, 20, L!("abcdefghijklmn…  (1)"));
        validate!(&mut pager, 19, L!("abcdefghijklm…  (1)"));
        validate!(&mut pager, 18, L!("abcdefghijkl…  (1)"));
        validate!(&mut pager, 17, L!("abcdefghijk…  (1)"));
        validate!(&mut pager, 16, L!("abcdefghij…  (1)"));

        // These test cases have no descriptions
        let c3s = vec![Completion::new(
            L!("abcdefghijklmnopqrst").to_owned(),
            L!("").to_owned(),
            StringFuzzyMatch::exact_match(),
            CompleteFlags::default(),
        )];
        pager.set_completions(&c3s, true);
        validate!(&mut pager, 26, L!("abcdefghijklmnopqrst"));
        validate!(&mut pager, 25, L!("abcdefghijklmnopqrst"));
        validate!(&mut pager, 24, L!("abcdefghijklmnopqrst"));
        validate!(&mut pager, 23, L!("abcdefghijklmnopqrst"));
        validate!(&mut pager, 22, L!("abcdefghijklmnopqrst"));
        validate!(&mut pager, 21, L!("abcdefghijklmnopqrst"));
        validate!(&mut pager, 20, L!("abcdefghijklmnopqrst"));
        validate!(&mut pager, 19, L!("abcdefghijklmnopqr…"));
        validate!(&mut pager, 18, L!("abcdefghijklmnopq…"));
        validate!(&mut pager, 17, L!("abcdefghijklmnop…"));
        validate!(&mut pager, 16, L!("abcdefghijklmno…"));

        // Newlines in prefix
        let c4s = vec![Completion::new(
            L!("Hello").to_owned(),
            L!("").to_owned(),
            StringFuzzyMatch::exact_match(),
            CompleteFlags::default(),
        )];
        pager.set_prefix(Cow::Borrowed(L!("{\\\n")), false); // }
        pager.set_completions(&c4s, true);
        validate!(&mut pager, 30, L!("{\\␊Hello")); // }
    }
}
