/** \file screen.c High level library for handling the terminal screen

The screen library allows the interactive reader to write its
output to screen efficiently by keeping an internal representation
of the current screen contents and trying to find the most
efficient way for transforming that to the desired screen content.
*/
// IWYU pragma: no_include <cstddef>
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#if HAVE_NCURSES_H
#include <ncurses.h>
#elif HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif
#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif
#include <wchar.h>
#include <time.h>
#include <assert.h>
#include <algorithm>
#include <string>
#include <vector>

#include "fallback.h"  // IWYU pragma: keep
#include "common.h"
#include "util.h"
#include "output.h"
#include "highlight.h"
#include "screen.h"
#include "env.h"
#include "pager.h"

/** The number of characters to indent new blocks */
#define INDENT_STEP 4u

/** The initial screen width */
#define SCREEN_WIDTH_UNINITIALIZED -1

/** A helper value for an invalid location */
#define INVALID_LOCATION (screen_data_t::cursor_t(-1, -1))

static void invalidate_soft_wrap(screen_t *scr);

/**
   Ugly kludge. The internal buffer used to store output of
   tputs. Since tputs external function can only take an integer and
   not a pointer as parameter we need a static storage buffer.
*/
typedef std::vector<char> data_buffer_t;
static data_buffer_t *s_writeb_buffer=0;

static int s_writeb(char c);

/* Class to temporarily set s_writeb_buffer and the writer function in a scoped way */
class scoped_buffer_t
{
    data_buffer_t * const old_buff;
    int (* const old_writer)(char);

public:
    explicit scoped_buffer_t(data_buffer_t *buff) : old_buff(s_writeb_buffer), old_writer(output_get_writer())
    {
        s_writeb_buffer = buff;
        output_set_writer(s_writeb);
    }

    ~scoped_buffer_t()
    {
        s_writeb_buffer = old_buff;
        output_set_writer(old_writer);
    }
};

/**
   Tests if the specified narrow character sequence is present at the
   specified position of the specified wide character string. All of
   \c seq must match, but str may be longer than seq.
*/
static size_t try_sequence(const char *seq, const wchar_t *str)
{
    for (size_t i=0; ; i++)
    {
        if (!seq[i])
            return i;

        if (seq[i] != str[i])
            return 0;
    }

    return 0;
}

/**
   Returns the number of columns left until the next tab stop, given
   the current cursor postion.
 */
static size_t next_tab_stop(size_t in)
{
    /*
      Assume tab stops every 8 characters if undefined
    */
    size_t tab_width = (init_tabs > 0 ? (size_t)init_tabs : 8);
    return ((in/tab_width)+1)*tab_width;
}

/* Like fish_wcwidth, but returns 0 for control characters instead of -1 */
static int fish_wcwidth_min_0(wchar_t wc)
{
    return maxi(0, fish_wcwidth(wc));
}

/* Whether we permit soft wrapping. If so, in some cases we don't explicitly move to the second physical line on a wrapped logical line; instead we just output it. */
static bool allow_soft_wrap(void)
{
    // Should we be looking at eat_newline_glitch as well?
    return !! auto_right_margin;
}


/* Returns the number of characters in the escape code starting at 'code' (which should initially contain \x1b) */
size_t escape_code_length(const wchar_t *code)
{
    assert(code != NULL);

    /* The only escape codes we recognize start with \x1b */
    if (code[0] != L'\x1b')
        return 0;

    size_t resulting_length = 0;
    bool found = false;

    if (cur_term != NULL)
    {
        /*
         Detect these terminfo color escapes with parameter
         value 0..7, all of which don't move the cursor
         */
        char * const esc[] =
        {
            set_a_foreground,
            set_a_background,
            set_foreground,
            set_background,
        };

        for (size_t p=0; p < sizeof esc / sizeof *esc && !found; p++)
        {
            if (!esc[p])
                continue;

            for (size_t k=0; k<8; k++)
            {
                size_t len = try_sequence(tparm(esc[p],k), code);
                if (len)
                {
                    resulting_length = len;
                    found = true;
                    break;
                }
            }
        }
    }

    if (cur_term != NULL)
    {
        /*
         Detect these semi-common terminfo escapes without any
         parameter values, all of which don't move the cursor
         */
        char * const esc2[] =
        {
            enter_bold_mode,
            exit_attribute_mode,
            enter_underline_mode,
            exit_underline_mode,
            enter_standout_mode,
            exit_standout_mode,
            flash_screen,
            enter_subscript_mode,
            exit_subscript_mode,
            enter_superscript_mode,
            exit_superscript_mode,
            enter_blink_mode,
            enter_italics_mode,
            exit_italics_mode,
            enter_reverse_mode,
            enter_shadow_mode,
            exit_shadow_mode,
            enter_standout_mode,
            exit_standout_mode,
            enter_secure_mode
        };



        for (size_t p=0; p < sizeof esc2 / sizeof *esc2 && !found; p++)
        {
            if (!esc2[p])
                continue;
            /*
             Test both padded and unpadded version, just to
             be safe. Most versions of tparm don't actually
             seem to do anything these days.
             */
            size_t len = maxi(try_sequence(tparm(esc2[p]), code), try_sequence(esc2[p], code));
            if (len)
            {
                resulting_length = len;
                found = true;
            }
        }
    }

    if (!found)
    {
        if (code[1] == L'k')
        {
            /* This looks like the escape sequence for setting a screen name */
            const env_var_t term_name = env_get_string(L"TERM");
            if (!term_name.missing() && string_prefixes_string(L"screen", term_name))
            {
                const wchar_t * const screen_name_end_sentinel = L"\x1b\\";
                const wchar_t *screen_name_end = wcsstr(&code[2], screen_name_end_sentinel);
                if (screen_name_end != NULL)
                {
                    const wchar_t *escape_sequence_end = screen_name_end + wcslen(screen_name_end_sentinel);
                    resulting_length = escape_sequence_end - code;
                }
                else
                {
                    /* Consider just <esc>k to be the code */
                    resulting_length = 2;
                }
                found = true;
            }
        }
    }
    
    if (! found)
    {
        /* iTerm2 escape codes: CSI followed by ], terminated by either BEL or escape + backslash. See https://code.google.com/p/iterm2/wiki/ProprietaryEscapeCodes */
        if (code[1] == ']')
        {
            // Start at 2 to skip over <esc>]
            size_t cursor = 2;
            for (; code[cursor] != L'\0'; cursor++)
            {
                /* Consume a sequence of characters up to <esc>\ or <bel> */
                if (code[cursor] == '\x07' || (code[cursor] == '\\' && code[cursor - 1] == '\x1b'))
                {
                    found = true;
                    break;
                }
            }
            if (found)
            {
                resulting_length = cursor + 1;
            }
        }
    }

    if (! found)
    {
        /* Generic VT100 one byte sequence: CSI followed by something in the range @ through _ */
        if (code[1] == L'[' && (code[2] >= L'@' && code[2] <= L'_'))
        {
            resulting_length = 3;
            found = true;
        }
    }

    if (! found)
    {
        /* Generic VT100 CSI-style sequence. <esc>, followed by zero or more ASCII characters NOT in the range [@,_], followed by one character in that range */
        if (code[1] == L'[')
        {
            // Start at 2 to skip over <esc>[
            size_t cursor = 2;
            for (; code[cursor] != L'\0'; cursor++)
            {
                /* Consume a sequence of ASCII characters not in the range [@, ~] */
                wchar_t c = code[cursor];

                /* If we're not in ASCII, just stop */
                if (c > 127)
                    break;

                /* If we're the end character, then consume it and then stop */
                if (c >= L'@' && c <= L'~')
                {
                    cursor++;
                    break;
                }
            }
            /* curs now indexes just beyond the end of the sequence (or at the terminating zero) */
            found = true;
            resulting_length = cursor;
        }
    }
    if (! found)
    {
        /* Generic VT100 two byte sequence: <esc> followed by something in the range @ through _ */
        if (code[1] >= L'@' && code[1] <= L'_')
        {
            resulting_length = 2;
            found = true;
        }
    }

    return resulting_length;
}

