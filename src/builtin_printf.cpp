// printf - format and print data
// Copyright (C) 1990-2007 Free Software Foundation, Inc.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

// Usage: printf format [argument...]
//
// A front end to the printf function that lets it be used from the shell.
//
// Backslash escapes:
//
// \" = double quote
// \\ = backslash
// \a = alert (bell)
// \b = backspace
// \c = produce no further output
// \e = escape
// \f = form feed
// \n = new line
// \r = carriage return
// \t = horizontal tab
// \v = vertical tab
// \ooo = octal number (ooo is 1 to 3 digits)
// \xhh = hexadecimal number (hhh is 1 to 2 digits)
// \uhhhh = 16-bit Unicode character (hhhh is 4 digits)
// \Uhhhhhhhh = 32-bit Unicode character (hhhhhhhh is 8 digits)
//
// Additional directive:
//
// %b = print an argument string, interpreting backslash escapes,
//   except that octal escapes are of the form \0 or \0ooo.
//
// The `format' argument is re-used as many times as necessary
// to convert all of the given arguments.
//
// David MacKenzie <djm@gnu.ai.mit.edu>

// This file has been imported from source code of printf command in GNU Coreutils version 6.9.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <wchar.h>
#include <wctype.h>

#include "builtin.h"
#include "common.h"
#include "io.h"
#include "proc.h"
#include "wutil.h"  // IWYU pragma: keep

class parser_t;

struct builtin_printf_state_t {
    // Out and err streams. Note this is a captured reference!
    io_streams_t &streams;

    // The status of the operation.
    int exit_code;

    // Whether we should stop outputting. This gets set in the case of an error, and also with the
    // \c escape.
    bool early_exit;

    explicit builtin_printf_state_t(io_streams_t &s)
        : streams(s), exit_code(0), early_exit(false) {}

    void verify_numeric(const wchar_t *s, const wchar_t *end, int errcode);

    void print_direc(const wchar_t *start, size_t length, wchar_t conversion, bool have_field_width,
                     int field_width, bool have_precision, int precision, wchar_t const *argument);

    int print_formatted(const wchar_t *format, int argc, wchar_t **argv);

    void fatal_error(const wchar_t *format, ...);

    long print_esc(const wchar_t *escstart, bool octal_0);
    void print_esc_string(const wchar_t *str);
    void print_esc_char(wchar_t c);

    void append_output(wchar_t c);
    void append_output(const wchar_t *c);
    void append_format_output(const wchar_t *fmt, ...);
};

static bool is_octal_digit(wchar_t c) { return c != L'\0' && wcschr(L"01234567", c) != NULL; }

static bool is_hex_digit(wchar_t c) {
    return c != L'\0' && wcschr(L"0123456789ABCDEFabcdef", c) != NULL;
}

static int hex_to_bin(const wchar_t &c) {
    switch (c) {
        case L'0': {
            return 0;
        }
        case L'1': {
            return 1;
        }
        case L'2': {
            return 2;
        }
        case L'3': {
            return 3;
        }
        case L'4': {
            return 4;
        }
        case L'5': {
            return 5;
        }
        case L'6': {
            return 6;
        }
        case L'7': {
            return 7;
        }
        case L'8': {
            return 8;
        }
        case L'9': {
            return 9;
        }
        case L'a':
        case L'A': {
            return 10;
        }
        case L'b':
        case L'B': {
            return 11;
        }
        case L'c':
        case L'C': {
            return 12;
        }
        case L'd':
        case L'D': {
            return 13;
        }
        case L'e':
        case L'E': {
            return 14;
        }
        case L'f':
        case L'F': {
            return 15;
        }
        default: { return -1; }
    }
}

static int octal_to_bin(wchar_t c) {
    switch (c) {
        case L'0': {
            return 0;
        }
        case L'1': {
            return 1;
        }
        case L'2': {
            return 2;
        }
        case L'3': {
            return 3;
        }
        case L'4': {
            return 4;
        }
        case L'5': {
            return 5;
        }
        case L'6': {
            return 6;
        }
        case L'7': {
            return 7;
        }
        default: { return -1; }
    }
}

