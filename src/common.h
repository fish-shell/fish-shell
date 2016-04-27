// Prototypes for various functions, mostly string utilities, that are used by most parts of fish.
#ifndef FISH_COMMON_H
#define FISH_COMMON_H
#include "config.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>  // IWYU pragma: keep
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <termios.h>
#include <wchar.h>
#include <memory>  // IWYU pragma: keep
#include <sstream>
#include <string>
#include <vector>

#include "fallback.h"  // IWYU pragma: keep
#include "signal.h"    // IWYU pragma: keep

/// Avoid writing the type name twice in a common "static_cast-initialization". Caveat: This doesn't
/// work with type names containing commas!
#define CAST_INIT(type, dst, src) type dst = static_cast<type>(src)

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
#define RESERVED_CHAR_BASE 0xFDD0u
#define RESERVED_CHAR_END 0xFDF0u
// Split the available noncharacter values into two ranges to ensure there are
// no conflicts among the places we use these special characters.
#define EXPAND_RESERVED_BASE RESERVED_CHAR_BASE
#define EXPAND_RESERVED_END (EXPAND_RESERVED_BASE + 16)
#define WILDCARD_RESERVED_BASE EXPAND_RESERVED_END
#define WILDCARD_RESERVED_END (WILDCARD_RESERVED_BASE + 16)
// Make sure the ranges defined above don't exceed the range for noncharacters.
// This is to make sure we didn't do something stupid in subdividing the
// Unicode range for our needs.
#if WILDCARD_RESERVED_END > RESERVED_CHAR_END
#error
#endif

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
#define ENCODE_DIRECT_BASE 0xF600u
#define ENCODE_DIRECT_END (ENCODE_DIRECT_BASE + 256)
#define INPUT_COMMON_BASE 0xF700u
#define INPUT_COMMON_END (INPUT_COMMON_BASE + 64)

// Flags for unescape_string functions.
enum {
    UNESCAPE_DEFAULT = 0,         // default behavior
    UNESCAPE_SPECIAL = 1 << 0,    // escape special fish syntax characters like the semicolon
    UNESCAPE_INCOMPLETE = 1 << 1  // allow incomplete escape sequences
};
typedef unsigned int unescape_flags_t;

// Flags for the escape() and escape_string() functions.
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

inline bool selection_direction_is_cardinal(selection_direction_t dir) {
    switch (dir) {
        case direction_north:
        case direction_page_north:
        case direction_east:
        case direction_page_south:
        case direction_south:
        case direction_west: {
            return true;
        }
        default: { return false; }
    }
}

