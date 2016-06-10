// Fish needs it's own globbing implementation to support tab-expansion of globbed parameters. Also
// provides recursive wildcards using **.
#include "config.h"  // IWYU pragma: keep

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wchar.h>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "common.h"
#include "complete.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "reader.h"
#include "wildcard.h"
#include "wutil.h"  // IWYU pragma: keep

/// Description for generic executable.
#define COMPLETE_EXEC_DESC _(L"Executable")
/// Description for link to executable.
#define COMPLETE_EXEC_LINK_DESC _(L"Executable link")
/// Description for regular file.
#define COMPLETE_FILE_DESC _(L"File")
/// Description for character device.
#define COMPLETE_CHAR_DESC _(L"Character device")
/// Description for block device.
#define COMPLETE_BLOCK_DESC _(L"Block device")
/// Description for fifo buffer.
#define COMPLETE_FIFO_DESC _(L"Fifo")
/// Description for symlink.
#define COMPLETE_SYMLINK_DESC _(L"Symbolic link")
/// Description for symlink.
#define COMPLETE_DIRECTORY_SYMLINK_DESC _(L"Symbolic link to directory")
/// Description for Rotten symlink.
#define COMPLETE_ROTTEN_SYMLINK_DESC _(L"Rotten symbolic link")
/// Description for symlink loop.
#define COMPLETE_LOOP_SYMLINK_DESC _(L"Symbolic link loop")
/// Description for socket files.
#define COMPLETE_SOCKET_DESC _(L"Socket")
/// Description for directories.
#define COMPLETE_DIRECTORY_DESC _(L"Directory")

/// Finds an internal (ANY_STRING, etc.) style wildcard, or wcstring::npos.
static size_t wildcard_find(const wchar_t *wc) {
    for (size_t i = 0; wc[i] != L'\0'; i++) {
        if (wc[i] == ANY_CHAR || wc[i] == ANY_STRING || wc[i] == ANY_STRING_RECURSIVE) {
            return i;
        }
    }
    return wcstring::npos;
}

/// Implementation of wildcard_has. Needs to take the length to handle embedded nulls (issue #1631).
static bool wildcard_has_impl(const wchar_t *str, size_t len, bool internal) {
    assert(str != NULL);
    const wchar_t *end = str + len;
    if (internal) {
        for (; str < end; str++) {
            if ((*str == ANY_CHAR) || (*str == ANY_STRING) || (*str == ANY_STRING_RECURSIVE))
                return true;
        }
    } else {
        wchar_t prev = 0;
        for (; str < end; str++) {
            if (((*str == L'*') || (*str == L'?')) && (prev != L'\\')) return true;
            prev = *str;
        }
    }

    return false;
}

bool wildcard_has(const wchar_t *str, bool internal) {
    assert(str != NULL);
    return wildcard_has_impl(str, wcslen(str), internal);
}

bool wildcard_has(const wcstring &str, bool internal) {
    return wildcard_has_impl(str.data(), str.size(), internal);
}

/// Check whether the string str matches the wildcard string wc.
///
/// \param str String to be matched.
/// \param wc The wildcard.
/// \param is_first Whether files beginning with dots should not be matched against wildcards.
static enum fuzzy_match_type_t wildcard_match_internal(const wchar_t *str, const wchar_t *wc,
                                                       bool leading_dots_fail_to_match,
                                                       bool is_first) {
    if (*str == 0 && *wc == 0) {
        return fuzzy_match_exact;  // we're done
    }

    // Hackish fix for issue #270. Prevent wildcards from matching . or .., but we must still allow
    // literal matches.
    if (leading_dots_fail_to_match && is_first && contains(str, L".", L"..")) {
        // The string is '.' or '..'. Return true if the wildcard exactly matches.
        return wcscmp(str, wc) ? fuzzy_match_none : fuzzy_match_exact;
    }

    if (*wc == ANY_STRING || *wc == ANY_STRING_RECURSIVE) {
        // Ignore hidden file
        if (leading_dots_fail_to_match && is_first && *str == L'.') {
            return fuzzy_match_none;
        }

        // Common case of * at the end. In that case we can early out since we know it will match.
        if (wc[1] == L'\0') {
            return fuzzy_match_exact;
        }

        // Try all submatches.
        do {
            enum fuzzy_match_type_t subresult =
                wildcard_match_internal(str, wc + 1, leading_dots_fail_to_match, false);
            if (subresult != fuzzy_match_none) {
                return subresult;
            }
        } while (*str++ != 0);
        return fuzzy_match_none;
    } else if (*str == 0) {
        // End of string, but not end of wildcard, and the next wildcard element is not a '*', so
        // this is not a match.
        return fuzzy_match_none;
    } else if (*wc == ANY_CHAR) {
        if (is_first && *str == L'.') {
            return fuzzy_match_none;
        }

        return wildcard_match_internal(str + 1, wc + 1, leading_dots_fail_to_match, false);
    } else if (*wc == *str) {
        return wildcard_match_internal(str + 1, wc + 1, leading_dots_fail_to_match, false);
    }

    return fuzzy_match_none;
}

