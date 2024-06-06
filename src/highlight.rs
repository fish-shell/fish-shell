//! Functions for syntax highlighting.
use crate::abbrs::{self, with_abbrs};
use crate::ast::{
    self, Argument, Ast, BlockStatement, BlockStatementHeaderVariant, DecoratedStatement, Keyword,
    Leaf, List, Node, NodeVisitor, Redirection, Token, Type, VariableAssignment,
};
use crate::builtins::shared::builtin_exists;
use crate::color::RgbColor;
use crate::common::{
    unescape_string, valid_var_name, valid_var_name_char, UnescapeFlags, ASCII_MAX,
    EXPAND_RESERVED_BASE, EXPAND_RESERVED_END,
};
use crate::env::Environment;
use crate::expand::{
    expand_one, expand_tilde, expand_to_command_and_args, ExpandFlags, ExpandResultCode,
    HOME_DIRECTORY, PROCESS_EXPAND_SELF_STR,
};
use crate::expand::{
    BRACE_BEGIN, BRACE_END, BRACE_SEP, INTERNAL_SEPARATOR, PROCESS_EXPAND_SELF, VARIABLE_EXPAND,
    VARIABLE_EXPAND_SINGLE,
};
use crate::function;
use crate::future_feature_flags::{feature_test, FeatureFlag};
use crate::history::{all_paths_are_valid, HistoryItem};
use crate::libc::_PC_CASE_SENSITIVE;
use crate::operation_context::OperationContext;
use crate::output::{parse_color, Outputter};
use crate::parse_constants::{
    ParseKeyword, ParseTokenType, ParseTreeFlags, SourceRange, StatementDecoration,
};
use crate::parse_util::{
    parse_util_locate_cmdsubst_range, parse_util_slice_length, MaybeParentheses,
};
use crate::path::{
    path_apply_working_directory, path_as_implicit_cd, path_get_cdpath, path_get_path,
    paths_are_same_file,
};
use crate::redirection::RedirectionMode;
use crate::threads::assert_is_background_thread;
use crate::tokenizer::{variable_assignment_equals_pos, PipeOrRedir};
use crate::wchar::{wstr, WString, L};
use crate::wchar_ext::WExt;
use crate::wcstringutil::{
    string_prefixes_string, string_prefixes_string_case_insensitive, string_suffixes_string,
};
use crate::wildcard::{ANY_CHAR, ANY_STRING, ANY_STRING_RECURSIVE};
use crate::wutil::dir_iter::DirIter;
use crate::wutil::fish_wcstoi;
use crate::wutil::{normalize_path, waccess, wstat};
use crate::wutil::{wbasename, wdirname};
use bitflags::bitflags;
use libc::{ENOENT, PATH_MAX, R_OK, W_OK};
use std::collections::hash_map::Entry;
use std::collections::{HashMap, HashSet};
use std::os::fd::RawFd;

impl HighlightSpec {
    pub fn new() -> Self {
        Self::default()
    }
    pub fn with_fg(fg: HighlightRole) -> Self {
        Self::with_fg_bg(fg, HighlightRole::normal)
    }
    pub fn with_fg_bg(fg: HighlightRole, bg: HighlightRole) -> Self {
        Self {
            foreground: fg,
            background: bg,
            ..Default::default()
        }
    }
    pub fn with_bg(bg: HighlightRole) -> Self {
        Self::with_fg_bg(HighlightRole::normal, bg)
    }
}

/// Given a string and list of colors of the same size, return the string with ANSI escape sequences
/// representing the colors.
pub fn colorize(text: &wstr, colors: &[HighlightSpec], vars: &dyn Environment) -> Vec<u8> {
    assert!(colors.len() == text.len());
    let mut rv = HighlightColorResolver::new();
    let mut outp = Outputter::new_buffering();

    let mut last_color = HighlightSpec::with_fg(HighlightRole::normal);
    for (i, c) in text.chars().enumerate() {
        let color = colors[i];
        if color != last_color {
            outp.set_color(rv.resolve_spec(&color, false, vars), RgbColor::NORMAL);
            last_color = color;
        }
        outp.writech(c);
    }
    outp.set_color(RgbColor::NORMAL, RgbColor::NORMAL);
    outp.contents().to_owned() // TODO should move
}

/// Perform syntax highlighting for the shell commands in buff. The result is stored in the color
/// array as a color_code from the HIGHLIGHT_ enum for each character in buff.
///
/// \param buffstr The buffer on which to perform syntax highlighting
/// \param color The array in which to store the color codes. The first 8 bits are used for fg
/// color, the next 8 bits for bg color.
/// \param ctx The variables and cancellation check for this operation.
/// \param io_ok If set, allow IO which may block. This means that e.g. invalid commands may be
/// detected.
/// \param cursor The position of the cursor in the commandline.
pub fn highlight_shell(
    buff: &wstr,
    color: &mut Vec<HighlightSpec>,
    ctx: &OperationContext<'_>,
    io_ok: bool, /* = false */
    cursor: Option<usize>,
) {
    let working_directory = ctx.vars().get_pwd_slash();
    let mut highlighter = Highlighter::new(buff, cursor, ctx, working_directory, io_ok);
    *color = highlighter.highlight();
}

/// highlight_color_resolver_t resolves highlight specs (like "a command") to actual RGB colors.
/// It maintains a cache with no invalidation mechanism. The lifetime of these should typically be
/// one screen redraw.
#[derive(Default)]
pub struct HighlightColorResolver {
    fg_cache: HashMap<HighlightSpec, RgbColor>,
    bg_cache: HashMap<HighlightSpec, RgbColor>,
}

/// highlight_color_resolver_t resolves highlight specs (like "a command") to actual RGB colors.
/// It maintains a cache with no invalidation mechanism. The lifetime of these should typically be
/// one screen redraw.
impl HighlightColorResolver {
    pub fn new() -> Self {
        Default::default()
    }
    /// Return an RGB color for a given highlight spec.
    pub fn resolve_spec(
        &mut self,
        highlight: &HighlightSpec,
        is_background: bool,
        vars: &dyn Environment,
    ) -> RgbColor {
        let cache = if is_background {
            &mut self.bg_cache
        } else {
            &mut self.fg_cache
        };
        match cache.entry(*highlight) {
            Entry::Occupied(e) => *e.get(),
            Entry::Vacant(e) => {
                let color = Self::resolve_spec_uncached(highlight, is_background, vars);
                e.insert(color);
                color
            }
        }
    }
    pub fn resolve_spec_uncached(
        highlight: &HighlightSpec,
        is_background: bool,
        vars: &dyn Environment,
    ) -> RgbColor {
        let mut result = RgbColor::NORMAL;
        let role = if is_background {
            highlight.background
        } else {
            highlight.foreground
        };

        let var = vars
            .get_unless_empty(get_highlight_var_name(role))
            .or_else(|| vars.get_unless_empty(get_highlight_var_name(get_fallback(role))))
            .or_else(|| vars.get(get_highlight_var_name(HighlightRole::normal)));
        if let Some(var) = var {
            result = parse_color(&var, is_background);
        }

        // Handle modifiers.
        if !is_background && highlight.valid_path {
            if let Some(var2) = vars.get(L!("fish_color_valid_path")) {
                let result2 = parse_color(&var2, is_background);
                if result.is_normal() {
                    result = result2;
                } else if !result2.is_normal() {
                    // Valid path has an actual color, use it and merge the modifiers.
                    let mut rescol = result2;
                    rescol.set_bold(result.is_bold() || result2.is_bold());
                    rescol.set_underline(result.is_underline() || result2.is_underline());
                    rescol.set_italics(result.is_italics() || result2.is_italics());
                    rescol.set_dim(result.is_dim() || result2.is_dim());
                    rescol.set_reverse(result.is_reverse() || result2.is_reverse());
                    result = rescol;
                } else {
                    if result2.is_bold() {
                        result.set_bold(true)
                    };
                    if result2.is_underline() {
                        result.set_underline(true)
                    };
                    if result2.is_italics() {
                        result.set_italics(true)
                    };
                    if result2.is_dim() {
                        result.set_dim(true)
                    };
                    if result2.is_reverse() {
                        result.set_reverse(true)
                    };
                }
            }
        }

        if !is_background && highlight.force_underline {
            result.set_underline(true);
        }

        result
    }
}