/* Information about a prompt layout */
struct prompt_layout_t
{
    /* How many lines the prompt consumes */
    size_t line_count;

    /* Width of the longest line */
    size_t max_line_width;

    /* Width of the last line */
    size_t last_line_width;
};

/**
   Calculate layout information for the given prompt. Does some clever magic
   to detect common escape sequences that may be embeded in a prompt,
   such as color codes.
*/
static prompt_layout_t calc_prompt_layout(const wchar_t *prompt)
{
    size_t current_line_width = 0;
    size_t j;

    prompt_layout_t prompt_layout = {};
    prompt_layout.line_count = 1;

    for (j=0; prompt[j]; j++)
    {
        if (prompt[j] == L'\x1b')
        {
            /* This is the start of an escape code. Skip over it if it's at least one character long. */
            size_t escape_len = escape_code_length(&prompt[j]);
            if (escape_len > 0)
            {
                j += escape_len - 1;
            }
        }
        else if (prompt[j] == L'\t')
        {
            current_line_width = next_tab_stop(current_line_width);
        }
        else if (prompt[j] == L'\n' || prompt[j] == L'\f')
        {
            /* PCA: At least one prompt uses \f\r as a newline. It's unclear to me what this is meant to do, but terminals seem to treat it as a newline so we do the same. */
            current_line_width = 0;
            prompt_layout.line_count += 1;
        }
        else if (prompt[j] == L'\r')
        {
            current_line_width = 0;
        }
        else
        {
            /* Ordinary decent character. Just add width. This returns -1 for a control character - don't add that. */
            current_line_width += fish_wcwidth_min_0(prompt[j]);
            prompt_layout.max_line_width = maxi(prompt_layout.max_line_width, current_line_width);
        }
    }
    prompt_layout.last_line_width = current_line_width;
    return prompt_layout;
}

static size_t calc_prompt_lines(const wcstring &prompt)
{
    // Hack for the common case where there's no newline at all
    // I don't know if a newline can appear in an escape sequence,
    // so if we detect a newline we have to defer to calc_prompt_width_and_lines
    size_t result = 1;
    if (prompt.find(L'\n') != wcstring::npos || prompt.find(L'\f') != wcstring::npos)
    {
        result = calc_prompt_layout(prompt.c_str()).line_count;
    }
    return result;
}

/**
   Stat stdout and stderr and save result.

   This should be done before calling a function that may cause output.
*/

static void s_save_status(screen_t *s)
{

    // PCA Let's not do this futimes stuff, because sudo dumbly uses the
    // tty's ctime as part of its tty_tickets feature
    // Disabling this should fix https://github.com/fish-shell/fish-shell/issues/122
#if 0
    /*
      This futimes call tries to trick the system into using st_mtime
      as a tampering flag. This of course only works on systems where
      futimes is defined, but it should make the status saving stuff
      failsafe.
    */
    struct timeval t[]=
    {
        {
            time(0)-1,
            0
        }
        ,
        {
            time(0)-1,
            0
        }
    }
    ;

    /*
      Don't check return value on these. We don't care if they fail,
      really.  This is all just to make the prompt look ok, which is
      impossible to do 100% reliably. We try, at least.
    */
    futimes(1, t);
    futimes(2, t);
#endif

    fstat(1, &s->prev_buff_1);
    fstat(2, &s->prev_buff_2);
}