// This does something horrible refactored from an even more horrible function.
static wcstring resolve_description(wcstring *completion, const wchar_t *explicit_desc,
                                    wcstring (*desc_func)(const wcstring &)) {
    size_t complete_sep_loc = completion->find(PROG_COMPLETE_SEP);
    if (complete_sep_loc != wcstring::npos) {
        // This completion has an embedded description, do not use the generic description.
        const wcstring description = completion->substr(complete_sep_loc + 1);
        completion->resize(complete_sep_loc);
        return description;
    }

    const wcstring func_result = (desc_func ? desc_func(*completion) : wcstring());
    if (!func_result.empty()) {
        return func_result;
    }
    return explicit_desc ? explicit_desc : L"";
}

// A transient parameter pack needed by wildcard_complete.
struct wc_complete_pack_t {
    const wcstring &orig;                     // the original string, transient
    const wchar_t *desc;                      // literal description
    wcstring (*desc_func)(const wcstring &);  // function for generating descriptions
    expand_flags_t expand_flags;
    wc_complete_pack_t(const wcstring &str, const wchar_t *des, wcstring (*df)(const wcstring &),
                       expand_flags_t fl)
        : orig(str), desc(des), desc_func(df), expand_flags(fl) {}
};

// Weirdly specific and non-reusable helper function that makes its one call site much clearer.
static bool has_prefix_match(const std::vector<completion_t> *comps, size_t first) {
    if (comps != NULL) {
        const size_t after_count = comps->size();
        for (size_t j = first; j < after_count; j++) {
            if (comps->at(j).match.type <= fuzzy_match_prefix) {
                return true;
            }
        }
    }
    return false;
}

/// Matches the string against the wildcard, and if the wildcard is a possible completion of the
/// string, the remainder of the string is inserted into the out vector.
///
/// We ignore ANY_STRING_RECURSIVE here. The consequence is that you cannot tab complete **
/// wildcards. This is historic behavior.
static bool wildcard_complete_internal(const wchar_t *str, const wchar_t *wc,
                                       const wc_complete_pack_t &params, complete_flags_t flags,
                                       std::vector<completion_t> *out, bool is_first_call = false) {
    assert(str != NULL);
    assert(wc != NULL);

    // Maybe early out for hidden files. We require that the wildcard match these exactly (i.e. a
    // dot); ANY_STRING not allowed.
    if (is_first_call && str[0] == L'.' && wc[0] != L'.') {
        return false;
    }

    // Locate the next wildcard character position, e.g. ANY_CHAR or ANY_STRING.
    const size_t next_wc_char_pos = wildcard_find(wc);

    // Maybe we have no more wildcards at all. This includes the empty string.
    if (next_wc_char_pos == wcstring::npos) {
        string_fuzzy_match_t match = string_fuzzy_match_string(wc, str);

        // If we're allowing fuzzy match, any match is OK. Otherwise we require a prefix match.
        bool match_acceptable;
        if (params.expand_flags & EXPAND_FUZZY_MATCH) {
            match_acceptable = match.type != fuzzy_match_none;
        } else {
            match_acceptable = match_type_shares_prefix(match.type);
        }

        if (match_acceptable && out != NULL) {
            // Wildcard complete.
            bool full_replacement = match_type_requires_full_replacement(match.type) ||
                                    (flags & COMPLETE_REPLACES_TOKEN);

            // If we are not replacing the token, be careful to only store the part of the string
            // after the wildcard.
            assert(!full_replacement || wcslen(wc) <= wcslen(str));
            wcstring out_completion = full_replacement ? params.orig : str + wcslen(wc);
            wcstring out_desc = resolve_description(&out_completion, params.desc, params.desc_func);

            // Note: out_completion may be empty if the completion really is empty, e.g.
            // tab-completing 'foo' when a file 'foo' exists.
            complete_flags_t local_flags = flags | (full_replacement ? COMPLETE_REPLACES_TOKEN : 0);
            append_completion(out, out_completion, out_desc, local_flags, match);
        }
        return match_acceptable;
    } else if (next_wc_char_pos > 0) {
        // Here we have a non-wildcard prefix. Note that we don't do fuzzy matching for stuff before
        // a wildcard, so just do case comparison and then recurse.
        if (wcsncmp(str, wc, next_wc_char_pos) == 0) {
            // Normal match.
            return wildcard_complete_internal(str + next_wc_char_pos, wc + next_wc_char_pos, params,
                                              flags, out);
        }
        if (wcsncasecmp(str, wc, next_wc_char_pos) == 0) {
            // Case insensitive match.
            return wildcard_complete_internal(str + next_wc_char_pos, wc + next_wc_char_pos, params,
                                              flags | COMPLETE_REPLACES_TOKEN, out);
        }
        return false;  // no match
    }

    // Our first character is a wildcard.
    assert(next_wc_char_pos == 0);
    switch (wc[0]) {
        case ANY_CHAR: {
            if (str[0] == L'\0') {
                return false;
            }
            return wildcard_complete_internal(str + 1, wc + 1, params, flags, out);
        }
        case ANY_STRING: {
            // Hackish. If this is the last character of the wildcard, then just complete with
            // the empty string. This fixes cases like "f*<tab>" -> "f*o".
            if (wc[1] == L'\0') {
                return wildcard_complete_internal(L"", L"", params, flags, out);
            }

            // Try all submatches. Issue #929: if the recursive call gives us a prefix match,
            // just stop. This is sloppy - what we really want to do is say, once we've seen a
            // match of a particular type, ignore all matches of that type further down the
            // string, such that the wildcard produces the "minimal match.".
            bool has_match = false;
            for (size_t i = 0; str[i] != L'\0'; i++) {
                const size_t before_count = out ? out->size() : 0;
                if (wildcard_complete_internal(str + i, wc + 1, params, flags, out)) {
                    // We found a match.
                    has_match = true;

                    // If out is NULL, we don't care about the actual matches. If out is not
                    // NULL but we have a prefix match, stop there.
                    if (out == NULL || has_prefix_match(out, before_count)) {
                        break;
                    }
                }
            }
            return has_match;
        }
        case ANY_STRING_RECURSIVE: {
            // We don't even try with this one.
            return false;
        }
        default: { assert(0 && "Unreachable code reached"); }
    }

    assert(0 && "Unreachable code reached");
}

