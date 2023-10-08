// Prototypes for various functions, mostly string utilities, that are used by most parts of fish.
#ifndef FISH_COMMON_H
#define FISH_COMMON_H
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <limits.h>
// Needed for va_list et al.
#include <stdarg.h>  // IWYU pragma: keep
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>  // IWYU pragma: keep
#endif
#include <sys/types.h>
#include <termios.h>

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "fallback.h"  // IWYU pragma: keep
#include "maybe.h"

// Create a generic define for all BSD platforms
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#define __BSD__
#endif

// PATH_MAX may not exist.
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// Define a symbol we can use elsewhere in our code to determine if we're being built on MS Windows
// under Cygwin.
#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(__CYGWIN__) || \
    defined(__WIN32__)
#define OS_IS_CYGWIN
#endif

// Check if Thread Sanitizer is enabled.
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define FISH_TSAN_WORKAROUNDS 1
#endif
#endif
#ifdef __SANITIZE_THREAD__
#define FISH_TSAN_WORKAROUNDS 1
#endif

// Common string type.
typedef std::wstring wcstring;

struct termsize_t;

// Highest legal ASCII value.
#define ASCII_MAX 127u

// Highest legal 16-bit Unicode value.
#define UCS2_MAX 0xFFFFu

// Highest legal byte value.
#define BYTE_MAX 0xFFu

// Unicode BOM value.
#define UTF8_BOM_WCHAR 0xFEFFu

// Use Unicode "non-characters" for internal characters as much as we can. This
// gives us 32 "characters" for internal use that we can guarantee should not
// appear in our input stream. See http://www.unicode.org/faq/private_use.html.
#define RESERVED_CHAR_BASE static_cast<wchar_t>(0xFDD0)
#define RESERVED_CHAR_END static_cast<wchar_t>(0xFDF0)
// Split the available non-character values into two ranges to ensure there are
// no conflicts among the places we use these special characters.
#define EXPAND_RESERVED_BASE RESERVED_CHAR_BASE
#define EXPAND_RESERVED_END (EXPAND_RESERVED_BASE + 16)
#define WILDCARD_RESERVED_BASE EXPAND_RESERVED_END
#define WILDCARD_RESERVED_END (WILDCARD_RESERVED_BASE + 16)
// Make sure the ranges defined above don't exceed the range for non-characters.
// This is to make sure we didn't do something stupid in subdividing the
// Unicode range for our needs.
// #if WILDCARD_RESERVED_END > RESERVED_CHAR_END
// #error
// #endif

// These are in the Unicode private-use range. We really shouldn't use this
// range but have little choice in the matter given how our lexer/parser works.
// We can't use non-characters for these two ranges because there are only 66 of
// them and we need at least 256 + 64.
//
// If sizeof(wchar_t))==4 we could avoid using private-use chars; however, that
// would result in fish having different behavior on machines with 16 versus 32
// bit wchar_t. It's better that fish behave the same on both types of systems.
//
// Note: We don't use the highest 8 bit range (0xF800 - 0xF8FF) because we know
// of at least one use of a codepoint in that range: the Apple symbol (0xF8FF)
// on Mac OS X. See http://www.unicode.org/faq/private_use.html.
#define ENCODE_DIRECT_BASE static_cast<wchar_t>(0xF600)
#define ENCODE_DIRECT_END (ENCODE_DIRECT_BASE + 256)

// NAME_MAX is not defined on Solaris
#if !defined(NAME_MAX)
#include <sys/param.h>
#if defined(MAXNAMELEN)
// MAXNAMELEN is defined on Linux, BSD, and Solaris among others
#define NAME_MAX MAXNAMELEN
#else
static_assert(false, "Neither NAME_MAX nor MAXNAMELEN is defined!");
#endif
#endif

// PATH_MAX may not exist.
#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
/// Fallback length of MAXPATHLEN. Hopefully a sane value.
#define PATH_MAX 4096
#endif
#endif

enum escape_string_style_t {
    STRING_STYLE_SCRIPT,
    STRING_STYLE_URL,
    STRING_STYLE_VAR,
    STRING_STYLE_REGEX,
};