fn command_is_valid(
    cmd: &wstr,
    decoration: StatementDecoration,
    working_directory: &wstr,
    vars: &dyn Environment,
) -> bool {
    // Determine which types we check, based on the decoration.
    let mut builtin_ok = true;
    let mut function_ok = true;
    let mut abbreviation_ok = true;
    let mut command_ok = true;
    let mut implicit_cd_ok = true;
    if matches!(
        decoration,
        StatementDecoration::command | StatementDecoration::exec
    ) {
        builtin_ok = false;
        function_ok = false;
        abbreviation_ok = false;
        command_ok = true;
        implicit_cd_ok = false;
    } else if decoration == StatementDecoration::builtin {
        builtin_ok = true;
        function_ok = false;
        abbreviation_ok = false;
        command_ok = false;
        implicit_cd_ok = false;
    }

    // Check them.
    let mut is_valid = false;

    // Builtins
    if !is_valid && builtin_ok {
        is_valid = builtin_exists(cmd)
    };

    // Functions
    if !is_valid && function_ok {
        is_valid = function::exists_no_autoload(cmd)
    };

    // Abbreviations
    if !is_valid && abbreviation_ok {
        is_valid = with_abbrs(|set| set.has_match(cmd, abbrs::Position::Command, L!("")))
    };

    // Regular commands
    if !is_valid && command_ok {
        is_valid = path_get_path(cmd, vars).is_some()
    };

    // Implicit cd
    if !is_valid && implicit_cd_ok {
        is_valid = path_as_implicit_cd(cmd, working_directory, vars).is_some();
    }

    // Return what we got.
    return is_valid;
}

fn has_expand_reserved(s: &wstr) -> bool {
    for wc in s.chars() {
        if (EXPAND_RESERVED_BASE..=EXPAND_RESERVED_END).contains(&wc) {
            return true;
        }
    }
    false
}

// Parse a command line. Return by reference the first command, and the first argument to that
// command (as a string), if any. This is used to validate autosuggestions.
fn autosuggest_parse_command(
    buff: &wstr,
    ctx: &OperationContext<'_>,
) -> Option<(WString, WString)> {
    let ast = Ast::parse(
        buff,
        ParseTreeFlags::CONTINUE_AFTER_ERROR | ParseTreeFlags::ACCEPT_INCOMPLETE_TOKENS,
        None,
    );

    // Find the first statement.
    let jc = ast.top().as_job_list().unwrap().get(0)?;
    let first_statement = jc.job.statement.contents.as_decorated_statement()?;

    if let Some(expanded_command) = statement_get_expanded_command(buff, first_statement, ctx) {
        let mut arg = WString::new();
        // Check if the first argument or redirection is, in fact, an argument.
        if let Some(arg_or_redir) = first_statement.args_or_redirs.get(0) {
            if arg_or_redir.is_argument() {
                arg = arg_or_redir.argument().source(buff).to_owned();
            }
        }

        return Some((expanded_command, arg));
    }

    None
}

/// Given an item `item` from the history which is a proposed autosuggestion, return whether the
/// autosuggestion is valid. It may not be valid if e.g. it is attempting to cd into a directory
/// which does not exist.
pub fn autosuggest_validate_from_history(
    item: &HistoryItem,
    working_directory: &wstr,
    ctx: &OperationContext<'_>,
) -> bool {
    assert_is_background_thread();

    // Parse the string.
    let Some((parsed_command, mut cd_dir)) = autosuggest_parse_command(item.str(), ctx) else {
        // This is for autosuggestions which are not decorated commands, e.g. function declarations.
        return true;
    };

    // We handle cd specially.
    if parsed_command == "cd" && !cd_dir.is_empty() {
        if expand_one(&mut cd_dir, ExpandFlags::FAIL_ON_CMDSUBST, ctx, None) {
            if string_prefixes_string(&cd_dir, L!("--help"))
                || string_prefixes_string(&cd_dir, L!("-h"))
            {
                // cd --help is always valid.
                return true;
            } else {
                // Check the directory target, respecting CDPATH.
                // Permit the autosuggestion if the path is valid and not our directory.
                let path = path_get_cdpath(&cd_dir, working_directory, ctx.vars());
                return path
                    .map(|p| !paths_are_same_file(working_directory, &p))
                    .unwrap_or(false);
            }
        }
    }

    // Not handled specially. Is the command valid?
    let cmd_ok = builtin_exists(&parsed_command)
        || function::exists_no_autoload(&parsed_command)
        || path_get_path(&parsed_command, ctx.vars()).is_some();
    if !cmd_ok {
        return false;
    }

    // Did the historical command have arguments that look like paths, which aren't paths now?
    let paths = item.get_required_paths();
    if !all_paths_are_valid(paths.iter().cloned(), ctx) {
        return false;
    }

    true
}