/**
   Stat stdout and stderr and compare result to previous result in
   reader_save_status. Repaint if modification time has changed.

   Unfortunately, for some reason this call seems to give a lot of
   false positives, at least under Linux.
*/

static void s_check_status(screen_t *s)
{
    fflush(stdout);
    fflush(stderr);

    fstat(1, &s->post_buff_1);
    fstat(2, &s->post_buff_2);

    int changed = (s->prev_buff_1.st_mtime != s->post_buff_1.st_mtime) ||
                  (s->prev_buff_2.st_mtime != s->post_buff_2.st_mtime);

    #if defined HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC
        changed = changed || s->prev_buff_1.st_mtimespec.tv_nsec != s->post_buff_1.st_mtimespec.tv_nsec ||
                    s->prev_buff_2.st_mtimespec.tv_nsec != s->post_buff_2.st_mtimespec.tv_nsec;
    #elif defined HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
        changed = changed || s->prev_buff_1.st_mtim.tv_nsec != s->post_buff_1.st_mtim.tv_nsec ||
                    s->prev_buff_2.st_mtim.tv_nsec != s->post_buff_2.st_mtim.tv_nsec;
    #endif

    if (changed)
    {
        /*
          Ok, someone has been messing with our screen. We will want
          to repaint. However, we do not know where the cursor is. It
          is our best bet that we are still on the same line, so we
          move to the beginning of the line, reset the modelled screen
          contents, and then set the modeled cursor y-pos to its
          earlier value.
        */

        int prev_line = s->actual.cursor.y;
        write_loop(STDOUT_FILENO, "\r", 1);
        s_reset(s, screen_reset_current_line_and_prompt);
        s->actual.cursor.y = prev_line;
    }
}

/**
   Appends a character to the end of the line that the output cursor is
   on. This function automatically handles linebreaks and lines longer
   than the screen width.
*/
static void s_desired_append_char(screen_t *s,
                                  wchar_t b,
                                  int c,
                                  int indent,
                                  size_t prompt_width)
{
    int line_no = s->desired.cursor.y;

    switch (b)
    {
        case L'\n':
        {
            int i;
            /* Current line is definitely hard wrapped */
            s->desired.create_line(s->desired.line_count());
            s->desired.line(s->desired.cursor.y).is_soft_wrapped = false;
            s->desired.cursor.y++;
            s->desired.cursor.x=0;
            for (i=0; i < prompt_width+indent*INDENT_STEP; i++)
            {
                s_desired_append_char(s, L' ', 0, indent, prompt_width);
            }
            break;
        }

        case L'\r':
        {
            line_t &current = s->desired.line(line_no);
            current.clear();
            s->desired.cursor.x = 0;
            break;
        }

        default:
        {
            int screen_width = common_get_width();
            int cw = fish_wcwidth_min_0(b);

            s->desired.create_line(line_no);

            /*
                   Check if we are at the end of the line. If so, continue on the next line.
                   */
            if ((s->desired.cursor.x + cw) > screen_width)
            {
                /* Current line is soft wrapped (assuming we support it) */
                s->desired.line(s->desired.cursor.y).is_soft_wrapped = true;
                //fprintf(stderr, "\n\n1 Soft wrapping %d\n\n", s->desired.cursor.y);

                line_no = (int)s->desired.line_count();
                s->desired.add_line();
                s->desired.cursor.y++;
                s->desired.cursor.x=0;
            }

            line_t &line = s->desired.line(line_no);
            line.append(b, c);
            s->desired.cursor.x+= cw;

            /* Maybe wrap the cursor to the next line, even if the line itself did not wrap. This avoids wonkiness in the last column. */
            if (s->desired.cursor.x >= screen_width)
            {
                line.is_soft_wrapped = true;
                s->desired.cursor.x = 0;
                s->desired.cursor.y++;
            }
            break;
        }
    }

}

/**
   The writeb function offered to tputs.
*/
static int s_writeb(char c)
{
    s_writeb_buffer->push_back(c);
    return 0;
}

/**
   Write the bytes needed to move screen cursor to the specified
   position to the specified buffer. The actual_cursor field of the
   specified screen_t will be updated.

   \param s the screen to operate on
   \param b the buffer to send the output escape codes to
   \param new_x the new x position
   \param new_y the new y position
*/
static void s_move(screen_t *s, data_buffer_t *b, int new_x, int new_y)
{
    if (s->actual.cursor.x == new_x && s->actual.cursor.y == new_y)
        return;

    // If we are at the end of our window, then either the cursor stuck to the edge or it didn't. We don't know! We can fix it up though.
    if (s->actual.cursor.x == common_get_width())
    {
        // Either issue a cr to go back to the beginning of this line, or a nl to go to the beginning of the next one, depending on what we think is more efficient
        if (new_y <= s->actual.cursor.y)
        {
            b->push_back('\r');
        }
        else
        {
            b->push_back('\n');
            s->actual.cursor.y++;
        }
        // Either way we're not in the first column
        s->actual.cursor.x = 0;
    }

    int i;
    int x_steps, y_steps;

    char *str;
    /*
      debug( 0, L"move from %d %d to %d %d",
      s->screen_cursor[0], s->screen_cursor[1],
      new_x, new_y );
    */
    scoped_buffer_t scoped_buffer(b);

    y_steps = new_y - s->actual.cursor.y;

    if (y_steps > 0 && (strcmp(cursor_down, "\n")==0))
    {
        /*
          This is very strange - it seems some (all?) consoles use a
          simple newline as the cursor down escape. This will of
          course move the cursor to the beginning of the line as well
          as moving it down one step. The cursor_up does not have this
          behaviour...
        */
        s->actual.cursor.x=0;
    }

    if (y_steps < 0)
    {
        str = cursor_up;
    }
    else
    {
        str = cursor_down;

    }

    for (i=0; i<abs(y_steps); i++)
    {
        writembs(str);
    }


    x_steps = new_x - s->actual.cursor.x;

    if (x_steps && new_x == 0)
    {
        b->push_back('\r');
        x_steps = 0;
    }

    char *multi_str = NULL;
    if (x_steps < 0)
    {
        str = cursor_left;
        multi_str = parm_left_cursor;
    }
    else
    {
        str = cursor_right;
        multi_str = parm_right_cursor;
    }
    
    // Use the bulk ('multi') output for cursor movement if it is supported and it would be shorter
    // Note that this is required to avoid some visual glitches in iTerm (#1448)
    bool use_multi = (multi_str != NULL && multi_str[0] != '\0' && abs(x_steps) * strlen(str) > strlen(multi_str));
    if (use_multi)
    {
        char *multi_param = tparm(multi_str, abs(x_steps));
        writembs(multi_param);
    }
    else
    {
        for (i=0; i<abs(x_steps); i++)
        {
            writembs(str);
        }
    }


    s->actual.cursor.x = new_x;
    s->actual.cursor.y = new_y;
}

