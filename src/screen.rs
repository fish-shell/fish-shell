//! High level library for handling the terminal screen
//!
//! The screen library allows the interactive reader to write its output to screen efficiently by
//! keeping an internal representation of the current screen contents and trying to find a reasonably
//! efficient way for transforming that to the desired screen content.
//!
//! The current implementation is less smart than ncurses allows and can not for example move blocks
//! of text around to handle text insertion.

use crate::editable_line::line_at_cursor;
use crate::key::ViewportPosition;
use crate::pager::{PageRendering, Pager, PAGER_MIN_HEIGHT};
use crate::FLOG;
use std::cell::RefCell;
use std::collections::LinkedList;
use std::io::Write;
use std::ops::Range;
use std::sync::atomic::AtomicU32;
use std::sync::Mutex;
use std::time::SystemTime;

use libc::{ONLCR, STDERR_FILENO, STDOUT_FILENO};

use crate::common::{
    get_ellipsis_char, get_omitted_newline_str, get_omitted_newline_width,
    has_working_tty_timestamps, shell_modes, str2wcstring, wcs2string, write_loop,
};
use crate::env::Environment;
use crate::fallback::fish_wcwidth;
use crate::flog::FLOGF;
#[allow(unused_imports)]
use crate::future::IsSomeAnd;
use crate::global_safety::RelaxedAtomicBool;
use crate::highlight::{HighlightColorResolver, HighlightSpec};
use crate::terminal::TerminalCommand::{
    self, ClearToEndOfLine, ClearToEndOfScreen, CursorDown, CursorLeft, CursorMove, CursorRight,
    CursorUp, EnterDimMode, ExitAttributeMode, Osc133PromptStart, ScrollForward,
};
use crate::terminal::{use_terminfo, BufferedOutputter, CardinalDirection, Output, Outputter};
use crate::termsize::{termsize_last, Termsize};
use crate::wchar::prelude::*;
use crate::wcstringutil::{fish_wcwidth_visible, string_prefixes_string};
use crate::wutil::fstat;

#[derive(Copy, Clone, Default)]
pub enum CharOffset {
    #[default]
    None,
    Cmd(usize),
    Pointer(usize),
    Pager(usize),
}

#[derive(Clone, Default)]
pub struct HighlightedChar {
    highlight: HighlightSpec,
    character: char,
    // Logical offset within the command line.
    offset_in_cmdline: CharOffset,
}

/// A class representing a single line of a screen.
#[derive(Clone, Default)]
pub struct Line {
    /// A pair of a character, and the color with which to draw it.
    pub text: Vec<HighlightedChar>,
    pub is_soft_wrapped: bool,
    pub indentation: usize,
}

impl Line {
    pub fn new() -> Self {
        Default::default()
    }

    /// Clear the line's contents.
    fn clear(&mut self) {
        self.text.clear();
    }

    /// Append a single character `txt` to the line with color `c`.
    pub fn append(
        &mut self,
        character: char,
        highlight: HighlightSpec,
        offset_in_cmdline: CharOffset,
    ) {
        self.text.push(HighlightedChar {
            highlight,
            character: rendered_character(character),
            offset_in_cmdline,
        })
    }

    /// Append a nul-terminated string `txt` to the line, giving each character `color`.
    pub fn append_str(
        &mut self,
        txt: &wstr,
        highlight: HighlightSpec,
        offset_in_cmdline: CharOffset,
    ) {
        for c in txt.chars() {
            self.append(c, highlight, offset_in_cmdline);
        }
    }

    /// Return the number of characters.
    pub fn len(&self) -> usize {
        self.text.len()
    }

    /// Return the character at a char index.
    pub fn char_at(&self, idx: usize) -> char {
        self.text[idx].character
    }

    /// Return the color at a char index.
    pub fn color_at(&self, idx: usize) -> HighlightSpec {
        self.text[idx].highlight
    }

    /// Return the logical offset corresponding to this cell
    pub fn offset_in_cmdline_at(&self, idx: usize) -> CharOffset {
        self.text[idx].offset_in_cmdline
    }

    /// Append the contents of `line` to this line.
    pub fn append_line(&mut self, line: &Line) {
        self.text.extend_from_slice(&line.text);
    }

    /// Return the width of this line, counting up to no more than `max` characters.
    /// This follows fish_wcswidth() semantics, except that characters whose width would be -1 are
    /// treated as 0.
    pub fn wcswidth_min_0(&self, max: usize /* = usize::MAX */) -> usize {
        let mut result: usize = 0;
        for c in &self.text[..max.min(self.text.len())] {
            result += wcwidth_rendered_min_0(c.character);
        }
        result
    }
}

/// Where the cursor is in (x, y) coordinates.
#[derive(Clone, Copy, Default)]
pub struct Cursor {
    x: usize,
    y: usize,
}

/// A class representing screen contents.
#[derive(Clone, Default)]
pub struct ScreenData {
    line_datas: Vec<Line>,

    /// The width of the screen once we have rendered.
    screen_width: Option<usize>,

    cursor: Cursor,
}

impl ScreenData {
    pub fn add_line(&mut self) -> &mut Line {
        self.line_datas.push(Line::new());
        self.line_datas.last_mut().unwrap()
    }

    pub fn resize(&mut self, size: usize) {
        self.line_datas.resize(size, Default::default())
    }

    pub fn create_line(&mut self, idx: usize) -> &mut Line {
        if idx >= self.line_datas.len() {
            self.line_datas.resize(idx + 1, Default::default())
        }
        self.line_mut(idx)
    }

    pub fn insert_line_at_index(&mut self, idx: usize) -> &mut Line {
        assert!(idx <= self.line_datas.len());
        self.line_datas.insert(idx, Default::default());
        &mut self.line_datas[idx]
    }

    pub fn line(&self, idx: usize) -> &Line {
        &self.line_datas[idx]
    }

    pub fn line_mut(&mut self, idx: usize) -> &mut Line {
        &mut self.line_datas[idx]
    }

    pub fn line_count(&self) -> usize {
        self.line_datas.len()
    }

    pub fn append_lines(&mut self, d: &ScreenData) {
        self.line_datas.extend_from_slice(&d.line_datas);
    }

    pub fn is_empty(&self) -> bool {
        self.line_datas.is_empty()
    }
}

/// The class representing the current and desired screen contents.
pub struct Screen {
    /// Whether the last-drawn autosuggestion (if any) is truncated, or hidden entirely.
    pub autosuggestion_is_truncated: bool,
    /// True if the last rendering was so large we could only display part of the command line.
    pub scrolled: bool,

    /// Receiver for our output.
    outp: &'static RefCell<Outputter>,

    /// The internal representation of the desired screen contents.
    desired: ScreenData,
    /// The internal representation of the actual screen contents.
    actual: ScreenData,
    /// A string containing the prompt which was last printed to the screen.
    actual_left_prompt: Option<WString>,
    /// Last right prompt width.
    last_right_prompt_width: usize,
    /// If we support soft wrapping, we can output to this location without any cursor motion.
    soft_wrap_location: Option<Cursor>,
    /// This flag is set to true when there is reason to suspect that the parts of the screen lines
    /// where the actual content is not filled in may be non-empty. This means that a clr_eol
    /// command has to be sent to the terminal at the end of each line, including
    /// actual_lines_before_reset.
    need_clear_lines: bool,
    /// Whether there may be yet more content after the lines, and we issue a clr_eos if possible.
    need_clear_screen: bool,
    /// If we need to clear, this is how many lines the actual screen had, before we reset it. This
    /// is used when resizing the window larger: if the cursor jumps to the line above, we need to
    /// remember to clear the subsequent lines.
    actual_lines_before_reset: usize,
    /// Modification times to check if any output has occurred other than from fish's
    /// main loop, in which case we need to redraw.
    mtime_stdout_stderr: (Option<SystemTime>, Option<SystemTime>),
}

impl Screen {
    pub fn new() -> Self {
        Self {
            outp: Outputter::stdoutput(),
            autosuggestion_is_truncated: Default::default(),
            scrolled: Default::default(),
            desired: Default::default(),
            actual: Default::default(),
            actual_left_prompt: Default::default(),
            last_right_prompt_width: Default::default(),
            soft_wrap_location: Default::default(),
            need_clear_lines: Default::default(),
            need_clear_screen: Default::default(),
            actual_lines_before_reset: Default::default(),
            mtime_stdout_stderr: Default::default(),
        }
    }

    /// Return whether the last rendering was so large we could only display part of the command line.
    pub fn scrolled(&self) -> bool {
        self.scrolled
    }

