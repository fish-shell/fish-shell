// Prototypes for string expansion functions. These functions perform several kinds of parameter
// expansion. There are a lot of issues with regards to memory allocation. Overall, these functions
// would benefit from using a more clever memory allocation scheme, perhaps an evil combination of
// talloc, string buffers and reference counting.
#ifndef FISH_EXPAND_H
#define FISH_EXPAND_H

#include "config.h"

#include <initializer_list>
#include <map>
#include <string>
#include <vector>

#include "common.h"
#include "enum_set.h"
#include "env.h"
#include "maybe.h"
#include "operation_context.h"
#include "parse_constants.h"

/// Set of flags controlling expansions.
enum expand_flag {
    /// Skip command substitutions.
    skip_cmdsubst = 1 << 0,
    /// Skip variable expansion.
    skip_variables = 1 << 1,
    /// Skip wildcard expansion.
    skip_wildcards = 1 << 2,
    /// The expansion is being done for tab or auto completions. Returned completions may have the
    /// wildcard as a prefix instead of a match.
    for_completions = 1 << 3,
    /// Only match files that are executable by the current user.
    executables_only = 1 << 4,
    /// Only match directories.
    directories_only = 1 << 5,
    /// Generate descriptions, stored in the description field of completions.
    gen_descriptions = 1 << 6,
    /// Un-expand home directories to tildes after.
    preserve_home_tildes = 1 << 7,
    /// Allow fuzzy matching.
    fuzzy_match = 1 << 8,
    /// Disallow directory abbreviations like /u/l/b for /usr/local/bin. Only applicable if
    /// fuzzy_match is set.
    no_fuzzy_directories = 1 << 9,
    /// Allows matching a leading dot even if the wildcard does not contain one.
    /// By default, wildcards only match a leading dot literally; this is why e.g. '*' does not
    /// match hidden files.
    allow_nonliteral_leading_dot = 1 << 10,
    /// Do expansions specifically to support cd. This means using CDPATH as a list of potential
    /// working directories, and to use logical instead of physical paths.
    special_for_cd = 1 << 11,
    /// Do expansions specifically for cd autosuggestion. This is to differentiate between cd
    /// completions and cd autosuggestions.
    special_for_cd_autosuggestion = 1 << 12,
    /// Do expansions specifically to support external command completions. This means using PATH as
    /// a list of potential working directories.
    special_for_command = 1 << 13,
};

using expand_flags_t = uint64_t;

enum : wchar_t {
    /// Character representing a home directory.
    HOME_DIRECTORY = EXPAND_RESERVED_BASE,
    /// Character representing process expansion for %self.
    PROCESS_EXPAND_SELF,
    /// Character representing variable expansion.
    VARIABLE_EXPAND,
    /// Character representing variable expansion into a single element.
    VARIABLE_EXPAND_SINGLE,
    /// Character representing the start of a bracket expansion.
    BRACE_BEGIN,
    /// Character representing the end of a bracket expansion.
    BRACE_END,
    /// Character representing separation between two bracket elements.
    BRACE_SEP,
    /// Character that takes the place of any whitespace within non-quoted text in braces
    BRACE_SPACE,
    /// Separate subtokens in a token with this character.
    INTERNAL_SEPARATOR,
    /// Character representing an empty variable expansion. Only used transitively while expanding
    /// variables.
    VARIABLE_EXPAND_EMPTY,
    /// This is a special pseudo-char that is not used other than to mark the end of the the special
    /// characters so we can sanity check the enum range.
    EXPAND_SENTINEL
};

/// The string represented by PROCESS_EXPAND_SELF
#define PROCESS_EXPAND_SELF_STR L"%self"
#define PROCESS_EXPAND_SELF_STR_LEN 5

#if INCLUDE_RUST_HEADERS
#include "expand.rs.h"
using expand_result_t = ExpandResult;
#endif

/// \param input the string to tilde expand
void expand_tilde(wcstring &input, const environment_t &vars);

#endif