double C_STRTOD(wchar_t const *nptr, wchar_t **endptr) {
    double r;

    const wcstring saved_locale = wsetlocale(LC_NUMERIC, NULL);

    if (!saved_locale.empty()) {
        wsetlocale(LC_NUMERIC, L"C");
    }

    r = wcstod(nptr, endptr);

    if (!saved_locale.empty()) {
        wsetlocale(LC_NUMERIC, saved_locale.c_str());
    }

    return r;
}

void builtin_printf_state_t::fatal_error(const wchar_t *fmt, ...) {
    // Don't error twice.
    if (early_exit) return;

    va_list va;
    va_start(va, fmt);
    wcstring errstr = vformat_string(fmt, va);
    va_end(va);
    streams.err.append(errstr);
    if (!string_suffixes_string(L"\n", errstr)) streams.err.push_back(L'\n');

    this->exit_code = STATUS_BUILTIN_ERROR;
    this->early_exit = true;
}

void builtin_printf_state_t::append_output(wchar_t c) {
    // Don't output if we're done.
    if (early_exit) return;

    streams.out.push_back(c);
}

void builtin_printf_state_t::append_output(const wchar_t *c) {
    // Don't output if we're done.
    if (early_exit) return;

    streams.out.append(c);
}

void builtin_printf_state_t::append_format_output(const wchar_t *fmt, ...) {
    // Don't output if we're done.
    if (early_exit) return;

    va_list va;
    va_start(va, fmt);
    wcstring tmp = vformat_string(fmt, va);
    va_end(va);
    streams.out.append(tmp);
}

void builtin_printf_state_t::verify_numeric(const wchar_t *s, const wchar_t *end, int errcode) {
    if (errcode != 0) {
        this->fatal_error(L"%ls: %s", s, strerror(errcode));
    } else if (*end) {
        if (s == end)
            this->fatal_error(_(L"%ls: expected a numeric value"), s);
        else
            this->fatal_error(_(L"%ls: value not completely converted"), s);
    }
}

template <typename T>
static T raw_string_to_scalar_type(const wchar_t *s, wchar_t **end);

// we use wcstoll instead of wcstoimax because FreeBSD 8 has busted wcstoumax and wcstoimax - see
// #626
template <>
intmax_t raw_string_to_scalar_type(const wchar_t *s, wchar_t **end) {
    return wcstoll(s, end, 0);
}

template <>
uintmax_t raw_string_to_scalar_type(const wchar_t *s, wchar_t **end) {
    return wcstoull(s, end, 0);
}

template <>
long double raw_string_to_scalar_type(const wchar_t *s, wchar_t **end) {
    return C_STRTOD(s, end);
}

template <typename T>
static T string_to_scalar_type(const wchar_t *s, builtin_printf_state_t *state) {
    T val;
    if (*s == L'\"' || *s == L'\'') {
        wchar_t ch = *++s;
        val = ch;
    } else {
        wchar_t *end = NULL;
        errno = 0;
        val = raw_string_to_scalar_type<T>(s, &end);
        state->verify_numeric(s, end, errno);
    }
    return val;
}

/// Output a single-character \ escape.
void builtin_printf_state_t::print_esc_char(wchar_t c) {
    switch (c) {
        case L'a': {  // alert
            this->append_output(L'\a');
            break;
        }
        case L'b': {  // backspace
            this->append_output(L'\b');
            break;
        }
        case L'c': {  // cancel the rest of the output
            this->early_exit = true;
            break;
        }
        case L'e': {  // escape
            this->append_output(L'\x1B');
            break;
        }
        case L'f': {  // form feed
            this->append_output(L'\f');
            break;
        }
        case L'n': {  // new line
            this->append_output(L'\n');
            break;
        }
        case L'r': {  // carriage return
            this->append_output(L'\r');
            break;
        }
        case L't': {  // horizontal tab
            this->append_output(L'\t');
            break;
        }
        case L'v': {  // vertical tab
            this->append_output(L'\v');
            break;
        }
        default: {
            this->append_output(c);
            break;
        }
    }
}