bool wildcard_complete(const wcstring &str, const wchar_t *wc, const wchar_t *desc,
                       wcstring (*desc_func)(const wcstring &), std::vector<completion_t> *out,
                       expand_flags_t expand_flags, complete_flags_t flags) {
    // Note out may be NULL.
    assert(wc != NULL);
    wc_complete_pack_t params(str, desc, desc_func, expand_flags);
    return wildcard_complete_internal(str.c_str(), wc, params, flags, out, true /* first call */);
}

bool wildcard_match(const wcstring &str, const wcstring &wc, bool leading_dots_fail_to_match) {
    enum fuzzy_match_type_t match = wildcard_match_internal(
        str.c_str(), wc.c_str(), leading_dots_fail_to_match, true /* first */);
    return match != fuzzy_match_none;
}

/// Obtain a description string for the file specified by the filename.
///
/// The returned value is a string constant and should not be free'd.
///
/// \param filename The file for which to find a description string
/// \param lstat_res The result of calling lstat on the file
/// \param lbuf The struct buf output of calling lstat on the file
/// \param stat_res The result of calling stat on the file
/// \param buf The struct buf output of calling stat on the file
/// \param err The errno value after a failed stat call on the file.
static wcstring file_get_desc(const wcstring &filename, int lstat_res, const struct stat &lbuf,
                              int stat_res, const struct stat &buf, int err) {
    if (!lstat_res) {
        if (S_ISLNK(lbuf.st_mode)) {
            if (!stat_res) {
                if (S_ISDIR(buf.st_mode)) {
                    return COMPLETE_DIRECTORY_SYMLINK_DESC;
                }
                if (buf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
                    if (waccess(filename, X_OK) == 0) {
                        // Weird group permissions and other such issues make it non-trivial to
                        // find out if we can actually execute a file using the result from
                        // stat. It is much safer to use the access function, since it tells us
                        // exactly what we want to know.
                        return COMPLETE_EXEC_LINK_DESC;
                    }
                }

                return COMPLETE_SYMLINK_DESC;
            }

            switch (err) {
                case ENOENT: {
                    return COMPLETE_ROTTEN_SYMLINK_DESC;
                }
                case ELOOP: {
                    return COMPLETE_LOOP_SYMLINK_DESC;
                }
                default: {
                    // On unknown errors we do nothing. The file will be given the default 'File'
                    // description or one based on the suffix.
                }
            }
        } else if (S_ISCHR(buf.st_mode)) {
            return COMPLETE_CHAR_DESC;
        } else if (S_ISBLK(buf.st_mode)) {
            return COMPLETE_BLOCK_DESC;
        } else if (S_ISFIFO(buf.st_mode)) {
            return COMPLETE_FIFO_DESC;
        } else if (S_ISSOCK(buf.st_mode)) {
            return COMPLETE_SOCKET_DESC;
        } else if (S_ISDIR(buf.st_mode)) {
            return COMPLETE_DIRECTORY_DESC;
        } else {
            if (buf.st_mode & (S_IXUSR | S_IXGRP | S_IXGRP)) {
                if (waccess(filename, X_OK) == 0) {
                    // Weird group permissions and other such issues make it non-trivial to find out
                    // if we can actually execute a file using the result from stat. It is much
                    // safer to use the access function, since it tells us exactly what we want to
                    // know.
                    return COMPLETE_EXEC_DESC;
                }
            }
        }
    }

    return COMPLETE_FILE_DESC;
}

