// Prototypes for various functions, mostly string utilities, that are used by most parts of fish.
#ifndef FISH_COMMON_H
#define FISH_COMMON_H
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>  // IWYU pragma: keep
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <wchar.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>  // IWYU pragma: keep
#endif

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "fallback.h"  // IWYU pragma: keep
#include "maybe.h"
#include "signal.h"  // IWYU pragma: keep

// Define a symbol we can use elsewhere in our code to determine if we're being built on MS Windows
// under Cygwin.
#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(__CYGWIN__) || \
    defined(__WIN32__)
#define OS_IS_CYGWIN
#endif

// Common string type.
typedef std::wstring wcstring;
typedef std::vector<wcstring> wcstring_list_t;

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
#define RESERVED_CHAR_BASE (wchar_t)0xFDD0
#define RESERVED_CHAR_END (wchar_t)0xFDF0
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
#define ENCODE_DIRECT_BASE (wchar_t)0xF600
#define ENCODE_DIRECT_END (ENCODE_DIRECT_BASE + 256)
#define INPUT_COMMON_BASE (wchar_t)0xF700
#define INPUT_COMMON_END (INPUT_COMMON_BASE + 64)

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

enum escape_string_style_t { STRING_STYLE_SCRIPT, STRING_STYLE_URL, STRING_STYLE_VAR };

// Flags for unescape_string functions.
enum {
    UNESCAPE_DEFAULT = 0,         // default behavior
    UNESCAPE_SPECIAL = 1 << 0,    // escape special fish syntax characters like the semicolon
    UNESCAPE_INCOMPLETE = 1 << 1  // allow incomplete escape sequences
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

// Directions.
enum selection_direction_t {
    // Visual directions.
    direction_north,
    direction_east,
    direction_south,
    direction_west,
    direction_page_north,
    direction_page_south,

    // Logical directions.
    direction_next,
    direction_prev,