    /// This is the main function for the screen output library. It is used to define the desired
    /// contents of the screen. The screen command will use its knowledge of the current contents of
    /// the screen in order to render the desired output using as few terminal commands as possible.
    ///
    /// \param left_prompt the prompt to prepend to the command line
    /// \param right_prompt the right prompt, or NULL if none
    /// \param commandline the command line
    /// \param explicit_len the number of characters of the "explicit" (non-autosuggestion) portion
    /// of the command line \param colors the colors to use for the commanad line \param indent the
    /// indent to use for the command line \param cursor_pos where the cursor is \param pager the
    /// pager to render below the command line \param page_rendering to cache the current pager view
    pub fn write(
        &mut self,
        left_prompt: &wstr,
        right_prompt: &wstr,
        commandline: &wstr,
        autosuggested_range: Range<usize>,
        mut colors: Vec<HighlightSpec>,
        mut indent: Vec<i32>,
        cursor_pos: usize,
        pager_search_field_position: Option<usize>,
        vars: &dyn Environment,
        pager: &mut Pager,
        page_rendering: &mut PageRendering,
        is_final_rendering: bool,
    ) {
        let curr_termsize = termsize_last();
        let screen_width = curr_termsize.width;
        let screen_height = curr_termsize.height;
        static REPAINTS: AtomicU32 = AtomicU32::new(0);
        FLOGF!(
            screen,
            "Repaint %u",
            1 + REPAINTS.fetch_add(1, std::sync::atomic::Ordering::Relaxed)
        );
        #[derive(Clone, Copy)]
        struct ScrolledCursor {
            cursor: Cursor,
            scroll_amount: usize,
        }
        let mut scrolled_cursor: Option<ScrolledCursor> = None;

        // Turn the command line into the explicit portion and the autosuggestion.
        let explicit_before_suggestion = &commandline[..autosuggested_range.start];
        let autosuggestion = &commandline[autosuggested_range.clone()];
        let explicit_after_suggestion = &commandline[autosuggested_range.end..];

        // If we are using a dumb terminal, don't try any fancy stuff, just print out the text.
        // right_prompt not supported.
        if is_dumb() {
            let prompt_narrow = wcs2string(left_prompt);

            let _ = write_loop(&STDOUT_FILENO, b"\r");
            let _ = write_loop(&STDOUT_FILENO, &prompt_narrow);
            let _ = write_loop(&STDOUT_FILENO, &wcs2string(explicit_before_suggestion));
            let _ = write_loop(&STDOUT_FILENO, &wcs2string(explicit_after_suggestion));

            return;
        }

        self.check_status();

        // Completely ignore impossibly small screens.
        if screen_width < 4 {
            return;
        }
        let screen_width = usize::try_from(screen_width).unwrap();
        if screen_height == 0 {
            return;
        }
        let screen_height = usize::try_from(curr_termsize.height).unwrap();

        // Compute a layout.
        let layout = compute_layout(
            get_ellipsis_char(),
            screen_width,
            left_prompt,
            right_prompt,
            explicit_before_suggestion,
            &mut colors,
            &mut indent,
            autosuggestion,
        );

        // Determine whether, if we have an autosuggestion, it was truncated.
        self.autosuggestion_is_truncated =
            !autosuggestion.is_empty() && autosuggestion != layout.autosuggestion;

        // Clear the desired screen and set its width.
        self.desired.screen_width = Some(screen_width);
        self.desired.resize(0);
        self.desired.cursor.x = 0;
        self.desired.cursor.y = 0;

        // Append spaces for the left prompt.
        let prompt_offset = if pager.search_field_shown {
            CharOffset::None
        } else {
            CharOffset::Pointer(0)
        };
        for _ in 0..layout.left_prompt_space {
            let _ = self.desired_append_char(
                prompt_offset,
                usize::MAX,
                ' ',
                HighlightSpec::new(),
                0,
                layout.left_prompt_space,
                1,
            );
        }

        // If the prompt doesn't occupy the full line, justify the command line to the end of the prompt.
        let first_line_prompt_space = if layout.left_prompt_space == screen_width {
            0
        } else {
            layout.left_prompt_space
        };

        // Reconstruct the command line.
        let effective_commandline = explicit_before_suggestion.to_owned()
            + &layout.autosuggestion[..]
            + explicit_after_suggestion;

        // Output the command line.
        let mut i = 0;
        assert!((0..=effective_commandline.len()).contains(&cursor_pos));
        let scrolled_cursor = loop {
            // Grab the current cursor's x,y position if this character matches the cursor's offset.
            if i == cursor_pos {
                scrolled_cursor = Some(ScrolledCursor {
                    cursor: self.desired.cursor,
                    scroll_amount: (self.desired.line_count()
                        + if self
                            .desired
                            .line_datas
                            .last()
                            .as_ref()
                            .map(|ld| ld.is_soft_wrapped)
                            .unwrap_or_default()
                        {
                            1
                        } else {
                            0
                        })
                    .saturating_sub(screen_height),
                });
            }
            if i == effective_commandline.len() {
                break scrolled_cursor.unwrap();
            }
            if !self.desired_append_char(
                if pager.search_field_shown {
                    CharOffset::None
                } else if i < explicit_before_suggestion.len() {
                    CharOffset::Cmd(i)
                } else if i < explicit_before_suggestion.len() + layout.autosuggestion.len() {
                    CharOffset::Pointer(explicit_before_suggestion.len())
                } else {
                    CharOffset::Cmd(i - layout.autosuggestion.len())
                },
                if is_final_rendering {
                    usize::MAX
                } else {
                    scrolled_cursor
                        .map(|sc| {
                            if sc.scroll_amount != 0 {
                                sc.cursor.y
                            } else {
                                screen_height - 1
                            }
                        })
                        .unwrap_or(usize::MAX)
                },
                effective_commandline.as_char_slice()[i],
                colors[i],
                usize::try_from(indent[i]).unwrap(),
                first_line_prompt_space,
                wcwidth_rendered_min_0(effective_commandline.as_char_slice()[i]),
            ) {
                break scrolled_cursor.unwrap();
            }
            i += 1;
        };

        // Add an empty line if there are no lines or if the last line was soft wrapped (but not by autosuggestion).
        if self.desired.line_datas.last().is_none_or(|line| {
            line.len() == screen_width
                && (commandline.is_empty()
                    || autosuggestion.is_empty()
                    || !explicit_after_suggestion.is_empty())
        }) {
            self.desired.add_line();
        }

        let full_line_count = self.desired.cursor.y
            - if self.desired.cursor.x == 0
                && self
                    .desired
                    .cursor
                    .y
                    .checked_sub(1)
                    .is_some_and(|y| self.desired.line_datas[y].is_soft_wrapped)
            {
                1
            } else {
                0
            }
            + calc_prompt_lines(&layout.left_prompt);
        let pager_available_height = std::cmp::max(
            1,
            curr_termsize
                .height
                .saturating_sub_unsigned(full_line_count),
        );

        // Now that we've output everything, set the cursor to the position that we saved in the loop
        // above.
        self.desired.cursor = match pager_search_field_position {
            Some(pager_cursor_pos)
                if pager_available_height >= isize::try_from(PAGER_MIN_HEIGHT).unwrap() =>
            {
                Cursor {
                    x: pager_cursor_pos,
                    y: self.desired.line_count(),
                }
            }
            _ => {
                let ScrolledCursor {
                    mut cursor,
                    scroll_amount,
                } = scrolled_cursor;
                if scroll_amount != 0 {
                    if !is_final_rendering {
                        self.desired.line_datas = self.desired.line_datas.split_off(scroll_amount);
                    }
                    cursor.y -= scroll_amount;
                }
                cursor
            }
        };

        // Re-render our completions page if necessary. Limit the term size of the pager to the true
        // term size, minus the number of lines consumed by our string.
        pager.set_term_size(&Termsize::new(
            std::cmp::max(1, curr_termsize.width),
            pager_available_height,
        ));

        pager.update_rendering(page_rendering);
        // Append pager_data (none if empty).
        self.desired.append_lines(&page_rendering.screen_data);

        self.scrolled = scrolled_cursor.scroll_amount != 0;

        self.with_buffered_output(|zelf| {
            zelf.update(
                vars,
                &layout.left_prompt,
                &layout.right_prompt,
                is_final_rendering,
            )
        });
        self.save_status();
    }

    pub fn multiline_prompt_hack(&mut self) {
        // If the prompt is multi-line, we need to move up to the prompt's initial line. We do this
        // by lying to ourselves and claiming that we're really below what we consider "line 0"
        // (which is the last line of the prompt). This will cause us to move up to try to get back
        // to line 0, but really we're getting back to the initial line of the prompt.
        let prompt_line_count = self
            .actual_left_prompt
            .as_ref()
            .map_or(1, |p| calc_prompt_lines(p));
        self.actual.cursor.y += prompt_line_count.checked_sub(1).unwrap();
    }

