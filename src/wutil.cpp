// Wide character equivalents of various standard unix functions.
#define FISH_NO_ISW_WRAPPERS
#include "config.h"

#include "wutil.h"  // IWYU pragma: keep

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#include <wctype.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <string>
#include <unordered_map>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fds.h"
#include "flog.h"
#include "wcstringutil.h"

using cstring = std::string;

const file_id_t kInvalidFileID{};

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

bool readdir_for_dirs(DIR *dir, std::string *out_name) {
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
        *out_name = result->d_name;
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

maybe_t<wcstring> wreadlink(const wcstring &file_name) {
    struct stat buf;
    if (lwstat(file_name, &buf) == -1) {
        return none();
    }
    ssize_t bufsize = buf.st_size + 1;
    char target_buf[bufsize];
    const std::string tmp = wcs2string(file_name);
    ssize_t nbytes = readlink(tmp.c_str(), target_buf, bufsize);
    if (nbytes == -1) {
        wperror(L"readlink");
        return none();
    }
    // The link might have been modified after our call to lstat.  If the link now points to a path
    // that's longer than the original one, we can't read everything in our buffer.  Simply give
    // up. We don't need to report an error since our only caller will already fall back to ENOENT.
    if (nbytes == bufsize) {
        return none();
    }

    return str2wcstring(target_buf, nbytes);
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

static bool fish_numeric_locale_is_valid = false;

void fish_invalidate_numeric_locale() { fish_numeric_locale_is_valid = false; }

locale_t fish_numeric_locale() {
    // The current locale, except LC_NUMERIC isn't forced to C.
    static locale_t loc = nullptr;
    if (!fish_numeric_locale_is_valid) {
        if (loc != nullptr) freelocale(loc);
        auto cur = duplocale(LC_GLOBAL_LOCALE);
        loc = newlocale(LC_NUMERIC_MASK, "", cur);
        fish_numeric_locale_is_valid = true;
    }
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

/// Like wcstod(), but allows underscore separators. Leading, trailing, and multiple underscores are
/// allowed, as are underscores next to decimal (.), exponent (E/e/P/p), and hexadecimal (X/x)
/// delimiters. This consumes trailing underscores -- endptr will point past the last underscore
/// which is legal to include in a parse (according to the above rules). Free-floating leading
/// underscores ("_ 3") are not allowed and will result in a no-parse. Underscores are not allowed
/// before or inside of "infinity" or "nan" input. Trailing underscores after "infinity" or "nan"
/// are not consumed.
double fish_wcstod_underscores(const wchar_t *str, wchar_t **endptr) {
    const wchar_t *orig = str;
    while (iswspace(*str)) str++;  // Skip leading whitespace.
    size_t leading_whitespace = size_t(str - orig);
    auto is_sign = [](wchar_t c) { return c == L'+' || c == L'-'; };
    auto is_inf_or_nan_char = [](wchar_t c) {
        return c == L'i' || c == L'I' || c == L'n' || c == L'N';
    };
    // We don't do any underscore-stripping for infinity/NaN.
    if (is_inf_or_nan_char(*str) || (is_sign(*str) && is_inf_or_nan_char(*(str + 1)))) {
        return fish_wcstod(orig, endptr);
    }
    // We build a string to pass to the system wcstod, pruned of underscores. We will take all
    // leading alphanumeric characters that can appear in a strtod numeric literal, dots (.), and
    // signs (+/-). In order to be more clever, for example to stop earlier in the case of strings
    // like "123xxxxx", we would need to do a full parse, because sometimes 'a' is a hex digit and
    // sometimes it is the end of the parse, sometimes a dot '.' is a decimal delimiter and
    // sometimes it is the end of the valid parse, as in "1_2.3_4.5_6", etc.
    wcstring pruned;
    // We keep track of the positions *in the pruned string* where there used to be underscores. We
    // will pass the pruned version of the input string to the system wcstod, which in turn will
    // tell us how many characters it consumed. Then we will set our own endptr based on (1) the
    // number of characters consumed from the pruned string, and (2) how many underscores came
    // before the last consumed character. The alternative to doing it this way (for example, "only
    // deleting the correct underscores") would require actually parsing the input string, so that
    // we can know when to stop grabbing characters and dropping underscores, as in "1_2.3_4.5_6".
    std::vector<size_t> underscores;
    // If we wanted to future-proof against a strtod from the future that, say, allows octal
    // literals using 0o, etc., we could just use iswalnum, instead of iswxdigit and P/p/X/x checks.
    while (iswxdigit(*str) || *str == L'P' || *str == L'p' || *str == L'X' || *str == L'x' ||
           is_sign(*str) || *str == L'.' || *str == L'_') {
        if (*str == L'_') {
            underscores.push_back(pruned.length());
        } else {
            pruned.push_back(*str);
        }
        str++;
    }
    const wchar_t *pruned_begin = pruned.c_str();
    const wchar_t *pruned_end = nullptr;
    double result = fish_wcstod(pruned_begin, (wchar_t **)(&pruned_end));
    if (pruned_end == pruned_begin) {
        if (endptr) *endptr = (wchar_t *)orig;
        return result;
    }
    auto consumed_underscores_end =
        std::upper_bound(underscores.begin(), underscores.end(), size_t(pruned_end - pruned_begin));
    size_t num_underscores_consumed = std::distance(underscores.begin(), consumed_underscores_end);
    if (endptr) {
        *endptr = (wchar_t *)(orig + leading_whitespace + (pruned_end - pruned_begin) +
                              num_underscores_consumed);
    }
    return result;
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
