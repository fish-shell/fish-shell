//! String expansion functions. These functions perform several kinds of parameter expansion. There
//! are a lot of issues with regards to memory allocation. Overall, these functions would benefit
//! from using a more clever memory allocation scheme, perhaps an evil combination of talloc,
//! string buffers and reference counting.

use crate::builtins::shared::{
    STATUS_CMD_ERROR, STATUS_CMD_UNKNOWN, STATUS_EXPAND_ERROR, STATUS_ILLEGAL_CMD,
    STATUS_INVALID_ARGS, STATUS_NOT_EXECUTABLE, STATUS_READ_TOO_MUCH, STATUS_UNMATCHED_WILDCARD,
};
use crate::common::{
    char_offset, charptr2wcstring, escape, escape_string, escape_string_for_double_quotes,
    unescape_string, valid_var_name_char, wcs2zstring, EscapeFlags, EscapeStringStyle,
    UnescapeFlags, UnescapeStringStyle, EXPAND_RESERVED_BASE, EXPAND_RESERVED_END,
};
use crate::complete::{CompleteFlags, Completion, CompletionList, CompletionReceiver};
use crate::env::{EnvVar, Environment};
use crate::exec::exec_subshell_for_expand;
use crate::future_feature_flags::{feature_test, FeatureFlag};
use crate::history::{history_session_id, History};
use crate::operation_context::OperationContext;
use crate::parse_constants::{ParseError, ParseErrorCode, ParseErrorList, SOURCE_LOCATION_UNKNOWN};
use crate::parse_util::{
    parse_util_expand_variable_error, parse_util_locate_cmdsubst_range, MaybeParentheses,
};
use crate::path::path_apply_working_directory;
use crate::util::wcsfilecmp_glob;
use crate::wchar::prelude::*;
use crate::wcstringutil::{join_strings, trim};
use crate::wildcard::{wildcard_expand_string, wildcard_has_internal};
use crate::wildcard::{WildcardResult, ANY_CHAR, ANY_STRING, ANY_STRING_RECURSIVE};
use crate::wutil::{normalize_path, wcstoi_partial, Options};
use bitflags::bitflags;
use std::mem::MaybeUninit;

