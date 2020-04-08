// High level library for handling the terminal screen.
//
// The screen library allows the interactive reader to write its output to screen efficiently by
// keeping an internal representation of the current screen contents and trying to find the most
// efficient way for transforming that to the desired screen content.
//
// IWYU pragma: no_include <cstddef>
#include "config.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <cstring>
#include <cwchar>

#if HAVE_CURSES_H
#include <curses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#elif HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#endif
#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif

#include <algorithm>
#include <string>
#include <vector>

#include "common.h"
#include "env.h"
#include "fallback.h"  // IWYU pragma: keep
#include "flog.h"
#include "highlight.h"
#include "output.h"
#include "pager.h"
#include "screen.h"

/// The number of characters to indent new blocks.
#define INDENT_STEP 4u

static void invalidate_soft_wrap(screen_t *scr);

/// RAII class to begin and end buffering around stdoutput().
class scoped_buffer_t {
    screen_t &screen_;

   public:
    explicit scoped_buffer_t(screen_t &s) : screen_(s) { screen_.outp().beginBuffering(); }

    ~scoped_buffer_t() { screen_.outp().endBuffering(); }
};

// Singleton of the cached escape sequences seen in prompts and similar strings.
// Note this is deliberately exported so that init_curses can clear it.
layout_cache_t cached_layouts;

/// Tests if the specified narrow character sequence is present at the specified position of the
/// specified wide character string. All of \c seq must match, but str may be longer than seq.
static size_t try_sequence(const char *seq, const wchar_t *str) {
    for (size_t i = 0;; i++) {
        if (!seq[i]) return i;
        if (seq[i] != str[i]) return 0;
    }

    DIE("unexpectedly fell off end of try_sequence()");
    return 0;  // this should never be executed
}

/// Returns the number of columns left until the next tab stop, given the current cursor position.
static size_t next_tab_stop(size_t current_line_width) {
    // Assume tab stops every 8 characters if undefined.
    size_t tab_width = init_tabs > 0 ? static_cast<size_t>(init_tabs) : 8;
    return ((current_line_width / tab_width) + 1) * tab_width;
}

/// Like fish_wcwidth, but returns 0 for control characters instead of -1.
static int fish_wcwidth_min_0(wchar_t widechar) { return std::max(0, fish_wcwidth(widechar)); }

/// Whether we permit soft wrapping. If so, in some cases we don't explicitly move to the second
/// physical line on a wrapped logical line; instead we just output it.
static bool allow_soft_wrap() {
    // Should we be looking at eat_newline_glitch as well?
    return auto_right_margin;
}

/// Does this look like the escape sequence for setting a screen name?
static bool is_screen_name_escape_seq(const wchar_t *code, size_t *resulting_length) {
    if (code[1] != L'k') {
        return false;
    }
    const wchar_t *const screen_name_end_sentinel = L"\x1B\\";
    const wchar_t *screen_name_end = std::wcsstr(&code[2], screen_name_end_sentinel);
    if (screen_name_end == nullptr) {
        // Consider just <esc>k to be the code.
        *resulting_length = 2;
    } else {
        const wchar_t *escape_sequence_end =
            screen_name_end + std::wcslen(screen_name_end_sentinel);
        *resulting_length = escape_sequence_end - code;
    }
    return true;
}

/// Operating System Command (OSC) escape codes, used by iTerm2 and others:
/// ESC followed by ], terminated by either BEL or escape + backslash.
/// See https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
/// and https://iterm2.com/documentation-escape-codes.html .
static bool is_osc_escape_seq(const wchar_t *code, size_t *resulting_length) {
    bool found = false;
    if (code[1] == ']') {
        // Start at 2 to skip over <esc>].
        size_t cursor = 2;
        for (; code[cursor] != L'\0'; cursor++) {
            // Consume a sequence of characters up to <esc>\ or <bel>.
            if (code[cursor] == '\x07' || (code[cursor] == '\\' && code[cursor - 1] == '\x1B')) {
                found = true;
                break;
            }
        }
        if (found) {
            *resulting_length = cursor + 1;
        }
    }
    return found;
}

/// Generic VT100 three byte sequence: CSI followed by something in the range @ through _.
static bool is_three_byte_escape_seq(const wchar_t *code, size_t *resulting_length) {
    bool found = false;
    if (code[1] == L'[' && (code[2] >= L'@' && code[2] <= L'_')) {
        *resulting_length = 3;
        found = true;
    }
    return found;
}

/// Generic VT100 two byte sequence: <esc> followed by something in the range @ through _.
static bool is_two_byte_escape_seq(const wchar_t *code, size_t *resulting_length) {
    bool found = false;
    if (code[1] >= L'@' && code[1] <= L'_') {
        *resulting_length = 2;
        found = true;
    }
    return found;
}

/// Generic VT100 CSI-style sequence. <esc>, followed by zero or more ASCII characters NOT in
/// the range [@,_], followed by one character in that range.
static bool is_csi_style_escape_seq(const wchar_t *code, size_t *resulting_length) {
    if (code[1] != L'[') {
        return false;
    }

    // Start at 2 to skip over <esc>[
    size_t cursor = 2;
    for (; code[cursor] != L'\0'; cursor++) {
        // Consume a sequence of ASCII characters not in the range [@, ~].
        wchar_t widechar = code[cursor];

        // If we're not in ASCII, just stop.
        if (widechar > 127) break;

        // If we're the end character, then consume it and then stop.
        if (widechar >= L'@' && widechar <= L'~') {
            cursor++;
            break;
        }
    }
    // cursor now indexes just beyond the end of the sequence (or at the terminating zero).
    *resulting_length = cursor;
    return true;
}