    // Special value that means deselect.
    direction_deselect
};

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
void __attribute__((noinline)) debug_impl(int level, const char *msg, ...)
    __attribute__((format(printf, 2, 3)));
void __attribute__((noinline)) debug_impl(int level, const wchar_t *msg, ...);

/// The verbosity level of fish. If a call to debug has a severity level higher than \c debug_level,
/// it will not be printed.
extern int debug_level;

inline bool should_debug(int level) { return level <= debug_level; }

#define debug(level, ...)                                            \
    do {                                                             \
        if (should_debug((level))) debug_impl((level), __VA_ARGS__); \
    } while (0)

/// Exits without invoking destructors (via _exit), useful for code after fork.
[[noreturn]] void exit_without_destructors(int code);

/// Save the shell mode on startup so we can restore them on exit.
extern struct termios shell_modes;

/// The character to use where the text has been truncated. Is an ellipsis on unicode system and a $
/// on other systems.
extern wchar_t ellipsis_char;
/// The character or string to use where text has been truncated (ellipsis if possible, otherwise
/// ...)
extern const wchar_t *ellipsis_str;

/// Character representing an omitted newline at the end of text.
extern wchar_t omitted_newline_char;

/// Character used for the silent mode of the read command
extern wchar_t obfuscation_read_char;

/// How many stack frames to show when a debug() call is made.
extern int debug_stack_frames;

/// Profiling flag. True if commands should be profiled.
extern bool g_profiling_active;

/// Name of the current program. Should be set at startup. Used by the debug function.
extern const wchar_t *program_name;

/// Set to false if it's been determined we can't trust the last modified timestamp on the tty.
extern const bool has_working_tty_timestamps;

/// A list of all whitespace characters
extern const wcstring whitespace;
extern const char *whitespace_narrow;

bool is_whitespace(const wchar_t input);
bool is_whitespace(const wcstring &input);
inline bool is_whitespace(const wchar_t *input) { return is_whitespace(wcstring(input)); }

/// This macro is used to check that an argument is true. It is a bit like a non-fatal form of
/// assert. Instead of exiting on failure, the current function is ended at once. The second
/// parameter is the return value of the current function on failure.
#define CHECK(arg, retval)                                                               \
    if (!(arg)) {                                                                        \
        debug(0, "function %s called with false value for argument %s", __func__, #arg); \
        bugreport();                                                                     \
        show_stackframe(L'E');                                                           \
        return retval;                                                                   \
    }

// Pause for input, then exit the program. If supported, print a backtrace first.
// The `return` will never be run  but silences oclint warnings. Especially when this is called
// from within a `switch` block. As of the time I'm writing this oclint doesn't recognize the
// `__attribute__((noreturn))` on the exit_without_destructors() function.
// TODO: we use C++11 [[noreturn]] now, does that change things?
#define FATAL_EXIT()                                \
    {                                               \
        char exit_read_buff;                        \
        show_stackframe(L'E');                      \
        ignore_result(read(0, &exit_read_buff, 1)); \
        exit_without_destructors(1);                \
    }

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

/// Check if signals are blocked. If so, print an error message and return from the function
/// performing this check.
#define CHECK_BLOCK(retval)
#if 0
#define CHECK_BLOCK(retval)                                                \
    if (signal_is_blocked()) {                                             \
        debug(0, "function %s called while blocking signals. ", __func__); \
        bugreport();                                                       \
        show_stackframe(L'E');                                             \
        return retval;                                                     \
    }
#endif

/// Shorthand for wgettext call in situations where a C-style string is needed (e.g., fwprintf()).
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

/// Print a stack trace to stderr.
void show_stackframe(const wchar_t msg_level, int frame_count = 100, int skip_levels = 0);

/// Read a line from the stream f into the string. Returns the number of bytes read or -1 on
/// failure.
///
/// If the carriage return character is encountered, it is ignored. fgetws() considers the line to
/// end if reading the file results in either a newline (L'\n') character, the null (L'\\0')
/// character or the end of file (WEOF) character.
int fgetws2(wcstring *s, FILE *f);

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
char *wcs2str(const wchar_t *in);
char *wcs2str(const wcstring &in);
std::string wcs2string(const wcstring &input);

/// Test if a string prefixes another. Returns true if a is a prefix of b.
bool string_prefixes_string(const wcstring &proposed_prefix, const wcstring &value);
bool string_prefixes_string(const wchar_t *proposed_prefix, const wcstring &value);
bool string_prefixes_string(const wchar_t *proposed_prefix, const wchar_t *value);
bool string_prefixes_string(const char *proposed_prefix, const std::string &value);
bool string_prefixes_string(const char *proposed_prefix, const char *value);

/// Test if a string is a suffix of another.
bool string_suffixes_string(const wcstring &proposed_suffix, const wcstring &value);
bool string_suffixes_string(const wchar_t *proposed_suffix, const wcstring &value);
bool string_suffixes_string_case_insensitive(const wcstring &proposed_suffix,
                                             const wcstring &value);

/// Test if a string prefixes another without regard to case. Returns true if a is a prefix of b.
bool string_prefixes_string_case_insensitive(const wcstring &proposed_prefix,
                                             const wcstring &value);

/// Case-insensitive string search, templated for use with both std::string and std::wstring.
/// Modeled after std::string::find().
/// \param fuzzy indicates this is being used for fuzzy matching and case insensitivity is
/// expanded to include symbolic characters (#3584).
/// \return the offset of the first case-insensitive matching instance of `needle` within
/// `haystack`, or `string::npos()` if no results were found.
template <typename T>
size_t ifind(const T &haystack, const T &needle, bool fuzzy = false) {
    using char_t = typename T::value_type;
    auto locale = std::locale();

    std::function<bool(char_t, char_t)> icase_eq;

    if (!fuzzy) {
        icase_eq = [&locale](char_t c1, char_t c2) {
            return std::toupper(c1, locale) == std::toupper(c2, locale);
        };
    } else {
        icase_eq = [&locale](char_t c1, char_t c2) {
            // This `ifind()` call is being used for fuzzy string matching. Further extend case
            // insensitivity to treat `-` and `_` as equal (#3584).

            // The two lines below were tested to be 27% faster than
            //      (c1 == '_' || c1 == '-') && (c2 == '-' || c2 == '_')
            // while returning no false positives for all (c1, c2) combinations in the printable
            // range (0x20-0x7E). It might return false positives outside that range, but fuzzy
            // comparisons are typically called for file names only, which are unlikely to have
            // such characters and this entire function is 100% broken on unicode so there's no
            // point in worrying about anything outside of the ANSII range.
            // ((c1 == Literal<char_t>('_') || c1 == Literal<char_t>('-')) &&
            // ((c1 ^ c2) == (Literal<char_t>('-') ^ Literal<char_t>('_'))));

            // One of the following would be an illegal comparison between a char and a wchar_t.
            // However, placing them behind a constexpr gate results in the elision of the if
            // statement and the incorrect branch, with the compiler's SFINAE support suppressing
            // any errors in the branch not taken.
            if (sizeof(char_t) == sizeof(char)) {
                return std::toupper(c1, locale) == std::toupper(c2, locale) ||
                ((c1 == '_' || c1 == '-') &&
                ((c1 ^ c2) == ('-' ^ '_')));
            } else {
                return std::toupper(c1, locale) == std::toupper(c2, locale) ||
                ((c1 == L'_' || c1 == L'-') &&
                ((c1 ^ c2) == (L'-' ^ L'_')));
            }
        };
    }

    auto result =
        std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(), icase_eq);
    if (result != haystack.end()) {
        return result - haystack.begin();
    }
    return T::npos;
}

/// Split a string by a separator character.
wcstring_list_t split_string(const wcstring &val, wchar_t sep);

/// Join a list of strings by a separator character.
wcstring join_strings(const wcstring_list_t &vals, wchar_t sep);

/// Support for iterating over a newline-separated string.
template <typename Collection>
class line_iterator_t {
    // Storage for each line.
    Collection storage;

