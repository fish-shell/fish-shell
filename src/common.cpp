// Various functions, mostly string utilities, that are used by most parts of fish.
#include "config.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif
#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <algorithm>
#include <memory>  // IWYU pragma: keep

#include "common.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "wildcard.h"
#include "wutil.h"  // IWYU pragma: keep

#define NOT_A_WCHAR (static_cast<wint_t>(WEOF))

struct termios shell_modes;

// Note we foolishly assume that pthread_t is just a primitive. But it might be a struct.
static pthread_t main_thread_id = 0;
static bool thread_assertions_configured_for_testing = false;

wchar_t ellipsis_char;
wchar_t omitted_newline_char;

bool g_profiling_active = false;

const wchar_t *program_name;

int debug_level = 1;

/// This struct maintains the current state of the terminal size. It is updated on demand after
/// receiving a SIGWINCH. Do not touch this struct directly, it's managed with a rwlock. Use
/// common_get_width()/common_get_height().
static struct winsize termsize;
static volatile bool termsize_valid;
static rwlock_t termsize_rwlock;

static char *wcs2str_internal(const wchar_t *in, char *out);

void show_stackframe() {
    ASSERT_IS_NOT_FORKED_CHILD();

    // Hack to avoid showing backtraces in the tester.
    if (program_name && !wcscmp(program_name, L"(ignore)")) return;

    void *trace[32];
    int trace_size = 0;

    trace_size = backtrace(trace, 32);
    debug(0, L"Backtrace:");
    backtrace_symbols_fd(trace, trace_size, STDERR_FILENO);
}

int fgetws2(wcstring *s, FILE *f) {
    int i = 0;
    wint_t c;

    while (1) {
        errno = 0;

        c = fgetwc(f);
        if (errno == EILSEQ || errno == EINTR) {
            continue;
        }

        switch (c) {
            // End of line.
            case WEOF:
            case L'\n':
            case L'\0': {
                return i;
            }
            // Ignore carriage returns.
            case L'\r': {
                break;
            }
            default: {
                i++;
                s->push_back((wchar_t)c);
                break;
            }
        }
    }
}

/// Converts the narrow character string \c in into its wide equivalent, and return it.
///
/// The string may contain embedded nulls.
///
/// This function encodes illegal character sequences in a reversible way using the private use
/// area.
static wcstring str2wcs_internal(const char *in, const size_t in_len) {
    if (in_len == 0) return wcstring();
    assert(in != NULL);

    wcstring result;
    result.reserve(in_len);
    size_t in_pos = 0;

    if (MB_CUR_MAX == 1)  // single-byte locale, all values are legal
    {
        while (in_pos < in_len) {
            result.push_back((unsigned char)in[in_pos]);
            in_pos++;
        }
        return result;
    }

    mbstate_t state = {};
    while (in_pos < in_len) {
        wchar_t wc = 0;
        size_t ret = mbrtowc(&wc, &in[in_pos], in_len - in_pos, &state);

        // Determine whether to encode this characters with our crazy scheme.
        bool use_encode_direct = false;
        if (wc >= ENCODE_DIRECT_BASE && wc < ENCODE_DIRECT_BASE + 256) {
            use_encode_direct = true;
        } else if (wc == INTERNAL_SEPARATOR) {
            use_encode_direct = true;
        } else if (ret == (size_t)-2) {
            // Incomplete sequence.
            use_encode_direct = true;
        } else if (ret == (size_t)-1) {
            // Invalid data.
            use_encode_direct = true;
        } else if (ret > in_len - in_pos) {
            // Other error codes? Terrifying, should never happen.
            use_encode_direct = true;
        }

        if (use_encode_direct) {
            wc = ENCODE_DIRECT_BASE + (unsigned char)in[in_pos];
            result.push_back(wc);
            in_pos++;
            memset(&state, 0, sizeof state);
        } else if (ret == 0) {
            // Embedded null byte!
            result.push_back(L'\0');
            in_pos++;
            memset(&state, 0, sizeof state);
        } else {
            // Normal case.
            result.push_back(wc);
            in_pos += ret;
        }
    }
    return result;
}

wcstring str2wcstring(const char *in, size_t len) { return str2wcs_internal(in, len); }

wcstring str2wcstring(const char *in) { return str2wcs_internal(in, strlen(in)); }

wcstring str2wcstring(const std::string &in) {
    /* Handles embedded nulls! */
    return str2wcs_internal(in.data(), in.size());
}

char *wcs2str(const wchar_t *in) {
    if (!in) return NULL;
    size_t desired_size = MAX_UTF8_BYTES * wcslen(in) + 1;
    char local_buff[512];
    if (desired_size <= sizeof local_buff / sizeof *local_buff) {
        // Convert into local buff, then use strdup() so we don't waste malloc'd space.
        char *result = wcs2str_internal(in, local_buff);
        if (result) {
            // It converted into the local buffer, so copy it.
            result = strdup(result);
            if (!result) {
                DIE_MEM();
            }
        }
        return result;

    } else {
        // Here we probably allocate a buffer probably much larger than necessary.
        char *out = (char *)malloc(MAX_UTF8_BYTES * wcslen(in) + 1);
        if (!out) {
            DIE_MEM();
        }
        return wcs2str_internal(in, out);
    }
}

char *wcs2str(const wcstring &in) { return wcs2str(in.c_str()); }

/// This function is distinguished from wcs2str_internal in that it allows embedded null bytes.
std::string wcs2string(const wcstring &input) {
    std::string result;
    result.reserve(input.size());

    mbstate_t state = {};
    char converted[MB_LEN_MAX + 1];

    for (size_t i = 0; i < input.size(); i++) {
        wchar_t wc = input[i];
        if (wc == INTERNAL_SEPARATOR) {
            // Do nothing.
        } else if (wc >= ENCODE_DIRECT_BASE && wc < ENCODE_DIRECT_BASE + 256) {
            result.push_back(wc - ENCODE_DIRECT_BASE);
        } else if (MB_CUR_MAX == 1)  // single-byte locale (C/POSIX/ISO-8859)
        {
            // If `wc` contains a wide character we emit a question-mark.
            if (wc & ~0xFF) {
                wc = '?';
            }
            converted[0] = wc;
            result.append(converted, 1);
        } else {
            memset(converted, 0, sizeof converted);
            size_t len = wcrtomb(converted, wc, &state);
            if (len == (size_t)(-1)) {
                debug(1, L"Wide character %d has no narrow representation", wc);
                memset(&state, 0, sizeof(state));
            } else {
                result.append(converted, len);
            }
        }
    }

    return result;
}

