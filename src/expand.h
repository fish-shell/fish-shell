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
#include "enum_set.h"
#include "maybe.h"
#include "parse_constants.h"

class environment_t;
class env_var_t;
class environment_t;

/// Set of flags controlling expansions.
enum class expand_flag {
    /// Skip command substitutions.
    skip_cmdsubst,
    /// Skip variable expansion.
    skip_variables,
    /// Skip wildcard expansion.
    skip_wildcards,
    /// The expansion is being done for tab or auto completions. Returned completions may have the
    /// wildcard as a prefix instead of a match.
    for_completions,
    /// Only match files that are executable by the current user.
    executables_only,
    /// Only match directories.
    directories_only,
    /// Don't generate descriptions.
    no_descriptions,
    /// Don't expand jobs (but still expand processes).
    skip_jobs,
    /// Don't expand home directories.
    skip_home_directories,
    /// Allow fuzzy matching.
    fuzzy_match,
    /// Disallow directory abbreviations like /u/l/b for /usr/local/bin. Only applicable if
    /// fuzzy_match is set.
    no_fuzzy_directories,
    /// Do expansions specifically to support cd. This means using CDPATH as a list of potential
    /// working directories, and to use logical instead of physical paths.
    special_for_cd,
    /// Do expansions specifically for cd autosuggestion. This is to differentiate between cd
    /// completions and cd autosuggestions.
    special_for_cd_autosuggestion,
    /// Do expansions specifically to support external command completions. This means using PATH as
    /// a list of potential working directories.
    special_for_command,

    COUNT,
};

template <>
struct enum_info_t<expand_flag> {
    static constexpr auto count = expand_flag::COUNT;
};

using expand_flags_t = enum_set_t<expand_flag>;

class completion_t;

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
    /// This is a special psuedo-char that is not used other than to mark the end of the the special
    /// characters so we can sanity check the enum range.
    EXPAND_SENTINAL
};

/// These are the possible return values for expand_string. Note how zero value is the only error.
enum class expand_result_t {
    /// Error
    error,
    /// Ok
    ok,
    /// Ok, a wildcard in the string matched no files.
    wildcard_no_match,
    /// Ok, a wildcard in the string matched a file.
    wildcard_match,
};

/// The string represented by PROCESS_EXPAND_SELF
#define PROCESS_EXPAND_SELF_STR L"%self"
#define PROCESS_EXPAND_SELF_STR_LEN 5

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
/// of skip_cmdsubst skip_variables and skip_wildcards
/// \param vars variables used during expansion.
/// \param parser the parser to use for command substitutions, or nullptr to disable.
/// \param errors Resulting errors, or NULL to ignore
///
/// \return An expand_result_t.
/// wildcard_no_match and wildcard_match are normal exit conditions used only on
/// strings containing wildcards to tell if the wildcard produced any matches.
class parser_t;
__warn_unused expand_result_t expand_string(wcstring input, std::vector<completion_t> *output,
                                            expand_flags_t flags, const environment_t &vars,
                                            const std::shared_ptr<parser_t> &parser,
                                            parse_error_list_t *errors);

/// expand_one is identical to expand_string, except it will fail if in expands to more than one
/// string. This is used for expanding command names.
///
/// \param inout_str The parameter to expand in-place
/// \param flags Specifies if any expansion pass should be skipped. Legal values are any combination
/// of skip_cmdsubst skip_variables and skip_wildcards
/// \param parser the parser to use for command substitutions, or nullptr to disable.
/// \param errors Resulting errors, or NULL to ignore
///
/// \return Whether expansion succeded
bool expand_one(wcstring &inout_str, expand_flags_t flags, const environment_t &vars,
                const std::shared_ptr<parser_t> &parser, parse_error_list_t *errors = NULL);

/// Expand a command string like $HOME/bin/cmd into a command and list of arguments.
/// Return the command and arguments by reference.
/// If the expansion resulted in no or an empty command, the command will be an empty string. Note
/// that API does not distinguish between expansion resulting in an empty command (''), and
/// expansion resulting in no command (e.g. unset variable).
// \return an expand error.
expand_result_t expand_to_command_and_args(const wcstring &instr, const environment_t &vars,
                                           wcstring *out_cmd, wcstring_list_t *out_args,
                                           parse_error_list_t *errors = NULL);

/// Convert the variable value to a human readable form, i.e. escape things, handle arrays, etc.
/// Suitable for pretty-printing.
wcstring expand_escape_variable(const env_var_t &var);

/// Perform tilde expansion and nothing else on the specified string, which is modified in place.
///
/// \param input the string to tilde expand
void expand_tilde(wcstring &input, const environment_t &vars);

/// Perform the opposite of tilde expansion on the string, which is modified in place.
wcstring replace_home_directory_with_tilde(const wcstring &str, const environment_t &vars);

/// Abbreviation support. Expand src as an abbreviation, returning the expanded form if found,
/// none() if not.
maybe_t<wcstring> expand_abbreviation(const wcstring &src, const environment_t &vars);

/// \return a snapshot of all abbreviations as a map abbreviation->expansion.
/// The abbreviations are unescaped, i.e. they may not be valid variable identifiers (#6166).
std::map<wcstring, wcstring> get_abbreviations(const environment_t &vars);

// Terrible hacks
bool fish_xdm_login_hack_hack_hack_hack(std::vector<std::string> *cmds, int argc,
                                        const char *const *argv);
#endif
