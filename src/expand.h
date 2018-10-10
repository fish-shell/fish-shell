// Prototypes for string expansion functions. These functions perform several kinds of parameter
// expansion. There are a lot of issues with regards to memory allocation. Overall, these functions
// would benefit from using a more clever memory allocation scheme, perhaps an evil combination of
// talloc, string buffers and reference counting.
#ifndef FISH_EXPAND_H
#define FISH_EXPAND_H

#include "config.h"

#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "common.h"
#include "parse_constants.h"

class env_var_t;

enum {
    /// Flag specifying that cmdsubst expansion should be skipped.
    EXPAND_SKIP_CMDSUBST = 1 << 0,
    /// Flag specifying that variable expansion should be skipped.
    EXPAND_SKIP_VARIABLES = 1 << 1,
    /// Flag specifying that wildcard expansion should be skipped.
    EXPAND_SKIP_WILDCARDS = 1 << 2,
    /// The expansion is being done for tab or auto completions. Returned completions may have the
    /// wildcard as a prefix instead of a match.
    EXPAND_FOR_COMPLETIONS = 1 << 3,
    /// Only match files that are executable by the current user.
    EXECUTABLES_ONLY = 1 << 4,
    /// Only match directories.
    DIRECTORIES_ONLY = 1 << 5,
    /// Don't generate descriptions.
    EXPAND_NO_DESCRIPTIONS = 1 << 6,
    /// Don't expand jobs (but you can still expand processes). This is because
    /// job expansion is not thread safe.
    EXPAND_SKIP_JOBS = 1 << 7,
    /// Don't expand home directories.
    EXPAND_SKIP_HOME_DIRECTORIES = 1 << 8,
    /// Allow fuzzy matching.
    EXPAND_FUZZY_MATCH = 1 << 9,
    /// Disallow directory abbreviations like /u/l/b for /usr/local/bin. Only applicable if
    /// EXPAND_FUZZY_MATCH is set.
    EXPAND_NO_FUZZY_DIRECTORIES = 1 << 10,
    /// Do expansions specifically to support cd. This means using CDPATH as a list of potential
    /// working directories.
    EXPAND_SPECIAL_FOR_CD = 1 << 11,
    /// Do expansions specifically for cd autosuggestion. This is to differentiate between cd
    /// completions and cd autosuggestions.
    EXPAND_SPECIAL_FOR_CD_AUTOSUGGEST = 1 << 12,
    /// Do expansions specifically to support external command completions. This means using PATH as
    /// a list of potential working directories.
    EXPAND_SPECIAL_FOR_COMMAND = 1 << 13
};
typedef int expand_flags_t;

class completion_t;

enum {
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
    /// This is a special psuedo-char that is not used other than to mark the end of the the special
    /// characters so we can sanity check the enum range.
    EXPAND_SENTINAL
};

/// These are the possible return values for expand_string. Note how zero value is the only error.
enum expand_error_t {
    /// Error
    EXPAND_ERROR,
    /// Ok
    EXPAND_OK,
    /// Ok, a wildcard in the string matched no files.
    EXPAND_WILDCARD_NO_MATCH,
    /// Ok, a wildcard in the string matched a file.
    EXPAND_WILDCARD_MATCH
};

/// The string represented by PROCESS_EXPAND_SELF
#define PROCESS_EXPAND_SELF_STR L"%self"

/// Perform various forms of expansion on in, such as tilde expansion (\~USER becomes the users home
/// directory), variable expansion (\$VAR_NAME becomes the value of the environment variable
/// VAR_NAME), cmdsubst expansion and wildcard expansion. The results are inserted into the list
/// out.
///
/// If the parameter does not need expansion, it is copied into the list out.
///
/// \param input The parameter to expand
/// \param output The list to which the result will be appended.
/// \param flags Specifies if any expansion pass should be skipped. Legal values are any combination
/// of EXPAND_SKIP_CMDSUBST EXPAND_SKIP_VARIABLES and EXPAND_SKIP_WILDCARDS
/// \param errors Resulting errors, or NULL to ignore
///
/// \return One of EXPAND_OK, EXPAND_ERROR, EXPAND_WILDCARD_MATCH and EXPAND_WILDCARD_NO_MATCH.
/// EXPAND_WILDCARD_NO_MATCH and EXPAND_WILDCARD_MATCH are normal exit conditions used only on
/// strings containing wildcards to tell if the wildcard produced any matches.
__warn_unused expand_error_t expand_string(wcstring input, std::vector<completion_t> *output,
                                           expand_flags_t flags, parse_error_list_t *errors);

/// expand_one is identical to expand_string, except it will fail if in expands to more than one
/// string. This is used for expanding command names.
///
/// \param inout_str The parameter to expand in-place
/// \param flags Specifies if any expansion pass should be skipped. Legal values are any combination
/// of EXPAND_SKIP_CMDSUBST EXPAND_SKIP_VARIABLES and EXPAND_SKIP_WILDCARDS
/// \param errors Resulting errors, or NULL to ignore
///
/// \return Whether expansion succeded
bool expand_one(wcstring &inout_str, expand_flags_t flags, parse_error_list_t *errors = NULL);

/// Expand a command string like $HOME/bin/cmd into a command and list of arguments.
/// Return the command and arguments by reference.
/// If the expansion resulted in no or an empty command, the command will be an empty string. Note
/// that API does not distinguish between expansion resulting in an empty command (''), and
/// expansion resulting in no command (e.g. unset variable).
// \return an expand error.
expand_error_t expand_to_command_and_args(const wcstring &instr, wcstring *out_cmd,
                                          wcstring_list_t *out_args,
                                          parse_error_list_t *errors = NULL);

/// Convert the variable value to a human readable form, i.e. escape things, handle arrays, etc.
/// Suitable for pretty-printing.
wcstring expand_escape_variable(const env_var_t &var);

/// Perform tilde expansion and nothing else on the specified string, which is modified in place.
///
/// \param input the string to tilde expand
void expand_tilde(wcstring &input);

/// Perform the opposite of tilde expansion on the string, which is modified in place.
wcstring replace_home_directory_with_tilde(const wcstring &str);

/// Abbreviation support. Expand src as an abbreviation, returning true if one was found, false if
/// not. If result is not-null, returns the abbreviation by reference.
void update_abbr_cache(const wchar_t *op, const wcstring &varname);
bool expand_abbreviation(const wcstring &src, wcstring *output);

/// \return a snapshot of all abbreviations as a map abbreviation->expansion.
std::map<wcstring, wcstring> get_abbreviations();

// Terrible hacks
bool fish_xdm_login_hack_hack_hack_hack(std::vector<std::string> *cmds, int argc,
                                        const char *const *argv);
#endif
