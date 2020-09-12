// Wide character equivalents of various standard unix functions.
#define FISH_NO_ISW_WRAPPERS
#include "config.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <cstring>
#if defined(__linux__)
#include <sys/statfs.h>
#endif
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#include <wctype.h>

#include <atomic>
#include <cwchar>
#include <string>
#include <unordered_map>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "flog.h"
#include "wcstringutil.h"
#include "wutil.h"  // IWYU pragma: keep

using cstring = std::string;

const file_id_t kInvalidFileID = {static_cast<dev_t>(-1LL),
                                  static_cast<ino_t>(-1LL),
                                  static_cast<uint64_t>(-1LL),
                                  -1,
                                  -1,
                                  -1,
                                  -1};

/// Map used as cache by wgettext.
static owning_lock<std::unordered_map<wcstring, wcstring>> wgettext_map;

bool wreaddir_resolving(DIR *dir, const wcstring &dir_path, wcstring &out_name, bool *out_is_dir) {
    struct dirent *result = readdir(dir);
    if (!result) {
        out_name.clear();
        return false;
    }

    out_name = str2wcstring(result->d_name);
    if (!out_is_dir) {
        return true;
    }

    // The caller cares if this is a directory, so check.
    bool is_dir = false;
    // We may be able to skip stat, if the readdir can tell us the file type directly.
    bool check_with_stat = true;
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
    if (result->d_type == DT_DIR) {
        // Known directory.
        is_dir = true;
        check_with_stat = false;
    } else if (result->d_type == DT_LNK || result->d_type == DT_UNKNOWN) {
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
        fullpath.append(result->d_name);
        struct stat buf;
        if (stat(fullpath.c_str(), &buf) != 0) {
            is_dir = false;
        } else {
            is_dir = S_ISDIR(buf.st_mode);
        }
    }
    *out_is_dir = is_dir;
    return true;
}

bool wreaddir(DIR *dir, wcstring &out_name) {
    struct dirent *result = readdir(dir);
    if (!result) {
        out_name.clear();
        return false;
    }

    out_name = str2wcstring(result->d_name);
    return true;
}

bool wreaddir_for_dirs(DIR *dir, wcstring *out_name) {
    struct dirent *result = nullptr;
    while (!result) {
        result = readdir(dir);
        if (!result) break;

#if HAVE_STRUCT_DIRENT_D_TYPE
        switch (result->d_type) {
            case DT_DIR:
            case DT_LNK:
            case DT_UNKNOWN: {
                break;  // these may be directories
            }
            default: {
                break;  // nothing else can
            }
        }
#else
        // We can't determine if it's a directory or not, so just return it.
        break;
#endif
    }

    if (result && out_name) {
        *out_name = str2wcstring(result->d_name);
    }
    return result != nullptr;
}

wcstring wgetcwd() {
    char cwd[PATH_MAX];
    char *res = getcwd(cwd, sizeof(cwd));
    if (res) {
        return str2wcstring(res);
    }

    FLOGF(error, _(L"getcwd() failed with errno %d/%s"), errno, std::strerror(errno));
    return wcstring();
}

int set_cloexec(int fd, bool should_set) {
    // Note we don't want to overwrite existing flags like O_NONBLOCK which may be set. So fetch the
    // existing flags and modify them.
    int flags = fcntl(fd, F_GETFD, 0);
    if (flags < 0) {
        return -1;
    }
    int new_flags = flags;
    if (should_set) {
        new_flags |= FD_CLOEXEC;
    } else {
        new_flags &= ~FD_CLOEXEC;
    }
    if (flags == new_flags) {
        return 0;
    } else {
        return fcntl(fd, F_SETFD, new_flags);
    }
}

int open_cloexec(const std::string &path, int flags, mode_t mode) {
    return open_cloexec(path.c_str(), flags, mode);
}

int open_cloexec(const char *path, int flags, mode_t mode) {
    int fd;

// Prefer to use O_CLOEXEC.
#ifdef O_CLOEXEC
    fd = open(path, flags | O_CLOEXEC, mode);
#else
    fd = open(path, flags, mode);
    if (fd >= 0 && !set_cloexec(fd)) {
        exec_close(fd);
        fd = -1;
    }
#endif
    return fd;
}

int wopen_cloexec(const wcstring &pathname, int flags, mode_t mode) {
    cstring tmp = wcs2string(pathname);
    return open_cloexec(tmp, flags, mode);
}

DIR *wopendir(const wcstring &name) {
    const cstring tmp = wcs2string(name);
    return opendir(tmp.c_str());
}

dir_t::dir_t(const wcstring &path) {
    const cstring tmp = wcs2string(path);
    this->dir = opendir(tmp.c_str());
}

dir_t::~dir_t() {
    if (this->dir != nullptr) {
        closedir(this->dir);
        this->dir = nullptr;
    }
}

bool dir_t::valid() const { return this->dir != nullptr; }

bool dir_t::read(wcstring &name) const { return wreaddir(this->dir, name); }

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
        std::fwprintf(stderr, L"%ls: ", s);
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

