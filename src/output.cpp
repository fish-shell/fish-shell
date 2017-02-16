// Generic output functions.
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include <limits.h>
#include <wchar.h>

#include <memory>
#include <string>
#include <vector>

#include "color.h"
#include "common.h"
#include "env.h"
#include "fallback.h"  // IWYU pragma: keep
#include "output.h"
#include "wutil.h"  // IWYU pragma: keep

static int writeb_internal(char c);

/// The function used for output.
static int (*out)(char c) = writeb_internal;  //!OCLINT(unused param)

/// Whether term256 and term24bit are supported.
static color_support_t color_support = 0;

/// Set the function used for writing in move_cursor, writespace and set_color and all other output
/// functions in this library. By default, the write call is used to give completely unbuffered
/// output to stdout.
void output_set_writer(int (*writer)(char)) {
    CHECK(writer, );
    out = writer;
}

/// Return the current output writer.
int (*output_get_writer())(char) { return out; }

/// Returns true if we think tparm can handle outputting a color index
static bool term_supports_color_natively(unsigned int c) { return (unsigned)max_colors >= c + 1; }

color_support_t output_get_color_support(void) { return color_support; }

void output_set_color_support(color_support_t val) { color_support = val; }

unsigned char index_for_color(rgb_color_t c) {
    if (c.is_named() || !(output_get_color_support() & color_support_term256)) {
        return c.to_name_index();
    }
    return c.to_term256_index();
}

static bool write_color_escape(char *todo, unsigned char idx, bool is_fg) {
    if (term_supports_color_natively(idx)) {
        // Use tparm to emit color escape.
        writembs(tparm(todo, idx));
        return true;
    }

    // We are attempting to bypass the term here. Generate the ANSI escape sequence ourself.
    char buff[16] = "";
    if (idx < 16) {
        // this allows the non-bright color to happen instead of no color working at all when a
        // bright is attempted when only colors 0-7 are supported.
        //
        // TODO: enter bold mode in builtin_set_color in the same circumstance- doing that combined
        // with what we do here, will make the brights actually work for virtual consoles/ancient
        // emulators.
        if (max_colors == 8 && idx > 8) idx -= 8;
        snprintf(buff, sizeof buff, "\e[%dm", ((idx > 7) ? 82 : 30) + idx + !is_fg * 10);
    } else {
        snprintf(buff, sizeof buff, "\e[%d;5;%dm", is_fg ? 38 : 48, idx);
    }

    int (*writer)(char) = output_get_writer();
    if (writer) {
        for (size_t i = 0; buff[i]; i++) {
            writer(buff[i]);
        }
    }

    return true;
}

static bool write_foreground_color(unsigned char idx) {
    if (!cur_term) return false;
    if (set_a_foreground && set_a_foreground[0]) {
        return write_color_escape(set_a_foreground, idx, true);
    } else if (set_foreground && set_foreground[0]) {
        return write_color_escape(set_foreground, idx, true);
    }
    return false;
}

static bool write_background_color(unsigned char idx) {
    if (!cur_term) return false;
    if (set_a_background && set_a_background[0]) {
        return write_color_escape(set_a_background, idx, false);
    } else if (set_background && set_background[0]) {
        return write_color_escape(set_background, idx, false);
    }
    return false;
}

// Exported for builtin_set_color's usage only.
bool write_color(rgb_color_t color, bool is_fg) {
    if (!cur_term) return false;
    bool supports_term24bit =
        static_cast<bool>(output_get_color_support() & color_support_term24bit);
    if (!supports_term24bit || !color.is_rgb()) {
        // Indexed or non-24 bit color.
        unsigned char idx = index_for_color(color);
        return (is_fg ? write_foreground_color : write_background_color)(idx);
    }

    // 24 bit! No tparm here, just ANSI escape sequences.
    // Foreground: ^[38;2;<r>;<g>;<b>m
    // Background: ^[48;2;<r>;<g>;<b>m
    color24_t rgb = color.to_color24();
    char buff[128];
    snprintf(buff, sizeof buff, "\e[%d;2;%u;%u;%um", is_fg ? 38 : 48, rgb.rgb[0], rgb.rgb[1],
             rgb.rgb[2]);
    int (*writer)(char) = output_get_writer();
    if (writer) {
        for (size_t i = 0; buff[i]; i++) {
            writer(buff[i]);
        }
    }

    return true;
}

