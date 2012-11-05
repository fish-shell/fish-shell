/** \file common.h
	Prototypes for various functions, mostly string utilities, that are used by most parts of fish.
*/

#ifndef FISH_COMMON_H
/**
   Header guard
*/
#define FISH_COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <termios.h>
#include <string>
#include <sstream>
#include <vector>
#include <pthread.h>
#include <string.h>

#include <errno.h>
#include <assert.h>
#include "util.h"

class completion_t;

/* Common string type */
typedef std::wstring wcstring;
typedef std::vector<wcstring> wcstring_list_t;

/**
   Maximum number of bytes used by a single utf-8 character
*/
#define MAX_UTF8_BYTES 6

/**
   This is in the unicode private use area.
*/
#define ENCODE_DIRECT_BASE 0xf100

/**
  Highest legal ascii value
*/
#define ASCII_MAX 127u

/**
  Highest legal 16-bit unicode value
*/
#define UCS2_MAX 0xffffu

/**
  Highest legal byte value
*/
#define BYTE_MAX 0xffu

/**
  Escape special fish syntax characters like the semicolon
 */
#define UNESCAPE_SPECIAL 1

/**
  Allow incomplete escape sequences
 */
#define UNESCAPE_INCOMPLETE 2

/* Flags for the escape() and escape_string() functions */
enum {
    /** Escape all characters, including magic characters like the semicolon */
     ESCAPE_ALL = 1 << 0,
     
    /** Do not try to use 'simplified' quoted escapes, and do not use empty quotes as the empty string */
    ESCAPE_NO_QUOTED = 1 << 1,
    
    /** Do not escape tildes */
    ESCAPE_NO_TILDE = 1 << 2
};
typedef unsigned int escape_flags_t;

/**
 Helper macro for errors
 */
#define VOMIT_ON_FAILURE(a) do { if (0 != (a)) { int err = errno; fprintf(stderr, "%s failed on line %d in file %s: %d (%s)\n", #a, __LINE__, __FILE__, err, strerror(err)); abort(); }} while (0)

/** Exits without invoking destructors (via _exit), useful for code after fork. */
void exit_without_destructors(int code) __attribute__ ((noreturn));

/** 
	Save the shell mode on startup so we can restore them on exit
*/
extern struct termios shell_modes;      

/**
   The character to use where the text has been truncated. Is an
   ellipsis on unicode system and a $ on other systems.
*/
extern wchar_t ellipsis_char;

/**
   The verbosity level of fish. If a call to debug has a severity
   level higher than \c debug_level, it will not be printed.
*/
extern int debug_level;

/**
   Profiling flag. True if commands should be profiled.
*/
extern char *profile;

/**
   Name of the current program. Should be set at startup. Used by the
   debug function.
*/
extern const wchar_t *program_name;

