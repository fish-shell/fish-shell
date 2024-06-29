//! High level library for handling the terminal screen
//!
//! The screen library allows the interactive reader to write its output to screen efficiently by
//! keeping an internal representation of the current screen contents and trying to find a reasonably
//! efficient way for transforming that to the desired screen content.
//!
//! The current implementation is less smart than ncurses allows and can not for example move blocks
//! of text around to handle text insertion.

use crate::pager::{PageRendering, Pager};
use std::cell::RefCell;
use std::collections::LinkedList;
use std::ffi::{CStr, CString};
use std::io::Write;
use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::Mutex;

use libc::{ONLCR, STDERR_FILENO, STDOUT_FILENO};

use crate::common::{
    fish_reserved_codepoint, get_ellipsis_char, get_omitted_newline_str, get_omitted_newline_width,
    has_working_tty_timestamps, shell_modes, str2wcstring, wcs2string, write_loop, ScopeGuard,
    ScopeGuarding,
};
use crate::curses::{term, tparm0, tparm1};
use crate::env::{Environment, TERM_HAS_XN};
use crate::fallback::fish_wcwidth;
use crate::flog::FLOGF;
use crate::future::IsSomeAnd;
use crate::global_safety::RelaxedAtomicBool;
use crate::highlight::HighlightColorResolver;
use crate::highlight::HighlightSpec;
use crate::output::Outputter;
use crate::termsize::{termsize_last, Termsize};
use crate::wchar::prelude::*;
use crate::wcstringutil::string_prefixes_string;

#[derive(Clone, Default)]
pub struct HighlightedChar {
    highlight: HighlightSpec,
    character: char,
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
    pub fn append(&mut self, character: char, highlight: HighlightSpec) {
        self.text.push(HighlightedChar {
            highlight,
            character: rendered_character(character),
        })
    }