// Flags for unescape_string functions.
enum {
    UNESCAPE_DEFAULT = 0,              // default behavior
    UNESCAPE_SPECIAL = 1 << 0,         // escape special fish syntax characters like the semicolon
    UNESCAPE_INCOMPLETE = 1 << 1,      // allow incomplete escape sequences
    UNESCAPE_NO_BACKSLASHES = 1 << 2,  // don't handle backslash escapes
};
typedef unsigned int unescape_flags_t;

// Flags for the escape_string() function. These are only applicable when the escape style is
// "script" (i.e., STRING_STYLE_SCRIPT).
enum {
    /// Do not escape special fish syntax characters like the semicolon. Only escape non-printable
    /// characters and backslashes.
    ESCAPE_NO_PRINTABLES = 1 << 0,
    /// Do not try to use 'simplified' quoted escapes, and do not use empty quotes as the empty
    /// string.
    ESCAPE_NO_QUOTED = 1 << 1,
    /// Do not escape tildes.
    ESCAPE_NO_TILDE = 1 << 2,
    /// Replace non-printable control characters with Unicode symbols.
    ESCAPE_SYMBOLIC = 1 << 3
};
typedef unsigned int escape_flags_t;

/// A user-visible job ID.
using job_id_t = int;

/// The non user-visible, never-recycled job ID.
/// Every job has a unique positive value for this.
using internal_job_id_t = uint64_t;

/// Exits without invoking destructors (via _exit), useful for code after fork.
[[noreturn]] void exit_without_destructors(int code);

/// Save the shell mode on startup so we can restore them on exit.
extern struct termios shell_modes;

/// The character to use where the text has been truncated. Is an ellipsis on unicode system and a $
/// on other systems.
wchar_t get_ellipsis_char();

/// The character or string to use where text has been truncated (ellipsis if possible, otherwise
/// ...)
const wchar_t *get_ellipsis_str();

/// Character representing an omitted newline at the end of text.
const wchar_t *get_omitted_newline_str();
int get_omitted_newline_width();

/// Character used for the silent mode of the read command
wchar_t get_obfuscation_read_char();

/// Profiling flag. True if commands should be profiled.
extern bool g_profiling_active;
void set_profiling_active(bool val);

/// Name of the current program. Should be set at startup. Used by the debug function.
extern const wchar_t *program_name;

/// Set to false if it's been determined we can't trust the last modified timestamp on the tty.
extern const bool has_working_tty_timestamps;

/// A global, empty string. This is useful for functions which wish to return a reference to an
/// empty string.
extern const wcstring g_empty_string;

/// A global, empty std::vector<wcstring>. This is useful for functions which wish to return a
/// reference to an empty string.
extern const std::vector<wcstring> g_empty_string_list;

// Pause for input, then exit the program. If supported, print a backtrace first.
#define FATAL_EXIT()                                \
    do {                                            \
        char exit_read_buff;                        \
        show_stackframe();                          \
        ignore_result(read(0, &exit_read_buff, 1)); \
        exit_without_destructors(1);                \
    } while (0)