bitflags! {
    /// Set of flags controlling expansions.
    #[derive(Copy, Clone, Default)]
    pub struct ExpandFlags : u16 {
        /// Fail expansion if there is a command substitution.
        const FAIL_ON_CMDSUBST = 1 << 0;
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
        /// The token has an unclosed brace, so don't add a space.
        const NO_SPACE_FOR_UNCLOSED_BRACE = 1 << 14;
        /// Skip command substitutions.
        const SKIP_CMDSUBST = 1 << 15;
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

impl ExpandResult {
    pub fn new(result: ExpandResultCode) -> Self {
        Self { result, status: 0 }
    }
    pub fn ok() -> Self {
        Self::new(ExpandResultCode::ok)
    }
    /// Make an error value with the given status.
    pub fn make_error(status: libc::c_int) -> Self {
        assert!(status != 0, "status cannot be 0 for an error result");
        Self {
            result: ExpandResultCode::error,
            status,
        }
    }
}

impl PartialEq<ExpandResultCode> for ExpandResult {
    fn eq(&self, other: &ExpandResultCode) -> bool {
        self.result == *other
    }
}

/// The string represented by PROCESS_EXPAND_SELF
pub const PROCESS_EXPAND_SELF_STR: &wstr = L!("%self");

/// Perform various forms of expansion on in, such as tilde expansion (\~USER becomes the users home
/// directory), variable expansion (\$VAR_NAME becomes the value of the environment variable
/// VAR_NAME), cmdsubst expansion and wildcard expansion. The results are inserted into the list
/// out.
///
/// If the parameter does not need expansion, it is copied into the list out.
///
/// \param input The parameter to expand
/// \param output The list to which the result will be appended.
/// \param ctx The parser, variables, and cancellation checker for this operation.  The parser may
/// be null. \param errors Resulting errors, or nullptr to ignore
///
/// Return An expand_result_t.
/// wildcard_no_match and wildcard_match are normal exit conditions used only on
/// strings containing wildcards to tell if the wildcard produced any matches.
pub fn expand_string(
    input: WString,
    out_completions: &mut CompletionList,
    flags: ExpandFlags,
    ctx: &OperationContext,
    errors: Option<&mut ParseErrorList>,
) -> ExpandResult {
    let mut completions = vec![];
    std::mem::swap(&mut completions, out_completions);
    let mut recv = CompletionReceiver::from_list(completions, ctx.expansion_limit);
    let result = expand_to_receiver(input, &mut recv, flags, ctx, errors);
    *out_completions = recv.take();
    result
}

/// Variant of string that inserts its results into a completion_receiver_t.
pub fn expand_to_receiver(
    input: WString,
    out_completions: &mut CompletionReceiver,
    flags: ExpandFlags,
    ctx: &OperationContext,
    errors: Option<&mut ParseErrorList>,
) -> ExpandResult {
    Expander::expand_string(input, out_completions, flags, ctx, errors)
}

/// expand_one is identical to expand_string, except it will fail if in expands to more than one
/// string. This is used for expanding command names.
///
/// \param inout_str The parameter to expand in-place
/// \param ctx The parser, variables, and cancellation checker for this operation. The parser may be
/// null.
/// \param errors Resulting errors, or nullptr to ignore
///
/// Return Whether expansion succeeded.
pub fn expand_one(
    s: &mut WString,
    flags: ExpandFlags,
    ctx: &OperationContext,
    errors: Option<&mut ParseErrorList>,
) -> bool {
    let mut completions = CompletionList::new();

    if !flags.contains(ExpandFlags::FOR_COMPLETIONS) && expand_is_clean(s) {
        return true;
    }

    let mut tmp = WString::new();
    std::mem::swap(s, &mut tmp);
    if expand_string(tmp, &mut completions, flags, ctx, errors) == ExpandResultCode::ok
        && completions.len() == 1
    {
        std::mem::swap(s, &mut completions[0].completion);
        return true;
    }

    false
}

/// Expand a command string like $HOME/bin/cmd into a command and list of arguments.
/// Return the command and arguments by reference.
/// If the expansion resulted in no or an empty command, the command will be an empty string. Note
/// that API does not distinguish between expansion resulting in an empty command (''), and
/// expansion resulting in no command (e.g. unset variable).
/// If `skip_wildcards` is true, then do not do wildcard expansion
/// Return an expand error.
pub fn expand_to_command_and_args(
    instr: &wstr,
    ctx: &OperationContext<'_>,
    out_cmd: &mut WString,
    mut out_args: Option<&mut Vec<WString>>,
    errors: Option<&mut ParseErrorList>,
    skip_wildcards: bool,
) -> ExpandResult {
    // Fast path.
    if expand_is_clean(instr) {
        *out_cmd = instr.to_owned();
        return ExpandResult::ok();
    }

    let mut eflags = ExpandFlags::FAIL_ON_CMDSUBST;
    if skip_wildcards {
        eflags |= ExpandFlags::SKIP_WILDCARDS;
    }

    let mut completions = CompletionList::new();
    let expand_err = expand_string(instr.to_owned(), &mut completions, eflags, ctx, errors);
    if expand_err == ExpandResultCode::ok {
        // The first completion is the command, any remaining are arguments.
        let mut completions = completions.into_iter();
        if let Some(comp) = completions.next() {
            *out_cmd = comp.completion;
        }
        if let Some(ref mut out_args) = out_args {
            for comp in completions {
                out_args.push(comp.completion);
            }
        }
    }

    expand_err
}

/// Convert the variable value to a human readable form, i.e. escape things, handle arrays, etc.
/// Suitable for pretty-printing.
pub fn expand_escape_variable(var: &EnvVar) -> WString {
    let mut buff = WString::new();

    let lst = var.as_list();
    for el in lst {
        if !buff.is_empty() {
            buff.push_str("  ");
        }

        // We want to use quotes if we have more than one string, or the string contains a space.
        let prefer_quotes = lst.len() > 1 || el.contains(' ');
        if prefer_quotes && is_quotable(el) {
            buff.push('\'');
            buff.push_utfstr(el);
            buff.push('\'');
        } else {
            buff.push_utfstr(&escape(el));
        }
    }
    buff
}

/// Convert a string value to a human readable form, i.e. escape things, handle arrays, etc.
/// Suitable for pretty-printing.
pub fn expand_escape_string(el: &wstr) -> WString {
    let mut buff = WString::new();
    let prefer_quotes = el.contains(' ');
    if prefer_quotes && is_quotable(el) {
        buff.push('\'');
        buff.push_utfstr(el);
        buff.push('\'');
    } else {
        buff.push_utfstr(&escape(el));
    }
    buff
}

/// Perform tilde expansion and nothing else on the specified string, which is modified in place.
///
/// \param input the string to tilde expand
pub fn expand_tilde(input: &mut WString, vars: &dyn Environment) {
    if input.chars().next() == Some('~') {
        input.replace_range(0..1, wstr::from_char_slice(&[HOME_DIRECTORY]));
        expand_home_directory(input, vars);
    }
}

/// Perform the opposite of tilde expansion on the string, which is modified in place.
pub fn replace_home_directory_with_tilde(s: &wstr, vars: &dyn Environment) -> WString {
    let mut result = s.to_owned();
    // Only absolute paths get this treatment.
    if result.starts_with(L!("/")) {
        let mut home_directory = L!("~").to_owned();
        expand_tilde(&mut home_directory, vars);
        // If we can't get a home directory, don't replace anything.
        // This is the case e.g. with --no-execute
        if home_directory.is_empty() {
            return result;
        }
        if !home_directory.ends_with(L!("/")) {
            home_directory.push('/');
        }

        // Now check if the home_directory prefixes the string.
        if result.starts_with(&home_directory) {
            // Success
            result.replace_range(0..home_directory.len(), L!("~/"));
        }
    }
    result
}

/// Characters which make a string unclean if they are the first character of the string. See \c
/// expand_is_clean().
const UNCLEAN_FIRST: &wstr = L!("~%");
/// Unclean characters. See \c expand_is_clean().
const UNCLEAN: &wstr = L!("$*?\\\"'({})");

/// Test if the specified argument is clean, i.e. it does not contain any tokens which need to be
/// expanded or otherwise altered. Clean strings can be passed through expand_string and expand_one
/// without changing them. About two thirds of all strings are clean, so skipping expansion on them
/// actually does save a small amount of time, since it avoids multiple memory allocations during
/// the expansion process.
///
/// \param in the string to test
fn expand_is_clean(input: &wstr) -> bool {
    if input.is_empty() {
        return true;
    }

    // Test characters that have a special meaning in the first character position.
    if UNCLEAN_FIRST.contains(input.as_char_slice()[0]) {
        return false;
    }

    // Test characters that have a special meaning in any character position.
    !input.chars().any(|c| UNCLEAN.contains(c))
}

/// Append a syntax error to the given error list.
macro_rules! append_syntax_error {
    (
        $errors:expr, $source_start:expr,
        $fmt:expr $(, $arg:expr )* $(,)?
    ) => {
        if let Some(ref mut errors) = $errors {
            let mut error = ParseError::default();
            error.source_start = $source_start;
            error.source_length = 0;
            error.code = ParseErrorCode::syntax;
            error.text = wgettext_fmt!($fmt $(, $arg)*);
            errors.push(error);
        }
    }
}

/// Append a cmdsub error to the given error list. But only do so if the error hasn't already been
/// recorded. This is needed because command substitution is a recursive process and some errors
/// could consequently be recorded more than once.
macro_rules! append_cmdsub_error {
    (
        $errors:expr, $source_start:expr, $source_end:expr,
        $fmt:expr $(, $arg:expr )* $(,)?
    ) => {
        append_cmdsub_error_formatted!(
            $errors, $source_start, $source_end,
            wgettext_fmt!($fmt $(, $arg)*));
    }
}

macro_rules! append_cmdsub_error_formatted {
    (
        $errors:expr, $source_start:expr, $source_end:expr,
        $text:expr $(,)?
    ) => {
        if let Some(ref mut errors) = $errors {
            let mut error = ParseError::default();
            error.source_start = $source_start;
            error.source_length = $source_end - $source_start + 1;
            error.code = ParseErrorCode::cmdsubst;
            error.text = $text;
            if !errors.iter().any(|e| e.text == error.text) {
                errors.push(error);
            }
        }
    };
}

/// Append an overflow error, when expansion produces too much data.
fn append_overflow_error(
    errors: &mut Option<&mut ParseErrorList>,
    source_start: Option<usize>,
) -> ExpandResult {
    if let Some(ref mut errors) = errors {
        let mut error = ParseError::default();
        error.source_start = source_start.unwrap_or(SOURCE_LOCATION_UNKNOWN);
        error.source_length = 0;
        error.code = ParseErrorCode::generic;
        error.text = wgettext!("Expansion produced too many results").to_owned();
        errors.push(error);
    }
    ExpandResult::make_error(STATUS_EXPAND_ERROR)
}

/// Test if the specified string does not contain character which can not be used inside a quoted
/// string.
fn is_quotable(s: &wstr) -> bool {
    !s.chars().any(|c| "\n\t\r\x08\x1B".contains(c))
}

enum ParseSliceError {
    zero_index,
    invalid_index,
}

/// Parse an array slicing specification Returns 0 on success. If a parse error occurs, returns the
/// index of the bad token. Note that 0 can never be a bad index because the string always starts
/// with [.
fn parse_slice(
    input: &wstr,
    idx: &mut Vec<i64>,
    array_size: usize,
) -> Result<usize, (usize, ParseSliceError)> {
    let size = i64::try_from(array_size).unwrap();
    let mut pos = 1; // skip past the opening square bracket

    loop {
        while input.char_at(pos).is_whitespace() || input.char_at(pos) == INTERNAL_SEPARATOR {
            pos += 1;
        }
        if input.char_at(pos) == ']' {
            pos += 1;
            break;
        }

        let tmp = if idx.is_empty() && input.char_at(pos) == '.' && input.char_at(pos + 1) == '.' {
            // If we are at the first index expression, a missing start-index means the range starts
            // at the first item.
            1 // first index
        } else {
            let mut consumed = 0;
            match wcstoi_partial(&input[pos..], Options::default(), &mut consumed) {
                Ok(tmp) => {
                    if tmp == 0 {
                        // Explicitly refuse $foo[0] as valid syntax, regardless of whether or
                        // not we're going to show an error if the index ultimately evaluates
                        // to zero. This will help newcomers to fish avoid a common off-by-one
                        // error. See #4862.
                        return Err((pos, ParseSliceError::zero_index));
                    }
                    pos += consumed;
                    // Skip trailing whitespace.
                    pos += input[pos..]
                        .chars()
                        .take_while(|c| c.is_whitespace())
                        .count();
                    tmp
                }
                Err(_error) => {
                    // We don't test `*end` as is typically done because we expect it to not
                    // be the null char. Ignore the case of errno==-1 because it means the end
                    // char wasn't the null char.
                    return Err((pos, ParseSliceError::invalid_index));
                }
            }
        };

        let mut i1 = if tmp > -1 { tmp } else { size + tmp + 1 };
        while input.char_at(pos) == INTERNAL_SEPARATOR {
            pos += 1;
        }
        if input.char_at(pos) == '.' && input.char_at(pos + 1) == '.' {
            pos += 2;
            while input.char_at(pos) == INTERNAL_SEPARATOR {
                pos += 1;
            }
            while input.char_at(pos).is_whitespace() {
                pos += 1; // Allow the space in "[.. ]".
            }

            // If we are at the last index range expression  then a missing end-index means the
            // range spans until the last item.
            let tmp1 = if input.char_at(pos) == ']' {
                -1 // last index
            } else {
                let mut consumed = 0;
                match wcstoi_partial(&input[pos..], Options::default(), &mut consumed) {
                    Ok(tmp) => {
                        if tmp == 0 {
                            return Err((pos, ParseSliceError::zero_index));
                        }
                        pos += consumed;
                        // Skip trailing whitespace.
                        pos += input[pos..]
                            .chars()
                            .take_while(|c| c.is_whitespace())
                            .count();
                        tmp
                    }
                    Err(_error) => {
                        return Err((pos, ParseSliceError::invalid_index));
                    }
                }
            };

            let mut i2 = if tmp1 > -1 { tmp1 } else { size + tmp1 + 1 };
            // Skip sequences that are entirely outside.
            // This means "17..18" expands to nothing if there are less than 17 elements.
            if i1 > size && i2 > size {
                continue;
            }
            let mut direction = if i2 < i1 { -1 } else { 1 };
            // If only the beginning is negative, always go reverse.
            // If only the end, always go forward.
            // Prevents `[x..-1]` from going reverse if less than x elements are there.
            if (tmp1 > -1) != (tmp > -1) {
                direction = if tmp1 > -1 { -1 } else { 1 };
            } else {
                // Clamp to array size when not forcing direction
                // - otherwise "2..-1" clamps both to 1 and then becomes "1..1".
                i1 = i1.min(size);
                i2 = i2.min(size);
            }
            let mut jjj = i1;
            while jjj * direction <= i2 * direction {
                idx.push(jjj);
                jjj += direction;
            }
            continue;
        }

        idx.push(i1);
    }

    Ok(pos)
}

/// Expand all environment variables in the string *ptr.
///
/// This function is slow, fragile and complicated. There are lots of little corner cases, like
/// $$foo should do a double expansion, $foo$bar should not double expand bar, etc.
///
/// This function operates on strings backwards, starting at last_idx.
///
/// Note: last_idx is considered to be where it previously finished processing. This means it
/// actually starts operating on last_idx-1. As such, to process a string fully, pass string.size()
/// as last_idx instead of string.size()-1.
///
/// Return the result of expansion.
fn expand_variables(
    instr: WString,
    out: &mut CompletionReceiver,
    last_idx: usize,
    vars: &dyn Environment,
    errors: &mut Option<&mut ParseErrorList>,
) -> ExpandResult {
    // last_idx may be 1 past the end of the string, but no further.
    assert!(last_idx <= instr.len(), "Invalid last_idx");
    if last_idx == 0 {
        if !out.add(instr) {
            return append_overflow_error(errors, None);
        }
        return ExpandResult::ok();
    }

    // Locate the last VARIABLE_EXPAND or VARIABLE_EXPAND_SINGLE
    let mut is_single = false;
    let mut varexp_char_idx = last_idx;
    loop {
        let done = varexp_char_idx == 0;
        varexp_char_idx = varexp_char_idx.wrapping_sub(1);
        if done {
            break;
        }
        let c = instr.as_char_slice()[varexp_char_idx];
        if [VARIABLE_EXPAND, VARIABLE_EXPAND_SINGLE].contains(&c) {
            is_single = c == VARIABLE_EXPAND_SINGLE;
            break;
        }
    }
    if varexp_char_idx == usize::MAX {
        // No variable expand char, we're done.
        if !out.add(instr) {
            return append_overflow_error(errors, None);
        }
        return ExpandResult::ok();
    }

    // Get the variable name.
    let var_name_start = varexp_char_idx + 1;
    let mut var_name_stop = var_name_start;
    while var_name_stop < instr.len() {
        let nc = instr.as_char_slice()[var_name_stop];
        if nc == VARIABLE_EXPAND_EMPTY {
            var_name_stop += 1;
            break;
        }
        if !valid_var_name_char(nc) {
            break;
        }
        var_name_stop += 1;
    }
    assert!(
        var_name_stop >= var_name_start,
        "Bogus variable name indexes"
    );

    // Get the variable name as a string, then try to get the variable from env.
    let var_name = &instr[var_name_start..var_name_stop];

    // It's an error if the name is empty.
    if var_name.is_empty() {
        if let Some(ref mut errors) = errors {
            parse_util_expand_variable_error(
                &instr,
                0, /* global_token_pos */
                varexp_char_idx,
                errors,
            );
        }
        return ExpandResult::make_error(STATUS_EXPAND_ERROR);
    }

    // Do a dirty hack to make sliced history fast (#4650). We expand from either a variable, or a
    // history_t. Note that "history" is read only in env.rs so it's safe to special-case it in
    // this way (it cannot be shadowed, etc).
    let mut history = None;
    let mut var = None;
    if var_name == "history" {
        history = Some(History::with_name(&history_session_id(vars)));
    } else if var_name.as_char_slice() != [VARIABLE_EXPAND_EMPTY] {
        var = vars.get(var_name);
    }

    // Parse out any following slice.
    // Record the end of the variable name and any following slice.
    let mut var_name_and_slice_stop = var_name_stop;
    let mut all_values = true;
    let slice_start = var_name_stop;
    let mut var_idx_list = vec![];

    if instr.as_char_slice().get(slice_start) == Some(&'[') {
        all_values = false;
        // If a variable is missing, behave as though we have one value, so that $var[1] always
        // works.
        let mut effective_val_count = 1;
        if let Some(ref var) = var {
            effective_val_count = var.as_list().len();
        } else if let Some(ref history) = history {
            effective_val_count = history.size();
        }
        match parse_slice(
            &instr[slice_start..],
            &mut var_idx_list,
            effective_val_count,
        ) {
            Ok(offset) => {
                var_name_and_slice_stop = slice_start + offset;
            }
            Err((bad_pos, error)) => {
                match error {
                    ParseSliceError::zero_index => {
                        append_syntax_error!(
                            errors,
                            slice_start + bad_pos,
                            "array indices start at 1, not 0."
                        );
                    }
                    ParseSliceError::invalid_index => {
                        append_syntax_error!(errors, slice_start + bad_pos, "Invalid index value");
                    }
                }
                return ExpandResult::make_error(STATUS_EXPAND_ERROR);
            }
        }
    }
    let var_idx_list = var_idx_list.iter().filter_map(|&n| n.try_into().ok());

    if var.is_none() && history.is_none() {
        // Expanding a non-existent variable.
        if !is_single {
            // Normal expansions of missing variables successfully expand to nothing.
            return ExpandResult::ok();
        } else {
            // Expansion to single argument.
            // Replace the variable name and slice with VARIABLE_EXPAND_EMPTY.
            let mut res = instr[..varexp_char_idx].to_owned();
            if res.as_char_slice().last() == Some(&VARIABLE_EXPAND_SINGLE) {
                res.push(VARIABLE_EXPAND_EMPTY);
            }
            res.push_utfstr(&instr[var_name_and_slice_stop..]);
            return expand_variables(res, out, varexp_char_idx, vars, errors);
        }
    }

    // Ok, we have a variable or a history. Let's expand it.
    // Start by respecting the sliced elements.
    assert!(
        var.is_some() || history.is_some(),
        "Should have variable or history here",
    );
    let mut var_item_list = vec![];
    if all_values {
        var_item_list = if let Some(ref history) = history {
            history.get_history()
        } else {
            var.as_ref().unwrap().as_list().to_vec()
        };
    } else {
        // We have to respect the slice.
        if let Some(ref history) = history {
            // Ask history to map indexes to item strings.
            // Note this may have missing entries for out-of-bounds.
            let item_map = history.items_at_indexes(var_idx_list.clone());
            for item_index in var_idx_list {
                if let Some(item) = item_map.get(&item_index) {
                    var_item_list.push(item.clone());
                }
            }
        } else {
            let all_var_items = var.as_ref().unwrap().as_list();
            for item_index in var_idx_list {
                // Check that we are within array bounds. If not, skip the element. Note:
                // Negative indices (`echo $foo[-1]`) are already converted to positive ones
                // here, So tmp < 1 means it's definitely not in.
                // Note we are 1-based.
                if item_index >= 1 && item_index <= all_var_items.len() {
                    var_item_list.push(all_var_items[item_index - 1].to_owned());
                }
            }
        }
    }

    if is_single {
        // Quoted expansion. Here we expect the variable's delimiter.
        // Note history always has a space delimiter.
        let delimit = if history.is_some() {
            ' '
        } else {
            var.as_ref().unwrap().get_delimiter()
        };
        let mut res = instr[..varexp_char_idx].to_owned();
        if !res.is_empty() {
            if res.as_char_slice().last() != Some(&VARIABLE_EXPAND_SINGLE) {
                res.push(INTERNAL_SEPARATOR);
            } else if var_item_list.is_empty() || var_item_list[0].is_empty() {
                // First expansion is empty, but we need to recursively expand.
                res.push(VARIABLE_EXPAND_EMPTY);
            }
        }

        // Append all entries in var_item_list, separated by the delimiter.
        res.push_utfstr(&join_strings(&var_item_list, delimit));
        res.push_utfstr(&instr[var_name_and_slice_stop..]);
        return expand_variables(res, out, varexp_char_idx, vars, errors);
    } else {
        // Normal cartesian-product expansion.
        for item in var_item_list {
            if varexp_char_idx == 0 && var_name_and_slice_stop == instr.len() {
                if !out.add(item) {
                    return append_overflow_error(errors, None);
                }
            } else {
                let mut new_in = instr[..varexp_char_idx].to_owned();
                if !new_in.is_empty() {
                    if new_in.as_char_slice().last() != Some(&VARIABLE_EXPAND) {
                        new_in.push(INTERNAL_SEPARATOR);
                    } else if item.is_empty() {
                        new_in.push(VARIABLE_EXPAND_EMPTY);
                    }
                }
                new_in.push_utfstr(&item);
                new_in.push_utfstr(&instr[var_name_and_slice_stop..]);
                let res = expand_variables(new_in, out, varexp_char_idx, vars, errors);
                if res.result != ExpandResultCode::ok {
                    return res;
                }
            }
        }
    }

    ExpandResult::ok()
}

/// Perform brace expansion, placing the expanded strings into `out`.
fn expand_braces(
    input: WString,
    flags: ExpandFlags,
    out: &mut CompletionReceiver,
    errors: &mut Option<&mut ParseErrorList>,
) -> ExpandResult {
    let mut syntax_error = false;
    let mut brace_count = 0;

    let mut brace_begin = None;
    let mut brace_end = None;
    let mut last_sep = None;

    // Locate the first non-nested brace pair.
    for (pos, c) in input.chars().enumerate() {
        match c {
            BRACE_BEGIN => {
                if brace_count == 0 {
                    brace_begin = Some(pos);
                }
                brace_count += 1;
            }
            BRACE_END => {
                brace_count -= 1;
                #[allow(clippy::comparison_chain)]
                if brace_count < 0 {
                    syntax_error = true;
                } else if brace_count == 0 {
                    brace_end = Some(pos);
                }
            }
            BRACE_SEP => {
                if brace_count == 1 {
                    last_sep = Some(pos);
                }
            }
            _ => {
                // we ignore all other characters here
            }
        }
    }

    if brace_count > 0 {
        if !flags.contains(ExpandFlags::FOR_COMPLETIONS) {
            syntax_error = true;
        } else {
            // The user hasn't typed an end brace yet; make one up and append it, then expand
            // that.
            let mut synth = WString::new();
            if let Some(last_sep) = last_sep {
                synth.push_utfstr(&input[..brace_begin.unwrap() + 1]);
                synth.push_utfstr(&input[last_sep + 1..]);
                synth.push(BRACE_END);
            } else {
                synth.push_utfstr(&input);
                synth.push(BRACE_END);
            }

            // Note: this code looks very fishy, apparently it has never worked.
            return expand_braces(synth, ExpandFlags::FAIL_ON_CMDSUBST, out, errors);
        }
    }

    if syntax_error {
        append_syntax_error!(errors, SOURCE_LOCATION_UNKNOWN, "Mismatched braces");
        return ExpandResult::make_error(STATUS_EXPAND_ERROR);
    }

    let Some(brace_begin) = brace_begin else {
        // No more brace expansions left; we can return the value as-is.
        if !out.add(input) {
            return append_overflow_error(errors, None);
        }
        return ExpandResult::ok();
    };
    let brace_end = brace_end.unwrap();

    let length_preceding_braces = brace_begin;
    let length_following_braces = input.len() - brace_end - 1;
    let tot_len = length_preceding_braces + length_following_braces;
    let mut item_begin = brace_begin + 1;
    for (pos, c) in input.chars().enumerate().skip(brace_begin + 1) {
        if brace_count == 0 && (c == BRACE_SEP || pos == brace_end) {
            assert!(pos >= item_begin);
            let item_len = pos - item_begin;
            let item = input[item_begin..pos].to_owned();
            let mut item = trim(item, Some(wstr::from_char_slice(&[BRACE_SPACE, '\0'])));
            for c in item.as_char_slice_mut() {
                if *c == BRACE_SPACE {
                    *c = ' ';
                }
            }

            // `whole_item` is a whitespace- and brace-stripped member of a single pass of brace
            // expansion, e.g. in `{ alpha , b,{c, d }}`, `alpha`, `b`, and `c, d` will, in the
            // first round of expansion, each in turn be a `whole_item` (with recursive commas
            // replaced by special placeholders).
            // We recursively call `expand_braces` with each item until it's been fully expanded.
            let mut whole_item = WString::new();
            whole_item.reserve(tot_len + item_len + 2);
            whole_item.push_utfstr(&input[..length_preceding_braces]);
            whole_item.push_utfstr(&item);
            whole_item.push_utfstr(&input[brace_end + 1..]);
            let _ = expand_braces(whole_item, flags, out, errors);

            item_begin = pos + 1;
            if pos == brace_end {
                break;
            }
        }

        if c == BRACE_BEGIN {
            brace_count += 1;
        }

        if c == BRACE_END {
            brace_count -= 1;
        }
    }

    ExpandResult::ok()
}

/// Expand a command substitution `input`, executing on `ctx`, and inserting the results into
/// `out_list`, or any errors into `errors`. Return an expand result.
pub fn expand_cmdsubst(
    input: WString,
    ctx: &OperationContext,
    out: &mut CompletionReceiver,
    errors: &mut Option<&mut ParseErrorList>,
) -> ExpandResult {
    let mut cursor = 0;
    let mut is_quoted = false;
    let mut has_dollar = false;
    let parens = match parse_util_locate_cmdsubst_range(
        &input,
        &mut cursor,
        false,
        Some(&mut is_quoted),
        Some(&mut has_dollar),
    ) {
        MaybeParentheses::Error => {
            append_syntax_error!(errors, SOURCE_LOCATION_UNKNOWN, "Mismatched parenthesis");
            return ExpandResult::make_error(STATUS_EXPAND_ERROR);
        }
        MaybeParentheses::None => {
            if !out.add(input) {
                return append_overflow_error(errors, None);
            }
            return ExpandResult::ok();
        }
        MaybeParentheses::CommandSubstitution(parens) => parens,
    };

    let mut sub_res = vec![];
    let job_group = ctx.job_group.clone();
    let subshell_status = exec_subshell_for_expand(
        &input[parens.command()],
        ctx.parser(),
        job_group.as_ref(),
        &mut sub_res,
    );

    if let Err(subshell_status) = subshell_status {
        // TODO: Ad-hoc switch, how can we enumerate the possible errors more safely?
        let err = match subshell_status {
            _ if subshell_status == STATUS_READ_TOO_MUCH => {
                wgettext!("Too much data emitted by command substitution so it was discarded")
            }
            // TODO: STATUS_CMD_ERROR is overused and too generic. We shouldn't have to test things
            // to figure out what error to show after we've already been given an error code.
            _ if subshell_status == STATUS_CMD_ERROR => {
                if ctx.parser().is_eval_depth_exceeded() {
                    wgettext!("Unable to evaluate string substitution")
                } else {
                    wgettext!("Too many active file descriptors")
                }
            }
            _ if subshell_status == STATUS_CMD_UNKNOWN => {
                wgettext!("Unknown command")
            }
            _ if subshell_status == STATUS_ILLEGAL_CMD => {
                wgettext!("Commandname was invalid")
            }
            _ if subshell_status == STATUS_NOT_EXECUTABLE => {
                wgettext!("Command not executable")
            }
            _ if subshell_status == STATUS_INVALID_ARGS => {
                // TODO: Also overused
                // This is sent for:
                // invalid redirections or pipes (like `<&foo`),
                // invalid variables (invalid name or read-only) for for-loops,
                // switch $foo if $foo expands to more than one argument
                // time in a background job.
                wgettext!("Invalid arguments")
            }
            _ if subshell_status == STATUS_EXPAND_ERROR => {
                // Sent in `for $foo in ...` if $foo expands to more than one word
                wgettext!("Expansion error")
            }
            _ if subshell_status == STATUS_UNMATCHED_WILDCARD => {
                // Sent in `for $foo in ...` if $foo expands to more than one word
                wgettext!("Unmatched wildcard")
            }
            _ => {
                wgettext!("Unknown error while evaluating command substitution")
            }
        };
        append_cmdsub_error_formatted!(errors, parens.start(), parens.end() - 1, err.to_owned());
        return ExpandResult::make_error(subshell_status);
    }

    // Expand slices like (cat /var/words)[1]
    let mut tail_begin = parens.end();
    if input.as_char_slice().get(tail_begin) == Some(&'[') {
        let mut slice_idx = vec![];
        let slice_begin = tail_begin;
        let slice_end = match parse_slice(&input[slice_begin..], &mut slice_idx, sub_res.len()) {
            Ok(offset) => slice_begin + offset,
            Err((bad_pos, error)) => {
                match error {
                    ParseSliceError::zero_index => {
                        append_syntax_error!(
                            errors,
                            slice_begin + bad_pos,
                            "array indices start at 1, not 0."
                        );
                    }
                    ParseSliceError::invalid_index => {
                        append_syntax_error!(errors, slice_begin + bad_pos, "Invalid index value");
                    }
                }
                return ExpandResult::make_error(STATUS_EXPAND_ERROR);
            }
        };

        let mut sub_res2 = vec![];
        tail_begin = slice_end;
        for idx in slice_idx {
            if idx as usize > sub_res.len() || idx < 1 {
                continue;
            }
            // -1 to convert from 1-based slice index to 0-based vector index.
            sub_res2.push(sub_res[idx as usize - 1].to_owned());
        }
        sub_res = sub_res2;
    }

    // Recursively call ourselves to expand any remaining command substitutions. The result of this
    // recursive call using the tail of the string is inserted into the tail_expand array list
    let mut tail_expand_recv = out.subreceiver();
    let mut tail = input[tail_begin..].to_owned();
    // A command substitution inside double quotes magically closes the quoted string.
    // Reopen the quotes just after the command substitution.
    if is_quoted {
        tail.insert(0, '"');
    }

    let _ = expand_cmdsubst(tail, ctx, &mut tail_expand_recv, errors); // TODO: offset error locations
    let tail_expand = tail_expand_recv.take();

    // Combine the result of the current command substitution with the result of the recursive tail
    // expansion.

    if is_quoted {
        // Awkwardly reconstruct the command output.
        let approx_size = sub_res.iter().map(|sub_item| sub_item.len() + 1).sum();
        let mut sub_res_joined = WString::new();
        sub_res_joined.reserve(approx_size);
        for line in sub_res {
            sub_res_joined.push_utfstr(&escape_string_for_double_quotes(&line));
            sub_res_joined.push('\n');
        }
        // Mimic POSIX shells by stripping all trailing newlines.
        if !sub_res_joined.is_empty() {
            let mut i = sub_res_joined.len();
            while i > 0 && sub_res_joined.as_char_slice()[i - 1] == '\n' {
                i -= 1;
            }
            sub_res_joined.truncate(i);
        }
        // Instead of performing cartesian product expansion, we directly insert the command
        // substitution output into the current expansion results.
        for tail_item in tail_expand {
            let mut whole_item = WString::new();
            whole_item.reserve(
                parens.start() + 1 + sub_res_joined.len() + 1 + tail_item.completion.len(),
            );
            whole_item.push_utfstr(&input[..parens.start() - if has_dollar { 1 } else { 0 }]);
            whole_item.push(INTERNAL_SEPARATOR);
            whole_item.push_utfstr(&sub_res_joined);
            whole_item.push(INTERNAL_SEPARATOR);
            whole_item.push_utfstr(&tail_item.completion["\"".len()..]);
            if !out.add(whole_item) {
                return append_overflow_error(errors, None);
            }
        }

        return ExpandResult::ok();
    }

    for sub_item in sub_res {
        let sub_item2 = escape_string(&sub_item, EscapeStringStyle::Script(EscapeFlags::COMMA));
        for tail_item in &*tail_expand {
            let mut whole_item = WString::new();
            whole_item
                .reserve(parens.start() + 1 + sub_item2.len() + 1 + tail_item.completion.len());
            whole_item.push_utfstr(&input[..parens.start() - if has_dollar { 1 } else { 0 }]);
            whole_item.push(INTERNAL_SEPARATOR);
            whole_item.push_utfstr(&sub_item2);
            whole_item.push(INTERNAL_SEPARATOR);
            whole_item.push_utfstr(&tail_item.completion);
            if !out.add(whole_item) {
                return append_overflow_error(errors, None);
            }
        }
    }

    ExpandResult::ok()
}

// Given that input[0] is HOME_DIRECTORY or tilde (ugh), return the user's name. Return the empty
// string if it is just a tilde. Also return by reference the index of the first character of the
// remaining part of the string (e.g. the subsequent slash).
fn get_home_directory_name<'a>(input: &'a wstr, out_tail_idx: &mut usize) -> &'a wstr {
    assert!([HOME_DIRECTORY, '~'].contains(&input.as_char_slice()[0]));
    // We get the position of the /, but we need to remove it as well.
    if let Some(pos) = input.chars().position(|c| c == '/') {
        *out_tail_idx = pos;
        &input[1..pos]
    } else {
        *out_tail_idx = input.len();
        &input[1..]
    }
}