/// Test if the given file is an executable (if EXECUTABLES_ONLY) or directory (if
/// DIRECTORIES_ONLY). If it matches, call wildcard_complete() with some description that we make
/// up. Note that the filename came from a readdir() call, so we know it exists.
static bool wildcard_test_flags_then_complete(const wcstring &filepath, const wcstring &filename,
                                              const wchar_t *wc, expand_flags_t expand_flags,
                                              std::vector<completion_t> *out) {
    // Check if it will match before stat().
    if (!wildcard_complete(filename, wc, NULL, NULL, NULL, expand_flags, 0)) {
        return false;
    }

    struct stat lstat_buf = {}, stat_buf = {};
    int stat_res = -1;
    int stat_errno = 0;
    int lstat_res = lwstat(filepath, &lstat_buf);
    if (lstat_res < 0) {
        // lstat failed.
    } else {
        if (S_ISLNK(lstat_buf.st_mode)) {
            stat_res = wstat(filepath, &stat_buf);

            if (stat_res < 0) {
                // In order to differentiate between e.g. rotten symlinks and symlink loops, we also
                // need to know the error status of wstat.
                stat_errno = errno;
            }
        } else {
            stat_buf = lstat_buf;
            stat_res = lstat_res;
        }
    }

    const long long file_size = stat_res == 0 ? stat_buf.st_size : 0;
    const bool is_directory = stat_res == 0 && S_ISDIR(stat_buf.st_mode);
    const bool is_executable = stat_res == 0 && S_ISREG(stat_buf.st_mode);

    if (expand_flags & DIRECTORIES_ONLY) {
        if (!is_directory) {
            return false;
        }
    }
    if (expand_flags & EXECUTABLES_ONLY) {
        if (!is_executable || waccess(filepath, X_OK) != 0) {
            return false;
        }
    }

    // Compute the description.
    wcstring desc;
    if (!(expand_flags & EXPAND_NO_DESCRIPTIONS)) {
        desc = file_get_desc(filepath, lstat_res, lstat_buf, stat_res, stat_buf, stat_errno);

        if (file_size >= 0) {
            if (!desc.empty()) desc.append(L", ");
            desc.append(format_size(file_size));
        }
    }

    // Append a / if this is a directory. Note this requirement may be the only reason we have to
    // call stat() in some cases.
    if (is_directory) {
        return wildcard_complete(filename + L'/', wc, desc.c_str(), NULL, out, expand_flags,
                                 COMPLETE_NO_SPACE);
    }
    return wildcard_complete(filename, wc, desc.c_str(), NULL, out, expand_flags, 0);
}

class wildcard_expander_t {
    // Prefix, i.e. effective working directory.
    const wcstring prefix;
    // The original base we are expanding.
    const wcstring original_base;
    // Original wildcard we are expanding.
    const wchar_t *const original_wildcard;
    // The set of items we have resolved, used to efficiently avoid duplication.
    std::set<wcstring> completion_set;
    // The set of file IDs we have visited, used to avoid symlink loops.
    std::set<file_id_t> visited_files;
    // Flags controlling expansion.
    const expand_flags_t flags;
    // Resolved items get inserted into here. This is transient of course.
    std::vector<completion_t> *resolved_completions;
    // Whether we have been interrupted.
    bool did_interrupt;
    // Whether we have successfully added any completions.
    bool did_add;

    /// We are a trailing slash - expand at the end.
    void expand_trailing_slash(const wcstring &base_dir);

    /// Given a directory base_dir, which is opened as base_dir_fp, expand an intermediate segment
    /// of the wildcard. Treat ANY_STRING_RECURSIVE as ANY_STRING. wc_segment is the wildcard
    /// segment for this directory wc_remainder is the wildcard for subdirectories
    void expand_intermediate_segment(const wcstring &base_dir, DIR *base_dir_fp,
                                     const wcstring &wc_segment, const wchar_t *wc_remainder);

