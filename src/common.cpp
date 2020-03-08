// Various functions, mostly string utilities, that are used by most parts of fish.
#include "config.h"

#ifdef HAVE_BACKTRACE_SYMBOLS
#include <cxxabi.h>
#endif

#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#include <wctype.h>

#include <cstring>
#include <cwchar>
#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif
#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef __linux__
// Includes for WSL detection
#include <sys/utsname.h>
#endif

#ifdef __FreeBSD__
#include <sys/sysctl.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#endif

#include <algorithm>
#include <atomic>
#include <memory>  // IWYU pragma: keep
#include <type_traits>

#include "common.h"
#include "env.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "flog.h"
#include "future_feature_flags.h"
#include "global_safety.h"
#include "iothread.h"
#include "parser.h"
#include "proc.h"
#include "signal.h"
#include "wcstringutil.h"
#include "wildcard.h"
#include "wutil.h"  // IWYU pragma: keep

struct termios shell_modes;

/// This allows us to notice when we've forked.
static relaxed_atomic_bool_t is_forked_proc{false};
/// This allows us to bypass the main thread checks
static relaxed_atomic_bool_t thread_asserts_cfg_for_testing{false};

static relaxed_atomic_t<wchar_t> ellipsis_char;
wchar_t get_ellipsis_char() { return ellipsis_char; }

static relaxed_atomic_t<const wchar_t *> ellipsis_str;
const wchar_t *get_ellipsis_str() { return ellipsis_str; }

static relaxed_atomic_t<const wchar_t *> omitted_newline_str;
const wchar_t *get_omitted_newline_str() { return omitted_newline_str; }

static relaxed_atomic_t<int> omitted_newline_width;
int get_omitted_newline_width() { return omitted_newline_width; }

static relaxed_atomic_t<wchar_t> obfuscation_read_char;
wchar_t get_obfuscation_read_char() { return obfuscation_read_char; }

bool g_profiling_active = false;
const wchar_t *program_name;
std::atomic<int> debug_level{1};  // default maximum debug output level (errors and warnings)

static relaxed_atomic_t<int> debug_stack_frames{0};
void set_debug_stack_frames(int v) { debug_stack_frames = v; }
int get_debug_stack_frames() { return debug_stack_frames; }

/// Be able to restore the term's foreground process group.
/// This is set during startup and not modified after.
static relaxed_atomic_t<pid_t> initial_fg_process_group{-1};

/// This struct maintains the current state of the terminal size. It is updated on demand after
/// receiving a SIGWINCH. Use common_get_width()/common_get_height() to read it lazily.
static constexpr struct winsize k_invalid_termsize = {USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX};
static owning_lock<struct winsize> s_termsize{k_invalid_termsize};

static relaxed_atomic_bool_t s_termsize_valid{false};

static char *wcs2str_internal(const wchar_t *in, char *out);
static void debug_shared(wchar_t msg_level, const wcstring &msg);

#if defined(OS_IS_CYGWIN) || defined(WSL)
// MS Windows tty devices do not currently have either a read or write timestamp. Those
// respective fields of `struct stat` are always the current time. Which means we can't
// use them. So we assume no external program has written to the terminal behind our
// back. This makes multiline promptusable. See issue #2859 and
// https://github.com/Microsoft/BashOnWindows/issues/545
const bool has_working_tty_timestamps = false;
#else
const bool has_working_tty_timestamps = true;
#endif

/// Convert a character to its integer equivalent if it is a valid character for the requested base.
/// Return the integer value if it is valid else -1.
long convert_digit(wchar_t d, int base) {
    long res = -1;
    if ((d <= L'9') && (d >= L'0')) {
        res = d - L'0';
    } else if ((d <= L'z') && (d >= L'a')) {
        res = d + 10 - L'a';
    } else if ((d <= L'Z') && (d >= L'A')) {
        res = d + 10 - L'A';
    }
    if (res >= base) {
        res = -1;
    }

    return res;
}

/// Test whether the char is a valid hex digit as used by the `escape_string_*()` functions.
static bool is_hex_digit(int c) { return std::strchr("0123456789ABCDEF", c) != nullptr; }

/// This is a specialization of `convert_digit()` that only handles base 16 and only uppercase.
long convert_hex_digit(wchar_t d) {
    if ((d <= L'9') && (d >= L'0')) {
        return d - L'0';
    } else if ((d <= L'Z') && (d >= L'A')) {
        return 10 + d - L'A';
    }

    return -1;
}

bool is_windows_subsystem_for_linux() {
#if defined(WSL)
    return true;
#elif not defined(__linux__)
    return false;
#else
    // We are purposely not using std::call_once as it may invoke locking, which is an unnecessary
    // overhead since there's no actual race condition here - even if multiple threads call this
    // routine simultaneously the first time around, we just end up needlessly querying uname(2) one
    // more time.

    static bool wsl_state = []() {
        utsname info;
        uname(&info);

        // Sample utsname.release under WSL, testing for something like `4.4.0-17763-Microsoft`
        if (std::strstr(info.release, "Microsoft") != nullptr) {
            const char *dash = std::strchr(info.release, '-');
            if (dash == nullptr || strtod(dash + 1, nullptr) < 17763) {
                // #5298, #5661: There are acknowledged, published, and (later) fixed issues with
                // job control under early WSL releases that prevent fish from running correctly,
                // with unexpected failures when piping. Fish 3.0 nightly builds worked around this
                // issue with some needlessly complicated code that was later stripped from the
                // fish 3.0 release, so we just bail. Note that fish 2.0 was also broken, but we
                // just didn't warn about it.

                // #6038 & 5101bde: It's been requested that there be some sort of way to disable
                // this check: if the environment variable FISH_NO_WSL_CHECK is present, this test
                // is bypassed. We intentionally do not include this in the error message because
                // it'll only allow fish to run but not to actually work. Here be dragons!
                if (getenv("FISH_NO_WSL_CHECK") == nullptr) {
                    FLOGF(error,
                          "This version of WSL has known bugs that prevent fish from working."
                          "Please upgrade to Windows 10 1809 (17763) or higher to use fish!");
                }
            }

            return true;
        } else {
            return false;
        }
    }();

    // Subsequent calls to this function may take place after fork() and before exec() in
    // postfork.cpp. Make sure we never dynamically allocate any memory in the fast path!
    return wsl_state;
#endif
}

#ifdef HAVE_BACKTRACE_SYMBOLS
// This function produces a stack backtrace with demangled function & method names. It is based on
// https://gist.github.com/fmela/591333 but adapted to the style of the fish project.
[[gnu::noinline]] static wcstring_list_t demangled_backtrace(int max_frames, int skip_levels) {
    void *callstack[128];
    const int n_max_frames = sizeof(callstack) / sizeof(callstack[0]);
    int n_frames = backtrace(callstack, n_max_frames);
    char **symbols = backtrace_symbols(callstack, n_frames);
    wchar_t text[1024];
    wcstring_list_t backtrace_text;

    if (skip_levels + max_frames < n_frames) n_frames = skip_levels + max_frames;

    for (int i = skip_levels; i < n_frames; i++) {
        Dl_info info;
        if (dladdr(callstack[i], &info) && info.dli_sname) {
            char *demangled = nullptr;
            int status = -1;
            if (info.dli_sname[0] == '_')
                demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
            swprintf(
                text, sizeof(text) / sizeof(wchar_t), L"%-3d %s + %td", i - skip_levels,
                status == 0 ? demangled : info.dli_sname == nullptr ? symbols[i] : info.dli_sname,
                static_cast<char *>(callstack[i]) - static_cast<char *>(info.dli_saddr));
            free(demangled);
        } else {
            swprintf(text, sizeof(text) / sizeof(wchar_t), L"%-3d %s", i - skip_levels, symbols[i]);
        }
        backtrace_text.push_back(text);
    }
    free(symbols);
    return backtrace_text;
}

