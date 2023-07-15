// Prototypes for wide character equivalents of various standard unix functions.
#ifndef FISH_WUTIL_H
#define FISH_WUTIL_H

#include "config.h"  // IWYU pragma: keep

#include <dirent.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef __APPLE__
// This include is required on macOS 10.10 for locale_t
#include <xlocale.h>  // IWYU pragma: keep
#endif

#include <ctime>
#include <functional>
#include <limits>
#include <locale>
#include <memory>
#include <string>

#include "common.h"
#include "maybe.h"

/// A POD wrapper around a null-terminated string, for ffi purposes.
/// This trivial type may be converted to and from const wchar_t *.
struct wcharz_t {
    const wchar_t *str;

    /* implicit */ wcharz_t(const wchar_t *s) : str(s) { assert(s && "wcharz_t must be non-null"); }
    operator const wchar_t *() const { return str; }
    operator wcstring() const { return str; }

    inline size_t size() const { return wcslen(str); }
    inline size_t length() const { return size(); }
};

// A helper type for passing vectors of strings back to Rust.
// This hides the vector so that autocxx doesn't complain about templates.
struct wcstring_list_ffi_t {
    std::vector<wcstring> vals{};

    wcstring_list_ffi_t() = default;
    /* implicit */ wcstring_list_ffi_t(std::vector<wcstring> vals) : vals(std::move(vals)) {}
    ~wcstring_list_ffi_t();

    bool empty() const { return vals.empty(); }
    size_t size() const { return vals.size(); }
    const wcstring &at(size_t idx) const { return vals.at(idx); }
    void clear() { vals.clear(); }

    /// Helper to construct one.
    static std::unique_ptr<wcstring_list_ffi_t> create() {
        return std::unique_ptr<wcstring_list_ffi_t>(new wcstring_list_ffi_t());
    }

    /// Append a string.
    void push(wcstring s) { vals.push_back(std::move(s)); }

    /// Helper functions used in tests only.
    static wcstring_list_ffi_t get_test_data();
    static void check_test_data(wcstring_list_ffi_t data);
};

/// Convert an iterable of strings to a list of wcharz_t.
template <typename T>
std::vector<wcharz_t> wcstring_list_to_ffi(const T &list) {
    std::vector<wcharz_t> result;
    for (const wcstring &str : list) {
        result.push_back(str.c_str());
    }
    return result;
}

class autoclose_fd_t;

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
void wperror(wcharz_t s);

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

/// Wide character version of dirname().
std::wstring wdirname(std::wstring path);

/// Wide character version of basename().
std::wstring wbasename(std::wstring path);

/// Wide character wrapper around the gettext function. For historic reasons, unlike the real
/// gettext function, wgettext takes care of setting the correct domain, etc. using the textdomain
/// and bindtextdomain functions. This should probably be moved out of wgettext, so that wgettext
/// will be nothing more than a wrapper around gettext, like all other functions in this file.
const wcstring &wgettext(const wchar_t *in);
const wchar_t *wgettext_ptr(const wchar_t *in);

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

// We need this because there are too many implementations that don't return the proper answer for
// some code points. See issue #3050.
#ifndef FISH_NO_ISW_WRAPPERS
#define iswalnum fish_iswalnum
#endif
int fish_iswalnum(wint_t wc);

int fish_wcswidth(const wchar_t *str);
int fish_wcswidth(const wcstring &str);

int fish_wcstoi(const wchar_t *str, const wchar_t **endptr = nullptr, int base = 10);
long fish_wcstol(const wchar_t *str, const wchar_t **endptr = nullptr, int base = 10);
double fish_wcstod(const wchar_t *str, wchar_t **endptr, size_t len);
double fish_wcstod(const wchar_t *str, wchar_t **endptr);
double fish_wcstod(const wcstring &str, wchar_t **endptr);

/// Class for representing a file's inode. We use this to detect and avoid symlink loops, among
/// other things. While an inode / dev pair is sufficient to distinguish co-existing files, Linux
/// seems to aggressively re-use inodes, so it cannot determine if a file has been deleted (ABA
/// problem). Therefore we include richer information.
struct file_id_t {
    dev_t device{static_cast<dev_t>(-1LL)};
    ino_t inode{static_cast<ino_t>(-1LL)};
    uint64_t size{static_cast<uint64_t>(-1LL)};
    time_t change_seconds{std::numeric_limits<time_t>::min()};
    long change_nanoseconds{-1};
    time_t mod_seconds{std::numeric_limits<time_t>::min()};
    long mod_nanoseconds{-1};

