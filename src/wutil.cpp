// Wide character equivalents of various standard unix functions.
#define FISH_NO_ISW_WRAPPERS
#include "config.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#include <map>
#include <string>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "wutil.h"     // IWYU pragma: keep

typedef std::string cstring;

const file_id_t kInvalidFileID = {(dev_t)-1LL, (ino_t)-1LL, (uint64_t)-1LL, -1, -1, -1, -1};

#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
/// Fallback length of MAXPATHLEN. Hopefully a sane value.
#define PATH_MAX 4096
#endif
#endif

/// Map used as cache by wgettext.
static owning_lock<std::map<wcstring, wcstring>> wgettext_map;

bool wreaddir_resolving(DIR *dir, const std::wstring &dir_path, std::wstring &out_name,
                        bool *out_is_dir) {
    struct dirent *d = readdir(dir);
    if (!d) return false;

    out_name = str2wcstring(d->d_name);
    if (!out_is_dir) return true;

    // The caller cares if this is a directory, so check.
    bool is_dir = false;
    // We may be able to skip stat, if the readdir can tell us the file type directly.
    bool check_with_stat = true;
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
    if (d->d_type == DT_DIR) {
        // Known directory.
        is_dir = true;
        check_with_stat = false;
    } else if (d->d_type == DT_LNK || d->d_type == DT_UNKNOWN) {
        // We want to treat symlinks to directories as directories. Use stat to resolve it.
        check_with_stat = true;
    } else {
        // Regular file.
        is_dir = false;
        check_with_stat = false;
    }
#endif  // HAVE_STRUCT_DIRENT_D_TYPE
    if (check_with_stat) {
        // We couldn't determine the file type from the dirent; check by stat'ing it.
        cstring fullpath = wcs2string(dir_path);
        fullpath.push_back('/');
        fullpath.append(d->d_name);
        struct stat buf;
        if (stat(fullpath.c_str(), &buf) != 0) {
            is_dir = false;
        } else {
            is_dir = static_cast<bool>(S_ISDIR(buf.st_mode));
        }
    }
    *out_is_dir = is_dir;
    return true;
}

bool wreaddir(DIR *dir, std::wstring &out_name) {
    struct dirent *d = readdir(dir);
    if (!d) return false;

    out_name = str2wcstring(d->d_name);
    return true;
}

bool wreaddir_for_dirs(DIR *dir, wcstring *out_name) {
    struct dirent *result = NULL;
    while (result == NULL) {
        struct dirent *d = readdir(dir);
        if (!d) break;

#if HAVE_STRUCT_DIRENT_D_TYPE
        switch (d->d_type) {
            case DT_DIR:
            case DT_LNK:
            case DT_UNKNOWN: {
                // These may be directories.
                result = d;
                break;
            }
            default: {
                // Nothing else can.
                break;
            }
        }
#else
        // We can't determine if it's a directory or not, so just return it.
        result = d;
#endif
    }
    if (result && out_name) {
        *out_name = str2wcstring(result->d_name);
    }
    return result != NULL;
}

const wcstring wgetcwd() {
    wcstring retval;

    char *res = getcwd(NULL, 0);
    if (res) {
        retval = str2wcstring(res);
        free(res);
    } else {
        debug(0, _(L"getcwd() failed with errno %d/%s"), errno, strerror(errno));
        retval = wcstring();
    }

    return retval;
}

int wchdir(const wcstring &dir) {
    cstring tmp = wcs2string(dir);
    return chdir(tmp.c_str());
}