// Highlights the variable starting with 'in', setting colors within the 'colors' array. Returns the
// number of characters consumed.
fn color_variable(inp: &wstr, colors: &mut [HighlightSpec]) -> usize {
    assert!(inp.char_at(0) == '$');

    // Handle an initial run of $s.
    let mut idx = 0;
    let mut dollar_count = 0;
    while inp.char_at(idx) == '$' {
        // Our color depends on the next char.
        let next = inp.char_at(idx + 1);
        if next == '$' || valid_var_name_char(next) {
            colors[idx] = HighlightSpec::with_fg(HighlightRole::operat);
        } else if next == '(' {
            colors[idx] = HighlightSpec::with_fg(HighlightRole::operat);
            return idx + 1;
        } else {
            colors[idx] = HighlightSpec::with_fg(HighlightRole::error);
        }
        idx += 1;
        dollar_count += 1;
    }

    // Handle a sequence of variable characters.
    // It may contain an escaped newline - see #8444.
    loop {
        if valid_var_name_char(inp.char_at(idx)) {
            colors[idx] = HighlightSpec::with_fg(HighlightRole::operat);
            idx += 1;
        } else if inp.char_at(idx) == '\\' && inp.char_at(idx + 1) == '\n' {
            colors[idx] = HighlightSpec::with_fg(HighlightRole::operat);
            idx += 1;
            colors[idx] = HighlightSpec::with_fg(HighlightRole::operat);
            idx += 1;
        } else {
            break;
        }
    }

    // Handle a slice, up to dollar_count of them. Note that we currently don't do any validation of
    // the slice's contents, e.g. $foo[blah] will not show an error even though it's invalid.
    for _slice_count in 0..dollar_count {
        match parse_util_slice_length(&inp[idx..]) {
            Some(slice_len) if slice_len > 0 => {
                colors[idx] = HighlightSpec::with_fg(HighlightRole::operat);
                colors[idx + slice_len - 1] = HighlightSpec::with_fg(HighlightRole::operat);
                idx += slice_len;
            }
            Some(_slice_len) => {
                // not a slice
                break;
            }
            None => {
                // Syntax error. Normally the entire token is colored red for us, but inside a
                // double-quoted string that doesn't happen. As such, color the variable + the slice
                // start red. Coloring any more than that looks bad, unless we're willing to try and
                // detect where the double-quoted string ends, and I'd rather not do that.
                colors[..idx + 1].fill(HighlightSpec::with_fg(HighlightRole::error));
                break;
            }
        }
    }
    idx
}

/// This function is a disaster badly in need of refactoring. It colors an argument or command,
/// without regard to command substitutions.
fn color_string_internal(buffstr: &wstr, base_color: HighlightSpec, colors: &mut [HighlightSpec]) {
    // Clarify what we expect.
    assert!(
        [
            HighlightSpec::with_fg(HighlightRole::param),
            HighlightSpec::with_fg(HighlightRole::option),
            HighlightSpec::with_fg(HighlightRole::command)
        ]
        .contains(&base_color),
        "Unexpected base color"
    );
    let buff_len = buffstr.len();
    colors.fill(base_color);

    // Hacky support for %self which must be an unquoted literal argument.
    if buffstr == PROCESS_EXPAND_SELF_STR {
        colors[..PROCESS_EXPAND_SELF_STR.len()].fill(HighlightSpec::with_fg(HighlightRole::operat));
        return;
    }

    #[derive(Eq, PartialEq)]
    enum Mode {
        unquoted,
        single_quoted,
        double_quoted,
    }
    let mut mode = Mode::unquoted;
    let mut unclosed_quote_offset = None;
    let mut bracket_count = 0;
    let mut in_pos = 0;
    while in_pos < buff_len {
        let c = buffstr.as_char_slice()[in_pos];
        match mode {
            Mode::unquoted => {
                if c == '\\' {
                    let mut fill_color = HighlightRole::escape; // may be set to highlight_error
                    let backslash_pos = in_pos;
                    let mut fill_end = backslash_pos;

                    // Move to the escaped character.
                    in_pos += 1;
                    let escaped_char = if in_pos < buff_len {
                        buffstr.as_char_slice()[in_pos]
                    } else {
                        '\0'
                    };

                    if escaped_char == '\0' {
                        fill_end = in_pos;
                        fill_color = HighlightRole::error;
                    } else if matches!(escaped_char, '~' | '%') {
                        if in_pos == 1 {
                            fill_end = in_pos + 1;
                        }
                    } else if escaped_char == ',' {
                        if bracket_count != 0 {
                            fill_end = in_pos + 1;
                        }
                    } else if "abefnrtv*?$(){}[]'\"<>^ \\#;|&".contains(escaped_char) {
                        fill_end = in_pos + 1;
                    } else if escaped_char == 'c' {
                        // Like \ci. So highlight three characters.
                        fill_end = in_pos + 1;
                    } else if "uUxX01234567".contains(escaped_char) {
                        let mut res = 0;
                        let mut chars = 2;
                        let mut base = 16;
                        let mut max_val = ASCII_MAX;

                        match escaped_char {
                            'u' => {
                                chars = 4;
                                max_val = '\u{FFFF}'; // UCS2_MAX
                                in_pos += 1;
                            }
                            'U' => {
                                chars = 8;
                                // Don't exceed the largest Unicode code point - see #1107.
                                max_val = '\u{10FFFF}';
                                in_pos += 1;
                            }
                            'x' | 'X' => {
                                max_val = 0xFF as char;
                                in_pos += 1;
                            }
                            _ => {
                                // a digit like \12
                                base = 8;
                                chars = 3;
                            }
                        }

                        // Consume
                        for _i in 0..chars {
                            if in_pos == buff_len {
                                break;
                            }
                            let Some(d) = buffstr.as_char_slice()[in_pos].to_digit(base) else {
                                break;
                            };
                            res = (res * base) + d;
                            in_pos += 1;
                        }
                        // in_pos is now at the first character that could not be converted (or
                        // buff_len).
                        assert!((backslash_pos..=buff_len).contains(&in_pos));
                        fill_end = in_pos;

                        // It's an error if we exceeded the max value.
                        if res > u32::from(max_val) {
                            fill_color = HighlightRole::error;
                        }

                        // Subtract one from in_pos, so that the increment in the loop will move to
                        // the next character.
                        in_pos -= 1;
                    }
                    assert!(fill_end >= backslash_pos);
                    colors[backslash_pos..fill_end].fill(HighlightSpec::with_fg(fill_color));
                } else {
                    // Not a backslash.
                    match c {
                        '~' => {
                            if in_pos == 0 {
                                colors[in_pos] = HighlightSpec::with_fg(HighlightRole::operat);
                            }
                        }
                        '$' => {
                            assert!(in_pos < buff_len);
                            in_pos += color_variable(&buffstr[in_pos..], &mut colors[in_pos..]);
                            // Subtract one to account for the upcoming loop increment.
                            in_pos -= 1;
                        }
                        '?' => {
                            if !feature_test(FeatureFlag::qmark_noglob) {
                                colors[in_pos] = HighlightSpec::with_fg(HighlightRole::operat);
                            }
                        }
                        '*' | '(' | ')' => {
                            colors[in_pos] = HighlightSpec::with_fg(HighlightRole::operat);
                        }
                        '{' => {
                            colors[in_pos] = HighlightSpec::with_fg(HighlightRole::operat);
                            bracket_count += 1;
                        }
                        '}' => {
                            colors[in_pos] = HighlightSpec::with_fg(HighlightRole::operat);
                            bracket_count -= 1;
                        }
                        ',' => {
                            if bracket_count > 0 {
                                colors[in_pos] = HighlightSpec::with_fg(HighlightRole::operat);
                            }
                        }
                        '\'' => {
                            colors[in_pos] = HighlightSpec::with_fg(HighlightRole::quote);
                            unclosed_quote_offset = Some(in_pos);
                            mode = Mode::single_quoted;
                        }
                        '"' => {
                            colors[in_pos] = HighlightSpec::with_fg(HighlightRole::quote);
                            unclosed_quote_offset = Some(in_pos);
                            mode = Mode::double_quoted;
                        }
                        _ => (), // we ignore all other characters
                    }
                }
            }
            // Mode 1 means single quoted string, i.e 'foo'.
            Mode::single_quoted => {
                colors[in_pos] = HighlightSpec::with_fg(HighlightRole::quote);
                if c == '\\' {
                    // backslash
                    if in_pos + 1 < buff_len {
                        let escaped_char = buffstr.as_char_slice()[in_pos + 1];
                        if matches!(escaped_char, '\\' | '\'') {
                            colors[in_pos] = HighlightSpec::with_fg(HighlightRole::escape); // backslash
                            colors[in_pos + 1] = HighlightSpec::with_fg(HighlightRole::escape); // escaped char
                            in_pos += 1; // skip over backslash
                        }
                    }
                } else if c == '\'' {
                    mode = Mode::unquoted;
                }
            }
            // Mode 2 means double quoted string, i.e. "foo".
            Mode::double_quoted => {
                // Slices are colored in advance, past `in_pos`, and we don't want to overwrite
                // that.
                if colors[in_pos] == base_color {
                    colors[in_pos] = HighlightSpec::with_fg(HighlightRole::quote);
                }
                match c {
                    '"' => {
                        mode = Mode::unquoted;
                    }
                    '\\' => {
                        // Backslash
                        if in_pos + 1 < buff_len {
                            let escaped_char = buffstr.as_char_slice()[in_pos + 1];
                            if matches!(escaped_char, '\\' | '"' | '\n' | '$') {
                                colors[in_pos] = HighlightSpec::with_fg(HighlightRole::escape); // backslash
                                colors[in_pos + 1] = HighlightSpec::with_fg(HighlightRole::escape); // escaped char
                                in_pos += 1; // skip over backslash
                            }
                        }
                    }
                    '$' => {
                        in_pos += color_variable(&buffstr[in_pos..], &mut colors[in_pos..]);
                        // Subtract one to account for the upcoming increment in the loop.
                        in_pos -= 1;
                    }
                    _ => (), // we ignore all other characters
                }
            }
        }
        in_pos += 1;
    }

    // Error on unclosed quotes.
    if mode != Mode::unquoted {
        colors[unclosed_quote_offset.unwrap()] = HighlightSpec::with_fg(HighlightRole::error);
    }
}