/**
   Set the pen color for the terminal
*/
static void s_set_color(screen_t *s, data_buffer_t *b, highlight_spec_t c)
{
    scoped_buffer_t scoped_buffer(b);

    unsigned int uc = (unsigned int)c;
    set_color(highlight_get_color(uc & 0xffff, false),
              highlight_get_color((uc>>16)&0xffff, true));
}

/**
   Convert a wide character to a multibyte string and append it to the
   buffer.
*/
static void s_write_char(screen_t *s, data_buffer_t *b, wchar_t c)
{
    scoped_buffer_t scoped_buffer(b);
    s->actual.cursor.x += fish_wcwidth_min_0(c);
    writech(c);
    if (s->actual.cursor.x == s->actual_width && allow_soft_wrap())
    {
        s->soft_wrap_location.x = 0;
        s->soft_wrap_location.y = s->actual.cursor.y + 1;

        /* Note that our cursor position may be a lie: Apple Terminal makes the right cursor stick to the margin, while Ubuntu makes it "go off the end" (but still doesn't wrap). We rely on s_move to fix this up. */
    }
    else
    {
        invalidate_soft_wrap(s);
    }
}

/**
   Send the specified string through tputs and append the output to
   the specified buffer.
*/
static void s_write_mbs(data_buffer_t *b, char *s)
{
    scoped_buffer_t scoped_buffer(b);
    writembs(s);
}

/**
   Convert a wide string to a multibyte string and append it to the
   buffer.
*/
static void s_write_str(data_buffer_t *b, const wchar_t *s)
{
    scoped_buffer_t scoped_buffer(b);
    writestr(s);
}

/** Returns the length of the "shared prefix" of the two lines, which is the run of matching text and colors.
    If the prefix ends on a combining character, do not include the previous character in the prefix.
*/
static size_t line_shared_prefix(const line_t &a, const line_t &b)
{
    size_t idx, max = std::min(a.size(), b.size());
    for (idx=0; idx < max; idx++)
    {
        wchar_t ac = a.char_at(idx), bc = b.char_at(idx);
        if (fish_wcwidth(ac) < 1 || fish_wcwidth(bc) < 1)
        {
            /* Possible combining mark, return one index prior */
            if (idx > 0) idx--;
            break;
        }

        /* We're done if the text or colors are different */
        if (ac != bc || a.color_at(idx) != b.color_at(idx))
            break;
    }
    return idx;
}

/* We are about to output one or more characters onto the screen at the given x, y. If we are at the end of previous line, and the previous line is marked as soft wrapping, then tweak the screen so we believe we are already in the target position. This lets the terminal take care of wrapping, which means that if you copy and paste the text, it won't have an embedded newline.  */
static bool perform_any_impending_soft_wrap(screen_t *scr, int x, int y)
{
    if (x == scr->soft_wrap_location.x && y == scr->soft_wrap_location.y)
    {
        /* We can soft wrap; but do we want to? */
        if (scr->desired.line(y - 1).is_soft_wrapped && allow_soft_wrap())
        {
            /* Yes. Just update the actual cursor; that will cause us to elide emitting the commands to move here, so we will just output on "one big line" (which the terminal soft wraps */
            scr->actual.cursor = scr->soft_wrap_location;
        }
    }
    return false;
}

/* Make sure we don't soft wrap */
static void invalidate_soft_wrap(screen_t *scr)
{
    scr->soft_wrap_location = INVALID_LOCATION;
}

/* Various code for testing term behavior */
#if 0
static bool test_stuff(screen_t *scr)
{
    data_buffer_t output;
    scoped_buffer_t scoped_buffer(&output);

    s_move(scr, &output, 0, 0);
    int screen_width = common_get_width();

    const wchar_t *left = L"left";
    const wchar_t *right = L"right";

    for (size_t idx = 0; idx < 80; idx++)
    {
        output.push_back('A');
    }

    if (! output.empty())
    {
        write_loop(STDOUT_FILENO, &output.at(0), output.size());
        output.clear();
    }

    sleep(5);

    for (size_t i=0; i < 1; i++)
    {
        writembs(cursor_left);
    }

    if (! output.empty())
    {
        write_loop(1, &output.at(0), output.size());
        output.clear();
    }



    while (1)
    {
        int c = getchar();
        if (c != EOF) break;
    }


    while (1)
    {
        int c = getchar();
        if (c != EOF) break;
    }
    puts("Bye");
    exit(0);
    while (1) sleep(10000);
    return true;
}
#endif

