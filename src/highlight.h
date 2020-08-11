// Prototypes for functions for syntax highlighting.
#ifndef FISH_HIGHLIGHT_H
#define FISH_HIGHLIGHT_H

#include <stddef.h>
#include <stdint.h>

#include <unordered_map>
#include <vector>

#include "color.h"
#include "common.h"
#include "env.h"

/// Describes the role of a span of text.
enum class highlight_role_t : uint8_t {
    normal = 0,            // normal text
    error,                 // error
    command,               // command
    statement_terminator,  // process separator
    param,                 // command parameter (argument)
    comment,               // comment
    search_match,          // search match
    operat,                // operator
    escape,                // escape sequences
    quote,                 // quoted string
    redirection,           // redirection
    autosuggestion,        // autosuggestion
    selection,

    // Pager support.
    // NOTE: pager.cpp relies on these being in this order.
    pager_progress,
    pager_background,
    pager_prefix,
    pager_completion,
    pager_description,
    pager_secondary_background,
    pager_secondary_prefix,
    pager_secondary_completion,
    pager_secondary_description,
    pager_selected_background,
    pager_selected_prefix,
    pager_selected_completion,
    pager_selected_description,
};

/// Simply value type describing how a character should be highlighted..
struct highlight_spec_t {
    highlight_role_t foreground{highlight_role_t::normal};
    highlight_role_t background{highlight_role_t::normal};
    bool valid_path{false};
    bool force_underline{false};

    highlight_spec_t() = default;

    /* implicit */ highlight_spec_t(highlight_role_t fg,
                                    highlight_role_t bg = highlight_role_t::normal)
        : foreground(fg), background(bg) {}

    bool operator==(const highlight_spec_t &rhs) const {
        return foreground == rhs.foreground && background == rhs.background &&
               valid_path == rhs.valid_path && force_underline == rhs.force_underline;
    }

    bool operator!=(const highlight_spec_t &rhs) const { return !(*this == rhs); }

    static highlight_spec_t make_background(highlight_role_t bg_role) {
        return highlight_spec_t{highlight_role_t::normal, bg_role};
    }
};

namespace std {
template <>
struct hash<highlight_spec_t> {
    std::size_t operator()(const highlight_spec_t &v) const {
        size_t vals[4] = {static_cast<uint32_t>(v.foreground), static_cast<uint32_t>(v.background),
                          v.valid_path, v.force_underline};
        return (vals[0] << 0) + (vals[1] << 6) + (vals[2] << 12) + (vals[3] << 18);
    }
};
}  // namespace std

class history_item_t;
class operation_context_t;

/// Given a string and list of colors of the same size, return the string with ANSI escape sequences
/// representing the colors.
std::string colorize(const wcstring &text, const std::vector<highlight_spec_t> &colors,
                     const environment_t &vars);

/// Perform syntax highlighting for the shell commands in buff. The result is stored in the color
/// array as a color_code from the HIGHLIGHT_ enum for each character in buff.
///
/// \param buffstr The buffer on which to perform syntax highlighting
/// \param color The array in which to store the color codes. The first 8 bits are used for fg
/// color, the next 8 bits for bg color.
/// \param ctx The variables and cancellation check for this operation.
/// \param io_ok If set, allow IO which may block. This means that e.g. invalid commands may be
/// detected.
void highlight_shell(const wcstring &buffstr, std::vector<highlight_spec_t> &color,
                     const operation_context_t &ctx, bool io_ok = false);

/// highlight_color_resolver_t resolves highlight specs (like "a command") to actual RGB colors.
/// It maintains a cache with no invalidation mechanism. The lifetime of these should typically be
/// one screen redraw.
struct highlight_color_resolver_t {
    /// \return an RGB color for a given highlight spec.
    rgb_color_t resolve_spec(const highlight_spec_t &highlight, bool is_background,
                             const environment_t &vars);

   private:
    std::unordered_map<highlight_spec_t, rgb_color_t> fg_cache_;
    std::unordered_map<highlight_spec_t, rgb_color_t> bg_cache_;
    rgb_color_t resolve_spec_uncached(const highlight_spec_t &highlight, bool is_background,
                                      const environment_t &vars) const;
};

/// Given a command 'str' from the history, try to determine whether we ought to suggest it by
/// specially recognizing the command. Returns true if we validated the command. If so, returns by
/// reference whether the suggestion is valid or not.
bool autosuggest_validate_from_history(const history_item_t &item,
                                       const wcstring &working_directory,
                                       const operation_context_t &ctx);

// Tests whether the specified string cpath is the prefix of anything we could cd to. directories is
// a list of possible parent directories (typically either the working directory, or the cdpath).
// This does I/O!
//
// This is used only internally to this file, and is exposed only for testing.
enum {
    // The path must be to a directory.
    PATH_REQUIRE_DIR = 1 << 0,
    // Expand any leading tilde in the path.
    PATH_EXPAND_TILDE = 1 << 1,
    // Normalize directories before resolving, as "cd".
    PATH_FOR_CD = 1 << 2,
};
typedef unsigned int path_flags_t;
bool is_potential_path(const wcstring &potential_path_fragment, const wcstring_list_t &directories,
                       const operation_context_t &ctx, path_flags_t flags);

#endif