FILE *wfopen(const wcstring &path, const char *mode) {
    int permissions = 0, options = 0;
    size_t idx = 0;
    switch (mode[idx++]) {
        case 'r': {
            permissions = O_RDONLY;
            break;
        }
        case 'w': {
            permissions = O_WRONLY;
            options = O_CREAT | O_TRUNC;
            break;
        }
        case 'a': {
            permissions = O_WRONLY;
            options = O_CREAT | O_APPEND;
            break;
        }
        default: {
            errno = EINVAL;
            return NULL;
        }
    }
    // Skip binary.
    if (mode[idx] == 'b') idx++;

    // Consider append option.
    if (mode[idx] == '+') permissions = O_RDWR;

    int fd = wopen_cloexec(path, permissions | options, 0666);
    if (fd < 0) return NULL;
    FILE *result = fdopen(fd, mode);
    if (result == NULL) close(fd);
    return result;
}

bool set_cloexec(int fd) {
    int flags = fcntl(fd, F_GETFD, 0);
    if (flags < 0) {
        return false;
    }
    if (flags & FD_CLOEXEC) {
        return true;
    }
    return fcntl(fd, F_SETFD, flags | FD_CLOEXEC) >= 0;
}

static int wopen_internal(const wcstring &pathname, int flags, mode_t mode, bool cloexec) {
    ASSERT_IS_NOT_FORKED_CHILD();
    cstring tmp = wcs2string(pathname);
    int fd;

#ifdef O_CLOEXEC
    // Prefer to use O_CLOEXEC. It has to both be defined and nonzero.
    if (cloexec) {
        fd = open(tmp.c_str(), flags | O_CLOEXEC, mode);
    } else {
        fd = open(tmp.c_str(), flags, mode);
    }
#else
    fd = open(tmp.c_str(), flags, mode);
    if (fd >= 0 && !set_cloexec(fd)) {
        close(fd);
        fd = -1;
    }
#endif
    return fd;
}

int wopen_cloexec(const wcstring &pathname, int flags, mode_t mode) {
    return wopen_internal(pathname, flags, mode, true);
}

DIR *wopendir(const wcstring &name) {
    const cstring tmp = wcs2string(name);
    return opendir(tmp.c_str());
}

int wstat(const wcstring &file_name, struct stat *buf) {
    const cstring tmp = wcs2string(file_name);
    return stat(tmp.c_str(), buf);
}

int lwstat(const wcstring &file_name, struct stat *buf) {
    const cstring tmp = wcs2string(file_name);
    return lstat(tmp.c_str(), buf);
}

int waccess(const wcstring &file_name, int mode) {
    const cstring tmp = wcs2string(file_name);
    return access(tmp.c_str(), mode);
}

int wunlink(const wcstring &file_name) {
    const cstring tmp = wcs2string(file_name);
    return unlink(tmp.c_str());
}

void wperror(const wchar_t *s) {
    int e = errno;
    if (s[0] != L'\0') {
        fwprintf(stderr, L"%ls: ", s);
    }
    fwprintf(stderr, L"%s\n", strerror(e));
}

int make_fd_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    int err = 0;
    bool nonblocking = flags & O_NONBLOCK;
    if (!nonblocking) {
        err = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    return err == -1 ? errno : 0;
}

int make_fd_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    int err = 0;
    bool nonblocking = flags & O_NONBLOCK;
    if (nonblocking) {
        err = fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }
    return err == -1 ? errno : 0;
}

static inline void safe_append(char *buffer, const char *s, size_t buffsize) {
    strncat(buffer, s, buffsize - strlen(buffer) - 1);
}