/// Detect whether the escape sequence sets foreground/background color. Note that 24-bit color
/// sequences are detected by `is_csi_style_escape_seq()` if they use the ANSI X3.64 pattern for
/// such sequences. This function only handles those escape sequences for setting color that rely on
/// the terminfo definition and which might use a different pattern.
static bool is_color_escape_seq(const wchar_t *code, size_t *resulting_length) {
    if (!cur_term) return false;

    // Detect these terminfo color escapes with parameter value up to max_colors, all of which
    // don't move the cursor.
    const char *const esc[] = {
        set_a_foreground,
        set_a_background,
        set_foreground,
        set_background,
    };

    for (auto p : esc) {
        if (!p) continue;

        for (int k = 0; k < max_colors; k++) {
            size_t esc_seq_len = try_sequence(tparm(const_cast<char *>(p), k), code);
            if (esc_seq_len) {
                *resulting_length = esc_seq_len;
                return true;
            }
        }
    }

    return false;
}

/// Detect whether the escape sequence sets one of the terminal attributes that affects how text is
/// displayed other than the color.
static bool is_visual_escape_seq(const wchar_t *code, size_t *resulting_length) {
    if (!cur_term) return false;
    const char *const esc2[] = {
        enter_bold_mode,     exit_attribute_mode, enter_underline_mode,   exit_underline_mode,
        enter_standout_mode, exit_standout_mode,  enter_blink_mode,       enter_protected_mode,
        enter_italics_mode,  exit_italics_mode,   enter_reverse_mode,     enter_shadow_mode,
        exit_shadow_mode,    enter_standout_mode, exit_standout_mode,     enter_secure_mode,
        enter_dim_mode,      enter_blink_mode,    enter_alt_charset_mode, exit_alt_charset_mode};

    for (auto p : esc2) {
        if (!p) continue;
        // Test both padded and unpadded version, just to be safe. Most versions of tparm don't
        // actually seem to do anything these days.
        size_t esc_seq_len =
            std::max(try_sequence(tparm(const_cast<char *>(p)), code), try_sequence(p, code));
        if (esc_seq_len) {
            *resulting_length = esc_seq_len;
            return true;
        }
    }

    return false;
}

/// Returns the number of characters in the escape code starting at 'code'. We only handle sequences
/// that begin with \x1B. If it doesn't we return zero. We also return zero if we don't recognize
/// the escape sequence based on querying terminfo and other heuristics.
size_t escape_code_length(const wchar_t *code) {
    assert(code != nullptr);
    if (*code != L'\x1B') return 0;

    size_t esc_seq_len = cached_layouts.find_escape_code(code);
    if (esc_seq_len) return esc_seq_len;

    bool found = is_color_escape_seq(code, &esc_seq_len);
    if (!found) found = is_visual_escape_seq(code, &esc_seq_len);
    if (!found) found = is_screen_name_escape_seq(code, &esc_seq_len);
    if (!found) found = is_osc_escape_seq(code, &esc_seq_len);
    if (!found) found = is_three_byte_escape_seq(code, &esc_seq_len);
    if (!found) found = is_csi_style_escape_seq(code, &esc_seq_len);
    if (!found) found = is_two_byte_escape_seq(code, &esc_seq_len);
    if (found) cached_layouts.add_escape_code(wcstring(code, esc_seq_len));
    return esc_seq_len;
}

maybe_t<prompt_layout_t> layout_cache_t::find_prompt_layout(const wcstring &input) {
    auto start = prompt_cache_.begin();
    auto end = prompt_cache_.end();
    for (auto iter = start; iter != end; ++iter) {
        if (iter->first == input) {
            // Found it. Move it to the front if not already there.
            if (iter != start) prompt_cache_.splice(start, prompt_cache_, iter);
            return iter->second;
        }
    }
    return none();
}

void layout_cache_t::add_prompt_layout(wcstring input, prompt_layout_t layout) {
    assert(!find_prompt_layout(input) && "Should not have a prompt layout for this input");
    prompt_cache_.emplace_front(std::move(input), layout);
    if (prompt_cache_.size() > prompt_cache_max_size) {
        prompt_cache_.pop_back();
    }
}

/// Calculate layout information for the given prompt. Does some clever magic to detect common
/// escape sequences that may be embedded in a prompt, such as those to set visual attributes.
/// escape sequences that may be embedded in a prompt, such as those to set visual attributes.
static prompt_layout_t calc_prompt_layout(const wcstring &prompt_str, layout_cache_t &cache) {
    if (auto cached_layout = cache.find_prompt_layout(prompt_str)) {
        return *cached_layout;
    }

    prompt_layout_t prompt_layout = {1, 0, 0};
    size_t current_line_width = 0;

    const wchar_t *prompt = prompt_str.c_str();
    for (size_t j = 0; prompt[j]; j++) {
        if (prompt[j] == L'\x1B') {
            // This is the start of an escape code. Skip over it if it's at least one char long.
            size_t len = escape_code_length(&prompt[j]);
            if (len > 0) j += len - 1;
        } else if (prompt[j] == L'\t') {
            current_line_width = next_tab_stop(current_line_width);
        } else if (prompt[j] == L'\n' || prompt[j] == L'\f') {
            // PCA: At least one prompt uses \f\r as a newline. It's unclear to me what this is
            // meant to do, but terminals seem to treat it as a newline so we do the same.
            current_line_width = 0;
            prompt_layout.line_count += 1;
        } else if (prompt[j] == L'\r') {
            current_line_width = 0;
        } else {
            // Ordinary char. Add its width with care to ignore control chars which have width -1.
            current_line_width += fish_wcwidth_min_0(prompt[j]);
            if (current_line_width > prompt_layout.max_line_width) {
                prompt_layout.max_line_width = current_line_width;
            }
        }
    }
    prompt_layout.last_line_width = current_line_width;
    cache.add_prompt_layout(prompt, prompt_layout);
    return prompt_layout;
}

static size_t calc_prompt_lines(const wcstring &prompt) {
    // Hack for the common case where there's no newline at all. I don't know if a newline can
    // appear in an escape sequence, so if we detect a newline we have to defer to
    // calc_prompt_width_and_lines.
    size_t result = 1;
    if (prompt.find(L'\n') != wcstring::npos || prompt.find(L'\f') != wcstring::npos) {
        result = calc_prompt_layout(prompt, cached_layouts).line_count;
    }
    return result;
}

/// Stat stdout and stderr and save result. This should be done before calling a function that may
/// cause output.
void s_save_status(screen_t *s) {
    fstat(STDOUT_FILENO, &s->prev_buff_1);
    fstat(STDERR_FILENO, &s->prev_buff_2);
}

