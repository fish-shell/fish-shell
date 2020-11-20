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

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
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

// Common string type.
typedef std::wstring wcstring;
typedef std::vector<wcstring> wcstring_list_t;

struct termsize_t;

// Maximum number of bytes used by a single utf-8 character.
#define MAX_UTF8_BYTES 6

// Highest legal ASCII value.
#define ASCII_MAX 127u

// Highest legal 16-bit Unicode value.
#define UCS2_MAX 0xFFFFu

// Highest legal byte value.
#define BYTE_MAX 0xFFu

// Unicode BOM value.
#define UTF8_BOM_WCHAR 0xFEFFu

// Unicode replacement character.
#define REPLACEMENT_WCHAR 0xFFFDu

// Use Unicode "noncharacters" for internal characters as much as we can. This
// gives us 32 "characters" for internal use that we can guarantee should not
// appear in our input stream. See http://www.unicode.org/faq/private_use.html.
#define RESERVED_CHAR_BASE static_cast<wchar_t>(0xFDD0)
#define RESERVED_CHAR_END static_cast<wchar_t>(0xFDF0)
// Split the available noncharacter values into two ranges to ensure there are
// no conflicts among the places we use these special characters.
#define EXPAND_RESERVED_BASE RESERVED_CHAR_BASE
#define EXPAND_RESERVED_END (EXPAND_RESERVED_BASE + 16)
#define WILDCARD_RESERVED_BASE EXPAND_RESERVED_END
#define WILDCARD_RESERVED_END (WILDCARD_RESERVED_BASE + 16)
// Make sure the ranges defined above don't exceed the range for noncharacters.
// This is to make sure we didn't do something stupid in subdividing the
// Unicode range for our needs.
//#if WILDCARD_RESERVED_END > RESERVED_CHAR_END
//#error
//#endif

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

// Flags for the escape_string() and escape_string() functions. These are only applicable when the
// escape style is "script" (i.e., STRING_STYLE_SCRIPT).
enum {
    /// Escape all characters, including magic characters like the semicolon.
    ESCAPE_ALL = 1 << 0,
    /// Do not try to use 'simplified' quoted escapes, and do not use empty quotes as the empty
    /// string.
    ESCAPE_NO_QUOTED = 1 << 1,
    /// Do not escape tildes.
    ESCAPE_NO_TILDE = 1 << 2
};
typedef unsigned int escape_flags_t;

/// Issue a debug message with printf-style string formating and automatic line breaking. The string
/// will begin with the string \c program_name, followed by a colon and a whitespace.
///
/// Because debug is often called to tell the user about an error, before using wperror to give a
/// specific error message, debug will never ever modify the value of errno.
///
/// \param level the priority of the message. Lower number means higher priority. Messages with a
/// priority_number higher than \c debug_level will be ignored..
/// \param msg the message format string.
///
/// Example:
///
/// <code>debug( 1, L"Pi = %.3f", M_PI );</code>
///
/// will print the string 'fish: Pi = 3.141', given that debug_level is 1 or higher, and that
/// program_name is 'fish'.
[[gnu::noinline, gnu::format(printf, 2, 3)]] void debug_impl(int level, const char *msg, ...);
[[gnu::noinline]] void debug_impl(int level, const wchar_t *msg, ...);

/// The verbosity level of fish. If a call to debug has a severity level higher than \c debug_level,
/// it will not be printed.
extern std::atomic<int> debug_level;

inline bool should_debug(int level) { return level <= debug_level.load(std::memory_order_relaxed); }

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

/// Name of the current program. Should be set at startup. Used by the debug function.
extern const wchar_t *program_name;

/// Set to false if it's been determined we can't trust the last modified timestamp on the tty.
extern const bool has_working_tty_timestamps;

// Pause for input, then exit the program. If supported, print a backtrace first.
// The `return` will never be run  but silences oclint warnings. Especially when this is called
// from within a `switch` block. As of the time I'm writing this oclint doesn't recognize the
// `__attribute__((noreturn))` on the exit_without_destructors() function.
// TODO: we use C++11 [[noreturn]] now, does that change things?
#define FATAL_EXIT()                                \
    do {                                            \
        char exit_read_buff;                        \
        show_stackframe(L'E');                      \
        ignore_result(read(0, &exit_read_buff, 1)); \
        exit_without_destructors(1);                \
    } while (0)

