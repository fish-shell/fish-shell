// Support for abbreviations.
//
#ifndef FISH_ABBRS_H
#define FISH_ABBRS_H

#include <unordered_map>

#include "common.h"
#include "maybe.h"

/// Controls where in the command line abbreviations may expand.
enum class abbrs_position_t : uint8_t {
    command,   // expand in command position
    anywhere,  // expand in any token
};

struct abbreviation_t {
    // Replacement string.
    wcstring replacement{};

    // Expansion position.
    abbrs_position_t position{abbrs_position_t::command};

    // Mark if we came from a universal variable.
    bool from_universal{};

    // A monotone key to allow reconstructing definition order.
    uint64_t order{};

    explicit abbreviation_t(wcstring replacement,
                            abbrs_position_t position = abbrs_position_t::command,
                            bool from_universal = false);

    abbreviation_t() = default;
};

using abbrs_map_t = std::unordered_map<wcstring, abbreviation_t>;

/// \return the mutable map of abbreviations, keyed by name.
acquired_lock<abbrs_map_t> abbrs_get_map();

/// \return the replacement value for a abbreviation token, if any.
/// The \p position is given to describe where the token was found.
maybe_t<wcstring> abbrs_expand(const wcstring &token, abbrs_position_t position);

/// \return the list of abbreviation keys.
wcstring_list_t abbrs_get_keys();

/// Import any abbreviations from universal variables.
class env_var_t;
void abbrs_import_from_uvars(const std::unordered_map<wcstring, env_var_t> &uvars);

#endif