/// Stat stdout and stderr and compare result to previous result in reader_save_status. Repaint if
/// modification time has changed.
///
/// Unfortunately, for some reason this call seems to give a lot of false positives, at least under
/// Linux.
static void s_check_status(screen_t *s) {
    fflush(stdout);
    fflush(stderr);
    if (!has_working_tty_timestamps) {
        // We can't reliably determine if the terminal has been written to behind our back so we
        // just assume that hasn't happened and hope for the best. This is important for multi-line
        // prompts to work correctly.
        return;
    }

    struct stat post_buff_1 {};
    struct stat post_buff_2 {};
    fstat(STDOUT_FILENO, &post_buff_1);
    fstat(STDERR_FILENO, &post_buff_2);

    bool changed = (s->prev_buff_1.st_mtime != post_buff_1.st_mtime) ||
                   (s->prev_buff_2.st_mtime != post_buff_2.st_mtime);

#if defined HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC
    changed = changed || s->prev_buff_1.st_mtimespec.tv_nsec != post_buff_1.st_mtimespec.tv_nsec ||
              s->prev_buff_2.st_mtimespec.tv_nsec != post_buff_2.st_mtimespec.tv_nsec;
#elif defined HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
    changed = changed || s->prev_buff_1.st_mtim.tv_nsec != post_buff_1.st_mtim.tv_nsec ||
              s->prev_buff_2.st_mtim.tv_nsec != post_buff_2.st_mtim.tv_nsec;
#endif

    if (changed) {
        // Ok, someone has been messing with our screen. We will want to repaint. However, we do not
        // know where the cursor is. It is our best bet that we are still on the same line, so we
        // move to the beginning of the line, reset the modelled screen contents, and then set the
        // modeled cursor y-pos to its earlier value.
        int prev_line = s->actual.cursor.y;
        write_loop(STDOUT_FILENO, "\r", 1);
        s_reset(s, screen_reset_mode_t::current_line_and_prompt);
        s->actual.cursor.y = prev_line;
    }
}

/// Appends a character to the end of the line that the output cursor is on. This function
/// automatically handles linebreaks and lines longer than the screen width.
static void s_desired_append_char(screen_t *s, wchar_t b, highlight_spec_t c, int indent,
                                  size_t prompt_width, size_t bwidth) {
    int line_no = s->desired.cursor.y;

    if (b == L'\n') {
        // Current line is definitely hard wrapped.
        // Create the next line.
        s->desired.create_line(s->desired.cursor.y + 1);
        s->desired.line(s->desired.cursor.y).is_soft_wrapped = false;
        int line_no = ++s->desired.cursor.y;
        s->desired.cursor.x = 0;
        size_t indentation = prompt_width + indent * INDENT_STEP;
        line_t &line = s->desired.line(line_no);
        line.indentation = indentation;
        for (size_t i = 0; i < indentation; i++) {
            s_desired_append_char(s, L' ', highlight_spec_t{}, indent, prompt_width, 1);
        }
    } else if (b == L'\r') {
        line_t &current = s->desired.line(line_no);
        current.clear();
        s->desired.cursor.x = 0;
    } else {
        int screen_width = common_get_width();
        int cw = bwidth;

        s->desired.create_line(line_no);

        // Check if we are at the end of the line. If so, continue on the next line.
        if ((s->desired.cursor.x + cw) > screen_width) {
            // Current line is soft wrapped (assuming we support it).
            s->desired.line(s->desired.cursor.y).is_soft_wrapped = true;

            line_no = static_cast<int>(s->desired.line_count());
            s->desired.add_line();
            s->desired.cursor.y++;
            s->desired.cursor.x = 0;
        }

        line_t &line = s->desired.line(line_no);
        line.append(b, c);
        s->desired.cursor.x += cw;

        // Maybe wrap the cursor to the next line, even if the line itself did not wrap. This
        // avoids wonkiness in the last column.
        if (s->desired.cursor.x >= screen_width) {
            line.is_soft_wrapped = true;
            s->desired.cursor.x = 0;
            s->desired.cursor.y++;
        }
    }
}

/// Write the bytes needed to move screen cursor to the specified position to the specified buffer.
/// The actual_cursor field of the specified screen_t will be updated.
///
/// \param s the screen to operate on
/// \param new_x the new x position
/// \param new_y the new y position
static void s_move(screen_t *s, int new_x, int new_y) {
    if (s->actual.cursor.x == new_x && s->actual.cursor.y == new_y) return;

    const scoped_buffer_t buffering(*s);

    // If we are at the end of our window, then either the cursor stuck to the edge or it didn't. We
    // don't know! We can fix it up though.
    if (s->actual.cursor.x == common_get_width()) {
        // Either issue a cr to go back to the beginning of this line, or a nl to go to the
        // beginning of the next one, depending on what we think is more efficient.
        if (new_y <= s->actual.cursor.y) {
            s->outp().push_back('\r');
        } else {
            s->outp().push_back('\n');
            s->actual.cursor.y++;
        }
        // Either way we're not in the first column.
        s->actual.cursor.x = 0;
    }

    int i;
    int x_steps, y_steps;

    const char *str;
    auto &outp = s->outp();

    y_steps = new_y - s->actual.cursor.y;

    if (y_steps < 0) {
        str = cursor_up;
    } else if (y_steps > 0) {
        str = cursor_down;
        if ((shell_modes.c_oflag & ONLCR) != 0 &&
            std::strcmp(str, "\n") == 0) {  // See GitHub issue #4505.
            // Most consoles use a simple newline as the cursor down escape.
            // If ONLCR is enabled (which it normally is) this will of course
            // also move the cursor to the beginning of the line.
            // We could do:
            // if (std::strcmp(cursor_up, "\x1B[A") == 0) str = "\x1B[B";
            // else ... but that doesn't work for unknown reasons.
            s->actual.cursor.x = 0;
        }
    }

    for (i = 0; i < abs(y_steps); i++) {
        writembs(outp, str);
    }

    x_steps = new_x - s->actual.cursor.x;

    if (x_steps && new_x == 0) {
        outp.push_back('\r');
        x_steps = 0;
    }

    const char *multi_str = nullptr;
    if (x_steps < 0) {
        str = cursor_left;
        multi_str = parm_left_cursor;
    } else {
        str = cursor_right;
        multi_str = parm_right_cursor;
    }

    // Use the bulk ('multi') output for cursor movement if it is supported and it would be shorter
    // Note that this is required to avoid some visual glitches in iTerm (issue #1448).
    bool use_multi = multi_str != nullptr && multi_str[0] != '\0' &&
                     abs(x_steps) * std::strlen(str) > std::strlen(multi_str);
    if (use_multi && cur_term) {
        char *multi_param = tparm(const_cast<char *>(multi_str), abs(x_steps));
        writembs(outp, multi_param);
    } else {
        for (i = 0; i < abs(x_steps); i++) {
            writembs(outp, str);
        }
    }

    s->actual.cursor.x = new_x;
    s->actual.cursor.y = new_y;
}

