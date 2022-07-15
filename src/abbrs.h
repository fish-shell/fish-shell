// Support for abbreviations.
//
#ifndef FISH_ABBRS_H
#define FISH_ABBRS_H

#include <unordered_map>
#include <unordered_set>

#include "common.h"
#include "maybe.h"

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

    // Replacement string.
    wcstring replacement{};

    // Expansion position.
    abbrs_position_t position{abbrs_position_t::command};

    // Mark if we came from a universal variable.
    bool from_universal{};

    // \return true if this is a regex abbreviation.
    bool is_regex() const { return false; }

    explicit abbreviation_t(wcstring name, wcstring replacement,
                            abbrs_position_t position = abbrs_position_t::command,
                            bool from_universal = false);

    abbreviation_t() = default;
};

class abbrs_set_t {
   public:
    /// \return the replacement value for a abbreviation token, if any.
    /// The \p position is given to describe where the token was found.
    maybe_t<wcstring> expand(const wcstring &token, abbrs_position_t position) const;

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

/// \return the replacement value for a abbreviation token, if any, using the global set.
/// The \p position is given to describe where the token was found.
maybe_t<wcstring> abbrs_expand(const wcstring &token, abbrs_position_t position);

#endif