    /// Given a directory base_dir, which is opened as base_dir_fp, expand an intermediate literal
    /// segment. Use a fuzzy matching algorithm.
    void expand_literal_intermediate_segment_with_fuzz(const wcstring &base_dir, DIR *base_dir_fp,
                                                       const wcstring &wc_segment,
                                                       const wchar_t *wc_remainder);

    /// Given a directory base_dir, which is opened as base_dir_fp, expand the last segment of the
    /// wildcard. Treat ANY_STRING_RECURSIVE as ANY_STRING. wc is the wildcard segment to use for
    /// matching wc_remainder is the wildcard for subdirectories.
    void expand_last_segment(const wcstring &base_dir, DIR *base_dir_fp, const wcstring &wc);

    /// Indicate whether we should cancel wildcard expansion. This latches 'interrupt'.
    bool interrupted() {
        if (!did_interrupt) {
            did_interrupt =
                (is_main_thread() ? reader_interrupted() : reader_thread_job_is_stale());
        }
        return did_interrupt;
    }

    void add_expansion_result(const wcstring &result) {
        // This function is only for the non-completions case.
        assert(!(this->flags & EXPAND_FOR_COMPLETIONS));
        if (this->completion_set.insert(result).second) {
            append_completion(this->resolved_completions, result);
            this->did_add = true;
        }
    }

    // Given a start point as an absolute path, for any directory that has exactly one non-hidden
    // entity in it which is itself a directory, return that. The result is a relative path. For
    // example, if start_point is '/usr' we may return 'local/bin/'.
    //
    // The result does not have a leading slash, but does have a trailing slash if non-empty.
    wcstring descend_unique_hierarchy(const wcstring &start_point) {
        assert(!start_point.empty() && start_point.at(0) == L'/');

        wcstring unique_hierarchy;
        wcstring abs_unique_hierarchy = start_point;

        bool stop_descent = false;
        DIR *dir;
        while (!stop_descent && (dir = wopendir(abs_unique_hierarchy))) {
            // We keep track of the single unique_entry entry. If we get more than one, it's not
            // unique and we stop the descent.
            wcstring unique_entry;

            bool child_is_dir;
            wcstring child_entry;
            while (wreaddir_resolving(dir, abs_unique_hierarchy, child_entry, &child_is_dir)) {
                if (child_entry.empty() || child_entry.at(0) == L'.') {
                    continue;  // either hidden, or . and .. entries -- skip them
                } else if (child_is_dir && unique_entry.empty()) {
                    unique_entry = child_entry;  // first candidate
                } else {
                    // We either have two or more candidates, or the child is not a directory. We're
                    // done.
                    stop_descent = true;
                    break;
                }
            }

            // We stop if we got two or more entries; also stop if we got zero.
            if (unique_entry.empty()) {
                stop_descent = true;
            }

            if (!stop_descent) {
                // We have an entry in the unique hierarchy!
                append_path_component(unique_hierarchy, unique_entry);
                unique_hierarchy.push_back(L'/');

                append_path_component(abs_unique_hierarchy, unique_entry);
                abs_unique_hierarchy.push_back(L'/');
            }
            closedir(dir);
        }
        return unique_hierarchy;
    }

    void try_add_completion_result(const wcstring &filepath, const wcstring &filename,
                                   const wcstring &wildcard) {
        // This function is only for the completions case.
        assert(this->flags & EXPAND_FOR_COMPLETIONS);

        wcstring abs_path = this->prefix;
        append_path_component(abs_path, filepath);

        size_t before = this->resolved_completions->size();
        if (wildcard_test_flags_then_complete(abs_path, filename, wildcard.c_str(), this->flags,
                                              this->resolved_completions)) {
            // Hack. We added this completion result based on the last component of the wildcard.
            // Prepend all prior components of the wildcard to each completion that replaces its
            // token.
            size_t wc_len = wildcard.size();
            size_t orig_wc_len = wcslen(this->original_wildcard);
            assert(wc_len <= orig_wc_len);
            const wcstring wc_base(this->original_wildcard, orig_wc_len - wc_len);

            size_t after = this->resolved_completions->size();
            for (size_t i = before; i < after; i++) {
                completion_t &c = this->resolved_completions->at(i);
                c.prepend_token_prefix(wc_base);
                c.prepend_token_prefix(this->original_base);
            }

            // Hack. Implement EXPAND_SPECIAL_FOR_CD by descending the deepest unique hierarchy we
            // can, and then appending any components to each new result.
            if (flags & EXPAND_SPECIAL_FOR_CD) {
                wcstring unique_hierarchy = this->descend_unique_hierarchy(abs_path);
                if (!unique_hierarchy.empty()) {
                    for (size_t i = before; i < after; i++) {
                        completion_t &c = this->resolved_completions->at(i);
                        c.completion.append(unique_hierarchy);
                    }
                }
            }

            this->did_add = true;
        }
    }