/// Print a \ escape sequence starting at ESCSTART.
/// Return the number of characters in the escape sequence besides the backslash..
/// If OCTAL_0 is nonzero, octal escapes are of the form \0ooo, where o
/// is an octal digit; otherwise they are of the form \ooo.
long builtin_printf_state_t::print_esc(const wchar_t *escstart, bool octal_0) {
    const wchar_t *p = escstart + 1;
    int esc_value = 0; /* Value of \nnn escape. */
    int esc_length;    /* Length of \nnn escape. */

    if (*p == L'x') {
        // A hexadecimal \xhh escape sequence must have 1 or 2 hex. digits.
        for (esc_length = 0, ++p; esc_length < 2 && is_hex_digit(*p); ++esc_length, ++p)
            esc_value = esc_value * 16 + hex_to_bin(*p);
        if (esc_length == 0) this->fatal_error(_(L"missing hexadecimal number in escape"));
        this->append_output(ENCODE_DIRECT_BASE + esc_value % 256);
    } else if (is_octal_digit(*p)) {
        // Parse \0ooo (if octal_0 && *p == L'0') or \ooo (otherwise). Allow \ooo if octal_0 && *p
        // != L'0'; this is an undocumented extension to POSIX that is compatible with Bash 2.05b.
        // Wrap mod 256, which matches historic behavior.
        for (esc_length = 0, p += octal_0 && *p == L'0'; esc_length < 3 && is_octal_digit(*p);
             ++esc_length, ++p)
            esc_value = esc_value * 8 + octal_to_bin(*p);
        this->append_output(ENCODE_DIRECT_BASE + esc_value % 256);
    } else if (*p && wcschr(L"\"\\abcefnrtv", *p)) {
        print_esc_char(*p++);
    } else if (*p == L'u' || *p == L'U') {
        wchar_t esc_char = *p;
        p++;
        uint32_t uni_value = 0;
        for (size_t esc_length = 0; esc_length < (esc_char == L'u' ? 4 : 8); esc_length++) {
            if (!is_hex_digit(*p)) {
                // Escape sequence must be done. Complain if we didn't get anything.
                if (esc_length == 0) {
                    this->fatal_error(_(L"Missing hexadecimal number in Unicode escape"));
                }
                break;
            }
            uni_value = uni_value * 16 + hex_to_bin(*p);
            p++;
        }

        // PCA GNU printf respects the limitations described in ISO N717, about which universal
        // characters "shall not" be specified. I believe this limitation is for the benefit of
        // compilers; I see no reason to impose it in builtin_printf.
        //
        // If __STDC_ISO_10646__ is defined, then it means wchar_t can and does hold Unicode code
        // points, so just use that. If not defined, use the %lc printf conversion; this probably
        // won't do anything good if your wide character set is not Unicode, but such platforms are
        // exceedingly rare.
        if (uni_value > 0x10FFFF) {
            this->fatal_error(_(L"Unicode character out of range: \\%c%0*x"), esc_char,
                              (esc_char == L'u' ? 4 : 8), uni_value);
        } else {
#if defined(__STDC_ISO_10646__)
            this->append_output(uni_value);
#else
            this->append_format_output(L"%lc", uni_value);
#endif
        }
    } else {
        this->append_output(L'\\');
        if (*p) {
            this->append_output(*p);
            p++;
        }
    }
    return p - escstart - 1;
}

/// Print string STR, evaluating \ escapes.
void builtin_printf_state_t::print_esc_string(const wchar_t *str) {
    for (; *str; str++)
        if (*str == L'\\')
            str += print_esc(str, true);
        else
            this->append_output(*str);
}

