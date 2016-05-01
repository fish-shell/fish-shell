// Prototypes for functions for syntax highlighting.
#ifndef FISH_HIGHLIGHT_H
#define FISH_HIGHLIGHT_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <vector>

#include "color.h"
#include "common.h"
#include "env.h"

// Internally, we specify highlight colors using a set of bits. Each highlight_spec is a 32 bit
// uint. We divide this into low 16 (foreground) and high 16 (background). Each half we further
// subdivide into low 8 (primary) and high 8 (modifiers). The primary is not a bitmask; specify
// exactly one. The modifiers are a bitmask; specify any number.
enum {
    // The following values are mutually exclusive; specify at most one.
    highlight_spec_normal = 0,            // normal text
    highlight_spec_error,                 // error
    highlight_spec_command,               // command
    highlight_spec_statement_terminator,  // process separator
    highlight_spec_param,                 // command parameter (argument)
    highlight_spec_comment,               // comment
    highlight_spec_match,                 // matching parenthesis, etc.
    highlight_spec_search_match,          // search match
    highlight_spec_operator,              // operator
    highlight_spec_escape,                // escape sequences
    highlight_spec_quote,                 // quoted string
    highlight_spec_redirection,           // redirection
    highlight_spec_autosuggestion,        // autosuggestion
    highlight_spec_selection,

    // Pager support.
    highlight_spec_pager_prefix,
    highlight_spec_pager_completion,
    highlight_spec_pager_description,
    highlight_spec_pager_progress,
    highlight_spec_pager_secondary,

    HIGHLIGHT_SPEC_PRIMARY_MASK = 0xFF,

    // The following values are modifiers.
    highlight_modifier_valid_path = 0x100,
    highlight_modifier_force_underline = 0x200,
    // Hackish, indicates that we should treat a foreground color as background, per certain
    // historical behavior.
    highlight_modifier_sloppy_background = 0x300,
    /* Very special value */
    highlight_spec_invalid = 0xFFFF

};
typedef uint32_t highlight_spec_t;

inline highlight_spec_t highlight_get_primary(highlight_spec_t val) {
    return val & HIGHLIGHT_SPEC_PRIMARY_MASK;
}

inline highlight_spec_t highlight_make_background(highlight_spec_t val) {
    assert(val >> 16 ==
           0);  // should have nothing in upper bits, otherwise this is already a background
    return val << 16;
}

class history_item_t;
struct file_detection_context_t;

/// Perform syntax highlighting for the shell commands in buff. The result is stored in the color
/// array as a color_code from the HIGHLIGHT_ enum for each character in buff.
///
/// \param buff The buffer on which to perform syntax highlighting
/// \param color The array in wchich to store the color codes. The first 8 bits are used for fg
/// color, the next 8 bits for bg color.
/// \param pos the cursor position. Used for quote matching, etc.
/// \param error a list in which a description of each error will be inserted. May be 0, in whcich
/// case no error descriptions will be generated.
void highlight_shell(const wcstring &buffstr, std::vector<highlight_spec_t> &color, size_t pos,
                     wcstring_list_t *error, const env_vars_snapshot_t &vars);

/// Perform a non-blocking shell highlighting. The function will not do any I/O that may block. As a
/// result, invalid commands may not be detected, etc.
void highlight_shell_no_io(const wcstring &buffstr, std::vector<highlight_spec_t> &color,
                           size_t pos, wcstring_list_t *error, const env_vars_snapshot_t &vars);

/// Perform syntax highlighting for the text in buff. Matching quotes and paranthesis are
/// highlighted. The result is stored in the color array as a color_code from the HIGHLIGHT_ enum
/// for each character in buff.
///
/// \param buff The buffer on which to perform syntax highlighting
/// \param color The array in wchich to store the color codes. The first 8 bits are used for fg
/// color, the next 8 bits for bg color.
/// \param pos the cursor position. Used for quote matching, etc.
/// \param error a list in which a description of each error will be inserted. May be 0, in whcich
/// case no error descriptions will be generated.
void highlight_universal(const wcstring &buffstr, std::vector<highlight_spec_t> &color, size_t pos,
                         wcstring_list_t *error, const env_vars_snapshot_t &vars);

/// Translate from HIGHLIGHT_* to FISH_COLOR_* according to environment variables. Defaults to
/// FISH_COLOR_NORMAL.
///
/// Example:
///
/// If the environment variable FISH_FISH_COLOR_ERROR is set to 'red', a call to
/// highlight_get_color( highlight_error) will return FISH_COLOR_RED.
rgb_color_t highlight_get_color(highlight_spec_t highlight, bool is_background);

/// Given a command 'str' from the history, try to determine whether we ought to suggest it by
/// specially recognizing the command. Returns true if we validated the command. If so, returns by
/// reference whether the suggestion is valid or not.
bool autosuggest_validate_from_history(const history_item_t &item,
                                       file_detection_context_t &detector,
                                       const wcstring &working_directory,
                                       const env_vars_snapshot_t &vars);

// Tests whether the specified string cpath is the prefix of anything we could cd to. directories is
// a list of possible parent directories (typically either the working directory, or the cdpath).
// This does I/O!
//
// This is used only internally to this file, and is exposed only for testing.
enum {
    // The path must be to a directory.
    PATH_REQUIRE_DIR = 1 << 0,
    // Expand any leading tilde in the path.
    PATH_EXPAND_TILDE = 1 << 1
};
typedef unsigned int path_flags_t;
bool is_potential_path(const wcstring &const_path, const wcstring_list_t &directories,
                       path_flags_t flags);

#endif