    /// Resets the screen buffer's internal knowledge about the contents of the screen,
    /// optionally repainting the prompt as well.
    /// This function assumes that the current line is still valid.
    pub fn reset_line(&mut self, repaint_prompt: bool /* = false */) {
        // Remember how many lines we had output to, so we can clear the remaining lines in the next
        // call to s_update. This prevents leaving junk underneath the cursor when resizing a window
        // wider such that it reduces our desired line count.
        self.actual_lines_before_reset =
            std::cmp::max(self.actual_lines_before_reset, self.actual.line_count());

        if repaint_prompt {
            self.multiline_prompt_hack();
            self.actual_left_prompt = None;
            self.need_clear_screen = true;
        }
        self.actual.resize(0);
        self.need_clear_lines = true;

        // This should prevent resetting the cursor position during the next repaint.
        let _ = write_loop(&STDOUT_FILENO, b"\r");
        self.actual.cursor.x = 0;

        self.save_status();
    }

    pub fn push_to_scrollback(&mut self, cursor_y: usize) {
        let prompt_y = self.command_line_y_given_cursor_y(cursor_y);
        let trailing_prompt_lines = self
            .actual_left_prompt
            .as_ref()
            .map_or(0, |p| calc_prompt_lines(p) - 1);
        let lines_to_scroll = prompt_y
            .checked_sub(trailing_prompt_lines)
            .unwrap_or_else(|| {
                FLOG!(
                    reader,
                    "Number of trailing prompt lines prompt lines",
                    trailing_prompt_lines,
                    "exceeds prompt's y",
                    prompt_y,
                    "inferred from reported cursor position",
                );
                0
            });
        if lines_to_scroll == 0 {
            return;
        }
        let mut out = BufferedOutputter::new(self.outp);
        // Scroll down.
        out.write_command(ScrollForward(lines_to_scroll));
        // Reposition cursor.
        out.write_command(CursorMove(CardinalDirection::Up, lines_to_scroll));
    }

    fn command_line_y_given_cursor_y(&mut self, viewport_cursor_y: usize) -> usize {
        let prompt_y = viewport_cursor_y.checked_sub(self.actual.cursor.y);
        prompt_y.unwrap_or_else(|| {
            FLOG!(
                reader,
                "Reported cursor line index",
                viewport_cursor_y,
                "is above fish's cursor",
                self.actual.cursor.y
            );
            0
        })
    }

    pub fn offset_in_cmdline_given_cursor(
        &mut self,
        viewport_position: ViewportPosition,
        viewport_cursor: ViewportPosition,
    ) -> CharOffset {
        let viewport_prompt_y = self.command_line_y_given_cursor_y(viewport_cursor.y);
        let y = viewport_position
            .y
            .checked_sub(viewport_prompt_y)
            .unwrap_or_else(|| {
                FLOG!(
                    reader,
                    "Given y",
                    viewport_position.y,
                    "exceeds the prompt's y",
                    viewport_prompt_y,
                    "inferred from reported cursor position",
                );
                0
            });
        let y = y.min(self.actual.line_count() - 1);
        let viewport_prompt_x = viewport_cursor.x - self.actual.cursor.x;
        let x = viewport_position.x - viewport_prompt_x;
        let line = self.actual.line(y);
        let x = x.max(line.indentation);
        let offset = line
            .text
            .get(x)
            .or(line.text.last())
            .or(if y > 0 {
                self.actual.line(y - 1).text.last()
            } else {
                None
            })
            .map(|char| char.offset_in_cmdline)
            .unwrap_or(CharOffset::Pointer(0));
        match offset {
            CharOffset::Cmd(value) if x >= line.len() => CharOffset::Cmd(value + 1),
            CharOffset::Pager(_) if x >= line.len() => CharOffset::None,
            offset => offset,
        }
    }

    /// Resets the screen buffer's internal knowledge about the contents of the screen,
    /// abandoning the current line and going to the next line.
    /// If clear_to_eos is set,
    /// The screen width must be provided for the PROMPT_SP hack.
    pub fn reset_abandoning_line(&mut self, screen_width: usize) {
        self.actual.cursor.y = 0;
        self.actual.resize(0);
        self.actual_left_prompt = None;
        self.need_clear_lines = true;

        // Do the PROMPT_SP hack.
        let mut abandon_line_string = WString::with_capacity(screen_width + 32);

        // Don't need to check for fish_wcwidth errors; this is done when setting up
        // omitted_newline_char in common.rs.
        let non_space_width = get_omitted_newline_width();
        // We do `>` rather than `>=` because the code below might require one extra space.
        if screen_width > non_space_width {
            if use_terminfo() {
                use crate::terminal::tparm1;
                use std::ffi::CString;
                let term = crate::terminal::term();
                let mut justgrey = true;
                let add = |abandon_line_string: &mut WString, s: Option<CString>| {
                    let Some(s) = s else {
                        return false;
                    };
                    abandon_line_string.push_utfstr(&str2wcstring(s.as_bytes()));
                    true
                };
                if let Some(enter_dim_mode) = term.enter_dim_mode.as_ref() {
                    if add(&mut abandon_line_string, Some(enter_dim_mode.clone())) {
                        // Use dim if they have it, so the color will be based on their actual normal
                        // color and the background of the terminal.
                        justgrey = false;
                    }
                }
                if let (true, Some(set_a_foreground)) = (justgrey, term.set_a_foreground.as_ref()) {
                    let max_colors = term.max_colors.unwrap_or_default();
                    if max_colors >= 238 {
                        // draw the string in a particular grey
                        add(&mut abandon_line_string, tparm1(set_a_foreground, 237));
                    } else if max_colors >= 9 {
                        // bright black (the ninth color, looks grey)
                        add(&mut abandon_line_string, tparm1(set_a_foreground, 8));
                    } else if max_colors >= 2 {
                        if let Some(enter_bold_mode) = term.enter_bold_mode.as_ref() {
                            // we might still get that color by setting black and going bold for bright
                            add(&mut abandon_line_string, Some(enter_bold_mode.clone()));
                            add(&mut abandon_line_string, tparm1(set_a_foreground, 0));
                        }
                    }
                }
            } else {
                let mut tmp = Vec::<u8>::new();
                tmp.write_command(EnterDimMode);
                abandon_line_string.push_utfstr(&str2wcstring(&tmp));
            }

            abandon_line_string.push_utfstr(&get_omitted_newline_str());

            // normal text ANSI escape sequence
            let mut tmp = Vec::<u8>::new();
            tmp.write_command(ExitAttributeMode);
            abandon_line_string.push_utfstr(&str2wcstring(&tmp));

            for _ in 0..screen_width - non_space_width {
                abandon_line_string.push(' ');
            }
        }

        abandon_line_string.push('\r');
        abandon_line_string.push_utfstr(get_omitted_newline_str());
        // Now we are certainly on a new line. But we may have dropped the omitted newline char on
        // it. So append enough spaces to overwrite the omitted newline char, and then clear all the
        // spaces from the new line.
        for _ in 0..non_space_width {
            abandon_line_string.push(' ');
        }
        abandon_line_string.push('\r');
        // Clear entire line. Zsh doesn't do this. Fish added this with commit 4417a6ee: If you have
        // a prompt preceded by a new line, you'll get a line full of spaces instead of an empty
        // line above your prompt. This doesn't make a difference in normal usage, but copying and
        // pasting your terminal log becomes a pain. This commit clears that line, making it an
        // actual empty line.
        let mut tmp = Vec::<u8>::new();
        tmp.write_command(ClearToEndOfLine);
        abandon_line_string.push_utfstr(&str2wcstring(&tmp));

        let narrow_abandon_line_string = wcs2string(&abandon_line_string);
        let _ = write_loop(&STDOUT_FILENO, &narrow_abandon_line_string);
        self.actual.cursor.x = 0;

        self.save_status();
    }

    /// Stat stdout and stderr and save result as the current timestamp.
    /// This is used to avoid reacting to changes that we ourselves made to the screen.
    pub fn save_status(&mut self) {
        self.mtime_stdout_stderr = mtime_stdout_stderr();
    }

    /// Return whether we believe the cursor is wrapped onto the last line, and that line is
    /// otherwise empty. This includes both soft and hard wrapping.
    pub fn cursor_is_wrapped_to_own_line(&self) -> bool {
        // Don't consider dumb terminals to have wrapping for the purposes of this function.
        self.actual.cursor.x == 0
            && self.actual.cursor.y != 0
            && self.actual.cursor.y + 1 == self.actual.line_count()
            && !is_dumb()
    }