/// Converts the wide character string \c in into it's narrow equivalent, stored in \c out. \c out
/// must have enough space to fit the entire string.
///
/// This function decodes illegal character sequences in a reversible way using the private use
/// area.
static char *wcs2str_internal(const wchar_t *in, char *out) {
    CHECK(in, 0);
    CHECK(out, 0);

    size_t in_pos = 0;
    size_t out_pos = 0;
    mbstate_t state = {};

    while (in[in_pos]) {
        if (in[in_pos] == INTERNAL_SEPARATOR) {
            // Do nothing.
        } else if (in[in_pos] >= ENCODE_DIRECT_BASE && in[in_pos] < ENCODE_DIRECT_BASE + 256) {
            out[out_pos++] = in[in_pos] - ENCODE_DIRECT_BASE;
        } else if (MB_CUR_MAX == 1)  // single-byte locale (C/POSIX/ISO-8859)
        {
            // If `wc` contains a wide character we emit a question-mark.
            if (in[in_pos] & ~0xFF) {
                out[out_pos++] = '?';
            } else {
                out[out_pos++] = (unsigned char)in[in_pos];
            }
        } else {
            size_t len = wcrtomb(&out[out_pos], in[in_pos], &state);
            if (len == (size_t)-1) {
                debug(1, L"Wide character %d has no narrow representation", in[in_pos]);
                memset(&state, 0, sizeof(state));
            } else {
                out_pos += len;
            }
        }
        in_pos++;
    }
    out[out_pos] = 0;

    return out;
}

wcstring format_string(const wchar_t *format, ...) {
    va_list va;
    va_start(va, format);
    wcstring result = vformat_string(format, va);
    va_end(va);
    return result;
}

void append_formatv(wcstring &target, const wchar_t *format, va_list va_orig) {
    const int saved_err = errno;
    // As far as I know, there is no way to check if a vswprintf-call failed because of a badly
    // formated string option or because the supplied destination string was to small. In GLIBC,
    // errno seems to be set to EINVAL either way.
    //
    // Because of this, on failiure we try to increase the buffer size until the free space is
    // larger than max_size, at which point it will conclude that the error was probably due to a
    // badly formated string option, and return an error. Make sure to null terminate string before
    // that, though.
    const size_t max_size = (128 * 1024 * 1024);
    wchar_t static_buff[256];
    size_t size = 0;
    wchar_t *buff = NULL;
    int status = -1;
    while (status < 0) {
        // Reallocate if necessary.
        if (size == 0) {
            buff = static_buff;
            size = sizeof static_buff;
        } else {
            size *= 2;
            if (size >= max_size) {
                buff[0] = '\0';
                break;
            }
            buff = (wchar_t *)realloc((buff == static_buff ? NULL : buff), size);
            if (buff == NULL) {
                DIE_MEM();
            }
        }

        // Try printing.
        va_list va;
        va_copy(va, va_orig);
        status = vswprintf(buff, size / sizeof(wchar_t), format, va);
        va_end(va);
    }

    target.append(buff);

    if (buff != static_buff) {
        free(buff);
    }

    errno = saved_err;
}

wcstring vformat_string(const wchar_t *format, va_list va_orig) {
    wcstring result;
    append_formatv(result, format, va_orig);
    return result;
}

void append_format(wcstring &str, const wchar_t *format, ...) {
    va_list va;
    va_start(va, format);
    append_formatv(str, format, va);
    va_end(va);
}

const wchar_t *wcsvarname(const wchar_t *str) {
    while (*str) {
        if ((!iswalnum(*str)) && (*str != L'_')) {
            return str;
        }
        str++;
    }
    return NULL;
}

const wchar_t *wcsvarname(const wcstring &str) { return wcsvarname(str.c_str()); }

const wchar_t *wcsfuncname(const wcstring &str) { return wcschr(str.c_str(), L'/'); }

bool wcsvarchr(wchar_t chr) { return iswalnum(chr) || chr == L'_'; }

int fish_wcswidth(const wchar_t *str) { return fish_wcswidth(str, wcslen(str)); }

int fish_wcswidth(const wcstring &str) { return fish_wcswidth(str.c_str(), str.size()); }

wchar_t *quote_end(const wchar_t *pos) {
    wchar_t c = *pos;

    while (1) {
        pos++;

        if (!*pos) return 0;

        if (*pos == L'\\') {
            pos++;
            if (!*pos) return 0;
        } else {
            if (*pos == c) {
                return (wchar_t *)pos;
            }
        }
    }
    return 0;
}

wcstring wsetlocale(int category, const wchar_t *locale) {
    char *lang = locale ? wcs2str(locale) : NULL;
    char *res = setlocale(category, lang);
    free(lang);

    // Use ellipsis if on known unicode system, otherwise use $.
    ellipsis_char = (wcwidth(L'\x2026') > 0) ? L'\x2026' : L'$';

    // U+23CE is the "return" character
    omitted_newline_char = (wcwidth(L'\x23CE') > 0) ? L'\x23CE' : L'~';

    if (!res)
        return wcstring();
    else
        return format_string(L"%s", res);
}

bool contains_internal(const wchar_t *a, int vararg_handle, ...) {
    const wchar_t *arg;
    va_list va;
    bool res = false;

    CHECK(a, 0);

    va_start(va, vararg_handle);
    while ((arg = va_arg(va, const wchar_t *)) != 0) {
        if (wcscmp(a, arg) == 0) {
            res = true;
            break;
        }
    }
    va_end(va);
    return res;
}

/// wcstring variant of contains_internal. The first parameter is a wcstring, the rest are const
/// wchar_t *. vararg_handle exists only to give us a POD-value to pass to va_start.
__sentinel bool contains_internal(const wcstring &needle, int vararg_handle, ...) {
    const wchar_t *arg;
    va_list va;
    int res = 0;

    const wchar_t *needle_cstr = needle.c_str();
    va_start(va, vararg_handle);
    while ((arg = va_arg(va, const wchar_t *)) != 0) {
        // libc++ has an unfortunate implementation of operator== that unconditonally wcslen's the
        // wchar_t* parameter, so prefer wcscmp directly.
        if (!wcscmp(needle_cstr, arg)) {
            res = 1;
            break;
        }
    }
    va_end(va);
    return res;
}

long read_blocked(int fd, void *buf, size_t count) {
    ssize_t res;
    sigset_t chldset, oldset;

    sigemptyset(&chldset);
    sigaddset(&chldset, SIGCHLD);
    VOMIT_ON_FAILURE(pthread_sigmask(SIG_BLOCK, &chldset, &oldset));
    res = read(fd, buf, count);
    VOMIT_ON_FAILURE(pthread_sigmask(SIG_SETMASK, &oldset, NULL));
    return res;
}

ssize_t write_loop(int fd, const char *buff, size_t count) {
    size_t out_cum = 0;
    while (out_cum < count) {
        ssize_t out = write(fd, &buff[out_cum], count - out_cum);
        if (out < 0) {
            if (errno != EAGAIN && errno != EINTR) {
                return -1;
            }
        } else {
            out_cum += (size_t)out;
        }
    }
    return (ssize_t)out_cum;
}

ssize_t read_loop(int fd, void *buff, size_t count) {
    ssize_t result;
    do {
        result = read(fd, buff, count);
    } while (result < 0 && (errno == EAGAIN || errno == EINTR));
    return result;
}

static bool should_debug(int level) {
    if (level > debug_level) return false;

    // Hack to not print error messages in the tests.
    if (program_name && !wcscmp(program_name, L"(ignore)")) return false;

    return true;
}

static void debug_shared(const wcstring &msg) {
    const wcstring sb = wcstring(program_name) + L": " + msg;
    fwprintf(stderr, L"%ls", reformat_for_screen(sb).c_str());
}