/// Indicates whether the source range of the given node forms a valid path in the given
/// working_directory.
fn range_is_potential_path(
    src: &wstr,
    range: SourceRange,
    at_cursor: bool,
    ctx: &OperationContext,
    working_directory: &wstr,
) -> bool {
    // Skip strings exceeding PATH_MAX. See #7837.
    // Note some paths may exceed PATH_MAX, but this is just for highlighting.
    if range.length() > (PATH_MAX as usize) {
        return false;
    }
    // Get the node source, unescape it, and then pass it to is_potential_path along with the
    // working directory (as a one element list).
    let mut result = false;
    if let Some(mut token) = unescape_string(
        &src[range.start()..range.end()],
        crate::common::UnescapeStringStyle::Script(UnescapeFlags::SPECIAL),
    ) {
        // Big hack: is_potential_path expects a tilde, but unescape_string gives us HOME_DIRECTORY.
        // Put it back.
        if token.char_at(0) == HOME_DIRECTORY {
            token.as_char_slice_mut()[0] = '~';
        }

        result = is_potential_path(
            &token,
            at_cursor,
            &[working_directory.to_owned()],
            ctx,
            PathFlags::PATH_EXPAND_TILDE,
        );
    }
    result
}

// Tests whether the specified string cpath is the prefix of anything we could cd to. directories is
// a list of possible parent directories (typically either the working directory, or the cdpath).
// This does I/O!
//
// This is used only internally to this file, and is exposed only for testing.
bitflags! {
    #[derive(Clone, Copy, Default)]
    pub struct PathFlags: u8 {
        // The path must be to a directory.
        const PATH_REQUIRE_DIR = 1 << 0;
        // Expand any leading tilde in the path.
        const PATH_EXPAND_TILDE = 1 << 1;
        // Normalize directories before resolving, as "cd".
        const PATH_FOR_CD = 1 << 2;
    }
}

/// Tests whether the specified string cpath is the prefix of anything we could cd to. directories
/// is a list of possible parent directories (typically either the working directory, or the
/// cdpath). This does I/O!
///
/// Hack: if out_suggested_cdpath is not NULL, it returns the autosuggestion for cd. This descends
/// the deepest unique directory hierarchy.
///
/// We expect the path to already be unescaped.
pub fn is_potential_path(
    potential_path_fragment: &wstr,
    at_cursor: bool,
    directories: &[WString],
    ctx: &OperationContext<'_>,
    flags: PathFlags,
) -> bool {
    assert_is_background_thread();

    if ctx.check_cancel() {
        return false;
    }

    let require_dir = flags.contains(PathFlags::PATH_REQUIRE_DIR);
    let mut clean_potential_path_fragment = WString::new();
    let mut has_magic = false;

    let mut path_with_magic = potential_path_fragment.to_owned();
    if flags.contains(PathFlags::PATH_EXPAND_TILDE) {
        expand_tilde(&mut path_with_magic, ctx.vars());
    }

    for c in path_with_magic.chars() {
        match c {
            PROCESS_EXPAND_SELF
            | VARIABLE_EXPAND
            | VARIABLE_EXPAND_SINGLE
            | BRACE_BEGIN
            | BRACE_END
            | BRACE_SEP
            | ANY_CHAR
            | ANY_STRING
            | ANY_STRING_RECURSIVE => {
                has_magic = true;
            }
            INTERNAL_SEPARATOR => (),
            _ => clean_potential_path_fragment.push(c),
        }
    }

    if has_magic || clean_potential_path_fragment.is_empty() {
        return false;
    }

    // Don't test the same path multiple times, which can happen if the path is absolute and the
    // CDPATH contains multiple entries.
    let mut checked_paths = HashSet::new();

    // Keep a cache of which paths / filesystems are case sensitive.
    let mut case_sensitivity_cache = CaseSensitivityCache::new();

    for wd in directories {
        if ctx.check_cancel() {
            return false;
        }
        let mut abs_path = path_apply_working_directory(&clean_potential_path_fragment, wd);
        let must_be_full_dir = abs_path.chars().next_back() == Some('/');
        if flags.contains(PathFlags::PATH_FOR_CD) {
            abs_path = normalize_path(&abs_path, /*allow_leading_double_slashes=*/ true);
        }

        // Skip this if it's empty or we've already checked it.
        if abs_path.is_empty() || checked_paths.contains(&abs_path) {
            continue;
        }
        checked_paths.insert(abs_path.clone());

        // If the user is still typing the argument, we want to highlight it if it's the prefix
        // of a valid path. This means we need to potentially walk all files in some directory.
        // There are two easy cases where we can skip this:
        // 1. If the argument ends with a slash, it must be a valid directory, no prefix.
        // 2. If the cursor is not at the argument, it means the user is definitely not typing it,
        //    so we can skip the prefix-match.
        if must_be_full_dir || !at_cursor {
            if let Ok(md) = wstat(&abs_path) {
                if !at_cursor || md.file_type().is_dir() {
                    return true;
                }
            }
        } else {
            // We do not end with a slash; it does not have to be a directory.
            let dir_name = wdirname(&abs_path);
            let filename_fragment = wbasename(&abs_path);
            if dir_name == "/" && filename_fragment == "/" {
                // cd ///.... No autosuggestion.
                return true;
            }

            if let Ok(mut dir) = DirIter::new(dir_name) {
                // Check if we're case insensitive.
                let do_case_insensitive =
                    fs_is_case_insensitive(dir_name, dir.fd(), &mut case_sensitivity_cache);

                // We opened the dir_name; look for a string where the base name prefixes it.
                while let Some(entry) = dir.next() {
                    let Ok(entry) = entry else { continue };
                    if ctx.check_cancel() {
                        return false;
                    }

                    // Maybe skip directories.
                    if require_dir && !entry.is_dir() {
                        continue;
                    }

                    if string_prefixes_string(filename_fragment, &entry.name)
                        || (do_case_insensitive
                            && string_prefixes_string_case_insensitive(
                                filename_fragment,
                                &entry.name,
                            ))
                    {
                        return true;
                    }
                }
            }
        }
    }
    false
}

