/** \file output.c
 Generic output functions
 */
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
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
#include <wchar.h>
#include <limits.h>
#include <string>
#include <vector>
#include <memory>

#include "fallback.h"  // IWYU pragma: keep
#include "wutil.h"  // IWYU pragma: keep
#include "common.h"
#include "output.h"
#include "color.h"

static int writeb_internal(char c);

/**
 The function used for output
 */

static int (*out)(char c) = &writeb_internal;

/**
 Name of terminal
 */
static wcstring current_term;

/* Whether term256 and term24bit are supported */
static color_support_t color_support = 0;


void output_set_writer(int (*writer)(char))
{
    CHECK(writer,);
    out = writer;
}

int (*output_get_writer())(char)
{
    return out;
}

static bool term256_support_is_native(void)
{
    /* Return YES if we think the term256 support is "native" as opposed to forced. */
    return max_colors >= 256;
}

color_support_t output_get_color_support(void)
{
    return color_support;
}

void output_set_color_support(color_support_t val)
{
    color_support = val;
}

unsigned char index_for_color(rgb_color_t c)
{
    if (c.is_named() || ! (output_get_color_support() & color_support_term256))
    {
        return c.to_name_index();
    }
    else
    {
        return c.to_term256_index();
    }
}


static bool write_color_escape(char *todo, unsigned char idx, bool is_fg)
{
    bool result = false;
    if (idx < 16 || term256_support_is_native())
    {
        /* Use tparm */
        writembs(tparm(todo, idx));
        result = true;
    }
    else
    {
        /* We are attempting to bypass the term here. Generate the ANSI escape sequence ourself. */
        char stridx[128];
        format_long_safe(stridx, idx);
        char buff[128] = "\x1b[";
        strcat(buff, is_fg ? "38;5;" : "48;5;");
        strcat(buff, stridx);
        strcat(buff, "m");

        int (*writer)(char) = output_get_writer();
        if (writer)
        {
            for (size_t i=0; buff[i]; i++)
            {
                writer(buff[i]);
            }
        }

        result = true;
    }
    return result;
}

static bool write_foreground_color(unsigned char idx)
{
    if (set_a_foreground && set_a_foreground[0])
    {
        return write_color_escape(set_a_foreground, idx, true);
    }
    else if (set_foreground && set_foreground[0])
    {
        return write_color_escape(set_foreground, idx, true);
    }
    else
    {
        return false;
    }
}

static bool write_background_color(unsigned char idx)
{
    if (set_a_background && set_a_background[0])
    {
        return write_color_escape(set_a_background, idx, false);
    }
    else if (set_background && set_background[0])
    {
        return write_color_escape(set_background, idx, false);
    }
    else
    {
        return false;
    }
}

void write_color(rgb_color_t color, bool is_fg)
{
    bool supports_term24bit = !! (output_get_color_support() & color_support_term24bit);
    if (! supports_term24bit || ! color.is_rgb())
    {
        /* Indexed or non-24 bit color */
        unsigned char idx = index_for_color(color);
        (is_fg ? write_foreground_color : write_background_color)(idx);
    }
    else
    {
        /* 24 bit! No tparm here, just ANSI escape sequences.
           Foreground: ^[38;2;<r>;<g>;<b>m
           Background: ^[48;2;<r>;<g>;<b>m
        */
        color24_t rgb = color.to_color24();
        char buff[128];
        snprintf(buff, sizeof buff, "\x1b[%u;2;%u;%u;%um", is_fg ? 38 : 48, rgb.rgb[0], rgb.rgb[1], rgb.rgb[2]);
        int (*writer)(char) = output_get_writer();
        if (writer)
        {
            for (size_t i=0; buff[i]; i++)
            {
                writer(buff[i]);
            }
        }
    }
}

