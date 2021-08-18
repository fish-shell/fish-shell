// Helper functions for working with wcstring.
#include "config.h"  // IWYU pragma: keep

#include "wcstringutil.h"

#include <wctype.h>

#include <locale>

#include "common.h"
#include "flog.h"

wcstring truncate(const wcstring &input, int max_len, ellipsis_type etype) {
    if (input.size() <= static_cast<size_t>(max_len)) {
        return input;
    }

    if (etype == ellipsis_type::None) {
        return input.substr(0, max_len);
    }
    if (etype == ellipsis_type::Prettiest) {
        const wchar_t *ellipsis_str = get_ellipsis_str();
        return input.substr(0, max_len - std::wcslen(ellipsis_str)).append(ellipsis_str);
    }
    wcstring output = input.substr(0, max_len - 1);
    output.push_back(get_ellipsis_char());
    return output;
}

wcstring trim(wcstring input) { return trim(std::move(input), L"\t\v \r\n"); }

wcstring trim(wcstring input, const wchar_t *any_of) {
    wcstring result = std::move(input);
    size_t suffix = result.find_last_not_of(any_of);
    if (suffix == wcstring::npos) {
        return wcstring{};
    }
    result.erase(suffix + 1);

    auto prefix = result.find_first_not_of(any_of);
    assert(prefix != wcstring::npos && "Should have one non-trimmed character");
    result.erase(0, prefix);
    return result;
}

wcstring wcstolower(wcstring input) {
    wcstring result = std::move(input);
    std::transform(result.begin(), result.end(), result.begin(), towlower);
    return result;
}

size_t count_preceding_backslashes(const wcstring &text, size_t idx) {
    assert(idx <= text.size() && "Out of bounds");
    size_t backslashes = 0;
    while (backslashes < idx && text.at(idx - backslashes - 1) == L'\\') {
        backslashes++;
    }
    return backslashes;
}

bool string_prefixes_string(const wchar_t *proposed_prefix, const wcstring &value) {
    return string_prefixes_string(proposed_prefix, value.c_str());
}

bool string_prefixes_string(const wcstring &proposed_prefix, const wcstring &value) {
    size_t prefix_size = proposed_prefix.size();
    return prefix_size <= value.size() && value.compare(0, prefix_size, proposed_prefix) == 0;
}

bool string_prefixes_string(const wchar_t *proposed_prefix, const wchar_t *value) {
    for (size_t idx = 0; proposed_prefix[idx] != L'\0'; idx++) {
        // Note if the prefix is longer than value, then we will compare a nonzero prefix character
        // against a zero value character, and so we'll return false;
        if (proposed_prefix[idx] != value[idx]) return false;
    }
    // We must have that proposed_prefix[idx] == L'\0', so we have a prefix match.
    return true;
}

bool string_prefixes_string(const char *proposed_prefix, const std::string &value) {
    return string_prefixes_string(proposed_prefix, value.c_str());
}

bool string_prefixes_string(const char *proposed_prefix, const char *value) {
    for (size_t idx = 0; proposed_prefix[idx] != L'\0'; idx++) {
        if (proposed_prefix[idx] != value[idx]) return false;
    }
    return true;
}

bool string_prefixes_string_case_insensitive(const wcstring &proposed_prefix,
                                             const wcstring &value) {
    size_t prefix_size = proposed_prefix.size();
    return prefix_size <= value.size() &&
           wcsncasecmp(proposed_prefix.c_str(), value.c_str(), prefix_size) == 0;
}

bool string_suffixes_string(const wcstring &proposed_suffix, const wcstring &value) {
    size_t suffix_size = proposed_suffix.size();
    return suffix_size <= value.size() &&
           value.compare(value.size() - suffix_size, suffix_size, proposed_suffix) == 0;
}

bool string_suffixes_string(const wchar_t *proposed_suffix, const wcstring &value) {
    size_t suffix_size = std::wcslen(proposed_suffix);
    return suffix_size <= value.size() &&
           value.compare(value.size() - suffix_size, suffix_size, proposed_suffix) == 0;
}

bool string_suffixes_string_case_insensitive(const wcstring &proposed_suffix,
                                             const wcstring &value) {
    size_t suffix_size = proposed_suffix.size();
    return suffix_size <= value.size() && wcsncasecmp(value.c_str() + (value.size() - suffix_size),
                                                      proposed_suffix.c_str(), suffix_size) == 0;
}

/// Returns true if needle, represented as a subsequence, is contained within haystack.
/// Note subsequence is not substring: "foo" is a subsequence of "follow" for example.
static bool subsequence_in_string(const wcstring &needle, const wcstring &haystack) {
    // Impossible if haystack is larger than string.
    if (haystack.size() > haystack.size()) {
        return false;
    }

    // Empty strings are considered to be subsequences of everything.
    if (needle.empty()) {
        return true;
    }

    auto ni = needle.begin();
    for (auto hi = haystack.begin(); ni != needle.end() && hi != haystack.end(); ++hi) {
        if (*ni == *hi) {
            ++ni;
        }
    }
    // We succeeded if we exhausted our sequence.
    assert(ni <= needle.end());
    return ni == needle.end();
}