int fd_check_is_remote(int fd) {
#if defined(__linux__)
    struct statfs buf {};
    if (fstatfs(fd, &buf) < 0) {
        return -1;
    }
    // Linux has constants for these like NFS_SUPER_MAGIC, SMB_SUPER_MAGIC, CIFS_MAGIC_NUMBER but
    // these are in varying headers. Simply hard code them.
    // NOTE: The cast is necessary for 32-bit systems because of the 4-byte CIFS_MAGIC_NUMBER
    switch (static_cast<unsigned int>(buf.f_type)) {
        case 0x6969:       // NFS_SUPER_MAGIC
        case 0x517B:       // SMB_SUPER_MAGIC
        case 0xFE534D42U:  // SMB2_MAGIC_NUMBER - not in the manpage
        case 0xFF534D42U:  // CIFS_MAGIC_NUMBER
            return 1;
        default:
            // Other FSes are assumed local.
            return 0;
    }
#elif defined(ST_LOCAL)
    // ST_LOCAL is a flag to statvfs, which is itself standardized.
    // In practice the only system to use this path is NetBSD.
    struct statvfs buf {};
    if (fstatvfs(fd, &buf) < 0) return -1;
    return (buf.f_flag & ST_LOCAL) ? 0 : 1;
#elif defined(MNT_LOCAL)
    struct statfs buf {};
    if (fstatfs(fd, &buf) < 0) return -1;
    return (buf.f_flags & MNT_LOCAL) ? 0 : 1;
#else
    return -1;
#endif
}

static inline void safe_append(char *buffer, const char *s, size_t buffsize) {
    std::strncat(buffer, s, buffsize - std::strlen(buffer) - 1);
}

// In general, strerror is not async-safe, and therefore we cannot use it directly. So instead we
// have to grub through sys_nerr and sys_errlist directly On GNU toolchain, this will produce a
// deprecation warning from the linker (!!), which appears impossible to suppress!
const char *safe_strerror(int err) {
#if defined(__UCLIBC__)
    // uClibc does not have sys_errlist, however, its strerror is believed to be async-safe.
    // See issue #808.
    return std::strerror(err);
#elif defined(HAVE__SYS__ERRS) || defined(HAVE_SYS_ERRLIST)
#ifdef HAVE_SYS_ERRLIST
    if (err >= 0 && err < sys_nerr && sys_errlist[err] != nullptr) {
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

    ignore_result(write(STDERR_FILENO, buff, std::strlen(buff)));
    errno = err;
}

/// Wide character realpath. The last path component does not need to be valid. If an error occurs,
/// wrealpath() returns none() and errno is likely set.
maybe_t<wcstring> wrealpath(const wcstring &pathname) {
    if (pathname.empty()) return none();

    cstring real_path;
    cstring narrow_path = wcs2string(pathname);

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

    wcstring_list_t comps = split_string(path, sep);
    wcstring_list_t new_comps;
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
    // Prepend one or two leading slashes.
    // Two slashes are preserved. Three+ slashes are collapsed to one. (!)
    result.insert(0, allow_leading_double_slashes && leading_slashes > 2 ? 1 : leading_slashes, sep);
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
    wcstring_list_t wd_comps = split_string(wd, sep);
    wcstring_list_t path_comps = split_string(path, sep);

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

wcstring wdirname(const wcstring &path) {
    std::string tmp = wcs2string(path);
    const char *narrow_res = dirname(&tmp[0]);
    return str2wcstring(narrow_res);
}

wcstring wbasename(const wcstring &path) {
    std::string tmp = wcs2string(path);
    char *narrow_res = basename(&tmp[0]);
    return str2wcstring(narrow_res);
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
            size_t amt = static_cast<size_t>(samt);
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

/// We need this because there are too many implementations that don't return the proper answer for
/// some code points. See issue #3050.
int fish_iswgraph(wint_t wc) {
    if (fish_reserved_codepoint(wc)) return 0;
    if (fish_is_pua(wc)) return 1;
    return iswgraph(wc);
}

/// Convenience variants on fish_wcwswidth().
///
/// See fallback.h for the normal definitions.
int fish_wcswidth(const wchar_t *str) { return fish_wcswidth(str, std::wcslen(str)); }

/// Convenience variants on fish_wcwswidth().
///
/// See fallback.h for the normal definitions.
int fish_wcswidth(const wcstring &str) { return fish_wcswidth(str.c_str(), str.size()); }

locale_t fish_c_locale() {
    static const locale_t loc = newlocale(LC_ALL_MASK, "C", nullptr);
    return loc;
}

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
    long long result = std::wcstoll(str, &_endptr, base);
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
    unsigned long long result = std::wcstoull(str, &_endptr, base);
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
double fish_wcstod(const wchar_t *str, wchar_t **endptr) {
    // The "fast path." If we're all ASCII and we fit inline, use strtod().
    char narrow[128];
    size_t len = std::wcslen(str);
    size_t len_plus_0 = 1 + len;
    auto is_digit = [](wchar_t c) { return '0' <= c && c <= '9'; };
    if (len_plus_0 <= sizeof narrow && std::all_of(str, str + len, is_digit)) {
        // Fast path. Copy the string into a local buffer and run strtod() on it.
        // We can ignore the locale-taking version because we are limited to ASCII digits.
        std::copy(str, str + len_plus_0, narrow);
        char *narrow_endptr = nullptr;
        double ret = strtod(narrow, endptr ? &narrow_endptr : nullptr);
        if (endptr) {
            assert(narrow_endptr && "narrow_endptr should not be null");
            *endptr = const_cast<wchar_t *>(str + (narrow_endptr - narrow));
        }
        return ret;
    }
    return wcstod_l(str, endptr, fish_c_locale());
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