// In general, strerror is not async-safe, and therefore we cannot use it directly. So instead we
// have to grub through sys_nerr and sys_errlist directly On GNU toolchain, this will produce a
// deprecation warning from the linker (!!), which appears impossible to suppress!
const char *safe_strerror(int err) {
#if defined(__UCLIBC__)
    // uClibc does not have sys_errlist, however, its strerror is believed to be async-safe.
    // See issue #808.
    return strerror(err);
#elif defined(HAVE__SYS__ERRS) || defined(HAVE_SYS_ERRLIST)
#ifdef HAVE_SYS_ERRLIST
    if (err >= 0 && err < sys_nerr && sys_errlist[err] != NULL) {
        return sys_errlist[err];
    }
#elif defined(HAVE__SYS__ERRS)
    extern const char _sys_errs[];
    extern const int _sys_index[];
    extern int _sys_num_err;

    if (err >= 0 && err < _sys_num_err) {
        return &_sys_errs[_sys_index[err]];
    }
#endif  // either HAVE__SYS__ERRS or HAVE_SYS_ERRLIST
#endif  // defined(HAVE__SYS__ERRS) || defined(HAVE_SYS_ERRLIST)

    int saved_err = errno;
    static char buff[384];  // use a shared buffer for this case
    char errnum_buff[64];
    format_long_safe(errnum_buff, err);

    buff[0] = '\0';
    safe_append(buff, "unknown error (errno was ", sizeof buff);
    safe_append(buff, errnum_buff, sizeof buff);
    safe_append(buff, ")", sizeof buff);

    errno = saved_err;
    return buff;
}

void safe_perror(const char *message) {
    // Note we cannot use strerror, because on Linux it uses gettext, which is not safe.
    int err = errno;

    char buff[384];
    buff[0] = '\0';

    if (message) {
        safe_append(buff, message, sizeof buff);
        safe_append(buff, ": ", sizeof buff);
    }
    safe_append(buff, safe_strerror(err), sizeof buff);
    safe_append(buff, "\n", sizeof buff);

    write_ignore(STDERR_FILENO, buff, strlen(buff));
    errno = err;
}

wchar_t *wrealpath(const wcstring &pathname, wchar_t *resolved_path) {
    if (pathname.size() == 0) return NULL;

    cstring real_path("");
    cstring narrow_path = wcs2string(pathname);

    // Strip trailing slashes. This is needed to be bug-for-bug compatible with GNU realpath which
    // treats "/a//" as equivalent to "/a" whether or not /a exists.
    while (narrow_path.size() > 1 && narrow_path.at(narrow_path.size() - 1) == '/') {
        narrow_path.erase(narrow_path.size() - 1, 1);
    }

    char *narrow_res = realpath(narrow_path.c_str(), NULL);
    if (narrow_res) {
        real_path.append(narrow_res);
    } else {
        size_t pathsep_idx = narrow_path.rfind('/');
        if (pathsep_idx == 0) {
            // If the only pathsep is the first character then it's an absolute path with a
            // single path component and thus doesn't need conversion.
            real_path = narrow_path;
        } else {
            if (pathsep_idx == cstring::npos) {
                // No pathsep means a single path component relative to pwd.
                narrow_res = realpath(".", NULL);
                assert(narrow_res != NULL);
                pathsep_idx = 0;
            } else {
                // Only call realpath() on the portion up to the last component.
                narrow_res = realpath(narrow_path.substr(0, pathsep_idx).c_str(), NULL);
                if (!narrow_res) return NULL;
                pathsep_idx++;
            }
            real_path.append(narrow_res);
            // This test is to deal with pathological cases such as /../../x => //x.
            if (real_path.size() > 1) real_path.append("/");
            real_path.append(narrow_path.substr(pathsep_idx, cstring::npos));
        }
    }
#if __APPLE__ && __DARWIN_C_LEVEL < 200809L
// OS X Snow Leopard is broken with respect to the dynamically allocated buffer returned by
// realpath(). It's not dynamically allocated so attempting to free that buffer triggers a
// malloc/free error. Thus we don't attempt the free in this case.
#else
    free(narrow_res);
#endif

    wcstring wreal_path = str2wcstring(real_path);
    if (resolved_path) {
        wcslcpy(resolved_path, wreal_path.c_str(), PATH_MAX);
        return resolved_path;
    }
    return wcsdup(wreal_path.c_str());
}

wcstring wdirname(const wcstring &path) {
    char *tmp = wcs2str(path.c_str());
    char *narrow_res = dirname(tmp);
    wcstring result = format_string(L"%s", narrow_res);
    free(tmp);
    return result;
}