void set_color(rgb_color_t c, rgb_color_t c2)
{

#if 0
    wcstring tmp = c.description();
    wcstring tmp2 = c2.description();
    printf("set_color %ls : %ls\n", tmp.c_str(), tmp2.c_str());
#endif
    ASSERT_IS_MAIN_THREAD();

    const rgb_color_t normal = rgb_color_t::normal();
    static rgb_color_t last_color = rgb_color_t::normal();
    static rgb_color_t last_color2 = rgb_color_t::normal();
    static int was_bold=0;
    static int was_underline=0;
    int bg_set=0, last_bg_set=0;

    int is_bold = 0;
    int is_underline = 0;

    /*
       Test if we have at least basic support for setting fonts, colors
       and related bits - otherwise just give up...
       */
    if (!exit_attribute_mode)
    {
        return;
    }


    is_bold |= c.is_bold();
    is_bold |= c2.is_bold();

    is_underline |= c.is_underline();
    is_underline |= c2.is_underline();

    if (c.is_reset() || c2.is_reset())
    {
        c = c2 = normal;
        was_bold=0;
        was_underline=0;
        /*
         If we exit attibute mode, we must first set a color, or
         previously coloured text might lose it's
         color. Terminals are weird...
         */
        write_foreground_color(0);
        writembs(exit_attribute_mode);
        return;
    }

    if (was_bold && !is_bold)
    {
        /*
             Only way to exit bold mode is a reset of all attributes.
             */
        writembs(exit_attribute_mode);
        last_color = normal;
        last_color2 = normal;
        was_bold=0;
        was_underline=0;
    }

    if (!last_color2.is_normal() && !last_color2.is_reset())
    {
        // Background was set.
        last_bg_set = 1;
    }

    if (!c2.is_normal())
    {
        // Background is set.
        bg_set = 1;
        if (c == c2) c = (c2==rgb_color_t::white())?rgb_color_t::black():rgb_color_t::white();
    }

    if ((enter_bold_mode != 0) && (strlen(enter_bold_mode) > 0))
    {
        if (bg_set && !last_bg_set)
        {
            /*
                   Background color changed and is set, so we enter bold
                   mode to make reading easier. This means bold mode is
                   _always_ on when the background color is set.
                   */
            writembs(enter_bold_mode);
        }
        if (!bg_set && last_bg_set)
        {
            /*
                   Background color changed and is no longer set, so we
                   exit bold mode
                   */
            writembs(exit_attribute_mode);
            was_bold=0;
            was_underline=0;
            /*
                   We don't know if exit_attribute_mode resets colors, so
                   we set it to something known.
                   */
            if (write_foreground_color(0))
            {
                last_color=rgb_color_t::black();
            }
        }
    }

    if (last_color != c)
    {
        if (c.is_normal())
        {
            write_foreground_color(0);
            writembs(exit_attribute_mode);

            last_color2 = rgb_color_t::normal();
            was_bold=0;
            was_underline=0;
        }
        else if (! c.is_special())
        {
            write_color(c, true /* foreground */);
        }
    }

    last_color = c;

    if (last_color2 != c2)
    {
        if (c2.is_normal())
        {
            write_background_color(0);

            writembs(exit_attribute_mode);
            if (! last_color.is_normal())
            {
                write_color(last_color, true /* foreground */);
            }


            was_bold=0;
            was_underline=0;
            last_color2 = c2;
        }
        else if (! c2.is_special())
        {
            write_color(c2, false /* not foreground */);
            last_color2 = c2;
        }
    }

    /*
       Lastly, we set bold mode and underline mode correctly
       */
    if ((enter_bold_mode != 0) && (strlen(enter_bold_mode) > 0) && !bg_set)
    {
        if (is_bold && !was_bold)
        {
            if (enter_bold_mode)
            {
                writembs(tparm(enter_bold_mode));
            }
        }
        was_bold = is_bold;
    }

    if (was_underline && !is_underline)
    {
        writembs(exit_underline_mode);
    }

    if (!was_underline && is_underline)
    {
        writembs(enter_underline_mode);
    }
    was_underline = is_underline;

}

/**
 Default output method, simply calls write() on stdout
 */
static int writeb_internal(char c)
{
    write_loop(1, &c, 1);
    return 0;
}

int writeb(tputs_arg_t b)
{
    out(b);
    return 0;
}

int writech(wint_t ch)
{
    char buff[MB_LEN_MAX+1];
    size_t len;

    if (ch >= ENCODE_DIRECT_BASE && ch < ENCODE_DIRECT_BASE + 256)
    {
        buff[0] = ch - ENCODE_DIRECT_BASE;
        len = 1;
    }
    else if (MB_CUR_MAX == 1) // single-byte locale (C/POSIX/ISO-8859)
    {
        // If `wc` contains a wide character we emit a question-mark.
        if (ch & ~0xFF)
        {
            ch = '?';
        }
        buff[0] = ch;
        len = 1;
    }
    else
    {
        mbstate_t state = {};
        len = wcrtomb(buff, ch, &state);
        if (len == (size_t)-1)
        {
            return 1;
        }
    }

    for (size_t i = 0; i < len; i++)
    {
        out(buff[i]);
    }
    return 0;
}