/// Set the pen color for the terminal.
static void s_set_color(screen_t *s, const environment_t &vars, highlight_spec_t c) {
    UNUSED(s);
    UNUSED(vars);
    s->outp().set_color(highlight_get_color(c, false), highlight_get_color(c, true));
}

/// Convert a wide character to a multibyte string and append it to the buffer.
static void s_write_char(screen_t *s, wchar_t c, size_t width) {
    scoped_buffer_t outp(*s);
    s->actual.cursor.x += width;
    s->outp().writech(c);
    if (s->actual.cursor.x == s->actual_width && allow_soft_wrap()) {
        s->soft_wrap_location = screen_data_t::cursor_t{0, s->actual.cursor.y + 1};

        // Note that our cursor position may be a lie: Apple Terminal makes the right cursor stick
        // to the margin, while Ubuntu makes it "go off the end" (but still doesn't wrap). We rely
        // on s_move to fix this up.
    } else {
        invalidate_soft_wrap(s);
    }
}

/// Send the specified string through tputs and append the output to the screen's outputter.
static void s_write_mbs(screen_t *screen, const char *s) { writembs(screen->outp(), s); }

/// Convert a wide string to a multibyte string and append it to the buffer.
static void s_write_str(screen_t *screen, const wchar_t *s) { screen->outp().writestr(s); }

/// Returns the length of the "shared prefix" of the two lines, which is the run of matching text
/// and colors. If the prefix ends on a combining character, do not include the previous character
/// in the prefix.
static size_t line_shared_prefix(const line_t &a, const line_t &b) {
    size_t idx, max = std::min(a.size(), b.size());
    for (idx = 0; idx < max; idx++) {
        wchar_t ac = a.char_at(idx), bc = b.char_at(idx);

        // We're done if the text or colors are different.
        if (ac != bc || a.color_at(idx) != b.color_at(idx)) {
            if (idx > 0) {
                const line_t *c = nullptr;
                // Possible combining mark, go back until we hit _two_ printable characters or idx
                // of 0.
                if (fish_wcwidth(a.char_at(idx)) < 1) {
                    c = &a;
                } else if (fish_wcwidth(b.char_at(idx)) < 1) {
                    c = &b;
                }

                if (c) {
                    while (idx > 1 && (fish_wcwidth(c->char_at(idx - 1)) < 1 ||
                                       fish_wcwidth(c->char_at(idx)) < 1))
                        idx--;
                    if (idx == 1 && fish_wcwidth(c->char_at(idx)) < 1) idx = 0;
                }
            }
            break;
        }
    }
    return idx;
}

// We are about to output one or more characters onto the screen at the given x, y. If we are at the
// end of previous line, and the previous line is marked as soft wrapping, then tweak the screen so
// we believe we are already in the target position. This lets the terminal take care of wrapping,
// which means that if you copy and paste the text, it won't have an embedded newline.
static bool perform_any_impending_soft_wrap(screen_t *scr, int x, int y) {
    if (scr->soft_wrap_location && x == scr->soft_wrap_location->x &&
        y == scr->soft_wrap_location->y) {  //!OCLINT
        // We can soft wrap; but do we want to?
        if (scr->desired.line(y - 1).is_soft_wrapped && allow_soft_wrap()) {
            // Yes. Just update the actual cursor; that will cause us to elide emitting the commands
            // to move here, so we will just output on "one big line" (which the terminal soft
            // wraps.
            scr->actual.cursor = scr->soft_wrap_location.value();
        }
    }
    return false;
}

/// Make sure we don't soft wrap.
static void invalidate_soft_wrap(screen_t *scr) { scr->soft_wrap_location = none(); }

