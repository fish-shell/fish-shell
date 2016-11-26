// Prototypes for wide character equivalents of various standard unix functions.
#ifndef FISH_WUTIL_H
#define FISH_WUTIL_H

#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <string>

#include "common.h"

/// Wide character version of fopen(). This sets CLO_EXEC.
FILE *wfopen(const wcstring &path, const char *mode);

/// Sets CLO_EXEC on a given fd.
bool set_cloexec(int fd);

/// Wide character version of open() that also sets the close-on-exec flag (atomically when
/// possible).
int wopen_cloexec(const wcstring &pathname, int flags, mode_t mode = 0);

/// Mark an fd as nonblocking; returns errno or 0 on success.
int make_fd_nonblocking(int fd);

/// Mark an fd as blocking; returns errno or 0 on success.
int make_fd_blocking(int fd);

/// Wide character version of opendir(). Note that opendir() is guaranteed to set close-on-exec by
/// POSIX (hooray).
DIR *wopendir(const wcstring &name);

/// Wide character version of stat().
int wstat(const wcstring &file_name, struct stat *buf);

/// Wide character version of lstat().
int lwstat(const wcstring &file_name, struct stat *buf);

/// Wide character version of access().
int waccess(const wcstring &pathname, int mode);

/// Wide character version of unlink().
int wunlink(const wcstring &pathname);

/// Wide character version of perror().
void wperror(const wchar_t *s);

/// Async-safe version of perror().
void safe_perror(const char *message);

/// Async-safe version of strerror().
const char *safe_strerror(int err);

/// Wide character version of getcwd().
const wcstring wgetcwd();

/// Wide character version of chdir().
int wchdir(const wcstring &dir);

/// Wide character version of realpath function. Just like the GNU version of realpath, wrealpath
/// will accept 0 as the value for the second argument, in which case the result will be allocated
/// using malloc, and must be free'd by the user.
wchar_t *wrealpath(const wcstring &pathname, wchar_t *resolved_path);

/// Wide character version of readdir().
bool wreaddir(DIR *dir, std::wstring &out_name);
bool wreaddir_resolving(DIR *dir, const std::wstring &dir_path, std::wstring &out_name,
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
int wmkdir(const wcstring &dir, int mode);

/// Wide character version of rename.
int wrename(const wcstring &oldName, const wcstring &newName);

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
#define iswalpha fish_iswalpha
#define iswgraph fish_iswgraph
#endif
int fish_iswalnum(wint_t wc);
int fish_iswalpha(wint_t wc);
int fish_iswgraph(wint_t wc);

const wchar_t *wcsvarname(const wchar_t *str);
const wchar_t *wcsvarname(const wcstring &str);
bool wcsfuncname(const wcstring &str);
bool wcsvarchr(wchar_t chr);
int fish_wcswidth(const wchar_t *str);
int fish_wcswidth(const wcstring &str);

int fish_wcstoi(const wchar_t *str, const wchar_t **endptr = NULL, int base = 10);
long fish_wcstol(const wchar_t *str, const wchar_t **endptr = NULL, int base = 10);
long long fish_wcstoll(const wchar_t *str, const wchar_t **endptr = NULL, int base = 10);

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

    static file_id_t file_id_from_stat(const struct stat *buf);

   private:
    int compare_file_id(const file_id_t &rhs) const;
};

file_id_t file_id_for_fd(int fd);
file_id_t file_id_for_path(const wcstring &path);

extern const file_id_t kInvalidFileID;

#endif