    // Helper to resolve using our prefix.
    DIR *open_dir(const wcstring &base_dir) const {
        wcstring path = this->prefix;
        append_path_component(path, base_dir);
        return wopendir(path);
    }

   public:
    wildcard_expander_t(const wcstring &pref, const wcstring &orig_base, const wchar_t *orig_wc,
                        expand_flags_t f, std::vector<completion_t> *r)
        : prefix(pref),
          original_base(orig_base),
          original_wildcard(orig_wc),
          flags(f),
          resolved_completions(r),
          did_interrupt(false),
          did_add(false) {
        assert(resolved_completions != NULL);

        // Insert initial completions into our set to avoid duplicates.
        for (std::vector<completion_t>::const_iterator iter = resolved_completions->begin();
             iter != resolved_completions->end(); ++iter) {
            this->completion_set.insert(iter->completion);
        }
    }

    // Do wildcard expansion. This is recursive.
    void expand(const wcstring &base_dir, const wchar_t *wc);

    int status_code() const {
        if (this->did_interrupt) {
            return -1;
        }
        return this->did_add ? 1 : 0;
    }
};

void wildcard_expander_t::expand_trailing_slash(const wcstring &base_dir) {
    if (interrupted()) {
        return;
    }

    if (!(flags & EXPAND_FOR_COMPLETIONS)) {
        // Trailing slash and not accepting incomplete, e.g. `echo /tmp/`. Insert this file if it
        // exists.
        if (waccess(base_dir, F_OK) == 0) {
            this->add_expansion_result(base_dir);
        }
    } else {
        // Trailing slashes and accepting incomplete, e.g. `echo /tmp/<tab>`. Everything is added.
        DIR *dir = open_dir(base_dir);
        if (dir) {
            wcstring next;
            while (wreaddir(dir, next) && !interrupted()) {
                if (!next.empty() && next.at(0) != L'.') {
                    this->try_add_completion_result(base_dir + next, next, L"");
                }
            }
            closedir(dir);
        }
    }
}

void wildcard_expander_t::expand_intermediate_segment(const wcstring &base_dir, DIR *base_dir_fp,
                                                      const wcstring &wc_segment,
                                                      const wchar_t *wc_remainder) {
    wcstring name_str;
    while (!interrupted() && wreaddir_for_dirs(base_dir_fp, &name_str)) {
        // Note that it's critical we ignore leading dots here, else we may descend into . and ..
        if (!wildcard_match(name_str, wc_segment, true)) {
            // Doesn't match the wildcard for this segment, skip it.
            continue;
        }

        wcstring full_path = base_dir + name_str;
        struct stat buf;
        if (0 != wstat(full_path, &buf) || !S_ISDIR(buf.st_mode)) {
            // We either can't stat it, or we did but it's not a directory.
            continue;
        }

        const file_id_t file_id = file_id_t::file_id_from_stat(&buf);
        if (!this->visited_files.insert(file_id).second) {
            // Symlink loop! This directory was already visited, so skip it.
            continue;
        }

        // We made it through. Perform normal wildcard expansion on this new directory, starting at
        // our tail_wc, which includes the ANY_STRING_RECURSIVE guy.
        full_path.push_back(L'/');
        this->expand(full_path, wc_remainder);

        // Now remove the visited file. This is for #2414: only directories "beneath" us should be
        // considered visited.
        this->visited_files.erase(file_id);
    }
}