/// Update the screen to match the desired output.
static void s_update(screen_t *scr, const wcstring &left_prompt, const wcstring &right_prompt) {
    const environment_t &vars = env_stack_t::principal();
    const scoped_buffer_t buffering(*scr);
    const size_t left_prompt_width =
        calc_prompt_layout(left_prompt, cached_layouts).last_line_width;
    const size_t right_prompt_width =
        calc_prompt_layout(right_prompt, cached_layouts).last_line_width;

    int screen_width = common_get_width();

    // Figure out how many following lines we need to clear (probably 0).
    size_t actual_lines_before_reset = scr->actual_lines_before_reset;
    scr->actual_lines_before_reset = 0;

    bool need_clear_lines = scr->need_clear_lines;
    bool need_clear_screen = scr->need_clear_screen;
    bool has_cleared_screen = false;

    if (scr->actual_width != screen_width) {
        // Ensure we don't issue a clear screen for the very first output, to avoid issue #402.
        if (scr->actual_width > 0) {
            need_clear_screen = true;
            s_move(scr, 0, 0);
            s_reset(scr, screen_reset_mode_t::current_line_contents);

            need_clear_lines = need_clear_lines || scr->need_clear_lines;
            need_clear_screen = need_clear_screen || scr->need_clear_screen;
        }
        scr->actual_width = screen_width;
    }

    scr->need_clear_lines = false;
    scr->need_clear_screen = false;

    // Determine how many lines have stuff on them; we need to clear lines with stuff that we don't
    // want.
    const size_t lines_with_stuff = std::max(actual_lines_before_reset, scr->actual.line_count());
    if (scr->desired.line_count() < lines_with_stuff) need_clear_screen = true;

    // Output the left prompt if it has changed.
    if (left_prompt != scr->actual_left_prompt) {
        s_move(scr, 0, 0);
        s_write_str(scr, left_prompt.c_str());
        scr->actual_left_prompt = left_prompt;
        scr->actual.cursor.x = static_cast<int>(left_prompt_width);
    }

    // Output all lines.
    for (size_t i = 0; i < scr->desired.line_count(); i++) {
        const line_t &o_line = scr->desired.line(i);
        line_t &s_line = scr->actual.create_line(i);
        size_t start_pos = i == 0 ? left_prompt_width : 0;
        int current_width = 0;
        bool has_cleared_line = false;

        // If this is the last line, maybe we should clear the screen.
        // Don't issue clr_eos if we think the cursor will end up in the last column - see #6951.
        const bool should_clear_screen_this_line =
            need_clear_screen && i + 1 == scr->desired.line_count() && clr_eos != nullptr &&
            !(scr->desired.cursor.x == 0 &&
              scr->desired.cursor.y == static_cast<int>(scr->desired.line_count()));

        // skip_remaining is how many columns are unchanged on this line.
        // Note that skip_remaining is a width, not a character count.
        size_t skip_remaining = start_pos;

        const size_t shared_prefix = line_shared_prefix(o_line, s_line);
        size_t skip_prefix = shared_prefix;
        if (shared_prefix < o_line.indentation) {
            if (o_line.indentation > s_line.indentation && !has_cleared_screen && clr_eol &&
                clr_eos) {
                s_set_color(scr, vars, highlight_spec_t{});
                s_move(scr, 0, static_cast<int>(i));
                s_write_mbs(scr, should_clear_screen_this_line ? clr_eos : clr_eol);
                has_cleared_screen = should_clear_screen_this_line;
                has_cleared_line = true;
            }
            skip_prefix = o_line.indentation;
        }

        // Compute how much we should skip. At a minimum we skip over the prompt. But also skip
        // over the shared prefix of what we want to output now, and what we output before, to
        // avoid repeatedly outputting it.
        if (skip_prefix > 0) {
            size_t skip_width = shared_prefix < skip_prefix
                                    ? skip_prefix
                                    : fish_wcswidth(&o_line.text.at(0), shared_prefix);
            if (skip_width > skip_remaining) skip_remaining = skip_width;
        }

        if (!should_clear_screen_this_line) {
            // If we're soft wrapped, and if we're going to change the first character of the next
            // line, don't skip over the last two characters so that we maintain soft-wrapping.
            if (o_line.is_soft_wrapped && i + 1 < scr->desired.line_count()) {
                bool next_line_will_change = true;
                if (i + 1 < scr->actual.line_count()) {  //!OCLINT
                    if (line_shared_prefix(scr->desired.line(i + 1), scr->actual.line(i + 1)) > 0) {
                        next_line_will_change = false;
                    }
                }
                if (next_line_will_change) {
                    skip_remaining =
                        std::min(skip_remaining, static_cast<size_t>(scr->actual_width - 2));
                }
            }
        }

        // Skip over skip_remaining width worth of characters.
        size_t j = 0;
        for (; j < o_line.size(); j++) {
            size_t width = fish_wcwidth_min_0(o_line.char_at(j));
            if (skip_remaining < width) break;
            skip_remaining -= width;
            current_width += width;
        }

        // Skip over zero-width characters (e.g. combining marks at the end of the prompt).
        for (; j < o_line.size(); j++) {
            int width = fish_wcwidth_min_0(o_line.char_at(j));
            if (width > 0) break;
        }

        // Now actually output stuff.
        for (;; j++) {
            bool done = j >= o_line.size();
            // Clear the screen if we have not done so yet.
            // If we are about to output into the last column, clear the screen first. If we clear
            // the screen after we output into the last column, it can erase the last character due
            // to the sticky right cursor. If we clear the screen too early, we can defeat soft
            // wrapping.
            if (should_clear_screen_this_line && !has_cleared_screen &&
                (done || j + 1 == static_cast<size_t>(screen_width))) {
                s_set_color(scr, vars, highlight_spec_t{});
                s_move(scr, current_width, static_cast<int>(i));
                s_write_mbs(scr, clr_eos);
                has_cleared_screen = true;
            }
            if (done) break;

            perform_any_impending_soft_wrap(scr, current_width, static_cast<int>(i));
            s_move(scr, current_width, static_cast<int>(i));
            s_set_color(scr, vars, o_line.color_at(j));
            auto width = fish_wcwidth_min_0(o_line.char_at(j));
            s_write_char(scr, o_line.char_at(j), width);
            current_width += width;
        }

        bool clear_remainder = false;
        // Clear the remainder of the line if we need to clear and if we didn't write to the end of
        // the line. If we did write to the end of the line, the "sticky right edge" (as part of
        // auto_right_margin) means that we'll be clearing the last character we wrote!
        if (has_cleared_screen || has_cleared_line) {
            // Already cleared everything.
            clear_remainder = false;
        } else if (need_clear_lines && current_width < screen_width) {
            clear_remainder = true;
        } else if (right_prompt_width < scr->last_right_prompt_width) {
            clear_remainder = true;
        } else {
            // This wcswidth shows up strong in the profile.
            // Only do it if the previous line could conceivably be wider.
            // That means if it is a prefix of the current one we can skip it.
            if (s_line.text.size() != shared_prefix) {
                int prev_width = fish_wcswidth(&s_line.text.at(0), s_line.text.size());
                clear_remainder = prev_width > current_width;
            }
        }
        if (clear_remainder && clr_eol) {
            s_set_color(scr, vars, highlight_spec_t{});
            s_move(scr, current_width, static_cast<int>(i));
            s_write_mbs(scr, clr_eol);
        }

        // Output any rprompt if this is the first line.
        if (i == 0 && right_prompt_width > 0) {  //!OCLINT(Use early exit/continue)
            s_move(scr, static_cast<int>(screen_width - right_prompt_width), static_cast<int>(i));
            s_set_color(scr, vars, highlight_spec_t{});
            s_write_str(scr, right_prompt.c_str());
            scr->actual.cursor.x += right_prompt_width;

            // We output in the last column. Some terms (Linux) push the cursor further right, past
            // the window. Others make it "stick." Since we don't really know which is which, issue
            // a cr so it goes back to the left.
            //
            // However, if the user is resizing the window smaller, then it's possible the cursor
            // wrapped. If so, then a cr will go to the beginning of the following line! So instead
            // issue a bunch of "move left" commands to get back onto the line, and then jump to the
            // front of it.
            s_move(scr, scr->actual.cursor.x - static_cast<int>(right_prompt_width),
                   scr->actual.cursor.y);
            s_write_str(scr, L"\r");
            scr->actual.cursor.x = 0;
        }
    }

    // Clear remaining lines (if any) if we haven't cleared the screen.
    if (!has_cleared_screen && need_clear_screen && clr_eol) {
        s_set_color(scr, vars, highlight_spec_t{});
        for (size_t i = scr->desired.line_count(); i < lines_with_stuff; i++) {
            s_move(scr, 0, static_cast<int>(i));
            s_write_mbs(scr, clr_eol);
        }
    }

    s_move(scr, scr->desired.cursor.x, scr->desired.cursor.y);
    s_set_color(scr, vars, highlight_spec_t{});

    // We have now synced our actual screen against our desired screen. Note that this is a big
    // assignment!
    scr->actual = scr->desired;
    scr->last_right_prompt_width = right_prompt_width;
}

