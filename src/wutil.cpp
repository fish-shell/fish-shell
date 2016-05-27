// Wide character equivalents of various standard unix functions.
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

/// Lock to protect wgettext.
static pthread_mutex_t wgettext_lock;

/// Map used as cache by wgettext.
typedef std::map<wcstring, wcstring> wgettext_map_t;
static wgettext_map_t wgettext_map;

bool wreaddir_resolving(DIR *dir, const std::wstring &dir_path, std::wstring &out_name,
                        bool *out_is_dir) {
    struct dirent *d = readdir(dir);
    if (!d) return false;

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
            cstring fullpath = wcs2string(dir_path);
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
            break;
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
    cstring tmp = wcs2string(pathname);
// Prefer to use O_CLOEXEC. It has to both be defined and nonzero.
#ifdef O_CLOEXEC
    if (cloexec && (O_CLOEXEC != 0)) {
        flags |= O_CLOEXEC;
        cloexec = false;
    }
#endif
    int fd = ::open(tmp.c_str(), flags, mode);
    if (cloexec && fd >= 0 && !set_cloexec(fd)) {
        close(fd);
        fd = -1;
    }
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
    if (!(flags & O_NONBLOCK)) {
        err = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    return err == -1 ? errno : 0;
}

int make_fd_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    int err = 0;
    if (flags & O_NONBLOCK) {
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

#ifdef HAVE_REALPATH_NULL

wchar_t *wrealpath(const wcstring &pathname, wchar_t *resolved_path) {
    cstring narrow_path = wcs2string(pathname);
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
    cstring tmp = wcs2string(pathname);
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

const wchar_t *wgettext(const wchar_t *in) {
    if (!in) return in;

    // Preserve errno across this since this is often used in printing error messages.
    int err = errno;

    wgettext_init_if_necessary();

    wcstring key = in;
    scoped_lock lock(wgettext_lock);

    wcstring &val = wgettext_map[key];
    if (val.empty()) {
        cstring mbs_in = wcs2string(key);
        char *out = fish_gettext(mbs_in.c_str());
        val = format_string(L"%s", out);
    }
    errno = err;

    // The returned string is stored in the map.
    // TODO: If we want to shrink the map, this would be a problem.
    return val.c_str();
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

int fish_wcstoi(const wchar_t *str, wchar_t **endptr, int base) {
    long ret = wcstol(str, endptr, base);
    if (ret > INT_MAX) {
        ret = INT_MAX;
        errno = ERANGE;
    } else if (ret < INT_MIN) {
        ret = INT_MIN;
        errno = ERANGE;
    }
    return (int)ret;
}

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