wcstring wbasename(const wcstring &path) {
    char *tmp = wcs2str(path.c_str());
    char *narrow_res = basename(tmp);
    wcstring result = format_string(L"%s", narrow_res);
    free(tmp);
    return result;
}

// Really init wgettext.
static void wgettext_really_init() {
    fish_bindtextdomain(PACKAGE_NAME, LOCALEDIR);
    fish_textdomain(PACKAGE_NAME);
}

/// For wgettext: Internal init function. Automatically called when a translation is first
/// requested.
static void wgettext_init_if_necessary() {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, wgettext_really_init);
}

const wcstring &wgettext(const wchar_t *in) {
    // Preserve errno across this since this is often used in printing error messages.
    int err = errno;
    wcstring key = in;

    wgettext_init_if_necessary();
    auto wmap = wgettext_map.acquire();
    wcstring &val = wmap.value[key];
    if (val.empty()) {
        cstring mbs_in = wcs2string(key);
        char *out = fish_gettext(mbs_in.c_str());
        val = format_string(L"%s", out);
    }
    errno = err;

    // The returned string is stored in the map.
    // TODO: If we want to shrink the map, this would be a problem.
    return val;
}

int wmkdir(const wcstring &name, int mode) {
    cstring name_narrow = wcs2string(name);
    return mkdir(name_narrow.c_str(), mode);
}

int wrename(const wcstring &old, const wcstring &newv) {
    cstring old_narrow = wcs2string(old);
    cstring new_narrow = wcs2string(newv);
    return rename(old_narrow.c_str(), new_narrow.c_str());
}

/// Return one if the code point is in a Unicode private use area.
int fish_is_pua(wint_t wc) {
    if (PUA1_START <= wc && wc < PUA1_END) return 1;
    if (PUA2_START <= wc && wc < PUA2_END) return 1;
    if (PUA3_START <= wc && wc < PUA3_END) return 1;
    return 0;
}

/// We need this because there are too many implementations that don't return the proper answer for
/// some code points. See issue #3050.
int fish_iswalnum(wint_t wc) {
    if (fish_reserved_codepoint(wc)) return 0;
    if (fish_is_pua(wc)) return 0;
    return iswalnum(wc);
}

/// We need this because there are too many implementations that don't return the proper answer for
/// some code points. See issue #3050.
int fish_iswalpha(wint_t wc) {
    if (fish_reserved_codepoint(wc)) return 0;
    if (fish_is_pua(wc)) return 0;
    return iswalpha(wc);
}

/// We need this because there are too many implementations that don't return the proper answer for
/// some code points. See issue #3050.
int fish_iswgraph(wint_t wc) {
    if (fish_reserved_codepoint(wc)) return 0;
    if (fish_is_pua(wc)) return 1;
    return iswgraph(wc);
}

/// Test if the given string is a valid variable name.
///
/// \return null if this is a valid name, and a pointer to the first invalid character otherwise.
const wchar_t *wcsvarname(const wchar_t *str) {
    if (str[0] == L'\0') return str;
    while (*str) {
        if ((!fish_iswalnum(*str)) && (*str != L'_')) {
            return str;
        }
        str++;
    }
    return NULL;
}

/// Test if the given string is a valid variable name.
///
/// \return null if this is a valid name, and a pointer to the first invalid character otherwise.
const wchar_t *wcsvarname(const wcstring &str) { return wcsvarname(str.c_str()); }

/// Test if the string is a valid function name.
///
/// \return true if it is valid else false.
bool wcsfuncname(const wcstring &str) {
    if (str.size() == 0) return false;
    if (str.at(0) == L'-') return false;
    if (str.find_first_of(L'/') != wcstring::npos) return false;
    return true;
}

/// Test if the given string is valid in a variable name.
///
/// \return true if this is a valid name, false otherwise.
bool wcsvarchr(wchar_t chr) { return fish_iswalnum(chr) || chr == L'_'; }

/// Convenience variants on fish_wcwswidth().
///
/// See fallback.h for the normal definitions.
int fish_wcswidth(const wchar_t *str) { return fish_wcswidth(str, wcslen(str)); }