    /// Appends a character to the end of the line that the output cursor is on. This function
    /// automatically handles linebreaks and lines longer than the screen width.
    fn desired_append_char(
        &mut self,
        offset_in_cmdline: CharOffset,
        max_y: usize,
        b: char,
        c: HighlightSpec,
        indent: usize,
        prompt_width: usize,
        bwidth: usize,
    ) -> bool {
        let mut line_no = self.desired.cursor.y;

        if b == '\n' {
            // Current line is definitely hard wrapped.
            // Create the next line.
            if self.desired.cursor.y + 1 > max_y {
                return false;
            }
            self.desired.create_line(self.desired.cursor.y + 1);
            self.desired.line_mut(self.desired.cursor.y).is_soft_wrapped = false;
            self.desired.cursor.y += 1;
            let line_no = self.desired.cursor.y;
            self.desired.cursor.x = 0;
            let indentation = prompt_width + indent * INDENT_STEP;
            let line = self.desired.line_mut(line_no);
            line.indentation = indentation;
            for _ in 0..indentation {
                if !self.desired_append_char(
                    offset_in_cmdline,
                    max_y,
                    ' ',
                    HighlightSpec::default(),
                    indent,
                    prompt_width,
                    1,
                ) {
                    return false;
                }
            }
        } else if b == '\r' {
            let current = self.desired.line_mut(line_no);
            current.clear();
            self.desired.cursor.x = 0;
        } else {
            let screen_width = self.desired.screen_width;
            let cw = bwidth;

            if line_no > max_y {
                return false;
            }
            self.desired.create_line(line_no);

            // Check if we are at the end of the line. If so, continue on the next line.
            if screen_width.is_none_or(|sw| (self.desired.cursor.x + cw) > sw) {
                if self.desired.cursor.y + 1 > max_y {
                    return false;
                }
                // Current line is soft wrapped (assuming we support it).
                self.desired.line_mut(self.desired.cursor.y).is_soft_wrapped = true;

                line_no = self.desired.line_count();
                self.desired.add_line();
                self.desired.cursor.y += 1;
                self.desired.cursor.x = 0;
            }

            self.desired
                .line_mut(line_no)
                .append(b, c, offset_in_cmdline);
            self.desired.cursor.x += cw;

            // Maybe wrap the cursor to the next line, even if the line itself did not wrap. This
            // avoids wonkiness in the last column.
            if screen_width.is_none_or(|sw| self.desired.cursor.x >= sw) {
                self.desired.line_mut(line_no).is_soft_wrapped = true;
                self.desired.cursor.x = 0;
                self.desired.cursor.y += 1;
            }
        }
        true
    }

    /// Stat stdout and stderr and compare result to previous result in reader_save_status. Repaint
    /// if modification time has changed.
    fn check_status(&mut self) {
        let _ = std::io::stdout().flush();
        let _ = std::io::stderr().flush();
        if !has_working_tty_timestamps() {
            // We can't reliably determine if the terminal has been written to behind our back so we
            // just assume that hasn't happened and hope for the best. This is important for multi-line
            // prompts to work correctly.
            return;
        }

        if self.mtime_stdout_stderr != mtime_stdout_stderr() {
            // Ok, someone has been messing with our screen. We will want to repaint. However, we do not
            // know where the cursor is. It is our best bet that we are still on the same line, so we
            // move to the beginning of the line, reset the modeled screen contents, and then set the
            // modeled cursor y-pos to its earlier value.
            let prev_line = self.actual.cursor.y;
            self.reset_line(true /* repaint prompt */);
            self.actual.cursor.y = prev_line;
        }
    }

