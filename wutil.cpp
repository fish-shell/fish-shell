/** \file wutil.c
  Wide character equivalents of various standard unix
  functions.
*/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <dirent.h>
#include <stdarg.h>
#include <limits.h>
#include <libgen.h>
#include <pthread.h>
#include <string>
#include <map>


#if HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "wutil.h"

typedef std::string cstring;

/**
   Minimum length of the internal covnersion buffers
*/
#define TMP_LEN_MIN 256

#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
/**
   Fallback length of MAXPATHLEN. Just a hopefully sane value...
*/
#define PATH_MAX 4096
#endif
#endif

/* Lock to protect wgettext */
static pthread_mutex_t wgettext_lock;

/* Maps string keys to (immortal) pointers to string values */
typedef std::map<wcstring, wcstring *> wgettext_map_t;
static std::map<wcstring, wcstring *> wgettext_map;

void wutil_init()
{
}

void wutil_destroy()
{
}

bool wreaddir_resolving(DIR *dir, const std::wstring &dir_path, std::wstring &out_name, bool *out_is_dir)
{
    struct dirent *d = readdir(dir);
    if (!d) return false;

    out_name = str2wcstring(d->d_name);
    if (out_is_dir)
    {
        /* The caller cares if this is a directory, so check */
        bool is_dir;
        if (d->d_type == DT_DIR)
        {
            is_dir = true;
        }
        else if (d->d_type == DT_LNK || d->d_type == DT_UNKNOWN)
        {
            /* We want to treat symlinks to directories as directories. Use stat to resolve it. */
            cstring fullpath = wcs2string(dir_path);
            fullpath.push_back('/');
            fullpath.append(d->d_name);
            struct stat buf;
            if (stat(fullpath.c_str(), &buf) != 0)
            {
                is_dir = false;
            }
            else
            {
                is_dir = !!(S_ISDIR(buf.st_mode));
            }
        }
        else
        {
            is_dir = false;
        }
        *out_is_dir = is_dir;
    }
    return true;
}

bool wreaddir(DIR *dir, std::wstring &out_name)
{
    struct dirent *d = readdir(dir);
    if (!d) return false;

    out_name = str2wcstring(d->d_name);
    return true;
}


wchar_t *wgetcwd(wchar_t *buff, size_t sz)
{
    char *buffc = (char *)malloc(sz*MAX_UTF8_BYTES);
    char *res;
    wchar_t *ret = 0;

    if (!buffc)
    {
        errno = ENOMEM;
        return 0;
    }

    res = getcwd(buffc, sz*MAX_UTF8_BYTES);
    if (res)
    {
        if ((size_t)-1 != mbstowcs(buff, buffc, sizeof(wchar_t) * sz))
        {
            ret = buff;
        }
    }

    free(buffc);

    return ret;
}

int wchdir(const wcstring &dir)
{
    cstring tmp = wcs2string(dir);
    return chdir(tmp.c_str());
}

FILE *wfopen(const wcstring &path, const char *mode)
{
    int permissions = 0, options = 0;
    size_t idx = 0;
    switch (mode[idx++])
    {
        case 'r':
            permissions = O_RDONLY;
            break;
        case 'w':
            permissions = O_WRONLY;
            options = O_CREAT | O_TRUNC;
            break;
        case 'a':
            permissions = O_WRONLY;
            options = O_CREAT | O_APPEND;
            break;
        default:
            errno = EINVAL;
            return NULL;
            break;
    }
    /* Skip binary */
    if (mode[idx] == 'b')
        idx++;

    /* Consider append option */
    if (mode[idx] == '+')
        permissions = O_RDWR;

    int fd = wopen_cloexec(path, permissions | options, 0666);
    if (fd < 0)
        return NULL;
    FILE *result = fdopen(fd, mode);
    if (result == NULL)
        close(fd);
    return result;
}

FILE *wfreopen(const wcstring &path, const char *mode, FILE *stream)
{
    cstring tmp = wcs2string(path);
    return freopen(tmp.c_str(), mode, stream);
}