    constexpr file_id_t() = default;

    bool operator==(const file_id_t &rhs) const;
    bool operator!=(const file_id_t &rhs) const;

    // Used to permit these as keys in std::map.
    bool operator<(const file_id_t &rhs) const;

    static file_id_t from_stat(const struct stat &buf);
    wcstring dump() const;

   private:
    int compare_file_id(const file_id_t &rhs) const;
};

/// Types of files that may be in a directory.
enum class dir_entry_type_t : uint8_t {
    fifo = 1,  // FIFO file
    chr,       // character device
    dir,       // directory
    blk,       // block device
    reg,       // regular file
    lnk,       // symlink
    sock,      // socket
    whiteout,  // whiteout (from BSD)
};

/// Class for iterating over a directory, wrapping readdir().
/// This allows enumerating the contents of a directory, exposing the file type if the filesystem
/// itself exposes that from readdir(). stat() is incurred only if necessary: if the entry is a
/// symlink, or if the caller asks for the stat buffer.
/// Symlinks are followed.
class dir_iter_t : noncopyable_t {
   private:
    /// Whether this dir_iter considers the "." and ".." filesystem entries.
    bool withdot_{false};

   public:
    struct entry_t;

    /// Open a directory at a given path. On failure, \p error() will return the error code.
    /// Note opendir is guaranteed to set close-on-exec by POSIX (hooray).
    explicit dir_iter_t(const wcstring &path, bool withdot = false);

    /// Advance this iterator.
    /// \return a pointer to the entry, or nullptr if the entry is finished, or an error occurred.
    /// The returned pointer is only valid until the next call to next().
    const entry_t *next();

    /// \return the errno value for the last error, or 0 if none.
    int error() const { return error_; }

    /// \return if we are valid: successfully opened a directory.
    bool valid() const { return dir_ != nullptr; }

    /// \return the underlying file descriptor, or -1 if invalid.
    int fd() const { return dir_ ? dirfd(&*dir_) : -1; }

    /// Rewind the directory to the beginning.
    void rewind();

    ~dir_iter_t();
    dir_iter_t(dir_iter_t &&);
    dir_iter_t &operator=(dir_iter_t &&);

    /// An entry returned by dir_iter_t.
    struct entry_t : noncopyable_t {
        /// File name of this entry.
        wcstring name{};

        /// inode of this entry.
        ino_t inode{};

        /// \return the type of this entry if it is already available, otherwise none().
        maybe_t<dir_entry_type_t> fast_type() const { return type_; }

        /// \return the type of this entry, falling back to stat() if necessary.
        /// If stat() fails because the file has disappeared, this will return none().
        /// If stat() fails because of a broken symlink, this will return type lnk.
        maybe_t<dir_entry_type_t> check_type() const;

        /// \return whether this is a directory. This may call stat().
        bool is_dir() const { return check_type() == dir_entry_type_t::dir; }

        /// \return the stat buff for this entry, invoking stat() if necessary.
        const maybe_t<struct stat> &stat() const;

       private:
        // Reset our fields.
        void reset();

        // Populate our stat buffer, and type. Errors are silently ignored.
        void do_stat() const;

        // Stat buff for this entry, or none if not yet computed.
        mutable maybe_t<struct stat> stat_{};

        // The type of the entry. This is initially none; it may be populated eagerly via readdir()
        // on some filesystems, or later via stat(). If stat() fails, the error is silently ignored
        // and the type is left as none(). Note this is an unavoidable race.
        mutable maybe_t<dir_entry_type_t> type_{};

        // fd of the DIR*, used for fstatat().
        int dirfd_{-1};

        entry_t();
        ~entry_t();
        entry_t(entry_t &&) = default;
        entry_t &operator=(entry_t &&) = default;
        friend class dir_iter_t;
    };

   private:
    struct dir_closer_t {
        void operator()(DIR *dir) const { (void)closedir(dir); }
    };
    std::unique_ptr<DIR, dir_closer_t> dir_{nullptr};
    int error_{0};
    entry_t entry_;
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
file_id_t file_id_for_fd(const autoclose_fd_t &fd);
file_id_t file_id_for_path(const wcstring &path);
file_id_t file_id_for_path(const std::string &path);

extern const file_id_t kInvalidFileID;

#endif