    /// Write the bytes needed to move screen cursor to the specified position to the specified
    /// buffer. The actual_cursor field of the specified screen_t will be updated.
    ///
    /// \param new_x the new x position
    /// \param new_y the new y position
    fn r#move(&mut self, new_x: usize, new_y: usize) {
        if self.actual.cursor.x == new_x && self.actual.cursor.y == new_y {
            return;
        }
        self.with_buffered_output(|zelf| zelf.do_move(new_x, new_y));
    }

    fn do_move(&mut self, new_x: usize, new_y: usize) {
        // If we are at the end of our window, then either the cursor stuck to the edge or it didn't. We
        // don't know! We can fix it up though.
        if self
            .actual
            .screen_width
            .is_some_and(|sw| self.actual.cursor.x == sw)
        {
            // Either issue a cr to go back to the beginning of this line, or a nl to go to the
            // beginning of the next one, depending on what we think is more efficient.
            if new_y <= self.actual.cursor.y {
                self.outp.borrow_mut().push(b'\r');
            } else {
                self.outp.borrow_mut().push(b'\n');
                self.actual.cursor.y += 1;
            }
            // Either way we're not in the first column.
            self.actual.cursor.x = 0;
        }

        let y_steps =
            isize::try_from(new_y).unwrap() - isize::try_from(self.actual.cursor.y).unwrap();

        let s = if y_steps < 0 {
            Some(CursorUp)
        } else if y_steps > 0 {
            if (shell_modes().c_oflag & ONLCR) != 0
                && (!use_terminfo()
                    || crate::terminal::term()
                        .cursor_down
                        .as_ref()
                        .is_some_and(|cud| cud.as_bytes() == b"\n"))
            {
                // See GitHub issue #4505.
                // Most consoles use a simple newline as the cursor down escape.
                // If ONLCR is enabled (which it normally is) this will of course
                // also move the cursor to the beginning of the line.
                self.actual.cursor.x = 0;
            }
            Some(CursorDown)
        } else {
            None
        };

        if let Some(s) = s {
            for _ in 0..y_steps.abs_diff(0) {
                self.outp.borrow_mut().write_command(s.clone());
            }
        }

        let mut x_steps =
            isize::try_from(new_x).unwrap() - isize::try_from(self.actual.cursor.x).unwrap();
        if x_steps != 0 && new_x == 0 {
            self.outp.borrow_mut().push(b'\r');
            x_steps = 0;
        }

        // Note that bulk output is a way to avoid some visual glitches in iTerm (issue #1448).
        let mut outp = BufferedOutputter::new(self.outp);
        match x_steps {
            0 => (),
            -1 => {
                outp.write_command(CursorLeft);
            }
            1 => {
                outp.write_command(CursorRight);
            }
            _ => {
                outp.write_command(CursorMove(
                    if x_steps < 0 {
                        CardinalDirection::Left
                    } else {
                        CardinalDirection::Right
                    },
                    x_steps.abs_diff(0),
                ));
            }
        };

        self.actual.cursor.x = new_x;
        self.actual.cursor.y = new_y;
    }

    /// Convert a wide character to a multibyte string and append it to the buffer.
    fn write_char(&mut self, c: char, width: usize) {
        self.actual.cursor.x = self.actual.cursor.x.wrapping_add(width);
        self.outp.borrow_mut().writech(c);
        if Some(self.actual.cursor.x) == self.actual.screen_width {
            self.soft_wrap_location = Some(Cursor {
                x: 0,
                y: self.actual.cursor.y + 1,
            });

            // Note that our cursor position may be a lie: Apple Terminal makes the right cursor stick
            // to the margin, while Ubuntu makes it "go off the end" (but still doesn't wrap). We rely
            // on s_move to fix this up.
        } else {
            self.soft_wrap_location = None;
        }
    }

    pub(crate) fn write_command(&mut self, command: TerminalCommand) {
        self.outp.borrow_mut().write_command(command);
    }

    /// Convert a wide string to a multibyte string and append it to the buffer.
    fn write_str(&mut self, s: &wstr) {
        self.outp.borrow_mut().write_wstr(s);
    }

    /// Update the cursor as if soft wrapping had been performed.
    /// We are about to output one or more characters onto the screen at the given x, y. If we are at the
    /// end of previous line, and the previous line is marked as soft wrapping, then tweak the screen so
    /// we believe we are already in the target position. This lets the terminal take care of wrapping,
    /// which means that if you copy and paste the text, it won't have an embedded newline.
    fn handle_soft_wrap(&mut self, x: usize, y: usize) {
        if self
            .soft_wrap_location
            .as_ref()
            .is_some_and(|swl| (x, y) == (swl.x, swl.y))
        {
            // We can soft wrap; but do we want to?
            if self.desired.line(y - 1).is_soft_wrapped {
                // Yes. Just update the actual cursor; that will cause us to elide emitting the commands
                // to move here, so we will just output on "one big line" (which the terminal soft
                // wraps.
                self.actual.cursor = self.soft_wrap_location.unwrap();
            }
        }
    }

    fn should_wrap(&self, i: usize) -> bool {
        allow_soft_wrap()
            && self.desired.line(i).is_soft_wrapped
            && i + 1 < self.desired.line_count()
            && !(i + 1 < self.actual.line_count()
                && line_shared_prefix(self.actual.line(i + 1), self.desired.line(i + 1)) > 0)
    }

    fn with_buffered_output(&mut self, f: impl FnOnce(&mut Self)) {
        self.outp.borrow_mut().begin_buffering();
        (f)(self);
        self.outp.borrow_mut().end_buffering();
    }

    /// Update the screen to match the desired output.
    fn update(
        &mut self,
        vars: &dyn Environment,
        left_prompt: &wstr,
        right_prompt: &wstr,
        is_final_rendering: bool,
    ) {
        // Helper function to set a resolved color, using the caching resolver.
        let mut color_resolver = HighlightColorResolver::new();
        let mut set_color = |zelf: &mut Self, c| {
            zelf.outp
                .borrow_mut()
                .set_text_face(color_resolver.resolve_spec(&c, vars));
        };

        let mut cached_layouts = LAYOUT_CACHE_SHARED.lock().unwrap();

        // Determine size of left and right prompt. Note these have already been truncated.
        let left_prompt_layout = cached_layouts.calc_prompt_layout(left_prompt, None, usize::MAX);
        let left_prompt_width = left_prompt_layout.last_line_width;
        let right_prompt_width = cached_layouts
            .calc_prompt_layout(right_prompt, None, usize::MAX)
            .last_line_width;

        // Figure out how many following lines we need to clear (probably 0).
        let actual_lines_before_reset = self.actual_lines_before_reset;
        self.actual_lines_before_reset = 0;

        let mut need_clear_lines = self.need_clear_lines;
        let mut need_clear_screen = self.need_clear_screen;
        let mut has_cleared_screen = false;

        let screen_width = self.desired.screen_width;

        if self.actual.screen_width != screen_width {
            // Ensure we don't issue a clear screen for the very first output, to avoid issue #402.
            if self.actual.screen_width.is_some_and(|sw| sw > 0) {
                need_clear_screen = true;
                self.r#move(0, 0);
                self.reset_line(false);

                need_clear_lines |= self.need_clear_lines;
                need_clear_screen |= self.need_clear_screen;
            }
            self.actual.screen_width = screen_width;
        }

        self.need_clear_lines = false;
        self.need_clear_screen = false;

        // Determine how many lines have stuff on them; we need to clear lines with stuff that we don't
        // want.
        let lines_with_stuff = std::cmp::max(actual_lines_before_reset, self.actual.line_count());
        if self.desired.line_count() < lines_with_stuff {
            need_clear_screen = true;
        }

        // Output the left prompt if it has changed.
        if self.scrolled() && !is_final_rendering {
            self.r#move(0, 0);
            self.write_command(ClearToEndOfLine);
            self.actual_left_prompt = None;
            self.actual.cursor.x = 0;
        } else if self
            .actual_left_prompt
            .as_ref()
            .is_none_or(|p| p != left_prompt)
            || (self.scrolled() && is_final_rendering)
            || Some(left_prompt_width) == screen_width && self.should_wrap(0)
        {
            self.r#move(0, 0);
            let mut start = 0;
            let mark_prompt_start = |zelf: &mut Screen| zelf.write_command(Osc133PromptStart);
            if left_prompt_layout.line_breaks.is_empty() {
                mark_prompt_start(self);
            }
            if self
                .actual_left_prompt
                .as_ref()
                .is_none_or(|p| p != left_prompt)
                || (self.scrolled() && is_final_rendering)
            {
                for (i, &line_break) in left_prompt_layout.line_breaks.iter().enumerate() {
                    self.write_command(ClearToEndOfLine);
                    if i == 0 {
                        mark_prompt_start(self);
                    }
                    self.write_str(&left_prompt[start..=line_break]);
                    start = line_break + 1;
                }
            } else {
                start = left_prompt_layout.line_breaks.last().map_or(0, |lb| lb + 1);
            }
            self.write_str(&left_prompt[start..]);
            self.actual_left_prompt = Some(left_prompt.to_owned());
            self.actual.cursor.x = left_prompt_width;
            if Some(left_prompt_width) == screen_width && self.should_wrap(0) {
                self.soft_wrap_location = Some(Cursor { x: 0, y: 1 });
            }
        }

        fn o_line(zelf: &Screen, i: usize) -> &Line {
            zelf.desired.line(i)
        }
        fn s_line(zelf: &Screen, i: usize) -> &Line {
            zelf.actual.line(i)
        }

        // Output all lines.
        for i in 0..self.desired.line_count() {
            self.actual.create_line(i);
            let is_first_line = i == 0 && !self.scrolled();
            let start_pos = if is_first_line { left_prompt_width } else { 0 };
            let mut current_width = 0;
            let mut has_cleared_line = false;

            // If this is the last line, maybe we should clear the screen.
            // Don't issue clr_eos if we think the cursor will end up in the last column - see #6951.
            let should_clear_screen_this_line = need_clear_screen
                && i + 1 == self.desired.line_count()
                && !(self.desired.cursor.x == 0
                    && self.desired.cursor.y == self.desired.line_count());

            // skip_remaining is how many columns are unchanged on this line.
            // Note that skip_remaining is a width, not a character count.
            let mut skip_remaining = start_pos;

            let shared_prefix = if self.scrolled() {
                0
            } else {
                line_shared_prefix(o_line(self, i), s_line(self, i))
            };
            let mut skip_prefix = shared_prefix;
            if shared_prefix < o_line(self, i).indentation {
                if o_line(self, i).indentation > s_line(self, i).indentation && !has_cleared_screen
                {
                    set_color(self, HighlightSpec::new());
                    self.r#move(0, i);
                    self.write_command(if should_clear_screen_this_line {
                        ClearToEndOfScreen
                    } else {
                        ClearToEndOfLine
                    });
                    has_cleared_screen = should_clear_screen_this_line;
                    has_cleared_line = true;
                }
                skip_prefix = o_line(self, i).indentation;
            }

            // Compute how much we should skip. At a minimum we skip over the prompt. But also skip
            // over the shared prefix of what we want to output now, and what we output before, to
            // avoid repeatedly outputting it.
            if skip_prefix > 0 {
                let skip_width = if shared_prefix < skip_prefix {
                    skip_prefix
                } else {
                    o_line(self, i).wcswidth_min_0(shared_prefix)
                };
                if skip_width > skip_remaining {
                    skip_remaining = skip_width;
                }
            }

            if !should_clear_screen_this_line && self.should_wrap(i) {
                // If we're soft wrapped, and if we're going to change the first character of the next
                // line, don't skip over the last two characters so that we maintain soft-wrapping.
                skip_remaining = skip_remaining.min(screen_width.unwrap() - 2);
                if is_first_line {
                    skip_remaining = skip_remaining.max(left_prompt_width);
                }
            }

            // Skip over skip_remaining width worth of characters.
            let mut j = 0;
            while j < o_line(self, i).len() {
                let width = wcwidth_rendered_min_0(o_line(self, i).char_at(j));
                if current_width + width > skip_remaining {
                    break;
                }
                current_width += width;
                j += 1;
            }

            // Now actually output stuff.
            loop {
                let done = j >= o_line(self, i).len();
                // Clear the screen if we have not done so yet.
                // If we are about to output into the last column, clear the screen first. If we clear
                // the screen after we output into the last column, it can erase the last character due
                // to the sticky right cursor. If we clear the screen too early, we can defeat soft
                // wrapping.
                if should_clear_screen_this_line
                    && !has_cleared_screen
                    && (done || Some(j + 1) == screen_width)
                {
                    set_color(self, HighlightSpec::new());
                    self.r#move(current_width, i);
                    self.write_command(ClearToEndOfScreen);
                    has_cleared_screen = true;
                }
                if done {
                    break;
                }

                self.handle_soft_wrap(current_width, i);
                self.r#move(current_width, i);
                let color = o_line(self, i).color_at(j);
                set_color(self, color);
                let ch = o_line(self, i).char_at(j);
                let width = wcwidth_rendered_min_0(ch);
                self.with_buffered_output(|zelf| zelf.write_char(ch, width));
                current_width += width;
                j += 1;
            }

            let mut clear_remainder = false;
            // Clear the remainder of the line if we need to clear and if we didn't write to the end of
            // the line. If we did write to the end of the line, the "sticky right edge" (as part of
            // auto_right_margin) means that we'll be clearing the last character we wrote!
            if has_cleared_screen || has_cleared_line {
                // Already cleared everything.
                clear_remainder = false;
            } else if need_clear_lines && screen_width.is_some_and(|sw| current_width < sw) {
                clear_remainder = true;
            } else if right_prompt_width < self.last_right_prompt_width {
                clear_remainder = true;
            } else {
                // This wcswidth shows up strong in the profile.
                // Only do it if the previous line could conceivably be wider.
                // That means if it is a prefix of the current one we can skip it.
                if s_line(self, i).text.len() != shared_prefix {
                    let prev_width = s_line(self, i).wcswidth_min_0(usize::MAX);
                    clear_remainder = prev_width > current_width;
                }
            }

            // We unset the color even if we don't clear the line.
            // This means that we switch background correctly on the next,
            // including our weird implicit bolding.
            set_color(self, HighlightSpec::new());
            if clear_remainder {
                self.r#move(current_width, i);
                self.write_command(ClearToEndOfLine);
            }

            // Output any rprompt if this is the first line.
            if is_first_line && right_prompt_width > 0 {
                // Move the cursor to the beginning of the line first to be independent of the width.
                // This helps prevent staircase effects if fish and the terminal disagree.
                self.r#move(0, 0);
                self.r#move(screen_width.unwrap() - right_prompt_width, i);
                set_color(self, HighlightSpec::new());
                self.write_str(right_prompt);
                self.actual.cursor.x += right_prompt_width;

                // We output in the last column. Some terms (Linux) push the cursor further right, past
                // the window. Others make it "stick." Since we don't really know which is which, issue
                // a cr so it goes back to the left.
                //
                // However, if the user is resizing the window smaller, then it's possible the cursor
                // wrapped. If so, then a cr will go to the beginning of the following line! So instead
                // issue a bunch of "move left" commands to get back onto the line, and then jump to the
                // front of it.
                let Cursor { x, y } = self.actual.cursor;
                self.r#move(x - right_prompt_width, y);
                self.write_str(L!("\r"));
                self.actual.cursor.x = 0;
            }
        }

        // Also move the cursor to the beginning of the line here,
        // in case we're wrong about the width anywhere.
        // Don't do it when running in midnight_commander because of
        // https://midnight-commander.org/ticket/4258.
        if !MIDNIGHT_COMMANDER_HACK.load() {
            self.r#move(0, 0);
        }

        // Clear remaining lines (if any) if we haven't cleared the screen.
        if !has_cleared_screen && need_clear_screen {
            set_color(self, HighlightSpec::new());
            for i in self.desired.line_count()..lines_with_stuff {
                self.r#move(0, i);
                self.write_command(ClearToEndOfLine);
            }
        }

        let Cursor { x, y } = self.desired.cursor;
        self.r#move(x, y);
        set_color(self, HighlightSpec::new());

        // We have now synced our actual screen against our desired screen. Note that this is a big
        // assignment!
        self.actual = self.desired.clone();
        self.last_right_prompt_width = right_prompt_width;
    }
}