/// Returns true if we are using a dumb terminal.
static bool is_dumb() {
    if (!cur_term) return true;
    return !cursor_up || !cursor_down || !cursor_left || !cursor_right;
}

struct screen_layout_t {
    // The left prompt that we're going to use.
    wcstring left_prompt;
    // How much space to leave for it.
    size_t left_prompt_space;
    // The right prompt.
    wcstring right_prompt;
    // The autosuggestion.
    wcstring autosuggestion;
    // Whether the prompts get their own line or not.
    bool prompts_get_own_line;
};

// Given a vector whose indexes are offsets and whose values are the widths of the string if
// truncated at that offset, return the offset that fits in the given width. Returns
// width_by_offset.size() - 1 if they all fit. The first value in width_by_offset is assumed to be
// 0.
static size_t truncation_offset_for_width(const std::vector<size_t> &width_by_offset,
                                          size_t max_width) {
    assert(!width_by_offset.empty() && width_by_offset.at(0) == 0);
    size_t i;
    for (i = 1; i < width_by_offset.size(); i++) {
        if (width_by_offset.at(i) > max_width) break;
    }
    // i is the first index that did not fit; i-1 is therefore the last that did.
    return i - 1;
}

static screen_layout_t compute_layout(screen_t *s, size_t screen_width,
                                      const wcstring &left_prompt_str,
                                      const wcstring &right_prompt_str, const wcstring &commandline,
                                      const wcstring &autosuggestion_str) {
    UNUSED(s);
    screen_layout_t result = {};

    // Start by ensuring that the prompts themselves can fit.
    const wchar_t *left_prompt = left_prompt_str.c_str();
    const wchar_t *right_prompt = right_prompt_str.c_str();
    const wchar_t *autosuggestion = autosuggestion_str.c_str();

    prompt_layout_t left_prompt_layout = calc_prompt_layout(left_prompt_str, cached_layouts);
    prompt_layout_t right_prompt_layout = calc_prompt_layout(right_prompt_str, cached_layouts);

    size_t left_prompt_width = left_prompt_layout.last_line_width;
    size_t right_prompt_width = right_prompt_layout.last_line_width;

    if (left_prompt_layout.max_line_width > screen_width) {
        // If we have a multi-line prompt, see if the longest line fits; if not neuter the whole
        // left prompt.
        left_prompt = L"> ";
        left_prompt_width = 2;
    }

    if (left_prompt_width + right_prompt_width >= screen_width) {
        // Nix right_prompt.
        right_prompt = L"";
        right_prompt_width = 0;
    }

    if (left_prompt_width + right_prompt_width >= screen_width) {
        // Still doesn't fit, neuter left_prompt.
        left_prompt = L"> ";
        left_prompt_width = 2;
    }

    // Now we should definitely fit.
    assert(left_prompt_width + right_prompt_width < screen_width);

    // Get the width of the first line, and if there is more than one line.
    bool multiline = false;
    size_t first_line_width = 0;
    for (auto c : commandline) {
        if (c == L'\n') {
            multiline = true;
            break;
        } else {
            first_line_width += fish_wcwidth_min_0(c);
        }
    }
    const size_t first_command_line_width = first_line_width;

    // If we have more than one line, ensure we have no autosuggestion.
    size_t autosuggest_total_width = 0;
    std::vector<size_t> autosuggest_truncated_widths;
    if (multiline) {
        autosuggestion = L"";
    } else {
        autosuggest_truncated_widths.reserve(1 + autosuggestion_str.size());
        for (size_t i = 0; autosuggestion[i] != L'\0'; i++) {
            autosuggest_truncated_widths.push_back(autosuggest_total_width);
            autosuggest_total_width += fish_wcwidth_min_0(autosuggestion[i]);
        }
    }

    // Here are the layouts we try in turn:
    //
    // 1. Left prompt visible, right prompt visible, command line visible, autosuggestion visible.
    //
    // 2. Left prompt visible, right prompt visible, command line visible, autosuggestion truncated
    // (possibly to zero).
    //
    // 3. Left prompt visible, right prompt hidden, command line visible, autosuggestion hidden.
    //
    // 4. Newline separator (left prompt visible, right prompt hidden, command line visible,
    // autosuggestion visible).
    //
    // A remark about layout #4: if we've pushed the command line to a new line, why can't we draw
    // the right prompt? The issue is resizing: if you resize the window smaller, then the right
    // prompt will wrap to the next line. This means that we can't go back to the line that we were
    // on, and things turn to chaos very quickly.
    size_t calculated_width;
    bool done = false;

    // Case 1
    if (!done) {
        calculated_width = left_prompt_width + right_prompt_width + first_command_line_width +
                           autosuggest_total_width;
        if (calculated_width < screen_width) {
            result.left_prompt = left_prompt;
            result.left_prompt_space = left_prompt_width;
            result.right_prompt = right_prompt;
            result.autosuggestion = autosuggestion;
            done = true;
        }
    }

    // Case 2. Note that we require strict inequality so that there's always at least one space
    // between the left edge and the rprompt.
    if (!done) {
        calculated_width = left_prompt_width + right_prompt_width + first_command_line_width;
        if (calculated_width < screen_width) {
            result.left_prompt = left_prompt;
            result.left_prompt_space = left_prompt_width;
            result.right_prompt = right_prompt;

            // Need at least two characters to show an autosuggestion.
            size_t available_autosuggest_space =
                screen_width - (left_prompt_width + right_prompt_width + first_command_line_width);
            if (autosuggest_total_width > 0 && available_autosuggest_space > 2) {
                size_t truncation_offset = truncation_offset_for_width(
                    autosuggest_truncated_widths, available_autosuggest_space - 2);
                result.autosuggestion = wcstring(autosuggestion, truncation_offset);
                result.autosuggestion.push_back(get_ellipsis_char());
            }
            done = true;
        }
    }

    // Case 3
    if (!done) {
        calculated_width = left_prompt_width + first_command_line_width;
        if (calculated_width < screen_width) {
            result.left_prompt = left_prompt;
            result.left_prompt_space = left_prompt_width;
            done = true;
        }
    }

    // Case 4
    if (!done) {
        result.left_prompt = left_prompt;
        result.left_prompt_space = left_prompt_width;
        // See remark about for why we can't use the right prompt here result.right_prompt =
        // right_prompt. If the command wraps, and the prompt is not short, place the command on its
        // own line. A short prompt is 33% or less of the terminal's width.
        const size_t prompt_percent_width = (100 * left_prompt_width) / screen_width;
        if (left_prompt_width + first_command_line_width + 1 > screen_width &&
            prompt_percent_width > 33) {
            result.prompts_get_own_line = true;
        }
    }

    return result;
}

