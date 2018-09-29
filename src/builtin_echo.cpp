// Implementation of the echo builtin.
#include "config.h"  // IWYU pragma: keep

#include <limits.h>
#include <stddef.h>

#include "builtin.h"
#include "builtin_echo.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

struct echo_cmd_opts_t {
    bool print_newline = true;
    bool print_spaces = true;
    bool interpret_special_chars = false;
};
static const wchar_t *const short_options = L"+:Eens";
static const struct woption *long_options = NULL;

static int parse_cmd_opts(echo_cmd_opts_t &opts, int *optind, int argc, wchar_t **argv,
                          parser_t &parser, io_streams_t &streams) {
    UNUSED(parser);
    UNUSED(streams);
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 'n': {
                opts.print_newline = false;
                break;
            }
            case 'e': {
                opts.interpret_special_chars = true;
                break;
            }
            case 's': {
                opts.print_spaces = false;
                break;
            }
            case 'E': {
                opts.interpret_special_chars = false;
                break;
            }
            case ':': {
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            case '?': {
                *optind = w.woptind - 1;
                return STATUS_CMD_OK;
            }
            default: {
                DIE("unexpected retval from wgetopt_long");
                break;
            }
        }
    }

    *optind = w.woptind;
    return STATUS_CMD_OK;
}

// Convert a octal or hex character to its binary value. Surprisingly a version
// of this function using a lookup table is only ~1.5% faster than the `switch`
// statement version below. Since that requires initializing a table statically
// (which is problematic if we run on an EBCDIC system) we don't use that
// solution. Also, we relax the style rule that `case` blocks should always be
// enclosed in parentheses given the nature of this code.
static unsigned int builtin_echo_digit(wchar_t wc, unsigned int base) {
    assert(base == 8 || base == 16);  // base must be hex or octal
    switch (wc) {
        case L'0':
            return 0;
        case L'1':
            return 1;
        case L'2':
            return 2;
        case L'3':
            return 3;
        case L'4':
            return 4;
        case L'5':
            return 5;
        case L'6':
            return 6;
        case L'7':
            return 7;
        default: { break; }
    }

    if (base != 16) return UINT_MAX;

    switch (wc) {
        case L'8':
            return 8;
        case L'9':
            return 9;
        case L'a':
        case L'A':
            return 10;
        case L'b':
        case L'B':
            return 11;
        case L'c':
        case L'C':
            return 12;
        case L'd':
        case L'D':
            return 13;
        case L'e':
        case L'E':
            return 14;
        case L'f':
        case L'F':
            return 15;
        default: { break; }
    }

    return UINT_MAX;
}

/// Parse a numeric escape sequence in str, returning whether we succeeded. Also return the number
/// of characters consumed and the resulting value. Supported escape sequences:
///
/// \0nnn: octal value, zero to three digits
/// \nnn: octal value, one to three digits
/// \xhh: hex value, one to two digits
static bool builtin_echo_parse_numeric_sequence(const wchar_t *str, size_t *consumed,
                                                unsigned char *out_val) {
    bool success = false;
    unsigned int start = 0;  // the first character of the numeric part of the sequence

    unsigned int base = 0, max_digits = 0;
    if (builtin_echo_digit(str[0], 8) != UINT_MAX) {
        // Octal escape
        base = 8;

        // If the first digit is a 0, we allow four digits (including that zero); otherwise, we
        // allow 3.
        max_digits = (str[0] == L'0' ? 4 : 3);
    } else if (str[0] == L'x') {
        // Hex escape
        base = 16;
        max_digits = 2;

        // Skip the x
        start = 1;
    }

    if (base == 0) {
        return success;
    }

    unsigned int idx;
    unsigned char val = 0;  // resulting character
    for (idx = start; idx < start + max_digits; idx++) {
        unsigned int digit = builtin_echo_digit(str[idx], base);
        if (digit == UINT_MAX) break;
        val = val * base + digit;
    }

    // We succeeded if we consumed at least one digit.
    if (idx > start) {
        *consumed = idx;
        *out_val = val;
        success = true;
    }
    return success;
}

/// The echo builtin.
///
/// Bash only respects -n if it's the first argument. We'll do the same. We also support a new,
/// fish specific, option -s to mean "no spaces".
int builtin_echo(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    UNUSED(cmd);
    int argc = builtin_count_args(argv);
    echo_cmd_opts_t opts;
    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    // The special character \c can be used to indicate no more output.
    bool continue_output = true;

    const wchar_t *const *args_to_echo = argv + optind;
    for (size_t idx = 0; continue_output && args_to_echo[idx] != NULL; idx++) {
        if (opts.print_spaces && idx > 0) {
            streams.out.push_back(' ');
        }

        const wchar_t *str = args_to_echo[idx];
        for (size_t j = 0; continue_output && str[j]; j++) {
            if (!opts.interpret_special_chars || str[j] != L'\\') {
                // Not an escape.
                streams.out.push_back(str[j]);
            } else {
                // Most escapes consume one character in addition to the backslash; the numeric
                // sequences may consume more, while an unrecognized escape sequence consumes none.
                wchar_t wc;
                size_t consumed = 1;
                switch (str[j + 1]) {
                    case L'a': {
                        wc = L'\a';
                        break;
                    }
                    case L'b': {
                        wc = L'\b';
                        break;
                    }
                    case L'e': {
                        wc = L'\x1B';
                        break;
                    }
                    case L'f': {
                        wc = L'\f';
                        break;
                    }
                    case L'n': {
                        wc = L'\n';
                        break;
                    }
                    case L'r': {
                        wc = L'\r';
                        break;
                    }
                    case L't': {
                        wc = L'\t';
                        break;
                    }
                    case L'v': {
                        wc = L'\v';
                        break;
                    }
                    case L'\\': {
                        wc = L'\\';
                        break;
                    }
                    case L'c': {
                        wc = 0;
                        continue_output = false;
                        break;
                    }
                    default: {
                        // Octal and hex escape sequences.
                        unsigned char narrow_val = 0;
                        if (builtin_echo_parse_numeric_sequence(str + j + 1, &consumed,
                                                                &narrow_val)) {
                            // Here consumed must have been set to something. The narrow_val is a
                            // literal byte that we want to output (#1894).
                            wc = ENCODE_DIRECT_BASE + narrow_val % 256;
                        } else {
                            // Not a recognized escape. We consume only the backslash.
                            wc = L'\\';
                            consumed = 0;
                        }
                        break;
                    }
                }

                // Skip over characters that were part of this escape sequence (but not the
                // backslash, which will be handled by the loop increment.
                j += consumed;

                if (continue_output) {
                    streams.out.push_back(wc);
                }
            }
        }
    }
    if (opts.print_newline && continue_output) {
        streams.out.push_back('\n');
    }
    return STATUS_CMD_OK;
}