/// Helper to get the mtime of stdout and stderr.
pub fn mtime_stdout_stderr() -> (Option<SystemTime>, Option<SystemTime>) {
    let mtime_out = fstat(STDOUT_FILENO).and_then(|md| md.modified()).ok();
    let mtime_err = fstat(STDERR_FILENO).and_then(|md| md.modified()).ok();
    (mtime_out, mtime_err)
}

/// Issues an immediate clr_eos.
pub fn screen_force_clear_to_end() {
    Outputter::stdoutput()
        .borrow_mut()
        .write_command(ClearToEndOfScreen);
}

/// Information about the layout of a prompt.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct PromptLayout {
    /// line breaks when rendering the prompt
    pub line_breaks: Vec<usize>,
    /// width of the longest line
    pub max_line_width: usize,
    /// width of the last line
    pub last_line_width: usize,
}

// Fields exposed for testing.
pub struct PromptCacheEntry {
    /// Original prompt string.
    pub text: WString,
    /// Max line width when computing layout (for truncation).
    pub max_line_width: usize,
    /// Resulting truncated prompt string.
    pub trunc_text: WString,
    /// Resulting layout.
    pub layout: PromptLayout,
}

// Maintain a mapping of escape sequences to their widths for fast lookup.
#[derive(Default)]
pub struct LayoutCache {
    // Cached escape sequences we've already detected in the prompt and similar strings, ordered
    // lexicographically.
    esc_cache: Vec<WString>,
    // LRU-list of prompts and their layouts.
    // Use a list so we can promote to the front on a cache hit.
    // Exposed for testing.
    pub prompt_cache: LinkedList<PromptCacheEntry>,
}

// Singleton of the cached escape sequences seen in prompts and similar strings.
// Note this is deliberately exported so that init_terminal can clear it.
// TODO un-export this once we remove the terminfo option.
pub static LAYOUT_CACHE_SHARED: Mutex<LayoutCache> = Mutex::new(LayoutCache::new());

impl LayoutCache {
    pub const fn new() -> Self {
        Self {
            esc_cache: vec![],
            prompt_cache: LinkedList::new(),
        }
    }

    pub const PROMPT_CACHE_MAX_SIZE: usize = 12;

    /// Return the size of the escape code cache.
    #[cfg(test)]
    pub fn esc_cache_size(&self) -> usize {
        self.esc_cache.len()
    }

    /// Insert the entry `str` in its sorted position, if it is not already present in the cache.
    pub fn add_escape_code(&mut self, s: WString) {
        if let Err(pos) = self.esc_cache.binary_search(&s) {
            self.esc_cache.insert(pos, s);
        }
    }

    /// Return the length of an escape code, accessing and perhaps populating the cache.
    pub fn escape_code_length(&mut self, code: &wstr) -> usize {
        if code.char_at(0) != '\x1B' {
            return 0;
        }

        let mut esc_seq_len = self.find_escape_code(code);
        if esc_seq_len != 0 {
            return esc_seq_len;
        }

        if let Some(len) = escape_code_length(code) {
            self.add_escape_code(code[..len].to_owned());
            esc_seq_len = len;
        }
        esc_seq_len
    }

    /// Return the length of a string that matches a prefix of `entry`.
    pub fn find_escape_code(&self, entry: &wstr) -> usize {
        // Do a binary search and see if the escape code right before our entry is a prefix of our
        // entry. Note this assumes that escape codes are prefix-free: no escape code is a prefix of
        // another one. This seems like a safe assumption.
        match self.esc_cache.binary_search_by(|e| e[..].cmp(entry)) {
            Ok(_) => return entry.len(),
            Err(pos) => {
                if pos != 0 {
                    let candidate = &self.esc_cache[pos - 1];
                    if string_prefixes_string(candidate, entry) {
                        return candidate.len();
                    }
                }
            }
        }
        0
    }

    /// Computes a prompt layout for `prompt_str`, perhaps truncating it to `max_line_width`.
    /// Return the layout, and optionally the truncated prompt itself, by reference.
    pub fn calc_prompt_layout(
        &mut self,
        prompt_str: &wstr,
        out_trunc_prompt: Option<&mut WString>,
        max_line_width: usize, /* = usize::MAX */
    ) -> PromptLayout {
        // FIXME: we could avoid allocating trunc_prompt if max_line_width is SIZE_T_MAX.
        if self.find_prompt_layout(prompt_str, max_line_width) {
            let entry = self.prompt_cache.front().unwrap();
            out_trunc_prompt.map(|prompt| *prompt = entry.trunc_text.clone());
            return entry.layout.clone();
        }

        let mut layout = PromptLayout::default();
        let mut trunc_prompt = WString::new();

        let mut run_start = 0;
        while run_start < prompt_str.len() {
            let mut run_end = 0;
            let mut line_width = measure_run_from(prompt_str, run_start, Some(&mut run_end), self);
            if line_width <= max_line_width {
                // No truncation needed on this line.
                trunc_prompt.extend(prompt_str[run_start..run_end].chars());
            } else {
                // Truncation needed on this line.
                let mut run_storage = prompt_str[run_start..run_end].to_owned();
                truncate_run(&mut run_storage, max_line_width, &mut line_width, self);
                trunc_prompt.extend(run_storage.chars());
            }
            layout.max_line_width = std::cmp::max(layout.max_line_width, line_width);
            layout.last_line_width = line_width;

            let endc = prompt_str.char_at(run_end);
            if endc != '\0' {
                if endc == '\n' || endc == '\x0C' {
                    layout.line_breaks.push(trunc_prompt.len());
                    // If the prompt ends in a new line, that's one empty last line.
                    if run_end == prompt_str.len() - 1 {
                        layout.last_line_width = 0;
                    }
                }
                trunc_prompt.push(endc);
                run_start = run_end + 1;
            } else {
                break;
            }
        }
        out_trunc_prompt.map(|prompt| *prompt = trunc_prompt.clone());
        self.add_prompt_layout(PromptCacheEntry {
            text: prompt_str.to_owned(),
            max_line_width,
            trunc_text: trunc_prompt,
            layout: layout.clone(),
        });
        layout
    }

    pub fn clear(&mut self) {
        self.esc_cache.clear();
        self.prompt_cache.clear();
    }

    /// Add a cache entry.
    /// Exposed for testing.
    pub fn add_prompt_layout(&mut self, entry: PromptCacheEntry) {
        self.prompt_cache.push_front(entry);
        if self.prompt_cache.len() > Self::PROMPT_CACHE_MAX_SIZE {
            self.prompt_cache.pop_back();
        }
    }

    /// Finds the layout for a prompt, promoting it to the front. Returns whether this was found.
    /// Exposed for testing.
    pub fn find_prompt_layout(
        &mut self,
        input: &wstr,
        max_line_width: usize, /* = usize::MAX */
    ) -> bool {
        let mut i = 0;
        for entry in &self.prompt_cache {
            if entry.text == input && entry.max_line_width == max_line_width {
                break;
            }
            i += 1;
        }
        if i < self.prompt_cache.len() {
            // Found it. Move it to the front if not already there.
            if i > 0 {
                let mut tail = self.prompt_cache.split_off(i);
                let extracted = tail.pop_front().unwrap();
                self.prompt_cache.append(&mut tail);
                self.prompt_cache.push_front(extracted);
            }
            return true;
        }
        false
    }
}

