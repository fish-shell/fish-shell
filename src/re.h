// Wraps PCRE2.
#ifndef FISH_RE_H
#define FISH_RE_H

#include <cstddef>
#include <memory>
#include <utility>

#include "common.h"
#include "maybe.h"

namespace re {

namespace adapters {
// Adapter to store pcre2_code in unique_ptr.
struct bytecode_deleter_t {
    void operator()(const void *);
};
using bytecode_ptr_t = std::unique_ptr<const void, bytecode_deleter_t>;

// Adapter to store pcre2_match_data in unique_ptr.
struct match_data_deleter_t {
    void operator()(void *);
};
using match_data_ptr_t = std::unique_ptr<void, match_data_deleter_t>;
}  // namespace adapters

/// Error code type alias.
using error_code_t = int;

/// Flags for compiling a regex.
struct flags_t {
    bool icase{};  // ignore case?
};

/// Flags for substituting a regex.
struct sub_flags_t {
    bool global{};    // perform multiple substitutions?
    bool extended{};  // apply PCRE2 extended backslash escapes?
};

/// A type wrapping up error information.
/// Beware, GNU defines error_t; hence we use an re_ prefix again.
struct re_error_t {
    error_code_t code{};  // error code
    size_t offset{};      // offset of the error in the pattern

    /// \return our error message.
    wcstring message() const;
};

/// A half-open range of a subject which matched.
struct match_range_t {
    size_t begin;
    size_t end;

    bool operator==(match_range_t rhs) const { return begin == rhs.begin && end == rhs.end; }
    bool operator!=(match_range_t rhs) const { return !(*this == rhs); }
};

/// A match data is the "stateful" object, storing string indices for where to start the next match,
/// capture results, etc. Create one via regex_t::prepare(). These are tied to the regex which
/// created them.
class match_data_t : noncopyable_t {
   public:
    match_data_t(match_data_t &&) = default;
    match_data_t &operator=(match_data_t &&) = default;
    ~match_data_t() = default;

    /// \return a "count" of the number of capture groups which matched.
    /// This is really one more than the highest matching group.
    /// 0 is considered a "group" for the entire match, so this will always return at least 1 for a
    /// successful match.
    size_t matched_capture_group_count() const { return max_capture; }

    /// Reset this data, as if this were freshly issued by a call to prepare().
    void reset();

   private:
    explicit match_data_t(adapters::match_data_ptr_t &&data) : data(std::move(data)) {}

    // Next start position. This may exceed the needle length, which indicates exhaustion.
    size_t start_offset{0};

    // One more than the highest numbered capturing pair that was set (e.g. 1 if no captures).
    size_t max_capture{0};

    // If set, the last match was empty.
    bool last_empty{false};

    // Underlying pcre2_match_data.
    adapters::match_data_ptr_t data{};

    friend class regex_t;
};

/// The compiled form of a PCRE2 regex.
/// This is thread safe.
class regex_t : noncopyable_t {
   public:
    /// Compile a pattern into a regex. \return the resulting regex, or none on error.
    /// If \p error is not null, populate it with the error information.
    static maybe_t<regex_t> try_compile(const wcstring &pattern, const flags_t &flags = flags_t{},
                                        re_error_t *out_error = nullptr);

    /// Create a match data for this regex.
    /// The result is tied to this regex; it should not be used for others.
    match_data_t prepare() const;

    /// Match against a string \p subject, populating \p md.
    /// \return a range on a successful match, none on no match.
    maybe_t<match_range_t> match(match_data_t &md, const wcstring &subject) const;

    /// A convenience function which calls prepare() for you.
    maybe_t<match_range_t> match(const wcstring &subject) const;

    /// A convenience function which calls prepare() for you.
    bool matches_ffi(const wcstring &subject) const;

    /// \return the matched range for an indexed or named capture group. 0 means the entire match.
    maybe_t<match_range_t> group(const match_data_t &md, size_t group_idx) const;
    maybe_t<match_range_t> group(const match_data_t &md, const wcstring &name) const;

    /// \return the matched substring for a capture group.
    maybe_t<wcstring> substring_for_group(const match_data_t &md, size_t group_idx,
                                          const wcstring &subject) const;
    maybe_t<wcstring> substring_for_group(const match_data_t &md, const wcstring &name,
                                          const wcstring &subject) const;

    /// \return the number of indexed capture groups.
    size_t capture_group_count() const;

    /// \return the list of capture group names.
    /// Note PCRE provides these in sorted order, not specification order.
    wcstring_list_t capture_group_names() const;

    /// Search \p subject for matches for this regex, starting at \p start_idx, and replacing them
    /// with \p replacement. If \p repl_count is not null, populate it with the number of
    /// replacements which occurred. This may fail for e.g. bad escapes in the replacement string.
    maybe_t<wcstring> substitute(const wcstring &subject, const wcstring &replacement,
                                 sub_flags_t flags, size_t start_idx = 0,
                                 re_error_t *out_error = nullptr,
                                 int *out_repl_count = nullptr) const;

    regex_t(regex_t &&) = default;
    regex_t &operator=(regex_t &&) = default;
    ~regex_t() = default;

   private:
    regex_t(adapters::bytecode_ptr_t &&);
    adapters::bytecode_ptr_t code_;
};

struct regex_result_ffi {
    std::unique_ptr<re::regex_t> regex;
    re::re_error_t error;

    bool has_error() const;
    std::unique_ptr<re::regex_t> get_regex();
    re::re_error_t get_error() const;
};

regex_result_ffi try_compile_ffi(const wcstring &pattern, const flags_t &flags);

}  // namespace re
#endif