/// Convenience variants on fish_wcwswidth().
///
/// See fallback.h for the normal definitions.
int fish_wcswidth(const wcstring &str) { return fish_wcswidth(str.c_str(), str.size()); }

/// Like fish_wcstol(), but fails on a value outside the range of an int.
///
/// This is needed because BSD and GNU implementations differ in several ways that make it really
/// annoying to use them in a portable fashion.
///
/// The caller doesn't have to zero errno. Sets errno to -1 if the int ends with something other
/// than a digit. Leading whitespace is ignored (per the base wcstol implementation). Trailing
/// whitespace is also ignored. We also treat empty strings and strings containing only whitespace
/// as invalid.
int fish_wcstoi(const wchar_t *str, const wchar_t **endptr, int base) {
    while (iswspace(*str)) ++str;  // skip leading whitespace
    if (!*str) {  // this is because some implementations don't handle this sensibly
        errno = EINVAL;
        if (endptr) *endptr = str;
        return 0;
    }

    errno = 0;
    wchar_t *_endptr;
    long result = wcstol(str, &_endptr, base);
    if (result > INT_MAX) {
        result = INT_MAX;
        errno = ERANGE;
    } else if (result < INT_MIN) {
        result = INT_MIN;
        errno = ERANGE;
    }
    while (iswspace(*_endptr)) ++_endptr;  // skip trailing whitespace
    if (!errno && *_endptr) {
        if (_endptr == str) {
            errno = EINVAL;
        } else {
            errno = -1;
        }
    }
    if (endptr) *endptr = _endptr;
    return (int)result;
}

/// An enhanced version of wcstol().
///
/// This is needed because BSD and GNU implementations differ in several ways that make it really
/// annoying to use them in a portable fashion.
///
/// The caller doesn't have to zero errno. Sets errno to -1 if the int ends with something other
/// than a digit. Leading whitespace is ignored (per the base wcstol implementation). Trailing
/// whitespace is also ignored.
long fish_wcstol(const wchar_t *str, const wchar_t **endptr, int base) {
    while (iswspace(*str)) ++str;  // skip leading whitespace
    if (!*str) {  // this is because some implementations don't handle this sensibly
        errno = EINVAL;
        if (endptr) *endptr = str;
        return 0;
    }

    errno = 0;
    wchar_t *_endptr;
    long result = wcstol(str, &_endptr, base);
    while (iswspace(*_endptr)) ++_endptr;  // skip trailing whitespace
    if (!errno && *_endptr) {
        if (_endptr == str) {
            errno = EINVAL;
        } else {
            errno = -1;
        }
    }
    if (endptr) *endptr = _endptr;
    return result;
}

/// An enhanced version of wcstoll().
///
/// This is needed because BSD and GNU implementations differ in several ways that make it really
/// annoying to use them in a portable fashion.
///
/// The caller doesn't have to zero errno. Sets errno to -1 if the int ends with something other
/// than a digit. Leading whitespace is ignored (per the base wcstoll implementation). Trailing
/// whitespace is also ignored.
long long fish_wcstoll(const wchar_t *str, const wchar_t **endptr, int base) {
    while (iswspace(*str)) ++str;  // skip leading whitespace
    if (!*str) {  // this is because some implementations don't handle this sensibly
        errno = EINVAL;
        if (endptr) *endptr = str;
        return 0;
    }

    errno = 0;
    wchar_t *_endptr;
    long long result = wcstoll(str, &_endptr, base);
    while (iswspace(*_endptr)) ++_endptr;  // skip trailing whitespace
    if (!errno && *_endptr) {
        if (_endptr == str) {
            errno = EINVAL;
        } else {
            errno = -1;
        }
    }
    if (endptr) *endptr = _endptr;
    return result;
}