void debug(int level, const wchar_t *msg, ...) {
    if (!should_debug(level)) return;
    int errno_old = errno;
    va_list va;
    va_start(va, msg);
    wcstring local_msg = vformat_string(msg, va);
    va_end(va);
    debug_shared(local_msg);
    errno = errno_old;
}

void debug(int level, const char *msg, ...) {
    if (!should_debug(level)) return;
    int errno_old = errno;
    char local_msg[512];
    va_list va;
    va_start(va, msg);
    vsnprintf(local_msg, sizeof local_msg, msg, va);
    va_end(va);
    debug_shared(str2wcstring(local_msg));
    errno = errno_old;
}

void read_ignore(int fd, void *buff, size_t count) {
    size_t ignore __attribute__((unused));
    ignore = read(fd, buff, count);
}

void write_ignore(int fd, const void *buff, size_t count) {
    size_t ignore __attribute__((unused));
    ignore = write(fd, buff, count);
}

void debug_safe(int level, const char *msg, const char *param1, const char *param2,
                const char *param3, const char *param4, const char *param5, const char *param6,
                const char *param7, const char *param8, const char *param9, const char *param10,
                const char *param11, const char *param12) {
    const char *const params[] = {param1, param2, param3, param4,  param5,  param6,
                                  param7, param8, param9, param10, param11, param12};
    if (!msg) return;

    // Can't call printf, that may allocate memory Just call write() over and over.
    if (level > debug_level) return;
    int errno_old = errno;

    size_t param_idx = 0;
    const char *cursor = msg;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, '%');
        if (end == NULL) end = cursor + strlen(cursor);

        write_ignore(STDERR_FILENO, cursor, end - cursor);

        if (end[0] == '%' && end[1] == 's') {
            // Handle a format string.
            assert(param_idx < sizeof params / sizeof *params);
            const char *format = params[param_idx++];
            if (!format) format = "(null)";
            write_ignore(STDERR_FILENO, format, strlen(format));
            cursor = end + 2;
        } else if (end[0] == '\0') {
            // Must be at the end of the string.
            cursor = end;
        } else {
            // Some other format specifier, just skip it.
            cursor = end + 1;
        }
    }

    // We always append a newline.
    write_ignore(STDERR_FILENO, "\n", 1);

    errno = errno_old;
}

void format_long_safe(char buff[64], long val) {
    if (val == 0) {
        strcpy(buff, "0");
    } else {
        // Generate the string in reverse.
        size_t idx = 0;
        bool negative = (val < 0);

        // Note that we can't just negate val if it's negative, because it may be the most negative
        // value. We do rely on round-towards-zero division though.
        while (val != 0) {
            long rem = val % 10;
            buff[idx++] = '0' + (rem < 0 ? -rem : rem);
            val /= 10;
        }
        if (negative) buff[idx++] = '-';
        buff[idx] = 0;

        size_t left = 0, right = idx - 1;
        while (left < right) {
            char tmp = buff[left];
            buff[left++] = buff[right];
            buff[right--] = tmp;
        }
    }
}

void format_long_safe(wchar_t buff[64], long val) {
    if (val == 0) {
        wcscpy(buff, L"0");
    } else {
        // Generate the string in reverse.
        size_t idx = 0;
        bool negative = (val < 0);

        while (val != 0) {
            long rem = val % 10;
            buff[idx++] = L'0' + (wchar_t)(rem < 0 ? -rem : rem);
            val /= 10;
        }
        if (negative) buff[idx++] = L'-';
        buff[idx] = 0;

        size_t left = 0, right = idx - 1;
        while (left < right) {
            wchar_t tmp = buff[left];
            buff[left++] = buff[right];
            buff[right--] = tmp;
        }
    }
}

void narrow_string_safe(char buff[64], const wchar_t *s) {
    size_t idx = 0;
    for (size_t widx = 0; s[widx] != L'\0'; widx++) {
        wchar_t c = s[widx];
        if (c <= 127) {
            buff[idx++] = char(c);
            if (idx + 1 == 64) {
                break;
            }
        }
    }
    buff[idx] = '\0';
}

wcstring reformat_for_screen(const wcstring &msg) {
    wcstring buff;
    int line_width = 0;
    int screen_width = common_get_width();

    if (screen_width) {
        const wchar_t *start = msg.c_str();
        const wchar_t *pos = start;
        while (1) {
            int overflow = 0;

            int tok_width = 0;

            // Tokenize on whitespace, and also calculate the width of the token.
            while (*pos && (!wcschr(L" \n\r\t", *pos))) {
                // Check is token is wider than one line. If so we mark it as an overflow and break
                // the token.
                if ((tok_width + fish_wcwidth(*pos)) > (screen_width - 1)) {
                    overflow = 1;
                    break;
                }

                tok_width += fish_wcwidth(*pos);
                pos++;
            }

            // If token is zero character long, we don't do anything.
            if (pos == start) {
                start = pos = pos + 1;
            } else if (overflow) {
                // In case of overflow, we print a newline, except if we already are at position 0.
                wchar_t *token = wcsndup(start, pos - start);
                if (line_width != 0) buff.push_back(L'\n');
                buff.append(format_string(L"%ls-\n", token));
                free(token);
                line_width = 0;
            } else {
                // Print the token.
                wchar_t *token = wcsndup(start, pos - start);
                if ((line_width + (line_width != 0 ? 1 : 0) + tok_width) > screen_width) {
                    buff.push_back(L'\n');
                    line_width = 0;
                }
                buff.append(format_string(L"%ls%ls", line_width ? L" " : L"", token));
                free(token);
                line_width += (line_width != 0 ? 1 : 0) + tok_width;
            }

            // Break on end of string.
            if (!*pos) {
                break;
            }

            start = pos;
        }
    } else {
        buff.append(msg);
    }
    buff.push_back(L'\n');
    return buff;
}

