// Wide character equivalents of various standard unix functions.
#define FISH_NO_ISW_WRAPPERS
#include "config.h"

#include <assert.h>
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
#include <map>
#include <memory>
#include <string>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "wutil.h"     // IWYU pragma: keep

const file_id_t kInvalidFileID = {(dev_t)-1LL, (ino_t)-1LL, (uint64_t)-1LL, -1, -1, -1, -1};

#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
#define PATH_MAX 4096 // Fallback length of MAXPATHLEN. Hopefully a sane value.
#endif
#endif

/// Map used as cache by wgettext.
typedef std::map<wcstring, wcstring> wgettext_map_t;
/// Lock to protect wgettext.
static pthread_mutex_t wgettext_lock;
static wgettext_map_t wgettext_map;

/// Wide character version of fopen(). This sets CLO_EXEC.
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
    } else if (flags & FD_CLOEXEC) {
        return true;
    } else {
        return fcntl(fd, F_SETFD, flags | FD_CLOEXEC) >= 0;
    }
}

static int wopen_internal(const wcstring &pathname, int flags, mode_t mode, bool cloexec) {
    ASSERT_IS_NOT_FORKED_CHILD();
    std::string tmp = wcs2string(pathname);
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

/// Wide character version of open() that also sets the close-on-exec flag (atomically when
/// possible).
int wopen_cloexec(const wcstring &pathname, int flags, mode_t mode) {
    return wopen_internal(pathname, flags, mode, true);
}


bool wreaddir_resolving(DIR *dir, const wcstring &dir_path, wcstring &out_name, bool *out_is_dir) {
    struct dirent *d = readdir(dir);    if (!d) return false;

    out_name = str2wcstring(d->d_name);
    if (out_is_dir) {
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
            std::string fullpath = wcs2string(dir_path);
            fullpath.push_back('/');
            fullpath.append(d->d_name);
            struct stat buf;
            if (stat(fullpath.c_str(), &buf) != 0) {
                is_dir = false;
            } else {
                is_dir = !!(S_ISDIR(buf.st_mode));
            }
        }
        *out_is_dir = is_dir;
    }
    return true;
}

/// Wide-character version of readdir()
bool wreaddir(DIR *dir, wcstring &out_name) {       
    struct dirent *d = readdir(dir);
    if (!d) return false;

    out_name = str2wcstring(d->d_name);
    return true;
}

/// Like wreaddir, but skip items that are known to not be directories. If this requires a stat
/// (i.e. the file is a symlink), then return it. Note that this does not guarantee that everything
/// returned is a directory, it's just an optimization for cases where we would check for
/// directories anyways.
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

/// Wide character version of getcwd().
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

/// Wide character version of chdir().
int wchdir(const wcstring &dir) {
    std::string tmp = wcs2string(dir);
    return chdir(tmp.c_str());
}

/// Wide character version of opendir(). Note that opendir() is guaranteed to set close-on-exec by
/// POSIX (hooray).
DIR *wopendir(const wcstring &name) {
    const std::string tmp = wcs2string(name);
    return opendir(tmp.c_str());
}

/// Wide character version of stat().
int wstat(const wcstring &file_name, struct stat *buf) {
    const std::string tmp = wcs2string(file_name);
    return stat(tmp.c_str(), buf);
}

/// Wide character version of lstat().
int lwstat(const wcstring &file_name, struct stat *buf) {
    const std::string tmp = wcs2string(file_name);
    return lstat(tmp.c_str(), buf);
}

/// Wide character version of access().
int waccess(const wcstring &file_name, int mode) {
    const std::string tmp = wcs2string(file_name);
    return access(tmp.c_str(), mode);
}

/// Wide character version of unlink().
int wunlink(const wcstring &file_name) {
    const std::string tmp = wcs2string(file_name);
    return unlink(tmp.c_str());
}

/// Wide character version of perror().
void wperror(const wchar_t *s) {
    int e = errno;
    if (s[0] != L'\0') {
        fwprintf(stderr, L"%ls: ", s);
    }
    fwprintf(stderr, L"%s\n", strerror(e));
}

/// Mark an fd as nonblocking; returns errno or 0 on success.
int make_fd_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    int err = 0;
    bool nonblocking = flags & O_NONBLOCK;
    if (!nonblocking) {
        err = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    return err == -1 ? errno : 0;
}

/// Mark an fd as blocking; returns errno or 0 on success.
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

/// Async-safe version of strerror().
/// In general, strerror is not async-safe, and therefore we cannot use it directly. So instead we
/// have to grub through sys_nerr and sys_errlist directly On GNU toolchain, this will produce a
/// deprecation warning from the linker (!!), which appears impossible to suppress!
/// XXX Use strerror_r instead!
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

/// Async-safe version of perror().
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

#ifdef HAVE_REALPATH_NULL

/// Wide character version of realpath function. Just like the GNU version of realpath, wrealpath
/// will accept 0 as the value for the second argument, in which case the result will be allocated
/// using malloc, and must be free'd by the user.
wchar_t *wrealpath(const wcstring &pathname, wchar_t *resolved_path) {
    std::string narrow_path = wcs2string(pathname);
    char *narrow_res = realpath(narrow_path.c_str(), NULL);

    if (!narrow_res) return NULL;

    wchar_t *res;
    wcstring wide_res = str2wcstring(narrow_res);
    if (resolved_path) {
        wcslcpy(resolved_path, wide_res.c_str(), PATH_MAX);
        res = resolved_path;
    } else {
        res = wcsdup(wide_res.c_str());
    }

#if __APPLE__ && __DARWIN_C_LEVEL < 200809L
// OS X Snow Leopard is broken with respect to the dynamically allocated buffer returned by
// realpath(). It's not dynamically allocated so attempting to free that buffer triggers a
// malloc/free error. Thus we don't attempt the free in this case.
#else
    free(narrow_res);
#endif

    return res;
}