// Given a string, return whether it prefixes a path that we could cd into. Return that path in
// out_path. Expects path to be unescaped.
fn is_potential_cd_path(
    path: &wstr,
    at_cursor: bool,
    working_directory: &wstr,
    ctx: &OperationContext<'_>,
    flags: PathFlags,
) -> bool {
    let mut directories = vec![];

    if string_prefixes_string(L!("./"), path) {
        // Ignore the CDPATH in this case; just use the working directory.
        directories.push(working_directory.to_owned());
    } else {
        // Get the CDPATH.
        let cdpath = ctx.vars().get_unless_empty(L!("CDPATH"));
        let mut pathsv = match cdpath {
            None => vec![L!(".").to_owned()],
            Some(cdpath) => cdpath.as_list().to_vec(),
        };
        // The current $PWD is always valid.
        pathsv.push(L!(".").to_owned());

        for mut next_path in pathsv {
            if next_path.is_empty() {
                next_path = L!(".").to_owned();
            }
            // Ensure that we use the working directory for relative cdpaths like ".".
            directories.push(path_apply_working_directory(&next_path, working_directory));
        }
    }

    // Call is_potential_path with all of these directories.
    is_potential_path(
        path,
        at_cursor,
        &directories,
        ctx,
        flags | PathFlags::PATH_REQUIRE_DIR | PathFlags::PATH_FOR_CD,
    )
}

pub type ColorArray = Vec<HighlightSpec>;

/// Syntax highlighter helper.
struct Highlighter<'s> {
    // The string we're highlighting. Note this is a reference member variable (to avoid copying)!
    buff: &'s wstr,
    // The position of the cursor within the string.
    cursor: Option<usize>,
    // The operation context.
    ctx: &'s OperationContext<'s>,
    // Whether it's OK to do I/O.
    io_ok: bool,
    // Working directory.
    working_directory: WString,
    // The resulting colors.
    color_array: ColorArray,
    // A stack of variables that the current commandline probably defines.  We mark redirections
    // as valid if they use one of these variables, to avoid marking valid targets as error.
    pending_variables: Vec<&'s wstr>,
    done: bool,
}

impl<'s> Highlighter<'s> {
    pub fn new(
        buff: &'s wstr,
        cursor: Option<usize>,
        ctx: &'s OperationContext<'s>,
        working_directory: WString,
        can_do_io: bool,
    ) -> Self {
        Self {
            buff,
            cursor,
            ctx,
            io_ok: can_do_io,
            working_directory,
            color_array: vec![],
            pending_variables: vec![],
            done: false,
        }
    }

    pub fn highlight(&mut self) -> ColorArray {
        assert!(!self.done);
        self.done = true;

        // If we are doing I/O, we must be in a background thread.
        if self.io_ok {
            assert_is_background_thread();
        }

        self.color_array
            .resize(self.buff.len(), HighlightSpec::default());

        // Flags we use for AST parsing.
        let ast_flags = ParseTreeFlags::CONTINUE_AFTER_ERROR
            | ParseTreeFlags::INCLUDE_COMMENTS
            | ParseTreeFlags::ACCEPT_INCOMPLETE_TOKENS
            | ParseTreeFlags::LEAVE_UNTERMINATED
            | ParseTreeFlags::SHOW_EXTRA_SEMIS;
        let ast = Ast::parse(self.buff, ast_flags, None);

        self.visit_children(ast.top());
        if self.ctx.check_cancel() {
            return std::mem::take(&mut self.color_array);
        }

        // Color every comment.
        let extras = &ast.extras;
        for range in &extras.comments {
            self.color_range(*range, HighlightSpec::with_fg(HighlightRole::comment));
        }

        // Color every extra semi.
        for range in &extras.semis {
            self.color_range(
                *range,
                HighlightSpec::with_fg(HighlightRole::statement_terminator),
            );
        }

        // Color every error range.
        for range in &extras.errors {
            self.color_range(*range, HighlightSpec::with_fg(HighlightRole::error));
        }

        std::mem::take(&mut self.color_array)
    }