/// Escape a string, storing the result in out_str.
static void escape_string_internal(const wchar_t *orig_in, size_t in_len, wcstring *out_str,
                                   escape_flags_t flags) {
    assert(orig_in != NULL);

    const wchar_t *in = orig_in;
    bool escape_all = !!(flags & ESCAPE_ALL);
    bool no_quoted = !!(flags & ESCAPE_NO_QUOTED);
    bool no_tilde = !!(flags & ESCAPE_NO_TILDE);

    int need_escape = 0;
    int need_complex_escape = 0;

    // Avoid dereferencing all over the place.
    wcstring &out = *out_str;

    if (!no_quoted && in_len == 0) {
        out.assign(L"''");
        return;
    }

    while (*in != 0) {
        if ((*in >= ENCODE_DIRECT_BASE) && (*in < ENCODE_DIRECT_BASE + 256)) {
            int val = *in - ENCODE_DIRECT_BASE;
            int tmp;

            out += L'\\';
            out += L'X';

            tmp = val / 16;
            out += tmp > 9 ? L'a' + (tmp - 10) : L'0' + tmp;

            tmp = val % 16;
            out += tmp > 9 ? L'a' + (tmp - 10) : L'0' + tmp;
            need_escape = need_complex_escape = 1;

        } else {
            wchar_t c = *in;
            switch (c) {
                case L'\t': {
                    out += L'\\';
                    out += L't';
                    need_escape = need_complex_escape = 1;
                    break;
                }
                case L'\n': {
                    out += L'\\';
                    out += L'n';
                    need_escape = need_complex_escape = 1;
                    break;
                }
                case L'\b': {
                    out += L'\\';
                    out += L'b';
                    need_escape = need_complex_escape = 1;
                    break;
                }
                case L'\r': {
                    out += L'\\';
                    out += L'r';
                    need_escape = need_complex_escape = 1;
                    break;
                }
                case L'\x1b': {
                    out += L'\\';
                    out += L'e';
                    need_escape = need_complex_escape = 1;
                    break;
                }
                case L'\\':
                case L'\'': {
                    need_escape = need_complex_escape = 1;
                    if (escape_all) out += L'\\';
                    out += *in;
                    break;
                }
                case ANY_CHAR: {
                    // Experimental fix for #1614. The hope is that any time these appear in a
                    // string, they came from wildcard expansion.
                    out += L'?';
                    break;
                }
                case ANY_STRING: {
                    out += L'*';
                    break;
                }
                case ANY_STRING_RECURSIVE: {
                    out += L"**";
                    break;
                }
                case L'&':
                case L'$':
                case L' ':
                case L'#':
                case L'^':
                case L'<':
                case L'>':
                case L'(':
                case L')':
                case L'[':
                case L']':
                case L'{':
                case L'}':
                case L'?':
                case L'*':
                case L'|':
                case L';':
                case L'"':
                case L'%':
                case L'~': {
                    if (!no_tilde || c != L'~') {
                        need_escape = 1;
                        if (escape_all) out += L'\\';
                    }
                    out += *in;
                    break;
                }

                default: {
                    if (*in < 32) {
                        if (*in < 27 && *in > 0) {
                            out += L'\\';
                            out += L'c';
                            out += L'a' + *in - 1;

                            need_escape = need_complex_escape = 1;
                            break;
                        }

                        int tmp = (*in) % 16;
                        out += L'\\';
                        out += L'x';
                        out += ((*in > 15) ? L'1' : L'0');
                        out += tmp > 9 ? L'a' + (tmp - 10) : L'0' + tmp;
                        need_escape = need_complex_escape = 1;
                    } else {
                        out += *in;
                    }
                    break;
                }
            }
        }

        in++;
    }

    // Use quoted escaping if possible, since most people find it easier to read.
    if (!no_quoted && need_escape && !need_complex_escape && escape_all) {
        wchar_t single_quote = L'\'';
        out.clear();
        out.reserve(2 + in_len);
        out.push_back(single_quote);
        out.append(orig_in, in_len);
        out.push_back(single_quote);
    }
}

wcstring escape(const wchar_t *in, escape_flags_t flags) {
    wcstring result;
    escape_string_internal(in, wcslen(in), &result, flags);
    return result;
}

wcstring escape_string(const wcstring &in, escape_flags_t flags) {
    wcstring result;
    escape_string_internal(in.c_str(), in.size(), &result, flags);
    return result;
}

/// Helper to return the last character in a string, or NOT_A_WCHAR.
static wint_t string_last_char(const wcstring &str) {
    size_t len = str.size();
    return len == 0 ? NOT_A_WCHAR : str.at(len - 1);
}

/// Given a null terminated string starting with a backslash, read the escape as if it is unquoted,
/// appending to result. Return the number of characters consumed, or 0 on error.
size_t read_unquoted_escape(const wchar_t *input, wcstring *result, bool allow_incomplete,
                            bool unescape_special) {
    if (input[0] != L'\\') {
        return 0;  // not an escape
    }

    // Here's the character we'll ultimately append, or NOT_A_WCHAR for none. Note that L'\0' is a
    // valid thing to append.
    wint_t result_char_or_none = NOT_A_WCHAR;

    bool errored = false;
    size_t in_pos = 1;  // in_pos always tracks the next character to read (and therefore the number
                        // of characters read so far)
    const wchar_t c = input[in_pos++];
    switch (c) {
        // A null character after a backslash is an error.
        case L'\0': {
            // Adjust in_pos to only include the backslash.
            assert(in_pos > 0);
            in_pos--;

            // It's an error, unless we're allowing incomplete escapes.
            if (!allow_incomplete) errored = true;
            break;
        }
        // Numeric escape sequences. No prefix means octal escape, otherwise hexadecimal.
        case L'0':
        case L'1':
        case L'2':
        case L'3':
        case L'4':
        case L'5':
        case L'6':
        case L'7':
        case L'u':
        case L'U':
        case L'x':
        case L'X': {
            long long res = 0;
            size_t chars = 2;
            int base = 16;
            bool byte_literal = false;
            wchar_t max_val = ASCII_MAX;

            switch (c) {
                case L'u': {
                    chars = 4;
                    max_val = UCS2_MAX;
                    break;
                }
                case L'U': {
                    chars = 8;
                    max_val = WCHAR_MAX;

                    // Don't exceed the largest Unicode code point - see #1107.
                    if (0x10FFFF < max_val) max_val = (wchar_t)0x10FFFF;
                    break;
                }
                case L'x': {
                    chars = 2;
                    max_val = ASCII_MAX;
                    break;
                }
                case L'X': {
                    byte_literal = true;
                    max_val = BYTE_MAX;
                    break;
                }
                default: {
                    base = 8;
                    chars = 3;
                    // Note that in_pos currently is just after the first post-backslash character;
                    // we want to start our escape from there.
                    assert(in_pos > 0);
                    in_pos--;
                    break;
                }
            }

            for (size_t i = 0; i < chars; i++) {
                long d = convert_digit(input[in_pos], base);
                if (d < 0) {
                    break;
                }

                res = (res * base) + d;
                in_pos++;
            }

            if (res <= max_val) {
                result_char_or_none = (wchar_t)((byte_literal ? ENCODE_DIRECT_BASE : 0) + res);
            } else {
                errored = true;
            }

            break;
        }
        // \a means bell (alert).
        case L'a': {
            result_char_or_none = L'\a';
            break;
        }
        // \b means backspace.
        case L'b': {
            result_char_or_none = L'\b';
            break;
        }
        // \cX means control sequence X.
        case L'c': {
            const wchar_t sequence_char = input[in_pos++];
            if (sequence_char >= L'a' && sequence_char <= (L'a' + 32)) {
                result_char_or_none = sequence_char - L'a' + 1;
            } else if (sequence_char >= L'A' && sequence_char <= (L'A' + 32)) {
                result_char_or_none = sequence_char - L'A' + 1;
            } else {
                errored = true;
            }
            break;
        }
        // \x1b means escape.
        case L'e': {
            result_char_or_none = L'\x1b';
            break;
        }
        // \f means form feed.
        case L'f': {
            result_char_or_none = L'\f';
            break;
        }
        // \n means newline.
        case L'n': {
            result_char_or_none = L'\n';
            break;
        }
        // \r means carriage return.
        case L'r': {
            result_char_or_none = L'\r';
            break;
        }
        // \t means tab.
        case L't': {
            result_char_or_none = L'\t';
            break;
        }
        // \v means vertical tab.
        case L'v': {
            result_char_or_none = L'\v';
            break;
        }
        // If a backslash is followed by an actual newline, swallow them both.
        case L'\n': {
            result_char_or_none = NOT_A_WCHAR;
            break;
        }
        default: {
            if (unescape_special) result->push_back(INTERNAL_SEPARATOR);
            result_char_or_none = c;
            break;
        }
    }

    if (!errored && result_char_or_none != NOT_A_WCHAR) {
        wchar_t result_char = static_cast<wchar_t>(result_char_or_none);
        // If result_char is not NOT_A_WCHAR, it must be a valid wchar.
        assert((wint_t)result_char == result_char_or_none);
        result->push_back(result_char);
    }
    return errored ? 0 : in_pos;
}