bool set_cloexec(int fd)
{
    int flags = fcntl(fd, F_GETFD, 0);
    if (flags < 0)
    {
        return false;
    }
    else if (flags & FD_CLOEXEC)
    {
        return true;
    }
    else
    {
        return fcntl(fd, F_SETFD, flags | FD_CLOEXEC) >= 0;
    }
}

static int wopen_internal(const wcstring &pathname, int flags, mode_t mode, bool cloexec)
{
    ASSERT_IS_NOT_FORKED_CHILD();
    cstring tmp = wcs2string(pathname);
    /* Prefer to use O_CLOEXEC. It has to both be defined and nonzero */
#ifdef O_CLOEXEC
    if (cloexec && O_CLOEXEC)
    {
        flags |= O_CLOEXEC;
        cloexec = false;
    }
#endif
    int fd = ::open(tmp.c_str(), flags, mode);
    if (cloexec && fd >= 0 && ! set_cloexec(fd))
    {
        close(fd);
        fd = -1;
    }
    return fd;

}
int wopen(const wcstring &pathname, int flags, mode_t mode)
{
    // off the main thread, always use wopen_cloexec
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_NOT_FORKED_CHILD();
    return wopen_internal(pathname, flags, mode, false);
}

int wopen_cloexec(const wcstring &pathname, int flags, mode_t mode)
{
    return wopen_internal(pathname, flags, mode, true);
}


int wcreat(const wcstring &pathname, mode_t mode)
{
    cstring tmp = wcs2string(pathname);
    return creat(tmp.c_str(), mode);
}

DIR *wopendir(const wcstring &name)
{
    cstring tmp = wcs2string(name);
    return opendir(tmp.c_str());
}

int wstat(const wcstring &file_name, struct stat *buf)
{
    cstring tmp = wcs2string(file_name);
    return stat(tmp.c_str(), buf);
}

int lwstat(const wcstring &file_name, struct stat *buf)
{
    cstring tmp = wcs2string(file_name);
    return lstat(tmp.c_str(), buf);
}

int waccess(const wcstring &file_name, int mode)
{
    cstring tmp = wcs2string(file_name);
    return access(tmp.c_str(), mode);
}

int wunlink(const wcstring &file_name)
{
    cstring tmp = wcs2string(file_name);
    return unlink(tmp.c_str());
}

void wperror(const wcstring &s)
{
    int e = errno;
    if (!s.empty())
    {
        fwprintf(stderr, L"%ls: ", s.c_str());
    }
    fwprintf(stderr, L"%s\n", strerror(e));
}

int make_fd_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    int err = 0;
    if (!(flags & O_NONBLOCK))
    {
        err = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    return err == -1 ? errno : 0;
}

int make_fd_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    int err = 0;
    if (flags & O_NONBLOCK)
    {
        err = fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }
    return err == -1 ? errno : 0;
}

static inline void safe_append(char *buffer, const char *s, size_t buffsize)
{
    strncat(buffer, s, buffsize - strlen(buffer) - 1);
}

const char *safe_strerror(int err)
{
#if defined(__UCLIBC__)
    // uClibc does not have sys_errlist, however, its strerror is believed to be async-safe
    // See #808
    return strerror(err);
#else
    if (err >= 0 && err < sys_nerr && sys_errlist[err] != NULL)
    {
        return sys_errlist[err];
    }
    else
    {
        int saved_err = errno;

        /* Use a shared buffer for this case */
        static char buff[384];
        char errnum_buff[64];
        format_long_safe(errnum_buff, err);

        buff[0] = '\0';
        safe_append(buff, "unknown error (errno was ", sizeof buff);
        safe_append(buff, errnum_buff, sizeof buff);
        safe_append(buff, ")", sizeof buff);

        errno = saved_err;
        return buff;
    }
#endif
}

void safe_perror(const char *message)
{
    // Note we cannot use strerror, because on Linux it uses gettext, which is not safe
    int err = errno;

    char buff[384];
    buff[0] = '\0';

    if (message)
    {
        safe_append(buff, message, sizeof buff);
        safe_append(buff, ": ", sizeof buff);
    }
    safe_append(buff, safe_strerror(err), sizeof buff);
    safe_append(buff, "\n", sizeof buff);

    write(STDERR_FILENO, buff, strlen(buff));
    errno = err;
}