/// Sets the fg and bg color. May be called as often as you like, since if the new color is the same
/// as the previous, nothing will be written. Negative values for set_color will also be ignored.
/// Since the terminfo string this function emits can potentially cause the screen to flicker, the
/// function takes care to write as little as possible.
///
/// Possible values for color are any form the FISH_COLOR_* enum and FISH_COLOR_RESET.
/// FISH_COLOR_RESET will perform an exit_attribute_mode, even if set_color thinks it is already in
/// FISH_COLOR_NORMAL mode.
///
/// In order to set the color to normal, three terminfo strings may have to be written.
///
/// - First a string to set the color, such as set_a_foreground. This is needed because otherwise
/// the previous strings colors might be removed as well.
///
/// - After that we write the exit_attribute_mode string to reset all color attributes.
///
/// - Lastly we may need to write set_a_background or set_a_foreground to set the other half of the
/// color pair to what it should be.
///
/// \param c Foreground color.
/// \param c2 Background color.
void set_color(rgb_color_t c, rgb_color_t c2) {
#if 0
    wcstring tmp = c.description();
    wcstring tmp2 = c2.description();
    debug(3, "set_color %ls : %ls\n", tmp.c_str(), tmp2.c_str());
#endif
    ASSERT_IS_MAIN_THREAD();
    if (!cur_term) return;

    const rgb_color_t normal = rgb_color_t::normal();
    static rgb_color_t last_color = rgb_color_t::normal();
    static rgb_color_t last_color2 = rgb_color_t::normal();
    static bool was_bold = false;
    static bool was_underline = false;
    static bool was_italics = false;
    static bool was_dim = false;
    static bool was_reverse = false;
    bool bg_set = false, last_bg_set = false;
    bool is_bold = false;
    bool is_underline = false;
    bool is_italics = false;
    bool is_dim = false;
    bool is_reverse = false;

    // Test if we have at least basic support for setting fonts, colors and related bits - otherwise
    // just give up...
    if (!exit_attribute_mode) {
        return;
    }

    is_bold |= c.is_bold();
    is_bold |= c2.is_bold();

    is_underline |= c.is_underline();
    is_underline |= c2.is_underline();

    is_italics |= c.is_italics();
    is_italics |= c2.is_italics();

    is_dim |= c.is_dim();
    is_dim |= c2.is_dim();

    is_reverse |= c.is_reverse();
    is_reverse |= c2.is_reverse();

    if (c.is_reset() || c2.is_reset()) {
        c = c2 = normal;
        was_bold = false;
        was_underline = false;
        was_italics = false;
        was_dim = false;
        was_reverse = false;
        // If we exit attibute mode, we must first set a color, or previously coloured text might
        // lose it's color. Terminals are weird...
        write_foreground_color(0);
        writembs(exit_attribute_mode);
        return;
    }

    if (was_bold && !is_bold) {
        // Only way to exit bold mode is a reset of all attributes.
        writembs(exit_attribute_mode);
        last_color = normal;
        last_color2 = normal;
        was_bold = false;
        was_underline = false;
        was_italics = false;
        was_dim = false;
        was_reverse = false;
    }

    if (was_dim && !is_dim) {
        // Only way to exit dim mode is a reset of all attributes.
        writembs(exit_attribute_mode);
        last_color = normal;
        last_color2 = normal;
        was_bold = false;
        was_underline = false;
        was_italics = false;
        was_dim = false;
        was_reverse = false;
    }

    if (was_reverse && !is_reverse) {
        // Only way to exit reverse mode is a reset of all attributes.
        writembs(exit_attribute_mode);
        last_color = normal;
        last_color2 = normal;
        was_bold = false;
        was_underline = false;
        was_italics = false;
        was_dim = false;
        was_reverse = false;
    }

    if (!last_color2.is_normal() && !last_color2.is_reset()) {
        // Background was set.
        last_bg_set = true;
    }

    if (!c2.is_normal()) {
        // Background is set.
        bg_set = true;
        if (c == c2) c = (c2 == rgb_color_t::white()) ? rgb_color_t::black() : rgb_color_t::white();
    }

    if ((enter_bold_mode != 0) && (strlen(enter_bold_mode) > 0)) {
        if (bg_set && !last_bg_set) {
            // Background color changed and is set, so we enter bold mode to make reading easier.
            // This means bold mode is _always_ on when the background color is set.
            writembs(enter_bold_mode);
        }
        if (!bg_set && last_bg_set) {
            // Background color changed and is no longer set, so we exit bold mode.
            writembs(exit_attribute_mode);
            was_bold = false;
            was_underline = false;
            was_italics = false;
            was_dim = false;
            was_reverse = false;
            // We don't know if exit_attribute_mode resets colors, so we set it to something known.
            if (write_foreground_color(0)) {
                last_color = rgb_color_t::black();
            }
        }
    }

    if (last_color != c) {
        if (c.is_normal()) {
            write_foreground_color(0);
            writembs(exit_attribute_mode);

            last_color2 = rgb_color_t::normal();
            was_bold = false;
            was_underline = false;
            was_italics = false;
            was_dim = false;
            was_reverse = false;
        } else if (!c.is_special()) {
            write_color(c, true /* foreground */);
        }
    }

    last_color = c;

    if (last_color2 != c2) {
        if (c2.is_normal()) {
            write_background_color(0);

            writembs(exit_attribute_mode);
            if (!last_color.is_normal()) {
                write_color(last_color, true /* foreground */);
            }

            was_bold = false;
            was_underline = false;
            was_italics = false;
            was_dim = false;
            was_reverse = false;
            last_color2 = c2;
        } else if (!c2.is_special()) {
            write_color(c2, false /* not foreground */);
            last_color2 = c2;
        }
    }

    // Lastly, we set bold, underline, italics, dim, and reverse modes correctly.
    if (is_bold && !was_bold && enter_bold_mode && strlen(enter_bold_mode) > 0 && !bg_set) {
        writembs(tparm(enter_bold_mode));
        was_bold = is_bold;
    }

    if (was_underline && !is_underline) {
        writembs(exit_underline_mode);
    }

    if (!was_underline && is_underline) {
        writembs(enter_underline_mode);
    }
    was_underline = is_underline;

    if (was_italics && !is_italics && enter_italics_mode && strlen(enter_italics_mode) > 0) {
        writembs(exit_italics_mode);
        was_italics = is_italics;
    }

    if (!was_italics && is_italics && enter_italics_mode && strlen(enter_italics_mode) > 0) {
        writembs(enter_italics_mode);
        was_italics = is_italics;
    }

    if (is_dim && !was_dim && enter_dim_mode && strlen(enter_dim_mode) > 0) {
        writembs(enter_dim_mode);
        was_dim = is_dim;
    }

    if (is_reverse && !was_reverse) {
        // Some terms do not have a reverse mode set, so standout mode is a fallback.
        if (enter_reverse_mode && strlen(enter_reverse_mode) > 0) {
            writembs(enter_reverse_mode);
            was_reverse = is_reverse;
        } else if (enter_standout_mode && strlen(enter_standout_mode) > 0) {
            writembs(enter_standout_mode);
            was_reverse = is_reverse;
        }
    }
}

