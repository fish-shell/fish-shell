#include "config.h"  // IWYU pragma: keep

#include "re.h"

#include <algorithm>
#include <cstdint>

#include "flog.h"

#define PCRE2_CODE_UNIT_WIDTH WCHAR_T_BITS
#ifdef _WIN32
#define PCRE2_STATIC
#endif

#include "pcre2.h"

using namespace re;
using namespace re::adapters;

void bytecode_deleter_t::operator()(const void *ptr) {
    if (ptr) {
        pcre2_code_free(static_cast<pcre2_code *>(const_cast<void *>(ptr)));
    }
}

void match_data_deleter_t::operator()(void *ptr) {
    if (ptr) {
        pcre2_match_data_free(static_cast<pcre2_match_data *>(ptr));
    }
}

// Get underlying pcre2_code from a bytecode_ptr_t.
const pcre2_code *get_code(const bytecode_ptr_t &ptr) {
    assert(ptr && "Null pointer");
    return static_cast<const pcre2_code *>(ptr.get());
}

// Get underlying match_data_t.
pcre2_match_data *get_md(const match_data_ptr_t &ptr) {
    assert(ptr && "Null pointer");
    return static_cast<pcre2_match_data *>(ptr.get());
}

// Convert a wcstring to a PCRE2_SPTR.
PCRE2_SPTR to_sptr(const wcstring &str) { return reinterpret_cast<PCRE2_SPTR>(str.c_str()); }

/// \return a message for an error code.
static wcstring message_for_code(error_code_t code) {
    wchar_t buf[128] = {};
    pcre2_get_error_message(code, reinterpret_cast<PCRE2_UCHAR *>(buf),
                            sizeof(buf) / sizeof(wchar_t));
    return buf;
}

maybe_t<regex_t> regex_t::try_compile(const wcstring &pattern, const flags_t &flags,
                                      re_error_t *error) {
    // Disable some sequences that can lead to security problems.
    uint32_t options = PCRE2_NEVER_UTF;
#if PCRE2_CODE_UNIT_WIDTH < 32
    options |= PCRE2_NEVER_BACKSLASH_C;
#endif
    if (flags.icase) options |= PCRE2_CASELESS;

    error_code_t err_code = 0;
    PCRE2_SIZE err_offset = 0;
    pcre2_code *code =
        pcre2_compile(to_sptr(pattern), pattern.size(), options, &err_code, &err_offset, nullptr);
    if (!code) {
        if (error) {
            error->code = err_code;
            error->offset = err_offset;
        }
        return none();
    }
    return regex_t{bytecode_ptr_t(code)};
}

match_data_t regex_t::prepare() const {
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(get_code(code_), nullptr);
    // Bogus assertion for memory exhaustion.
    if (unlikely(!md)) {
        DIE("Out of memory");
    }
    return match_data_t{match_data_ptr_t(static_cast<void *>(md))};
}

void match_data_t::reset() {
    start_offset = 0;
    max_capture = 0;
    last_empty = false;
}

maybe_t<match_range_t> regex_t::match(match_data_t &md, const wcstring &subject) const {
    pcre2_match_data *const match_data = get_md(md.data);
    assert(match_data && "Invalid match data");

    // Handle exhausted matches.
    if (md.start_offset > subject.size() || (md.last_empty && md.start_offset == subject.size())) {
        md.max_capture = 0;
        return none();
    }
    PCRE2_SIZE start_offset = md.start_offset;

    // See pcre2demo.c for an explanation of this logic.
    uint32_t options = md.last_empty ? PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED : 0;
    error_code_t code = pcre2_match(get_code(code_), to_sptr(subject), subject.size(), start_offset,
                                    options, match_data, nullptr);
    if (code == PCRE2_ERROR_NOMATCH && !md.last_empty) {
        // Failed to match.
        md.start_offset = subject.size();
        md.max_capture = 0;
        return none();
    } else if (code == PCRE2_ERROR_NOMATCH && md.last_empty) {
        // Failed to find a non-empty-string match at a point where there was a previous
        // empty-string match. Advance by one character and try again.
        md.start_offset += 1;
        md.last_empty = false;
        return this->match(md, subject);
    } else if (code < 0) {
        FLOG(error, "pcre2_match unexpected error:", message_for_code(code));
        return none();
    }

    // Match succeeded.
    // Start at end of previous match, marking if it was empty.
    const auto *ovector = pcre2_get_ovector_pointer(match_data);
    md.start_offset = ovector[1];
    md.max_capture = static_cast<size_t>(code);
    md.last_empty = ovector[0] == ovector[1];
    return match_range_t{ovector[0], ovector[1]};
}

