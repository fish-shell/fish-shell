// Implementation of the echo builtin.
#include "config.h"  // IWYU pragma: keep

#include "echo.h"

#include <climits>
#include <cstddef>

#include "../builtin.h"
#include "../common.h"
#include "../fallback.h"  // IWYU pragma: keep
#include "../io.h"
#include "../wgetopt.h"
#include "../wutil.h"  // IWYU pragma: keep

struct echo_cmd_opts_t {
    bool print_newline = true;
    bool print_spaces = true;
    bool interpret_special_chars = false;
};
static const wchar_t *const short_options = L"+:Eens";
static const struct woption *const long_options = nullptr;

static int parse_cmd_opts(echo_cmd_opts_t &opts, int *optind, int argc, const wchar_t **argv,
                          parser_t &parser, io_streams_t &streams) {
    UNUSED(parser);
    UNUSED(streams);
    const wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    echo_cmd_opts_t oldopts = opts;
    int oldoptind = 0;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
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
                opts = oldopts;
                *optind = w.woptind - 1;
                return STATUS_CMD_OK;
            }
            default: {
                DIE("unexpected retval from wgetopt_long");
            }
        }

        // Super cheesy: We keep an old copy of the option state around,
        // so we can revert it in case we get an argument like
        // "-n foo".
        // We need to keep it one out-of-date so we can ignore the *last* option.
        // (this might be an issue in wgetopt, but that's a whole other can of worms
        //  and really only occurs with our weird "put it back" option parsing)
        if (w.woptind == oldoptind + 2) {
            oldopts = opts;
            oldoptind = w.woptind;
        }
    }

    *optind = w.woptind;
    return STATUS_CMD_OK;
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
    if (convert_digit(str[0], 8) != -1) {
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
        int digit = convert_digit(str[idx], base);
        if (digit == -1) break;
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
maybe_t<int> builtin_echo(parser_t &parser, io_streams_t &streams, const wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    UNUSED(cmd);
    int argc = builtin_count_args(argv);
    echo_cmd_opts_t opts;
    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    // The special character \c can be used to indicate no more output.
    bool continue_output = true;

    const wchar_t *const *args_to_echo = argv + optind;
    // We buffer output so we can write in one go,
    // this matters when writing to an fd.
    wcstring out;
    for (size_t idx = 0; continue_output && args_to_echo[idx] != nullptr; idx++) {
        if (opts.print_spaces && idx > 0) {
            out.push_back(' ');
        }

        const wchar_t *str = args_to_echo[idx];
        for (size_t j = 0; continue_output && str[j]; j++) {
            if (!opts.interpret_special_chars || str[j] != L'\\') {
                // Not an escape.
                out.push_back(str[j]);
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
                    out.push_back(wc);
                }
            }
        }
    }
    if (opts.print_newline && continue_output) {
        out.push_back('\n');
    }

    if (!out.empty()) {
        streams.out.append(out);
    }

    return STATUS_CMD_OK;
}