void writestr(const wchar_t *str)
{
    CHECK(str,);

    if (MB_CUR_MAX == 1) // single-byte locale (C/POSIX/ISO-8859)
    {
        while( *str )
        {
            writech( *str++ );
        }
        return;
    }

    size_t len = wcstombs(0, str, 0);  // figure amount of space needed
    if (len == (size_t)-1)
    {
        debug(1, L"Tried to print invalid wide character string");
        return;
    }

    // Convert the string.
    len++;
    char *buffer, static_buffer[256];
    if (len <= sizeof static_buffer)
        buffer = static_buffer;
    else
        buffer = new char[len];

    wcstombs(buffer,
             str,
             len);

    /*
       Write
       */
    for (char *pos = buffer; *pos; pos++)
    {
        out(*pos);
    }

    if (buffer != static_buffer)
        delete[] buffer;
}

rgb_color_t best_color(const std::vector<rgb_color_t> &candidates, color_support_t support)
{
    if (candidates.empty())
    {
        return rgb_color_t::none();
    }
    
    rgb_color_t first_rgb = rgb_color_t::none(), first_named = rgb_color_t::none();
    for (size_t i=0; i < candidates.size(); i++)
    {
        const rgb_color_t &color = candidates.at(i);
        if (first_rgb.is_none() && color.is_rgb())
        {
            first_rgb = color;
        }
        if (first_named.is_none() && color.is_named())
        {
            first_named = color;
        }
    }
    // If we have both RGB and named colors, then prefer rgb if term256 is supported
    rgb_color_t result = rgb_color_t::none();
    bool has_term256 = !! (support & color_support_term256);
    if ((!first_rgb.is_none() && has_term256) || first_named.is_none())
    {
        result = first_rgb;
    }
    else
    {
        result = first_named;
    }
    if (result.is_none())
    {
        result = candidates.at(0);
    }
    return result;
}

/* This code should be refactored to enable sharing with builtin_set_color */
rgb_color_t parse_color(const wcstring &val, bool is_background)
{
    int is_bold=0;
    int is_underline=0;

    std::vector<rgb_color_t> candidates;

    wcstring_list_t el;
    tokenize_variable_array(val, el);

    for (size_t j=0; j < el.size(); j++)
    {
        const wcstring &next = el.at(j);
        wcstring color_name;
        if (is_background)
        {
            // look for something like "--background=red"
            const wcstring prefix = L"--background=";
            if (string_prefixes_string(prefix, next))
            {
                color_name = wcstring(next, prefix.size());
            }
        }
        else
        {
            if (next == L"--bold" || next == L"-o")
                is_bold = true;
            else if (next == L"--underline" || next == L"-u")
                is_underline = true;
            else
                color_name = next;
        }

        if (! color_name.empty())
        {
            rgb_color_t color = rgb_color_t(color_name);
            if (! color.is_none())
            {
                candidates.push_back(color);
            }
        }
    }
    rgb_color_t result = best_color(candidates, output_get_color_support());
    
    if (result.is_none())
        result = rgb_color_t::normal();

    result.set_bold(is_bold);
    result.set_underline(is_underline);

#if 0
    wcstring desc = result.description();
    printf("Parsed %ls from %ls (%s)\n", desc.c_str(), val.c_str(), is_background ? "background" : "foreground");
#endif

    return result;
}

void output_set_term(const wcstring &term)
{
    current_term.assign(term);
}

const wchar_t *output_get_term()
{
    return current_term.empty() ? L"<unknown>" : current_term.c_str();
}

void writembs_check(char *mbs, const char *mbs_name, const char *file, long line)
{
    if (mbs != NULL)
    {
        tputs(mbs, 1, &writeb);
    }
    else
    {
        debug( 0, _(L"Tried to use terminfo string %s on line %ld of %s, which is undefined in terminal of type \"%ls\". Please report this error to %s"),
              mbs_name,
              line,
              file,
              output_get_term(),
              PACKAGE_BUGREPORT);
    }
}