/**
   Update the screen to match the desired output.
*/
static void s_update(screen_t *scr, const wchar_t *left_prompt, const wchar_t *right_prompt)
{
    //if (test_stuff(scr)) return;
    const size_t left_prompt_width = calc_prompt_layout(left_prompt).last_line_width;
    const size_t right_prompt_width = calc_prompt_layout(right_prompt).last_line_width;

    int screen_width = common_get_width();

    /* Figure out how many following lines we need to clear (probably 0) */
    size_t actual_lines_before_reset = scr->actual_lines_before_reset;
    scr->actual_lines_before_reset = 0;

    data_buffer_t output;

    bool need_clear_lines = scr->need_clear_lines;
    bool need_clear_screen = scr->need_clear_screen;
    bool has_cleared_screen = false;

    if (scr->actual_width != screen_width)
    {
        /* Ensure we don't issue a clear screen for the very first output, to avoid https://github.com/fish-shell/fish-shell/issues/402 */
        if (scr->actual_width != SCREEN_WIDTH_UNINITIALIZED)
        {
            need_clear_screen = true;
            s_move(scr, &output, 0, 0);
            s_reset(scr, screen_reset_current_line_contents);

            need_clear_lines = need_clear_lines || scr->need_clear_lines;
            need_clear_screen = need_clear_screen || scr->need_clear_screen;
        }
        scr->actual_width = screen_width;
    }

    scr->need_clear_lines = false;
    scr->need_clear_screen = false;

    /* Determine how many lines have stuff on them; we need to clear lines with stuff that we don't want */
    const size_t lines_with_stuff = maxi(actual_lines_before_reset, scr->actual.line_count());
    if (lines_with_stuff > scr->desired.line_count())
    {
        /* There are lines that we output to previously that will need to be cleared */
        //need_clear_lines = true;
    }

    if (wcscmp(left_prompt, scr->actual_left_prompt.c_str()))
    {
        s_move(scr, &output, 0, 0);
        s_write_str(&output, left_prompt);
        scr->actual_left_prompt = left_prompt;
        scr->actual.cursor.x = (int)left_prompt_width;
    }

    for (size_t i=0; i < scr->desired.line_count(); i++)
    {
        const line_t &o_line = scr->desired.line(i);
        line_t &s_line = scr->actual.create_line(i);
        size_t start_pos = (i==0 ? left_prompt_width : 0);
        int current_width = 0;

        /* If this is the last line, maybe we should clear the screen */
        const bool should_clear_screen_this_line = (need_clear_screen && i + 1 == scr->desired.line_count() && clr_eos != NULL);

        /* Note that skip_remaining is a width, not a character count */
        size_t skip_remaining = start_pos;

        if (! should_clear_screen_this_line)
        {
            /* Compute how much we should skip. At a minimum we skip over the prompt. But also skip over the shared prefix of what we want to output now, and what we output before, to avoid repeatedly outputting it. */
            const size_t shared_prefix = line_shared_prefix(o_line, s_line);
            if (shared_prefix > 0)
            {
                int prefix_width = fish_wcswidth(&o_line.text.at(0), shared_prefix);
                if (prefix_width > skip_remaining)
                    skip_remaining = prefix_width;
            }

            /* If we're soft wrapped, and if we're going to change the first character of the next line, don't skip over the last two characters so that we maintain soft-wrapping */
            if (o_line.is_soft_wrapped && i + 1 < scr->desired.line_count())
            {
                bool first_character_of_next_line_will_change = true;
                if (i + 1 < scr->actual.line_count())
                {
                    if (line_shared_prefix(scr->desired.line(i+1), scr->actual.line(i+1)) > 0)
                    {
                        first_character_of_next_line_will_change = false;
                    }
                }
                if (first_character_of_next_line_will_change)
                {
                    skip_remaining = mini(skip_remaining, (size_t)(scr->actual_width - 2));
                }
            }
        }

        /* Skip over skip_remaining width worth of characters */
        size_t j = 0;
        for (; j < o_line.size(); j++)
        {
            int width = fish_wcwidth_min_0(o_line.char_at(j));
            if (skip_remaining < width)
                break;
            skip_remaining -= width;
            current_width += width;
        }

        /* Skip over zero-width characters (e.g. combining marks at the end of the prompt) */
        for (; j < o_line.size(); j++)
        {
            int width = fish_wcwidth_min_0(o_line.char_at(j));
            if (width > 0)
                break;
        }

        /* Now actually output stuff */
        for (; j < o_line.size(); j++)
        {
            /* If we are about to output into the last column, clear the screen first. If we clear the screen after we output into the last column, it can erase the last character due to the sticky right cursor. If we clear the screen too early, we can defeat soft wrapping. */
            if (j + 1 == screen_width && should_clear_screen_this_line && ! has_cleared_screen)
            {
                s_move(scr, &output, current_width, (int)i);
                s_write_mbs(&output, clr_eos);
                has_cleared_screen = true;
            }

            perform_any_impending_soft_wrap(scr, current_width, (int)i);
            s_move(scr, &output, current_width, (int)i);
            s_set_color(scr, &output, o_line.color_at(j));
            s_write_char(scr, &output, o_line.char_at(j));
            current_width += fish_wcwidth_min_0(o_line.char_at(j));
        }

        /* Clear the screen if we have not done so yet. */
        if (should_clear_screen_this_line && ! has_cleared_screen)
        {
            s_move(scr, &output, current_width, (int)i);
            s_write_mbs(&output, clr_eos);
            has_cleared_screen = true;
        }

        bool clear_remainder = false;
        /* Clear the remainder of the line if we need to clear and if we didn't write to the end of the line. If we did write to the end of the line, the "sticky right edge" (as part of auto_right_margin) means that we'll be clearing the last character we wrote! */
        if (has_cleared_screen)
        {
            /* Already cleared everything */
            clear_remainder = false;
        }
        else if (need_clear_lines && current_width < screen_width)
        {
            clear_remainder = true;
        }
        else if (right_prompt_width < scr->last_right_prompt_width)
        {
            clear_remainder = true;
        }
        else
        {
            int prev_width = (s_line.text.empty() ? 0 : fish_wcswidth(&s_line.text.at(0), s_line.text.size()));
            clear_remainder = prev_width > current_width;

        }
        if (clear_remainder)
        {
            s_set_color(scr, &output, 0xffffffff);
            s_move(scr, &output, current_width, (int)i);
            s_write_mbs(&output, clr_eol);
        }

        /* Output any rprompt if this is the first line. */
        if (i == 0 && right_prompt_width > 0)
        {
            s_move(scr, &output, (int)(screen_width - right_prompt_width), (int)i);
            s_set_color(scr, &output, 0xffffffff);
            s_write_str(&output, right_prompt);
            scr->actual.cursor.x += right_prompt_width;

            /* We output in the last column. Some terms (Linux) push the cursor further right, past the window. Others make it "stick." Since we don't really know which is which, issue a cr so it goes back to the left.

               However, if the user is resizing the window smaller, then it's possible the cursor wrapped. If so, then a cr will go to the beginning of the following line! So instead issue a bunch of "move left" commands to get back onto the line, and then jump to the front of it (!)
            */

            s_move(scr, &output, scr->actual.cursor.x - (int)right_prompt_width, scr->actual.cursor.y);
            s_write_str(&output, L"\r");
            scr->actual.cursor.x = 0;
        }
    }


    /* Clear remaining lines (if any) if we haven't cleared the screen. */
    if (! has_cleared_screen && scr->desired.line_count() < lines_with_stuff)
    {
        s_set_color(scr, &output, 0xffffffff);
        for (size_t i=scr->desired.line_count(); i < lines_with_stuff; i++)
        {
            s_move(scr, &output, 0, (int)i);
            s_write_mbs(&output, clr_eol);
        }
    }

    s_move(scr, &output, scr->desired.cursor.x, scr->desired.cursor.y);
    s_set_color(scr, &output, 0xffffffff);

    if (! output.empty())
    {
        write_loop(STDOUT_FILENO, &output.at(0), output.size());
    }

    /* We have now synced our actual screen against our desired screen. Note that this is a big assignment! */
    scr->actual = scr->desired;
    scr->last_right_prompt_width = right_prompt_width;
}

