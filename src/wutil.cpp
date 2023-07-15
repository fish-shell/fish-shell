// Wide character equivalents of various standard unix functions.
#define FISH_NO_ISW_WRAPPERS
#include "config.h"

#include "wutil.h"  // IWYU pragma: keep

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wctype.h>

#ifdef HAVE_XLOCALE_H
#include <xlocale.h>
#endif

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <locale>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fds.h"
#include "flog.h"
#include "wcstringutil.h"

using cstring = std::string;

const file_id_t kInvalidFileID{};

wcstring_list_ffi_t::~wcstring_list_ffi_t() = default;

/// Map used as cache by wgettext.
static owning_lock<std::unordered_map<wcstring, wcstring>> wgettext_map;

wcstring wgetcwd() {
    char cwd[PATH_MAX];
    char *res = getcwd(cwd, sizeof(cwd));
    if (res) {
        return str2wcstring(res);
    }

    FLOGF(error, _(L"getcwd() failed with errno %d/%s"), errno, std::strerror(errno));
    return wcstring();
}

DIR *wopendir(const wcstring &name) {
    const cstring tmp = wcs2zstring(name);
    return opendir(tmp.c_str());
}

#ifdef HAVE_STRUCT_DIRENT_D_TYPE
static maybe_t<dir_entry_type_t> dirent_type_to_entry_type(uint8_t dt) {
    switch (dt) {
        case DT_FIFO:
            return dir_entry_type_t::fifo;
        case DT_CHR:
            return dir_entry_type_t::chr;
        case DT_DIR:
            return dir_entry_type_t::dir;
        case DT_BLK:
            return dir_entry_type_t::blk;
        case DT_REG:
            return dir_entry_type_t::reg;
        case DT_LNK:
            return dir_entry_type_t::lnk;
        case DT_SOCK:
            return dir_entry_type_t::sock;
#if defined(DT_WHT)
        // OpenBSD doesn't have this one
        case DT_WHT:
            return dir_entry_type_t::whiteout;
#endif
        case DT_UNKNOWN:
        default:
            return none();
    }
}
#endif

static maybe_t<dir_entry_type_t> stat_mode_to_entry_type(mode_t m) {
    switch (m & S_IFMT) {
        case S_IFIFO:
            return dir_entry_type_t::fifo;
        case S_IFCHR:
            return dir_entry_type_t::chr;
        case S_IFDIR:
            return dir_entry_type_t::dir;
        case S_IFBLK:
            return dir_entry_type_t::blk;
        case S_IFREG:
            return dir_entry_type_t::reg;
        case S_IFLNK:
            return dir_entry_type_t::lnk;
        case S_IFSOCK:
            return dir_entry_type_t::sock;
#if defined(S_IFWHT)
        case S_IFWHT:
            return dir_entry_type_t::whiteout;
#endif
        default:
            return none();
    }
}

dir_iter_t::entry_t::entry_t() = default;
dir_iter_t::entry_t::~entry_t() = default;

void dir_iter_t::entry_t::reset() {
    this->name.clear();
    this->inode = {};
    this->type_.reset();
    this->stat_.reset();
}

maybe_t<dir_entry_type_t> dir_iter_t::entry_t::check_type() const {
    // Call stat if needed to populate our type, swallowing errors.
    if (!this->type_) {
        this->do_stat();
    }
    return this->type_;
}

const maybe_t<struct stat> &dir_iter_t::entry_t::stat() const {
    if (!stat_) {
        (void)this->do_stat();
    }
    return stat_;
}

void dir_iter_t::entry_t::do_stat() const {
    // We want to set both our type and our stat buffer.
    // If we follow symlinks and stat() errors with a bad symlink, set the type to link, but do not
    // populate the stat buffer.
    if (this->dirfd_ < 0) {
        return;
    }
    std::string narrow = wcs2zstring(this->name);
    struct stat s {};
    if (fstatat(this->dirfd_, narrow.c_str(), &s, 0) == 0) {
        this->stat_ = s;
        this->type_ = stat_mode_to_entry_type(s.st_mode);
    } else {
        switch (errno) {
            case ELOOP:
                this->type_ = dir_entry_type_t::lnk;
                break;

            case EACCES:
            case EIO:
            case ENOENT:
            case ENOTDIR:
            case ENAMETOOLONG:
            case ENODEV:
                // These are "expected" errors.
                this->type_ = none();
                break;

            default:
                this->type_ = none();
                // This used to print an error, but given that we have seen
                // both ENODEV (above) and ENOTCONN,
                // and that the error isn't actionable and shows up while typing,
                // let's not do that.
                // wperror(L"fstatat");
                break;
        }
    }
}