/// Exit the program at once after emitting an error message and stack trace if possible.
/// We use our own private implementation of `assert()` for two reasons. First, some implementations
/// are subtly broken. For example, using `printf()` which can cause problems when mixed with wide
/// stdio functions and should be writing the message to stderr rather than stdout. Second, if
/// possible it is useful to provide additional context such as a stack backtrace.
#undef assert
#define assert(e) (e) ? ((void)0) : __fish_assert(#e, __FILE__, __LINE__, 0)
#define assert_with_errno(e) (e) ? ((void)0) : __fish_assert(#e, __FILE__, __LINE__, errno)
#define DIE(msg) __fish_assert(msg, __FILE__, __LINE__, 0)
#define DIE_WITH_ERRNO(msg) __fish_assert(msg, __FILE__, __LINE__, errno)
/// This macro is meant to be used with functions that return zero on success otherwise return an
/// errno value. Most notably the pthread family of functions which we never expect to fail.
#define DIE_ON_FAILURE(e)                                  \
    do {                                                   \
        int status = e;                                    \
        if (status != 0) {                                 \
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

/// Test if a collection contains a value.
template <typename Col, typename T2>
bool contains(const Col &col, const T2 &val) {
    return std::find(std::begin(col), std::end(col), val) != std::end(col);
}

/// Append a vector \p donator to the vector \p receiver.
template <typename T>
void vec_append(std::vector<T> &receiver, std::vector<T> &&donator) {
    receiver.insert(receiver.end(), std::make_move_iterator(donator.begin()),
                    std::make_move_iterator(donator.end()));
}

/// Move an object into a shared_ptr.
template <typename T>
std::shared_ptr<T> move_to_sharedptr(T &&v) {
    return std::make_shared<T>(std::move(v));
}

/// A function type to check for cancellation.
/// \return true if execution should cancel.
using cancel_checker_t = std::function<bool()>;

/// Print a stack trace to stderr.
void show_stackframe(const wchar_t msg_level, int frame_count = 100, int skip_levels = 0);

/// Returns a  wide character string equivalent of the specified multibyte character string.
///
/// This function encodes illegal character sequences in a reversible way using the private use
/// area.
wcstring str2wcstring(const char *in);
wcstring str2wcstring(const char *in, size_t len);
wcstring str2wcstring(const std::string &in);
wcstring str2wcstring(const std::string &in, size_t len);

/// Returns a newly allocated multibyte character string equivalent of the specified wide character
/// string.
///
/// This function decodes illegal character sequences in a reversible way using the private use
/// area.
std::string wcs2string(const wcstring &input);

enum fuzzy_match_type_t {
    // We match the string exactly: FOOBAR matches FOOBAR.
    fuzzy_match_exact = 0,

    // We match a prefix of the string: FO matches FOOBAR.
    fuzzy_match_prefix,

    // We match the string exactly, but in a case insensitive way: foobar matches FOOBAR.
    fuzzy_match_case_insensitive,

    // We match a prefix of the string, in a case insensitive way: foo matches FOOBAR.
    fuzzy_match_prefix_case_insensitive,

    // We match a substring of the string: OOBA matches FOOBAR.
    fuzzy_match_substring,

    // We match a substring of the string: ooBA matches FOOBAR.
    fuzzy_match_substring_case_insensitive,

    // A subsequence match with insertions only: FBR matches FOOBAR.
    fuzzy_match_subsequence_insertions_only,

    // We don't match the string.
    fuzzy_match_none
};

/// Indicates where a match type requires replacing the entire token.
static inline bool match_type_requires_full_replacement(fuzzy_match_type_t t) {
    switch (t) {
        case fuzzy_match_exact:
        case fuzzy_match_prefix: {
            return false;
        }
        case fuzzy_match_case_insensitive:
        case fuzzy_match_prefix_case_insensitive:
        case fuzzy_match_substring:
        case fuzzy_match_substring_case_insensitive:
        case fuzzy_match_subsequence_insertions_only:
        case fuzzy_match_none: {
            return true;
        }
        default: {
            DIE("Unreachable");
            return false;
        }
    }
}

/// Indicates where a match shares a prefix with the string it matches.
static inline bool match_type_shares_prefix(fuzzy_match_type_t t) {
    switch (t) {
        case fuzzy_match_exact:
        case fuzzy_match_prefix:
        case fuzzy_match_case_insensitive:
        case fuzzy_match_prefix_case_insensitive: {
            return true;
        }
        case fuzzy_match_substring:
        case fuzzy_match_substring_case_insensitive:
        case fuzzy_match_subsequence_insertions_only:
        case fuzzy_match_none: {
            return false;
        }
        default: {
            DIE("Unreachabe");
            return false;
        }
    }
}

/// Test if string is a fuzzy match to another.
struct string_fuzzy_match_t {
    enum fuzzy_match_type_t type;

    // Strength of the match. The value depends on the type. Lower is stronger.
    size_t match_distance_first;
    size_t match_distance_second;

    // Constructor.
    explicit string_fuzzy_match_t(enum fuzzy_match_type_t t, size_t distance_first = 0,
                                  size_t distance_second = 0);

    // Return -1, 0, 1 if this match is (respectively) better than, equal to, or worse than rhs.
    int compare(const string_fuzzy_match_t &rhs) const;
};

/// Compute a fuzzy match for a string. If maximum_match is not fuzzy_match_none, limit the type to
/// matches at or below that type.
string_fuzzy_match_t string_fuzzy_match_string(const wcstring &string,
                                               const wcstring &match_against,
                                               fuzzy_match_type_t limit_type = fuzzy_match_none);

// Check if we are running in the test mode, where we should suppress error output
#define TESTS_PROGRAM_NAME L"(ignore)"
bool should_suppress_stderr_for_tests();

void assert_is_main_thread(const char *who);
#define ASSERT_IS_MAIN_THREAD_TRAMPOLINE(x) assert_is_main_thread(x)
#define ASSERT_IS_MAIN_THREAD() ASSERT_IS_MAIN_THREAD_TRAMPOLINE(__FUNCTION__)

void assert_is_background_thread(const char *who);
#define ASSERT_IS_BACKGROUND_THREAD_TRAMPOLINE(x) assert_is_background_thread(x)
#define ASSERT_IS_BACKGROUND_THREAD() ASSERT_IS_BACKGROUND_THREAD_TRAMPOLINE(__FUNCTION__)

/// Useful macro for asserting that a lock is locked. This doesn't check whether this thread locked
/// it, which it would be nice if it did, but here it is anyways.
void assert_is_locked(void *mutex, const char *who, const char *caller);
#define ASSERT_IS_LOCKED(x) assert_is_locked(reinterpret_cast<void *>(&x), #x, __FUNCTION__)

/// Format the specified size (in bytes, kilobytes, etc.) into the specified stringbuffer.
wcstring format_size(long long sz);

/// Version of format_size that does not allocate memory.
void format_size_safe(char buff[128], unsigned long long sz);

/// Our crappier versions of debug which is guaranteed to not allocate any memory, or do anything
/// other than call write(). This is useful after a call to fork() with threads.
void debug_safe(int level, const char *msg, const char *param1 = nullptr,
                const char *param2 = nullptr, const char *param3 = nullptr,
                const char *param4 = nullptr, const char *param5 = nullptr,
                const char *param6 = nullptr, const char *param7 = nullptr,
                const char *param8 = nullptr, const char *param9 = nullptr,
                const char *param10 = nullptr, const char *param11 = nullptr,
                const char *param12 = nullptr);

/// Writes out a long safely.
void format_long_safe(char buff[64], long val);
void format_long_safe(wchar_t buff[64], long val);
void format_ullong_safe(wchar_t buff[64], unsigned long long val);

/// "Narrows" a wide character string. This just grabs any ASCII characters and trunactes.
void narrow_string_safe(char buff[64], const wchar_t *s);

typedef std::lock_guard<std::mutex> scoped_lock;
typedef std::lock_guard<std::recursive_mutex> scoped_rlock;

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
class acquired_lock {
    template <typename T>
    friend class owning_lock;

    template <typename T>
    friend class acquired_lock;

    acquired_lock(std::mutex &lk, Data *v) : lock(lk), value(v) {}
    acquired_lock(std::unique_lock<std::mutex> &&lk, Data *v) : lock(std::move(lk)), value(v) {}

    std::unique_lock<std::mutex> lock;
    Data *value;

   public:
    // No copying, move construction only
    acquired_lock &operator=(const acquired_lock &) = delete;
    acquired_lock(const acquired_lock &) = delete;
    acquired_lock(acquired_lock &&) = default;
    acquired_lock &operator=(acquired_lock &&) = default;

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

/// A helper class for managing and automatically closing a file descriptor.
class autoclose_fd_t {
    int fd_;

   public:
    // Closes the fd if not already closed.
    void close();

    // Returns the fd.
    int fd() const { return fd_; }

    // Returns the fd, transferring ownership to the caller.
    int acquire() {
        int temp = fd_;
        fd_ = -1;
        return temp;
    }

    // Resets to a new fd, taking ownership.
    void reset(int fd) {
        if (fd == fd_) return;
        close();
        fd_ = fd;
    }

    // \return if this has a valid fd.
    bool valid() const { return fd_ >= 0; }

    autoclose_fd_t(const autoclose_fd_t &) = delete;
    void operator=(const autoclose_fd_t &) = delete;
    autoclose_fd_t(autoclose_fd_t &&rhs) : fd_(rhs.fd_) { rhs.fd_ = -1; }

    void operator=(autoclose_fd_t &&rhs) {
        close();
        std::swap(this->fd_, rhs.fd_);
    }

    explicit autoclose_fd_t(int fd = -1) : fd_(fd) {}
    ~autoclose_fd_t() { close(); }
};

/// Close a file descriptor \p fd, retrying on EINTR.
void exec_close(int fd);

wcstring format_string(const wchar_t *format, ...);
wcstring vformat_string(const wchar_t *format, va_list va_orig);
void append_format(wcstring &str, const wchar_t *format, ...);
void append_formatv(wcstring &target, const wchar_t *format, va_list va_orig);

#ifdef HAVE_STD__MAKE_UNIQUE
using std::make_unique;
#else
/// make_unique implementation
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args &&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#endif

/// This functions returns the end of the quoted substring beginning at \c in. The type of quoting
/// character is detemrined by examining \c in. Returns 0 on error.
///
/// \param in the position of the opening quote.
wchar_t *quote_end(const wchar_t *pos);

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
wcstring escape_string(const wchar_t *in, escape_flags_t flags,
                       escape_string_style_t style = STRING_STYLE_SCRIPT);
wcstring escape_string(const wcstring &in, escape_flags_t flags,
                       escape_string_style_t style = STRING_STYLE_SCRIPT);

/// \return a string representation suitable for debugging (not for presenting to the user). This
/// replaces non-ASCII characters with either tokens like <BRACE_SEP> or <\xfdd7>. No other escapes
/// are made (i.e. this is a lossy escape).
wcstring debug_escape(const wcstring &in);

/// Expand backslashed escapes and substitute them with their unescaped counterparts. Also
/// optionally change the wildcards, the tilde character and a few more into constants which are
/// defined in a private use area of Unicode. This assumes wchar_t is a unicode character set.

/// Given a null terminated string starting with a backslash, read the escape as if it is unquoted,
/// appending to result. Return the number of characters consumed, or none() on error.
maybe_t<size_t> read_unquoted_escape(const wchar_t *input, wcstring *result, bool allow_incomplete,
                                     bool unescape_special);

/// Unescapes a string in-place. A true result indicates the string was unescaped, a false result
/// indicates the string was unmodified.
bool unescape_string_in_place(wcstring *str, unescape_flags_t escape_special);

/// Reverse the effects of calling `escape_string`. Returns the unescaped value by reference. On
/// failure, the output is set to an empty string.
bool unescape_string(const wchar_t *input, wcstring *output, unescape_flags_t escape_special,
                     escape_string_style_t style = STRING_STYLE_SCRIPT);

bool unescape_string(const wcstring &input, wcstring *output, unescape_flags_t escape_special,
                     escape_string_style_t style = STRING_STYLE_SCRIPT);

/// Write the given paragraph of output, redoing linebreaks to fit \p termsize.
wcstring reformat_for_screen(const wcstring &msg, const termsize_t &termsize);

/// Print a short message about how to file a bug report to stderr.
void bugreport();

/// Return the number of seconds from the UNIX epoch, with subsecond precision. This function uses
/// the gettimeofday function and will have the same precision as that function.
double timef();

/// Call the following function early in main to set the main thread. This is our replacement for
/// pthread_main_np().
void set_main_thread();
bool is_main_thread();

/// Configures thread assertions for testing.
void configure_thread_assertions_for_testing();

/// Set up a guard to complain if we try to do certain things (like take a lock) after calling fork.
void setup_fork_guards(void);

/// Save the value of tcgetpgrp so we can restore it on exit.
void save_term_foreground_process_group();
void restore_term_foreground_process_group_for_exit();

/// Return whether we are the child of a fork.
bool is_forked_child(void);
void assert_is_not_forked_child(const char *who);
#define ASSERT_IS_NOT_FORKED_CHILD_TRAMPOLINE(x) assert_is_not_forked_child(x)
#define ASSERT_IS_NOT_FORKED_CHILD() ASSERT_IS_NOT_FORKED_CHILD_TRAMPOLINE(__FUNCTION__)

/// Determines if we are running under Microsoft's Windows Subsystem for Linux to work around
/// some known limitations and/or bugs.
/// See https://github.com/Microsoft/WSL/issues/423 and Microsoft/WSL#2997
bool is_windows_subsystem_for_linux();

/// Detect if we are running under Cygwin or Cgywin64
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

/// Used for constructing mappings between enums and strings. The resulting array must be sorted
/// according to the `str` member since str_to_enum() does a binary search. Also the last entry must
/// have NULL for the `str` member and the default value for `val` to be returned if the string
/// isn't found.
template <typename T>
struct enum_map {
    T val;
    const wchar_t *const str;
};

/// Given a string return the matching enum. Return the sentinel enum if no match is made. The map
/// must be sorted by the `str` member. A binary search is twice as fast as a linear search with 16
/// elements in the map.
template <typename T>
static T str_to_enum(const wchar_t *name, const enum_map<T> map[], int len) {
    // Ignore the sentinel value when searching as it is the "not found" value.
    size_t left = 0, right = len - 1;

    while (left < right) {
        size_t mid = left + (right - left) / 2;
        int cmp = std::wcscmp(name, map[mid].str);
        if (cmp < 0) {
            right = mid;  // name was smaller than mid
        } else if (cmp > 0) {
            left = mid + 1;  // name was larger than mid
        } else {
            return map[mid].val;  // found it
        }
    }
    return map[len - 1].val;  // return the sentinel value
}

/// Given an enum return the matching string.
template <typename T>
static const wchar_t *enum_to_str(T enum_val, const enum_map<T> map[]) {
    for (const enum_map<T> *entry = map; entry->str; entry++) {
        if (enum_val == entry->val) {
            return entry->str;
        }
    }
    return nullptr;
};

void redirect_tty_output();

std::string get_path_to_tmp_dir();

bool valid_var_name_char(wchar_t chr);
bool valid_var_name(const wcstring &str);
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
    /// TODO: Figure out why we have two distinct failure codes for when an external command cannot
    /// be run.
    ///
    /// The status code used when an external command can not be run.
    STATUS_NOT_EXECUTABLE = 126,
    /// The status code used when an external command can not be run.
    STATUS_EXEC_FAIL = 125,

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

#endif  // FISH_COMMON_H