    /// Append a nul-terminated string `txt` to the line, giving each character `color`.
    pub fn append_str(&mut self, txt: &wstr, highlight: HighlightSpec) {
        for c in txt.chars() {
            self.append(c, highlight);
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
            result += wcwidth_rendered(c.character);
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

    /// The width of the screen in this rendering.
    /// -1 if not set, i.e. we have not rendered before.
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

    /// Receiver for our output.
    outp: &'static RefCell<Outputter>,

    /// The internal representation of the desired screen contents.
    desired: ScreenData,
    /// The internal representation of the actual screen contents.
    actual: ScreenData,
    /// A string containing the prompt which was last printed to the screen.
    actual_left_prompt: WString,
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
    /// These status buffers are used to check if any output has occurred other than from fish's
    /// main loop, in which case we need to redraw.
    prev_buff_1: libc::stat,
    prev_buff_2: libc::stat,
}

impl Screen {
    pub fn new() -> Self {
        Self {
            outp: Outputter::stdoutput(),
            autosuggestion_is_truncated: Default::default(),
            desired: Default::default(),
            actual: Default::default(),
            actual_left_prompt: Default::default(),
            last_right_prompt_width: Default::default(),
            soft_wrap_location: Default::default(),
            need_clear_lines: Default::default(),
            need_clear_screen: Default::default(),
            actual_lines_before_reset: Default::default(),
            prev_buff_1: unsafe { std::mem::zeroed() },
            prev_buff_2: unsafe { std::mem::zeroed() },
        }
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
    /// \param cursor_is_within_pager whether the position is within the pager line (first line)
    pub fn write(
        &mut self,
        left_prompt: &wstr,
        right_prompt: &wstr,
        commandline: &wstr,
        explicit_len: usize,
        colors: &[HighlightSpec],
        indent: &[i32],
        cursor_pos: usize,
        vars: &dyn Environment,
        pager: &mut Pager,
        page_rendering: &mut PageRendering,
        cursor_is_within_pager: bool,
    ) {
        let curr_termsize = termsize_last();
        let screen_width = curr_termsize.width;
        static REPAINTS: AtomicU32 = AtomicU32::new(0);
        FLOGF!(
            screen,
            "Repaint %u",
            1 + REPAINTS.fetch_add(1, std::sync::atomic::Ordering::Relaxed)
        );
        let mut cursor_arr = Cursor::default();

        // Turn the command line into the explicit portion and the autosuggestion.
        let (explicit_command_line, autosuggestion) = commandline.split_at(explicit_len);

        // If we are using a dumb terminal, don't try any fancy stuff, just print out the text.
        // right_prompt not supported.
        if is_dumb() {
            let prompt_narrow = wcs2string(left_prompt);
            let command_line_narrow = wcs2string(explicit_command_line);

            let _ = write_loop(&STDOUT_FILENO, b"\r");
            let _ = write_loop(&STDOUT_FILENO, &prompt_narrow);
            let _ = write_loop(&STDOUT_FILENO, &command_line_narrow);

            return;
        }

        self.check_status();

        // Completely ignore impossibly small screens.
        if screen_width < 4 {
            return;
        }
        let screen_width = usize::try_from(screen_width).unwrap();

        // Compute a layout.
        let layout = compute_layout(
            screen_width,
            left_prompt,
            right_prompt,
            explicit_command_line,
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
        for _ in 0..layout.left_prompt_space {
            self.desired_append_char(' ', HighlightSpec::new(), 0, layout.left_prompt_space, 1);
        }

        // If overflowing, give the prompt its own line to improve the situation.
        let first_line_prompt_space = layout.left_prompt_space;

        // Reconstruct the command line.
        let effective_commandline = explicit_command_line.to_owned() + &layout.autosuggestion[..];

        // Output the command line.
        let mut i = 0;
        while i < effective_commandline.len() {
            // Grab the current cursor's x,y position if this character matches the cursor's offset.
            if !cursor_is_within_pager && i == cursor_pos {
                cursor_arr = self.desired.cursor;
            }
            self.desired_append_char(
                effective_commandline.as_char_slice()[i],
                colors[i],
                usize::try_from(indent[i]).unwrap(),
                first_line_prompt_space,
                wcwidth_rendered(effective_commandline.as_char_slice()[i]),
            );
            i += 1;
        }

        // Cursor may have been at the end too.
        if !cursor_is_within_pager && i == cursor_pos {
            cursor_arr = self.desired.cursor;
        }

        let full_line_count = self.desired.cursor.y + 1;

        // Now that we've output everything, set the cursor to the position that we saved in the loop
        // above.
        self.desired.cursor = cursor_arr;

        if cursor_is_within_pager {
            self.desired.cursor.x = cursor_pos;
            self.desired.cursor.y = self.desired.line_count();
        }

        // Re-render our completions page if necessary. Limit the term size of the pager to the true
        // term size, minus the number of lines consumed by our string.
        pager.set_term_size(&Termsize::new(
            std::cmp::max(1, curr_termsize.width),
            std::cmp::max(
                1,
                curr_termsize
                    .height
                    .saturating_sub_unsigned(full_line_count),
            ),
        ));

        pager.update_rendering(page_rendering);
        // Append pager_data (none if empty).
        self.desired.append_lines(&page_rendering.screen_data);

        self.update(&layout.left_prompt, &layout.right_prompt, vars);
        self.save_status();
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
            // If the prompt is multi-line, we need to move up to the prompt's initial line. We do this
            // by lying to ourselves and claiming that we're really below what we consider "line 0"
            // (which is the last line of the prompt). This will cause us to move up to try to get back
            // to line 0, but really we're getting back to the initial line of the prompt.
            let prompt_line_count = calc_prompt_lines(&self.actual_left_prompt);
            self.actual.cursor.y += prompt_line_count.checked_sub(1).unwrap();
            self.actual_left_prompt.clear();
        }
        self.actual.resize(0);
        self.need_clear_lines = true;

        // This should prevent resetting the cursor position during the next repaint.
        let _ = write_loop(&STDOUT_FILENO, b"\r");
        self.actual.cursor.x = 0;

        self.save_status();
    }

    /// Resets the screen buffer's internal knowledge about the contents of the screen,
    /// abandoning the current line and going to the next line.
    /// If clear_to_eos is set,
    /// The screen width must be provided for the PROMPT_SP hack.
    pub fn reset_abandoning_line(&mut self, screen_width: usize) {
        self.actual.cursor.y = 0;
        self.actual.resize(0);
        self.actual_left_prompt.clear();
        self.need_clear_lines = true;

        // Do the PROMPT_SP hack.
        let mut abandon_line_string = WString::with_capacity(screen_width + 32);

        // Don't need to check for fish_wcwidth errors; this is done when setting up
        // omitted_newline_char in common.cpp.
        let non_space_width = get_omitted_newline_width();
        let term = term();
        let term = term.as_ref();
        // We do `>` rather than `>=` because the code below might require one extra space.
        if screen_width > non_space_width {
            let mut justgrey = true;
            let add = |abandon_line_string: &mut WString, s: Option<CString>| {
                let Some(s) = s else {
                    return false;
                };
                abandon_line_string.push_utfstr(&str2wcstring(s.as_bytes()));
                true
            };
            if let Some(enter_dim_mode) = term.and_then(|term| term.enter_dim_mode.as_ref()) {
                if add(&mut abandon_line_string, tparm0(enter_dim_mode)) {
                    // Use dim if they have it, so the color will be based on their actual normal
                    // color and the background of the terminal.
                    justgrey = false;
                }
            }
            if let (true, Some(set_a_foreground)) = (
                justgrey,
                term.and_then(|term| term.set_a_foreground.as_ref()),
            ) {
                let max_colors = term.unwrap().max_colors.unwrap_or_default();
                if max_colors >= 238 {
                    // draw the string in a particular grey
                    add(&mut abandon_line_string, tparm1(set_a_foreground, 237));
                } else if max_colors >= 9 {
                    // bright black (the ninth color, looks grey)
                    add(&mut abandon_line_string, tparm1(set_a_foreground, 8));
                } else if max_colors >= 2 {
                    if let Some(enter_bold_mode) = term.unwrap().enter_bold_mode.as_ref() {
                        // we might still get that color by setting black and going bold for bright
                        add(&mut abandon_line_string, tparm0(enter_bold_mode));
                        add(&mut abandon_line_string, tparm1(set_a_foreground, 0));
                    }
                }
            }

            abandon_line_string.push_utfstr(&get_omitted_newline_str());

            if let Some(exit_attribute_mode) =
                term.and_then(|term| term.exit_attribute_mode.as_ref())
            {
                // normal text ANSI escape sequence
                add(&mut abandon_line_string, tparm0(exit_attribute_mode));
            }

            let newline_glitch_width = if TERM_HAS_XN.load(Ordering::Relaxed) {
                0
            } else {
                1
            };
            for _ in 0..screen_width - non_space_width - newline_glitch_width {
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
        if !is_dumb() {
            if let Some(clr_eol) = term.unwrap().clr_eol.as_ref() {
                abandon_line_string.push_utfstr(&str2wcstring(clr_eol.as_bytes()));
            }
        }

        let narrow_abandon_line_string = wcs2string(&abandon_line_string);
        let _ = write_loop(&STDOUT_FILENO, &narrow_abandon_line_string);
        self.actual.cursor.x = 0;

        self.save_status();
    }

    /// Stat stdout and stderr and save result as the current timestamp.
    /// This is used to avoid reacting to changes that we ourselves made to the screen.
    pub fn save_status(&mut self) {
        unsafe {
            libc::fstat(STDOUT_FILENO, &mut self.prev_buff_1);
            libc::fstat(STDERR_FILENO, &mut self.prev_buff_2);
        }
    }

    /// Return whether we believe the cursor is wrapped onto the last line, and that line is
    /// otherwise empty. This includes both soft and hard wrapping.
    pub fn cursor_is_wrapped_to_own_line(&self) -> bool {
        // Note == comparison against the line count is correct: we do not create a line just for the
        // cursor. If there is a line containing the cursor, then it means that line has contents and we
        // should return false.
        // Don't consider dumb terminals to have wrapping for the purposes of this function.
        self.actual.cursor.x == 0 && self.actual.cursor.y == self.actual.line_count() && !is_dumb()
    }

    /// Appends a character to the end of the line that the output cursor is on. This function
    /// automatically handles linebreaks and lines longer than the screen width.
    fn desired_append_char(
        &mut self,
        b: char,
        c: HighlightSpec,
        indent: usize,
        prompt_width: usize,
        bwidth: usize,
    ) {
        let mut line_no = self.desired.cursor.y;

        if b == '\n' {
            // Current line is definitely hard wrapped.
            // Create the next line.
            self.desired.create_line(self.desired.cursor.y + 1);
            self.desired.line_mut(self.desired.cursor.y).is_soft_wrapped = false;
            self.desired.cursor.y += 1;
            let line_no = self.desired.cursor.y;
            self.desired.cursor.x = 0;
            let indentation = prompt_width + indent * INDENT_STEP;
            let line = self.desired.line_mut(line_no);
            line.indentation = indentation;
            for _ in 0..indentation {
                self.desired_append_char(' ', HighlightSpec::default(), indent, prompt_width, 1);
            }
        } else if b == '\r' {
            let current = self.desired.line_mut(line_no);
            current.clear();
            self.desired.cursor.x = 0;
        } else {
            let screen_width = self.desired.screen_width;
            let cw = bwidth;

            self.desired.create_line(line_no);

            // Check if we are at the end of the line. If so, continue on the next line.
            if screen_width.is_none_or(|sw| (self.desired.cursor.x + cw) > sw) {
                // Current line is soft wrapped (assuming we support it).
                self.desired.line_mut(self.desired.cursor.y).is_soft_wrapped = true;

                line_no = self.desired.line_count();
                self.desired.add_line();
                self.desired.cursor.y += 1;
                self.desired.cursor.x = 0;
            }

            self.desired.line_mut(line_no).append(b, c);
            self.desired.cursor.x += cw;

            // Maybe wrap the cursor to the next line, even if the line itself did not wrap. This
            // avoids wonkiness in the last column.
            if screen_width.is_none_or(|sw| self.desired.cursor.x >= sw) {
                self.desired.line_mut(line_no).is_soft_wrapped = true;
                self.desired.cursor.x = 0;
                self.desired.cursor.y += 1;
            }
        }
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

        let mut post_buff_1: libc::stat = unsafe { std::mem::zeroed() };
        let mut post_buff_2: libc::stat = unsafe { std::mem::zeroed() };
        unsafe { libc::fstat(STDOUT_FILENO, &mut post_buff_1) };
        unsafe { libc::fstat(STDERR_FILENO, &mut post_buff_2) };

        // Yes these differ in one `_`. I hate it.
        #[cfg(not(target_os = "netbsd"))]
        let changed = self.prev_buff_1.st_mtime != post_buff_1.st_mtime
            || self.prev_buff_1.st_mtime_nsec != post_buff_1.st_mtime_nsec
            || self.prev_buff_2.st_mtime != post_buff_2.st_mtime
            || self.prev_buff_2.st_mtime_nsec != post_buff_2.st_mtime_nsec;
        #[cfg(target_os = "netbsd")]
        let changed = self.prev_buff_1.st_mtime != post_buff_1.st_mtime
            || self.prev_buff_1.st_mtimensec != post_buff_1.st_mtimensec
            || self.prev_buff_2.st_mtime != post_buff_2.st_mtime
            || self.prev_buff_2.st_mtimensec != post_buff_2.st_mtimensec;

        if changed {
            // Ok, someone has been messing with our screen. We will want to repaint. However, we do not
            // know where the cursor is. It is our best bet that we are still on the same line, so we
            // move to the beginning of the line, reset the modelled screen contents, and then set the
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

        let mut zelf = self.scoped_buffer();

        // If we are at the end of our window, then either the cursor stuck to the edge or it didn't. We
        // don't know! We can fix it up though.
        if zelf
            .actual
            .screen_width
            .is_some_and(|sw| zelf.actual.cursor.x == sw)
        {
            // Either issue a cr to go back to the beginning of this line, or a nl to go to the
            // beginning of the next one, depending on what we think is more efficient.
            if new_y <= zelf.actual.cursor.y {
                zelf.outp.borrow_mut().push(b'\r');
            } else {
                zelf.outp.borrow_mut().push(b'\n');
                zelf.actual.cursor.y += 1;
            }
            // Either way we're not in the first column.
            zelf.actual.cursor.x = 0;
        }

        let y_steps =
            isize::try_from(new_y).unwrap() - isize::try_from(zelf.actual.cursor.y).unwrap();

        let Some(term) = term() else {
            return;
        };
        let term = term.as_ref();

        let s = if y_steps < 0 {
            term.cursor_up.as_ref()
        } else if y_steps > 0 {
            let s = term.cursor_down.as_ref();
            if (shell_modes().c_oflag & ONLCR) != 0 && s.is_some_and(|s| s.as_bytes() == b"\n") {
                // See GitHub issue #4505.
                // Most consoles use a simple newline as the cursor down escape.
                // If ONLCR is enabled (which it normally is) this will of course
                // also move the cursor to the beginning of the line.
                // We could do:
                // if (std::strcmp(cursor_up, "\x1B[A") == 0) str = "\x1B[B";
                // else ... but that doesn't work for unknown reasons.
                zelf.actual.cursor.x = 0;
            }
            s
        } else {
            None
        };

        for _ in 0..y_steps.abs_diff(0) {
            zelf.outp.borrow_mut().tputs_if_some(&s);
        }

        let mut x_steps =
            isize::try_from(new_x).unwrap() - isize::try_from(zelf.actual.cursor.x).unwrap();
        if x_steps != 0 && new_x == 0 {
            zelf.outp.borrow_mut().push(b'\r');
            x_steps = 0;
        }

        let (s, multi_str) = if x_steps < 0 {
            (term.cursor_left.as_ref(), term.parm_left_cursor.as_ref())
        } else {
            (term.cursor_right.as_ref(), term.parm_right_cursor.as_ref())
        };

        // Use the bulk ('multi') zelf.output for cursor movement if it is supported and it would be shorter
        // Note that this is required to avoid some visual glitches in iTerm (issue #1448).
        let use_multi = multi_str.is_some_and(|ms| !ms.as_bytes().is_empty())
            && x_steps.abs_diff(0) * s.map_or(0, |s| s.as_bytes().len())
                > multi_str.unwrap().as_bytes().len();
        if use_multi {
            let multi_param = tparm1(
                multi_str.as_ref().unwrap(),
                i32::try_from(x_steps.abs_diff(0)).unwrap(),
            );
            zelf.outp.borrow_mut().tputs_if_some(&multi_param);
        } else {
            for _ in 0..x_steps.abs_diff(0) {
                zelf.outp.borrow_mut().tputs_if_some(&s);
            }
        }

        zelf.actual.cursor.x = new_x;
        zelf.actual.cursor.y = new_y;
    }

    /// Convert a wide character to a multibyte string and append it to the buffer.
    fn write_char(&mut self, c: char, width: isize) {
        let mut zelf = self.scoped_buffer();
        zelf.actual.cursor.x = zelf.actual.cursor.x.wrapping_add(width as usize);
        zelf.outp.borrow_mut().writech(c);
        if Some(zelf.actual.cursor.x) == zelf.actual.screen_width && allow_soft_wrap() {
            zelf.soft_wrap_location = Some(Cursor {
                x: 0,
                y: zelf.actual.cursor.y + 1,
            });

            // Note that our cursor position may be a lie: Apple Terminal makes the right cursor stick
            // to the margin, while Ubuntu makes it "go off the end" (but still doesn't wrap). We rely
            // on s_move to fix this up.
        } else {
            zelf.soft_wrap_location = None;
        }
    }

    /// Send the specified string through tputs and append the output to the screen's outputter.
    fn write_mbs(&mut self, s: &CStr) {
        self.outp.borrow_mut().tputs(s);
    }

    fn write_mbs_if_some(&mut self, s: &Option<impl AsRef<CStr>>) -> bool {
        self.outp.borrow_mut().tputs_if_some(s)
    }

    pub(crate) fn write_bytes(&mut self, s: &[u8]) {
        self.outp.borrow_mut().tputs_bytes(s);
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
            if self.desired.line(y - 1).is_soft_wrapped && allow_soft_wrap() {
                // Yes. Just update the actual cursor; that will cause us to elide emitting the commands
                // to move here, so we will just output on "one big line" (which the terminal soft
                // wraps.
                self.actual.cursor = self.soft_wrap_location.unwrap();
            }
        }
    }

    fn scoped_buffer(&mut self) -> impl ScopeGuarding<Target = &mut Screen> {
        self.outp.borrow_mut().begin_buffering();
        ScopeGuard::new(self, |zelf| {
            zelf.outp.borrow_mut().end_buffering();
        })
    }

    /// Update the screen to match the desired output.
    fn update(&mut self, left_prompt: &wstr, right_prompt: &wstr, vars: &dyn Environment) {
        // Helper function to set a resolved color, using the caching resolver.
        let mut color_resolver = HighlightColorResolver::new();
        let mut set_color = |zelf: &mut Self, c| {
            let fg = color_resolver.resolve_spec(&c, false, vars);
            let bg = color_resolver.resolve_spec(&c, true, vars);
            zelf.outp.borrow_mut().set_color(fg, bg);
        };

        let mut cached_layouts = LAYOUT_CACHE_SHARED.lock().unwrap();
        let mut zelf = self.scoped_buffer();

        // Determine size of left and right prompt. Note these have already been truncated.
        let left_prompt_layout = cached_layouts.calc_prompt_layout(left_prompt, None, usize::MAX);
        let left_prompt_width = left_prompt_layout.last_line_width;
        let right_prompt_width = cached_layouts
            .calc_prompt_layout(right_prompt, None, usize::MAX)
            .last_line_width;

        // Figure out how many following lines we need to clear (probably 0).
        let actual_lines_before_reset = zelf.actual_lines_before_reset;
        zelf.actual_lines_before_reset = 0;

        let mut need_clear_lines = zelf.need_clear_lines;
        let mut need_clear_screen = zelf.need_clear_screen;
        let mut has_cleared_screen = false;

        let screen_width = zelf.desired.screen_width;

        if zelf.actual.screen_width != screen_width {
            // Ensure we don't issue a clear screen for the very first output, to avoid issue #402.
            if zelf.actual.screen_width.is_some_and(|sw| sw > 0) {
                need_clear_screen = true;
                zelf.r#move(0, 0);
                zelf.reset_line(false);

                need_clear_lines |= zelf.need_clear_lines;
                need_clear_screen |= zelf.need_clear_screen;
            }
            zelf.actual.screen_width = screen_width;
        }

        zelf.need_clear_lines = false;
        zelf.need_clear_screen = false;

        // Determine how many lines have stuff on them; we need to clear lines with stuff that we don't
        // want.
        let lines_with_stuff = std::cmp::max(actual_lines_before_reset, zelf.actual.line_count());
        if zelf.desired.line_count() < lines_with_stuff {
            need_clear_screen = true;
        }

        let term = term();
        let term = term.as_ref();

        // Output the left prompt if it has changed.
        if left_prompt != zelf.actual_left_prompt {
            zelf.r#move(0, 0);
            zelf.write_bytes(b"\x1b]133;A;special_key=1\x07");
            let mut start = 0;
            for line_break in left_prompt_layout.line_breaks {
                zelf.write_str(&left_prompt[start..line_break]);
                zelf.outp
                    .borrow_mut()
                    .tputs_if_some(&term.and_then(|term| term.clr_eol.as_ref()));
                start = line_break;
            }
            zelf.write_str(&left_prompt[start..]);
            zelf.actual_left_prompt = left_prompt.to_owned();
            zelf.actual.cursor.x = left_prompt_width;
        }

        fn o_line(zelf: &Screen, i: usize) -> &Line {
            zelf.desired.line(i)
        }
        fn s_line(zelf: &Screen, i: usize) -> &Line {
            zelf.actual.line(i)
        }

        // Output all lines.
        for i in 0..zelf.desired.line_count() {
            zelf.actual.create_line(i);

            let start_pos = if i == 0 { left_prompt_width } else { 0 };
            let mut current_width = 0;
            let mut has_cleared_line = false;

            // If this is the last line, maybe we should clear the screen.
            // Don't issue clr_eos if we think the cursor will end up in the last column - see #6951.
            let should_clear_screen_this_line = need_clear_screen
                && i + 1 == zelf.desired.line_count()
                && term.is_some_and(|term| term.clr_eos.is_some())
                && !(zelf.desired.cursor.x == 0
                    && zelf.desired.cursor.y == zelf.desired.line_count());

            // skip_remaining is how many columns are unchanged on this line.
            // Note that skip_remaining is a width, not a character count.
            let mut skip_remaining = start_pos;

            let shared_prefix = line_shared_prefix(o_line(&zelf, i), s_line(&zelf, i));
            let mut skip_prefix = shared_prefix;
            if shared_prefix < o_line(&zelf, i).indentation {
                if o_line(&zelf, i).indentation > s_line(&zelf, i).indentation
                    && !has_cleared_screen
                    && term.is_some_and(|term| term.clr_eol.is_some() && term.clr_eos.is_some())
                {
                    set_color(&mut zelf, HighlightSpec::new());
                    zelf.r#move(0, i);
                    let term = term.unwrap();
                    zelf.write_mbs_if_some(if should_clear_screen_this_line {
                        &term.clr_eos
                    } else {
                        &term.clr_eol
                    });
                    has_cleared_screen = should_clear_screen_this_line;
                    has_cleared_line = true;
                }
                skip_prefix = o_line(&zelf, i).indentation;
            }

            // Compute how much we should skip. At a minimum we skip over the prompt. But also skip
            // over the shared prefix of what we want to output now, and what we output before, to
            // avoid repeatedly outputting it.
            if skip_prefix > 0 {
                let skip_width = if shared_prefix < skip_prefix {
                    skip_prefix
                } else {
                    o_line(&zelf, i).wcswidth_min_0(shared_prefix)
                };
                if skip_width > skip_remaining {
                    skip_remaining = skip_width;
                }
            }

            if !should_clear_screen_this_line {
                // If we're soft wrapped, and if we're going to change the first character of the next
                // line, don't skip over the last two characters so that we maintain soft-wrapping.
                if o_line(&zelf, i).is_soft_wrapped && i + 1 < zelf.desired.line_count() {
                    let mut next_line_will_change = true;
                    if i + 1 < zelf.actual.line_count() {
                        if line_shared_prefix(zelf.desired.line(i + 1), zelf.actual.line(i + 1)) > 0
                        {
                            next_line_will_change = false;
                        }
                    }
                    if next_line_will_change {
                        skip_remaining =
                            std::cmp::min(skip_remaining, zelf.actual.screen_width.unwrap() - 2);
                    }
                }
            }

            // Skip over skip_remaining width worth of characters.
            let mut j = 0;
            while j < o_line(&zelf, i).len() {
                let width = wcwidth_rendered(o_line(&zelf, i).char_at(j));
                if skip_remaining < width {
                    break;
                }
                skip_remaining -= width;
                current_width += width;
                j += 1;
            }

            // Skip over zero-width characters (e.g. combining marks at the end of the prompt).
            while j < o_line(&zelf, i).len() {
                let width = wcwidth_rendered(o_line(&zelf, i).char_at(j));
                if width > 0 {
                    break;
                }
                j += 1;
            }

            // Now actually output stuff.
            loop {
                let done = j >= o_line(&zelf, i).len();
                // Clear the screen if we have not done so yet.
                // If we are about to output into the last column, clear the screen first. If we clear
                // the screen after we output into the last column, it can erase the last character due
                // to the sticky right cursor. If we clear the screen too early, we can defeat soft
                // wrapping.
                if should_clear_screen_this_line
                    && !has_cleared_screen
                    && (done || Some(j + 1) == screen_width)
                {
                    set_color(&mut zelf, HighlightSpec::new());
                    zelf.r#move(current_width, i);
                    zelf.write_mbs_if_some(&term.and_then(|term| term.clr_eos.as_ref()));
                    has_cleared_screen = true;
                }
                if done {
                    break;
                }

                zelf.handle_soft_wrap(current_width, i);
                zelf.r#move(current_width, i);
                let color = o_line(&zelf, i).color_at(j);
                set_color(&mut zelf, color);
                let ch = o_line(&zelf, i).char_at(j);
                let width = wcwidth_rendered(ch);
                zelf.write_char(ch, isize::try_from(width).unwrap());
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
            } else if right_prompt_width < zelf.last_right_prompt_width {
                clear_remainder = true;
            } else {
                // This wcswidth shows up strong in the profile.
                // Only do it if the previous line could conceivably be wider.
                // That means if it is a prefix of the current one we can skip it.
                if s_line(&zelf, i).text.len() != shared_prefix {
                    let prev_width = s_line(&zelf, i).wcswidth_min_0(usize::MAX);
                    clear_remainder = prev_width > current_width;
                }
            }

            // We unset the color even if we don't clear the line.
            // This means that we switch background correctly on the next,
            // including our weird implicit bolding.
            set_color(&mut zelf, HighlightSpec::new());
            if let (true, Some(clr_eol)) =
                (clear_remainder, term.and_then(|term| term.clr_eol.as_ref()))
            {
                zelf.r#move(current_width, i);
                zelf.write_mbs(clr_eol);
            }

            // Output any rprompt if this is the first line.
            if i == 0 && right_prompt_width > 0 {
                // Move the cursor to the beginning of the line first to be independent of the width.
                // This helps prevent staircase effects if fish and the terminal disagree.
                zelf.r#move(0, 0);
                zelf.r#move(screen_width.unwrap() - right_prompt_width, i);
                set_color(&mut zelf, HighlightSpec::new());
                zelf.write_str(right_prompt);
                zelf.actual.cursor.x += right_prompt_width;

                // We output in the last column. Some terms (Linux) push the cursor further right, past
                // the window. Others make it "stick." Since we don't really know which is which, issue
                // a cr so it goes back to the left.
                //
                // However, if the user is resizing the window smaller, then it's possible the cursor
                // wrapped. If so, then a cr will go to the beginning of the following line! So instead
                // issue a bunch of "move left" commands to get back onto the line, and then jump to the
                // front of it.
                let Cursor { x, y } = zelf.actual.cursor;
                zelf.r#move(x - right_prompt_width, y);
                zelf.write_str(L!("\r"));
                zelf.actual.cursor.x = 0;
            }
        }

        // Also move the cursor to the beginning of the line here,
        // in case we're wrong about the width anywhere.
        // Don't do it when running in midnight_commander because of
        // https://midnight-commander.org/ticket/4258.
        if !MIDNIGHT_COMMANDER_HACK.load() {
            zelf.r#move(0, 0);
        }

        // Clear remaining lines (if any) if we haven't cleared the screen.
        if let (false, true, Some(clr_eol)) = (
            has_cleared_screen,
            need_clear_screen,
            term.and_then(|term| term.clr_eol.as_ref()),
        ) {
            set_color(&mut zelf, HighlightSpec::new());
            for i in zelf.desired.line_count()..lines_with_stuff {
                zelf.r#move(0, i);
                zelf.write_mbs(clr_eol);
            }
        }

        let Cursor { x, y } = zelf.desired.cursor;
        zelf.r#move(x, y);
        set_color(&mut zelf, HighlightSpec::new());

        // We have now synced our actual screen against our desired screen. Note that this is a big
        // assignment!
        zelf.actual = zelf.desired.clone();
        zelf.last_right_prompt_width = right_prompt_width;
    }
}

/// Issues an immediate clr_eos.
pub fn screen_force_clear_to_end() {
    Outputter::stdoutput()
        .borrow_mut()
        .tputs_if_some(&term().unwrap().clr_eos);
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
// Note this is deliberately exported so that init_curses can clear it.
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
                    // If the prompt ends in a new line, that's one empy last line.
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

    /// Finds the layout for a prompt, promoting it to the front. Returns nullptr if not found.
    /// Note this points into our cache; do not modify the cache while the pointer lives.
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
/// the escape sequence based on querying terminfo and other heuristics.
pub fn escape_code_length(code: &wstr) -> Option<usize> {
    if code.char_at(0) != '\x1B' {
        return None;
    }

    is_visual_escape_seq(code)
        .or_else(|| is_screen_name_escape_seq(code))
        .or_else(|| is_osc_escape_seq(code))
        .or_else(|| is_three_byte_escape_seq(code))
        .or_else(|| is_csi_style_escape_seq(code))
        .or_else(|| is_two_byte_escape_seq(code))
}

pub fn screen_clear() -> WString {
    term()
        .unwrap()
        .clear_screen
        .as_ref()
        .map(|clear_screen| str2wcstring(clear_screen.as_bytes()))
        .unwrap_or_default()
}

static MIDNIGHT_COMMANDER_HACK: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

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
    // Assume tab stops every 8 characters if undefined.
    let tab_width = term().unwrap().init_tabs.unwrap_or(8);
    ((current_line_width / tab_width) + 1) * tab_width
}

/// Whether we permit soft wrapping. If so, in some cases we don't explicitly move to the second
/// physical line on a wrapped logical line; instead we just output it.
fn allow_soft_wrap() -> bool {
    // Should we be looking at eat_newline_glitch as well?
    term().unwrap().auto_right_margin
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
/// See https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
/// and https://iterm2.com/documentation-escape-codes.html .
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

/// Generic VT100 three byte sequence: CSI followed by something in the range @ through _.
fn is_three_byte_escape_seq(code: &wstr) -> Option<usize> {
    if code.char_at(1) == '[' && (code.char_at(2) >= '@' && code.char_at(2) <= '_') {
        return Some(3);
    }
    None
}

/// Generic VT100 two byte sequence: <esc> followed by something in the range @ through _.
fn is_two_byte_escape_seq(code: &wstr) -> Option<usize> {
    if code.char_at(1) >= '@' && code.char_at(1) <= '_' {
        return Some(2);
    }
    None
}

/// Generic VT100 CSI-style sequence. <esc>, followed by zero or more ASCII characters NOT in
/// the range [@,_], followed by one character in that range.
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
fn is_visual_escape_seq(code: &wstr) -> Option<usize> {
    let term = term()?;
    let esc2 = [
        &term.enter_bold_mode,
        &term.exit_attribute_mode,
        &term.enter_underline_mode,
        &term.exit_underline_mode,
        &term.enter_standout_mode,
        &term.exit_standout_mode,
        &term.enter_blink_mode,
        &term.enter_protected_mode,
        &term.enter_italics_mode,
        &term.exit_italics_mode,
        &term.enter_reverse_mode,
        &term.enter_shadow_mode,
        &term.exit_shadow_mode,
        &term.enter_secure_mode,
        &term.enter_dim_mode,
        &term.enter_alt_charset_mode,
        &term.exit_alt_charset_mode,
    ];

    for p in &esc2 {
        let Some(p) = p else { continue };
        // Test both padded and unpadded version, just to be safe. Most versions of fish_tparm don't
        // actually seem to do anything these days.
        let esc_seq_len = std::cmp::max(
            try_sequence(tparm0(p).unwrap().as_bytes(), code),
            try_sequence(p.as_bytes(), code),
        );
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
            width += wcwidth_rendered(input.char_at(idx));
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
            let char_width = wcwidth_rendered(c);
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

/// Returns true if we are using a dumb terminal.
fn is_dumb() -> bool {
    term().is_none_or(|term| {
        term.cursor_up.is_none()
            || term.cursor_down.is_none()
            || term.cursor_left.is_none()
            || term.cursor_right.is_none()
    })
}

#[derive(Default)]
struct ScreenLayout {
    // The left prompt that we're going to use.
    left_prompt: WString,
    // How much space to leave for it.
    left_prompt_space: usize,
    // The right prompt.
    right_prompt: WString,
    // The autosuggestion.
    autosuggestion: WString,
}

// Given a vector whose indexes are offsets and whose values are the widths of the string if
// truncated at that offset, return the offset that fits in the given width. Returns
// width_by_offset.size() - 1 if they all fit. The first value in width_by_offset is assumed to be
// 0.
fn truncation_offset_for_width(width_by_offset: &[usize], max_width: usize) -> usize {
    assert!(width_by_offset[0] == 0);
    let mut i = 1;
    while i < width_by_offset.len() {
        if width_by_offset[i] > max_width {
            break;
        }
        i += 1;
    }
    // i is the first index that did not fit; i-1 is therefore the last that did.
    i - 1
}

fn compute_layout(
    screen_width: usize,
    left_untrunc_prompt: &wstr,
    right_untrunc_prompt: &wstr,
    commandline: &wstr,
    autosuggestion_str: &wstr,
) -> ScreenLayout {
    let mut result = ScreenLayout::default();

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

    if left_prompt_width + right_prompt_width > screen_width {
        // Nix right_prompt.
        right_prompt.truncate(0);
        right_prompt_width = 0;
    }

    // Now we should definitely fit.
    assert!(left_prompt_width + right_prompt_width <= screen_width);

    // Get the width of the first line, and if there is more than one line.
    let mut multiline = false;
    let mut first_line_width = 0;
    for c in commandline.chars() {
        if c == '\n' {
            multiline = true;
            break;
        } else {
            first_line_width += wcwidth_rendered(c);
        }
    }
    let first_command_line_width = first_line_width;

    // If we have more than one line, ensure we have no autosuggestion.
    let mut autosuggestion = autosuggestion_str;
    let mut autosuggest_total_width = 0;
    let mut autosuggest_truncated_widths = vec![];
    if multiline {
        autosuggestion = L!("");
    } else {
        autosuggest_truncated_widths.reserve(1 + autosuggestion_str.len());
        for c in autosuggestion.chars() {
            autosuggest_truncated_widths.push(autosuggest_total_width);
            autosuggest_total_width += wcwidth_rendered(c);
        }
    }

    // Here are the layouts we try in turn:
    //
    // 1. Left prompt visible, right prompt visible, command line visible, autosuggestion visible.
    //
    // 2. Left prompt visible, right prompt visible, command line visible, autosuggestion truncated
    // (possibly to zero).
    //
    // 3. Left prompt visible, right prompt hidden, command line visible, autosuggestion visible
    //
    // 4. Left prompt visible, right prompt hidden, command line visible, autosuggestion truncated
    //
    // 5. Newline separator (left prompt visible, right prompt hidden, command line visible,
    // autosuggestion visible).
    //
    // A remark about layout #4: if we've pushed the command line to a new line, why can't we draw
    // the right prompt? The issue is resizing: if you resize the window smaller, then the right
    // prompt will wrap to the next line. This means that we can't go back to the line that we were
    // on, and things turn to chaos very quickly.

    // Case 1
    let calculated_width =
        left_prompt_width + right_prompt_width + first_command_line_width + autosuggest_total_width;
    if calculated_width <= screen_width {
        result.left_prompt = left_prompt;
        result.left_prompt_space = left_prompt_width;
        result.right_prompt = right_prompt;
        result.autosuggestion = autosuggestion.to_owned();
        return result;
    }

    // Case 2. Note that we require strict inequality so that there's always at least one space
    // between the left edge and the rprompt.
    let calculated_width = left_prompt_width + right_prompt_width + first_command_line_width;
    if calculated_width <= screen_width {
        result.left_prompt = left_prompt;
        result.left_prompt_space = left_prompt_width;
        result.right_prompt = right_prompt;

        // Need at least two characters to show an autosuggestion.
        let available_autosuggest_space =
            screen_width - (left_prompt_width + right_prompt_width + first_command_line_width);
        if autosuggest_total_width > 0 && available_autosuggest_space > 2 {
            let truncation_offset = truncation_offset_for_width(
                &autosuggest_truncated_widths,
                available_autosuggest_space - 2,
            );
            result.autosuggestion = autosuggestion[..truncation_offset].to_owned();
            result.autosuggestion.push(get_ellipsis_char());
        }
        return result;
    }

    // Case 3
    let calculated_width = left_prompt_width + first_command_line_width + autosuggest_total_width;
    if calculated_width <= screen_width {
        result.left_prompt = left_prompt;
        result.left_prompt_space = left_prompt_width;
        result.autosuggestion = autosuggestion.to_owned();
        return result;
    }

    // Case 4
    let calculated_width = left_prompt_width + first_command_line_width;
    if calculated_width <= screen_width {
        result.left_prompt = left_prompt;
        result.left_prompt_space = left_prompt_width;

        // Need at least two characters to show an autosuggestion.
        let available_autosuggest_space =
            screen_width - (left_prompt_width + first_command_line_width);
        if autosuggest_total_width > 0 && available_autosuggest_space > 2 {
            let truncation_offset = truncation_offset_for_width(
                &autosuggest_truncated_widths,
                available_autosuggest_space - 2,
            );
            result.autosuggestion = autosuggestion[..truncation_offset].to_owned();
            result.autosuggestion.push(get_ellipsis_char());
        }
        return result;
    }

    // Case 5
    result.left_prompt = left_prompt;
    result.left_prompt_space = left_prompt_width;
    result.autosuggestion = autosuggestion.to_owned();
    result
}

// Display non-printable control characters as a graphic symbol.
// This is to prevent control characters like \t and \v from moving the
// cursor in a way we don't handle.  The ones we do handle are \r and
// \n.
// See https://unicode-table.com/en/blocks/control-pictures/
fn rendered_character(c: char) -> char {
    if fish_reserved_codepoint(c) {
        return '�'; // replacement character
    }
    if c <= '\x1F' {
        char::from_u32(u32::from(c) + 0x2400).unwrap()
    } else {
        c
    }
}

fn wcwidth_rendered(c: char) -> usize {
    usize::try_from(fish_wcwidth(rendered_character(c))).unwrap_or_default()
}