/// Returns the unescaped version of input_str into output_str (by reference). Returns true if
/// successful. If false, the contents of output_str are undefined (!).
static bool unescape_string_internal(const wchar_t *const input, const size_t input_len,
                                     wcstring *output_str, unescape_flags_t flags) {
    // Set up result string, which we'll swap with the output on success.
    wcstring result;
    result.reserve(input_len);

    const bool unescape_special = !!(flags & UNESCAPE_SPECIAL);
    const bool allow_incomplete = !!(flags & UNESCAPE_INCOMPLETE);

    int bracket_count = 0;

    bool errored = false;
    enum { mode_unquoted, mode_single_quotes, mode_double_quotes } mode = mode_unquoted;

    for (size_t input_position = 0; input_position < input_len && !errored; input_position++) {
        const wchar_t c = input[input_position];
        // Here's the character we'll append to result, or NOT_A_WCHAR to suppress it.
        wint_t to_append_or_none = c;
        if (mode == mode_unquoted) {
            switch (c) {
                case L'\\': {
                    // Backslashes (escapes) are complicated and may result in errors, or appending
                    // INTERNAL_SEPARATORs, so we have to handle them specially.
                    size_t escape_chars = read_unquoted_escape(input + input_position, &result,
                                                               allow_incomplete, unescape_special);
                    if (escape_chars == 0) {
                        // A 0 return indicates an error.
                        errored = true;
                    } else {
                        // Skip over the characters we read, minus one because the outer loop will
                        // increment it.
                        assert(escape_chars > 0);
                        input_position += escape_chars - 1;
                    }
                    // We've already appended, don't append anything else.
                    to_append_or_none = NOT_A_WCHAR;
                    break;
                }
                case L'~': {
                    if (unescape_special && (input_position == 0)) {
                        to_append_or_none = HOME_DIRECTORY;
                    }
                    break;
                }
                case L'%': {
                    if (unescape_special && (input_position == 0)) {
                        to_append_or_none = PROCESS_EXPAND;
                    }
                    break;
                }
                case L'*': {
                    if (unescape_special) {
                        // In general, this is ANY_STRING. But as a hack, if the last appended char
                        // is ANY_STRING, delete the last char and store ANY_STRING_RECURSIVE to
                        // reflect the fact that ** is the recursive wildcard.
                        if (string_last_char(result) == ANY_STRING) {
                            assert(result.size() > 0);
                            result.resize(result.size() - 1);
                            to_append_or_none = ANY_STRING_RECURSIVE;
                        } else {
                            to_append_or_none = ANY_STRING;
                        }
                    }
                    break;
                }
                case L'?': {
                    if (unescape_special) {
                        to_append_or_none = ANY_CHAR;
                    }
                    break;
                }
                case L'$': {
                    if (unescape_special) {
                        to_append_or_none = VARIABLE_EXPAND;
                    }
                    break;
                }
                case L'{': {
                    if (unescape_special) {
                        bracket_count++;
                        to_append_or_none = BRACKET_BEGIN;
                    }
                    break;
                }
                case L'}': {
                    if (unescape_special) {
                        bracket_count--;
                        to_append_or_none = BRACKET_END;
                    }
                    break;
                }
                case L',': {
                    // If the last character was a separator, then treat this as a literal comma.
                    if (unescape_special && bracket_count > 0 &&
                        string_last_char(result) != BRACKET_SEP) {
                        to_append_or_none = BRACKET_SEP;
                    }
                    break;
                }
                case L'\'': {
                    mode = mode_single_quotes;
                    to_append_or_none = unescape_special ? INTERNAL_SEPARATOR : NOT_A_WCHAR;
                    break;
                }
                case L'\"': {
                    mode = mode_double_quotes;
                    to_append_or_none = unescape_special ? INTERNAL_SEPARATOR : NOT_A_WCHAR;
                    break;
                }
            }
        } else if (mode == mode_single_quotes) {
            if (c == L'\\') {
                // A backslash may or may not escape something in single quotes.
                switch (input[input_position + 1]) {
                    case '\\':
                    case L'\'': {
                        to_append_or_none = input[input_position + 1];
                        input_position += 1;  // skip over the backslash
                        break;
                    }
                    case L'\0': {
                        if (!allow_incomplete) {
                            errored = true;
                        } else {
                            // PCA this line had the following cryptic comment: 'We may ever escape
                            // a NULL character, but still appending a \ in case I am wrong.' Not
                            // sure what it means or the importance of this.
                            input_position += 1; /* Skip over the backslash */
                            to_append_or_none = L'\\';
                        }
                        break;
                    }
                    default: {
                        // Literal backslash that doesn't escape anything! Leave things alone; we'll
                        // append the backslash itself.
                        break;
                    }
                }
            } else if (c == L'\'') {
                to_append_or_none = unescape_special ? INTERNAL_SEPARATOR : NOT_A_WCHAR;
                mode = mode_unquoted;
            }
        } else if (mode == mode_double_quotes) {
            switch (c) {
                case L'"': {
                    mode = mode_unquoted;
                    to_append_or_none = unescape_special ? INTERNAL_SEPARATOR : NOT_A_WCHAR;
                    break;
                }
                case '\\': {
                    switch (input[input_position + 1]) {
                        case L'\0': {
                            if (!allow_incomplete) {
                                errored = true;
                            } else {
                                to_append_or_none = L'\0';
                            }
                            break;
                        }
                        case '\\':
                        case L'$':
                        case '"': {
                            to_append_or_none = input[input_position + 1];
                            input_position += 1; /* Skip over the backslash */
                            break;
                        }
                        case '\n': {
                            /* Swallow newline */
                            to_append_or_none = NOT_A_WCHAR;
                            input_position += 1; /* Skip over the backslash */
                            break;
                        }
                        default: {
                            /* Literal backslash that doesn't escape anything! Leave things alone;
                             * we'll append the backslash itself */
                            break;
                        }
                    }
                    break;
                }
                case '$': {
                    if (unescape_special) {
                        to_append_or_none = VARIABLE_EXPAND_SINGLE;
                    }
                    break;
                }
            }
        }

        // Now maybe append the char.
        if (to_append_or_none != NOT_A_WCHAR) {
            wchar_t to_append_char = static_cast<wchar_t>(to_append_or_none);
            // If result_char is not NOT_A_WCHAR, it must be a valid wchar.
            assert((wint_t)to_append_char == to_append_or_none);
            result.push_back(to_append_char);
        }
    }

    // Return the string by reference, and then success.
    if (!errored) {
        output_str->swap(result);
    }
    return !errored;
}