dir_iter_t::dir_iter_t(const wcstring &path, bool withdot) {
    dir_.reset(wopendir(path));
    if (!dir_) {
        error_ = errno;
        return;
    }
    withdot_ = withdot;
    entry_.dirfd_ = dirfd(&*dir_);
}

dir_iter_t::dir_iter_t(dir_iter_t &&rhs) { *this = std::move(rhs); }

dir_iter_t &dir_iter_t::operator=(dir_iter_t &&rhs) {
    // Steal the fields; ensure rhs no longer has DIR* and forgets its fd.
    this->dir_ = std::move(rhs.dir_);
    this->error_ = rhs.error_;
    this->entry_ = std::move(rhs.entry_);
    rhs.dir_ = nullptr;
    rhs.entry_.dirfd_ = -1;
    return *this;
}

void dir_iter_t::rewind() {
    if (dir_) {
        rewinddir(&*dir_);
    }
}

dir_iter_t::~dir_iter_t() = default;

const dir_iter_t::entry_t *dir_iter_t::next() {
    if (!dir_) {
        return nullptr;
    }
    errno = 0;
    struct dirent *dent = readdir(&*dir_);
    if (!dent) {
        error_ = errno;
        return nullptr;
    }
    // Skip . and ..,
    // unless we've been told not to.
    if (!withdot_ && (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, ".."))) {
        return next();
    }
    entry_.reset();
    entry_.name = str2wcstring(dent->d_name);
    entry_.inode = dent->d_ino;
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
    auto type = dirent_type_to_entry_type(dent->d_type);
    // Do not store symlinks as we will need to resolve them.
    if (type != dir_entry_type_t::lnk) {
        entry_.type_ = type;
    }
#endif
    return &entry_;
}

int wstat(const wcstring &file_name, struct stat *buf) {
    const cstring tmp = wcs2zstring(file_name);
    return stat(tmp.c_str(), buf);
}

int lwstat(const wcstring &file_name, struct stat *buf) {
    const cstring tmp = wcs2zstring(file_name);
    return lstat(tmp.c_str(), buf);
}

int waccess(const wcstring &file_name, int mode) {
    const cstring tmp = wcs2zstring(file_name);
    return access(tmp.c_str(), mode);
}

int wunlink(const wcstring &file_name) {
    const cstring tmp = wcs2zstring(file_name);
    return unlink(tmp.c_str());
}