    /// Return a substring of our buffer.
    pub fn get_source(&self, r: SourceRange) -> &'s wstr {
        assert!(r.end() >= r.start(), "Overflow");
        assert!(r.end() <= self.buff.len(), "Out of range");
        &self.buff[r.start()..r.end()]
    }

    fn io_still_ok(&self) -> bool {
        self.io_ok && !self.ctx.check_cancel()
    }

    // Color a command.
    fn color_command(&mut self, node: &ast::String_) {
        let source_range = node.source_range();
        let cmd_str = self.get_source(source_range);

        let arg_start = source_range.start();
        color_string_internal(
            cmd_str,
            HighlightSpec::with_fg(HighlightRole::command),
            &mut self.color_array[arg_start..],
        );
    }
    // Color a node as if it were an argument.
    fn color_as_argument(&mut self, node: &dyn ast::Node, options_allowed: bool /* = true */) {
        // node does not necessarily have type symbol_argument here.
        let source_range = node.source_range();
        let arg_str = self.get_source(source_range);

        let arg_start = source_range.start();

        // Color this argument without concern for command substitutions.
        if options_allowed && arg_str.char_at(0) == '-' {
            color_string_internal(
                arg_str,
                HighlightSpec::with_fg(HighlightRole::option),
                &mut self.color_array[arg_start..],
            );
        } else {
            color_string_internal(
                arg_str,
                HighlightSpec::with_fg(HighlightRole::param),
                &mut self.color_array[arg_start..],
            );
        }

        // Now do command substitutions.
        let mut cmdsub_cursor = 0;
        let mut is_quoted = false;
        while let MaybeParentheses::CommandSubstitution(parens) = parse_util_locate_cmdsubst_range(
            arg_str,
            &mut cmdsub_cursor,
            /*accept_incomplete=*/ true,
            Some(&mut is_quoted),
            None,
        ) {
            // Highlight the parens. The open parens must exist; the closed paren may not if it was
            // incomplete.
            assert!(parens.start() < arg_str.len());
            self.color_array[arg_start..][parens.opening()]
                .fill(HighlightSpec::with_fg(HighlightRole::operat));
            self.color_array[arg_start..][parens.closing()]
                .fill(HighlightSpec::with_fg(HighlightRole::operat));

            // Highlight it recursively.
            let arg_cursor = self
                .cursor
                .map(|c| c.wrapping_sub(arg_start + parens.start()));
            let cmdsub_contents = &arg_str[parens.command()];
            let mut cmdsub_highlighter = Highlighter::new(
                cmdsub_contents,
                arg_cursor,
                self.ctx,
                self.working_directory.clone(),
                self.io_still_ok(),
            );
            let subcolors = cmdsub_highlighter.highlight();

            // Copy out the subcolors back into our array.
            assert!(subcolors.len() == cmdsub_contents.len());
            self.color_array[arg_start..][parens.command()].copy_from_slice(&subcolors);
        }
    }
    // Colors the source range of a node with a given color.
    fn color_node(&mut self, node: &dyn ast::Node, color: HighlightSpec) {
        self.color_range(node.source_range(), color)
    }
    // Colors a range with a given color.
    fn color_range(&mut self, range: SourceRange, color: HighlightSpec) {
        assert!(range.end() <= self.color_array.len(), "Range out of bounds");
        self.color_array[range.start()..range.end()].fill(color);
    }

    // Visit the children of a node.
    fn visit_children(&mut self, node: &dyn Node) {
        node.accept(self, false);
    }
    // AST visitor implementations.
    fn visit_keyword(&mut self, node: &dyn Keyword) {
        let mut role = HighlightRole::normal;
        match node.keyword() {
            ParseKeyword::kw_begin
            | ParseKeyword::kw_builtin
            | ParseKeyword::kw_case
            | ParseKeyword::kw_command
            | ParseKeyword::kw_else
            | ParseKeyword::kw_end
            | ParseKeyword::kw_exec
            | ParseKeyword::kw_for
            | ParseKeyword::kw_function
            | ParseKeyword::kw_if
            | ParseKeyword::kw_in
            | ParseKeyword::kw_switch
            | ParseKeyword::kw_while => role = HighlightRole::keyword,
            ParseKeyword::kw_and
            | ParseKeyword::kw_or
            | ParseKeyword::kw_not
            | ParseKeyword::kw_exclam
            | ParseKeyword::kw_time => role = HighlightRole::operat,
            ParseKeyword::none => (),
        };
        self.color_node(node.leaf_as_node(), HighlightSpec::with_fg(role));
    }
    fn visit_token(&mut self, tok: &dyn Token) {
        let mut role = HighlightRole::normal;
        match tok.token_type() {
            ParseTokenType::end | ParseTokenType::pipe | ParseTokenType::background => {
                role = HighlightRole::statement_terminator
            }
            ParseTokenType::andand | ParseTokenType::oror => role = HighlightRole::operat,
            ParseTokenType::string => {
                // Assume all strings are params. This handles e.g. the variables a for header or
                // function header. Other strings (like arguments to commands) need more complex
                // handling, which occurs in their respective overrides of visit().
                role = HighlightRole::param;
            }
            _ => (),
        }
        self.color_node(tok.leaf_as_node(), HighlightSpec::with_fg(role));
    }
    // Visit an argument, perhaps knowing that our command is cd.
    fn visit_argument(&mut self, arg: &Argument, cmd_is_cd: bool, options_allowed: bool) {
        self.color_as_argument(arg.as_node(), options_allowed);
        if !self.io_still_ok() {
            return;
        }
        // Underline every valid path.
        let mut is_valid_path = false;
        let at_cursor = self
            .cursor
            .map_or(false, |c| arg.source_range().contains_inclusive(c));
        if cmd_is_cd {
            // Mark this as an error if it's not 'help' and not a valid cd path.
            let mut param = arg.source(self.buff).to_owned();
            if expand_one(&mut param, ExpandFlags::FAIL_ON_CMDSUBST, self.ctx, None) {
                let is_help = string_prefixes_string(&param, L!("--help"))
                    || string_prefixes_string(&param, L!("-h"));
                if !is_help {
                    is_valid_path = is_potential_cd_path(
                        &param,
                        at_cursor,
                        &self.working_directory,
                        self.ctx,
                        PathFlags::PATH_EXPAND_TILDE,
                    );
                    if !is_valid_path {
                        self.color_node(
                            arg.as_node(),
                            HighlightSpec::with_fg(HighlightRole::error),
                        );
                    }
                }
            }
        } else if range_is_potential_path(
            self.buff,
            arg.range().unwrap(),
            at_cursor,
            self.ctx,
            &self.working_directory,
        ) {
            is_valid_path = true;
        }
        if is_valid_path {
            for i in arg.range().unwrap().start()..arg.range().unwrap().end() {
                self.color_array[i].valid_path = true;
            }
        }
    }
    fn visit_redirection(&mut self, redir: &Redirection) {
        // like 2>
        let oper = PipeOrRedir::try_from(redir.oper.source(self.buff))
            .expect("Should have successfully parsed a pipe_or_redir_t since it was in our ast");
        let mut target = redir.target.source(self.buff).to_owned(); // like &1 or file path

        // Color the > part.
        // It may have parsed successfully yet still be invalid (e.g. 9999999999999>&1)
        // If so, color the whole thing invalid and stop.
        if !oper.is_valid() {
            self.color_node(
                redir.as_node(),
                HighlightSpec::with_fg(HighlightRole::error),
            );
            return;
        }

        // Color the operator part like 2>.
        self.color_node(
            &redir.oper,
            HighlightSpec::with_fg(HighlightRole::redirection),
        );

        // Color the target part.
        // Check if the argument contains a command substitution. If so, highlight it as a param
        // even though it's a command redirection, and don't try to do any other validation.
        if has_cmdsub(&target) {
            self.color_as_argument(redir.target.leaf_as_node(), true);
        } else {
            // No command substitution, so we can highlight the target file or fd. For example,
            // disallow redirections into a non-existent directory.
            let target_is_valid;
            if !self.io_still_ok() {
                // I/O is disallowed, so we don't have much hope of catching anything but gross
                // errors. Assume it's valid.
                target_is_valid = true;
            } else if contains_pending_variable(&self.pending_variables, &target) {
                target_is_valid = true;
            } else if !expand_one(&mut target, ExpandFlags::FAIL_ON_CMDSUBST, self.ctx, None) {
                // Could not be expanded.
                target_is_valid = false;
            } else {
                // Ok, we successfully expanded our target. Now verify that it works with this
                // redirection. We will probably need it as a path (but not in the case of fd
                // redirections). Note that the target is now unescaped.
                let target_path = path_apply_working_directory(&target, &self.working_directory);
                match oper.mode {
                    RedirectionMode::fd => {
                        if target == "-" {
                            target_is_valid = true;
                        } else {
                            target_is_valid = match fish_wcstoi(&target) {
                                Ok(fd) => fd >= 0,
                                Err(_) => false,
                            };
                        }
                    }
                    RedirectionMode::input | RedirectionMode::try_input => {
                        // Input redirections must have a readable non-directory.
                        target_is_valid = waccess(&target_path, R_OK) == 0
                            && match wstat(&target_path) {
                                Ok(md) => !md.file_type().is_dir(),
                                Err(_) => false,
                            };
                    }
                    RedirectionMode::overwrite
                    | RedirectionMode::append
                    | RedirectionMode::noclob => {
                        // Test whether the file exists, and whether it's writable (possibly after
                        // creating it). access() returns failure if the file does not exist.
                        let file_exists;
                        let file_is_writable;

                        if string_suffixes_string(L!("/"), &target) {
                            // Redirections to things that are directories is definitely not
                            // allowed.
                            file_exists = false;
                            file_is_writable = false;
                        } else {
                            match wstat(&target_path) {
                                Ok(md) => {
                                    // No err. We can write to it if it's not a directory and we have
                                    // permission.
                                    file_exists = true;
                                    file_is_writable = !md.file_type().is_dir()
                                        && waccess(&target_path, W_OK) == 0;
                                }
                                Err(err) => {
                                    if err.raw_os_error() == Some(ENOENT) {
                                        // File does not exist. Check if its parent directory is writable.
                                        let mut parent = wdirname(&target_path).to_owned();

                                        // Ensure that the parent ends with the path separator. This will ensure
                                        // that we get an error if the parent directory is not really a
                                        // directory.
                                        if !string_suffixes_string(L!("/"), &parent) {
                                            parent.push('/');
                                        }

                                        // Now the file is considered writable if the parent directory is
                                        // writable.
                                        file_exists = false;
                                        file_is_writable = waccess(&parent, W_OK) == 0;
                                    } else {
                                        // Other errors we treat as not writable. This includes things like
                                        // ENOTDIR.
                                        file_exists = false;
                                        file_is_writable = false;
                                    }
                                }
                            }
                        }

                        // NOCLOB means that we must not overwrite files that exist.
                        target_is_valid = file_is_writable
                            && !(file_exists && oper.mode == RedirectionMode::noclob);
                    }
                }
            }
            self.color_node(
                redir.target.leaf_as_node(),
                HighlightSpec::with_fg(if target_is_valid {
                    HighlightRole::redirection
                } else {
                    HighlightRole::error
                }),
            );
        }
    }
    fn visit_variable_assignment(&mut self, varas: &VariableAssignment) {
        self.color_as_argument(varas, true);
        // Highlight the '=' in variable assignments as an operator.
        if let Some(offset) = variable_assignment_equals_pos(varas.source(self.buff)) {
            let equals_loc = varas.source_range().start() + offset;
            self.color_array[equals_loc] = HighlightSpec::with_fg(HighlightRole::operat);
            let var_name = &varas.source(self.buff)[..offset];
            self.pending_variables.push(var_name);
        }
    }
    fn visit_semi_nl(&mut self, node: &dyn Node) {
        self.color_node(
            node,
            HighlightSpec::with_fg(HighlightRole::statement_terminator),
        )
    }
    fn visit_decorated_statement(&mut self, stmt: &DecoratedStatement) {
        // Color any decoration.
        if let Some(decoration) = stmt.opt_decoration.as_ref() {
            self.visit_keyword(decoration);
        }

        // Color the command's source code.
        // If we get no source back, there's nothing to color.
        if stmt.command.try_source_range().is_none() {
            return;
        }
        let cmd = stmt.command.source(self.buff);

        let mut expanded_cmd = WString::new();
        let mut is_valid_cmd = false;
        if !self.io_still_ok() {
            // We cannot check if the command is invalid, so just assume it's valid.
            is_valid_cmd = true;
        } else if variable_assignment_equals_pos(cmd).is_some() {
            is_valid_cmd = true;
        } else {
            // Check to see if the command is valid.
            // Try expanding it. If we cannot, it's an error.
            if let Some(expanded) = statement_get_expanded_command(self.buff, stmt, self.ctx) {
                expanded_cmd = expanded;
                if !has_expand_reserved(&expanded_cmd) {
                    is_valid_cmd = command_is_valid(
                        &expanded_cmd,
                        stmt.decoration(),
                        &self.working_directory,
                        self.ctx.vars(),
                    );
                }
            }
        }

        // Color our statement.
        if is_valid_cmd {
            self.color_command(&stmt.command);
        } else {
            self.color_node(&stmt.command, HighlightSpec::with_fg(HighlightRole::error))
        }

        // Color arguments and redirections.
        // Except if our command is 'cd' we have special logic for how arguments are colored.
        let is_cd = expanded_cmd == "cd";
        let mut is_set = expanded_cmd == "set";
        // If we have seen a "--" argument, color all options from then on as normal arguments.
        let mut have_dashdash = false;
        for v in &stmt.args_or_redirs {
            if v.is_argument() {
                if is_set {
                    let arg = v.argument().source(self.buff);
                    if valid_var_name(arg) {
                        self.pending_variables.push(arg);
                        is_set = false;
                    }
                }
                self.visit_argument(v.argument(), is_cd, !have_dashdash);
                if v.argument().source(self.buff) == "--" {
                    have_dashdash = true;
                }
            } else {
                self.visit_redirection(v.redirection());
            }
        }
    }
    fn visit_block_statement(&mut self, block: &BlockStatement) {
        match &*block.header {
            BlockStatementHeaderVariant::None => panic!(),
            BlockStatementHeaderVariant::ForHeader(node) => self.visit(node),
            BlockStatementHeaderVariant::WhileHeader(node) => self.visit(node),
            BlockStatementHeaderVariant::FunctionHeader(node) => self.visit(node),
            BlockStatementHeaderVariant::BeginHeader(node) => self.visit(node),
        }
        self.visit(&block.args_or_redirs);
        let pending_variables_count = self.pending_variables.len();
        if let Some(fh) = block.header.as_for_header() {
            let var_name = fh.var_name.source(self.buff);
            self.pending_variables.push(var_name);
        }
        self.visit(&block.jobs);
        self.visit(&block.end);
        self.pending_variables.truncate(pending_variables_count);
    }
}