/**
   This macro is used to check that an input argument is not null. It
   is a bit lika a non-fatal form of assert. Instead of exit-ing on
   failiure, the current function is ended at once. The second
   parameter is the return value of the current function on failiure.
*/
#define CHECK( arg, retval )											\
	if( !(arg) )														\
	{																	\
		debug( 0,														\
			   "function %s called with null value for argument %s. ",  \
			   __func__,												\
			   #arg );													\
		bugreport();													\
		show_stackframe();												\
		return retval;													\
	}

/**
   Pause for input, then exit the program. If supported, print a backtrace first.
*/
#define FATAL_EXIT()											\
	{															\
		char exit_read_buff;			\
		show_stackframe();										\
		read( 0, &exit_read_buff, 1 );			\
		exit_without_destructors( 1 );												\
	}															\
		

/**
   Exit program at once, leaving an error message about running out of memory.
*/
#define DIE_MEM()														\
	{																	\
		fwprintf( stderr,												\
				  L"fish: Out of memory on line %ld of file %s, shutting down fish\n", \
				  (long)__LINE__,												\
				  __FILE__ );											\
		FATAL_EXIT();														\
	}

/**
   Check if signals are blocked. If so, print an error message and
   return from the function performing this check.
*/
#define CHECK_BLOCK( retval )											\
	if( signal_is_blocked() )											\
	{																	\
		debug( 0,														\
			    "function %s called while blocking signals. ",  		\
			   __func__);												\
		bugreport();													\
		show_stackframe();												\
		return retval;													\
	}
		
/**
   Shorthand for wgettext call
*/
#define _(wstr) wgettext((const wchar_t *)wstr)

/**
   Noop, used to tell xgettext that a string should be translated,
   even though it is not directly sent to wgettext. 
*/
#define N_(wstr) wstr

/**
   Check if the specified string element is a part of the specified string list
 */
#define contains( str,... ) contains_internal( str, __VA_ARGS__, NULL )

/**
  Print a stack trace to stderr
*/
void show_stackframe();


/**
   Read a line from the stream f into the string. Returns 
   the number of bytes read or -1 on failiure.

   If the carriage return character is encountered, it is
   ignored. fgetws() considers the line to end if reading the file
   results in either a newline (L'\n') character, the null (L'\\0')
   character or the end of file (WEOF) character.
*/
int fgetws2(wcstring *s, FILE *f);

/**
   Returns a newly allocated wide character string equivalent of the
   specified multibyte character string

   This function encodes illegal character sequences in a reversible
   way using the private use area.
*/
wchar_t *str2wcs( const char *in );

/**
 Returns a newly allocated wide character string equivalent of the
 specified multibyte character string
 
 This function encodes illegal character sequences in a reversible
 way using the private use area.
 */
wcstring str2wcstring( const char *in );
wcstring str2wcstring( const std::string &in );

/**
   Converts the narrow character string \c in into it's wide
   equivalent, stored in \c out. \c out must have enough space to fit
   the entire string.

   This function encodes illegal character sequences in a reversible
   way using the private use area.
*/
wchar_t *str2wcs_internal( const char *in, wchar_t *out );

/**
   Returns a newly allocated multibyte character string equivalent of
   the specified wide character string

   This function decodes illegal character sequences in a reversible
   way using the private use area.
*/
char *wcs2str( const wchar_t *in );
std::string wcs2string(const wcstring &input);

/** Test if a string prefixes another. Returns true if a is a prefix of b */
bool string_prefixes_string(const wcstring &proposed_prefix, const wcstring &value);
bool string_prefixes_string(const wchar_t *proposed_prefix, const wcstring &value);

/** Test if a string is a suffix of another */
bool string_suffixes_string(const wcstring &proposed_suffix, const wcstring &value);
bool string_suffixes_string(const wchar_t *proposed_suffix, const wcstring &value);


/** Test if a string prefixes another without regard to case. Returns true if a is a prefix of b */
bool string_prefixes_string_case_insensitive(const wcstring &proposed_prefix, const wcstring &value);

/** Test if a list contains a string using a linear search. */
bool list_contains_string(const wcstring_list_t &list, const wcstring &str);


void assert_is_main_thread(const char *who);
#define ASSERT_IS_MAIN_THREAD_TRAMPOLINE(x) assert_is_main_thread(x)
#define ASSERT_IS_MAIN_THREAD() ASSERT_IS_MAIN_THREAD_TRAMPOLINE(__FUNCTION__)

void assert_is_background_thread(const char *who);
#define ASSERT_IS_BACKGROUND_THREAD_TRAMPOLINE(x) assert_is_background_thread(x)
#define ASSERT_IS_BACKGROUND_THREAD() ASSERT_IS_BACKGROUND_THREAD_TRAMPOLINE(__FUNCTION__)

/* Useful macro for asserting that a lock is locked. This doesn't check whether this thread locked it, which it would be nice if it did, but here it is anyways. */
void assert_is_locked(void *mutex, const char *who, const char *caller);
#define ASSERT_IS_LOCKED(x) assert_is_locked((void *)(&x), #x, __FUNCTION__)

/**
   Converts the wide character string \c in into it's narrow
   equivalent, stored in \c out. \c out must have enough space to fit
   the entire string.

   This function decodes illegal character sequences in a reversible
   way using the private use area.
*/
char *wcs2str_internal( const wchar_t *in, char *out );

/** Format the specified size (in bytes, kilobytes, etc.) into the specified stringbuffer. */
wcstring format_size(long long sz);

/** Version of format_size that does not allocate memory. */
void format_size_safe(char buff[128], unsigned long long sz);

/** Our crappier versions of debug which is guaranteed to not allocate any memory, or do anything other than call write(). This is useful after a call to fork() with threads. */
void debug_safe(int level, const char *msg, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL, const char *param4 = NULL, const char *param5 = NULL, const char *param6 = NULL, const char *param7 = NULL, const char *param8 = NULL, const char *param9 = NULL, const char *param10 = NULL, const char *param11 = NULL, const char *param12 = NULL);

/** Writes out a long safely */
void format_long_safe(char buff[128], long val);
void format_long_safe(wchar_t buff[128], long val);


template<typename T>
T from_string(const wcstring &x) {
    T result;
    std::wstringstream stream(x);
    stream >> result;
    return result;
}

template<typename T>
T from_string(const std::string &x) {
    T result = T();
    std::stringstream stream(x);
    stream >> result;
    return result;
}

template<typename T>
wcstring to_string(const T &x) {
    std::wstringstream stream;
    stream << x;
    return stream.str();
}

/* wstringstream is a huge memory pig. Let's provide some specializations where we can. */
template<>
inline wcstring to_string(const long &x) {
    wchar_t buff[128];
    format_long_safe(buff, x);
    return wcstring(buff);
}

template<>
inline bool from_string(const std::string &x) {
    return ! x.empty() && strchr("YTyt1", x.at(0));
}

template<>
inline bool from_string(const wcstring &x) {
    return ! x.empty() && wcschr(L"YTyt1", x.at(0));
}

template<>
inline wcstring to_string(const int &x) {
    return to_string(static_cast<long>(x));
}


/* Helper class for managing a null-terminated array of null-terminated strings (of some char type) */
template <typename CharType_t>
class null_terminated_array_t {
    CharType_t **array;
    
    typedef std::basic_string<CharType_t> string_t;
    typedef std::vector<string_t> string_list_t;

    void swap(null_terminated_array_t<CharType_t> &him) { std::swap(array, him.array); }

    /* Silly function to get the length of a null terminated array of...something */
    template <typename T>
    static size_t count_not_null(const T *arr) {
        size_t len;
        for (len=0; arr[len] != T(0); len++)
            ;
        return len;        
    }

    size_t size() const {
        return count_not_null(array);
    }

    void free(void) {
        if (array != NULL) {
            for (size_t i = 0; array[i] != NULL; i++) {
                delete [] array[i];
            }
            delete [] array;
            array = NULL;
        }
    }
        
    public:
    null_terminated_array_t() : array(NULL) { }
    null_terminated_array_t(const string_list_t &argv) : array(NULL) { this->set(argv); }
    ~null_terminated_array_t() { this->free(); }
    
    /** operator=. Notice the pass-by-value parameter. */
    null_terminated_array_t& operator=(null_terminated_array_t rhs) {
        if (this != &rhs)
            this->swap(rhs);
        return *this;
    }
    
    /* Copy constructor. */
    null_terminated_array_t(const null_terminated_array_t &him) : array(NULL) {
        this->set(him.array);
    }
    
    void set(const string_list_t &argv) {
        /* Get rid of the old argv */
        this->free();

        /* Allocate our null-terminated array of null-terminated strings */
        size_t i, count = argv.size();
        this->array = new CharType_t * [count + 1];
        for (i=0; i < count; i++) {
            const string_t &str = argv.at(i);
            this->array[i] = new CharType_t [1 + str.size()];
            std::copy(str.begin(), str.end(), this->array[i]);
            this->array[i][str.size()] = CharType_t(0);
        }
        this->array[count] = NULL;
    }
    
    void set(const CharType_t * const *new_array) {
        if (new_array == array)
            return;
            
        /* Get rid of the old argv */
        this->free();
        
        /* Copy the new one */
        if (new_array) {
            size_t i, count = count_not_null(new_array);
            this->array = new CharType_t * [count + 1];
            for (i=0; i < count; i++) {
                size_t len = count_not_null(new_array[i]);
                this->array[i] = new CharType_t [1 + len];
                std::copy(new_array[i], new_array[i] + len, this->array[i]);
                this->array[i][len] = CharType_t(0);
            }
            this->array[count] = NULL;
        }
    }
    
    CharType_t **get() { return array; }
    const CharType_t * const *get() const { return array; }
    
    string_list_t to_list() const {
        string_list_t lst;
        if (array != NULL) {
            size_t count = this->size();
            lst.reserve(count);
            lst.insert(lst.end(), array, array + count);
        }
        return lst;
    }
};

/* Helper function to convert from a null_terminated_array_t<wchar_t> to a null_terminated_array_t<char_t> */
null_terminated_array_t<char> convert_wide_array_to_narrow(const null_terminated_array_t<wchar_t> &arr);

/* Helper class to cache a narrow version of a wcstring in a malloc'd buffer, so that we can read it after fork() */
class narrow_string_rep_t {
    private:
    const char *str;
    
    /* No copying */
    narrow_string_rep_t &operator=(const narrow_string_rep_t &);
    narrow_string_rep_t(const narrow_string_rep_t &x);
    
    public:
    ~narrow_string_rep_t() {
        free((void *)str);
    }
    
    narrow_string_rep_t() : str(NULL) {}
    
    void set(const wcstring &s) {
        free((void *)str);
        str = wcs2str(s.c_str());
    }
    
    const char *get() const {
        return str;
    }
};

bool is_forked_child();

/* Basic scoped lock class */
class scoped_lock {
    pthread_mutex_t *lock_obj;
    bool locked;
    
    /* No copying */
    scoped_lock &operator=(const scoped_lock &);
    scoped_lock(const scoped_lock &);

public:
    void lock(void);
    void unlock(void);
    scoped_lock(pthread_mutex_t &mutex);
    ~scoped_lock();
};

/* Wrapper around wcstok */
class wcstokenizer {
    wchar_t *buffer, *str, *state;
    const wcstring sep;
    
    /* No copying */
    wcstokenizer &operator=(const wcstokenizer &);
    wcstokenizer(const wcstokenizer &);
    
public:
    wcstokenizer(const wcstring &s, const wcstring &separator);
    bool next(wcstring &result);
    ~wcstokenizer();
};

/** 
 Appends a path component, with a / if necessary
 */
void append_path_component(wcstring &path, const wcstring &component);

wcstring format_string(const wchar_t *format, ...);
wcstring vformat_string(const wchar_t *format, va_list va_orig);
void append_format(wcstring &str, const wchar_t *format, ...);

/**
   Returns a newly allocated wide character string array equivalent of
   the specified multibyte character string array
*/
char **wcsv2strv( const wchar_t * const *in );

/**
   Test if the given string is a valid variable name. 

   \return null if this is a valid name, and a pointer to the first invalid character otherwise
*/

wchar_t *wcsvarname( const wchar_t *str );


/**
   Test if the given string is a valid function name. 

   \return null if this is a valid name, and a pointer to the first invalid character otherwise
*/

const wchar_t *wcsfuncname( const wchar_t *str );

/**
   Test if the given string is valid in a variable name 

   \return 1 if this is a valid name, 0 otherwise
*/

int wcsvarchr( wchar_t chr );


/**
   A wcswidth workalike. Fish uses this since the regular wcswidth seems flaky.
*/
int my_wcswidth( const wchar_t *c );

/**
   This functions returns the end of the quoted substring beginning at
   \c in. The type of quoting character is detemrined by examining \c
   in. Returns 0 on error.

   \param in the position of the opening quote
*/
wchar_t *quote_end( const wchar_t *in );

/**
   A call to this function will reset the error counter. Some
   functions print out non-critical error messages. These should check
   the error_count before, and skip printing the message if
   MAX_ERROR_COUNT messages have been printed. The error_reset()
   should be called after each interactive command executes, to allow
   new messages to be printed.
*/
void error_reset();

/**
   This function behaves exactly like a wide character equivalent of
   the C function setlocale, except that it will also try to detect if
   the user is using a Unicode character set, and if so, use the
   unicode ellipsis character as ellipsis, instead of '$'.      
*/
wcstring wsetlocale( int category, const wchar_t *locale );

/**
   Checks if \c needle is included in the list of strings specified. A warning is printed if needle is zero.

   \param needle the string to search for in the list 

   \return zero if needle is not found, of if needle is null, non-zero otherwise
*/
__sentinel bool contains_internal( const wchar_t *needle, ... );
__sentinel bool contains_internal( const wcstring &needle, ... );

/**
   Call read while blocking the SIGCHLD signal. Should only be called
   if you _know_ there is data available for reading, or the program
   will hang until there is data.
*/
long read_blocked(int fd, void *buf, size_t count);

/**
   Loop a write request while failiure is non-critical. Return -1 and set errno
   in case of critical error.
 */
ssize_t write_loop(int fd, const char *buff, size_t count);

/**
   Loop a read request while failiure is non-critical. Return -1 and set errno
   in case of critical error.
 */
ssize_t read_loop(int fd, void *buff, size_t count);


/**
   Issue a debug message with printf-style string formating and
   automatic line breaking. The string will begin with the string \c
   program_name, followed by a colon and a whitespace.

   Because debug is often called to tell the user about an error,
   before using wperror to give a specific error message, debug will
   never ever modify the value of errno.
   
   \param level the priority of the message. Lower number means higher priority. Messages with a priority_number higher than \c debug_level will be ignored..
   \param msg the message format string. 

   Example:

   <code>debug( 1, L"Pi = %.3f", M_PI );</code>

   will print the string 'fish: Pi = 3.141', given that debug_level is 1 or higher, and that program_name is 'fish'.
*/
void debug( int level, const char *msg, ... );
void debug( int level, const wchar_t *msg, ... );

/**
   Replace special characters with backslash escape sequences. Newline is
   replaced with \n, etc. 

   \param in The string to be escaped
   \param escape_all Whether all characters wich hold special meaning in fish (Pipe, semicolon, etc,) should be escaped, or only unprintable characters
   \return The escaped string, or 0 if there is not enough memory
*/

wchar_t *escape( const wchar_t *in, escape_flags_t flags );
wcstring escape_string( const wcstring &in, escape_flags_t flags );

/**
   Expand backslashed escapes and substitute them with their unescaped
   counterparts. Also optionally change the wildcards, the tilde
   character and a few more into constants which are defined in a
   private use area of Unicode. This assumes wchar_t is a unicode
   character set.

   The result must be free()d. The original string is not modified. If
   an invalid sequence is specified, 0 is returned.

*/
wchar_t *unescape( const wchar_t * in, 
				   int escape_special );

bool unescape_string( wcstring &str, 
                  int escape_special );


/** 
    Returns the width of the terminal window, so that not all
    functions that use these values continually have to keep track of
    it separately.

    Only works if common_handle_winch is registered to handle winch signals.
*/
int common_get_width();
/**
   Returns the height of the terminal window, so that not all
   functions that use these values continually have to keep track of
   it separatly.

   Only works if common_handle_winch is registered to handle winch signals.
*/
int common_get_height();

/**
  Handle a window change event by looking up the new window size and
  saving it in an internal variable used by common_get_wisth and
  common_get_height().
*/
void common_handle_winch( int signal );

/**
   Write paragraph of output to the specified stringbuffer, and redo
   the linebreaks to fit the current screen.
*/
void write_screen( const wcstring &msg, wcstring &buff );

/**
   Tokenize the specified string into the specified wcstring_list_t.   
   \param val the input string. The contents of this string is not changed.
   \param out the list in which to place the elements. 
*/
void tokenize_variable_array( const wcstring &val, wcstring_list_t &out);

/**
   Make sure the specified direcotry exists. If needed, try to create
   it and any currently not existing parent directories..

   \return 0 if, at the time of function return the directory exists, -1 otherwise.
*/
int create_directory( const wcstring &d );

/**
   Print a short message about how to file a bug report to stderr
*/
void bugreport();

/**
   Return the number of seconds from the UNIX epoch, with subsecond
   precision. This function uses the gettimeofday function, and will
   have the same precision as that function.

   If an error occurs, NAN is returned.
 */
double timef();

/**
	Call the following function early in main to set the main thread.
    This is our replacement for pthread_main_np().
 */
void set_main_thread();
bool is_main_thread();

/** Configures thread assertions for testing */
void configure_thread_assertions_for_testing();

/** Set up a guard to complain if we try to do certain things (like take a lock) after calling fork */
void setup_fork_guards(void);

/** Return whether we are the child of a fork */
bool is_forked_child(void);
void assert_is_not_forked_child(const char *who);
#define ASSERT_IS_NOT_FORKED_CHILD_TRAMPOLINE(x) assert_is_not_forked_child(x)
#define ASSERT_IS_NOT_FORKED_CHILD() ASSERT_IS_NOT_FORKED_CHILD_TRAMPOLINE(__FUNCTION__)

extern "C" {
__attribute__((noinline)) void debug_thread_error(void);
}


#endif