/// Attempts tilde expansion of the string specified, modifying it in place.
fn expand_home_directory(input: &mut WString, vars: &dyn Environment) {
    let starts_with_tilde = input.as_char_slice().first() == Some(&HOME_DIRECTORY);
    *input = input.replace([HOME_DIRECTORY], L!("~"));
    if !starts_with_tilde {
        return;
    }

    let mut tail_idx = usize::MAX;
    let username = get_home_directory_name(input, &mut tail_idx);
    let mut home = None;
    if username.is_empty() {
        // Current users home directory.
        match vars.get_unless_empty(L!("HOME")) {
            None => {
                input.clear();
                return;
            }
            Some(home_var) => {
                home = Some(home_var.as_string());
                tail_idx = 1;
            }
        };
    } else {
        // Some other user's home directory.
        let name_cstr = wcs2zstring(username);
        let mut userinfo = MaybeUninit::uninit();
        let mut result: *mut libc::passwd = std::ptr::null_mut();
        let mut buf = [0 as libc::c_char; 8192];
        let retval = unsafe {
            libc::getpwnam_r(
                name_cstr.as_ptr(),
                userinfo.as_mut_ptr(),
                &mut buf[0],
                std::mem::size_of_val(&buf),
                &mut result,
            )
        };
        if retval == 0 && !result.is_null() {
            let userinfo = unsafe { userinfo.assume_init() };
            home = Some(charptr2wcstring(userinfo.pw_dir));
        }
    }

    if let Some(home) = home {
        input.replace_range(..tail_idx, &normalize_path(&home, true));
    }
}