void s_write(screen_t *s, const wcstring &left_prompt, const wcstring &right_prompt,
             const wcstring &commandline, size_t explicit_len,
             const std::vector<highlight_spec_t> &colors, const std::vector<int> &indent,
             size_t cursor_pos, const page_rendering_t &pager, bool cursor_is_within_pager) {
    static relaxed_atomic_t<uint32_t> s_repaints{0};
    FLOGF(screen, "Repaint %u", static_cast<unsigned>(++s_repaints));
    screen_data_t::cursor_t cursor_arr;

    // Turn the command line into the explicit portion and the autosuggestion.
    const wcstring explicit_command_line = commandline.substr(0, explicit_len);
    const wcstring autosuggestion = commandline.substr(explicit_len);

    // If we are using a dumb terminal, don't try any fancy stuff, just print out the text.
    // right_prompt not supported.
    if (is_dumb()) {
        const std::string prompt_narrow = wcs2string(left_prompt);
        const std::string command_line_narrow = wcs2string(explicit_command_line);

        write_loop(STDOUT_FILENO, "\r", 1);
        write_loop(STDOUT_FILENO, prompt_narrow.c_str(), prompt_narrow.size());
        write_loop(STDOUT_FILENO, command_line_narrow.c_str(), command_line_narrow.size());

        return;
    }

    s_check_status(s);
    const size_t screen_width = common_get_width();

    // Completely ignore impossibly small screens.
    if (screen_width < 4) {
        return;
    }

    // Compute a layout.
    const screen_layout_t layout = compute_layout(s, screen_width, left_prompt, right_prompt,
                                                  explicit_command_line, autosuggestion);

    // Determine whether, if we have an autosuggestion, it was truncated.
    s->autosuggestion_is_truncated =
        !autosuggestion.empty() && autosuggestion != layout.autosuggestion;

    // Clear the desired screen.
    s->desired.resize(0);
    s->desired.cursor.x = s->desired.cursor.y = 0;

    // Append spaces for the left prompt.
    for (size_t i = 0; i < layout.left_prompt_space; i++) {
        s_desired_append_char(s, L' ', highlight_spec_t{}, 0, layout.left_prompt_space, 1);
    }

    // If overflowing, give the prompt its own line to improve the situation.
    size_t first_line_prompt_space = layout.left_prompt_space;
    if (layout.prompts_get_own_line) {
        s_desired_append_char(s, L'\n', highlight_spec_t{}, 0, 0, 0);
        first_line_prompt_space = 0;
    }

    // Reconstruct the command line.
    wcstring effective_commandline = explicit_command_line + layout.autosuggestion;

    // Output the command line.
    size_t i;
    for (i = 0; i < effective_commandline.size(); i++) {
        // Grab the current cursor's x,y position if this character matches the cursor's offset.
        if (!cursor_is_within_pager && i == cursor_pos) {
            cursor_arr = s->desired.cursor;
        }
        s_desired_append_char(s, effective_commandline.at(i), colors[i], indent[i],
                              first_line_prompt_space,
                              fish_wcwidth_min_0(effective_commandline.at(i)));
    }

    // Cursor may have been at the end too.
    if (!cursor_is_within_pager && i == cursor_pos) {
        cursor_arr = s->desired.cursor;
    }

    // Now that we've output everything, set the cursor to the position that we saved in the loop
    // above.
    s->desired.cursor = cursor_arr;

    if (cursor_is_within_pager) {
        s->desired.cursor.x = static_cast<int>(cursor_pos);
        s->desired.cursor.y = static_cast<int>(s->desired.line_count());
    }

    // Append pager_data (none if empty).
    s->desired.append_lines(pager.screen_data);

    s_update(s, layout.left_prompt, layout.right_prompt);
    s_save_status(s);
}
void s_reset(screen_t *s, screen_reset_mode_t mode) {
    assert(s && "Null screen");

    bool abandon_line = false, repaint_prompt = false, clear_to_eos = false;
    switch (mode) {
        case screen_reset_mode_t::current_line_contents: {
            break;
        }
        case screen_reset_mode_t::current_line_and_prompt: {
            repaint_prompt = true;
            break;
        }
        case screen_reset_mode_t::abandon_line: {
            abandon_line = true;
            repaint_prompt = true;
            break;
        }
        case screen_reset_mode_t::abandon_line_and_clear_to_end_of_screen: {
            abandon_line = true;
            repaint_prompt = true;
            clear_to_eos = true;
            break;
        }
    }

    // If we're abandoning the line, we must also be repainting the prompt.
    assert(!abandon_line || repaint_prompt);

    // If we are not abandoning the line, we need to remember how many lines we had output to, so we
    // can clear the remaining lines in the next call to s_update. This prevents leaving junk
    // underneath the cursor when resizing a window wider such that it reduces our desired line
    // count.
    if (!abandon_line) {
        s->actual_lines_before_reset =
            std::max(s->actual_lines_before_reset, s->actual.line_count());
    }

    if (repaint_prompt && !abandon_line) {
        // If the prompt is multi-line, we need to move up to the prompt's initial line. We do this
        // by lying to ourselves and claiming that we're really below what we consider "line 0"
        // (which is the last line of the prompt). This will cause us to move up to try to get back
        // to line 0, but really we're getting back to the initial line of the prompt.
        const size_t prompt_line_count = calc_prompt_lines(s->actual_left_prompt);
        assert(prompt_line_count >= 1);
        s->actual.cursor.y += (prompt_line_count - 1);
    } else if (abandon_line) {
        s->actual.cursor.y = 0;
    }

    if (repaint_prompt) s->actual_left_prompt.clear();
    s->actual.resize(0);
    s->need_clear_lines = true;
    s->need_clear_screen = s->need_clear_screen || clear_to_eos;

    if (abandon_line) {
        // Do the PROMPT_SP hack.
        int screen_width = common_get_width();
        wcstring abandon_line_string;
        abandon_line_string.reserve(screen_width + 32);  // should be enough

        // Don't need to check for fish_wcwidth errors; this is done when setting up
        // omitted_newline_char in common.cpp.
        int non_space_width = get_omitted_newline_width();
        // We do `>` rather than `>=` because the code below might require one extra space.
        if (screen_width > non_space_width) {
            bool justgrey = true;
            if (cur_term && enter_dim_mode) {
                std::string dim = tparm(const_cast<char *>(enter_dim_mode));
                if (!dim.empty()) {
                    // Use dim if they have it, so the color will be based on their actual normal
                    // color and the background of the termianl.
                    abandon_line_string.append(str2wcstring(dim));
                    justgrey = false;
                }
            }
            if (cur_term && justgrey && set_a_foreground) {
                if (max_colors >= 238) {
                    // draw the string in a particular grey
                    abandon_line_string.append(
                        str2wcstring(tparm(const_cast<char *>(set_a_foreground), 237)));
                } else if (max_colors >= 9) {
                    // bright black (the ninth color, looks grey)
                    abandon_line_string.append(
                        str2wcstring(tparm(const_cast<char *>(set_a_foreground), 8)));
                } else if (max_colors >= 2 && enter_bold_mode) {
                    // we might still get that color by setting black and going bold for bright
                    abandon_line_string.append(
                        str2wcstring(tparm(const_cast<char *>(enter_bold_mode))));
                    abandon_line_string.append(
                        str2wcstring(tparm(const_cast<char *>(set_a_foreground), 0)));
                }
            }

            abandon_line_string.append(get_omitted_newline_str());

            if (cur_term && exit_attribute_mode) {
                abandon_line_string.append(str2wcstring(tparm(
                    const_cast<char *>(exit_attribute_mode))));  // normal text ANSI escape sequence
            }

            int newline_glitch_width = term_has_xn ? 0 : 1;
            abandon_line_string.append(screen_width - non_space_width - newline_glitch_width, L' ');
        }

        abandon_line_string.push_back(L'\r');
        abandon_line_string.append(get_omitted_newline_str());
        // Now we are certainly on a new line. But we may have dropped the omitted newline char on
        // it. So append enough spaces to overwrite the omitted newline char, and then clear all the
        // spaces from the new line.
        abandon_line_string.append(non_space_width, L' ');
        abandon_line_string.push_back(L'\r');
        // Clear entire line. Zsh doesn't do this. Fish added this with commit 4417a6ee: If you have
        // a prompt preceded by a new line, you'll get a line full of spaces instead of an empty
        // line above your prompt. This doesn't make a difference in normal usage, but copying and
        // pasting your terminal log becomes a pain. This commit clears that line, making it an
        // actual empty line.
        if (!is_dumb() && clr_eol) {
            abandon_line_string.append(str2wcstring(clr_eol));
        }

        const std::string narrow_abandon_line_string = wcs2string(abandon_line_string);
        write_loop(STDOUT_FILENO, narrow_abandon_line_string.c_str(),
                   narrow_abandon_line_string.size());
        s->actual.cursor.x = 0;
    }

    if (!abandon_line) {
        // This should prevent resetting the cursor position during the next repaint.
        write_loop(STDOUT_FILENO, "\r", 1);
        s->actual.cursor.x = 0;
    }

    fstat(1, &s->prev_buff_1);
    fstat(2, &s->prev_buff_2);
}

void screen_force_clear_to_end() {
    if (clr_eos) {
        writembs(outputter_t::stdoutput(), clr_eos);
    }
}

screen_t::screen_t() : outp_(outputter_t::stdoutput()) {}

bool screen_t::cursor_is_wrapped_to_own_line() const {
    // Note == comparison against the line count is correct: we do not create a line just for the
    // cursor. If there is a line containing the cursor, then it means that line has contents and we
    // should return false.
    // Don't consider dumb terminals to have wrapping for the purposes of this function.
    return actual.cursor.x == 0 && static_cast<size_t>(actual.cursor.y) == actual.line_count() &&
           !is_dumb();
}