/// Helper macro for errors.
#define VOMIT_ON_FAILURE(a)         \
    do {                            \
        if (0 != (a)) {             \
            VOMIT_ABORT(errno, #a); \
        }                           \
    } while (0)
#define VOMIT_ON_FAILURE_NO_ERRNO(a) \
    do {                             \
        int err = (a);               \
        if (0 != err) {              \
            VOMIT_ABORT(err, #a);    \
        }                            \
    } while (0)
#define VOMIT_ABORT(err, str)                                                                  \
    do {                                                                                       \
        int code = (err);                                                                      \
        fprintf(stderr, "%s failed on line %d in file %s: %d (%s)\n", str, __LINE__, __FILE__, \
                code, strerror(code));                                                         \
        abort();                                                                               \
    } while (0)

/// Exits without invoking destructors (via _exit), useful for code after fork.
void exit_without_destructors(int code) __attribute__((noreturn));

/// Save the shell mode on startup so we can restore them on exit.
extern struct termios shell_modes;

/// The character to use where the text has been truncated. Is an ellipsis on unicode system and a $
/// on other systems.
extern wchar_t ellipsis_char;

/// Character representing an omitted newline at the end of text.
extern wchar_t omitted_newline_char;

/// The verbosity level of fish. If a call to debug has a severity level higher than \c debug_level,
/// it will not be printed.
extern int debug_level;

/// Profiling flag. True if commands should be profiled.
extern bool g_profiling_active;

/// Name of the current program. Should be set at startup. Used by the debug function.
extern const wchar_t *program_name;

// Variants of read() and write() that ignores return values, defeating a warning.
void read_ignore(int fd, void *buff, size_t count);
void write_ignore(int fd, const void *buff, size_t count);

/// This macro is used to check that an input argument is not null. It is a bit lika a non-fatal
/// form of assert. Instead of exit-ing on failure, the current function is ended at once. The
/// second parameter is the return value of the current function on failure.
#define CHECK(arg, retval)                                                                \
    if (!(arg)) {                                                                         \
        debug(0, "function %s called with null value for argument %s. ", __func__, #arg); \
        bugreport();                                                                      \
        show_stackframe();                                                                \
        return retval;                                                                    \
    }

/// Pause for input, then exit the program. If supported, print a backtrace first.
#define FATAL_EXIT()                        \
    {                                       \
        char exit_read_buff;                \
        show_stackframe();                  \
        read_ignore(0, &exit_read_buff, 1); \
        exit_without_destructors(1);        \
    }

/// Exit program at once, leaving an error message about running out of memory.
#define DIE_MEM()                                                                             \
    {                                                                                         \
        fwprintf(stderr, L"fish: Out of memory on line %ld of file %s, shutting down fish\n", \
                 (long)__LINE__, __FILE__);                                                   \
        FATAL_EXIT();                                                                         \
    }

/// Check if signals are blocked. If so, print an error message and return from the function
/// performing this check.
#define CHECK_BLOCK(retval)                                                \
    if (signal_is_blocked()) {                                             \
        debug(0, "function %s called while blocking signals. ", __func__); \
        bugreport();                                                       \
        show_stackframe();                                                 \
        return retval;                                                     \
    }

/// Shorthand for wgettext call.
#define _(wstr) wgettext(wstr)

/// Noop, used to tell xgettext that a string should be translated, even though it is not directly
/// sent to wgettext.
#define N_(wstr) wstr

/// Check if the specified string element is a part of the specified string list.
#define contains(str, ...) contains_internal(str, 0, __VA_ARGS__, NULL)

/// Print a stack trace to stderr.
void show_stackframe();

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

/// Test if a string is a suffix of another.
bool string_suffixes_string(const wcstring &proposed_suffix, const wcstring &value);
bool string_suffixes_string(const wchar_t *proposed_suffix, const wcstring &value);

/// Test if a string prefixes another without regard to case. Returns true if a is a prefix of b.
bool string_prefixes_string_case_insensitive(const wcstring &proposed_prefix,
                                             const wcstring &value);

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

/// Test if a list contains a string using a linear search.
bool list_contains_string(const wcstring_list_t &list, const wcstring &str);

void assert_is_main_thread(const char *who);
#define ASSERT_IS_MAIN_THREAD_TRAMPOLINE(x) assert_is_main_thread(x)
#define ASSERT_IS_MAIN_THREAD() ASSERT_IS_MAIN_THREAD_TRAMPOLINE(__FUNCTION__)

void assert_is_background_thread(const char *who);
#define ASSERT_IS_BACKGROUND_THREAD_TRAMPOLINE(x) assert_is_background_thread(x)
#define ASSERT_IS_BACKGROUND_THREAD() ASSERT_IS_BACKGROUND_THREAD_TRAMPOLINE(__FUNCTION__)

/// Useful macro for asserting that a lock is locked. This doesn't check whether this thread locked
/// it, which it would be nice if it did, but here it is anyways.
void assert_is_locked(void *mutex, const char *who, const char *caller);
#define ASSERT_IS_LOCKED(x) assert_is_locked((void *)(&x), #x, __FUNCTION__)

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

// A hackish thing to simulate rvalue references in C++98. The idea is that you can define a
// constructor to take a moved_ref<T> and then swap() out of it.
template <typename T>
struct moved_ref {
    T &val;

    explicit moved_ref(T &v) : val(v) {}
};

wchar_t **make_null_terminated_array(const wcstring_list_t &lst);
char **make_null_terminated_array(const std::vector<std::string> &lst);

// Helper class for managing a null-terminated array of null-terminated strings (of some char type).
template <typename CharType_t>
class null_terminated_array_t {
    CharType_t **array;

    // No assignment or copying.
    void operator=(null_terminated_array_t rhs);
    null_terminated_array_t(const null_terminated_array_t &);

    typedef std::vector<std::basic_string<CharType_t> > string_list_t;

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
    null_terminated_array_t() : array(NULL) {}
    explicit null_terminated_array_t(const string_list_t &argv)
        : array(make_null_terminated_array(argv)) {}

    ~null_terminated_array_t() { this->free(); }

    void set(const string_list_t &argv) {
        this->free();
        this->array = make_null_terminated_array(argv);
    }

    const CharType_t *const *get() const { return array; }

    void clear() { this->free(); }
};

// Helper function to convert from a null_terminated_array_t<wchar_t> to a
// null_terminated_array_t<char_t>.
void convert_wide_array_to_narrow(const null_terminated_array_t<wchar_t> &arr,
                                  null_terminated_array_t<char> *output);

bool is_forked_child();

class mutex_lock_t {
   public:
    pthread_mutex_t mutex;
    mutex_lock_t() { VOMIT_ON_FAILURE_NO_ERRNO(pthread_mutex_init(&mutex, NULL)); }

    ~mutex_lock_t() { VOMIT_ON_FAILURE_NO_ERRNO(pthread_mutex_destroy(&mutex)); }
};

// Basic scoped lock class.
class scoped_lock {
    pthread_mutex_t *lock_obj;
    bool locked;

    // No copying.
    scoped_lock &operator=(const scoped_lock &);
    scoped_lock(const scoped_lock &);

   public:
    void lock(void);
    void unlock(void);
    explicit scoped_lock(pthread_mutex_t &mutex);
    explicit scoped_lock(mutex_lock_t &lock);
    ~scoped_lock();
};

class rwlock_t {
   public:
    pthread_rwlock_t rwlock;
    rwlock_t() { VOMIT_ON_FAILURE_NO_ERRNO(pthread_rwlock_init(&rwlock, NULL)); }

    ~rwlock_t() { VOMIT_ON_FAILURE_NO_ERRNO(pthread_rwlock_destroy(&rwlock)); }
};

// Scoped lock class for rwlocks.
class scoped_rwlock {
    pthread_rwlock_t *rwlock_obj;
    bool locked;
    bool locked_shared;

    // No copying.
    scoped_rwlock &operator=(const scoped_lock &);
    explicit scoped_rwlock(const scoped_lock &);

   public:
    void lock(void);
    void unlock(void);
    void lock_shared(void);
    void unlock_shared(void);
    // Upgrade shared lock to exclusive. Equivalent to `lock.unlock_shared(); lock.lock();`.
    void upgrade(void);
    explicit scoped_rwlock(pthread_rwlock_t &rwlock, bool shared = false);
    explicit scoped_rwlock(rwlock_t &rwlock, bool shared = false);
    ~scoped_rwlock();
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

    scoped_push(T *r, const T &new_value) : ref(r), saved_value(*r), restored(false) {
        *r = new_value;
    }

    ~scoped_push() { restore(); }

    void restore() {
        if (!restored) {
            std::swap(*ref, saved_value);
            restored = true;
        }
    }
};

/// Wrapper around wcstok.
class wcstokenizer {
    wchar_t *buffer, *str, *state;
    const wcstring sep;

    // No copying.
    wcstokenizer &operator=(const wcstokenizer &);
    wcstokenizer(const wcstokenizer &);

   public:
    wcstokenizer(const wcstring &s, const wcstring &separator);
    bool next(wcstring &result);
    ~wcstokenizer();
};

/// Appends a path component, with a / if necessary.
void append_path_component(wcstring &path, const wcstring &component);

wcstring format_string(const wchar_t *format, ...);
wcstring vformat_string(const wchar_t *format, va_list va_orig);
void append_format(wcstring &str, const wchar_t *format, ...);
void append_formatv(wcstring &str, const wchar_t *format, va_list ap);

/// Test if the given string is a valid variable name.
///
/// \return null if this is a valid name, and a pointer to the first invalid character otherwise.
const wchar_t *wcsvarname(const wchar_t *str);
const wchar_t *wcsvarname(const wcstring &str);

/// Test if the given string is a valid function name.
///
/// \return null if this is a valid name, and a pointer to the first invalid character otherwise.
const wchar_t *wcsfuncname(const wcstring &str);

/// Test if the given string is valid in a variable name.
///
/// \return true if this is a valid name, false otherwise.
bool wcsvarchr(wchar_t chr);

/// Convenience variants on fish_wcwswidth().
///
/// See fallback.h for the normal definitions.
int fish_wcswidth(const wchar_t *str);
int fish_wcswidth(const wcstring &str);

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

/// This function behaves exactly like a wide character equivalent of the C function setlocale,
/// except that it will also try to detect if the user is using a Unicode character set, and if so,
/// use the unicode ellipsis character as ellipsis, instead of '$'.
wcstring wsetlocale(int category, const wchar_t *locale);

/// Checks if \c needle is included in the list of strings specified. A warning is printed if needle
/// is zero.
///
/// \param needle the string to search for in the list.
///
/// \return zero if needle is not found, of if needle is null, non-zero otherwise.
__sentinel bool contains_internal(const wchar_t *needle, int vararg_handle, ...);
__sentinel bool contains_internal(const wcstring &needle, int vararg_handle, ...);

/// Call read while blocking the SIGCHLD signal. Should only be called if you _know_ there is data
/// available for reading, or the program will hang until there is data.
long read_blocked(int fd, void *buf, size_t count);

/// Loop a write request while failure is non-critical. Return -1 and set errno in case of critical
/// error.
ssize_t write_loop(int fd, const char *buff, size_t count);

/// Loop a read request while failure is non-critical. Return -1 and set errno in case of critical
/// error.
ssize_t read_loop(int fd, void *buff, size_t count);

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
void debug(int level, const char *msg, ...);
void debug(int level, const wchar_t *msg, ...);

/// Replace special characters with backslash escape sequences. Newline is replaced with \n, etc.
///
/// \param in The string to be escaped
/// \param flags Flags to control the escaping
/// \return The escaped string
wcstring escape(const wchar_t *in, escape_flags_t flags);
wcstring escape_string(const wcstring &in, escape_flags_t flags);

/// Expand backslashed escapes and substitute them with their unescaped counterparts. Also
/// optionally change the wildcards, the tilde character and a few more into constants which are
/// defined in a private use area of Unicode. This assumes wchar_t is a unicode character set.

/// Given a null terminated string starting with a backslash, read the escape as if it is unquoted,
/// appending to result. Return the number of characters consumed, or 0 on error.
size_t read_unquoted_escape(const wchar_t *input, wcstring *result, bool allow_incomplete,
                            bool unescape_special);

/// Unescapes a string in-place. A true result indicates the string was unescaped, a false result
/// indicates the string was unmodified.
bool unescape_string_in_place(wcstring *str, unescape_flags_t escape_special);

/// Unescapes a string, returning the unescaped value by reference. On failure, the output is set to
/// an empty string.
bool unescape_string(const wchar_t *input, wcstring *output, unescape_flags_t escape_special);
bool unescape_string(const wcstring &input, wcstring *output, unescape_flags_t escape_special);

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

/// Tokenize the specified string into the specified wcstring_list_t.
///
/// \param val the input string. The contents of this string is not changed.
/// \param out the list in which to place the elements.
void tokenize_variable_array(const wcstring &val, wcstring_list_t &out);

/// Make sure the specified direcotry exists. If needed, try to create it and any currently not
/// existing parent directories.
///
/// \return 0 if, at the time of function return the directory exists, -1 otherwise.
int create_directory(const wcstring &d);

/// Print a short message about how to file a bug report to stderr.
void bugreport();

/// Return the number of seconds from the UNIX epoch, with subsecond precision. This function uses
/// the gettimeofday function, and will have the same precision as that function. If an error
/// occurs, NAN is returned.
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

/// Macro to help suppress potentially unused variable warnings.
#define USE(var) (void)(var)

extern "C" {
__attribute__((noinline)) void debug_thread_error(void);
}

#endif
