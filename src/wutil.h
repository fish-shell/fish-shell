// Prototypes for wide character equivalents of various standard unix functions.
#ifndef FISH_WUTIL_H
#define FISH_WUTIL_H

#include <dirent.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <wctype.h>

#include <string>

#ifdef HAVE_XLOCALE_H
#include <xlocale.h>
#endif

#include "common.h"
#include "maybe.h"

/// Sets CLO_EXEC on a given fd according to the value of \p should_set.
int set_cloexec(int fd, bool should_set = true);

/// Wide character version of open() that also sets the close-on-exec flag (atomically when
/// possible).
int wopen_cloexec(const wcstring &pathname, int flags, mode_t mode = 0);

/// Narrow versions of wopen_cloexec.
int open_cloexec(const std::string &path, int flags, mode_t mode = 0);
int open_cloexec(const char *path, int flags, mode_t mode = 0);

/// Mark an fd as nonblocking; returns errno or 0 on success.
int make_fd_nonblocking(int fd);

/// Mark an fd as blocking; returns errno or 0 on success.
int make_fd_blocking(int fd);

/// Check if an fd is on a remote filesystem (NFS, SMB, CFS)
/// Return 1 if remote, 0 if local, -1 on error or if not implemented on this platform.
int fd_check_is_remote(int fd);

/// Wide character version of opendir(). Note that opendir() is guaranteed to set close-on-exec by
/// POSIX (hooray).
DIR *wopendir(const wcstring &name);

/// Wide character version of stat().
int wstat(const wcstring &file_name, struct stat *buf);

/// Wide character version of lstat().
int lwstat(const wcstring &file_name, struct stat *buf);

/// Wide character version of access().
int waccess(const wcstring &file_name, int mode);

/// Wide character version of unlink().
int wunlink(const wcstring &file_name);

/// Wide character version of perror().
void wperror(const wchar_t *s);

/// Async-safe version of perror().
void safe_perror(const char *message);

/// Async-safe version of std::strerror().
const char *safe_strerror(int err);

/// Wide character version of getcwd().
wcstring wgetcwd();

/// Wide character version of realpath function.
/// \returns the canonicalized path, or none if the path is invalid.
maybe_t<wcstring> wrealpath(const wcstring &pathname);

/// Given an input path, "normalize" it:
/// 1. Collapse multiple /s into a single /, except maybe at the beginning.
/// 2. .. goes up a level.
/// 3. Remove /./ in the middle.
wcstring normalize_path(const wcstring &path, bool allow_leading_double_slashes = true);

/// Given an input path \p path and a working directory \p wd, do a "normalizing join" in a way
/// appropriate for cd. That is, return effectively wd + path while resolving leading ../s from
/// path. The intent here is to allow 'cd' out of a directory which may no longer exist, without
/// allowing 'cd' into a directory that may not exist; see #5341.
wcstring path_normalize_for_cd(const wcstring &wd, const wcstring &path);

/// Wide character version of readdir().
bool wreaddir(DIR *dir, wcstring &out_name);
bool wreaddir_resolving(DIR *dir, const std::wstring &dir_path, wcstring &out_name,
                        bool *out_is_dir);

/// Like wreaddir, but skip items that are known to not be directories. If this requires a stat
/// (i.e. the file is a symlink), then return it. Note that this does not guarantee that everything
/// returned is a directory, it's just an optimization for cases where we would check for
/// directories anyways.
bool wreaddir_for_dirs(DIR *dir, wcstring *out_name);

/// Wide character version of dirname().
std::wstring wdirname(const std::wstring &path);

/// Wide character version of basename().
std::wstring wbasename(const std::wstring &path);

/// Wide character wrapper around the gettext function. For historic reasons, unlike the real
/// gettext function, wgettext takes care of setting the correct domain, etc. using the textdomain
/// and bindtextdomain functions. This should probably be moved out of wgettext, so that wgettext
/// will be nothing more than a wrapper around gettext, like all other functions in this file.
const wcstring &wgettext(const wchar_t *in);

/// Wide character version of mkdir.
int wmkdir(const wcstring &name, int mode);

/// Wide character version of rename.
int wrename(const wcstring &oldName, const wcstring &newv);

/// Write a wide string to a file descriptor. This avoids doing any additional allocation.
/// This does NOT retry on EINTR or EAGAIN, it simply returns.
/// \return -1 on error in which case errno will have been set. In this event, the number of bytes
/// actually written cannot be obtained.
ssize_t wwrite_to_fd(const wchar_t *input, size_t len, int fd);

/// Variant of above that accepts a wcstring.
inline ssize_t wwrite_to_fd(const wcstring &s, int fd) {
    return wwrite_to_fd(s.c_str(), s.size(), fd);
}

#define PUA1_START 0xE000
#define PUA1_END 0xF900
#define PUA2_START 0xF0000
#define PUA2_END 0xFFFFE
#define PUA3_START 0x100000
#define PUA3_END 0x10FFFE

// We need this because there are too many implementations that don't return the proper answer for
// some code points. See issue #3050.
#ifndef FISH_NO_ISW_WRAPPERS
#define iswalnum fish_iswalnum
#define iswgraph fish_iswgraph
#endif
int fish_iswalnum(wint_t wc);
int fish_iswgraph(wint_t wc);

int fish_wcswidth(const wchar_t *str);
int fish_wcswidth(const wcstring &str);

// returns an immortal locale_t corresponding to the C locale.
locale_t fish_c_locale();

int fish_wcstoi(const wchar_t *str, const wchar_t **endptr = nullptr, int base = 10);
long fish_wcstol(const wchar_t *str, const wchar_t **endptr = nullptr, int base = 10);
long long fish_wcstoll(const wchar_t *str, const wchar_t **endptr = nullptr, int base = 10);
unsigned long long fish_wcstoull(const wchar_t *str, const wchar_t **endptr = nullptr,
                                 int base = 10);
double fish_wcstod(const wchar_t *str, wchar_t **endptr);

/// Class for representing a file's inode. We use this to detect and avoid symlink loops, among
/// other things. While an inode / dev pair is sufficient to distinguish co-existing files, Linux
/// seems to aggressively re-use inodes, so it cannot determine if a file has been deleted (ABA
/// problem). Therefore we include richer information.
struct file_id_t {
    dev_t device;
    ino_t inode;
    uint64_t size;
    time_t change_seconds;
    long change_nanoseconds;
    time_t mod_seconds;
    long mod_nanoseconds;

    bool operator==(const file_id_t &rhs) const;
    bool operator!=(const file_id_t &rhs) const;

    // Used to permit these as keys in std::map.
    bool operator<(const file_id_t &rhs) const;

    static file_id_t from_stat(const struct stat &buf);

   private:
    int compare_file_id(const file_id_t &rhs) const;
};

/// RAII wrapper for DIR*
struct dir_t {
    DIR *dir;
    bool valid() const;
    bool read(wcstring &name) const;
    dir_t(const wcstring &path);
    ~dir_t();
};

#ifndef HASH_FILE_ID
#define HASH_FILE_ID 1
namespace std {
template <>
struct hash<file_id_t> {
    size_t operator()(const file_id_t &f) const {
        std::hash<decltype(f.device)> hasher1;
        std::hash<decltype(f.inode)> hasher2;

        return hasher1(f.device) ^ hasher2(f.inode);
    }
};
}  // namespace std
#endif

file_id_t file_id_for_fd(int fd);
file_id_t file_id_for_path(const wcstring &path);
file_id_t file_id_for_path(const std::string &path);

extern const file_id_t kInvalidFileID;

#endif