#ifdef HAVE_REALPATH_NULL

wchar_t *wrealpath(const wcstring &pathname, wchar_t *resolved_path)
{
    cstring narrow_path = wcs2string(pathname);
    char *narrow_res = realpath(narrow_path.c_str(), NULL);

    if (!narrow_res)
        return NULL;

    wchar_t *res;
    wcstring wide_res = str2wcstring(narrow_res);
    if (resolved_path)
    {
        wcslcpy(resolved_path, wide_res.c_str(), PATH_MAX);
        res = resolved_path;
    }
    else
    {
        res = wcsdup(wide_res.c_str());
    }

    free(narrow_res);

    return res;
}

#else

wchar_t *wrealpath(const wcstring &pathname, wchar_t *resolved_path)
{
    cstring tmp = wcs2string(pathname);
    char narrow_buff[PATH_MAX];
    char *narrow_res = realpath(tmp.c_str(), narrow_buff);
    wchar_t *res;

    if (!narrow_res)
        return 0;

    const wcstring wide_res = str2wcstring(narrow_res);
    if (resolved_path)
    {
        wcslcpy(resolved_path, wide_res.c_str(), PATH_MAX);
        res = resolved_path;
    }
    else
    {
        res = wcsdup(wide_res.c_str());
    }
    return res;
}

#endif


wcstring wdirname(const wcstring &path)
{
    char *tmp = wcs2str(path.c_str());
    char *narrow_res = dirname(tmp);
    wcstring result = format_string(L"%s", narrow_res);
    free(tmp);
    return result;
}

wcstring wbasename(const wcstring &path)
{
    char *tmp = wcs2str(path.c_str());
    char *narrow_res = basename(tmp);
    wcstring result = format_string(L"%s", narrow_res);
    free(tmp);
    return result;
}

/* Really init wgettext */
static void wgettext_really_init()
{
    pthread_mutex_init(&wgettext_lock, NULL);
    fish_bindtextdomain(PACKAGE_NAME, LOCALEDIR);
    fish_textdomain(PACKAGE_NAME);
}

/**
   For wgettext: Internal init function. Automatically called when a translation is first requested.
*/
static void wgettext_init_if_necessary()
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, wgettext_really_init);
}

const wchar_t *wgettext(const wchar_t *in)
{
    if (!in)
        return in;

    // preserve errno across this since this is often used in printing error messages
    int err = errno;

    wgettext_init_if_necessary();

    wcstring key = in;
    scoped_lock lock(wgettext_lock);

    wcstring *& val = wgettext_map[key];
    if (val == NULL)
    {
        cstring mbs_in = wcs2string(key);
        char *out = fish_gettext(mbs_in.c_str());
        val = new wcstring(format_string(L"%s", out)); //note that this writes into the map!
    }
    errno = err;
    return val->c_str();
}

const wchar_t *wgetenv(const wcstring &name)
{
    ASSERT_IS_MAIN_THREAD();
    cstring name_narrow = wcs2string(name);
    char *res_narrow = getenv(name_narrow.c_str());
    static wcstring out;

    if (!res_narrow)
        return 0;

    out = format_string(L"%s", res_narrow);
    return out.c_str();

}

int wmkdir(const wcstring &name, int mode)
{
    cstring name_narrow = wcs2string(name);
    return mkdir(name_narrow.c_str(), mode);
}

int wrename(const wcstring &old, const wcstring &newv)
{
    cstring old_narrow = wcs2string(old);
    cstring new_narrow =wcs2string(newv);
    return rename(old_narrow.c_str(), new_narrow.c_str());
}

int fish_wcstoi(const wchar_t *str, wchar_t ** endptr, int base)
{
    long ret = wcstol(str, endptr, base);
    if (ret > INT_MAX)
    {
        ret = INT_MAX;
        errno = ERANGE;
    }
    else if (ret < INT_MIN)
    {
        ret = INT_MIN;
        errno = ERANGE;
    }
    return (int)ret;
}
