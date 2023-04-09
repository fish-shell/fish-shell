use crate::common::{char_offset, EXPAND_RESERVED_BASE, EXPAND_RESERVED_END};
use crate::env::Environment;
use crate::operation_context::OperationContext;
use crate::parse_constants::ParseErrorList;
use crate::wchar::{wstr, WString};
use bitflags::bitflags;
use widestring_suffix::widestrs;

bitflags! {
    /// Set of flags controlling expansions.
    pub struct ExpandFlags : u16 {
        /// Skip command substitutions.
        const SKIP_CMDSUBST = 1 << 0;
        /// Skip variable expansion.
        const SKIP_VARIABLES = 1 << 1;
        /// Skip wildcard expansion.
        const SKIP_WILDCARDS = 1 << 2;
        /// The expansion is being done for tab or auto completions. Returned completions may have the
        /// wildcard as a prefix instead of a match.
        const FOR_COMPLETIONS = 1 << 3;
        /// Only match files that are executable by the current user.
        const EXECUTABLES_ONLY = 1 << 4;
        /// Only match directories.
        const DIRECTORIES_ONLY = 1 << 5;
        /// Generate descriptions, stored in the description field of completions.
        const GEN_DESCRIPTIONS = 1 << 6;
        /// Un-expand home directories to tildes after.
        const PRESERVE_HOME_TILDES = 1 << 7;
        /// Allow fuzzy matching.
        const FUZZY_MATCH = 1 << 8;
        /// Disallow directory abbreviations like /u/l/b for /usr/local/bin. Only applicable if
        /// fuzzy_match is set.
        const NO_FUZZY_DIRECTORIES = 1 << 9;
        /// Allows matching a leading dot even if the wildcard does not contain one.
        /// By default, wildcards only match a leading dot literally; this is why e.g. '*' does not
        /// match hidden files.
        const ALLOW_NONLITERAL_LEADING_DOT = 1 << 10;
        /// Do expansions specifically to support cd. This means using CDPATH as a list of potential
        /// working directories, and to use logical instead of physical paths.
        const SPECIAL_FOR_CD = 1 << 11;
        /// Do expansions specifically for cd autosuggestion. This is to differentiate between cd
        /// completions and cd autosuggestions.
        const SPECIAL_FOR_CD_AUTOSUGGESTION = 1 << 12;
        /// Do expansions specifically to support external command completions. This means using PATH as
        /// a list of potential working directories.
        const SPECIAL_FOR_COMMAND = 1 << 13;
    }
}

/// Character representing a home directory.
pub const HOME_DIRECTORY: char = char_offset(EXPAND_RESERVED_BASE, 0);
/// Character representing process expansion for %self.
pub const PROCESS_EXPAND_SELF: char = char_offset(EXPAND_RESERVED_BASE, 1);
/// Character representing variable expansion.
pub const VARIABLE_EXPAND: char = char_offset(EXPAND_RESERVED_BASE, 2);
/// Character representing variable expansion into a single element.
pub const VARIABLE_EXPAND_SINGLE: char = char_offset(EXPAND_RESERVED_BASE, 3);
/// Character representing the start of a bracket expansion.
pub const BRACE_BEGIN: char = char_offset(EXPAND_RESERVED_BASE, 4);
/// Character representing the end of a bracket expansion.
pub const BRACE_END: char = char_offset(EXPAND_RESERVED_BASE, 5);
/// Character representing separation between two bracket elements.
pub const BRACE_SEP: char = char_offset(EXPAND_RESERVED_BASE, 6);
/// Character that takes the place of any whitespace within non-quoted text in braces
pub const BRACE_SPACE: char = char_offset(EXPAND_RESERVED_BASE, 7);
/// Separate subtokens in a token with this character.
pub const INTERNAL_SEPARATOR: char = char_offset(EXPAND_RESERVED_BASE, 8);
/// Character representing an empty variable expansion. Only used transitively while expanding
/// variables.
pub const VARIABLE_EXPAND_EMPTY: char = char_offset(EXPAND_RESERVED_BASE, 9);

const _: () = assert!(
    EXPAND_RESERVED_END as u32 > VARIABLE_EXPAND_EMPTY as u32,
    "Characters used in expansions must stay within private use area"
);

#[derive(Copy, Clone, Eq, PartialEq)]
pub enum ExpandResultCode {
    /// There was an error, for example, unmatched braces.
    error,
    /// Expansion succeeded.
    ok,
    /// Expansion was cancelled (e.g. control-C).
    cancel,
    /// Expansion succeeded, but a wildcard in the string matched no files,
    /// so the output is empty.
    wildcard_no_match,
}

/// These are the possible return values for expand_string.
pub struct ExpandResult {
    // todo!
    pub result: ExpandResultCode,
}

impl PartialEq<ExpandResultCode> for ExpandResult {
    fn eq(&self, other: &ExpandResultCode) -> bool {
        self.result == *other
    }
}

/// The string represented by PROCESS_EXPAND_SELF
#[widestrs]
pub const PROCESS_EXPAND_SELF_STR: &wstr = "%self"L;

/// expand_one is identical to expand_string, except it will fail if in expands to more than one
/// string. This is used for expanding command names.
///
/// \param inout_str The parameter to expand in-place
/// \param flags Specifies if any expansion pass should be skipped. Legal values are any combination
/// of skip_cmdsubst skip_variables and skip_wildcards
/// \param ctx The parser, variables, and cancellation checker for this operation. The parser may be
/// null. \param errors Resulting errors, or nullptr to ignore
///
/// \return Whether expansion succeeded.
#[allow(unused_variables)]
pub fn expand_one(
    s: &mut WString,
    flags: ExpandFlags,
    ctx: &OperationContext,
    errors: Option<&mut ParseErrorList>,
) -> bool {
    todo!()
}

/// Expand a command string like $HOME/bin/cmd into a command and list of arguments.
/// Return the command and arguments by reference.
/// If the expansion resulted in no or an empty command, the command will be an empty string. Note
/// that API does not distinguish between expansion resulting in an empty command (''), and
/// expansion resulting in no command (e.g. unset variable).
/// If \p skip_wildcards is true, then do not do wildcard expansion
/// \return an expand error.
#[allow(unused_variables)]
pub fn expand_to_command_and_args(
    instr: &wstr,
    ctx: &OperationContext,
    out_cmd: &mut WString,
    out_args: Option<&Vec<WString>>,
    errors: &mut ParseErrorList,
    skip_wildcards: bool,
) -> ExpandResult {
    todo!()
}

/// Perform tilde expansion and nothing else on the specified string, which is modified in place.
///
/// \param input the string to tilde expand
pub fn expand_tilde(input: &mut WString, _vars: &dyn Environment) {
    if input.chars().next() == Some('~') {
        input.replace_range(0..1, wstr::from_char_slice(&[HOME_DIRECTORY]));
        todo!();
        // expand_home_directory(input, vars);
    }
}
