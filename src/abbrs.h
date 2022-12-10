// Support for abbreviations.
//
#ifndef FISH_ABBRS_H
#define FISH_ABBRS_H

#include <unordered_map>
#include <unordered_set>

#include "common.h"
#include "maybe.h"
#include "parse_constants.h"
#include "re.h"

class env_var_t;

/// Controls where in the command line abbreviations may expand.
enum class abbrs_position_t : uint8_t {
    command,   // expand in command position
    anywhere,  // expand in any token
};

struct abbreviation_t {
    // Abbreviation name. This is unique within the abbreviation set.
    // This is used as the token to match unless we have a regex.
    wcstring name{};

    /// The key (recognized token) - either a literal or a regex pattern.
    wcstring key{};

    /// If set, use this regex to recognize tokens.
    /// If unset, the key is to be interpreted literally.
    /// Note that the fish interface enforces that regexes match the entire token;
    /// we accomplish this by surrounding the regex in ^ and $.
    maybe_t<re::regex_t> regex{};

    /// Replacement string.
    wcstring replacement{};

    /// If set, the replacement is a function name.
    bool replacement_is_function{};

    /// Expansion position.
    abbrs_position_t position{abbrs_position_t::command};

    /// If set, then move the cursor to the first instance of this string in the expansion.
    maybe_t<wcstring> set_cursor_indicator{};

    /// Mark if we came from a universal variable.
    bool from_universal{};

    // \return true if this is a regex abbreviation.
    bool is_regex() const { return this->regex.has_value(); }

    // \return true if we match a token at a given position.
    bool matches(const wcstring &token, abbrs_position_t position) const;

    // Construct from a name, a key which matches a token, a replacement token, a position, and
    // whether we are derived from a universal variable.
    explicit abbreviation_t(wcstring name, wcstring key, wcstring replacement,
                            abbrs_position_t position = abbrs_position_t::command,
                            bool from_universal = false);

    abbreviation_t() = default;

   private:
    // \return if we expand in a given position.
    bool matches_position(abbrs_position_t position) const;
};

/// The result of an abbreviation expansion.
struct abbrs_replacer_t {
    /// The string to use to replace the incoming token, either literal or as a function name.
    wcstring replacement;

    /// If true, treat 'replacement' as the name of a function.
    bool is_function;

    /// If set, the cursor should be moved to the first instance of this string in the expansion.
    maybe_t<wcstring> set_cursor_indicator;
};
using abbrs_replacer_list_t = std::vector<abbrs_replacer_t>;

/// A helper type for replacing a range in a string.
struct abbrs_replacement_t {
    /// The original range of the token in the command line.
    source_range_t range{};

    /// The string to replace with.
    wcstring text{};

    /// The new cursor location, or none to use the default.
    /// This is relative to the original range.
    maybe_t<size_t> cursor{};

    /// Construct a replacement from a replacer.
    /// The \p range is the range of the text matched by the replacer in the command line.
    /// The text is passed in separately as it may be the output of the replacer's function.
    static abbrs_replacement_t from(source_range_t range, wcstring text,
                                    const abbrs_replacer_t &replacer);
};

class abbrs_set_t {
   public:
    /// \return the list of replacers for an input token, in priority order.
    /// The \p position is given to describe where the token was found.
    abbrs_replacer_list_t match(const wcstring &token, abbrs_position_t position) const;

    /// \return whether we would have at least one replacer for a given token.
    bool has_match(const wcstring &token, abbrs_position_t position) const;

    /// Add an abbreviation. Any abbreviation with the same name is replaced.
    void add(abbreviation_t &&abbr);

    /// Rename an abbreviation. This asserts that the old name is used, and the new name is not; the
    /// caller should check these beforehand with has_name().
    void rename(const wcstring &old_name, const wcstring &new_name);

    /// Erase an abbreviation by name.
    /// \return true if erased, false if not found.
    bool erase(const wcstring &name);

    /// \return true if we have an abbreviation with the given name.
    bool has_name(const wcstring &name) const { return used_names_.count(name) > 0; }

    /// \return a reference to the abbreviation list.
    const std::vector<abbreviation_t> &list() const { return abbrs_; }

    /// Import from a universal variable set.
    void import_from_uvars(const std::unordered_map<wcstring, env_var_t> &uvars);

   private:
    /// List of abbreviations, in definition order.
    std::vector<abbreviation_t> abbrs_{};

    /// Set of used abbrevation names.
    /// This is to avoid a linear scan when adding new abbreviations.
    std::unordered_set<wcstring> used_names_;
};

/// \return the global mutable set of abbreviations.
acquired_lock<abbrs_set_t> abbrs_get_set();

/// \return the list of replacers for an input token, in priority order, using the global set.
/// The \p position is given to describe where the token was found.
inline abbrs_replacer_list_t abbrs_match(const wcstring &token, abbrs_position_t position) {
    return abbrs_get_set()->match(token, position);
}

#endif