/// Return whether a string contains a command substitution.
fn has_cmdsub(src: &wstr) -> bool {
    let mut cursor = 0;
    match parse_util_locate_cmdsubst_range(src, &mut cursor, true, None, None) {
        MaybeParentheses::Error => return false,
        MaybeParentheses::None => return false,
        MaybeParentheses::CommandSubstitution(_) => return true,
    }
}

fn contains_pending_variable(pending_variables: &[&wstr], haystack: &wstr) -> bool {
    for var_name in pending_variables {
        let mut nextpos = 0;
        while let Some(relpos) = &haystack[nextpos..]
            .as_char_slice()
            .windows(var_name.len())
            .position(|w| w == var_name.as_char_slice())
        {
            let pos = nextpos + relpos;
            nextpos = pos + 1;
            if pos == 0 || haystack.as_char_slice()[pos - 1] != '$' {
                continue;
            }
            let end = pos + var_name.len();
            if end < haystack.len() && valid_var_name_char(haystack.as_char_slice()[end]) {
                continue;
            }
            return true;
        }
    }
    false
}

impl<'s, 'a> NodeVisitor<'a> for Highlighter<'s> {
    fn visit(&mut self, node: &'a dyn Node) {
        if let Some(keyword) = node.as_keyword() {
            return self.visit_keyword(keyword);
        }
        if let Some(token) = node.as_token() {
            if token.token_type() == ParseTokenType::end {
                self.visit_semi_nl(node);
                return;
            }
            self.visit_token(token);
            return;
        }
        match node.typ() {
            Type::argument => self.visit_argument(node.as_argument().unwrap(), false, true),
            Type::redirection => self.visit_redirection(node.as_redirection().unwrap()),
            Type::variable_assignment => {
                self.visit_variable_assignment(node.as_variable_assignment().unwrap())
            }
            Type::decorated_statement => {
                self.visit_decorated_statement(node.as_decorated_statement().unwrap())
            }
            Type::block_statement => self.visit_block_statement(node.as_block_statement().unwrap()),
            // Default implementation is to just visit children.
            _ => self.visit_children(node),
        }
    }
}