    // The collection we're iterating. Note we hold this by reference.
    const Collection &coll;

    // The current location in the iteration.
    typename Collection::const_iterator current;

public:
    /// Construct from a collection (presumably std::string or std::wcstring).
    line_iterator_t(const Collection &coll) : coll(coll), current(coll.cbegin()) {}

    /// Access the storage in which the last line was stored.
    const Collection &line() const {
        return storage;
    }

    /// Advances to the next line. \return true on success, false if we have exhausted the string.
    bool next() {
        if (current == coll.end())
            return false;
        auto newline_or_end = std::find(current, coll.cend(), '\n');
        storage.assign(current, newline_or_end);
        current = newline_or_end;

        // Skip the newline.
        if (current != coll.cend())
            ++current;
        return true;
    }
};

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
        default: { return true; }
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
        default: { return false; }
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

// fish_mutex is a wrapper around std::mutex that tracks whether it is locked, allowing for checking
// if the mutex is locked. It owns a boolean guarded by the lock that records whether the lock is
// currently locked; this is only used by assertions for correctness.
class fish_mutex_t {
    std::mutex lock_{};
    bool is_locked_{false};

   public:
    constexpr fish_mutex_t() = default;
    ~fish_mutex_t() = default;

    void lock() {
        lock_.lock();
        is_locked_ = true;
    }

    void unlock() {
        is_locked_ = false;
        lock_.unlock();
    }

    // assert that this lock (identified as 'who') is locked in the function 'caller'.
    void assert_is_locked(const char *who, const char *caller) const;

    // return the underlying std::mutex. Note the fish_mutex_t cannot track locks to the underlying
    // mutex; do not use assert_is_locked() with this.
    std::mutex &get_mutex() { return lock_; }
};

/// Useful macro for asserting that a lock is locked. This doesn't check whether this thread locked
/// it, which it would be nice if it did, but here it is anyways.
void assert_is_locked(const fish_mutex_t &m, const char *who, const char *caller);
#define ASSERT_IS_LOCKED(x) (x).assert_is_locked(#x, __FUNCTION__)

/// Format the specified size (in bytes, kilobytes, etc.) into the specified stringbuffer.
wcstring format_size(long long sz);

/// Version of format_size that does not allocate memory.
void format_size_safe(char buff[128], unsigned long long sz);

/// Our crappier versions of debug which is guaranteed to not allocate any memory, or do anything
/// other than call write(). This is useful after a call to fork() with threads.
void debug_safe(int level, const char *msg, const char *param1 = NULL, const char *param2 = NULL,
                const char *param3 = NULL, const char *param4 = NULL, const char *param5 = NULL,
                const char *param6 = NULL, const char *param7 = NULL, const char *param8 = NULL,
                const char *param9 = NULL, const char *param10 = NULL, const char *param11 = NULL,
                const char *param12 = NULL);

/// Writes out a long safely.
void format_long_safe(char buff[64], long val);
void format_long_safe(wchar_t buff[64], long val);

/// "Narrows" a wide character string. This just grabs any ASCII characters and trunactes.
void narrow_string_safe(char buff[64], const wchar_t *s);

template <typename T>
T from_string(const wcstring &x) {
    T result;
    std::wstringstream stream(x);
    stream >> result;
    return result;
}

template <typename T>
T from_string(const std::string &x) {
    T result = T();
    std::stringstream stream(x);
    stream >> result;
    return result;
}

template <typename T>
wcstring to_string(const T &x) {
    std::wstringstream stream;
    stream << x;
    return stream.str();
}

// wstringstream is a huge memory pig. Let's provide some specializations where we can.
template <>
inline wcstring to_string(const long &x) {
    wchar_t buff[128];
    format_long_safe(buff, x);
    return wcstring(buff);
}

template <>
inline bool from_string(const std::string &x) {
    return !x.empty() && strchr("YTyt1", x.at(0));
}

template <>
inline bool from_string(const wcstring &x) {
    return !x.empty() && wcschr(L"YTyt1", x.at(0));
}

template <>
inline wcstring to_string(const int &x) {
    return to_string(static_cast<long>(x));
}

wchar_t **make_null_terminated_array(const wcstring_list_t &lst);
char **make_null_terminated_array(const std::vector<std::string> &lst);

// Helper class for managing a null-terminated array of null-terminated strings (of some char type).
template <typename CharType_t>
class null_terminated_array_t {
    CharType_t **array{NULL};