// static
maybe_t<string_fuzzy_match_t> string_fuzzy_match_t::try_create(const wcstring &string,
                                                               const wcstring &match_against,
                                                               bool anchor_start) {
    // Helper to lazily compute if case insensitive matches should use icase or smartcase.
    // Use icase if the input contains any uppercase characters, smartcase otherwise.
    auto get_case_fold = [&] {
        for (wchar_t c : string) {
            if (towlower(c) != static_cast<wint_t>(c)) return case_fold_t::icase;
        }
        return case_fold_t::smartcase;
    };

    // A string cannot fuzzy match against a shorter string.
    if (string.size() > match_against.size()) {
        return none();
    }

    // exact samecase
    if (string == match_against) {
        return string_fuzzy_match_t{contain_type_t::exact, case_fold_t::samecase};
    }

    // prefix samecase
    if (string_prefixes_string(string, match_against)) {
        return string_fuzzy_match_t{contain_type_t::prefix, case_fold_t::samecase};
    }

    // exact icase
    if (wcscasecmp(string.c_str(), match_against.c_str()) == 0) {
        return string_fuzzy_match_t{contain_type_t::exact, get_case_fold()};
    }

    // prefix icase
    if (string_prefixes_string_case_insensitive(string, match_against)) {
        return string_fuzzy_match_t{contain_type_t::prefix, get_case_fold()};
    }

    // If anchor_start is set, this is as far as we go.
    if (anchor_start) {
        return none();
    }

    // substr samecase
    if (match_against.find(string) != wcstring::npos) {
        return string_fuzzy_match_t{contain_type_t::substr, case_fold_t::samecase};
    }

    // substr icase
    if (ifind(match_against, string, true /* fuzzy */) != wcstring::npos) {
        return string_fuzzy_match_t{contain_type_t::substr, get_case_fold()};
    }

    // subseq samecase
    if (subsequence_in_string(string, match_against)) {
        return string_fuzzy_match_t{contain_type_t::subseq, case_fold_t::samecase};
    }

    // We do not currently test subseq icase.
    return none();
}

uint32_t string_fuzzy_match_t::rank() const {
    // Combine our type and our case fold into a single number, such that better matches are
    // smaller. Treat 'exact' types the same as 'prefix' types; this is because we do not
    // prefer exact matches to prefix matches when presenting completions to the user.
    // Treat smartcase the same as samecase; see #3978.
    auto effective_type = (type == contain_type_t::exact ? contain_type_t::prefix : type);
    auto effective_case = (case_fold == case_fold_t::smartcase ? case_fold_t::samecase : case_fold);

    // Type dominates fold.
    return static_cast<uint32_t>(effective_type) * 8 + static_cast<uint32_t>(effective_case);
}

template <bool Fuzzy, typename T>
size_t ifind_impl(const T &haystack, const T &needle) {
    using char_t = typename T::value_type;
    std::locale locale;

    auto ieq = [&locale](char_t c1, char_t c2) {
        if (c1 == c2 || std::toupper(c1, locale) == std::toupper(c2, locale)) return true;

        // In fuzzy matching treat treat `-` and `_` as equal (#3584).
        if (Fuzzy) {
            if ((c1 == '-' || c1 == '_') && (c2 == '-' || c2 == '_')) return true;
        }
        return false;
    };

    auto result = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(), ieq);
    if (result != haystack.end()) {
        return result - haystack.begin();
    }
    return T::npos;
}

size_t ifind(const wcstring &haystack, const wcstring &needle, bool fuzzy) {
    return fuzzy ? ifind_impl<true>(haystack, needle) : ifind_impl<false>(haystack, needle);
}

size_t ifind(const std::string &haystack, const std::string &needle, bool fuzzy) {
    return fuzzy ? ifind_impl<true>(haystack, needle) : ifind_impl<false>(haystack, needle);
}

wcstring_list_t split_string(const wcstring &val, wchar_t sep) {
    wcstring_list_t out;
    size_t pos = 0, end = val.size();
    while (pos <= end) {
        size_t next_pos = val.find(sep, pos);
        if (next_pos == wcstring::npos) {
            next_pos = end;
        }
        out.emplace_back(val, pos, next_pos - pos);
        pos = next_pos + 1;  // skip the separator, or skip past the end
    }
    return out;
}

wcstring_list_t split_string_tok(const wcstring &val, const wcstring &seps, size_t max_results) {
    wcstring_list_t out;
    size_t end = val.size();
    size_t pos = 0;
    while (pos < end && out.size() + 1 < max_results) {
        // Skip leading seps.
        pos = val.find_first_not_of(seps, pos);
        if (pos == wcstring::npos) break;

        // Find next sep.
        size_t next_sep = val.find_first_of(seps, pos);
        if (next_sep == wcstring::npos) {
            next_sep = end;
        }
        out.emplace_back(val, pos, next_sep - pos);
        // Note we skip exactly one sep here. This is because on the last iteration we retain all
        // but the first leading separators. This is historical.
        pos = next_sep + 1;
    }
    if (pos < end && max_results > 0) {
        assert(out.size() + 1 == max_results && "Should have split the max");
        out.emplace_back(val, pos);
    }
    assert(out.size() <= max_results && "Got too many results");
    return out;
}

wcstring join_strings(const wcstring_list_t &vals, wchar_t sep) {
    if (vals.empty()) return wcstring{};

    // Reserve the size we will need.
    // count-1 separators, plus the length of all strings.
    size_t size = vals.size() - 1;
    for (const wcstring &s : vals) {
        size += s.size();
    }

    // Construct the string.
    wcstring result;
    result.reserve(size);
    bool first = true;
    for (const wcstring &s : vals) {
        if (!first) {
            result.push_back(sep);
        }
        result.append(s);
        first = false;
    }
    return result;
}

void wcs2string_bad_char(wchar_t wc) {
    FLOGF(char_encoding, L"Wide character U+%4X has no narrow representation", wc);
}