/// Default output method, simply calls write() on stdout.
static int writeb_internal(char c) {  // cppcheck
    write_loop(1, &c, 1);
    return 0;
}

/// This is for writing process notification messages. Has to write to stdout, so clr_eol and such
/// functions will work correctly. Not an issue since this function is only used in interactive mode
/// anyway.
int writeb(tputs_arg_t b) {
    out(b);
    return 0;
}

/// Write a wide character using the output method specified using output_set_writer(). This should
/// only be used when writing characters from user supplied strings. This is needed due to our use
/// of the ENCODE_DIRECT_BASE mechanism to allow the user to specify arbitrary byte values to be
/// output. Such as in a `printf` invocation that includes literal byte values such as `\x1B`.
/// This should not be used for writing non-user supplied characters.
int writech(wint_t ch) {
    char buff[MB_LEN_MAX + 1];
    size_t len;

    if (ch >= ENCODE_DIRECT_BASE && ch < ENCODE_DIRECT_BASE + 256) {
        buff[0] = ch - ENCODE_DIRECT_BASE;
        len = 1;
    } else if (MB_CUR_MAX == 1) {
        // single-byte locale (C/POSIX/ISO-8859)
        // If `wc` contains a wide character we emit a question-mark.
        buff[0] = ch & ~0xFF ? '?' : ch;
        len = 1;
    } else {
        mbstate_t state = {};
        len = wcrtomb(buff, ch, &state);
        if (len == (size_t)-1) {
            return 1;
        }
    }

    for (size_t i = 0; i < len; i++) {
        out(buff[i]);
    }
    return 0;
}