void wildcard_expander_t::expand_literal_intermediate_segment_with_fuzz(
    const wcstring &base_dir, DIR *base_dir_fp, const wcstring &wc_segment,
    const wchar_t *wc_remainder) {
    // This only works with tab completions. Ordinary wildcard expansion should never go fuzzy.
    wcstring name_str;
    while (!interrupted() && wreaddir_for_dirs(base_dir_fp, &name_str)) {
        // Don't bother with . and ..
        if (contains(name_str, L".", L"..")) {
            continue;
        }

        // Skip cases that don't match or match exactly. The match-exactly case was handled directly
        // in expand().
        const string_fuzzy_match_t match = string_fuzzy_match_string(wc_segment, name_str);
        if (match.type == fuzzy_match_none || match.type == fuzzy_match_exact) {
            continue;
        }

        wcstring new_full_path = base_dir + name_str;
        new_full_path.push_back(L'/');
        struct stat buf;
        if (0 != wstat(new_full_path, &buf) || !S_ISDIR(buf.st_mode)) {
            /* We either can't stat it, or we did but it's not a directory */
            continue;
        }

        // Ok, this directory matches. Recurse to it. Then perform serious surgery on each result!
        // Each result was computed with a prefix of original_wildcard. We need to replace our
        // segment of that with our name_str. We also have to mark the completion as replacing and
        // fuzzy.
        const size_t before = this->resolved_completions->size();

        this->expand(new_full_path, wc_remainder);
        const size_t after = this->resolved_completions->size();

        assert(before <= after);
        for (size_t i = before; i < after; i++) {
            completion_t *c = &this->resolved_completions->at(i);
            // Mark the completion as replacing.
            if (!(c->flags & COMPLETE_REPLACES_TOKEN)) {
                c->flags |= COMPLETE_REPLACES_TOKEN;
                c->prepend_token_prefix(this->original_wildcard);
                c->prepend_token_prefix(this->original_base);
            }
            // Ok, it's now replacing and is prefixed with the segment base, plus our original
            // wildcard. Replace our segment with name_str. Our segment starts at the length of the
            // original wildcard, minus what we have left to process, minus the length of our
            // segment. This logic is way too picky. Need to clean this up. One possibility is to
            // send the "resolved wildcard" along with the actual wildcard.
            const size_t original_wildcard_len = wcslen(this->original_wildcard);
            const size_t wc_remainder_len = wcslen(wc_remainder);
            const size_t segment_len = wc_segment.length();
            assert(c->completion.length() >= original_wildcard_len);
            const size_t segment_start = original_wildcard_len + this->original_base.size() -
                                         wc_remainder_len - wc_segment.length() -
                                         1;  // -1 for the slash after our segment
            assert(segment_start < original_wildcard_len);
            assert(c->completion.substr(segment_start, segment_len) == wc_segment);
            c->completion.replace(segment_start, segment_len, name_str);

            // And every match must be made at least as fuzzy as ours.
            if (match.compare(c->match) > 0) {
                // Our match is fuzzier.
                c->match = match;
            }
        }
    }
}

void wildcard_expander_t::expand_last_segment(const wcstring &base_dir, DIR *base_dir_fp,
                                              const wcstring &wc) {
    wcstring name_str;
    while (wreaddir(base_dir_fp, name_str)) {
        if (flags & EXPAND_FOR_COMPLETIONS) {
            this->try_add_completion_result(base_dir + name_str, name_str, wc);
        } else {
            // Normal wildcard expansion, not for completions.
            if (wildcard_match(name_str, wc, true /* skip files with leading dots */)) {
                this->add_expansion_result(base_dir + name_str);
            }
        }
    }
}