bool unescape_string_in_place(wcstring *str, unescape_flags_t escape_special) {
    assert(str != NULL);
    wcstring output;
    bool success = unescape_string_internal(str->c_str(), str->size(), &output, escape_special);
    if (success) {
        str->swap(output);
    }
    return success;
}

bool unescape_string(const wchar_t *input, wcstring *output, unescape_flags_t escape_special) {
    bool success = unescape_string_internal(input, wcslen(input), output, escape_special);
    if (!success) output->clear();
    return success;
}

bool unescape_string(const wcstring &input, wcstring *output, unescape_flags_t escape_special) {
    bool success = unescape_string_internal(input.c_str(), input.size(), output, escape_special);
    if (!success) output->clear();
    return success;
}

void common_handle_winch(int signal) {
    // Don't run ioctl() here, it's not safe to use in signals.
    termsize_valid = false;
}

/// Updates termsize as needed, and returns a copy of the winsize.
static struct winsize get_current_winsize() {
#ifndef HAVE_WINSIZE
    struct winsize retval = {0};
    retval.ws_col = 80;
    retval.ws_row = 24;
    return retval;
#endif
    scoped_rwlock guard(termsize_rwlock, true);
    struct winsize retval = termsize;
    if (!termsize_valid) {
        struct winsize size;
        if (ioctl(1, TIOCGWINSZ, &size) == 0) {
            retval = size;
            guard.upgrade();
            termsize = retval;
        }
        termsize_valid = true;
    }
    return retval;
}

int common_get_width() { return get_current_winsize().ws_col; }

int common_get_height() { return get_current_winsize().ws_row; }

void tokenize_variable_array(const wcstring &val, std::vector<wcstring> &out) {
    size_t pos = 0, end = val.size();
    while (pos <= end) {
        size_t next_pos = val.find(ARRAY_SEP, pos);
        if (next_pos == wcstring::npos) {
            next_pos = end;
        }
        out.resize(out.size() + 1);
        out.back().assign(val, pos, next_pos - pos);
        pos = next_pos + 1;  // skip the separator, or skip past the end
    }
}

bool string_prefixes_string(const wchar_t *proposed_prefix, const wcstring &value) {
    size_t prefix_size = wcslen(proposed_prefix);
    return prefix_size <= value.size() && value.compare(0, prefix_size, proposed_prefix) == 0;
}

bool string_prefixes_string(const wcstring &proposed_prefix, const wcstring &value) {
    size_t prefix_size = proposed_prefix.size();
    return prefix_size <= value.size() && value.compare(0, prefix_size, proposed_prefix) == 0;
}

bool string_prefixes_string_case_insensitive(const wcstring &proposed_prefix,
                                             const wcstring &value) {
    size_t prefix_size = proposed_prefix.size();
    return prefix_size <= value.size() &&
           wcsncasecmp(proposed_prefix.c_str(), value.c_str(), prefix_size) == 0;
}

bool string_suffixes_string(const wcstring &proposed_suffix, const wcstring &value) {
    size_t suffix_size = proposed_suffix.size();
    return suffix_size <= value.size() &&
           value.compare(value.size() - suffix_size, suffix_size, proposed_suffix) == 0;
}

bool string_suffixes_string(const wchar_t *proposed_suffix, const wcstring &value) {
    size_t suffix_size = wcslen(proposed_suffix);
    return suffix_size <= value.size() &&
           value.compare(value.size() - suffix_size, suffix_size, proposed_suffix) == 0;
}

/// Returns true if seq, represented as a subsequence, is contained within string.
static bool subsequence_in_string(const wcstring &seq, const wcstring &str) {
    // Impossible if seq is larger than string.
    if (seq.size() > str.size()) {
        return false;
    }

    // Empty strings are considered to be subsequences of everything.
    if (seq.empty()) {
        return true;
    }

    size_t str_idx, seq_idx;
    for (seq_idx = str_idx = 0; seq_idx < seq.size() && str_idx < str.size(); seq_idx++) {
        wchar_t c = seq.at(seq_idx);
        size_t char_loc = str.find(c, str_idx);
        if (char_loc == wcstring::npos) {
            break;  // didn't find this character
        } else {
            str_idx = char_loc + 1;  // we found it, continue the search just after it
        }
    }

    // We succeeded if we exhausted our sequence.
    assert(seq_idx <= seq.size());
    return seq_idx == seq.size();
}

string_fuzzy_match_t::string_fuzzy_match_t(enum fuzzy_match_type_t t, size_t distance_first,
                                           size_t distance_second)
    : type(t), match_distance_first(distance_first), match_distance_second(distance_second) {}

string_fuzzy_match_t string_fuzzy_match_string(const wcstring &string,
                                               const wcstring &match_against,
                                               fuzzy_match_type_t limit_type) {
    // Distances are generally the amount of text not matched.
    string_fuzzy_match_t result(fuzzy_match_none, 0, 0);
    size_t location;
    if (limit_type >= fuzzy_match_exact && string == match_against) {
        result.type = fuzzy_match_exact;
    } else if (limit_type >= fuzzy_match_prefix && string_prefixes_string(string, match_against)) {
        result.type = fuzzy_match_prefix;
        assert(match_against.size() >= string.size());
        result.match_distance_first = match_against.size() - string.size();
    } else if (limit_type >= fuzzy_match_case_insensitive &&
               wcscasecmp(string.c_str(), match_against.c_str()) == 0) {
        result.type = fuzzy_match_case_insensitive;
    } else if (limit_type >= fuzzy_match_prefix_case_insensitive &&
               string_prefixes_string_case_insensitive(string, match_against)) {
        result.type = fuzzy_match_prefix_case_insensitive;
        assert(match_against.size() >= string.size());
        result.match_distance_first = match_against.size() - string.size();
    } else if (limit_type >= fuzzy_match_substring &&
               (location = match_against.find(string)) != wcstring::npos) {
        // String is contained within match against.
        result.type = fuzzy_match_substring;
        assert(match_against.size() >= string.size());
        result.match_distance_first = match_against.size() - string.size();
        result.match_distance_second = location;  // prefer earlier matches
    } else if (limit_type >= fuzzy_match_subsequence_insertions_only &&
               subsequence_in_string(string, match_against)) {
        result.type = fuzzy_match_subsequence_insertions_only;
        assert(match_against.size() >= string.size());
        result.match_distance_first = match_against.size() - string.size();
        // It would be nice to prefer matches with greater matching runs here.
    }
    return result;
}

template <typename T>
static inline int compare_ints(T a, T b) {
    if (a < b) return -1;
    if (a == b) return 0;
    return 1;
}

/// Compare types; if the types match, compare distances.
int string_fuzzy_match_t::compare(const string_fuzzy_match_t &rhs) const {
    if (this->type != rhs.type) {
        return compare_ints(this->type, rhs.type);
    } else if (this->match_distance_first != rhs.match_distance_first) {
        return compare_ints(this->match_distance_first, rhs.match_distance_first);
    } else if (this->match_distance_second != rhs.match_distance_second) {
        return compare_ints(this->match_distance_second, rhs.match_distance_second);
    }
    return 0;  // equal
}

