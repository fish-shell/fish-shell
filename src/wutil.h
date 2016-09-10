// Prototypes for wide character equivalents of various standard unix functions.
#ifndef FISH_WUTIL_H
#define FISH_WUTIL_H

#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <string>

#include "common.h"

FILE *wfopen(const wcstring &path, const char *mode);

bool set_cloexec(int fd);

int wopen_cloexec(const wcstring &pathname, int flags, mode_t mode = 0);

int make_fd_nonblocking(int fd);

int make_fd_blocking(int fd);

DIR *wopendir(const wcstring &name);

int wstat(const wcstring &file_name, struct stat *buf);

int lwstat(const wcstring &file_name, struct stat *buf);

int waccess(const wcstring &pathname, int mode);

int wunlink(const wcstring &pathname);

void wperror(const wchar_t *s);

void safe_perror(const char *message);

const char *safe_strerror(int err);

const wcstring wgetcwd();

int wchdir(const wcstring &dir);

wchar_t *wrealpath(const wcstring &pathname, wchar_t *resolved_path);

bool wreaddir(DIR *dir, std::wstring &out_name);

bool wreaddir_resolving(DIR *dir, const std::wstring &dir_path, std::wstring &out_name,
                        bool *out_is_dir);

bool wreaddir_for_dirs(DIR *dir, wcstring *out_name);

std::wstring wdirname(const std::wstring &path);

std::wstring wbasename(const std::wstring &path);

const wcstring &wgettext(const wchar_t *in);

int wmkdir(const wcstring &dir, int mode);

int wrename(const wcstring &oldName, const wcstring &newName);

int fish_wcstoi(const wchar_t *str, wchar_t **endptr, int base);

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
