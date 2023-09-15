// Fish needs it's own globbing implementation to support tab-expansion of globbed parameters. Also
// provides recursive wildcards using **.
#include "config.h"  // IWYU pragma: keep

#include "wildcard.h"

#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cwchar>
#include <functional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common.h"
#include "complete.h"
#include "enum_set.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "future_feature_flags.h"
#include "maybe.h"
#include "path.h"
#include "wcstringutil.h"
#include "wutil.h"  // IWYU pragma: keep

/// Finds an internal (ANY_STRING, etc.) style wildcard, or wcstring::npos.
static size_t wildcard_find(const wchar_t *wc) {
    for (size_t i = 0; wc[i] != L'\0'; i++) {
        if (wc[i] == ANY_CHAR || wc[i] == ANY_STRING || wc[i] == ANY_STRING_RECURSIVE) {
            return i;
        }
    }
    return wcstring::npos;
}

// This does something horrible refactored from an even more horrible function.
static wcstring resolve_description(const wcstring &full_completion, wcstring *completion,
                                    expand_flags_t expand_flags,
                                    const description_func_t &desc_func) {
    size_t complete_sep_loc = completion->find(PROG_COMPLETE_SEP);
    if (complete_sep_loc != wcstring::npos) {
        // This completion has an embedded description, do not use the generic description.
        wcstring description = completion->substr(complete_sep_loc + 1);
        completion->resize(complete_sep_loc);
        return description;
    }
    if (desc_func && (expand_flags & expand_flag::gen_descriptions)) {
        return desc_func(full_completion);
    }
    return wcstring{};
}

namespace {
// A transient parameter pack needed by wildcard_complete.
struct wc_complete_pack_t {
    const wcstring &orig;                 // the original string, transient
    const description_func_t &desc_func;  // function for generating descriptions
    expand_flags_t expand_flags;
    wc_complete_pack_t(const wcstring &str, const description_func_t &df, expand_flags_t fl)
        : orig(str), desc_func(df), expand_flags(fl) {}
};
}  // namespace