[[gnu::noinline]] void show_stackframe(const wchar_t msg_level, int frame_count, int skip_levels) {
    if (frame_count < 1) return;

    wcstring_list_t bt = demangled_backtrace(frame_count, skip_levels + 2);
    debug_shared(msg_level, L"Backtrace:\n" + join_strings(bt, L'\n') + L'\n');
}

#else   // HAVE_BACKTRACE_SYMBOLS

[[gnu::noinline]] void show_stackframe(const wchar_t msg_level, int, int) {
    debug_shared(msg_level, L"Sorry, but your system does not support backtraces");
}
#endif  // HAVE_BACKTRACE_SYMBOLS

/// Converts the narrow character string \c in into its wide equivalent, and return it.
///
/// The string may contain embedded nulls.
///
/// This function encodes illegal character sequences in a reversible way using the private use
/// area.
static wcstring str2wcs_internal(const char *in, const size_t in_len) {
    if (in_len == 0) return wcstring();
    assert(in != nullptr);

    wcstring result;
    result.reserve(in_len);
    size_t in_pos = 0;

    if (MB_CUR_MAX == 1) {
        // Single-byte locale, all values are legal.
        while (in_pos < in_len) {
            result.push_back(static_cast<unsigned char>(in[in_pos]));
            in_pos++;
        }
        return result;
    }

    mbstate_t state = {};
    while (in_pos < in_len) {
        bool use_encode_direct = false;
        size_t ret = 0;
        wchar_t wc = 0;

        if ((in[in_pos] & 0xF8) == 0xF8) {
            // Protect against broken std::mbrtowc() implementations which attempt to encode UTF-8
            // sequences longer than four bytes (e.g., OS X Snow Leopard).
            use_encode_direct = true;
        } else if (sizeof(wchar_t) == 2 &&  //!OCLINT(constant if expression)
                   (in[in_pos] & 0xF8) == 0xF0) {
            // Assume we are in a UTF-16 environment (e.g., Cygwin) using a UTF-8 encoding.
            // The bits set check will be true for a four byte UTF-8 sequence that requires
            // two UTF-16 chars. Something that doesn't work with our simple use of std::mbrtowc().
            use_encode_direct = true;
        } else {
            ret = std::mbrtowc(&wc, &in[in_pos], in_len - in_pos, &state);
            // Determine whether to encode this character with our crazy scheme.
            if (wc >= ENCODE_DIRECT_BASE && wc < ENCODE_DIRECT_BASE + 256) {
                use_encode_direct = true;
            } else if (wc == INTERNAL_SEPARATOR) {
                use_encode_direct = true;
            } else if (ret == static_cast<size_t>(-2)) {
                // Incomplete sequence.
                use_encode_direct = true;
            } else if (ret == static_cast<size_t>(-1)) {
                // Invalid data.
                use_encode_direct = true;
            } else if (ret > in_len - in_pos) {
                // Other error codes? Terrifying, should never happen.
                use_encode_direct = true;
            } else if (sizeof(wchar_t) == 2 && wc >= 0xD800 &&  //!OCLINT(constant if expression)
                       wc <= 0xDFFF) {
                // If we get a surrogate pair char on a UTF-16 system (e.g., Cygwin) then
                // it's guaranteed the UTF-8 decoding is wrong so use direct encoding.
                use_encode_direct = true;
            }
        }

        if (use_encode_direct) {
            wc = ENCODE_DIRECT_BASE + static_cast<unsigned char>(in[in_pos]);
            result.push_back(wc);
            in_pos++;
            std::memset(&state, 0, sizeof state);
        } else if (ret == 0) {  // embedded null byte!
            result.push_back(L'\0');
            in_pos++;
            std::memset(&state, 0, sizeof state);
        } else {  // normal case
            result.push_back(wc);
            in_pos += ret;
        }
    }

    return result;
}

wcstring str2wcstring(const char *in, size_t len) { return str2wcs_internal(in, len); }

wcstring str2wcstring(const char *in) { return str2wcs_internal(in, std::strlen(in)); }

wcstring str2wcstring(const std::string &in) {
    // Handles embedded nulls!
    return str2wcs_internal(in.data(), in.size());
}

wcstring str2wcstring(const std::string &in, size_t len) {
    // Handles embedded nulls!
    return str2wcs_internal(in.data(), len);
}

char *wcs2str(const wchar_t *in, size_t len) {
    if (!in) return nullptr;
    size_t desired_size = MAX_UTF8_BYTES * len + 1;
    char local_buff[512];
    if (desired_size <= sizeof local_buff / sizeof *local_buff) {
        // Convert into local buff, then use strdup() so we don't waste malloc'd space.
        char *result = wcs2str_internal(in, local_buff);
        if (result) {
            // It converted into the local buffer, so copy it.
            result = strdup(result);
            assert(result);
        }
        return result;
    }

    // Here we probably allocate a buffer probably much larger than necessary.
    char *out = static_cast<char *>(malloc(MAX_UTF8_BYTES * len + 1));
    assert(out);
    // Instead of returning the return value of wcs2str_internal, return `out` directly.
    // This eliminates false warnings in coverity about resource leaks.
    wcs2str_internal(in, out);
    return out;
}

char *wcs2str(const wchar_t *in) { return wcs2str(in, std::wcslen(in)); }
char *wcs2str(const wcstring &in) { return wcs2str(in.c_str(), in.length()); }