/// Expand the %self escape. Note this can only come at the beginning of the string.
fn expand_percent_self(input: &mut WString) {
    if input.as_char_slice().first() == Some(&PROCESS_EXPAND_SELF) {
        input.replace_range(0..1, &crate::nix::getpid().to_wstring());
    }
}

/// Remove any internal separators. Also optionally convert wildcard characters to regular
/// equivalents. This is done to support skip_wildcards.
fn remove_internal_separator(s: &mut WString, conv: bool) {
    // Remove all instances of INTERNAL_SEPARATOR.
    s.retain(|c| c != INTERNAL_SEPARATOR);

    // If conv is true, replace all instances of ANY_STRING with '*',
    // ANY_STRING_RECURSIVE with '*'.
    if conv {
        for idx in s.as_char_slice_mut() {
            match *idx {
                ANY_CHAR => {
                    *idx = '?';
                }
                ANY_STRING | ANY_STRING_RECURSIVE => {
                    *idx = '*';
                }
                _ => {
                    // we ignore all other characters
                }
            }
        }
    }
}

/// A type that knows how to perform expansions.
struct Expander<'a, 'b, 'c> {
    /// Operation context for this expansion.
    ctx: &'c OperationContext<'b>,

    /// Flags to use during expansion.
    flags: ExpandFlags,

    /// List to receive any errors generated during expansion, or null to ignore errors.
    errors: &'c mut Option<&'a mut ParseErrorList>,
}