/** Returns true if we are using a dumb terminal. */
static bool is_dumb(void)
{
    return (!cursor_up || !cursor_down || !cursor_left || !cursor_right);
}

struct screen_layout_t
{
    /* The left prompt that we're going to use */
    wcstring left_prompt;

    /* How much space to leave for it */
    size_t left_prompt_space;

    /* The right prompt */
    wcstring right_prompt;

    /* The autosuggestion */
    wcstring autosuggestion;

    /* Whether the prompts get their own line or not */
    bool prompts_get_own_line;
};

/* Given a vector whose indexes are offsets and whose values are the widths of the string if truncated at that offset, return the offset that fits in the given width. Returns width_by_offset.size() - 1 if they all fit. The first value in width_by_offset is assumed to be 0. */
static size_t truncation_offset_for_width(const std::vector<size_t> &width_by_offset, size_t max_width)
{
    assert(! width_by_offset.empty() && width_by_offset.at(0) == 0);
    size_t i;
    for (i=1; i < width_by_offset.size(); i++)
    {
        if (width_by_offset.at(i) > max_width)
            break;
    }
    /* i is the first index that did not fit; i-1 is therefore the last that did */
    return i - 1;
}

static screen_layout_t compute_layout(screen_t *s,
                                      size_t screen_width,
                                      const wcstring &left_prompt_str,
                                      const wcstring &right_prompt_str,
                                      const wcstring &commandline,
                                      const wcstring &autosuggestion_str,
                                      const int *indent)
{
    screen_layout_t result = {};

    /* Start by ensuring that the prompts themselves can fit */
    const wchar_t *left_prompt = left_prompt_str.c_str();
    const wchar_t *right_prompt = right_prompt_str.c_str();
    const wchar_t *autosuggestion = autosuggestion_str.c_str();

    prompt_layout_t left_prompt_layout = calc_prompt_layout(left_prompt);
    prompt_layout_t right_prompt_layout = calc_prompt_layout(right_prompt);

    size_t left_prompt_width = left_prompt_layout.last_line_width;
    size_t right_prompt_width = right_prompt_layout.last_line_width;

    if (left_prompt_layout.max_line_width > screen_width)
    {
        /* If we have a multi-line prompt, see if the longest line fits; if not neuter the whole left prompt */
        left_prompt = L"> ";
        left_prompt_width = 2;
    }

    if (left_prompt_width + right_prompt_width >= screen_width)
    {
        /* Nix right_prompt */
        right_prompt = L"";
        right_prompt_width = 0;
    }

    if (left_prompt_width + right_prompt_width >= screen_width)
    {
        /* Still doesn't fit, neuter left_prompt */
        left_prompt = L"> ";
        left_prompt_width = 2;
    }

    /* Now we should definitely fit */
    assert(left_prompt_width + right_prompt_width < screen_width);


    /* Convert commandline to a list of lines and their widths */
    wcstring_list_t command_lines(1);
    std::vector<size_t> line_widths(1);
    for (size_t i=0; i < commandline.size(); i++)
    {
        wchar_t c = commandline.at(i);
        if (c == L'\n')
        {
            /* Make a new line */
            command_lines.push_back(wcstring());
            line_widths.push_back(indent[i]*INDENT_STEP);
        }
        else
        {
            command_lines.back() += c;
            line_widths.back() += fish_wcwidth_min_0(c);
        }
    }
    const size_t first_command_line_width = line_widths.at(0);

    /* If we have more than one line, ensure we have no autosuggestion */
    if (command_lines.size() > 1)
    {
        autosuggestion = L"";
    }

    /* Compute the width of the autosuggestion at all possible truncation offsets */
    std::vector<size_t> autosuggestion_truncated_widths;
    autosuggestion_truncated_widths.reserve(1 + wcslen(autosuggestion));
    size_t autosuggestion_total_width = 0;
    for (size_t i=0; autosuggestion[i] != L'\0'; i++)
    {
        autosuggestion_truncated_widths.push_back(autosuggestion_total_width);
        autosuggestion_total_width += fish_wcwidth_min_0(autosuggestion[i]);
    }

    /* Here are the layouts we try in turn:

    1. Left prompt visible, right prompt visible, command line visible, autosuggestion visible
    2. Left prompt visible, right prompt visible, command line visible, autosuggestion truncated (possibly to zero)
    3. Left prompt visible, right prompt hidden, command line visible, autosuggestion hidden
    4. Newline separator (left prompt visible, right prompt hidden, command line visible, autosuggestion visible)

    A remark about layout #4: if we've pushed the command line to a new line, why can't we draw the right prompt? The issue is resizing: if you resize the window smaller, then the right prompt will wrap to the next line. This means that we can't go back to the line that we were on, and things turn to chaos very quickly.

    */

    bool done = false;

    /* Case 1 */
    if (! done && left_prompt_width + right_prompt_width + first_command_line_width + autosuggestion_total_width < screen_width)
    {
        result.left_prompt = left_prompt;
        result.left_prompt_space = left_prompt_width;
        result.right_prompt = right_prompt;
        result.autosuggestion = autosuggestion;
        done = true;
    }

    /* Case 2. Note that we require strict inequality so that there's always at least one space between the left edge and the rprompt */
    if (! done && left_prompt_width + right_prompt_width + first_command_line_width < screen_width)
    {
        result.left_prompt = left_prompt;
        result.left_prompt_space = left_prompt_width;
        result.right_prompt = right_prompt;

        /* Need at least two characters to show an autosuggestion */
        size_t available_autosuggestion_space = screen_width - (left_prompt_width + right_prompt_width + first_command_line_width);
        if (autosuggestion_total_width > 0 && available_autosuggestion_space > 2)
        {
            size_t truncation_offset = truncation_offset_for_width(autosuggestion_truncated_widths, available_autosuggestion_space - 2);
            result.autosuggestion = wcstring(autosuggestion, truncation_offset);
            result.autosuggestion.push_back(ellipsis_char);
        }
        done = true;
    }

    /* Case 3 */
    if (! done && left_prompt_width + first_command_line_width < screen_width)
    {
        result.left_prompt = left_prompt;
        result.left_prompt_space = left_prompt_width;
        done = true;
    }

    /* Case 4 */
    if (! done)
    {
        result.left_prompt = left_prompt;
        result.left_prompt_space = left_prompt_width;
        // See remark about for why we can't use the right prompt here
        //result.right_prompt = right_prompt;

        // If the command wraps, and the prompt is not short, place the command on its own line.
        // A short prompt is 33% or less of the terminal's width.
        const size_t prompt_percent_width = (100 * left_prompt_width) / screen_width;
        if (left_prompt_width + first_command_line_width + 1 > screen_width && prompt_percent_width > 33)
        {
            result.prompts_get_own_line = true;
        }

        done = true;
    }

    assert(done);
    return result;
}