/// An enhanced version of wcstoull().
///
/// This is needed because BSD and GNU implementations differ in several ways that make it really
/// annoying to use them in a portable fashion.
///
/// The caller doesn't have to zero errno. Sets errno to -1 if the int ends with something other
/// than a digit. Leading minus is considered invalid. Leading whitespace is ignored (per the base
/// wcstoull implementation). Trailing whitespace is also ignored.
unsigned long long fish_wcstoull(const wchar_t *str, const wchar_t **endptr, int base) {
    while (iswspace(*str)) ++str;  // skip leading whitespace
    if (!*str ||      // this is because some implementations don't handle this sensibly
        *str == '-')  // disallow minus as the first character to avoid questionable wrap-around
    {
        errno = EINVAL;
        if (endptr) *endptr = str;
        return 0;
    }

    errno = 0;
    wchar_t *_endptr;
    unsigned long long result = wcstoull(str, &_endptr, base);
    while (iswspace(*_endptr)) ++_endptr;  // skip trailing whitespace
    if (!errno && *_endptr) {
        if (_endptr == str) {
            errno = EINVAL;
        } else {
            errno = -1;
        }
    }
    if (endptr) *endptr = _endptr;
    return result;
}

file_id_t file_id_t::file_id_from_stat(const struct stat *buf) {
    assert(buf != NULL);

    file_id_t result = {};
    result.device = buf->st_dev;
    result.inode = buf->st_ino;
    result.size = buf->st_size;
    result.change_seconds = buf->st_ctime;
    result.mod_seconds = buf->st_mtime;

#ifdef HAVE_STRUCT_STAT_ST_CTIME_NSEC
    result.change_nanoseconds = buf->st_ctime_nsec;
    result.mod_nanoseconds = buf->st_mtime_nsec;
#elif defined(__APPLE__)
    result.change_nanoseconds = buf->st_ctimespec.tv_nsec;
    result.mod_nanoseconds = buf->st_mtimespec.tv_nsec;
#elif defined(_BSD_SOURCE) || defined(_SVID_SOURCE) || defined(_XOPEN_SOURCE)
    result.change_nanoseconds = buf->st_ctim.tv_nsec;
    result.mod_nanoseconds = buf->st_mtim.tv_nsec;
#else
    result.change_nanoseconds = 0;
    result.mod_nanoseconds = 0;
#endif

    return result;
}

file_id_t file_id_for_fd(int fd) {
    file_id_t result = kInvalidFileID;
    struct stat buf = {};
    if (fd >= 0 && 0 == fstat(fd, &buf)) {
        result = file_id_t::file_id_from_stat(&buf);
    }
    return result;
}

file_id_t file_id_for_path(const wcstring &path) {
    file_id_t result = kInvalidFileID;
    struct stat buf = {};
    if (0 == wstat(path, &buf)) {
        result = file_id_t::file_id_from_stat(&buf);
    }
    return result;
}

bool file_id_t::operator==(const file_id_t &rhs) const { return this->compare_file_id(rhs) == 0; }

bool file_id_t::operator!=(const file_id_t &rhs) const { return !(*this == rhs); }

template <typename T>
int compare(T a, T b) {
    if (a < b) {
        return -1;
    } else if (a > b) {
        return 1;
    }
    return 0;
}

int file_id_t::compare_file_id(const file_id_t &rhs) const {
    // Compare each field, stopping when we get to a non-equal field.
    int ret = 0;
    if (!ret) ret = compare(device, rhs.device);
    if (!ret) ret = compare(inode, rhs.inode);
    if (!ret) ret = compare(size, rhs.size);
    if (!ret) ret = compare(change_seconds, rhs.change_seconds);
    if (!ret) ret = compare(change_nanoseconds, rhs.change_nanoseconds);
    if (!ret) ret = compare(mod_seconds, rhs.mod_seconds);
    if (!ret) ret = compare(mod_nanoseconds, rhs.mod_nanoseconds);
    return ret;
}

bool file_id_t::operator<(const file_id_t &rhs) const { return this->compare_file_id(rhs) < 0; }