impl<'a, 'b, 'c> Expander<'a, 'b, 'c> {
    fn new(
        ctx: &'c OperationContext<'b>,
        flags: ExpandFlags,
        errors: &'c mut Option<&'a mut ParseErrorList>,
    ) -> Self {
        Self { ctx, flags, errors }
    }

    fn expand_string(
        input: WString,
        out_completions: &'a mut CompletionReceiver,
        flags: ExpandFlags,
        ctx: &'a OperationContext<'b>,
        mut errors: Option<&'a mut ParseErrorList>,
    ) -> ExpandResult {
        assert!(
            flags.contains(ExpandFlags::FAIL_ON_CMDSUBST) || ctx.has_parser(),
            "Must have a parser if not skipping command substitutions"
        );
        // Early out. If we're not completing, and there's no magic in the input, we're done.
        if !flags.contains(ExpandFlags::FOR_COMPLETIONS) && expand_is_clean(&input) {
            if !out_completions.add(input) {
                return append_overflow_error(&mut errors, None);
            }
            return ExpandResult::ok();
        }

        let mut expand = Expander::new(ctx, flags, &mut errors);

        // Our expansion stages.
        // An expansion stage is a member function pointer.
        // It accepts the input string (transferring ownership) and returns the list of output
        // completions by reference. It may return an error, which halts expansion.
        let stages = [
            Expander::stage_cmdsubst,
            Expander::stage_variables,
            Expander::stage_braces,
            Expander::stage_home_and_self,
            Expander::stage_wildcards,
        ];

        // Load up our single initial completion.
        let mut completions = vec![Completion::from_completion(input.clone())];

        let mut total_result = ExpandResult::ok();
        let mut output_storage = out_completions.subreceiver();
        for stage in stages {
            for comp in completions {
                if expand.ctx.check_cancel() {
                    total_result = ExpandResult::new(ExpandResultCode::cancel);
                    break;
                }
                let this_result = (stage)(&mut expand, comp.completion, &mut output_storage);
                total_result = this_result;
                if matches!(
                    total_result.result,
                    ExpandResultCode::error | ExpandResultCode::overflow
                ) {
                    break;
                }
            }

            // Output becomes our next stage's input.
            completions = output_storage.take();
            if matches!(
                total_result.result,
                ExpandResultCode::error | ExpandResultCode::overflow
            ) {
                break;
            }
        }

        // This is a little tricky: if one wildcard failed to match but we still got output, it
        // means that a previous expansion resulted in multiple strings. For example:
        //   set dirs ./a ./b
        //   echo $dirs/*.txt
        // Here if ./a/*.txt matches and ./b/*.txt does not, then we don't want to report a failed
        // wildcard. So swallow failed-wildcard errors if we got any output.
        if total_result == ExpandResultCode::wildcard_no_match && !completions.is_empty() {
            total_result = ExpandResult::ok();
        }

        if total_result == ExpandResultCode::ok {
            // Unexpand tildes if we want to preserve them (see #647).
            if flags.contains(ExpandFlags::PRESERVE_HOME_TILDES) {
                expand.unexpand_tildes(&input, &mut completions);
            }
            if !out_completions.extend(completions) {
                total_result = append_overflow_error(expand.errors, None);
            }
        }

        total_result
    }

