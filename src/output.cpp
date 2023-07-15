// Generic output functions.
#include "config.h"

#include <stdio.h>
#include <unistd.h>

#if HAVE_CURSES_H
#include <curses.h>  // IWYU pragma: keep
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

#include <cwchar>
#include <mutex>
#include <string>
#include <vector>

#include "color.h"
#include "common.h"
#include "env.h"
#include "fallback.h"  // IWYU pragma: keep
#include "flog.h"
#include "maybe.h"
#include "output.h"
#include "threads.rs.h"
#include "wcstringutil.h"
#include "wutil.h"  // IWYU pragma: keep

/// Given a list of rgb_color_t, pick the "best" one, as determined by the color support. Returns
/// rgb_color_t::none() if empty.
/// TODO: This is duplicated with Rust.
rgb_color_t best_color(const std::vector<rgb_color_t> &candidates, color_support_t support) {
    if (candidates.empty()) {
        return rgb_color_t::none();
    }

    rgb_color_t first_rgb = rgb_color_t::none(), first_named = rgb_color_t::none();
    for (const auto &color : candidates) {
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
/// TODO: This code should be refactored to enable sharing with builtin_set_color.
///       In particular, the argument parsing still isn't fully capable.
/// TODO: This is duplicated with Rust.
rgb_color_t parse_color(const env_var_t &var, bool is_background) {
    bool is_bold = false;
    bool is_underline = false;
    bool is_italics = false;
    bool is_dim = false;
    bool is_reverse = false;

    std::vector<rgb_color_t> candidates;

    const wchar_t *prefix = L"--background=";
    // wcslen is not available as constexpr
    size_t prefix_len = wcslen(prefix);

    bool next_is_background = false;
    wcstring color_name;
    for (const wcstring &next : var.as_list()->vals) {
        color_name.clear();
        if (is_background) {
            if (color_name.empty() && next_is_background) {
                color_name = next;
                next_is_background = false;
            } else if (string_prefixes_string(prefix, next)) {
                // Look for something like "--background=red".
                color_name = wcstring(next, prefix_len);
            } else if (next == L"--background" || next == L"-b") {
                // Without argument attached the next token is the color
                // - if it's another option it's an error.
                next_is_background = true;
            } else if (next == L"--reverse" || next == L"-r") {
                // Reverse should be meaningful in either context
                is_reverse = true;
            } else if (string_prefixes_string(L"-b", next)) {
                // Look for something like "-bred".
                // Yes, that length is hardcoded.
                color_name = wcstring(next, 2);
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
    return result;
}

void writembs_nofail(outputter_t &outp, const char *str) {
    assert(str != nullptr && "Null string");
    outp.writembs(str);
}

void writembs(outputter_t &outp, const char *str) { writembs_nofail(outp, str); }