/// Exit the program at once after emitting an error message and stack trace if possible.
/// We use our own private implementation of `assert()` for two reasons. First, some implementations
/// are subtly broken. For example, using `printf()` which can cause problems when mixed with wide
/// stdio functions and should be writing the message to stderr rather than stdout. Second, if
/// possible it is useful to provide additional context such as a stack backtrace.
#undef assert
#define assert(e) likely(e) ? ((void)0) : __fish_assert(#e, __FILE__, __LINE__, 0)
#define assert_with_errno(e) likely(e) ? ((void)0) : __fish_assert(#e, __FILE__, __LINE__, errno)
#define DIE(msg) __fish_assert(msg, __FILE__, __LINE__, 0)
#define DIE_WITH_ERRNO(msg) __fish_assert(msg, __FILE__, __LINE__, errno)
/// This macro is meant to be used with functions that return zero on success otherwise return an
/// errno value. Most notably the pthread family of functions which we never expect to fail.
#define DIE_ON_FAILURE(e)                                  \
    do {                                                   \
        int status = e;                                    \
        if (unlikely(status != 0)) {                       \
            __fish_assert(#e, __FILE__, __LINE__, status); \
        }                                                  \
    } while (0)

[[noreturn]] void __fish_assert(const char *msg, const char *file, size_t line, int error);

/// Shorthand for wgettext call in situations where a C-style string is needed (e.g.,
/// std::fwprintf()).
#define _(wstr) wgettext(wstr).c_str()

/// Noop, used to tell xgettext that a string should be translated. Use this when a string cannot be
/// passed through wgettext() at the point where it is used. For example, when initializing a
/// static array or structure. You must pass the string through wgettext() when it is used.
/// See https://developer.gnome.org/glib/stable/glib-I18N.html#N-:CAPS
#define N_(wstr) wstr

/// An empty struct which may be embedded (or inherited from) to prevent copying.
struct [[gnu::unused]] noncopyable_t {
    noncopyable_t() = default;
    noncopyable_t(noncopyable_t &&) = default;
    noncopyable_t &operator=(noncopyable_t &&) = default;
    noncopyable_t(const noncopyable_t &) = delete;
    noncopyable_t &operator=(const noncopyable_t &) = delete;
};

struct [[gnu::unused]] nonmovable_t {
    nonmovable_t() = default;
    nonmovable_t(nonmovable_t &&) = delete;
    nonmovable_t &operator=(nonmovable_t &&) = delete;
};

/// Test if a collection contains a value.
template <typename Col, typename T2>
bool contains(const Col &col, const T2 &val) {
    return std::find(std::begin(col), std::end(col), val) != std::end(col);
}

template <typename T1, typename T2>
bool contains(std::initializer_list<T1> col, const T2 &val) {
    return std::find(std::begin(col), std::end(col), val) != std::end(col);
}

/// Append a vector \p donator to the vector \p receiver.
template <typename T>
void vec_append(std::vector<T> &receiver, std::vector<T> &&donator) {
    if (receiver.empty()) {
        receiver = std::move(donator);
    } else {
        receiver.insert(receiver.end(), std::make_move_iterator(donator.begin()),
                        std::make_move_iterator(donator.end()));
    }
}

/// A function type to check for cancellation.
/// \return true if execution should cancel.
using cancel_checker_t = std::function<bool()>;

/// Print a stack trace to stderr.
void show_stackframe(int frame_count = 100, int skip_levels = 0);

/// Returns a  wide character string equivalent of the specified multibyte character string.
///
/// This function encodes illegal character sequences in a reversible way using the private use
/// area.
wcstring str2wcstring(const std::string &in);
wcstring str2wcstring(const std::string &in, size_t len);
wcstring str2wcstring(const char *in);
wcstring str2wcstring(const char *in, size_t len);

/// Returns a newly allocated multibyte character string equivalent of the specified wide character
/// string.
///
/// This function decodes illegal character sequences in a reversible way using the private use
/// area.
std::string wcs2string(const wcstring &input);
std::string wcs2string(const wchar_t *in, size_t len);

/// Same as wcs2string. Meant to be used when we need a zero-terminated string to feed legacy APIs.
std::string wcs2zstring(const wcstring &input);
std::string wcs2zstring(const wchar_t *in, size_t len);

/// Like wcs2string, but appends to \p receiver instead of returning a new string.
void wcs2string_appending(const wchar_t *in, size_t len, std::string *receiver);

// Check if we are running in the test mode, where we should suppress error output
#define TESTS_PROGRAM_NAME L"(ignore)"
bool should_suppress_stderr_for_tests();

/// Branch prediction hints. Idea borrowed from Linux kernel. Just used for asserts.
#define likely(x) __builtin_expect(bool(x), 1)
#define unlikely(x) __builtin_expect(bool(x), 0)

/// Format the specified size (in bytes, kilobytes, etc.) into the specified stringbuffer.
wcstring format_size(long long sz);

/// Version of format_size that does not allocate memory.
void format_size_safe(char buff[128], unsigned long long sz);

/// Writes out a long safely.
void format_long_safe(char buff[64], long val);
void format_long_safe(wchar_t buff[64], long val);
void format_llong_safe(wchar_t buff[64], long long val);
void format_ullong_safe(wchar_t buff[64], unsigned long long val);

/// "Narrows" a wide character string. This just grabs any ASCII characters and truncates.
void narrow_string_safe(char buff[64], const wchar_t *s);

/// Stored in blocks to reference the file which created the block.
using filename_ref_t = std::shared_ptr<wcstring>;

using scoped_lock = std::lock_guard<std::mutex>;

// An object wrapping a scoped lock and a value
// This is returned from owning_lock.acquire()
// Sample usage:
//   owning_lock<string> locked_name;
//   acquired_lock<string> name = name.acquire();
//   name.value = "derp"
//
// Or for simple cases:
//   name.acquire().value = "derp"
//
template <typename Data>
class acquired_lock : noncopyable_t {
    template <typename T>
    friend class owning_lock;

    template <typename T>
    friend class acquired_lock;

    acquired_lock(std::mutex &lk, Data *v) : lock(lk), value(v) {}
    acquired_lock(std::unique_lock<std::mutex> &&lk, Data *v) : lock(std::move(lk)), value(v) {}

    std::unique_lock<std::mutex> lock;
    Data *value;

   public:
    Data *operator->() { return value; }
    const Data *operator->() const { return value; }
    Data &operator*() { return *value; }
    const Data &operator*() const { return *value; }

    /// Implicit conversion to const version.
    operator acquired_lock<const Data>() {
        // We're about to give up our lock, don't hold onto the data.
        const Data *cvalue = value;
        value = nullptr;
        return acquired_lock<const Data>(std::move(lock), cvalue);
    }

    /// Create from a global lock.
    /// This is used in weird cases where a global lock protects more than one piece of data.
    static acquired_lock from_global(std::mutex &lk, Data *v) { return acquired_lock{lk, v}; }

    /// \return a reference to the lock, for use with a condition variable.
    std::unique_lock<std::mutex> &get_lock() { return lock; }
};

// A lock that owns a piece of data
// Access to the data is only provided by taking the lock
template <typename Data>
class owning_lock {
    // No copying
    owning_lock &operator=(const scoped_lock &) = delete;
    owning_lock(const scoped_lock &) = delete;
    owning_lock(owning_lock &&) = default;
    owning_lock &operator=(owning_lock &&) = default;

    std::mutex lock;
    Data data;

   public:
    owning_lock(Data &&d) : data(std::move(d)) {}
    owning_lock(const Data &d) : data(d) {}
    owning_lock() : data() {}

    acquired_lock<Data> acquire() { return {lock, &data}; }
};

/// A scoped manager to save the current value of some variable, and optionally set it to a new
/// value. On destruction it restores the variable to its old value.
///
/// This can be handy when there are multiple code paths to exit a block.
template <typename T>
class scoped_push {
    T *const ref;
    T saved_value;
    bool restored;

   public:
    explicit scoped_push(T *r) : ref(r), saved_value(*r), restored(false) {}

    scoped_push(T *r, T new_value) : ref(r), restored(false) {
        saved_value = std::move(*ref);
        *ref = std::move(new_value);
    }

    ~scoped_push() { restore(); }

    void restore() {
        if (!restored) {
            *ref = std::move(saved_value);
            restored = true;
        }
    }
};

wcstring format_string(const wchar_t *format, ...);
wcstring vformat_string(const wchar_t *format, va_list va_orig);
void append_format(wcstring &str, const wchar_t *format, ...);
void append_formatv(wcstring &target, const wchar_t *format, va_list va_orig);

#ifndef HAVE_STD__MAKE_UNIQUE
/// make_unique implementation
namespace std {
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args &&...args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
}  // namespace std
#endif
using std::make_unique;

/// This functions returns the end of the quoted substring beginning at \c pos. Returns 0 on error.
///
/// \param pos the position of the opening quote.
/// \param quote the quote to use, usually pointed to by \c pos.
const wchar_t *quote_end(const wchar_t *pos, wchar_t quote);

/// This functions returns the end of the comment substring beginning at \c pos.
///
/// \param pos the position where the comment starts, including the '#' symbol.
const wchar_t *comment_end(const wchar_t *pos);

/// This function should be called after calling `setlocale()` to perform fish specific locale
/// initialization.
void fish_setlocale();

/// Call read, blocking and repeating on EINTR. Exits on EAGAIN.
/// \return the number of bytes read, or 0 on EOF. On EAGAIN, returns -1 if nothing was read.
long read_blocked(int fd, void *buf, size_t count);

/// Loop a write request while failure is non-critical. Return -1 and set errno in case of critical
/// error.
ssize_t write_loop(int fd, const char *buff, size_t count);

/// Loop a read request while failure is non-critical. Return -1 and set errno in case of critical
/// error.
ssize_t read_loop(int fd, void *buff, size_t count);

/// Replace special characters with backslash escape sequences. Newline is replaced with \n, etc.
///
/// \param in The string to be escaped
/// \param flags Flags to control the escaping
/// \return The escaped string
wcstring escape_string(const wchar_t *in, escape_flags_t flags = 0,
                       escape_string_style_t style = STRING_STYLE_SCRIPT);
wcstring escape_string(const wcstring &in, escape_flags_t flags = 0,
                       escape_string_style_t style = STRING_STYLE_SCRIPT);

/// Escape a string so that it may be inserted into a double-quoted string.
/// This permits ownership transfer.
wcstring escape_string_for_double_quotes(wcstring in);

/// Expand backslashed escapes and substitute them with their unescaped counterparts. Also
/// optionally change the wildcards, the tilde character and a few more into constants which are
/// defined in a private use area of Unicode. This assumes wchar_t is a unicode character set.

/// Given a null terminated string starting with a backslash, read the escape as if it is unquoted,
/// appending to result. Return the number of characters consumed, or none() on error.
maybe_t<size_t> read_unquoted_escape(const wchar_t *input, wcstring *result, bool allow_incomplete,
                                     bool unescape_special);

/// Return the number of seconds from the UNIX epoch, with subsecond precision. This function uses
/// the gettimeofday function and will have the same precision as that function.
using timepoint_t = double;
timepoint_t timef();

/// Save the value of tcgetpgrp so we can restore it on exit.
void save_term_foreground_process_group();
void restore_term_foreground_process_group_for_exit();

/// Determines if we are running under Microsoft's Windows Subsystem for Linux to work around
/// some known limitations and/or bugs.
/// See https://github.com/Microsoft/WSL/issues/423 and Microsoft/WSL#2997
bool is_windows_subsystem_for_linux();

/// Detect if we are running under Cygwin or Cygwin64
constexpr bool is_cygwin() {
#ifdef __CYGWIN__
    return true;
#else
    return false;
#endif
}

extern "C" {
[[gnu::noinline]] void debug_thread_error(void);
}

/// Converts from wide char to digit in the specified base. If d is not a valid digit in the
/// specified base, return -1.
long convert_digit(wchar_t d, int base);

/// This is a macro that can be used to silence "unused parameter" warnings from the compiler for
/// functions which need to accept parameters they do not use because they need to be compatible
/// with an interface. It's similar to the Python idiom of doing `_ = expr` at the top of a
/// function in the same situation.
#define UNUSED(expr)  \
    do {              \
        (void)(expr); \
    } while (0)

// Return true if the character is in a range reserved for fish's private use.
bool fish_reserved_codepoint(wchar_t c);

void redirect_tty_output();

std::string get_path_to_tmp_dir();

bool valid_var_name_char(wchar_t chr);
bool valid_var_name(const wcstring &str);
bool valid_var_name(const wchar_t *str);
bool valid_func_name(const wcstring &str);

// Return values (`$status` values for fish scripts) for various situations.
enum {
    /// The status code used for normal exit in a command.
    STATUS_CMD_OK = 0,
    /// The status code used for failure exit in a command (but not if the args were invalid).
    STATUS_CMD_ERROR = 1,
    /// The status code used for invalid arguments given to a command. This is distinct from valid
    /// arguments that might result in a command failure. An invalid args condition is something
    /// like an unrecognized flag, missing or too many arguments, an invalid integer, etc. But
    STATUS_INVALID_ARGS = 2,

    /// The status code used when a command was not found.
    STATUS_CMD_UNKNOWN = 127,

    /// The status code used when an external command can not be run.
    STATUS_NOT_EXECUTABLE = 126,

    /// The status code used when a wildcard had no matches.
    STATUS_UNMATCHED_WILDCARD = 124,
    /// The status code used when illegal command name is encountered.
    STATUS_ILLEGAL_CMD = 123,
    /// The status code used when `read` is asked to consume too much data.
    STATUS_READ_TOO_MUCH = 122,
    /// The status code when an expansion fails, for example, "$foo["
    STATUS_EXPAND_ERROR = 121,
};

/* Normally casting an expression to void discards its value, but GCC
   versions 3.4 and newer have __attribute__ ((__warn_unused_result__))
   which may cause unwanted diagnostics in that case.  Use __typeof__
   and __extension__ to work around the problem, if the workaround is
   known to be needed.  */
#if 3 < __GNUC__ + (4 <= __GNUC_MINOR__)
#define ignore_result(x)         \
    (__extension__({             \
        __typeof__(x) __x = (x); \
        (void)__x;               \
    }))
#else
#define ignore_result(x) ((void)(x))
#endif

// Custom hash function used by unordered_map/unordered_set when key is const
#ifndef CONST_WCSTRING_HASH
#define CONST_WCSTRING_HASH 1
namespace std {
template <>
struct hash<const wcstring> {
    std::size_t operator()(const wcstring &w) const {
        std::hash<wcstring> hasher;
        return hasher(w);
    }
};
}  // namespace std
#endif

/// Get the absolute path to the fish executable itself
std::string get_executable_path(const char *argv0);

/// A RAII wrapper for resources that don't recur, so we don't have to create a separate RAII
/// wrapper for each function. Avoids needing to call "return cleanup()" or similar / everywhere.
struct cleanup_t {
   private:
    const std::function<void()> cleanup;

   public:
    cleanup_t(std::function<void()> exit_actions) : cleanup{std::move(exit_actions)} {}
    ~cleanup_t() { cleanup(); }
};

bool is_console_session();

/// Compile-time agnostic-size strcmp/wcscmp implementation. Unicode-unaware.
template <typename T>
constexpr ssize_t const_strcmp(const T *lhs, const T *rhs) {
    return (*lhs == *rhs) ? (*lhs == 0 ? 0 : const_strcmp(lhs + 1, rhs + 1))
                          : (*lhs > *rhs ? 1 : -1);
}

/// Compile-time agnostic-size strlen/wcslen implementation. Unicode-unaware.
template <typename T, size_t N>
constexpr size_t const_strlen(const T (&val)[N], size_t last_checked_idx = N,
                              size_t first_nul_idx = N) {
    // Assume there's a nul char at the end (index N) but there may be one before that that.
    return last_checked_idx == 0
               ? first_nul_idx
               : const_strlen(val, last_checked_idx - 1,
                              val[last_checked_idx - 1] ? first_nul_idx : last_checked_idx - 1);
}

/// \return true if the array \p vals is sorted by its name property.
template <typename T, size_t N>
constexpr bool is_sorted_by_name(const T (&vals)[N], size_t idx = 1) {
    return idx >= N ? true
                    : (const_strcmp(vals[idx - 1].name, vals[idx].name) <= 0 &&
                       is_sorted_by_name(vals, idx + 1));
}
#define ASSERT_SORTED_BY_NAME(x) static_assert(is_sorted_by_name(x), #x " not sorted by name")

/// \return a pointer to the first entry with the given name, assuming the entries are sorted by
/// name. \return nullptr if not found.
template <typename T, size_t N>
const T *get_by_sorted_name(const wchar_t *name, const T (&vals)[N]) {
    assert(name && "Null name");
    auto is_less = [](const T &v, const wchar_t *n) -> bool { return std::wcscmp(v.name, n) < 0; };
    auto where = std::lower_bound(std::begin(vals), std::end(vals), name, is_less);
    if (where != std::end(vals) && std::wcscmp(where->name, name) == 0) {
        return &*where;
    }
    return nullptr;
}

template <typename T, size_t N>
const T *get_by_sorted_name(const wcstring &name, const T (&vals)[N]) {
    return get_by_sorted_name(name.c_str(), vals);
}

/// As established in 1ab81ab90d1a408702e11f081fdaaafa30636c31, iswdigit() is very slow under glibc,
/// and does nothing more than establish whether or not the single specified character is in the
/// range ('0','9').
__attribute__((always_inline)) bool inline iswdigit(const wchar_t c) {
    return c >= L'0' && c <= L'9';
}

#if INCLUDE_RUST_HEADERS
#include "common.rs.h"
#endif

#endif  // FISH_COMMON_H