/// Evaluate a printf conversion specification.  START is the start of the directive, LENGTH is its
/// length, and CONVERSION specifies the type of conversion.  LENGTH does not include any length
/// modifier or the conversion specifier itself.  FIELD_WIDTH and PRECISION are the field width and
/// precision for '*' values, if HAVE_FIELD_WIDTH and HAVE_PRECISION are true, respectively.
/// ARGUMENT is the argument to be formatted.
void builtin_printf_state_t::print_direc(const wchar_t *start, size_t length, wchar_t conversion,
                                         bool have_field_width, int field_width,
                                         bool have_precision, int precision,
                                         wchar_t const *argument) {
    // Start with everything except the conversion specifier.
    wcstring fmt(start, length);

    // Create a copy of the % directive, with an intmax_t-wide width modifier substituted for any
    // existing integer length modifier.
    switch (conversion) {
        case L'd':
        case L'i':
        case L'u': {
            fmt.append(L"ll");
            break;
        }
        case L'a':
        case L'e':
        case L'f':
        case L'g':
        case L'A':
        case L'E':
        case L'F':
        case L'G': {
            fmt.append(L"L");
            break;
        }
        case L's':
        case L'c': {
            fmt.append(L"l");
            break;
        }
        default: { break; }
    }

    // Append the conversion itself.
    fmt.push_back(conversion);

    switch (conversion) {
        case L'd':
        case L'i': {
            intmax_t arg = string_to_scalar_type<intmax_t>(argument, this);
            if (!have_field_width) {
                if (!have_precision)
                    this->append_format_output(fmt.c_str(), arg);
                else
                    this->append_format_output(fmt.c_str(), precision, arg);
            } else {
                if (!have_precision)
                    this->append_format_output(fmt.c_str(), field_width, arg);
                else
                    this->append_format_output(fmt.c_str(), field_width, precision, arg);
            }
            break;
        }
        case L'o':
        case L'u':
        case L'x':
        case L'X': {
            uintmax_t arg = string_to_scalar_type<uintmax_t>(argument, this);
            if (!have_field_width) {
                if (!have_precision)
                    this->append_format_output(fmt.c_str(), arg);
                else
                    this->append_format_output(fmt.c_str(), precision, arg);
            } else {
                if (!have_precision)
                    this->append_format_output(fmt.c_str(), field_width, arg);
                else
                    this->append_format_output(fmt.c_str(), field_width, precision, arg);
            }
            break;
        }
        case L'a':
        case L'A':
        case L'e':
        case L'E':
        case L'f':
        case L'F':
        case L'g':
        case L'G': {
            long double arg = string_to_scalar_type<long double>(argument, this);
            if (!have_field_width) {
                if (!have_precision)
                    this->append_format_output(fmt.c_str(), arg);
                else
                    this->append_format_output(fmt.c_str(), precision, arg);
            } else {
                if (!have_precision)
                    this->append_format_output(fmt.c_str(), field_width, arg);
                else
                    this->append_format_output(fmt.c_str(), field_width, precision, arg);
            }
            break;
        }
        case L'c': {
            if (!have_field_width)
                this->append_format_output(fmt.c_str(), *argument);
            else
                this->append_format_output(fmt.c_str(), field_width, *argument);
            break;
        }
        case L's': {
            if (!have_field_width) {
                if (!have_precision) {
                    this->append_format_output(fmt.c_str(), argument);
                } else
                    this->append_format_output(fmt.c_str(), precision, argument);
            } else {
                if (!have_precision)
                    this->append_format_output(fmt.c_str(), field_width, argument);
                else
                    this->append_format_output(fmt.c_str(), field_width, precision, argument);
            }
            break;
        }
    }
}

/// For each character in str, set the corresponding boolean in the array to the given flag.
static inline void modify_allowed_format_specifiers(bool ok[UCHAR_MAX + 1], const char *str,
                                                    bool flag) {
    for (const char *c = str; *c != '\0'; c++) {
        unsigned char idx = static_cast<unsigned char>(*c);
        ok[idx] = flag;
    }
}