/// Write a wide character string to stdout. This should not be used to output things like warning
/// messages; just use debug() or fwprintf() for that. It should only be used to output user
/// supplied strings that might contain literal bytes; e.g., "\342\224\214" from issue #1894. This
/// is needed because those strings may contain chars specially encoded using ENCODE_DIRECT_BASE.
void writestr(const wchar_t *str) {
    CHECK(str, );

    if (MB_CUR_MAX == 1) {
        // Single-byte locale (C/POSIX/ISO-8859).
        while (*str) writech(*str++);
        return;
    }

    size_t len = wcstombs(0, str, 0);  // figure amount of space needed
    if (len == (size_t)-1) {
        debug(1, L"Tried to print invalid wide character string");
        return;
    }

    // Convert the string.
    len++;
    char *buffer, static_buffer[256];
    if (len <= sizeof static_buffer) {
        buffer = static_buffer;
    } else {
        buffer = new char[len];
    }
    wcstombs(buffer, str, len);

    // Write the string.
    for (char *pos = buffer; *pos; pos++) {
        out(*pos);
    }

    if (buffer != static_buffer) delete[] buffer;
}

/// Given a list of rgb_color_t, pick the "best" one, as determined by the color support. Returns
/// rgb_color_t::none() if empty.
rgb_color_t best_color(const std::vector<rgb_color_t> &candidates, color_support_t support) {
    if (candidates.empty()) {
        return rgb_color_t::none();
    }

    rgb_color_t first_rgb = rgb_color_t::none(), first_named = rgb_color_t::none();
    for (size_t i = 0; i < candidates.size(); i++) {
        const rgb_color_t &color = candidates.at(i);
        if (first_rgb.is_none() && color.is_rgb()) {
            first_rgb = color;
        }
        if (first_named.is_none() && color.is_named()) {
            first_named = color;
        }
    }
    // If we have both RGB and named colors, then prefer rgb if term256 is supported.
    rgb_color_t result = rgb_color_t::none();
    bool has_term256 = static_cast<bool>(support & color_support_term256);
    if ((!first_rgb.is_none() && has_term256) || first_named.is_none()) {
        result = first_rgb;
    } else {
        result = first_named;
    }
    if (result.is_none()) {
        result = candidates.at(0);
    }
    return result;
}

/// Return the internal color code representing the specified color.
/// XXX This code should be refactored to enable sharing with builtin_set_color.
rgb_color_t parse_color(const wcstring &val, bool is_background) {
    int is_bold = 0;
    int is_underline = 0;
    int is_italics = 0;
    int is_dim = 0;
    int is_reverse = 0;

    std::vector<rgb_color_t> candidates;

    wcstring_list_t el;
    tokenize_variable_array(val, el);

    for (size_t j = 0; j < el.size(); j++) {
        const wcstring &next = el.at(j);
        wcstring color_name;
        if (is_background) {
            // Look for something like "--background=red".
            const wcstring prefix = L"--background=";
            if (string_prefixes_string(prefix, next)) {
                color_name = wcstring(next, prefix.size());
            }
        } else {
            if (next == L"--bold" || next == L"-o")
                is_bold = true;
            else if (next == L"--underline" || next == L"-u")
                is_underline = true;
            else if (next == L"--italics" || next == L"-i")
                is_italics = true;
            else if (next == L"--dim" || next == L"-d")
                is_dim = true;
            else if (next == L"--reverse" || next == L"-r")
                is_reverse = true;
            else
                color_name = next;
        }

        if (!color_name.empty()) {
            rgb_color_t color = rgb_color_t(color_name);
            if (!color.is_none()) {
                candidates.push_back(color);
            }
        }
    }
    rgb_color_t result = best_color(candidates, output_get_color_support());

    if (result.is_none()) result = rgb_color_t::normal();

    result.set_bold(is_bold);
    result.set_underline(is_underline);
    result.set_italics(is_italics);
    result.set_dim(is_dim);
    result.set_reverse(is_reverse);

#if 0
    wcstring desc = result.description();
    fwprintf(stdout, L"Parsed %ls from %ls (%s)\n", desc.c_str(), val.c_str(),
             is_background ? "background" : "foreground");
#endif

    return result;
}

/// Write specified multibyte string.
void writembs_check(char *mbs, const char *mbs_name, const char *file, long line) {
    if (mbs != NULL) {
        tputs(mbs, 1, &writeb);
    } else {
        env_var_t term = env_get_string(L"TERM");
        debug(0, _(L"Tried to use terminfo string %s on line %ld of %s, which is undefined in "
                   L"terminal of type \"%ls\". Please report this error to %s"),
              mbs_name, line, file, term.c_str(), PACKAGE_BUGREPORT);
    }
}