    // No assignment or copying.
    void operator=(null_terminated_array_t rhs);
    null_terminated_array_t(const null_terminated_array_t &);

    typedef std::vector<std::basic_string<CharType_t>> string_list_t;

    size_t size() const {
        size_t len = 0;
        if (array != NULL) {
            while (array[len] != NULL) {
                len++;
            }
        }
        return len;
    }

    void free(void) {
        ::free((void *)array);
        array = NULL;
    }

   public:
    null_terminated_array_t() = default;

    explicit null_terminated_array_t(const string_list_t &argv)
        : array(make_null_terminated_array(argv)) {}

    ~null_terminated_array_t() { this->free(); }

    null_terminated_array_t(null_terminated_array_t &&rhs) : array(rhs.array) {
        rhs.array = nullptr;
    }

    null_terminated_array_t operator=(null_terminated_array_t &&rhs) {
        free();
        array = rhs.array;
        rhs.array = nullptr;
    }

    void set(const string_list_t &argv) {
        this->free();
        this->array = make_null_terminated_array(argv);
    }

    const CharType_t *const *get() const { return array; }
    CharType_t **get() { return array; }

    void clear() { this->free(); }
};

// Helper function to convert from a null_terminated_array_t<wchar_t> to a
// null_terminated_array_t<char_t>.
void convert_wide_array_to_narrow(const null_terminated_array_t<wchar_t> &arr,
                                  null_terminated_array_t<char> *output);
typedef std::lock_guard<fish_mutex_t> scoped_lock;
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
template <typename DATA>
class acquired_lock {
    std::unique_lock<fish_mutex_t> lock;
    acquired_lock(fish_mutex_t &lk, DATA *v) : lock(lk), value(v) {}

    template <typename T>
    friend class owning_lock;

    DATA *value;

   public:
    // No copying, move construction only
    acquired_lock &operator=(const acquired_lock &) = delete;
    acquired_lock(const acquired_lock &) = delete;
    acquired_lock(acquired_lock &&) = default;
    acquired_lock &operator=(acquired_lock &&) = default;

    DATA *operator->() { return value; }
    const DATA *operator->() const { return value; }
    DATA &operator*() { return *value; }
    const DATA &operator*() const { return *value; }
};

// A lock that owns a piece of data
// Access to the data is only provided by taking the lock
template <typename DATA>
class owning_lock {
    // No copying
    owning_lock &operator=(const scoped_lock &) = delete;
    owning_lock(const scoped_lock &) = delete;
    owning_lock(owning_lock &&) = default;
    owning_lock &operator=(owning_lock &&) = default;

    fish_mutex_t lock;
    DATA data;

   public:
    owning_lock(DATA &&d) : data(std::move(d)) {}
    owning_lock() : data() {}