#else

wchar_t *wrealpath(const wcstring &pathname, wchar_t *resolved_path) {
    std::string tmp = wcs2string(pathname);
    char narrow_buff[PATH_MAX];
    char *narrow_res = realpath(tmp.c_str(), narrow_buff);
    wchar_t *res;

    if (!narrow_res) return 0;

    const wcstring wide_res = str2wcstring(narrow_res);
    if (resolved_path) {
        wcslcpy(resolved_path, wide_res.c_str(), PATH_MAX);
        res = resolved_path;
    } else {
        res = wcsdup(wide_res.c_str());
    }
    return res;
}

#endif

/// Wide character version of dirname().
wcstring wdirname(const wcstring &path) {
    char *tmp = wcs2str(path.c_str());
    char *narrow_res = dirname(tmp);
    wcstring result = format_string(L"%s", narrow_res);
    free(tmp);
    return result;
}

/// Wide character version of basename().
wcstring wbasename(const wcstring &path) {
    char *tmp = wcs2str(path.c_str());
    char *narrow_res = basename(tmp);
    wcstring result = format_string(L"%s", narrow_res);
    free(tmp);
    return result;
}

// Really init wgettext.
static void wgettext_really_init() {
    pthread_mutex_init(&wgettext_lock, NULL);
    fish_bindtextdomain(PACKAGE_NAME, LOCALEDIR);
    fish_textdomain(PACKAGE_NAME);
}

/// For wgettext: Internal init function. Automatically called when a translation is first
/// requested.
static void wgettext_init_if_necessary() {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, wgettext_really_init);
}

/// Wide character wrapper around the gettext function. For historic reasons, unlike the real
/// gettext function, wgettext takes care of setting the correct domain, etc. using the textdomain
/// and bindtextdomain functions. This should probably be moved out of wgettext, so that wgettext
/// will be nothing more than a wrapper around gettext, like all other functions in this file.
const wcstring &wgettext(const wchar_t *in) {
    // Preserve errno across this since this is often used in printing error messages.
    int err = errno;
    wcstring key = in;

    wgettext_init_if_necessary();
    scoped_lock locker(wgettext_lock);
    wcstring &val = wgettext_map[key];
    if (val.empty()) {
        std::string mbs_in = wcs2string(key);
        char *out = fish_gettext(mbs_in.c_str());
        val = format_string(L"%s", out);
    }
    errno = err;

    // The returned string is stored in the map.
    // TODO: If we want to shrink the map, this would be a problem.
    return val;
}

/// Wide character version of mkdir.
int wmkdir(const wcstring &name, int mode) {
    std::string name_narrow = wcs2string(name);
    return mkdir(name_narrow.c_str(), mode);
}

/// Wide character version of rename.
int wrename(const wcstring &old, const wcstring &newv) {
    std::string old_narrow = wcs2string(old);
    std::string new_narrow = wcs2string(newv);
    return rename(old_narrow.c_str(), new_narrow.c_str());
}

/// Return one if the code point is in the range we reserve for internal use.
int fish_is_reserved_codepoint(wint_t wc) {
    if (RESERVED_CHAR_BASE <= wc && wc < RESERVED_CHAR_END) return 1;
    if (EXPAND_RESERVED_BASE <= wc && wc < EXPAND_RESERVED_END) return 1;
    if (WILDCARD_RESERVED_BASE <= wc && wc < WILDCARD_RESERVED_END) return 1;
    return 0;
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
    if (fish_is_reserved_codepoint(wc)) return 0;
    if (fish_is_pua(wc)) return 0;
    return iswalnum(wc);
}

/// We need this because there are too many implementations that don't return the proper answer for
/// some code points. See issue #3050.
int fish_iswalpha(wint_t wc) {
    if (fish_is_reserved_codepoint(wc)) return 0;
    if (fish_is_pua(wc)) return 0;
    return iswalpha(wc);
}

/// We need this because there are too many implementations that don't return the proper answer for
/// some code points. See issue #3050.
int fish_iswgraph(wint_t wc) {
    if (fish_is_reserved_codepoint(wc)) return 0;
    if (fish_is_pua(wc)) return 1;
    return iswgraph(wc);
}

/// Test if the given string is a valid variable name.
///
/// \return null if this is a valid name, and a pointer to the first invalid character otherwise.
const wchar_t *wcsvarname(const wchar_t *str) {
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

/// Test if the given string is a valid function name.
///
/// \return null if this is a valid name, and a pointer to the first invalid character otherwise.
const wchar_t *wcsfuncname(const wcstring &str) { return wcschr(str.c_str(), L'/'); }

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

file_id_t file_id_t::file_id_from_stat(const struct stat *buf) {
    assert(buf != NULL);

    file_id_t result = {};
    result.device = buf->st_dev;
    result.inode = buf->st_ino;
    result.size = buf->st_size;
    result.change_seconds = buf->st_ctime;
    result.mod_seconds = buf->st_mtime;

#if STAT_HAVE_NSEC
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
    if (0 == fstat(fd, &buf)) {
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