/// Returns the number of characters in the escape code starting at 'code'. We only handle sequences
/// that begin with \x1B. If it doesn't we return zero. We also return zero if we don't recognize
/// the escape sequence.
pub fn escape_code_length(code: &wstr) -> Option<usize> {
    if code.char_at(0) != '\x1B' {
        return None;
    }

    is_terminfo_escape_seq(code)
        .or_else(|| is_screen_name_escape_seq(code))
        .or_else(|| is_osc_escape_seq(code))
        .or_else(|| is_three_byte_escape_seq(code))
        .or_else(|| is_csi_style_escape_seq(code))
        .or_else(|| is_two_byte_escape_seq(code))
}

static MIDNIGHT_COMMANDER_HACK: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

/// Midnight Commander tries to extract the last line of the prompt, and does so in a way that is
/// broken if you do '\r' after it like we normally do.
/// See <https://midnight-commander.org/ticket/4258>.
pub fn screen_set_midnight_commander_hack() {
    MIDNIGHT_COMMANDER_HACK.store(true)
}

/// The number of characters to indent new blocks.
const INDENT_STEP: usize = 4;

/// Tests if the specified narrow character sequence is present at the specified position of the
/// specified wide character string. All of \c seq must match, but str may be longer than seq.
fn try_sequence(seq: &[u8], s: &wstr) -> usize {
    let mut i = 0;
    loop {
        if i == seq.len() {
            return i;
        }
        if char::from(seq[i]) != s.char_at(i) {
            return 0;
        }
        i += 1;
    }
}

/// Returns the number of columns left until the next tab stop, given the current cursor position.
fn next_tab_stop(current_line_width: usize) -> usize {
    // Assume tab stops every 8 characters.
    let tab_width = if use_terminfo() {
        crate::terminal::term().init_tabs.unwrap_or(8)
    } else {
        8
    };
    ((current_line_width / tab_width) + 1) * tab_width
}

/// Whether we permit soft wrapping. If so, in some cases we don't explicitly move to the second
/// physical line on a wrapped logical line; instead we just output it.
fn allow_soft_wrap() -> bool {
    !use_terminfo() || crate::terminal::term().auto_right_margin
}

/// Does this look like the escape sequence for setting a screen name?
fn is_screen_name_escape_seq(code: &wstr) -> Option<usize> {
    // Tmux escapes start with `\ePtmux;` and end also in `\e\\`,
    // so we can just handle them here.
    let tmux_seq = L!("Ptmux;");
    let mut is_tmux = false;
    if code.char_at(1) != 'k' {
        if code.starts_with(tmux_seq) {
            is_tmux = true;
        } else {
            return None;
        }
    }
    let screen_name_end_sentinel = L!("\x1B\\");
    let mut offset = 2;
    let escape_sequence_end;
    loop {
        let Some(pos) = code[offset..].find(screen_name_end_sentinel) else {
            // Consider just <esc>k to be the code.
            // (note: for the tmux sequence this is broken, but since we have no idea...)
            escape_sequence_end = 2;
            break;
        };
        let screen_name_end = offset + pos;
        // The tmux sequence requires that all escapes in the payload sequence
        // be doubled. So if we have \e\e\\ that's still not the end.
        if is_tmux {
            let mut esc_count = 0;
            let mut i = screen_name_end;
            while i > 0 && code.as_char_slice()[i - 1] == '\x1B' {
                i -= 1;
                if i > 0 {
                    esc_count += 1;
                }
            }
            if esc_count % 2 == 1 {
                offset = screen_name_end + 1;
                continue;
            }
        }
        escape_sequence_end = screen_name_end + screen_name_end_sentinel.len();
        break;
    }
    Some(escape_sequence_end)
}

/// Operating System Command (OSC) escape codes, used by iTerm2 and others:
/// ESC followed by ], terminated by either BEL or escape + backslash.
/// See <https://invisible-island.net/xterm/ctlseqs/ctlseqs.html>
/// and <https://iterm2.com/documentation-escape-codes.html>.
fn is_osc_escape_seq(code: &wstr) -> Option<usize> {
    if code.char_at(1) == ']' {
        // Start at 2 to skip over <esc>].
        let mut cursor = 2;
        while cursor < code.len() {
            let code = code.as_char_slice();
            // Consume a sequence of characters up to <esc>\ or <bel>.
            if code[cursor] == '\x07' || (code[cursor] == '\\' && code[cursor - 1] == '\x1B') {
                return Some(cursor + 1);
            }
            cursor += 1;
        }
    }
    None
}

/// Generic VT100 three byte sequence: `CSI` followed by something in the range @ through _.
fn is_three_byte_escape_seq(code: &wstr) -> Option<usize> {
    if code.char_at(1) == '[' && (code.char_at(2) >= '@' && code.char_at(2) <= '_') {
        return Some(3);
    }
    None
}

/// Generic VT100 two byte sequence: `<esc>` followed by something in the range @ through _.
fn is_two_byte_escape_seq(code: &wstr) -> Option<usize> {
    if code.char_at(1) >= '@' && code.char_at(1) <= '_' {
        return Some(2);
    }
    None
}

/// Generic VT100 CSI-style sequence. `<esc>`, followed by zero or more ASCII characters NOT in
/// the range `[@,_],` followed by one character in that range.
/// This will also catch color sequences.
fn is_csi_style_escape_seq(code: &wstr) -> Option<usize> {
    if code.char_at(1) != '[' {
        return None;
    }

    // Start at 2 to skip over <esc>[
    let mut cursor = 2;
    while cursor < code.len() {
        // Consume a sequence of ASCII characters not in the range [@, ~].
        let widechar = code.as_char_slice()[cursor];

        // If we're not in ASCII, just stop.
        if !widechar.is_ascii() {
            break;
        }

        // If we're the end character, then consume it and then stop.
        if ('@'..'~').contains(&widechar) {
            cursor += 1;
            break;
        }
        cursor += 1;
    }
    // cursor now indexes just beyond the end of the sequence (or at the terminating zero).
    Some(cursor)
}

/// Detect whether the escape sequence sets one of the terminal attributes that affects how text is
/// displayed other than the color.
fn is_terminfo_escape_seq(code: &wstr) -> Option<usize> {
    if !use_terminfo() {
        return None;
    }
    let term = crate::terminal::term();
    let esc2 = [
        &term.enter_bold_mode,
        &term.exit_attribute_mode,
        &term.enter_underline_mode,
        &term.exit_underline_mode,
        &term.enter_standout_mode,
        &term.enter_italics_mode,
        &term.exit_italics_mode,
        &term.enter_reverse_mode,
        &term.enter_dim_mode,
    ];

    for p in &esc2 {
        let Some(p) = p else { continue };
        // Test both padded and unpadded version, just to be safe. Most versions of fish_tparm don't
        // actually seem to do anything these days.
        let esc_seq_len = try_sequence(p.as_bytes(), code);
        if esc_seq_len != 0 {
            return Some(esc_seq_len);
        }
    }

    None
}

/// Return whether `c` ends a measuring run.
fn is_run_terminator(c: char) -> bool {
    matches!(c, '\0' | '\n' | '\r' | '\x0C')
}

/// Measure a run of characters in `input` starting at `start`.
/// Stop when we reach a run terminator, and return its index in `out_end` (if not null).
/// Note \0 is a run terminator so there will always be one.
/// We permit escape sequences to have run terminators other than \0. That is, escape sequences may
/// have embedded newlines, etc.; it's unclear if this is possible but we allow it.
fn measure_run_from(
    input: &wstr,
    start: usize,
    out_end: Option<&mut usize>,
    cache: &mut LayoutCache,
) -> usize {
    let mut width = 0;
    let mut idx = start;
    while !is_run_terminator(input.char_at(idx)) {
        if input.char_at(idx) == '\x1B' {
            // This is the start of an escape code; we assume it has width 0.
            // -1 because we are going to increment in the loop.
            let len = cache.escape_code_length(&input[idx..]);
            if len > 0 {
                idx += len - 1;
            }
        } else if input.char_at(idx) == '\t' {
            width = next_tab_stop(width);
        } else {
            // Ordinary char. Add its width with care to ignore control chars which have width -1.
            if let Ok(ww) = usize::try_from(fish_wcwidth_visible(input.char_at(idx))) {
                width += ww;
            } else {
                width = width.saturating_sub(1);
            }
        }
        idx += 1;
    }
    out_end.map(|end| *end = idx);
    width
}

/// Attempt to truncate the prompt run `run`, which has width `width`, to `no` more than
/// desired_width. Return the resulting width and run by reference.
fn truncate_run(
    run: &mut WString,
    desired_width: usize,
    width: &mut usize,
    cache: &mut LayoutCache,
) {
    let mut curr_width = *width;
    if curr_width < desired_width {
        return;
    }

    // Bravely prepend ellipsis char and skip it.
    // Ellipsis is always width 1.
    let ellipsis = get_ellipsis_char();
    run.insert(0, ellipsis);
    curr_width += 1;

    // Start removing characters after ellipsis.
    // Note we modify 'run' inside this loop.
    let mut idx = 1;
    while curr_width > desired_width && idx < run.len() {
        let c = run.as_char_slice()[idx];
        assert!(
            !is_run_terminator(c),
            "Should not have run terminator inside run"
        );
        if c == '\x1B' {
            let len = cache.escape_code_length(&run[idx..]);
            idx += std::cmp::max(len, 1);
        } else if c == '\t' {
            // Tabs would seem to be quite annoying to measure while truncating.
            // We simply remove these and start over.
            run.remove(idx);
            curr_width = measure_run_from(run, 0, None, cache);
            idx = 0;
        } else {
            // FIXME: In case of backspace, this would remove the last width.
            let char_width = usize::try_from(fish_wcwidth_visible(c)).unwrap_or(0);
            curr_width -= std::cmp::min(curr_width, char_width);
            run.remove(idx);
        }
    }
    *width = curr_width;
}