/// Print the text in FORMAT, using ARGV (with ARGC elements) for arguments to any `%' directives.
/// Return the number of elements of ARGV used.
int builtin_printf_state_t::print_formatted(const wchar_t *format, int argc, wchar_t **argv) {
    int save_argc = argc;        /* Preserve original value.  */
    const wchar_t *f;            /* Pointer into `format'.  */
    const wchar_t *direc_start;  /* Start of % directive.  */
    size_t direc_length;         /* Length of % directive.  */
    bool have_field_width;       /* True if FIELD_WIDTH is valid.  */
    int field_width = 0;         /* Arg to first '*'.  */
    bool have_precision;         /* True if PRECISION is valid.  */
    int precision = 0;           /* Arg to second '*'.  */
    bool ok[UCHAR_MAX + 1] = {}; /* ok['x'] is true if %x is allowed.  */

    for (f = format; *f != L'\0'; ++f) {
        switch (*f) {
            case L'%': {
                direc_start = f++;
                direc_length = 1;
                have_field_width = have_precision = false;
                if (*f == L'%') {
                    this->append_output(L'%');
                    break;
                }
                if (*f == L'b') {
                    // FIXME: Field width and precision are not supported for %b, even though POSIX
                    // requires it.
                    if (argc > 0) {
                        print_esc_string(*argv);
                        ++argv;
                        --argc;
                    }
                    break;
                }

                modify_allowed_format_specifiers(ok, "aAcdeEfFgGiosuxX", true);

                for (;; f++, direc_length++) {
                    switch (*f) {
                        case L'I':
                        case L'\'': {
                            modify_allowed_format_specifiers(ok, "aAceEosxX", false);
                            break;
                        }
                        case '-':
                        case '+':
                        case ' ': {
                            break;
                        }
                        case L'#': {
                            modify_allowed_format_specifiers(ok, "cdisu", false);
                            break;
                        }
                        case '0': {
                            modify_allowed_format_specifiers(ok, "cs", false);
                            break;
                        }
                        default: { goto no_more_flag_characters; }
                    }
                }
            no_more_flag_characters:;

                if (*f == L'*') {
                    ++f;
                    ++direc_length;
                    if (argc > 0) {
                        intmax_t width = string_to_scalar_type<intmax_t>(*argv, this);
                        if (INT_MIN <= width && width <= INT_MAX)
                            field_width = static_cast<int>(width);
                        else
                            this->fatal_error(_(L"invalid field width: %ls"), *argv);
                        ++argv;
                        --argc;
                    } else {
                        field_width = 0;
                    }
                    have_field_width = true;
                } else {
                    while (iswdigit(*f)) {
                        ++f;
                        ++direc_length;
                    }
                }
                if (*f == L'.') {
                    ++f;
                    ++direc_length;
                    modify_allowed_format_specifiers(ok, "c", false);
                    if (*f == L'*') {
                        ++f;
                        ++direc_length;
                        if (argc > 0) {
                            intmax_t prec = string_to_scalar_type<intmax_t>(*argv, this);
                            if (prec < 0) {
                                // A negative precision is taken as if the precision were omitted,
                                // so -1 is safe here even if prec < INT_MIN.
                                precision = -1;
                            } else if (INT_MAX < prec)
                                this->fatal_error(_(L"invalid precision: %ls"), *argv);
                            else {
                                precision = static_cast<int>(prec);
                            }
                            ++argv;
                            --argc;
                        } else {
                            precision = 0;
                        }
                        have_precision = true;
                    } else {
                        while (iswdigit(*f)) {
                            ++f;
                            ++direc_length;
                        }
                    }
                }

                while (*f == L'l' || *f == L'L' || *f == L'h' || *f == L'j' || *f == L't' ||
                       *f == L'z') {
                    ++f;
                }

                wchar_t conversion = *f;
                if (conversion > 0xFF || !ok[conversion]) {
                    this->fatal_error(_(L"%.*ls: invalid conversion specification"),
                                      (int)(f + 1 - direc_start), direc_start);
                    return 0;
                }

                print_direc(direc_start, direc_length, *f, have_field_width, field_width,
                            have_precision, precision, (argc <= 0 ? L"" : (argc--, *argv++)));
                break;
            }
            case L'\\': {
                f += print_esc(f, false);
                break;
            }
            default: { this->append_output(*f); }
        }
    }
    return save_argc - argc;
}

/// The printf builtin.
int builtin_printf(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    builtin_printf_state_t state(streams);

    wchar_t *format;
    int args_used;
    int argc = builtin_count_args(argv);

    if (argc <= 1) {
        state.fatal_error(_(L"printf: not enough arguments"));
        return STATUS_BUILTIN_ERROR;
    }

    format = argv[1];
    argc -= 2;
    argv += 2;

    do {
        args_used = state.print_formatted(format, argc, argv);
        argc -= args_used;
        argv += args_used;
    } while (args_used > 0 && argc > 0 && !state.early_exit);
    return state.exit_code;
}