void s_write(screen_t *s,
             const wcstring &left_prompt,
             const wcstring &right_prompt,
             const wcstring &commandline,
             size_t explicit_len,
             const highlight_spec_t *colors,
             const int *indent,
             size_t cursor_pos,
             size_t sel_start_pos,
             size_t sel_stop_pos,
             const page_rendering_t &pager,
             bool cursor_position_is_within_pager)
{
    screen_data_t::cursor_t cursor_arr;

    CHECK(s,);
    CHECK(indent,);

    /* Turn the command line into the explicit portion and the autosuggestion */
    const wcstring explicit_command_line = commandline.substr(0, explicit_len);
    const wcstring autosuggestion = commandline.substr(explicit_len);

    /*
      If we are using a dumb terminal, don't try any fancy stuff,
      just print out the text. right_prompt not supported.
     */
    if (is_dumb())
    {
        const std::string prompt_narrow = wcs2string(left_prompt);
        const std::string command_line_narrow = wcs2string(explicit_command_line);

        write_loop(STDOUT_FILENO, "\r", 1);
        write_loop(STDOUT_FILENO, prompt_narrow.c_str(), prompt_narrow.size());
        write_loop(STDOUT_FILENO, command_line_narrow.c_str(), command_line_narrow.size());

        return;
    }

    s_check_status(s);
    const size_t screen_width = common_get_width();

    /* Completely ignore impossibly small screens */
    if (screen_width < 4)
    {
        return;
    }

    /* Compute a layout */
    const screen_layout_t layout = compute_layout(s, screen_width, left_prompt, right_prompt, explicit_command_line, autosuggestion, indent);

    /* Determine whether, if we have an autosuggestion, it was truncated */
    s->autosuggestion_is_truncated = ! autosuggestion.empty() && autosuggestion != layout.autosuggestion;

    /* Clear the desired screen */
    s->desired.resize(0);
    s->desired.cursor.x = s->desired.cursor.y = 0;

    /* Append spaces for the left prompt */
    for (size_t i=0; i < layout.left_prompt_space; i++)
    {
        s_desired_append_char(s, L' ', 0, 0, layout.left_prompt_space);
    }

    /* If overflowing, give the prompt its own line to improve the situation. */
    size_t first_line_prompt_space = layout.left_prompt_space;
    if (layout.prompts_get_own_line)
    {
        s_desired_append_char(s, L'\n', 0, 0, 0);
        first_line_prompt_space = 0;
    }

    /* Reconstruct the command line */
    wcstring effective_commandline = explicit_command_line + layout.autosuggestion;

    /* Output the command line */
    size_t i;
    for (i=0; i < effective_commandline.size(); i++)
    {
        /* Grab the current cursor's x,y position if this character matches the cursor's offset */
        if (! cursor_position_is_within_pager && i == cursor_pos)
        {
            cursor_arr = s->desired.cursor;
        }
        s_desired_append_char(s, effective_commandline.at(i), colors[i], indent[i], first_line_prompt_space);
    }
    
    /* Cursor may have been at the end too */
    if (! cursor_position_is_within_pager && i == cursor_pos)
    {
        cursor_arr = s->desired.cursor;
    }
    
    /* Now that we've output everything, set the cursor to the position that we saved in the loop above */
    s->desired.cursor = cursor_arr;

    if (cursor_position_is_within_pager)
    {
        s->desired.cursor.x = (int)cursor_pos;
        s->desired.cursor.y = (int)s->desired.line_count();
    }

    /* Append pager_data (none if empty) */
    s->desired.append_lines(pager.screen_data);

    s_update(s, layout.left_prompt.c_str(), layout.right_prompt.c_str());
    s_save_status(s);
}