void wperror(wcharz_t s) {
    int e = errno;
    if (s.str[0] != L'\0') {
        std::fwprintf(stderr, L"%ls: ", s.str);
    }
    std::fwprintf(stderr, L"%s\n", std::strerror(e));
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

/// Wide character realpath. The last path component does not need to be valid. If an error occurs,
/// wrealpath() returns none() and errno is likely set.
maybe_t<wcstring> wrealpath(const wcstring &pathname) {
    if (pathname.empty()) return none();

    cstring real_path;
    cstring narrow_path = wcs2zstring(pathname);

    // Strip trailing slashes. This is treats "/a//" as equivalent to "/a" if /a is a non-directory.
    while (narrow_path.size() > 1 && narrow_path.at(narrow_path.size() - 1) == '/') {
        narrow_path.erase(narrow_path.size() - 1, 1);
    }

    char tmpbuf[PATH_MAX];
    char *narrow_res = realpath(narrow_path.c_str(), tmpbuf);

    if (narrow_res) {
        real_path.append(narrow_res);
    } else {
        // Check if everything up to the last path component is valid.
        size_t pathsep_idx = narrow_path.rfind('/');

        if (pathsep_idx == 0) {
            // If the only pathsep is the first character then it's an absolute path with a
            // single path component and thus doesn't need conversion.
            real_path = narrow_path;
        } else {
            // Only call realpath() on the portion up to the last component.
            errno = 0;

            if (pathsep_idx == cstring::npos) {
                // If there is no "/", this is a file in $PWD, so give the realpath to that.
                narrow_res = realpath(".", tmpbuf);
            } else {
                errno = 0;
                // Only call realpath() on the portion up to the last component.
                narrow_res = realpath(narrow_path.substr(0, pathsep_idx).c_str(), tmpbuf);
            }

            if (!narrow_res) return none();

            pathsep_idx++;
            real_path.append(narrow_res);

            // This test is to deal with cases such as /../../x => //x.
            if (real_path.size() > 1) real_path.append("/");

            real_path.append(narrow_path.substr(pathsep_idx, cstring::npos));
        }
    }
    return str2wcstring(real_path);
}

wcstring normalize_path(const wcstring &path, bool allow_leading_double_slashes) {
    // Count the leading slashes.
    const wchar_t sep = L'/';
    size_t leading_slashes = 0;
    for (wchar_t c : path) {
        if (c != sep) break;
        leading_slashes++;
    }

    std::vector<wcstring> comps = split_string(path, sep);
    std::vector<wcstring> new_comps;
    for (wcstring &comp : comps) {
        if (comp.empty() || comp == L".") {
            continue;
        } else if (comp != L"..") {
            new_comps.push_back(std::move(comp));
        } else if (!new_comps.empty() && new_comps.back() != L"..") {
            // '..' with a real path component, drop that path component.
            new_comps.pop_back();
        } else if (leading_slashes == 0) {
            // We underflowed the .. and are a relative (not absolute) path.
            new_comps.push_back(L"..");
        }
    }
    wcstring result = join_strings(new_comps, sep);
    // If we don't allow leading double slashes, collapse them to 1 if there are any.
    int numslashes = leading_slashes > 0 ? 1 : 0;
    // If we do, prepend one or two leading slashes.
    // Yes, three+ slashes are collapsed to one. (!)
    if (allow_leading_double_slashes && leading_slashes == 2) numslashes = 2;
    result.insert(0, numslashes, sep);
    // Ensure ./ normalizes to . and not empty.
    if (result.empty()) result.push_back(L'.');
    return result;
}

wcstring path_normalize_for_cd(const wcstring &wd, const wcstring &path) {
    // Fast paths.
    const wchar_t sep = L'/';
    assert(!wd.empty() && wd.front() == sep && wd.back() == sep &&
           "Invalid working directory, it must start and end with /");
    if (path.empty()) {
        return wd;
    } else if (path.front() == sep) {
        return path;
    } else if (path.front() != L'.') {
        return wd + path;
    }

    // Split our strings by the sep.
    std::vector<wcstring> wd_comps = split_string(wd, sep);
    std::vector<wcstring> path_comps = split_string(path, sep);

    // Remove empty segments from wd_comps.
    // In particular this removes the leading and trailing empties.
    wd_comps.erase(std::remove(wd_comps.begin(), wd_comps.end(), L""), wd_comps.end());

    // Erase leading . and .. components from path_comps, popping from wd_comps as we go.
    size_t erase_count = 0;
    for (const wcstring &comp : path_comps) {
        bool erase_it = false;
        if (comp.empty() || comp == L".") {
            erase_it = true;
        } else if (comp == L".." && !wd_comps.empty()) {
            erase_it = true;
            wd_comps.pop_back();
        }
        if (erase_it) {
            erase_count++;
        } else {
            break;
        }
    }
    // Append un-erased elements to wd_comps and join them, then prepend the leading /.
    std::move(path_comps.begin() + erase_count, path_comps.end(), std::back_inserter(wd_comps));
    wcstring result = join_strings(wd_comps, sep);
    result.insert(0, 1, L'/');
    return result;
}

wcstring wdirname(wcstring path) {
    // Do not use system-provided dirname (#7837).
    // On Mac it's not thread safe, and will error for paths exceeding PATH_MAX.
    // This follows OpenGroup dirname recipe.
    // 1: Double-slash stays.
    if (path == L"//") return path;

    // 2: All slashes => return slash.
    if (!path.empty() && path.find_first_not_of(L'/') == wcstring::npos) return L"/";

    // 3: Trim trailing slashes.
    while (!path.empty() && path.back() == L'/') path.pop_back();

    // 4: No slashes left => return period.
    size_t last_slash = path.rfind(L'/');
    if (last_slash == wcstring::npos) return L".";

    // 5: Remove trailing non-slashes.
    path.erase(last_slash + 1, wcstring::npos);

    // 6: Skip as permitted.
    // 7: Remove trailing slashes again.
    while (!path.empty() && path.back() == L'/') path.pop_back();

    // 8: Empty => return slash.
    if (path.empty()) path = L"/";
    return path;
}

wcstring wbasename(wcstring path) {
    // This follows OpenGroup basename recipe.
    // 1: empty => allowed to return ".". This is what system impls do.
    if (path.empty()) return L".";

    // 2: Skip as permitted.
    // 3: All slashes => return slash.
    if (!path.empty() && path.find_first_not_of(L'/') == wcstring::npos) return L"/";

    // 4: Remove trailing slashes.
    while (!path.empty() && path.back() == L'/') path.pop_back();

    // 5: Remove up to and including last slash.
    size_t last_slash = path.rfind(L'/');
    if (last_slash != wcstring::npos) path.erase(0, last_slash + 1);
    return path;
}

// Really init wgettext.
static void wgettext_really_init() {
    fish_bindtextdomain(PACKAGE_NAME, LOCALEDIR);
    fish_textdomain(PACKAGE_NAME);
}

/// For wgettext: Internal init function. Automatically called when a translation is first
/// requested.
static void wgettext_init_if_necessary() {
    static std::once_flag s_wgettext_init{};
    std::call_once(s_wgettext_init, wgettext_really_init);
}

const wcstring &wgettext(const wchar_t *in) {
    // Preserve errno across this since this is often used in printing error messages.
    int err = errno;
    wcstring key = in;

    wgettext_init_if_necessary();
    auto wmap = wgettext_map.acquire();
    wcstring &val = (*wmap)[key];
    if (val.empty()) {
        cstring mbs_in = wcs2zstring(key);
        char *out = fish_gettext(mbs_in.c_str());
        val = format_string(L"%s", out);
    }
    errno = err;

    // The returned string is stored in the map.
    // TODO: If we want to shrink the map, this would be a problem.
    return val;
}

const wchar_t *wgettext_ptr(const wchar_t *in) { return wgettext(in).c_str(); }

int wmkdir(const wcstring &name, int mode) {
    cstring name_narrow = wcs2zstring(name);
    return mkdir(name_narrow.c_str(), mode);
}

int wrename(const wcstring &old, const wcstring &newv) {
    cstring old_narrow = wcs2zstring(old);
    cstring new_narrow = wcs2zstring(newv);
    return rename(old_narrow.c_str(), new_narrow.c_str());
}

ssize_t wwrite_to_fd(const wchar_t *input, size_t input_len, int fd) {
    // Accumulate data in a local buffer.
    char accum[512];
    size_t accumlen{0};
    constexpr size_t maxaccum = sizeof accum / sizeof *accum;

    // Helper to perform a write to 'fd', looping as necessary.
    // \return true on success, false on error.
    ssize_t total_written = 0;
    auto do_write = [fd, &total_written](const char *cursor, size_t remaining) {
        while (remaining > 0) {
            ssize_t samt = write(fd, cursor, remaining);
            if (samt < 0) return false;
            total_written += samt;
            auto amt = static_cast<size_t>(samt);
            assert(amt <= remaining && "Wrote more than requested");
            remaining -= amt;
            cursor += amt;
        }
        return true;
    };

    // Helper to flush the accumulation buffer.
    auto flush_accum = [&] {
        if (!do_write(accum, accumlen)) return false;
        accumlen = 0;
        return true;
    };

    bool success = wcs2string_callback(input, input_len, [&](const char *buff, size_t len) {
        if (len + accumlen > maxaccum) {
            // We have to flush.
            // Note this modifies 'accumlen'.
            if (!flush_accum()) return false;
        }
        if (len + accumlen <= maxaccum) {
            // Accumulate more.
            memmove(accum + accumlen, buff, len);
            accumlen += len;
            return true;
        } else {
            // Too much data to even fit, just write it immediately.
            return do_write(buff, len);
        }
    });
    // Flush any remaining.
    if (success) success = flush_accum();
    return success ? total_written : -1;
}

enum : wint_t {
    PUA1_START = 0xE000,
    PUA1_END = 0xF900,
    PUA2_START = 0xF0000,
    PUA2_END = 0xFFFFE,
    PUA3_START = 0x100000,
    PUA3_END = 0x10FFFE,
};

/// Return one if the code point is in a Unicode private use area.
static int fish_is_pua(wint_t wc) {
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

/// Convenience variants on fish_wcwswidth().
///
/// See fallback.h for the normal definitions.
int fish_wcswidth(const wchar_t *str) { return fish_wcswidth(str, std::wcslen(str)); }

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
    long result = std::wcstol(str, &_endptr, base);
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
    return static_cast<int>(result);
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
    long result = std::wcstol(str, &_endptr, base);
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

/// Like wcstod(), but wcstod() is enormously expensive on some platforms so this tries to have a
/// fast path.
double fish_wcstod(const wchar_t *str, wchar_t **endptr, size_t len) {
    // We can ignore the locale because we use LC_NUMERIC=C!
    // The "fast path." If we're all ASCII and we fit inline, use strtod().
    char narrow[128];
    size_t len_plus_0 = 1 + len;
    auto is_ascii = [](wchar_t c) { return 0 <= c && c <= 127; };
    if (len_plus_0 <= sizeof narrow && std::all_of(str, str + len, is_ascii)) {
        // Fast path. Copy the string into a local buffer and run strtod() on it.
        std::copy(str, str + len_plus_0, narrow);
        char *narrow_endptr = nullptr;
        double ret = strtod(narrow, endptr ? &narrow_endptr : nullptr);
        if (endptr) {
            assert(narrow_endptr && "narrow_endptr should not be null");
            *endptr = const_cast<wchar_t *>(str + (narrow_endptr - narrow));
        }
        return ret;
    }
    return std::wcstod(str, endptr);
}

double fish_wcstod(const wchar_t *str, wchar_t **endptr) {
    return fish_wcstod(str, endptr, std::wcslen(str));
}
double fish_wcstod(const wcstring &str, wchar_t **endptr) {
    return fish_wcstod(str.c_str(), endptr, str.size());
}

file_id_t file_id_t::from_stat(const struct stat &buf) {
    file_id_t result = {};
    result.device = buf.st_dev;
    result.inode = buf.st_ino;
    result.size = buf.st_size;
    result.change_seconds = buf.st_ctime;
    result.mod_seconds = buf.st_mtime;

#ifdef HAVE_STRUCT_STAT_ST_CTIME_NSEC
    result.change_nanoseconds = buf.st_ctime_nsec;
    result.mod_nanoseconds = buf.st_mtime_nsec;
#elif defined(__APPLE__)
    result.change_nanoseconds = buf.st_ctimespec.tv_nsec;
    result.mod_nanoseconds = buf.st_mtimespec.tv_nsec;
#elif defined(_BSD_SOURCE) || defined(_SVID_SOURCE) || defined(_XOPEN_SOURCE)
    result.change_nanoseconds = buf.st_ctim.tv_nsec;
    result.mod_nanoseconds = buf.st_mtim.tv_nsec;
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
        result = file_id_t::from_stat(buf);
    }
    return result;
}

file_id_t file_id_for_fd(const autoclose_fd_t &fd) { return file_id_for_fd(fd.fd()); }

file_id_t file_id_for_path(const wcstring &path) {
    file_id_t result = kInvalidFileID;
    struct stat buf = {};
    if (0 == wstat(path, &buf)) {
        result = file_id_t::from_stat(buf);
    }
    return result;
}

file_id_t file_id_for_path(const std::string &path) {
    file_id_t result = kInvalidFileID;
    struct stat buf = {};
    if (0 == stat(path.c_str(), &buf)) {
        result = file_id_t::from_stat(buf);
    }
    return result;
}

bool file_id_t::operator==(const file_id_t &rhs) const { return this->compare_file_id(rhs) == 0; }

bool file_id_t::operator!=(const file_id_t &rhs) const { return !(*this == rhs); }

wcstring file_id_t::dump() const {
    using llong = long long;
    wcstring result;
    append_format(result, L"     device: %lld\n", llong(device));
    append_format(result, L"      inode: %lld\n", llong(inode));
    append_format(result, L"       size: %lld\n", llong(size));
    append_format(result, L"     change: %lld\n", llong(change_seconds));
    append_format(result, L"change_nano: %lld\n", llong(change_nanoseconds));
    append_format(result, L"        mod: %lld\n", llong(mod_seconds));
    append_format(result, L"   mod_nano: %lld", llong(mod_nanoseconds));
    return result;
}

template <typename T>
static int compare(T a, T b) {
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

// static
wcstring_list_ffi_t wcstring_list_ffi_t::get_test_data() {
    return std::vector<wcstring>{L"foo", L"bar", L"baz"};
}

// static
void wcstring_list_ffi_t::check_test_data(wcstring_list_ffi_t data) {
    assert(data.size() == 3);
    assert(data.at(0) == L"foo");
    assert(data.at(1) == L"bar");
    assert(data.at(2) == L"baz");
}