maybe_t<match_range_t> regex_t::match(const wcstring &subject) const {
    match_data_t md = this->prepare();
    return this->match(md, subject);
}

maybe_t<match_range_t> regex_t::group(const match_data_t &md, size_t group_idx) const {
    if (group_idx >= md.max_capture || group_idx >= pcre2_get_ovector_count(get_md(md.data))) {
        return none();
    }

    const PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(get_md(md.data));
    PCRE2_SIZE start = ovector[2 * group_idx];
    PCRE2_SIZE end = ovector[2 * group_idx + 1];
    if (start == PCRE2_UNSET || end == PCRE2_UNSET) {
        return none();
    }
    // From PCRE2 docs: "Note that when a pattern such as (?=ab\K) matches, the reported start of
    // the match can be greater than the end of the match."
    // Saturate the end.
    end = std::max(start, end);
    return match_range_t{start, end};
}

maybe_t<match_range_t> regex_t::group(const match_data_t &match_data, const wcstring &name) const {
    const auto *pcname = to_sptr(name);
    // Beware, pcre2_substring_copy_byname and pcre2_substring_copy_bynumber both have a bug
    // on at least one Ubuntu (running PCRE2) where it outputs garbage for the first character.
    // Read out from the ovector directly.
    int num = pcre2_substring_number_from_name(get_code(code_), pcname);
    if (num <= 0) {
        return none();
    }
    return this->group(match_data, static_cast<size_t>(num));
}

static maybe_t<wcstring> range_to_substr(const wcstring &subject, maybe_t<match_range_t> range) {
    if (!range) {
        return none();
    }
    assert(range->begin <= range->end && range->end <= subject.size() && "Invalid range");
    return subject.substr(range->begin, range->end - range->begin);
}

maybe_t<wcstring> regex_t::substring_for_group(const match_data_t &md, size_t group_idx,
                                               const wcstring &subject) const {
    return range_to_substr(subject, this->group(md, group_idx));
}

maybe_t<wcstring> regex_t::substring_for_group(const match_data_t &md, const wcstring &name,
                                               const wcstring &subject) const {
    return range_to_substr(subject, this->group(md, name));
}

size_t regex_t::capture_group_count() const {
    uint32_t count{};
    pcre2_pattern_info(get_code(code_), PCRE2_INFO_CAPTURECOUNT, &count);
    return count;
}

wcstring_list_t regex_t::capture_group_names() const {
    PCRE2_SPTR name_table{};
    uint32_t name_entry_size{};
    uint32_t name_count{};

    const auto *code = get_code(code_);
    pcre2_pattern_info(code, PCRE2_INFO_NAMETABLE, &name_table);
    pcre2_pattern_info(code, PCRE2_INFO_NAMEENTRYSIZE, &name_entry_size);
    pcre2_pattern_info(code, PCRE2_INFO_NAMECOUNT, &name_count);

    struct name_table_entry_t {
#if PCRE2_CODE_UNIT_WIDTH == 8
        uint8_t match_index_msb;
        uint8_t match_index_lsb;
#if CHAR_BIT == PCRE2_CODE_UNIT_WIDTH
        char name[];
#else
        char8_t name[];
#endif
#elif PCRE2_CODE_UNIT_WIDTH == 16
        uint16_t match_index;
#if WCHAR_T_BITS == PCRE2_CODE_UNIT_WIDTH
        wchar_t name[];
#else
        char16_t name[];
#endif
#else
        uint32_t match_index;
#if WCHAR_T_BITS == PCRE2_CODE_UNIT_WIDTH
        wchar_t name[];
#else
        char32_t name[];
#endif  // WCHAR_T_BITS
#endif  // PCRE2_CODE_UNIT_WIDTH
    };

    const auto *names = reinterpret_cast<const name_table_entry_t *>(name_table);
    wcstring_list_t result;
    result.reserve(name_count);
    for (uint32_t i = 0; i < name_count; ++i) {
        const auto &name_entry = names[i * name_entry_size];
        result.emplace_back(name_entry.name);
    }
    return result;
}