// Given a plain statement node in a parse tree, get the command and return it, expanded
// appropriately for commands. If we succeed, return true.
fn statement_get_expanded_command(
    src: &wstr,
    stmt: &ast::DecoratedStatement,
    ctx: &OperationContext<'_>,
) -> Option<WString> {
    // Get the command. Try expanding it. If we cannot, it's an error.
    let cmd = stmt.command.try_source(src)?;
    let mut out_cmd = WString::new();
    let err = expand_to_command_and_args(cmd, ctx, &mut out_cmd, None, None, false);
    (err == ExpandResultCode::ok).then_some(out_cmd)
}

fn get_highlight_var_name(role: HighlightRole) -> &'static wstr {
    match role {
        HighlightRole::normal => L!("fish_color_normal"),
        HighlightRole::error => L!("fish_color_error"),
        HighlightRole::command => L!("fish_color_command"),
        HighlightRole::keyword => L!("fish_color_keyword"),
        HighlightRole::statement_terminator => L!("fish_color_end"),
        HighlightRole::param => L!("fish_color_param"),
        HighlightRole::option => L!("fish_color_option"),
        HighlightRole::comment => L!("fish_color_comment"),
        HighlightRole::search_match => L!("fish_color_search_match"),
        HighlightRole::operat => L!("fish_color_operator"),
        HighlightRole::escape => L!("fish_color_escape"),
        HighlightRole::quote => L!("fish_color_quote"),
        HighlightRole::redirection => L!("fish_color_redirection"),
        HighlightRole::autosuggestion => L!("fish_color_autosuggestion"),
        HighlightRole::selection => L!("fish_color_selection"),
        HighlightRole::pager_progress => L!("fish_pager_color_progress"),
        HighlightRole::pager_background => L!("fish_pager_color_background"),
        HighlightRole::pager_prefix => L!("fish_pager_color_prefix"),
        HighlightRole::pager_completion => L!("fish_pager_color_completion"),
        HighlightRole::pager_description => L!("fish_pager_color_description"),
        HighlightRole::pager_secondary_background => L!("fish_pager_color_secondary_background"),
        HighlightRole::pager_secondary_prefix => L!("fish_pager_color_secondary_prefix"),
        HighlightRole::pager_secondary_completion => L!("fish_pager_color_secondary_completion"),
        HighlightRole::pager_secondary_description => L!("fish_pager_color_secondary_description"),
        HighlightRole::pager_selected_background => L!("fish_pager_color_selected_background"),
        HighlightRole::pager_selected_prefix => L!("fish_pager_color_selected_prefix"),
        HighlightRole::pager_selected_completion => L!("fish_pager_color_selected_completion"),
        HighlightRole::pager_selected_description => L!("fish_pager_color_selected_description"),
    }
}

// Table used to fetch fallback highlights in case the specified one
// wasn't set.
fn get_fallback(role: HighlightRole) -> HighlightRole {
    match role {
        HighlightRole::normal
        | HighlightRole::error
        | HighlightRole::command
        | HighlightRole::statement_terminator
        | HighlightRole::param
        | HighlightRole::search_match
        | HighlightRole::comment
        | HighlightRole::operat
        | HighlightRole::escape
        | HighlightRole::quote
        | HighlightRole::redirection
        | HighlightRole::autosuggestion
        | HighlightRole::selection
        | HighlightRole::pager_progress
        | HighlightRole::pager_background
        | HighlightRole::pager_prefix
        | HighlightRole::pager_completion
        | HighlightRole::pager_description => HighlightRole::normal,
        HighlightRole::keyword => HighlightRole::command,
        HighlightRole::option => HighlightRole::param,
        HighlightRole::pager_secondary_background => HighlightRole::pager_background,
        HighlightRole::pager_secondary_prefix | HighlightRole::pager_selected_prefix => {
            HighlightRole::pager_prefix
        }
        HighlightRole::pager_secondary_completion | HighlightRole::pager_selected_completion => {
            HighlightRole::pager_completion
        }
        HighlightRole::pager_secondary_description | HighlightRole::pager_selected_description => {
            HighlightRole::pager_description
        }
        HighlightRole::pager_selected_background => HighlightRole::search_match,
    }
}

/// Determine if the filesystem containing the given fd is case insensitive for lookups regardless
/// of whether it preserves the case when saving a pathname.
///
/// Returns:
///     false: the filesystem is not case insensitive
///     true: the file system is case insensitive
pub type CaseSensitivityCache = HashMap<WString, bool>;
fn fs_is_case_insensitive(
    path: &wstr,
    fd: RawFd,
    case_sensitivity_cache: &mut CaseSensitivityCache,
) -> bool {
    let mut result = false;
    if *_PC_CASE_SENSITIVE != 0 {
        // Try the cache first.
        match case_sensitivity_cache.entry(path.to_owned()) {
            Entry::Occupied(e) => {
                /* Use the cached value */
                result = *e.get();
            }
            Entry::Vacant(e) => {
                // Ask the system. A -1 value means error (so assume case sensitive), a 1 value means case
                // sensitive, and a 0 value means case insensitive.
                let ret = unsafe { libc::fpathconf(fd, *_PC_CASE_SENSITIVE) };
                result = ret == 0;
                e.insert(result);
            }
        }
    }
    result
}

impl Default for HighlightRole {
    fn default() -> Self {
        Self::normal
    }
}

impl Default for HighlightSpec {
    fn default() -> Self {
        Self {
            foreground: Default::default(),
            background: Default::default(),
            valid_path: Default::default(),
            force_underline: Default::default(),
        }
    }
}

/// Describes the role of a span of text.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
#[repr(u8)]
pub enum HighlightRole {
    normal,  // normal text
    error,   // error
    command, // command
    keyword,
    statement_terminator, // process separator
    param,                // command parameter (argument)
    option,               // argument starting with "-", up to a "--"
    comment,              // comment
    search_match,         // search match
    operat,               // operator
    escape,               // escape sequences
    quote,                // quoted string
    redirection,          // redirection
    autosuggestion,       // autosuggestion
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
}

/// Simply value type describing how a character should be highlighted..
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct HighlightSpec {
    pub foreground: HighlightRole,
    pub background: HighlightRole,
    pub valid_path: bool,
    pub force_underline: bool,
}