    fn stage_cmdsubst(&mut self, input: WString, out: &mut CompletionReceiver) -> ExpandResult {
        if self.flags.contains(ExpandFlags::SKIP_CMDSUBST) {
            if !out.add(input) {
                return append_overflow_error(self.errors, None);
            }
            return ExpandResult::ok();
        }
        if self.flags.contains(ExpandFlags::FAIL_ON_CMDSUBST) {
            let mut cursor = 0;
            match parse_util_locate_cmdsubst_range(&input, &mut cursor, true, None, None) {
                MaybeParentheses::Error => {
                    return ExpandResult::make_error(STATUS_EXPAND_ERROR);
                }
                MaybeParentheses::None => {
                    if !out.add(input) {
                        return append_overflow_error(self.errors, None);
                    }
                    return ExpandResult::ok();
                }
                MaybeParentheses::CommandSubstitution(parens) => {
                    append_cmdsub_error!(
                                self.errors,
                                parens.start(),
                                parens.end()-1,
                                "command substitutions not allowed in command position. Try var=(your-cmd) $var ..."
                            );
                    return ExpandResult::make_error(STATUS_EXPAND_ERROR);
                }
            }
        } else {
            assert!(
                self.ctx.has_parser(),
                "Must have a parser to expand command substitutions"
            );
            expand_cmdsubst(input, self.ctx, out, self.errors)
        }
    }