maybe_t<wcstring> regex_t::substitute(const wcstring &subject, const wcstring &replacement,
                                      sub_flags_t flags, size_t start_idx, re_error_t *out_error,
                                      int *out_repl_count) const {
    constexpr size_t stack_bufflen = 256;
    wchar_t buffer[stack_bufflen];

    // SUBSTITUTE_GLOBAL means more than one substitution happens.
    uint32_t options = PCRE2_SUBSTITUTE_UNSET_EMPTY        // don't error on unmatched
                       | PCRE2_SUBSTITUTE_OVERFLOW_LENGTH  // return required length on overflow
                       | (flags.global ? PCRE2_SUBSTITUTE_GLOBAL : 0)      // replace multiple
                       | (flags.extended ? PCRE2_SUBSTITUTE_EXTENDED : 0)  // backslash escapes
        ;
    size_t bufflen = stack_bufflen;
    error_code_t rc =
        pcre2_substitute(get_code(code_), to_sptr(subject), subject.size(), start_idx, options,
                         nullptr /* match_data */, nullptr /* context */, to_sptr(replacement),
                         // (not using UCHAR32 here for cygwin's benefit)
                         replacement.size(), reinterpret_cast<PCRE2_UCHAR *>(buffer), &bufflen);

    if (out_repl_count) {
        *out_repl_count = std::max(rc, 0);
    }
    if (rc == 0) {
        // No replacements.
        return subject;
    } else if (rc > 0) {
        // Some replacement which fit in our buffer.
        // Note we may have had embedded nuls.
        assert(bufflen <= stack_bufflen && "bufflen should not exceed buffer size");
        return wcstring(buffer, bufflen);
    } else if (rc == PCRE2_ERROR_NOMEMORY) {
        // bufflen has been updated to required buffer size.
        // Try again with a real string.
        wcstring res(bufflen, L'\0');
        rc = pcre2_substitute(get_code(code_), to_sptr(subject), subject.size(), start_idx, options,
                              nullptr /* match_data */, nullptr /* context */, to_sptr(replacement),
                              replacement.size(), reinterpret_cast<PCRE2_UCHAR32 *>(&res[0]),
                              &bufflen);
        if (out_repl_count) {
            *out_repl_count = std::max(rc, 0);
        }
        if (rc >= 0) {
            res.resize(bufflen);
            return res;
        }
    }
    // Some error. The offset may be returned in the bufflen.
    if (out_error) {
        out_error->code = rc;
        out_error->offset = (bufflen == PCRE2_UNSET ? 0 : bufflen);
    }
    return none();
}

regex_t::regex_t(adapters::bytecode_ptr_t &&code) : code_(std::move(code)) {
    assert(code_ && "Null impl");
}

wcstring re_error_t::message() const { return message_for_code(this->code); }

wcstring re::make_anchored(wcstring pattern) {
    // PATTERN -> ^(:?PATTERN)$.
    const wchar_t *prefix = L"^(?:";
    const wchar_t *suffix = L")$";
    pattern.reserve(pattern.size() + wcslen(prefix) + wcslen(suffix));
    pattern.insert(0, prefix);
    pattern.append(suffix);
    return pattern;
}