bool list_contains_string(const wcstring_list_t &list, const wcstring &str) {
    return std::find(list.begin(), list.end(), str) != list.end();
}

int create_directory(const wcstring &d) {
    int ok = 0;
    struct stat buf;
    int stat_res = 0;

    while ((stat_res = wstat(d, &buf)) != 0) {
        if (errno != EAGAIN) break;
    }

    if (stat_res == 0) {
        if (S_ISDIR(buf.st_mode)) {
            ok = 1;
        }
    } else {
        if (errno == ENOENT) {
            wcstring dir = wdirname(d);
            if (!create_directory(dir)) {
                if (!wmkdir(d, 0700)) {
                    ok = 1;
                }
            }
        }
    }

    return ok ? 0 : -1;
}

__attribute__((noinline)) void bugreport() {
    debug(1, _(L"This is a bug. Break on bugreport to debug."
               L"If you can reproduce it, please send a bug report to %s."),
          PACKAGE_BUGREPORT);
}

wcstring format_size(long long sz) {
    wcstring result;
    const wchar_t *sz_name[] = {L"kB", L"MB", L"GB", L"TB", L"PB", L"EB", L"ZB", L"YB", 0};

    if (sz < 0) {
        result.append(L"unknown");
    } else if (sz < 1) {
        result.append(_(L"empty"));
    } else if (sz < 1024) {
        result.append(format_string(L"%lldB", sz));
    } else {
        int i;

        for (i = 0; sz_name[i]; i++) {
            if (sz < (1024 * 1024) || !sz_name[i + 1]) {
                long isz = ((long)sz) / 1024;
                if (isz > 9)
                    result.append(format_string(L"%d%ls", isz, sz_name[i]));
                else
                    result.append(format_string(L"%.1f%ls", (double)sz / 1024, sz_name[i]));
                break;
            }
            sz /= 1024;
        }
    }
    return result;
}

/// Crappy function to extract the most significant digit of an unsigned long long value.
static char extract_most_significant_digit(unsigned long long *xp) {
    unsigned long long place_value = 1;
    unsigned long long x = *xp;
    while (x >= 10) {
        x /= 10;
        place_value *= 10;
    }
    *xp -= (place_value * x);
    return x + '0';
}

void append_ull(char *buff, unsigned long long val, size_t *inout_idx, size_t max_len) {
    size_t idx = *inout_idx;
    while (val > 0 && idx < max_len) buff[idx++] = extract_most_significant_digit(&val);
    *inout_idx = idx;
}

void append_str(char *buff, const char *str, size_t *inout_idx, size_t max_len) {
    size_t idx = *inout_idx;
    while (*str && idx < max_len) buff[idx++] = *str++;
    *inout_idx = idx;
}

void format_size_safe(char buff[128], unsigned long long sz) {
    const size_t buff_size = 128;
    const size_t max_len = buff_size - 1;  // need to leave room for a null terminator
    memset(buff, 0, buff_size);
    size_t idx = 0;
    const char *const sz_name[] = {"kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB", NULL};
    if (sz < 1) {
        strncpy(buff, "empty", buff_size);
    } else if (sz < 1024) {
        append_ull(buff, sz, &idx, max_len);
        append_str(buff, "B", &idx, max_len);
    } else {
        for (size_t i = 0; sz_name[i]; i++) {
            if (sz < (1024 * 1024) || !sz_name[i + 1]) {
                unsigned long long isz = sz / 1024;
                if (isz > 9) {
                    append_ull(buff, isz, &idx, max_len);
                } else {
                    if (isz == 0) {
                        append_str(buff, "0", &idx, max_len);
                    } else {
                        append_ull(buff, isz, &idx, max_len);
                    }

                    // Maybe append a single fraction digit.
                    unsigned long long remainder = sz % 1024;
                    if (remainder > 0) {
                        char tmp[3] = {'.', extract_most_significant_digit(&remainder), 0};
                        append_str(buff, tmp, &idx, max_len);
                    }
                }
                append_str(buff, sz_name[i], &idx, max_len);
                break;
            }
            sz /= 1024;
        }
    }
}

double timef() {
    struct timeval tv;
    int time_res = gettimeofday(&tv, 0);

    if (time_res) {
        // Fixme: What on earth is the correct parameter value for NaN? The man pages and the
        // standard helpfully state that this parameter is implementation defined. Gcc gives a
        // warning if a null pointer is used. But not even all mighty Google gives a hint to what
        // value should actually be returned.
        return nan("");
    }

    return (double)tv.tv_sec + 0.000001 * tv.tv_usec;
}

void exit_without_destructors(int code) { _exit(code); }

/// Helper function to convert from a null_terminated_array_t<wchar_t> to a
/// null_terminated_array_t<char_t>.
void convert_wide_array_to_narrow(const null_terminated_array_t<wchar_t> &wide_arr,
                                  null_terminated_array_t<char> *output) {
    const wchar_t *const *arr = wide_arr.get();
    if (!arr) {
        output->clear();
        return;
    }

    std::vector<std::string> list;
    for (size_t i = 0; arr[i]; i++) {
        list.push_back(wcs2string(arr[i]));
    }
    output->set(list);
}

void append_path_component(wcstring &path, const wcstring &component) {
    if (path.empty() || component.empty()) {
        path.append(component);
    } else {
        size_t path_len = path.size();
        bool path_slash = path.at(path_len - 1) == L'/';
        bool comp_slash = component.at(0) == L'/';
        if (!path_slash && !comp_slash) {
            // Need a slash
            path.push_back(L'/');
        } else if (path_slash && comp_slash) {
            // Too many slashes.
            path.erase(path_len - 1, 1);
        }
        path.append(component);
    }
}

extern "C" {
__attribute__((noinline)) void debug_thread_error(void) {
    while (1) sleep(9999999);
}
}

void set_main_thread() { main_thread_id = pthread_self(); }

void configure_thread_assertions_for_testing(void) {
    thread_assertions_configured_for_testing = true;
}

/// Notice when we've forked.
static pid_t initial_pid = 0;

/// Be able to restore the term's foreground process group.
static pid_t initial_foreground_process_group = -1;

bool is_forked_child(void) {
    // Just bail if nobody's called setup_fork_guards, e.g. some of our tools.
    if (!initial_pid) return false;

    bool is_child_of_fork = (getpid() != initial_pid);
    if (is_child_of_fork) {
        printf("Uh-oh: %d\n", getpid());
        while (1) sleep(10000);
    }
    return is_child_of_fork;
}

void setup_fork_guards(void) {
    // Notice when we fork by stashing our pid. This seems simpler than pthread_atfork().
    initial_pid = getpid();
}

void save_term_foreground_process_group(void) {
    initial_foreground_process_group = tcgetpgrp(STDIN_FILENO);
}

void restore_term_foreground_process_group(void) {
    if (initial_foreground_process_group != -1) {
        // This is called during shutdown and from a signal handler. We don't bother to complain on
        // failure.
        tcsetpgrp(STDIN_FILENO, initial_foreground_process_group);
    }
}