// Weirdly specific and non-reusable helper function that makes its one call site much clearer.
static bool has_prefix_match(const completion_receiver_t *comps, size_t first) {
    if (comps != nullptr) {
        const size_t after_count = comps->size();
        for (size_t j = first; j < after_count; j++) {
            const auto &match = comps->at(j).match;
            if (match.type <= string_fuzzy_match_t::contain_type_t::prefix &&
                match.case_fold == string_fuzzy_match_t::case_fold_t::samecase) {
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
static wildcard_result_t wildcard_complete_internal(const wchar_t *const str, size_t str_len,
                                                    const wchar_t *const wc, size_t wc_len,
                                                    const wc_complete_pack_t &params,
                                                    complete_flags_t flags,
                                                    completion_receiver_t *out,
                                                    bool is_first_call = false) {
    assert(str != nullptr);
    assert(wc != nullptr);

    // Maybe early out for hidden files. We require that the wildcard match these exactly (i.e. a
    // dot); ANY_STRING not allowed.
    if (is_first_call && !params.expand_flags.get(expand_flag::allow_nonliteral_leading_dot) &&
        str[0] == L'.' && wc[0] != L'.') {
        return wildcard_result_t::no_match;
    }

    // Locate the next wildcard character position, e.g. ANY_CHAR or ANY_STRING.
    const size_t next_wc_char_pos = wildcard_find(wc);

    // Maybe we have no more wildcards at all. This includes the empty string.
    if (next_wc_char_pos == wcstring::npos) {
        // Try matching.
        maybe_t<string_fuzzy_match_t> match = string_fuzzy_match_string(wc, str);
        if (!match) return wildcard_result_t::no_match;

        // If we're not allowing fuzzy match, then we require a prefix match.
        bool needs_prefix_match = !(params.expand_flags & expand_flag::fuzzy_match);
        if (needs_prefix_match && !match->is_exact_or_prefix()) {
            return wildcard_result_t::no_match;
        }

        // The match was successful. If the string is not requested we're done.
        if (out == nullptr) {
            return wildcard_result_t::match;
        }

        // Wildcard complete.
        bool full_replacement =
            match->requires_full_replacement() || (flags & COMPLETE_REPLACES_TOKEN);

        // If we are not replacing the token, be careful to only store the part of the string after
        // the wildcard.
        assert(!full_replacement || wc_len <= str_len);
        wcstring out_completion = full_replacement ? params.orig : str + wc_len;
        wcstring out_desc = resolve_description(params.orig, &out_completion, params.expand_flags,
                                                params.desc_func);

        // Note: out_completion may be empty if the completion really is empty, e.g. tab-completing
        // 'foo' when a file 'foo' exists.
        complete_flags_t local_flags = flags | (full_replacement ? COMPLETE_REPLACES_TOKEN : 0);
        if (!out->add(std::move(out_completion), std::move(out_desc), local_flags, *match)) {
            return wildcard_result_t::overflow;
        }
        return wildcard_result_t::match;
    } else if (next_wc_char_pos > 0) {
        // The literal portion of a wildcard cannot be longer than the string itself,
        // e.g. `abc*` can never match a string that is only two characters long.
        if (next_wc_char_pos >= str_len) {
            return wildcard_result_t::no_match;
        }

        // Here we have a non-wildcard prefix. Note that we don't do fuzzy matching for stuff before
        // a wildcard, so just do case comparison and then recurse.
        if (std::wcsncmp(str, wc, next_wc_char_pos) == 0) {
            // Normal match.
            return wildcard_complete_internal(str + next_wc_char_pos, str_len - next_wc_char_pos,
                                              wc + next_wc_char_pos, wc_len - next_wc_char_pos,
                                              params, flags, out);
        }
        if (wcsncasecmp(str, wc, next_wc_char_pos) == 0) {
            // Case insensitive match.
            return wildcard_complete_internal(str + next_wc_char_pos, str_len - next_wc_char_pos,
                                              wc + next_wc_char_pos, wc_len - next_wc_char_pos,
                                              params, flags | COMPLETE_REPLACES_TOKEN, out);
        }
        return wildcard_result_t::no_match;
    }

    // Our first character is a wildcard.
    assert(next_wc_char_pos == 0);
    switch (wc[0]) {
        case ANY_CHAR: {
            if (str[0] == L'\0') {
                return wildcard_result_t::no_match;
            }
            return wildcard_complete_internal(str + 1, str_len - 1, wc + 1, wc_len - 1, params,
                                              flags, out);
        }
        case ANY_STRING: {
            // Hackish. If this is the last character of the wildcard, then just complete with
            // the empty string. This fixes cases like "f*<tab>" -> "f*o".
            if (wc[1] == L'\0') {
                return wildcard_complete_internal(L"", 0, L"", 0, params, flags, out);
            }

            // Try all submatches. Issue #929: if the recursive call gives us a prefix match,
            // just stop. This is sloppy - what we really want to do is say, once we've seen a
            // match of a particular type, ignore all matches of that type further down the
            // string, such that the wildcard produces the "minimal match.".
            bool has_match = false;
            for (size_t i = 0; str[i] != L'\0'; i++) {
                const size_t before_count = out ? out->size() : 0;
                auto submatch_res = wildcard_complete_internal(str + i, str_len - i, wc + 1,
                                                               wc_len - 1, params, flags, out);
                switch (submatch_res) {
                    case wildcard_result_t::no_match:
                        break;
                    case wildcard_result_t::match:
                        has_match = true;
                        // If out is NULL, we don't care about the actual matches. If out is not
                        // NULL but we have a prefix match, stop there.
                        if (out == nullptr || has_prefix_match(out, before_count)) {
                            return wildcard_result_t::match;
                        }
                        break;
                    case wildcard_result_t::cancel:
                    case wildcard_result_t::overflow:
                        // Note early return.
                        return submatch_res;
                }
            }
            return has_match ? wildcard_result_t::match : wildcard_result_t::no_match;
        }
        case ANY_STRING_RECURSIVE: {
            // We don't even try with this one.
            return wildcard_result_t::no_match;
        }
        default: {
            DIE("unreachable code reached");
        }
    }

    DIE("unreachable code reached");
}

wildcard_result_t wildcard_complete(const wcstring &str, const wchar_t *wc,
                                    const std::function<wcstring(const wcstring &)> &desc_func,
                                    completion_receiver_t *out, expand_flags_t expand_flags,
                                    complete_flags_t flags) {
    // Note out may be NULL.
    assert(wc != nullptr);
    wc_complete_pack_t params(str, desc_func, expand_flags);
    return wildcard_complete_internal(str.c_str(), str.size(), wc, std::wcslen(wc), params, flags,
                                      out, true /* first call */);
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
static const wchar_t *file_get_desc(const wcstring &filename, int lstat_res,
                                    const struct stat &lbuf, int stat_res, const struct stat &buf,
                                    int err, bool definitely_executable) {
    if (lstat_res) {
        return COMPLETE_FILE_DESC;
    }

    if (S_ISLNK(lbuf.st_mode)) {
        if (!stat_res) {
            if (S_ISDIR(buf.st_mode)) {
                return COMPLETE_DIRECTORY_SYMLINK_DESC;
            }
            if (definitely_executable || (buf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH) && waccess(filename, X_OK) == 0)) {
                // Weird group permissions and other such issues make it non-trivial to find out if
                // we can actually execute a file using the result from stat. It is much safer to
                // use the access function, since it tells us exactly what we want to know.
                //
                // We skip this check in case the caller tells us the file is definitely executable.
                return COMPLETE_EXEC_LINK_DESC;
            }

            return COMPLETE_SYMLINK_DESC;
        }

        if (err == ENOENT) return COMPLETE_BROKEN_SYMLINK_DESC;
        if (err == ELOOP) return COMPLETE_LOOP_SYMLINK_DESC;
        // On unknown errors we do nothing. The file will be given the default 'File'
        // description or one based on the suffix.
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
    } else if (definitely_executable || (buf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH) && waccess(filename, X_OK) == 0)) {
        // Weird group permissions and other such issues make it non-trivial to find out if we can
        // actually execute a file using the result from stat. It is much safer to use the access
        // function, since it tells us exactly what we want to know.
        //
        // We skip this check in case the caller tells us the file is definitely executable.
        return COMPLETE_EXEC_DESC;
    }

    return COMPLETE_FILE_DESC;
}

/// Test if the given file is an executable (if executables_only) or directory (if
/// directories_only). If it matches, call wildcard_complete() with some description that we make
/// up. Note that the filename came from a readdir() call, so we know it exists.
static bool wildcard_test_flags_then_complete(const wcstring &filepath, const wcstring &filename,
                                              const wchar_t *wc, expand_flags_t expand_flags,
                                              completion_receiver_t *out, bool known_dir) {
    const bool executables_only = expand_flags & expand_flag::executables_only;
    const bool need_directory = expand_flags & expand_flag::directories_only;
    // Fast path: If we need directories, and we already know it is one,
    // and we don't need to do anything else, just return it.
    // This is a common case for cd completions, and removes the `stat` entirely in case the system
    // supports it.
    if (known_dir && !executables_only && !(expand_flags & expand_flag::gen_descriptions)) {
        return wildcard_complete(filename + L'/', wc, const_desc(L""), out, expand_flags,
                                 COMPLETE_NO_SPACE) == wildcard_result_t::match;
    }
    // Check if it will match before stat().
    if (wildcard_complete(filename, wc, {}, nullptr, expand_flags, 0) != wildcard_result_t::match) {
        return false;
    }

    struct stat lstat_buf = {}, stat_buf = {};
    int stat_res = -1;
    int stat_errno = 0;
    int lstat_res = lwstat(filepath, &lstat_buf);
    if (lstat_res >= 0) {
        if (S_ISLNK(lstat_buf.st_mode)) {
            stat_res = wstat(filepath, &stat_buf);

            if (stat_res < 0) {
                // In order to differentiate between e.g. broken symlinks and symlink loops, we also
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

    if (need_directory && !is_directory) {
        return false;
    }

    if (executables_only && (!is_executable || waccess(filepath, X_OK) != 0)) {
        return false;
    }

    if (executables_only && is_windows_subsystem_for_linux() &&
        string_suffixes_string_case_insensitive(L".dll", filename)) {
        return false;
    }

    // Compute the description.
    wcstring desc;
    if (expand_flags & expand_flag::gen_descriptions) {
        // If we have executables_only, we already checked waccess above,
        // so we tell file_get_desc that this file is definitely executable so it can skip the check.
        desc = file_get_desc(filepath, lstat_res, lstat_buf, stat_res, stat_buf, stat_errno, executables_only);

        if (!is_directory && !is_executable && file_size >= 0) {
            if (!desc.empty()) desc.append(L", ");
            desc.append(format_size(file_size));
        }
    }

    // Append a / if this is a directory. Note this requirement may be the only reason we have to
    // call stat() in some cases.
    auto desc_func = const_desc(desc);
    if (is_directory) {
        return wildcard_complete(filename + L'/', wc, desc_func, out, expand_flags,
                                 COMPLETE_NO_SPACE) == wildcard_result_t::match;
    }
    return wildcard_complete(filename, wc, desc_func, out, expand_flags, 0) ==
           wildcard_result_t::match;
}

namespace {
class wildcard_expander_t {
    // A function to call to check cancellation.
    cancel_checker_t cancel_checker;
    // The working directory to resolve paths against
    const wcstring working_directory;
    // The set of items we have resolved, used to efficiently avoid duplication.
    std::unordered_set<wcstring> completion_set;
    // The set of file IDs we have visited, used to avoid symlink loops.
    std::unordered_set<file_id_t> visited_files;
    // Flags controlling expansion.
    const expand_flags_t flags;
    // Resolved items get inserted into here. This is transient of course.
    completion_receiver_t *resolved_completions;
    // Whether we have been interrupted.
    bool did_interrupt{false};
    // Whether we have overflowed.
    bool did_overflow{false};
    // Whether we have successfully added any completions.
    bool did_add{false};
    // Whether some parent expansion is fuzzy, and therefore completions always prepend their prefix
    // This variable is a little suspicious - it should be passed along, not stored here
    // If we ever try to do parallel wildcard expansion we'll have to remove this
    bool has_fuzzy_ancestor{false};

    /// We are a trailing slash - expand at the end.
    void expand_trailing_slash(const wcstring &base_dir, const wcstring &prefix);

    /// Given a directory base_dir, which is opened as base_dir_iter, expand an intermediate segment
    /// of the wildcard. Treat ANY_STRING_RECURSIVE as ANY_STRING. wc_segment is the wildcard
    /// segment for this directory, wc_remainder is the wildcard for subdirectories,
    /// prefix is the prefix for completions.
    void expand_intermediate_segment(const wcstring &base_dir, dir_iter_t &base_dir_iter,
                                     const wcstring &wc_segment, const wchar_t *wc_remainder,
                                     const wcstring &prefix);

    /// Given a directory base_dir, which is opened as base_dir_fp, expand an intermediate literal
    /// segment. Use a fuzzy matching algorithm.
    void expand_literal_intermediate_segment_with_fuzz(const wcstring &base_dir,
                                                       dir_iter_t &base_dir_iter,
                                                       const wcstring &wc_segment,
                                                       const wchar_t *wc_remainder,
                                                       const wcstring &prefix);

    /// Given a directory base_dir, which is opened as base_dir_iter, expand the last segment of the
    /// wildcard. Treat ANY_STRING_RECURSIVE as ANY_STRING. wc is the wildcard segment to use for
    /// matching, wc_remainder is the wildcard for subdirectories, prefix is the prefix for
    /// completions.
    void expand_last_segment(const wcstring &base_dir, dir_iter_t &base_dir_iter,
                             const wcstring &wc, const wcstring &prefix);

    /// Indicate whether we should cancel wildcard expansion. This latches 'interrupt'.
    bool interrupted_or_overflowed() {
        did_interrupt = did_interrupt || cancel_checker();
        return did_interrupt || did_overflow;
    }

    void add_expansion_result(wcstring &&result) {
        // This function is only for the non-completions case.
        assert(!(this->flags & expand_flag::for_completions));
        if (this->completion_set.insert(result).second) {
            if (!this->resolved_completions->add(std::move(result))) {
                this->did_overflow = true;
            }
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

        // Ensure we don't fall into a symlink loop.
        // Ideally we would compare both devices and inodes, but devices require a stat call, so we
        // use inodes exclusively.
        std::unordered_set<ino_t> visited_inodes;

        for (;;) {
            // We keep track of the single unique_entry entry. If we get more than one, it's not
            // unique and we stop the descent.
            wcstring unique_entry;
            dir_iter_t dir(abs_unique_hierarchy);
            if (!dir.valid()) {
                break;
            }
            while (const auto *entry = dir.next()) {
                if (entry->name.empty() || entry->name.at(0) == L'.') {
                    continue;  // either hidden, or . and .. entries -- skip them
                }
                if (!visited_inodes.insert(entry->inode).second) {
                    // Either we've visited this inode already or there's multiple files;
                    // either way stop.
                    break;
                } else if (entry->is_dir() && unique_entry.empty()) {
                    unique_entry = entry->name;  // first candidate
                } else {
                    // We either have two or more candidates, or the child is not a directory. We're
                    // done.
                    unique_entry.clear();
                    break;
                }
            }

            // We stop if we got two or more entries; also stop if we got zero or were interrupted
            if (unique_entry.empty() || interrupted_or_overflowed()) {
                break;
            }

            // We have an entry in the unique hierarchy!
            append_path_component(unique_hierarchy, unique_entry);
            unique_hierarchy.push_back(L'/');

            append_path_component(abs_unique_hierarchy, unique_entry);
            abs_unique_hierarchy.push_back(L'/');
        }
        return unique_hierarchy;
    }

    void try_add_completion_result(const wcstring &filepath, const wcstring &filename,
                                   const wcstring &wildcard, const wcstring &prefix,
                                   bool known_dir) {
        // This function is only for the completions case.
        assert(this->flags & expand_flag::for_completions);

        wcstring abs_path = this->working_directory;
        append_path_component(abs_path, filepath);

        // We must normalize the path to allow 'cd ..' to operate on logical paths.
        if (flags & expand_flag::special_for_cd) abs_path = normalize_path(abs_path);

        size_t before = this->resolved_completions->size();
        if (wildcard_test_flags_then_complete(abs_path, filename, wildcard.c_str(), this->flags,
                                              this->resolved_completions, known_dir)) {
            // Hack. We added this completion result based on the last component of the wildcard.
            // Prepend our prefix to each wildcard that replaces its token.
            // Note that prepend_token_prefix is a no-op unless COMPLETE_REPLACES_TOKEN is set
            size_t after = this->resolved_completions->size();
            for (size_t i = before; i < after; i++) {
                completion_t *c = &this->resolved_completions->at(i);
                if (this->has_fuzzy_ancestor && !(c->flags & COMPLETE_REPLACES_TOKEN)) {
                    c->flags |= COMPLETE_REPLACES_TOKEN;
                    c->prepend_token_prefix(wildcard);
                }
                c->prepend_token_prefix(prefix);
            }

            // Implement special_for_cd_autosuggestion by descending the deepest unique
            // hierarchy we can, and then appending any components to each new result.
            // Only descend deepest unique for cd autosuggest and not for cd tab completion
            // (issue #4402).
            if (flags & expand_flag::special_for_cd_autosuggestion) {
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
    dir_iter_t open_dir(const wcstring &base_dir, bool dotdot = false) const {
        wcstring path = this->working_directory;
        append_path_component(path, base_dir);
        if (flags & expand_flag::special_for_cd) {
            // cd operates on logical paths.
            // for example, cd ../<tab> should complete "without resolving symlinks".
            path = normalize_path(path);
        }
        return dir_iter_t(path, dotdot);
    }

   public:
    wildcard_expander_t(wcstring wd, expand_flags_t f, cancel_checker_t cancel_checker,
                        completion_receiver_t *r)
        : cancel_checker(std::move(cancel_checker)),
          working_directory(std::move(wd)),
          flags(f),
          resolved_completions(r) {
        assert(resolved_completions != nullptr);

        // Insert initial completions into our set to avoid duplicates.
        for (const auto &resolved_completion : resolved_completions->get_list()) {
            this->completion_set.insert(resolved_completion.completion);
        }
    }

    // Do wildcard expansion. This is recursive.
    void expand(const wcstring &base_dir, const wchar_t *wc, const wcstring &prefix);

    wildcard_result_t status_code() const {
        if (this->did_interrupt) {
            return wildcard_result_t::cancel;
        } else if (this->did_overflow) {
            return wildcard_result_t::overflow;
        }
        return this->did_add ? wildcard_result_t::match : wildcard_result_t::no_match;
    }
};

void wildcard_expander_t::expand_trailing_slash(const wcstring &base_dir, const wcstring &prefix) {
    if (interrupted_or_overflowed()) {
        return;
    }

    if (!(flags & expand_flag::for_completions)) {
        // Trailing slash and not accepting incomplete, e.g. `echo /xyz/`. Insert this file, we already know it exists!
        this->add_expansion_result(wcstring{base_dir});
    } else {
        // Trailing slashes and accepting incomplete, e.g. `echo /xyz/<tab>`. Everything is added.
        dir_iter_t dir = open_dir(base_dir);
        if (dir.valid()) {
            // wreaddir_resolving without the out argument is just wreaddir.
            // So we can use the information in case we need it.
            bool need_dir = flags & expand_flag::directories_only;
            wcstring path = base_dir;
            if (flags & expand_flag::special_for_cd) {
                path = this->working_directory;
                append_path_component(path, base_dir);
                // cd operates on logical paths.
                // for example, cd ../<tab> should complete "without resolving symlinks".
                path = normalize_path(path);
            }
            while (const auto *entry = dir.next()) {
                if (interrupted_or_overflowed()) {
                    break;
                }
                // Note that is_dir() may cause a stat() call.
                bool known_dir = need_dir ? entry->is_dir() : false;
                if (need_dir && !known_dir) continue;
                if (!entry->name.empty() && entry->name.at(0) != L'.') {
                    this->try_add_completion_result(base_dir + entry->name, entry->name, L"",
                                                    prefix, known_dir);
                }
            }
        }
    }
}

void wildcard_expander_t::expand_intermediate_segment(const wcstring &base_dir,
                                                      dir_iter_t &base_dir_iter,
                                                      const wcstring &wc_segment,
                                                      const wchar_t *wc_remainder,
                                                      const wcstring &prefix) {
    std::string narrow;
    const dir_iter_t::entry_t *entry{};
    while (!interrupted_or_overflowed() && (entry = base_dir_iter.next())) {
        // Note that it's critical we ignore leading dots here, else we may descend into . and ..
        if (!wildcard_match(entry->name, wc_segment, true)) {
            // Doesn't match the wildcard for this segment, skip it.
            continue;
        }

        if (!entry->is_dir()) {
            continue;
        }

        auto statbuf = entry->stat();
        if (!statbuf) {
            continue;
        }

        const file_id_t file_id = file_id_t::from_stat(*statbuf);
        if (!this->visited_files.insert(file_id).second) {
            // Symlink loop! This directory was already visited, so skip it.
            continue;
        }

        // We made it through. Perform normal wildcard expansion on this new directory, starting at
        // our tail_wc, which includes the ANY_STRING_RECURSIVE guy.
        wcstring full_path = base_dir + entry->name;
        full_path.push_back(L'/');
        this->expand(full_path, wc_remainder, prefix + wc_segment + L'/');

        // Now remove the visited file. This is for #2414: only directories "beneath" us should be
        // considered visited.
        this->visited_files.erase(file_id);
    }
}

void wildcard_expander_t::expand_literal_intermediate_segment_with_fuzz(const wcstring &base_dir,
                                                                        dir_iter_t &base_dir_iter,
                                                                        const wcstring &wc_segment,
                                                                        const wchar_t *wc_remainder,
                                                                        const wcstring &prefix) {
    // Mark that we are fuzzy for the duration of this function
    const scoped_push<bool> scoped_fuzzy(&this->has_fuzzy_ancestor, true);
    const dir_iter_t::entry_t *entry{};
    while (!interrupted_or_overflowed() && (entry = base_dir_iter.next())) {
        // Don't bother with . and ..
        if (entry->name == L"." || entry->name == L"..") {
            continue;
        }

        const auto match = string_fuzzy_match_string(wc_segment, entry->name);
        if (!match || match->is_samecase_exact()) continue;

        // Note is_dir() may trigger a stat call.
        if (!entry->is_dir()) continue;

        // Determine the effective prefix for our children.
        // Normally this would be the wildcard segment, but here we know our segment doesn't have
        // wildcards ("literal") and we are doing fuzzy expansion, which means we replace the
        // segment with files found through fuzzy matching.
        const wcstring child_prefix = prefix + entry->name + L'/';

        wcstring new_full_path = base_dir + entry->name;
        new_full_path.push_back(L'/');

        // Ok, this directory matches. Recurse to it. Then mark each resulting completion as fuzzy.
        const size_t before = this->resolved_completions->size();
        this->expand(new_full_path, wc_remainder, child_prefix);
        const size_t after = this->resolved_completions->size();

        assert(before <= after);
        for (size_t i = before; i < after; i++) {
            completion_t *c = &this->resolved_completions->at(i);
            // Mark the completion as replacing.
            if (!(c->flags & COMPLETE_REPLACES_TOKEN)) {
                c->flags |= COMPLETE_REPLACES_TOKEN;
                c->prepend_token_prefix(child_prefix);
            }
            // And every match must be made at least as fuzzy as ours.
            // TODO: justify this, tests do not exercise it yet.
            if (match->rank() > c->match.rank()) {
                // Our match is fuzzier.
                c->match = *match;
            }
        }
    }
}

void wildcard_expander_t::expand_last_segment(const wcstring &base_dir, dir_iter_t &base_dir_iter,
                                              const wcstring &wc, const wcstring &prefix) {
    bool is_dir = false;
    bool need_dir = flags & expand_flag::directories_only;

    const dir_iter_t::entry_t *entry{};
    while (!interrupted_or_overflowed() && (entry = base_dir_iter.next())) {
        if (need_dir && !entry->is_dir()) continue;
        if (flags & expand_flag::for_completions) {
            this->try_add_completion_result(base_dir + entry->name, entry->name, wc, prefix,
                                            is_dir);
        } else {
            // Normal wildcard expansion, not for completions.
            if (wildcard_match(entry->name, wc, true /* skip files with leading dots */)) {
                this->add_expansion_result(base_dir + entry->name);
            }
        }
    }
}

/// The real implementation of wildcard expansion is in this function. Other functions are just
/// wrappers around this one.
///
/// This function traverses the relevant directory tree looking for matches, and recurses when
/// needed to handle wildcards spanning multiple components and recursive wildcards.
///
/// Args:
/// base_dir: the "working directory" against which the wildcard is to be resolved
/// wc: the wildcard string itself, e.g. foo*bar/baz (where * is actually ANY_CHAR)
/// effective_prefix: the string that should be prepended for completions that replace their token.
///    This is usually the same thing as the original wildcard, but for fuzzy matching, we
///    expand intermediate segments. effective_prefix is always either empty, or ends with a slash
void wildcard_expander_t::expand(const wcstring &base_dir, const wchar_t *wc,
                                 const wcstring &effective_prefix) {
    assert(wc != nullptr);

    if (interrupted_or_overflowed()) {
        return;
    }

    // Get the current segment and compute interesting properties about it.
    const wchar_t *const next_slash = std::wcschr(wc, L'/');
    const bool is_last_segment = (next_slash == nullptr);
    const size_t wc_segment_len = next_slash ? next_slash - wc : std::wcslen(wc);
    const wcstring wc_segment = wcstring(wc, wc_segment_len);
    const bool segment_has_wildcards = wildcard_has_internal(wc_segment);  // e.g. ANY_STRING.
    const wchar_t *const wc_remainder = next_slash ? next_slash + 1 : nullptr;

    if (wc_segment.empty()) {
        // Handle empty segment.
        assert(!segment_has_wildcards);  //!OCLINT(multiple unary operator)
        if (is_last_segment) {
            this->expand_trailing_slash(base_dir, effective_prefix);
        } else {
            // Multiple adjacent slashes in the wildcard. Just skip them.
            this->expand(base_dir, wc_remainder, effective_prefix + L'/');
        }
    } else if (!segment_has_wildcards && !is_last_segment) {
        // Literal intermediate match. Note that we may not be able to actually read the directory
        // (issue #2099).
        assert(next_slash != nullptr);

        // Absolute path of the intermediate directory
        const wcstring intermediate_dirpath = base_dir + wc_segment + L'/';

        // This just trumps everything.
        size_t before = this->resolved_completions->size();
        this->expand(intermediate_dirpath, wc_remainder, effective_prefix + wc_segment + L'/');

        // Maybe try a fuzzy match (#94) if nothing was found with the literal match. Respect
        // EXPAND_NO_DIRECTORY_ABBREVIATIONS (issue #2413).
        // Don't do fuzzy matches if the literal segment was valid (#3211)
        bool allow_fuzzy = this->flags.get(expand_flag::fuzzy_match) &&
                           !this->flags.get(expand_flag::no_fuzzy_directories);
        if (allow_fuzzy && this->resolved_completions->size() == before &&
            waccess(intermediate_dirpath, F_OK) != 0) {
            assert(this->flags & expand_flag::for_completions);
            dir_iter_t base_dir_iter = open_dir(base_dir);
            if (base_dir_iter.valid()) {
                this->expand_literal_intermediate_segment_with_fuzz(
                    base_dir, base_dir_iter, wc_segment, wc_remainder, effective_prefix);
            }
        }
    } else {
        assert(!wc_segment.empty() && (segment_has_wildcards || is_last_segment));

        if (!is_last_segment && wc_segment == wcstring{ANY_STRING_RECURSIVE}) {
            // Hack for #7222. This is an intermediate wc segment that is exactly **. The
            // tail matches in subdirectories as normal, but also the current directory.
            // That is, '**/bar' may match 'bar' and 'foo/bar'.
            // Implement this by matching the wildcard tail only, in this directory.
            // Note if the segment is not exactly ANY_STRING_RECURSIVE then the segment may only
            // match subdirectories.
            this->expand(base_dir, wc_remainder, effective_prefix);
            if (interrupted_or_overflowed()) {
                return;
            }
        }

        // return "." and ".." entries if we're doing completions
        dir_iter_t dir =
            open_dir(base_dir, /* return . and .. */ flags & expand_flag::for_completions);
        if (dir.valid()) {
            if (is_last_segment) {
                // Last wildcard segment, nonempty wildcard.
                this->expand_last_segment(base_dir, dir, wc_segment, effective_prefix);
            } else {
                // Not the last segment, nonempty wildcard.
                assert(next_slash != nullptr);
                this->expand_intermediate_segment(base_dir, dir, wc_segment, wc_remainder,
                                                  effective_prefix + wc_segment + L'/');
            }

            size_t asr_idx = wc_segment.find(ANY_STRING_RECURSIVE);
            if (asr_idx != wcstring::npos) {
                // Apply the recursive **.
                // Construct a "head + any" wildcard for matching stuff in this directory, and an
                // "any + tail" wildcard for matching stuff in subdirectories. Note that the
                // ANY_STRING_RECURSIVE character is present in both the head and the tail.
                const wcstring head_any(wc_segment, 0, asr_idx + 1);
                const wchar_t *any_tail = wc + asr_idx;
                assert(head_any.at(head_any.size() - 1) == ANY_STRING_RECURSIVE);
                assert(any_tail[0] == ANY_STRING_RECURSIVE);

                dir.rewind();
                this->expand_intermediate_segment(base_dir, dir, head_any, any_tail,
                                                  effective_prefix);
            }
        }
    }
}
}  // namespace

wildcard_result_t wildcard_expand_string(const wcstring &wc, const wcstring &working_directory,
                                         expand_flags_t flags,
                                         const cancel_checker_t &cancel_checker,
                                         completion_receiver_t *output) {
    assert(output != nullptr);
    // Fuzzy matching only if we're doing completions.
    assert(flags.get(expand_flag::for_completions) || !flags.get(expand_flag::fuzzy_match));

    // expand_flag::special_for_cd requires expand_flag::directories_only and
    // expand_flag::for_completions and !expand_flag::gen_descriptions.
    assert(!(flags.get(expand_flag::special_for_cd)) ||
           ((flags.get(expand_flag::directories_only)) &&
            (flags.get(expand_flag::for_completions)) &&
            (!flags.get(expand_flag::gen_descriptions))));

    // Hackish fix for issue #1631. We are about to call c_str(), which will produce a string
    // truncated at any embedded nulls. We could fix this by passing around the size, etc. However
    // embedded nulls are never allowed in a filename, so we just check for them and return 0 (no
    // matches) if there is an embedded null.
    if (wc.find(L'\0') != wcstring::npos) {
        return wildcard_result_t::no_match;
    }

    // We do not support tab-completing recursive (**) wildcards. This is historic behavior.
    // Do not descend any directories if there is a ** wildcard.
    if (flags.get(expand_flag::for_completions) &&
        wc.find(ANY_STRING_RECURSIVE) != wcstring::npos) {
        return wildcard_result_t::no_match;
    }

    // Compute the prefix and base dir. The prefix is what we prepend for filesystem operations
    // (i.e. the working directory), the base_dir is the part of the wildcard consumed thus far,
    // which we also have to append. The difference is that the base_dir is returned as part of the
    // expansion, and the prefix is not.
    //
    // Check for a leading slash. If we find one, we have an absolute path: the prefix is empty, the
    // base dir is /, and the wildcard is the remainder. If we don't find one, the prefix is the
    // working directory, the base dir is empty.
    wcstring prefix, base_dir, effective_wc;
    if (string_prefixes_string(L"/", wc)) {
        base_dir = L"/";
        effective_wc = wc.substr(1);
    } else {
        prefix = working_directory;
        effective_wc = wc;
    }

    wildcard_expander_t expander(prefix, flags, cancel_checker, output);
    expander.expand(base_dir, effective_wc.c_str(), base_dir);
    return expander.status_code();
}