/// The real implementation of wildcard expansion is in this function. Other functions are just
/// wrappers around this one.
///
/// This function traverses the relevant directory tree looking for matches, and recurses when
/// needed to handle wildcrards spanning multiple components and recursive wildcards.
///
/// Because this function calls itself recursively with substrings, it's important that the
/// parameters be raw pointers instead of wcstring, which would be too expensive to construct for
/// all substrings.
///
/// Args:
/// base_dir: the "working directory" against which the wildcard is to be resolved
/// wc: the wildcard string itself, e.g. foo*bar/baz (where * is acutally ANY_CHAR)
void wildcard_expander_t::expand(const wcstring &base_dir, const wchar_t *wc) {
    assert(wc != NULL);

    if (interrupted()) {
        return;
    }

    // Get the current segment and compute interesting properties about it.
    const size_t wc_len = wcslen(wc);
    const wchar_t *const next_slash = wcschr(wc, L'/');
    const bool is_last_segment = (next_slash == NULL);
    const size_t wc_segment_len = next_slash ? next_slash - wc : wc_len;
    const wcstring wc_segment = wcstring(wc, wc_segment_len);
    const bool segment_has_wildcards =
        wildcard_has(wc_segment, true /* internal, i.e. look for ANY_CHAR instead of ? */);

    if (wc_segment.empty()) {
        // Handle empty segment.
        assert(!segment_has_wildcards);
        if (is_last_segment) {
            this->expand_trailing_slash(base_dir);
        } else {
            // Multiple adjacent slashes in the wildcard. Just skip them.
            this->expand(base_dir, next_slash + 1);
        }
    } else if (!segment_has_wildcards && !is_last_segment) {
        // Literal intermediate match. Note that we may not be able to actually read the directory
        // (issue #2099).
        assert(next_slash != NULL);
        const wchar_t *wc_remainder = next_slash;
        while (*wc_remainder == L'/') {
            wc_remainder++;
        }

        // This just trumps everything.
        size_t before = this->resolved_completions->size();
        this->expand(base_dir + wc_segment + L'/', wc_remainder);

        // Maybe try a fuzzy match (#94) if nothing was found with the literal match. Respect
        // EXPAND_NO_DIRECTORY_ABBREVIATIONS (issue #2413).
        bool allow_fuzzy = (this->flags & (EXPAND_FUZZY_MATCH | EXPAND_NO_FUZZY_DIRECTORIES)) ==
                           EXPAND_FUZZY_MATCH;
        if (allow_fuzzy && this->resolved_completions->size() == before) {
            assert(this->flags & EXPAND_FOR_COMPLETIONS);
            DIR *base_dir_fd = open_dir(base_dir);
            if (base_dir_fd != NULL) {
                this->expand_literal_intermediate_segment_with_fuzz(base_dir, base_dir_fd,
                                                                    wc_segment, wc_remainder);
                closedir(base_dir_fd);
            }
        }
    } else {
        assert(!wc_segment.empty() && (segment_has_wildcards || is_last_segment));
        DIR *dir = open_dir(base_dir);
        if (dir) {
            if (is_last_segment) {
                // Last wildcard segment, nonempty wildcard.
                this->expand_last_segment(base_dir, dir, wc_segment);
            } else {
                // Not the last segment, nonempty wildcard.
                assert(next_slash != NULL);
                const wchar_t *wc_remainder = next_slash;
                while (*wc_remainder == L'/') {
                    wc_remainder++;
                }
                this->expand_intermediate_segment(base_dir, dir, wc_segment, wc_remainder);
            }

            // Recursive wildcards require special handling.
            size_t asr_idx = wc_segment.find(ANY_STRING_RECURSIVE);
            if (asr_idx != wcstring::npos) {
                // Construct a "head + any" wildcard for matching stuff in this directory, and an
                // "any + tail" wildcard for matching stuff in subdirectories. Note that the
                // ANY_STRING_RECURSIVE character is present in both the head and the tail.
                const wcstring head_any(wc_segment, 0, asr_idx + 1);
                const wchar_t *any_tail = wc + asr_idx;
                assert(head_any.at(head_any.size() - 1) == ANY_STRING_RECURSIVE);
                assert(any_tail[0] == ANY_STRING_RECURSIVE);

                rewinddir(dir);
                this->expand_intermediate_segment(base_dir, dir, head_any, any_tail);
            }
            closedir(dir);
        }
    }
}

int wildcard_expand_string(const wcstring &wc, const wcstring &working_directory,
                           expand_flags_t flags, std::vector<completion_t> *output) {
    assert(output != NULL);
    // Fuzzy matching only if we're doing completions.
    assert((flags & (EXPAND_FUZZY_MATCH | EXPAND_FOR_COMPLETIONS)) != EXPAND_FUZZY_MATCH);

    // EXPAND_SPECIAL_FOR_CD requires DIRECTORIES_ONLY and EXPAND_FOR_COMPLETIONS and
    // EXPAND_NO_DESCRIPTIONS.
    assert(!(flags & EXPAND_SPECIAL_FOR_CD) ||
           ((flags & DIRECTORIES_ONLY) && (flags & EXPAND_FOR_COMPLETIONS) &&
            (flags & EXPAND_NO_DESCRIPTIONS)));

    // Hackish fix for issue #1631. We are about to call c_str(), which will produce a string
    // truncated at any embedded nulls. We could fix this by passing around the size, etc. However
    // embedded nulls are never allowed in a filename, so we just check for them and return 0 (no
    // matches) if there is an embedded null.
    if (wc.find(L'\0') != wcstring::npos) {
        return 0;
    }

    // Compute the prefix and base dir. The prefix is what we prepend for filesystem operations
    // (i.e. the working directory), the base_dir is the part of the wildcard consumed thus far,
    // which we also have to append. The difference is that the base_dir is returned as part of the
    // expansion, and the prefix is not.
    //
    // Check for a leading slash. If we find one, we have an absolute path: the prefix is empty, the
    // base dir is /, and the wildcard is the remainder. If we don't find one, the prefix is the
    // working directory, there's base dir is empty.
    wcstring prefix, base_dir, effective_wc;
    if (string_prefixes_string(L"/", wc)) {
        prefix = L"";
        base_dir = L"/";
        effective_wc = wc.substr(1);
    } else {
        prefix = working_directory;
        base_dir = L"";
        effective_wc = wc;
    }

    wildcard_expander_t expander(prefix, base_dir, effective_wc.c_str(), flags, output);
    expander.expand(base_dir, wc.c_str());
    return expander.status_code();
}