void s_reset(screen_t *s, screen_reset_mode_t mode)
{
    CHECK(s,);

    bool abandon_line = false, repaint_prompt = false, clear_to_eos = false;
    switch (mode)
    {
        case screen_reset_current_line_contents:
            break;

        case screen_reset_current_line_and_prompt:
            repaint_prompt = true;
            break;

        case screen_reset_abandon_line:
            abandon_line = true;
            repaint_prompt = true;
            break;

        case screen_reset_abandon_line_and_clear_to_end_of_screen:
            abandon_line = true;
            repaint_prompt = true;
            clear_to_eos = true;
            break;
    }

    /* If we're abandoning the line, we must also be repainting the prompt */
    assert(! abandon_line || repaint_prompt);

    /* If we are not abandoning the line, we need to remember how many lines we had output to, so we can clear the remaining lines in the next call to s_update. This prevents leaving junk underneath the cursor when resizing a window wider such that it reduces our desired line count. */
    if (! abandon_line)
    {
        s->actual_lines_before_reset =  maxi(s->actual_lines_before_reset, s->actual.line_count());
    }

    if (repaint_prompt && ! abandon_line)
    {

        /* If the prompt is multi-line, we need to move up to the prompt's initial line. We do this by lying to ourselves and claiming that we're really below what we consider "line 0" (which is the last line of the prompt). This will cause us to move up to try to get back to line 0, but really we're getting back to the initial line of the prompt. */
        const size_t prompt_line_count = calc_prompt_lines(s->actual_left_prompt);
        assert(prompt_line_count >= 1);
        s->actual.cursor.y += (prompt_line_count - 1);
    }
    else if (abandon_line)
    {
        s->actual.cursor.y = 0;
    }

    if (repaint_prompt)
        s->actual_left_prompt.clear();
    s->actual.resize(0);
    s->need_clear_lines = true;
    s->need_clear_screen = s->need_clear_screen || clear_to_eos;

    if (abandon_line)
    {
        /* Do the PROMPT_SP hack */
        int screen_width = common_get_width();
        wcstring abandon_line_string;
        abandon_line_string.reserve(screen_width + 32); //should be enough

	/* Don't need to check for wcwidth errors; this is done when setting up omitted_newline_char in common.cpp */
        int non_space_width = wcwidth(omitted_newline_char);
        if (screen_width >= non_space_width)
        {
            if (output_get_color_support() & color_support_term256)
            {
                // draw the string in term256 gray
                abandon_line_string.append(L"\x1b[38;5;245m");
            }
            else
            {
                // draw in "bright black" (gray)
                abandon_line_string.append(L"\x1b[0m" //bright
                                           L"\x1b[30;1m"); //black
            }
            abandon_line_string.push_back(omitted_newline_char);
            abandon_line_string.append(L"\x1b[0m"); //normal text ANSI escape sequence
            abandon_line_string.append(screen_width - non_space_width, L' ');

        }
        abandon_line_string.push_back(L'\r');
        // now we are certainly on a new line. But we may have dropped the omitted newline char on it. So append enough spaces to overwrite the omitted newline char, and then
        abandon_line_string.append(non_space_width, L' ');
        abandon_line_string.push_back(L'\r');
        abandon_line_string.append(L"\x1b[2K"); //clear all the spaces from the new line

        const std::string narrow_abandon_line_string = wcs2string(abandon_line_string);
        write_loop(STDOUT_FILENO, narrow_abandon_line_string.c_str(), narrow_abandon_line_string.size());
        s->actual.cursor.x = 0;
    }

    if (! abandon_line)
    {
        /* This should prevent resetting the cursor position during the next repaint. */
        write_loop(STDOUT_FILENO, "\r", 1);
        s->actual.cursor.x = 0;
    }

    fstat(1, &s->prev_buff_1);
    fstat(2, &s->prev_buff_2);
}

bool screen_force_clear_to_end()
{
    bool result = false;
    if (clr_eos)
    {
        data_buffer_t output;
        s_write_mbs(&output, clr_eos);
        if (! output.empty())
        {
            write_loop(STDOUT_FILENO, &output.at(0), output.size());
            result = true;
        }
    }
    return result;
}

screen_t::screen_t() :
    desired(),
    actual(),
    actual_left_prompt(),
    last_right_prompt_width(),
    actual_width(SCREEN_WIDTH_UNINITIALIZED),
    soft_wrap_location(INVALID_LOCATION),
    autosuggestion_is_truncated(false),
    need_clear_lines(false),
    need_clear_screen(false),
    actual_lines_before_reset(0),
    prev_buff_1(), prev_buff_2(), post_buff_1(), post_buff_2()
{
}