/// This function is distinguished from wcs2str_internal in that it allows embedded null bytes.
std::string wcs2string(const wcstring &input) {
    std::string result;
    result.reserve(input.size());

    mbstate_t state = {};
    char converted[MB_LEN_MAX];

    for (auto wc : input) {
        if (wc == INTERNAL_SEPARATOR) {
            // do nothing
        } else if (wc >= ENCODE_DIRECT_BASE && wc < ENCODE_DIRECT_BASE + 256) {
            result.push_back(wc - ENCODE_DIRECT_BASE);
        } else if (MB_CUR_MAX == 1) {  // single-byte locale (C/POSIX/ISO-8859)
            // If `wc` contains a wide character we emit a question-mark.
            if (wc & ~0xFF) {
                wc = '?';
            }
            converted[0] = wc;
            result.append(converted, 1);
        } else {
            std::memset(converted, 0, sizeof converted);
            size_t len = std::wcrtomb(converted, wc, &state);
            if (len == static_cast<size_t>(-1)) {
                FLOGF(char_encoding, L"Wide character U+%4X has no narrow representation", wc);
                std::memset(&state, 0, sizeof(state));
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
    assert(in && out && "in and out must not be null");
    size_t in_pos = 0;
    size_t out_pos = 0;
    mbstate_t state = {};

    while (in[in_pos]) {
        if (in[in_pos] == INTERNAL_SEPARATOR) {
            // do nothing
        } else if (in[in_pos] >= ENCODE_DIRECT_BASE && in[in_pos] < ENCODE_DIRECT_BASE + 256) {
            out[out_pos++] = in[in_pos] - ENCODE_DIRECT_BASE;
        } else if (MB_CUR_MAX == 1)  // single-byte locale (C/POSIX/ISO-8859)
        {
            // If `wc` contains a wide character we emit a question-mark.
            if (in[in_pos] & ~0xFF) {
                out[out_pos++] = '?';
            } else {
                out[out_pos++] = static_cast<unsigned char>(in[in_pos]);
            }
        } else {
            size_t len = std::wcrtomb(&out[out_pos], in[in_pos], &state);
            if (len == static_cast<size_t>(-1)) {
                FLOGF(char_encoding, L"Wide character U+%4X has no narrow representation",
                      in[in_pos]);
                std::memset(&state, 0, sizeof(state));
            } else {
                out_pos += len;
            }
        }
        in_pos++;
    }
    out[out_pos] = 0;

    return out;
}

/// Test if the character can be encoded using the current locale.
static bool can_be_encoded(wchar_t wc) {
    char converted[MB_LEN_MAX];
    mbstate_t state = {};

    return std::wcrtomb(converted, wc, &state) != static_cast<size_t>(-1);
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
    // Because of this, on failure we try to increase the buffer size until the free space is
    // larger than max_size, at which point it will conclude that the error was probably due to a
    // badly formated string option, and return an error. Make sure to null terminate string before
    // that, though.
    const size_t max_size = (128 * 1024 * 1024);
    wchar_t static_buff[256];
    size_t size = 0;
    wchar_t *buff = nullptr;
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
            buff = static_cast<wchar_t *>(realloc((buff == static_buff ? nullptr : buff), size));
            assert(buff != nullptr);
        }

        // Try printing.
        va_list va;
        va_copy(va, va_orig);
        status = std::vswprintf(buff, size / sizeof(wchar_t), format, va);
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

wchar_t *quote_end(const wchar_t *pos) {
    wchar_t c = *pos;

    while (true) {
        pos++;

        if (!*pos) return nullptr;

        if (*pos == L'\\') {
            pos++;
            if (!*pos) return nullptr;
        } else {
            if (*pos == c) {
                return const_cast<wchar_t *>(pos);
            }
        }
    }
    return nullptr;
}

void fish_setlocale() {
    // Use various Unicode symbols if they can be encoded using the current locale, else a simple
    // ASCII char alternative. All of the can_be_encoded() invocations should return the same
    // true/false value since the code points are in the BMP but we're going to be paranoid. This
    // is also technically wrong if we're not in a Unicode locale but we expect (or hope)
    // can_be_encoded() will return false in that case.
    if (can_be_encoded(L'\u2026')) {
        ellipsis_char = L'\u2026';
        ellipsis_str = L"\u2026";
    } else {
        ellipsis_char = L'$';  // "horizontal ellipsis"
        ellipsis_str = L"...";
    }

    if (is_windows_subsystem_for_linux()) {
        // neither of \u23CE and \u25CF can be displayed in the default fonts on Windows, though
        // they can be *encoded* just fine. Use alternative glyphs.
        omitted_newline_str = L"\u00b6";  // "pilcrow"
        omitted_newline_width = 1;
        obfuscation_read_char = L'\u2022';  // "bullet"
    } else if (is_console_session()) {
        omitted_newline_str = L"^J";
        omitted_newline_width = 2;
        obfuscation_read_char = L'*';
    } else {
        if (can_be_encoded(L'\u23CE')) {
            omitted_newline_str = L"\u23CE";
            omitted_newline_width = 1;
        } else {
            omitted_newline_str = L"^J";
            omitted_newline_width = 2;
        }
        obfuscation_read_char = can_be_encoded(L'\u25CF') ? L'\u25CF' : L'#';  // "black circle"
    }
}

long read_blocked(int fd, void *buf, size_t count) {
    long bytes_read = 0;

    while (count) {
        ssize_t res = read(fd, static_cast<char *>(buf) + bytes_read, count);
        if (res == 0) {
            break;
        } else if (res == -1) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN) return bytes_read ? bytes_read : -1;
            return -1;
        } else {
            bytes_read += res;
            count -= res;
        }
    }

    return bytes_read;
}

/// Loop a write request while failure is non-critical. Return -1 and set errno in case of critical
/// error.
ssize_t write_loop(int fd, const char *buff, size_t count) {
    size_t out_cum = 0;
    while (out_cum < count) {
        ssize_t out = write(fd, &buff[out_cum], count - out_cum);
        if (out < 0) {
            if (errno != EAGAIN && errno != EINTR) {
                return -1;
            }
        } else {
            out_cum += static_cast<size_t>(out);
        }
    }
    return static_cast<ssize_t>(out_cum);
}

ssize_t read_loop(int fd, void *buff, size_t count) {
    ssize_t result;
    do {
        result = read(fd, buff, count);
    } while (result < 0 && (errno == EAGAIN || errno == EINTR));
    return result;
}

/// Hack to not print error messages in the tests. Do not call this from functions in this module
/// like `debug()`. It is only intended to suppress diagnostic noise from testing things like the
/// fish parser where we expect a lot of diagnostic messages due to testing error conditions.
bool should_suppress_stderr_for_tests() {
    return program_name && !std::wcscmp(program_name, TESTS_PROGRAM_NAME);
}

static void debug_shared(const wchar_t level, const wcstring &msg) {
    pid_t current_pid;
    if (!is_forked_child()) {
        std::fwprintf(stderr, L"<%lc> %ls: %ls\n", static_cast<unsigned long>(level), program_name,
                      msg.c_str());
    } else {
        current_pid = getpid();
        std::fwprintf(stderr, L"<%lc> %ls: %d: %ls\n", static_cast<unsigned long>(level),
                      program_name, current_pid, msg.c_str());
    }
}

static const wchar_t level_char[] = {L'E', L'W', L'2', L'3', L'4', L'5'};
[[gnu::noinline]] void debug_impl(int level, const wchar_t *msg, ...) {
    int errno_old = errno;
    va_list va;
    va_start(va, msg);
    wcstring local_msg = vformat_string(msg, va);
    va_end(va);
    const wchar_t msg_level = level <= 5 ? level_char[level] : L'9';
    debug_shared(msg_level, local_msg);
    if (debug_stack_frames > 0) {
        show_stackframe(msg_level, debug_stack_frames, 1);
    }
    errno = errno_old;
}

[[gnu::noinline]] void debug_impl(int level, const char *msg, ...) {
    if (!should_debug(level)) return;
    int errno_old = errno;
    char local_msg[512];
    va_list va;
    va_start(va, msg);
    vsnprintf(local_msg, sizeof local_msg, msg, va);
    va_end(va);
    const wchar_t msg_level = level <= 5 ? level_char[level] : L'9';
    debug_shared(msg_level, str2wcstring(local_msg));
    if (debug_stack_frames > 0) {
        show_stackframe(msg_level, debug_stack_frames, 1);
    }
    errno = errno_old;
}

void debug_safe(int level, const char *msg, const char *param1, const char *param2,
                const char *param3, const char *param4, const char *param5, const char *param6,
                const char *param7, const char *param8, const char *param9, const char *param10,
                const char *param11, const char *param12) {
    const char *const params[] = {param1, param2, param3, param4,  param5,  param6,
                                  param7, param8, param9, param10, param11, param12};
    if (!msg) return;

    // Can't call fwprintf, that may allocate memory Just call write() over and over.
    if (level > debug_level) return;
    int errno_old = errno;

    size_t param_idx = 0;
    const char *cursor = msg;
    while (*cursor != '\0') {
        const char *end = std::strchr(cursor, '%');
        if (end == nullptr) end = cursor + std::strlen(cursor);

        ignore_result(write(STDERR_FILENO, cursor, end - cursor));

        if (end[0] == '%' && end[1] == 's') {
            // Handle a format string.
            assert(param_idx < sizeof params / sizeof *params);
            const char *format = params[param_idx++];
            if (!format) format = "(null)";
            ignore_result(write(STDERR_FILENO, format, std::strlen(format)));
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
    ignore_result(write(STDERR_FILENO, "\n", 1));

    errno = errno_old;
}

// Careful to not negate LLONG_MIN.
static unsigned long long absolute_value(long long x) {
    if (x >= 0) return static_cast<unsigned long long>(x);
    x = -(x + 1);
    return static_cast<unsigned long long>(x) + 1;
}

template <typename CharT>
void format_safe_impl(CharT *buff, size_t size, unsigned long long val) {
    size_t idx = 0;
    if (val == 0) {
        buff[idx++] = '0';
    } else {
        // Generate the string backwards, then reverse it.
        while (val != 0) {
            buff[idx++] = (val % 10) + '0';
            val /= 10;
        }
        std::reverse(buff, buff + idx);
    }
    buff[idx++] = '\0';
    assert(idx <= size && "Buffer overflowed");
}

void format_long_safe(char buff[64], long val) {
    unsigned long long uval = absolute_value(val);
    if (val >= 0) {
        format_safe_impl(buff, 64, uval);
    } else {
        buff[0] = '-';
        format_safe_impl(buff + 1, 63, uval);
    }
}

void format_long_safe(wchar_t buff[64], long val) {
    unsigned long long uval = absolute_value(val);
    if (val >= 0) {
        format_safe_impl(buff, 64, uval);
    } else {
        buff[0] = '-';
        format_safe_impl(buff + 1, 63, uval);
    }
}

void format_ullong_safe(wchar_t buff[64], unsigned long long val) {
    return format_safe_impl(buff, 64, val);
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
        while (true) {
            int overflow = 0;

            int tok_width = 0;

            // Tokenize on whitespace, and also calculate the width of the token.
            while (*pos && (!std::wcschr(L" \n\r\t", *pos))) {
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
                pos = pos + 1;
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

/// Escape a string in a fashion suitable for using as a URL. Store the result in out_str.
static void escape_string_url(const wcstring &in, wcstring &out) {
    const std::string narrow = wcs2string(in);
    for (auto &c1 : narrow) {
        // This silliness is so we get the correct result whether chars are signed or unsigned.
        unsigned int c2 = static_cast<unsigned int>(c1) & 0xFF;
        if (!(c2 & 0x80) &&
            (isalnum(c2) || c2 == '/' || c2 == '.' || c2 == '~' || c2 == '-' || c2 == '_')) {
            // The above characters don't need to be encoded.
            out.push_back(static_cast<wchar_t>(c2));
        } else {
            // All other chars need to have their UTF-8 representation encoded in hex.
            wchar_t buf[4];
            swprintf(buf, sizeof buf / sizeof buf[0], L"%%%02X", c2);
            out.append(buf);
        }
    }
}

/// Reverse the effects of `escape_string_url()`. By definition the string has consist of just ASCII
/// chars.
static bool unescape_string_url(const wchar_t *in, wcstring *out) {
    std::string result;
    result.reserve(out->size());
    for (wchar_t c = *in; c; c = *++in) {
        if (c > 0x7F) return false;  // invalid character means we can't decode the string
        if (c == '%') {
            int c1 = in[1];
            if (c1 == 0) return false;  // found unexpected end of string
            if (c1 == '%') {
                result.push_back('%');
                in++;
            } else {
                int c2 = in[2];
                if (c2 == 0) return false;  // string ended prematurely
                long d1 = convert_digit(c1, 16);
                if (d1 < 0) return false;
                long d2 = convert_digit(c2, 16);
                if (d2 < 0) return false;
                result.push_back(16 * d1 + d2);
                in += 2;
            }
        } else {
            result.push_back(c);
        }
    }

    *out = str2wcstring(result);
    return true;
}

/// Escape a string in a fashion suitable for using as a fish var name. Store the result in out_str.
static void escape_string_var(const wcstring &in, wcstring &out) {
    bool prev_was_hex_encoded = false;
    const std::string narrow = wcs2string(in);
    for (auto c1 : narrow) {
        // This silliness is so we get the correct result whether chars are signed or unsigned.
        unsigned int c2 = static_cast<unsigned int>(c1) & 0xFF;
        if (!(c2 & 0x80) && isalnum(c2) && (!prev_was_hex_encoded || !is_hex_digit(c2))) {
            // ASCII alphanumerics don't need to be encoded.
            if (prev_was_hex_encoded) {
                out.push_back(L'_');
                prev_was_hex_encoded = false;
            }
            out.push_back(static_cast<wchar_t>(c2));
        } else if (c2 == '_') {
            // Underscores are encoded by doubling them.
            out.append(L"__");
            prev_was_hex_encoded = false;
        } else {
            // All other chars need to have their UTF-8 representation encoded in hex.
            wchar_t buf[4];
            swprintf(buf, sizeof buf / sizeof buf[0], L"_%02X", c2);
            out.append(buf);
            prev_was_hex_encoded = true;
        }
    }
    if (prev_was_hex_encoded) {
        out.push_back(L'_');
    }
}

/// Reverse the effects of `escape_string_var()`. By definition the string has consist of just ASCII
/// chars.
static bool unescape_string_var(const wchar_t *in, wcstring *out) {
    std::string result;
    result.reserve(out->size());
    bool prev_was_hex_encoded = false;
    for (wchar_t c = *in; c; c = *++in) {
        if (c > 0x7F) return false;  // invalid character means we can't decode the string
        if (c == '_') {
            int c1 = in[1];
            if (c1 == 0) {
                if (prev_was_hex_encoded) break;
                return false;  // found unexpected escape char at end of string
            }
            if (c1 == '_') {
                result.push_back('_');
                in++;
            } else if (is_hex_digit(c1)) {
                int c2 = in[2];
                if (c2 == 0) return false;  // string ended prematurely
                long d1 = convert_hex_digit(c1);
                if (d1 < 0) return false;
                long d2 = convert_hex_digit(c2);
                if (d2 < 0) return false;
                result.push_back(16 * d1 + d2);
                in += 2;
                prev_was_hex_encoded = true;
            }
            // No "else" clause because if the first char after an underscore is not another
            // underscore or a valid hex character then the underscore is there to improve
            // readability after we've encoded a character not valid in a var name.
        } else {
            result.push_back(c);
        }
    }

    *out = str2wcstring(result);
    return true;
}

/// Escape a string in a fashion suitable for using in fish script. Store the result in out_str.
static void escape_string_script(const wchar_t *orig_in, size_t in_len, wcstring &out,
                                 escape_flags_t flags) {
    const wchar_t *in = orig_in;
    const bool escape_all = static_cast<bool>(flags & ESCAPE_ALL);
    const bool no_quoted = static_cast<bool>(flags & ESCAPE_NO_QUOTED);
    const bool no_tilde = static_cast<bool>(flags & ESCAPE_NO_TILDE);
    const bool no_caret = feature_test(features_t::stderr_nocaret);
    const bool no_qmark = feature_test(features_t::qmark_noglob);

    int need_escape = 0;
    int need_complex_escape = 0;

    if (!no_quoted && in_len == 0) {
        out.assign(L"''");
        return;
    }

    for (size_t i = 0; i < in_len; i++) {
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
                case L'\x1B': {
                    out += L'\\';
                    out += L'e';
                    need_escape = need_complex_escape = 1;
                    break;
                }
                case L'\\':
                case L'\'': {
                    need_escape = need_complex_escape = 1;
                    out += L'\\';
                    out += *in;
                    break;
                }
                case ANY_CHAR: {
                    // See #1614
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
                    bool char_is_normal = (c == L'~' && no_tilde) || (c == L'^' && no_caret) ||
                                          (c == L'?' && no_qmark);
                    if (!char_is_normal) {
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

/// Escapes a string for use in a regex string. Not safe for use with `eval` as only
/// characters reserved by PCRE2 are escaped, i.e. it relies on fish's automatic escaping
/// of subshell output in subsequent concatenation or for use as an argument.
/// \param in is the raw string to be searched for literally when substituted in a PCRE2 expression.
static wcstring escape_string_pcre2(const wcstring &in) {
    wcstring out;
    out.reserve(in.size() * 1.3);  // a wild guess

    for (auto c : in) {
        switch (c) {
            case L'.':
            case L'^':
            case L'$':
            case L'*':
            case L'+':
            case L'(':
            case L')':
            case L'?':
            case L'[':
            case L'{':
            case L'}':
            case L'\\':
            case L'|':
            // these two only *need* to be escaped within a character class, and technically it
            // makes no sense to ever use process substitution output to compose a character class,
            // but...
            case L'-':
            case L']':
                out.push_back('\\');
                /* FALLTHROUGH */
            default:
                out.push_back(c);
        }
    }

    return out;
}

wcstring escape_string(const wchar_t *in, escape_flags_t flags, escape_string_style_t style) {
    wcstring result;

    switch (style) {
        case STRING_STYLE_SCRIPT: {
            escape_string_script(in, std::wcslen(in), result, flags);
            break;
        }
        case STRING_STYLE_URL: {
            escape_string_url(in, result);
            break;
        }
        case STRING_STYLE_VAR: {
            escape_string_var(in, result);
            break;
        }
        case STRING_STYLE_REGEX: {
            result = escape_string_pcre2(in);
            break;
        }
    }

    return result;
}

wcstring escape_string(const wcstring &in, escape_flags_t flags, escape_string_style_t style) {
    wcstring result;

    switch (style) {
        case STRING_STYLE_SCRIPT: {
            escape_string_script(in.c_str(), in.size(), result, flags);
            break;
        }
        case STRING_STYLE_URL: {
            escape_string_url(in, result);
            break;
        }
        case STRING_STYLE_VAR: {
            escape_string_var(in, result);
            break;
        }
        case STRING_STYLE_REGEX: {
            result = escape_string_pcre2(in);
            break;
        }
    }

    return result;
}

wcstring debug_escape(const wcstring &in) {
    wcstring result;
    result.reserve(in.size());
    for (wchar_t wc : in) {
        uint32_t c = static_cast<uint32_t>(wc);
        if (c <= 127 && isprint(c)) {
            result.push_back(wc);
            continue;
        }

#define TEST(x)                             \
    case x:                                 \
        append_format(result, L"<%s>", #x); \
        break;
        switch (wc) {
            TEST(HOME_DIRECTORY)
            TEST(VARIABLE_EXPAND)
            TEST(VARIABLE_EXPAND_SINGLE)
            TEST(BRACE_BEGIN)
            TEST(BRACE_END)
            TEST(BRACE_SEP)
            TEST(BRACE_SPACE)
            TEST(INTERNAL_SEPARATOR)
            TEST(VARIABLE_EXPAND_EMPTY)
            TEST(EXPAND_SENTINEL)
            TEST(ANY_CHAR)
            TEST(ANY_STRING)
            TEST(ANY_STRING_RECURSIVE)
            TEST(ANY_SENTINEL)
            default:
                append_format(result, L"<\\x%02x>", c);
                break;
        }
    }
    return result;
}

/// Helper to return the last character in a string, or none.
static maybe_t<wchar_t> string_last_char(const wcstring &str) {
    if (str.empty()) return none();
    return str.back();
}

/// Given a null terminated string starting with a backslash, read the escape as if it is unquoted,
/// appending to result. Return the number of characters consumed, or 0 on error.
maybe_t<size_t> read_unquoted_escape(const wchar_t *input, wcstring *result, bool allow_incomplete,
                                     bool unescape_special) {
    assert(input[0] == L'\\' && "Not an escape");

    // Here's the character we'll ultimately append, or none. Note that L'\0' is a
    // valid thing to append.
    maybe_t<wchar_t> result_char_or_none = none();

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
                    if (0x10FFFF < max_val) max_val = static_cast<wchar_t>(0x10FFFF);
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
                result_char_or_none =
                    static_cast<wchar_t>((byte_literal ? ENCODE_DIRECT_BASE : 0) + res);
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
        // \x1B means escape.
        case L'e': {
            result_char_or_none = L'\x1B';
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
            result_char_or_none = none();
            break;
        }
        default: {
            if (unescape_special) result->push_back(INTERNAL_SEPARATOR);
            result_char_or_none = c;
            break;
        }
    }

    if (!errored && result_char_or_none.has_value()) {
        result->push_back(*result_char_or_none);
    }
    if (errored) return none();

    return in_pos;
}

/// Returns the unescaped version of input_str into output_str (by reference). Returns true if
/// successful. If false, the contents of output_str are undefined (!).
static bool unescape_string_internal(const wchar_t *const input, const size_t input_len,
                                     wcstring *output_str, unescape_flags_t flags) {
    // Set up result string, which we'll swap with the output on success.
    wcstring result;
    result.reserve(input_len);

    const bool unescape_special = static_cast<bool>(flags & UNESCAPE_SPECIAL);
    const bool allow_incomplete = static_cast<bool>(flags & UNESCAPE_INCOMPLETE);
    const bool ignore_backslashes = static_cast<bool>(flags & UNESCAPE_NO_BACKSLASHES);

    // The positions of open braces.
    std::vector<size_t> braces;
    // The positions of variable expansions or brace ","s.
    // We only read braces as expanders if there's a variable expansion or "," in them.
    std::vector<size_t> vars_or_seps;
    int brace_count = 0;

    bool errored = false;
    enum {
        mode_unquoted,
        mode_single_quotes,
        mode_double_quotes,
    } mode = mode_unquoted;

    for (size_t input_position = 0; input_position < input_len && !errored; input_position++) {
        const wchar_t c = input[input_position];
        // Here's the character we'll append to result, or none() to suppress it.
        maybe_t<wchar_t> to_append_or_none = c;
        if (mode == mode_unquoted) {
            switch (c) {
                case L'\\': {
                    if (!ignore_backslashes) {
                        // Backslashes (escapes) are complicated and may result in errors, or appending
                        // INTERNAL_SEPARATORs, so we have to handle them specially.
                        auto escape_chars = read_unquoted_escape(input + input_position, &result,
                                                                 allow_incomplete, unescape_special);
                        if (!escape_chars) {
                            // A none() return indicates an error.
                            errored = true;
                        } else {
                            // Skip over the characters we read, minus one because the outer loop will
                            // increment it.
                            assert(*escape_chars > 0);
                            input_position += *escape_chars - 1;
                        }
                        // We've already appended, don't append anything else.
                        to_append_or_none = none();
                    }
                    break;
                }
                case L'~': {
                    if (unescape_special && (input_position == 0)) {
                        to_append_or_none = HOME_DIRECTORY;
                    }
                    break;
                }
                case L'%': {
                    // Note that this only recognizes %self if the string is literally %self.
                    // %self/foo will NOT match this.
                    if (unescape_special && input_position == 0 &&
                        !std::wcscmp(input, PROCESS_EXPAND_SELF_STR)) {
                        to_append_or_none = PROCESS_EXPAND_SELF;
                        input_position += PROCESS_EXPAND_SELF_STR_LEN - 1;  // skip over 'self's
                    }
                    break;
                }
                case L'*': {
                    if (unescape_special) {
                        // In general, this is ANY_STRING. But as a hack, if the last appended char
                        // is ANY_STRING, delete the last char and store ANY_STRING_RECURSIVE to
                        // reflect the fact that ** is the recursive wildcard.
                        if (string_last_char(result) == ANY_STRING) {
                            assert(!result.empty());
                            result.resize(result.size() - 1);
                            to_append_or_none = ANY_STRING_RECURSIVE;
                        } else {
                            to_append_or_none = ANY_STRING;
                        }
                    }
                    break;
                }
                case L'?': {
                    if (unescape_special && !feature_test(features_t::qmark_noglob)) {
                        to_append_or_none = ANY_CHAR;
                    }
                    break;
                }
                case L'$': {
                    if (unescape_special) {
                        to_append_or_none = VARIABLE_EXPAND;
                        vars_or_seps.push_back(input_position);
                    }
                    break;
                }
                case L'{': {
                    if (unescape_special) {
                        brace_count++;
                        to_append_or_none = BRACE_BEGIN;
                        // We need to store where the brace *ends up* in the output.
                        braces.push_back(result.size());
                    }
                    break;
                }
                case L'}': {
                    if (unescape_special) {
                        // HACK: The completion machinery sometimes hands us partial tokens.
                        // We can't parse them properly, but it shouldn't hurt,
                        // so we don't assert here.
                        // See #4954.
                        // assert(brace_count > 0 && "imbalanced brackets are a tokenizer error, we
                        // shouldn't be able to get here");
                        brace_count--;
                        to_append_or_none = BRACE_END;
                        if (!braces.empty()) {
                            // If we didn't have a var or separator since the last '{',
                            // put the literal back.
                            if (vars_or_seps.empty() || vars_or_seps.back() < braces.back()) {
                                result[braces.back()] = L'{';
                                // We also need to turn all spaces back.
                                for (size_t i = braces.back() + 1; i < result.size(); i++) {
                                    if (result[i] == BRACE_SPACE) result[i] = L' ';
                                }
                                to_append_or_none = L'}';
                            }

                            // Remove all seps inside the current brace pair, so if we have a
                            // surrounding pair we only get seps inside *that*.
                            if (!vars_or_seps.empty()) {
                                while (!vars_or_seps.empty() && vars_or_seps.back() > braces.back())
                                    vars_or_seps.pop_back();
                            }
                            braces.pop_back();
                        }
                    }
                    break;
                }
                case L',': {
                    if (unescape_special && brace_count > 0) {
                        to_append_or_none = BRACE_SEP;
                        vars_or_seps.push_back(input_position);
                    }
                    break;
                }
                case L' ': {
                    if (unescape_special && brace_count > 0) {
                        to_append_or_none = BRACE_SPACE;
                    }
                    break;
                }
                case L'\'': {
                    mode = mode_single_quotes;
                    to_append_or_none =
                        unescape_special ? maybe_t<wchar_t>(INTERNAL_SEPARATOR) : none();
                    break;
                }
                case L'\"': {
                    mode = mode_double_quotes;
                    to_append_or_none =
                        unescape_special ? maybe_t<wchar_t>(INTERNAL_SEPARATOR) : none();
                    break;
                }
                default: {
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
                to_append_or_none =
                    unescape_special ? maybe_t<wchar_t>(INTERNAL_SEPARATOR) : none();
                mode = mode_unquoted;
            }
        } else if (mode == mode_double_quotes) {
            switch (c) {
                case L'"': {
                    mode = mode_unquoted;
                    to_append_or_none =
                        unescape_special ? maybe_t<wchar_t>(INTERNAL_SEPARATOR) : none();
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
                            to_append_or_none = none();
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
                        vars_or_seps.push_back(input_position);
                    }
                    break;
                }
                default: {
                    break;
                }
            }
        }

        // Now maybe append the char.
        if (to_append_or_none.has_value()) {
            result.push_back(*to_append_or_none);
        }
    }

    // Return the string by reference, and then success.
    if (!errored) {
        *output_str = std::move(result);
    }
    return !errored;
}

bool unescape_string_in_place(wcstring *str, unescape_flags_t escape_special) {
    assert(str != nullptr);
    wcstring output;
    bool success = unescape_string_internal(str->c_str(), str->size(), &output, escape_special);
    if (success) {
        *str = std::move(output);
    }
    return success;
}

bool unescape_string(const wchar_t *input, wcstring *output, unescape_flags_t escape_special,
                     escape_string_style_t style) {
    bool success = false;
    switch (style) {
        case STRING_STYLE_SCRIPT: {
            success = unescape_string_internal(input, std::wcslen(input), output, escape_special);
            break;
        }
        case STRING_STYLE_URL: {
            success = unescape_string_url(input, output);
            break;
        }
        case STRING_STYLE_VAR: {
            success = unescape_string_var(input, output);
            break;
        }
        case STRING_STYLE_REGEX: {
            // unescaping PCRE2 is not needed/supported, the PCRE2 engine is responsible for that
            success = false;
            break;
        }
    }
    if (!success) output->clear();
    return success;
}

bool unescape_string(const wcstring &input, wcstring *output, unescape_flags_t escape_special,
                     escape_string_style_t style) {
    bool success = false;
    switch (style) {
        case STRING_STYLE_SCRIPT: {
            success = unescape_string_internal(input.c_str(), input.size(), output, escape_special);
            break;
        }
        case STRING_STYLE_URL: {
            success = unescape_string_url(input.c_str(), output);
            break;
        }
        case STRING_STYLE_VAR: {
            success = unescape_string_var(input.c_str(), output);
            break;
        }
        case STRING_STYLE_REGEX: {
            // unescaping PCRE2 is not needed/supported, the PCRE2 engine is responsible for that
            success = false;
            break;
        }
    }
    if (!success) output->clear();
    return success;
}

/// Used to invalidate our idea of having a valid window size. This can occur when either the
/// COLUMNS or LINES variables are changed. This is also invoked when the shell regains control of
/// the tty since it is possible the terminal size changed while an external command was running.
void invalidate_termsize(bool invalidate_vars) {
    s_termsize_valid = false;
    if (invalidate_vars) {
        auto termsize = s_termsize.acquire();
        termsize->ws_col = termsize->ws_row = USHRT_MAX;
    }
}

/// Handle SIGWINCH. This is also invoked when the shell regains control of the tty since it is
/// possible the terminal size changed while an external command was running.
void common_handle_winch(int signal) {
    (void)signal;
    s_termsize_valid = false;
}

/// Validate the new terminal size. Fallback to the env vars if necessary. Ensure the values are
/// sane and if not fallback to a default of 80x24.
static void validate_new_termsize(struct winsize *new_termsize, const environment_t &vars) {
    if (new_termsize->ws_col == 0 || new_termsize->ws_row == 0) {
#ifdef HAVE_WINSIZE
        // Highly hackish. This seems like it should be moved.
        if (is_main_thread() && parser_t::principal_parser().is_interactive()) {
            FLOGF(warning, _(L"Current terminal parameters have rows and/or columns set to zero."));
            FLOGF(warning, _(L"The stty command can be used to correct this "
                             L"(e.g., stty rows 80 columns 24)."));
        }
#endif
        // Fallback to the environment vars.
        maybe_t<env_var_t> col_var = vars.get(L"COLUMNS");
        maybe_t<env_var_t> row_var = vars.get(L"LINES");
        if (!col_var.missing_or_empty() && !row_var.missing_or_empty()) {
            // Both vars have to have valid values.
            int col = fish_wcstoi(col_var->as_string().c_str());
            bool col_ok = errno == 0 && col > 0 && col <= USHRT_MAX;
            int row = fish_wcstoi(row_var->as_string().c_str());
            bool row_ok = errno == 0 && row > 0 && row <= USHRT_MAX;
            if (col_ok && row_ok) {
                new_termsize->ws_col = col;
                new_termsize->ws_row = row;
            }
        }
    }

    if (new_termsize->ws_col < MIN_TERM_COL || new_termsize->ws_row < MIN_TERM_ROW) {
        // Also highly hackisk.
        if (is_main_thread() && parser_t::principal_parser().is_interactive()) {
            FLOGF(warning,
                  _(L"Current terminal parameters set terminal size to unreasonable value."));
            FLOGF(warning, _(L"Defaulting terminal size to 80x24."));
        }
        new_termsize->ws_col = DFLT_TERM_COL;
        new_termsize->ws_row = DFLT_TERM_ROW;
    }
}

/// Export the new terminal size as env vars and to the kernel if possible.
static void export_new_termsize(struct winsize *new_termsize, env_stack_t &vars) {
    auto cols = vars.get(L"COLUMNS", ENV_EXPORT);
    vars.set_one(L"COLUMNS", ENV_GLOBAL | (cols.missing_or_empty() ? ENV_DEFAULT : ENV_EXPORT),
                 std::to_wstring(int(new_termsize->ws_col)));

    auto lines = vars.get(L"LINES", ENV_EXPORT);
    vars.set_one(L"LINES", ENV_GLOBAL | (lines.missing_or_empty() ? ENV_DEFAULT : ENV_EXPORT),
                 std::to_wstring(int(new_termsize->ws_row)));

#ifdef HAVE_WINSIZE
    // Only write the new terminal size if we are in the foreground (#4477)
    if (tcgetpgrp(STDOUT_FILENO) == getpgrp()) {
        ioctl(STDOUT_FILENO, TIOCSWINSZ, new_termsize);
    }
#endif
}

/// Get the current termsize, lazily computing it. Return by reference if it changed.
static struct winsize get_current_winsize_prim(bool *changed, const environment_t &vars) {
    auto termsize = s_termsize.acquire();
    if (s_termsize_valid) return *termsize;

    struct winsize new_termsize = {0, 0, 0, 0};
#ifdef HAVE_WINSIZE
    errno = 0;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &new_termsize) != -1 &&
        new_termsize.ws_col == termsize->ws_col && new_termsize.ws_row == termsize->ws_row) {
        s_termsize_valid = true;
        return *termsize;
    }
#endif
    validate_new_termsize(&new_termsize, vars);
    termsize->ws_col = new_termsize.ws_col;
    termsize->ws_row = new_termsize.ws_row;
    *changed = true;
    s_termsize_valid = true;
    return *termsize;
}

/// Updates termsize as needed, and returns a copy of the winsize.
struct winsize get_current_winsize() {
    bool changed = false;
    auto &vars = env_stack_t::globals();
    struct winsize termsize = get_current_winsize_prim(&changed, vars);
    if (changed) {
        // TODO: this may call us reentrantly through the environment dispatch mechanism. We need to
        // rationalize this.
        export_new_termsize(&termsize, vars);
        // Hack: due to the dispatch the termsize may have just become invalid. Stomp it back to
        // valid. What a mess.
        *s_termsize.acquire() = termsize;
        s_termsize_valid = true;
    }
    return termsize;
}

int common_get_width() { return get_current_winsize().ws_col; }

int common_get_height() { return get_current_winsize().ws_row; }

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
    } else if (limit_type >= fuzzy_match_substring_case_insensitive &&
               (location = ifind(match_against, string, true)) != wcstring::npos) {
        // A case-insensitive version of the string is in the match against.
        result.type = fuzzy_match_substring_case_insensitive;
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

[[gnu::noinline]] void bugreport() {
    FLOG(error, _(L"This is a bug. Break on 'bugreport' to debug."));
    FLOG(error, _(L"If you can reproduce it, please report: "), PACKAGE_BUGREPORT, L'.');
}

wcstring format_size(long long sz) {
    wcstring result;
    const wchar_t *sz_name[] = {L"kB", L"MB", L"GB", L"TB", L"PB", L"EB", L"ZB", L"YB", nullptr};

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
                long isz = (static_cast<long>(sz)) / 1024;
                if (isz > 9)
                    result.append(format_string(L"%d%ls", isz, sz_name[i]));
                else
                    result.append(
                        format_string(L"%.1f%ls", static_cast<double>(sz) / 1024, sz_name[i]));
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
    std::memset(buff, 0, buff_size);
    size_t idx = 0;
    const char *const sz_name[] = {"kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB", nullptr};
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
                    append_ull(buff, isz, &idx, max_len);

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

/// Return the number of seconds from the UNIX epoch, with subsecond precision. This function uses
/// the gettimeofday function and will have the same precision as that function.
double timef() {
    struct timeval tv;
    assert_with_errno(gettimeofday(&tv, nullptr) != -1);
    // return (double)tv.tv_sec + 0.000001 * tv.tv_usec;
    return static_cast<double>(tv.tv_sec) + 1e-6 * tv.tv_usec;
}

void exit_without_destructors(int code) { _exit(code); }

void autoclose_fd_t::close() {
    if (fd_ < 0) return;
    exec_close(fd_);
    fd_ = -1;
}

void exec_close(int fd) {
    assert(fd >= 0 && "Invalid fd");
    while (close(fd) == -1) {
        if (errno != EINTR) {
            wperror(L"close");
            break;
        }
    }
}

extern "C" {
[[gnu::noinline]] void debug_thread_error(void) {
    // Wait for a SIGINT. We can't use sigsuspend() because the signal may be delivered on another
    // thread.
    sigint_checker_t sigint;
    sigint.wait();
}
}

void set_main_thread() {
    // Just call thread_id() once to force increment of thread_id.
    uint64_t tid = thread_id();
    assert(tid == 1 && "main thread should have thread ID 1");
    (void)tid;
}

void configure_thread_assertions_for_testing() { thread_asserts_cfg_for_testing = true; }

bool is_forked_child() { return is_forked_proc; }

void setup_fork_guards() {
    is_forked_proc = false;
    static std::once_flag fork_guard_flag;
    std::call_once(fork_guard_flag,
                   [] { pthread_atfork(nullptr, nullptr, [] { is_forked_proc = true; }); });
}

void save_term_foreground_process_group() {
    ASSERT_IS_MAIN_THREAD();
    initial_fg_process_group = tcgetpgrp(STDIN_FILENO);
}

void restore_term_foreground_process_group() {
    if (initial_fg_process_group == -1) return;
    // This is called during shutdown and from a signal handler. We don't bother to complain on
    // failure because doing so is unlikely to be noticed.
    if (tcsetpgrp(STDIN_FILENO, initial_fg_process_group) == -1 && errno == ENOTTY) {
        redirect_tty_output();
    }
}

bool is_main_thread() { return thread_id() == 1; }

void assert_is_main_thread(const char *who) {
    if (!is_main_thread() && !thread_asserts_cfg_for_testing) {
        FLOGF(error, L"%s called off of main thread.", who);
        FLOGF(error, L"Break on debug_thread_error to debug.");
        debug_thread_error();
    }
}

void assert_is_not_forked_child(const char *who) {
    if (is_forked_child()) {
        FLOGF(error, L"%s called in a forked child.", who);
        FLOG(error, L"Break on debug_thread_error to debug.");
        debug_thread_error();
    }
}

void assert_is_background_thread(const char *who) {
    if (is_main_thread() && !thread_asserts_cfg_for_testing) {
        FLOGF(error, L"%s called on the main thread (may block!).", who);
        FLOG(error, L"Break on debug_thread_error to debug.");
        debug_thread_error();
    }
}

void assert_is_locked(void *vmutex, const char *who, const char *caller) {
    std::mutex *mutex = static_cast<std::mutex *>(vmutex);

    // Note that std::mutex.try_lock() is allowed to return false when the mutex isn't
    // actually locked; fortunately we are checking the opposite so we're safe.
    if (mutex->try_lock()) {
        FLOGF(error, L"%s is not locked when it should be in '%s'", who, caller);
        FLOG(error, L"Break on debug_thread_error to debug.");
        debug_thread_error();
        mutex->unlock();
    }
}

/// Test if the specified character is in a range that fish uses interally to store special tokens.
///
/// NOTE: This is used when tokenizing the input. It is also used when reading input, before
/// tokenization, to replace such chars with REPLACEMENT_WCHAR if they're not part of a quoted
/// string. We don't want external input to be able to feed reserved characters into our
/// lexer/parser or code evaluator.
//
// TODO: Actually implement the replacement as documented above.
bool fish_reserved_codepoint(wchar_t c) {
    return (c >= RESERVED_CHAR_BASE && c < RESERVED_CHAR_END) ||
           (c >= ENCODE_DIRECT_BASE && c < ENCODE_DIRECT_END);
}

/// Reopen stdin, stdout and/or stderr on /dev/null. This is invoked when we find that our tty has
/// become invalid.
void redirect_tty_output() {
    struct termios t;
    int fd = open("/dev/null", O_WRONLY);
    if (fd == -1) {
        __fish_assert("Could not open /dev/null!", __FILE__, __LINE__, errno);
    }
    if (tcgetattr(STDIN_FILENO, &t) == -1 && errno == EIO) dup2(fd, STDIN_FILENO);
    if (tcgetattr(STDOUT_FILENO, &t) == -1 && errno == EIO) dup2(fd, STDOUT_FILENO);
    if (tcgetattr(STDERR_FILENO, &t) == -1 && errno == EIO) dup2(fd, STDERR_FILENO);
    close(fd);
}

/// Display a failed assertion message, dump a stack trace if possible, then die.
[[noreturn]] void __fish_assert(const char *msg, const char *file, size_t line, int error) {
    if (error) {
        FLOGF(error, L"%s:%zu: failed assertion: %s: errno %d (%s)", file, line, msg, error,
              std::strerror(error));
    } else {
        FLOGF(error, L"%s:%zu: failed assertion: %s", file, line, msg);
    }
    show_stackframe(L'E', 99, 1);
    abort();
}

/// Test if the given char is valid in a variable name.
bool valid_var_name_char(wchar_t chr) { return fish_iswalnum(chr) || chr == L'_'; }

/// Test if the given string is a valid variable name.
bool valid_var_name(const wcstring &str) {
    return std::find_if_not(str.begin(), str.end(), valid_var_name_char) == str.end();
}

/// Test if the string is a valid function name.
bool valid_func_name(const wcstring &str) {
    if (str.empty()) return false;
    if (str.at(0) == L'-') return false;
    if (str.find_first_of(L'/') != wcstring::npos) return false;
    return true;
}

/// Return the path to the current executable. This needs to be realpath'd.
std::string get_executable_path(const char *argv0) {
    char buff[PATH_MAX];

#if __APPLE__
    // On OS X use it's proprietary API to get the path to the executable.
    // This is basically grabbing exec_path after argc, argv, envp, ...: for us
    // https://opensource.apple.com/source/adv_cmds/adv_cmds-163/ps/print.c
    uint32_t buffSize = sizeof buff;
    if (_NSGetExecutablePath(buff, &buffSize) == 0) return std::string(buff);
#elif __FreeBSD__
    // FreeBSD does not have /proc by default, but it can be mounted as procfs via the
    // Linux compatibility layer. Per sysctl(3), passing in a process ID of -1 returns
    // the value for the current process.
    size_t buff_size = sizeof buff;
    int name[] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    int result = sysctl(name, sizeof(name) / sizeof(int), buff, &buff_size, nullptr, 0);
    if (result != 0) {
        wperror(L"sysctl KERN_PROC_PATHNAME");
    } else {
        return std::string(buff);
    }
#else
    // On other unixes, fall back to the Linux-ish /proc/ directory
    ssize_t len;
    len = readlink("/proc/self/exe", buff, sizeof buff - 1);  // Linux
    if (len == -1) {
        len = readlink("/proc/curproc/file", buff, sizeof buff - 1);  // other BSDs
        if (len == -1) {
            len = readlink("/proc/self/path/a.out", buff, sizeof buff - 1);  // Solaris
        }
    }
    if (len > 0) {
        buff[len] = '\0';
        return std::string(buff);
    }
#endif

    // Just return argv0, which probably won't work (i.e. it's not an absolute path or a path
    // relative to the working directory, but instead something the caller found via $PATH). We'll
    // eventually fall back to the compile time paths.
    return std::string(argv0 ? argv0 : "");
}

/// Return a path to a directory where we can store temporary files.
std::string get_path_to_tmp_dir() {
    char *env_tmpdir = getenv("TMPDIR");
    if (env_tmpdir) {
        return env_tmpdir;
    }
#if defined(_CS_DARWIN_USER_TEMP_DIR)
    char osx_tmpdir[PATH_MAX];
    size_t n = confstr(_CS_DARWIN_USER_TEMP_DIR, osx_tmpdir, PATH_MAX);
    if (0 < n && n <= PATH_MAX) {
        return osx_tmpdir;
    } else {
        return "/tmp";
    }
#elif defined(P_tmpdir)
    return P_tmpdir;
#elif defined(_PATH_TMP)
    return _PATH_TMP;
#else
    return "/tmp";
#endif
}

// This function attempts to distinguish between a console session (at the actual login vty) and a
// session within a terminal emulator inside a desktop environment or over SSH. Unfortunately
// there are few values of $TERM that we can interpret as being exclusively console sessions, and
// most common operating systems do not use them. The value is cached for the duration of the fish
// session. We err on the side of assuming it's not a console session. This approach isn't
// bullet-proof and that's OK.
bool is_console_session() {
    static const bool console_session = []() {
        ASSERT_IS_MAIN_THREAD();

        const char *tty_name = ttyname(0);
        auto len = strlen("/dev/tty");
        const char *TERM = getenv("TERM");
        return
            // Test that the tty matches /dev/(console|dcons|tty[uv\d])
            tty_name &&
            ((strncmp(tty_name, "/dev/tty", len) == 0 &&
              (tty_name[len] == 'u' || tty_name[len] == 'v' || isdigit(tty_name[len]))) ||
             strcmp(tty_name, "/dev/dcons") == 0 || strcmp(tty_name, "/dev/console") == 0)
            // and that $TERM is simple, e.g. `xterm` or `vt100`, not `xterm-something`
            && (!TERM || !strchr(TERM, '-') || !strcmp(TERM, "sun-color"));
    }();
    return console_session;
}