    // We pass by value to match other stages. NOLINTNEXTLINE(performance-unnecessary-value-param)
    fn stage_variables(&mut self, input: WString, out: &mut CompletionReceiver) -> ExpandResult {
        // We accept incomplete strings here, since complete uses expand_string to expand incomplete
        // strings from the commandline.
        let mut next = unescape_string(
            &input,
            UnescapeStringStyle::Script(UnescapeFlags::SPECIAL | UnescapeFlags::INCOMPLETE),
        )
        .unwrap_or_default();

        if self.flags.contains(ExpandFlags::SKIP_VARIABLES) {
            for i in next.as_char_slice_mut() {
                if [VARIABLE_EXPAND, VARIABLE_EXPAND_SINGLE].contains(i) {
                    *i = '$';
                }
            }
            if !out.add(next) {
                return append_overflow_error(self.errors, None);
            }
            ExpandResult::ok()
        } else {
            let size = next.len();
            expand_variables(next, out, size, self.ctx.vars(), self.errors)
        }
    }

    fn stage_braces(&mut self, input: WString, out: &mut CompletionReceiver) -> ExpandResult {
        expand_braces(input, self.flags, out, self.errors)
    }

    fn stage_home_and_self(
        &mut self,
        mut input: WString,
        out: &mut CompletionReceiver,
    ) -> ExpandResult {
        expand_home_directory(&mut input, self.ctx.vars());
        if !feature_test(FeatureFlag::remove_percent_self) {
            expand_percent_self(&mut input);
        }
        if !out.add(input) {
            return append_overflow_error(self.errors, None);
        }
        ExpandResult::ok()
    }