bool is_main_thread() {
    assert(main_thread_id != 0);
    return main_thread_id == pthread_self();
}

void assert_is_main_thread(const char *who) {
    if (!is_main_thread() && !thread_assertions_configured_for_testing) {
        fprintf(stderr,
                "Warning: %s called off of main thread. Break on debug_thread_error to debug.\n",
                who);
        debug_thread_error();
    }
}

void assert_is_not_forked_child(const char *who) {
    if (is_forked_child()) {
        fprintf(stderr,
                "Warning: %s called in a forked child. Break on debug_thread_error to debug.\n",
                who);
        debug_thread_error();
    }
}

void assert_is_background_thread(const char *who) {
    if (is_main_thread() && !thread_assertions_configured_for_testing) {
        fprintf(stderr,
                "Warning: %s called on the main thread (may block!). Break on debug_thread_error "
                "to debug.\n",
                who);
        debug_thread_error();
    }
}

void assert_is_locked(void *vmutex, const char *who, const char *caller) {
    pthread_mutex_t *mutex = static_cast<pthread_mutex_t *>(vmutex);
    if (0 == pthread_mutex_trylock(mutex)) {
        fprintf(stderr,
                "Warning: %s is not locked when it should be in '%s'. Break on debug_thread_error "
                "to debug.\n",
                who, caller);
        debug_thread_error();
        pthread_mutex_unlock(mutex);
    }
}

void scoped_lock::lock(void) {
    assert(!locked);
    assert(!is_forked_child());
    VOMIT_ON_FAILURE_NO_ERRNO(pthread_mutex_lock(lock_obj));
    locked = true;
}

void scoped_lock::unlock(void) {
    assert(locked);
    assert(!is_forked_child());
    VOMIT_ON_FAILURE_NO_ERRNO(pthread_mutex_unlock(lock_obj));
    locked = false;
}

scoped_lock::scoped_lock(pthread_mutex_t &mutex) : lock_obj(&mutex), locked(false) { this->lock(); }

scoped_lock::scoped_lock(mutex_lock_t &lock) : lock_obj(&lock.mutex), locked(false) {
    this->lock();
}

scoped_lock::~scoped_lock() {
    if (locked) this->unlock();
}

void scoped_rwlock::lock(void) {
    assert(!(locked || locked_shared));
    assert(!is_forked_child());
    VOMIT_ON_FAILURE_NO_ERRNO(pthread_rwlock_rdlock(rwlock_obj));
    locked = true;
}

void scoped_rwlock::unlock(void) {
    assert(locked);
    assert(!is_forked_child());
    VOMIT_ON_FAILURE_NO_ERRNO(pthread_rwlock_unlock(rwlock_obj));
    locked = false;
}

void scoped_rwlock::lock_shared(void) {
    assert(!(locked || locked_shared));
    assert(!is_forked_child());
    VOMIT_ON_FAILURE_NO_ERRNO(pthread_rwlock_wrlock(rwlock_obj));
    locked_shared = true;
}

void scoped_rwlock::unlock_shared(void) {
    assert(locked_shared);
    assert(!is_forked_child());
    VOMIT_ON_FAILURE_NO_ERRNO(pthread_rwlock_unlock(rwlock_obj));
    locked_shared = false;
}

void scoped_rwlock::upgrade(void) {
    assert(locked_shared);
    assert(!is_forked_child());
    VOMIT_ON_FAILURE_NO_ERRNO(pthread_rwlock_unlock(rwlock_obj));
    locked = false;
    VOMIT_ON_FAILURE_NO_ERRNO(pthread_rwlock_wrlock(rwlock_obj));
    locked_shared = true;
}

scoped_rwlock::scoped_rwlock(pthread_rwlock_t &rwlock, bool shared)
    : rwlock_obj(&rwlock), locked(false), locked_shared(false) {
    if (shared) {
        this->lock_shared();
    } else {
        this->lock();
    }
}

scoped_rwlock::scoped_rwlock(rwlock_t &rwlock, bool shared)
    : rwlock_obj(&rwlock.rwlock), locked(false), locked_shared(false) {
    if (shared) {
        this->lock_shared();
    } else {
        this->lock();
    }
}

scoped_rwlock::~scoped_rwlock() {
    if (locked) {
        this->unlock();
    } else if (locked_shared) {
        this->unlock_shared();
    }
}

wcstokenizer::wcstokenizer(const wcstring &s, const wcstring &separator)
    : buffer(), str(), state(), sep(separator) {
    buffer = wcsdup(s.c_str());
    str = buffer;
    state = NULL;
}

bool wcstokenizer::next(wcstring &result) {
    wchar_t *tmp = wcstok(str, sep.c_str(), &state);
    str = NULL;
    if (tmp) result = tmp;
    return tmp != NULL;
}

wcstokenizer::~wcstokenizer() { free(buffer); }

template <typename CharType_t>
static CharType_t **make_null_terminated_array_helper(
    const std::vector<std::basic_string<CharType_t> > &argv) {
    size_t count = argv.size();

    // We allocate everything in one giant block. First compute how much space we need.
    // N + 1 pointers.
    size_t pointers_allocation_len = (count + 1) * sizeof(CharType_t *);

    // In the very unlikely event that CharType_t has stricter alignment requirements than does a
    // pointer, round us up to the size of a CharType_t.
    pointers_allocation_len += sizeof(CharType_t) - 1;
    pointers_allocation_len -= pointers_allocation_len % sizeof(CharType_t);

    // N null terminated strings.
    size_t strings_allocation_len = 0;
    for (size_t i = 0; i < count; i++) {
        // The size of the string, plus a null terminator.
        strings_allocation_len += (argv.at(i).size() + 1) * sizeof(CharType_t);
    }

    // Now allocate their sum.
    unsigned char *base =
        static_cast<unsigned char *>(malloc(pointers_allocation_len + strings_allocation_len));
    if (!base) return NULL;

    // Divvy it up into the pointers and strings.
    CharType_t **pointers = reinterpret_cast<CharType_t **>(base);
    CharType_t *strings = reinterpret_cast<CharType_t *>(base + pointers_allocation_len);

    // Start copying.
    for (size_t i = 0; i < count; i++) {
        const std::basic_string<CharType_t> &str = argv.at(i);
        *pointers++ = strings;  // store the current string pointer into self
        strings = std::copy(str.begin(), str.end(), strings);  // copy the string into strings
        *strings++ = (CharType_t)(0);  // each string needs a null terminator
    }
    *pointers++ = NULL;  // array of pointers needs a null terminator

    // Make sure we know what we're doing.
    assert((unsigned char *)pointers - base == (std::ptrdiff_t)pointers_allocation_len);
    assert((unsigned char *)strings - (unsigned char *)pointers ==
           (std::ptrdiff_t)strings_allocation_len);
    assert((unsigned char *)strings - base ==
           (std::ptrdiff_t)(pointers_allocation_len + strings_allocation_len));

    return reinterpret_cast<CharType_t **>(base);
}

wchar_t **make_null_terminated_array(const wcstring_list_t &lst) {
    return make_null_terminated_array_helper(lst);
}

char **make_null_terminated_array(const std::vector<std::string> &lst) {
    return make_null_terminated_array_helper(lst);
}