    acquired_lock<DATA> acquire() { return {lock, &data}; }
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

/// Appends a path component, with a / if necessary.
void append_path_component(wcstring &path, const wcstring &component);

wcstring format_string(const wchar_t *format, ...);
wcstring vformat_string(const wchar_t *format, va_list va_orig);
void append_format(wcstring &str, const wchar_t *format, ...);
void append_formatv(wcstring &str, const wchar_t *format, va_list ap);

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
wchar_t *quote_end(const wchar_t *in);

/// A call to this function will reset the error counter. Some functions print out non-critical
/// error messages. These should check the error_count before, and skip printing the message if
/// MAX_ERROR_COUNT messages have been printed. The error_reset() should be called after each
/// interactive command executes, to allow new messages to be printed.
void error_reset();

/// This function should be called after calling `setlocale()` to perform fish specific locale
/// initialization.
void fish_setlocale();

/// Call read while blocking the SIGCHLD signal. Should only be called if you _know_ there is data
/// available for reading, or the program will hang until there is data.
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

/// Returns the width of the terminal window, so that not all functions that use these values
/// continually have to keep track of it separately.
///
/// Only works if common_handle_winch is registered to handle winch signals.
int common_get_width();

/// Returns the height of the terminal window, so that not all functions that use these values
/// continually have to keep track of it separatly.
///
/// Only works if common_handle_winch is registered to handle winch signals.
int common_get_height();

/// Handle a window change event by looking up the new window size and saving it in an internal
/// variable used by common_get_wisth and common_get_height().
void common_handle_winch(int signal);

/// Write the given paragraph of output, redoing linebreaks to fit the current screen.
wcstring reformat_for_screen(const wcstring &msg);

/// Make sure the specified direcotry exists. If needed, try to create it and any currently not
/// existing parent directories.
///
/// \return 0 if, at the time of function return the directory exists, -1 otherwise.
int create_directory(const wcstring &d);

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
void save_term_foreground_process_group(void);
void restore_term_foreground_process_group(void);

/// Return whether we are the child of a fork.
bool is_forked_child(void);
void assert_is_not_forked_child(const char *who);
#define ASSERT_IS_NOT_FORKED_CHILD_TRAMPOLINE(x) assert_is_not_forked_child(x)
#define ASSERT_IS_NOT_FORKED_CHILD() ASSERT_IS_NOT_FORKED_CHILD_TRAMPOLINE(__FUNCTION__)

/// Detect if we are Windows Subsystem for Linux by inspecting /proc/sys/kernel/osrelease
/// and checking if "Microsoft" is in the first line.
/// See https://github.com/Microsoft/WSL/issues/423 and Microsoft/WSL#2997
constexpr bool is_windows_subsystem_for_linux() {
#ifdef WSL
    return true;
#else
    return false;
#endif
}

extern "C" {
__attribute__((noinline)) void debug_thread_error(void);
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

/// Given a string return the matching enum. Return the sentinal enum if no match is made. The map
/// must be sorted by the `str` member. A binary search is twice as fast as a linear search with 16
/// elements in the map.
template <typename T>
static T str_to_enum(const wchar_t *name, const enum_map<T> map[], int len) {
    // Ignore the sentinel value when searching as it is the "not found" value.
    size_t left = 0, right = len - 1;

    while (left < right) {
        size_t mid = left + (right - left) / 2;
        int cmp = wcscmp(name, map[mid].str);
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
    return NULL;
};

template <typename... Args>
using tuple_list = std::vector<std::tuple<Args...>>;

// Given a container mapping one X to many Y, return a list of {X,Y}
template <typename X, typename Y>
inline tuple_list<X, Y> flatten(const std::unordered_map<X, std::vector<Y>> &list) {
    tuple_list<X, Y> results(list.size() * 1.5);  // just a guess as to the initial size
    for (auto &kv : list) {
        for (auto &v : kv.second) {
            results.emplace_back(std::make_tuple(kv.first, v));
        }
    }

    return results;
}

void redirect_tty_output();

// Minimum allowed terminal size and default size if the detected size is not reasonable.
#define MIN_TERM_COL 20
#define MIN_TERM_ROW 2
#define DFLT_TERM_COL 80
#define DFLT_TERM_ROW 24
#define DFLT_TERM_COL_STR L"80"
#define DFLT_TERM_ROW_STR L"24"
void invalidate_termsize(bool invalidate_vars = false);
struct winsize get_current_winsize();

bool valid_var_name_char(wchar_t chr);
bool valid_var_name(const wchar_t *str);
bool valid_var_name(const wcstring &str);
bool valid_func_name(const wcstring &str);

// Return values (`$status` values for fish scripts) for various situations.
enum {
    /// The status code used for normal exit in a command.
    STATUS_CMD_OK = 0,
    /// The status code used for failure exit in a command (but not if the args were invalid).
    STATUS_CMD_ERROR = 1,
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
    /// The status code used for invalid arguments given to a command. This is distinct from valid
    /// arguments that might result in a command failure. An invalid args condition is something
    /// like an unrecognized flag, missing or too many arguments, an invalid integer, etc. But
    STATUS_INVALID_ARGS = 121,
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
        return hasher((wcstring)w);
    }
};
}  // namespace std
#endif

/// Get the absolute path to the fish executable itself
std::string get_executable_path(const char *fallback);

/// A RAII wrapper for resources that don't recur, so we don't have to create a separate RAII
/// wrapper for each function. Avoids needing to call "return cleanup()" or similar / everywhere.
struct cleanup_t {
private:
    const std::function<void()> cleanup;
public:
    cleanup_t(std::function<void()> exit_actions)
        : cleanup{exit_actions} {}
    ~cleanup_t() {
        cleanup();
    }
};

#endif