    fn stage_wildcards(
        &mut self,
        mut path_to_expand: WString,
        out: &mut CompletionReceiver,
    ) -> ExpandResult {
        let mut result = ExpandResult::ok();

        remove_internal_separator(
            &mut path_to_expand,
            self.flags.contains(ExpandFlags::SKIP_WILDCARDS),
        );
        let has_wildcard = wildcard_has_internal(&path_to_expand); // e.g. ANY_STRING
        let for_completions = self.flags.contains(ExpandFlags::FOR_COMPLETIONS);
        let skip_wildcards = self.flags.contains(ExpandFlags::SKIP_WILDCARDS);

        if has_wildcard && self.flags.contains(ExpandFlags::EXECUTABLES_ONLY) {
            // don't do wildcard expansion for executables, see issue #785
        } else if (for_completions && !skip_wildcards) || has_wildcard {
            // We either have a wildcard, or we don't have a wildcard but we're doing completion
            // expansion (so we want to get the completion of a file path). Note that if
            // skip_wildcards is set, we stomped wildcards in remove_internal_separator above, so
            // there actually aren't any.
            //
            // So we're going to treat this input as a file path. Compute the "working directories",
            // which may be CDPATH if the special flag is set.
            let working_dir = self.ctx.vars().get_pwd_slash();
            let mut effective_working_dirs = vec![];
            let for_cd = self.flags.contains(ExpandFlags::SPECIAL_FOR_CD);
            let for_command = self.flags.contains(ExpandFlags::SPECIAL_FOR_COMMAND);
            if !for_cd && !for_command {
                // Common case.
                effective_working_dirs.push(working_dir);
            } else {
                // Either special_for_command or special_for_cd. We can handle these
                // mostly the same. There's the following differences:
                //
                // 1. An empty CDPATH should be treated as '.', but an empty PATH should be left empty
                // (no commands can be found). Also, an empty element in either is treated as '.' for
                // consistency with POSIX shells. Note that we rely on the latter by having called
                // `munge_colon_delimited_array()` for these special env vars. Thus we do not
                // special-case them here.
                //
                // 2. PATH is only "one level," while CDPATH is multiple levels. That is, input like
                // 'foo/bar' should resolve against CDPATH, but not PATH.
                //
                // In either case, we ignore the path if we start with ./ or /. Also ignore it if we are
                // doing command completion and we contain a slash, per IEEE 1003.1, chapter 8 under
                // PATH.
                if path_to_expand.starts_with(L!("/"))
                    || path_to_expand.starts_with(L!("./"))
                    || path_to_expand.starts_with(L!("../"))
                    || (for_command && path_to_expand.contains('/'))
                {
                    effective_working_dirs.push(working_dir);
                } else {
                    // Get the PATH/CDPATH and CWD. Perhaps these should be passed in. An empty CDPATH
                    // implies just the current directory, while an empty PATH is left empty.
                    let mut paths = self
                        .ctx
                        .vars()
                        .get(if for_cd { L!("CDPATH") } else { L!("PATH") })
                        .map(|var| var.as_list().to_owned())
                        .unwrap_or_default();

                    // The current directory is always valid.
                    paths.push(if for_cd { L!(".") } else { L!("") }.to_owned());
                    for next_path in paths {
                        effective_working_dirs
                            .push(path_apply_working_directory(&next_path, &working_dir));
                    }
                }
            }

            result = ExpandResult::new(ExpandResultCode::wildcard_no_match);
            let mut expanded_recv = out.subreceiver();
            for effective_working_dir in effective_working_dirs {
                let expand_res = wildcard_expand_string(
                    &path_to_expand,
                    &effective_working_dir,
                    self.flags,
                    &*self.ctx.cancel_checker,
                    &mut expanded_recv,
                );
                match expand_res {
                    WildcardResult::Match => result = ExpandResult::ok(),
                    WildcardResult::NoMatch => (),
                    WildcardResult::Overflow => return append_overflow_error(self.errors, None),
                    WildcardResult::Cancel => return ExpandResult::new(ExpandResultCode::cancel),
                }
            }

            let mut expanded = expanded_recv.take();
            expanded.sort_by(|a, b| wcsfilecmp_glob(&a.completion, &b.completion));
            if !out.extend(expanded) {
                result = ExpandResult::new(ExpandResultCode::overflow);
            }
        } else {
            // Can't fully justify this check. I think it's that SKIP_WILDCARDS is used when completing
            // to mean don't do file expansions, so if we're not doing file expansions, just drop this
            // completion on the floor.
            #[allow(clippy::collapsible_if)]
            if !self.flags.contains(ExpandFlags::FOR_COMPLETIONS) {
                if !out.add(path_to_expand) {
                    return append_overflow_error(self.errors, None);
                }
            }
        }
        result
    }

    // Given an original input string, if it starts with a tilde, "unexpand" the expanded home
    // directory.  Note this may be just a tilde or a user name like ~foo/.
    fn unexpand_tildes(&self, input: &wstr, completions: &mut CompletionList) {
        // If input begins with tilde, then try to replace the corresponding string in each completion
        // with the tilde. If it does not, there's nothing to do.
        if input.as_char_slice().first() != Some(&'~') {
            return;
        }

        // This is a subtle kludge. We need to decide whether to unexpand tildes for all
        // completions, or only those which replace their tokens. The problem is that we're sloppy
        // about setting the COMPLETE_REPLACES_TOKEN flag, except when we're completing in the
        // wildcard stage, because no other clients of string expansion care. Example:
        //   HOME=/foo
        //   mkdir ~/foo # makes /foo/foo
        //   cd ~/<tab>
        // Here we are likely to get a completion 'foo' which may match $HOME, but it extends its token
        // instead of replacing it, so we don't modify it (it will just be appended to the original ~/).
        //
        // However if we are not completing, just expanding, then expansion just produces the full paths
        // so we should unconditionally unexpand tildes.
        let only_replacers = self.flags.contains(ExpandFlags::FOR_COMPLETIONS);

        // Helper to decide whether to process a completion.
        let should_process = |c: &Completion| !only_replacers || c.replaces_token();

        // Early out if none qualify.
        if !completions.iter().any(should_process) {
            return;
        }

        // Get the username_with_tilde (like ~bert) and expand it into a home directory.
        let mut tail_idx = usize::MAX;
        let username_with_tilde =
            WString::from_str("~") + get_home_directory_name(input, &mut tail_idx);
        let mut home = username_with_tilde.clone();
        expand_tilde(&mut home, self.ctx.vars());

        // Now for each completion that starts with home, replace it with the username_with_tilde.
        for comp in completions {
            if should_process(comp) && comp.completion.starts_with(&home) {
                comp.completion
                    .replace_range(..home.len(), &username_with_tilde);

                // And mark that our tilde is literal, so it doesn't try to escape it.
                comp.flags |= CompleteFlags::DONT_ESCAPE_TILDES;
            }
        }
    }
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum ExpandResultCode {
    /// There was an error, for example, unmatched braces.
    error,
    /// Expansion would exceed the maximum number of elements.
    overflow,
    /// Expansion succeeded.
    ok,
    /// Expansion was cancelled (e.g. control-C).
    cancel,
    /// Expansion succeeded, but a wildcard in the string matched no files,
    /// so the output is empty.
    wildcard_no_match,
}

/// These are the possible return values for expand_string.
#[must_use]
#[derive(Debug)]
pub struct ExpandResult {
    /// The result of expansion.
    pub result: ExpandResultCode,

    /// If expansion resulted in an error, this is an appropriate value with which to populate
    /// $status.
    // todo!("should be c_int?");
    pub status: i32,
}