fn calc_prompt_lines(prompt: &wstr) -> usize {
    // Hack for the common case where there's no newline at all. I don't know if a newline can
    // appear in an escape sequence, so if we detect a newline we have to defer to
    // calc_prompt_width_and_lines.
    let mut result = 1;
    if prompt.chars().any(|c| matches!(c, '\n' | '\x0C')) {
        result = LAYOUT_CACHE_SHARED
            .lock()
            .unwrap()
            .calc_prompt_layout(prompt, None, usize::MAX)
            .line_breaks
            .len()
            + 1;
    }
    result
}

/// Returns the length of the "shared prefix" of the two lines, which is the run of matching text
/// and colors. If the prefix ends on a combining character, do not include the previous character
/// in the prefix.
fn line_shared_prefix(a: &Line, b: &Line) -> usize {
    let mut idx = 0;
    let max = std::cmp::min(a.len(), b.len());
    while idx < max {
        let ac = a.char_at(idx);
        let bc = b.char_at(idx);

        // We're done if the text or colors are different.
        if ac != bc || a.color_at(idx) != b.color_at(idx) {
            if idx > 0 {
                let mut c = None;
                // Possible combining mark, go back until we hit _two_ printable characters or idx
                // of 0.
                if fish_wcwidth(a.char_at(idx)) < 1 {
                    c = Some(&a);
                } else if fish_wcwidth(b.char_at(idx)) < 1 {
                    c = Some(&b);
                }

                if let Some(c) = c {
                    while idx > 1
                        && (fish_wcwidth(c.char_at(idx - 1)) < 1
                            || fish_wcwidth(c.char_at(idx)) < 1)
                    {
                        idx -= 1;
                    }
                    if idx == 1 && fish_wcwidth(c.char_at(idx)) < 1 {
                        idx = 0;
                    }
                }
            }
            break;
        }
        idx += 1;
    }
    idx
}

pub(crate) static IS_DUMB: RelaxedAtomicBool = RelaxedAtomicBool::new(false);
pub(crate) static ONLY_GRAYSCALE: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

/// Returns true if we are using a dumb terminal.
pub(crate) fn is_dumb() -> bool {
    if use_terminfo() {
        let term = crate::terminal::term();
        return term.cursor_up.is_none()
            || term.cursor_down.is_none()
            || term.cursor_left.is_none()
            || term.cursor_right.is_none();
    }
    IS_DUMB.load()
}

pub(crate) fn only_grayscale() -> bool {
    ONLY_GRAYSCALE.load()
}

// Exposed for testing.
#[derive(Debug, Default, Eq, PartialEq)]
pub(crate) struct ScreenLayout {
    // The left prompt that we're going to use.
    pub(crate) left_prompt: WString,
    // How much space to leave for it.
    pub(crate) left_prompt_space: usize,
    // The right prompt.
    pub(crate) right_prompt: WString,
    // The autosuggestion.
    pub(crate) autosuggestion: WString,
}

// Given a vector whose indexes are offsets and whose values are the widths of the string if
// truncated at that offset, return the offset that fits in the given width. Returns
// width_by_offset.size() - 1 if they all fit. The first value in width_by_offset is assumed to be
// 0.
fn truncation_offset_for_width(str: &wstr, max_width: usize) -> usize {
    let mut i = 0;
    let mut width = 0;
    while i < str.len() && width <= max_width {
        width += wcwidth_rendered_min_0(str.char_at(i));
        i += 1;
    }
    // i is the first index that did not fit; i - 1 is therefore the last that did.
    i - 1
}

// Exposed for testing.
pub(crate) fn compute_layout(
    ellipsis_char: char,
    screen_width: usize,
    left_untrunc_prompt: &wstr,
    right_untrunc_prompt: &wstr,
    commandline_before_suggestion: &wstr,
    colors: &mut Vec<HighlightSpec>,
    indent: &mut Vec<i32>,
    autosuggestion_str: &wstr,
) -> ScreenLayout {
    // Truncate both prompts to screen width (#904).
    let mut left_prompt = WString::new();
    let left_prompt_layout = LAYOUT_CACHE_SHARED.lock().unwrap().calc_prompt_layout(
        left_untrunc_prompt,
        Some(&mut left_prompt),
        screen_width,
    );

    let mut right_prompt = WString::new();
    let right_prompt_layout = LAYOUT_CACHE_SHARED.lock().unwrap().calc_prompt_layout(
        right_untrunc_prompt,
        Some(&mut right_prompt),
        screen_width,
    );

    let left_prompt_width = left_prompt_layout.last_line_width;
    let mut right_prompt_width = right_prompt_layout.last_line_width;

    // Get the width of the first line, and if there is more than one line.
    let first_command_line_width: usize = line_at_cursor(commandline_before_suggestion, 0)
        .chars()
        .map(wcwidth_rendered_min_0)
        .sum();
    let autosuggestion_line_explicit_width: usize = line_at_cursor(
        commandline_before_suggestion,
        commandline_before_suggestion.len(),
    )
    .chars()
    .map(wcwidth_rendered_min_0)
    .sum();
    let autosuggest_total_width = autosuggestion_str.chars().map(wcwidth_rendered_min_0).sum();

    // Here are the layouts we try:
    // 1. Right prompt visible.
    // 2. Right prompt hidden.
    // 3. Newline separator (right prompt hidden).
    //
    // Left prompt and command line are always visible.
    // Autosuggestion is truncated to fit on the line (possibly to ellipsis_char or not at all).
    //
    // A remark about layout #3: if we've pushed the command line to a new line, why can't we draw
    // the right prompt? The issue is resizing: if you resize the window smaller, then the right
    // prompt will wrap to the next line. This means that we can't go back to the line that we were
    // on, and things turn to chaos very quickly.

    let mut result = ScreenLayout::default();

    // Always visible.
    result.left_prompt = left_prompt;
    result.left_prompt_space = left_prompt_width;

    // Hide the right prompt if it doesn't fit on the first line.
    if left_prompt_width + first_command_line_width + right_prompt_width < screen_width {
        result.right_prompt = right_prompt;
    } else {
        right_prompt_width = 0;
    }

    // Now we should definitely fit.
    assert!(left_prompt_width + right_prompt_width <= screen_width);

    // Calculate space available for autosuggestion.
    let width = (left_prompt_width
        + autosuggestion_line_explicit_width
        + commandline_before_suggestion
            .chars()
            .rposition(|c| c == '\n')
            .map_or(right_prompt_width, |pos| {
                usize::try_from(indent[pos]).unwrap() * INDENT_STEP
            }))
        % screen_width;
    let available_autosuggest_space = screen_width - width;

    // Truncate the autosuggestion to fit on the line.
    let mut autosuggestion = WString::new();
    if available_autosuggest_space >= autosuggest_total_width {
        autosuggestion = autosuggestion_str.to_owned();
    } else if autosuggest_total_width > 0 {
        let truncation_offset =
            truncation_offset_for_width(autosuggestion_str, available_autosuggest_space - 1);
        autosuggestion = autosuggestion_str[..truncation_offset].to_owned();
        autosuggestion.push(ellipsis_char);
    }

    let suggestion_start = commandline_before_suggestion.len();
    let truncation_range =
        suggestion_start + autosuggestion.len()..suggestion_start + autosuggestion_str.len();
    colors.drain(truncation_range.clone());
    indent.drain(truncation_range);
    result.autosuggestion = autosuggestion;

    result
}

// Display non-printable control characters as a graphic symbol.
// This is to prevent control characters like \t and \v from moving the
// cursor in a way we don't handle.  The ones we do handle are \r and
// \n.
// See https://unicode-table.com/en/blocks/control-pictures/
fn rendered_character(c: char) -> char {
    if c <= '\x1F' {
        char::from_u32(u32::from(c) + 0x2400).unwrap()
    } else {
        c
    }
}

fn wcwidth_rendered_min_0(c: char) -> usize {
    usize::try_from(wcwidth_rendered(c)).unwrap_or_default()
}
pub fn wcwidth_rendered(c: char) -> isize {
    fish_wcwidth(rendered_character(c))
}
pub fn wcswidth_rendered(s: &wstr) -> isize {
    s.chars().map(wcwidth_rendered).sum()
}
