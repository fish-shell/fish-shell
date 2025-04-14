//! Functions for syntax highlighting.
use crate::abbrs::{self, with_abbrs};
use crate::ast::{
    self, Argument, Ast, BlockStatement, BlockStatementHeaderVariant, BraceStatement,
    DecoratedStatement, Keyword, List, Node, NodeVisitor, Redirection, Token, Type,
    VariableAssignment,
};
use crate::builtins::shared::builtin_exists;
use crate::color::Color;
use crate::common::{
    valid_var_name, valid_var_name_char, ASCII_MAX, EXPAND_RESERVED_BASE, EXPAND_RESERVED_END,
};
use crate::complete::complete_wrap_map;
use crate::env::{EnvVar, Environment};
use crate::expand::{
    expand_one, expand_to_command_and_args, ExpandFlags, ExpandResultCode, PROCESS_EXPAND_SELF_STR,
};
use crate::function;
use crate::future_feature_flags::{feature_test, FeatureFlag};
use crate::highlight::file_tester::FileTester;
use crate::history::{all_paths_are_valid, HistoryItem};
use crate::operation_context::OperationContext;
use crate::parse_constants::{
    ParseKeyword, ParseTokenType, ParseTreeFlags, SourceRange, StatementDecoration,
};
use crate::parse_util::{
    parse_util_locate_cmdsubst_range, parse_util_slice_length, MaybeParentheses,
};
use crate::path::{path_as_implicit_cd, path_get_cdpath, path_get_path, paths_are_same_file};
use crate::terminal::Outputter;
use crate::text_face::{parse_text_face, TextFace, UnderlineStyle};
use crate::threads::assert_is_background_thread;
use crate::tokenizer::{variable_assignment_equals_pos, PipeOrRedir};
use crate::wchar::{wstr, WString, L};
use crate::wchar_ext::WExt;
use crate::wcstringutil::string_prefixes_string;
use std::collections::hash_map::Entry;
use std::collections::HashMap;

use super::file_tester::IsFile;

impl HighlightSpec {
    pub fn new() -> Self {
        Self::default()
    }
    pub fn with_fg_bg(fg: HighlightRole, bg: HighlightRole) -> Self {
        Self {
            foreground: fg,
            background: bg,
            ..Default::default()
        }
    }
    pub fn with_fg(fg: HighlightRole) -> Self {
        Self::with_fg_bg(fg, HighlightRole::normal)
    }
    pub fn with_bg(bg: HighlightRole) -> Self {
        Self::with_fg_bg(HighlightRole::normal, bg)
    }
    pub fn with_both(role: HighlightRole) -> Self {
        Self::with_fg_bg(role, role)
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
            let mut face = rv.resolve_spec(&color, vars);
            face.bg = Color::Normal; // Historical behavior.
            outp.set_text_face(face);
            last_color = color;
        }
        outp.writech(c);
    }
    outp.set_text_face(TextFace::default());
    outp.contents().to_owned()
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
    cache: HashMap<HighlightSpec, TextFace>,
}

/// highlight_color_resolver_t resolves highlight specs (like "a command") to actual RGB colors.
/// It maintains a cache with no invalidation mechanism. The lifetime of these should typically be
/// one screen redraw.
impl HighlightColorResolver {
    pub fn new() -> Self {
        Default::default()
    }
    /// Return an RGB color for a given highlight spec.
    pub(crate) fn resolve_spec(
        &mut self,
        highlight: &HighlightSpec,
        vars: &dyn Environment,
    ) -> TextFace {
        match self.cache.entry(*highlight) {
            Entry::Occupied(e) => *e.get(),
            Entry::Vacant(e) => {
                let face = Self::resolve_spec_uncached(highlight, vars);
                e.insert(face);
                face
            }
        }
    }
    pub(crate) fn resolve_spec_uncached(
        highlight: &HighlightSpec,
        vars: &dyn Environment,
    ) -> TextFace {
        let resolve_role = |role| {
            vars.get_unless_empty(get_highlight_var_name(role))
                .or_else(|| vars.get_unless_empty(get_highlight_var_name(get_fallback(role))))
                .or_else(|| vars.get(get_highlight_var_name(HighlightRole::normal)))
                .as_ref()
                .map(parse_text_face_for_highlight)
                .unwrap_or_else(TextFace::default)
        };
        let mut face = resolve_role(highlight.foreground);

        // Optimization: no need to parse again if both colors are the same.
        if highlight.background != highlight.foreground {
            let bg_face = resolve_role(highlight.background);
            face.bg = bg_face.bg;
            // In case the background role is different from the foreground one, we ignore its style
            // except for reverse mode.
            face.style.reverse |= bg_face.style.is_reverse();
        }

        // Handle modifiers.
        if highlight.valid_path {
            if let Some(valid_path_var) = vars.get(L!("fish_color_valid_path")) {
                // Historical behavior is to not apply background.
                let valid_path_face = parse_text_face_for_highlight(&valid_path_var);
                // Apply the foreground, except if it's normal. The intention here is likely
                // to only override foreground if the valid path color has an explicit foreground.
                if !valid_path_face.fg.is_normal() {
                    face.fg = valid_path_face.fg;
                }
                face.style = face.style.union_prefer_right(valid_path_face.style);
            }
        }

        if highlight.force_underline {
            face.style.inject_underline(UnderlineStyle::Single);
        }

        face
    }
}

/// Return the internal color code representing the specified color.
pub(crate) fn parse_text_face_for_highlight(var: &EnvVar) -> TextFace {
    let face = parse_text_face(var.as_list());
    let default = TextFace::default();
    let fg = face.fg.unwrap_or(default.fg);
    let bg = face.bg.unwrap_or(default.bg);
    let underline_color = face.underline_color.unwrap_or(default.underline_color);
    let style = face.style;
    TextFace {
        fg,
        bg,
        underline_color,
        style,
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

pub fn is_veritable_cd(expanded_command: &wstr) -> bool {
    expanded_command == L!("cd") && complete_wrap_map().get(L!("cd")).is_none()
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
    if is_veritable_cd(&parsed_command) && !cd_dir.is_empty() {
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
    // Our component for testing strings for being potential file paths.
    file_tester: FileTester<'s>,
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
        let file_tester = FileTester::new(working_directory.clone(), ctx);
        Self {
            buff,
            cursor,
            ctx,
            io_ok: can_do_io,
            working_directory,
            file_tester,
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

        color_string_internal(
            cmd_str,
            HighlightSpec::with_fg(HighlightRole::command),
            &mut self.color_array[source_range.as_usize()],
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
                &mut self.color_array[source_range.as_usize()],
            );
        } else {
            color_string_internal(
                arg_str,
                HighlightSpec::with_fg(HighlightRole::param),
                &mut self.color_array[source_range.as_usize()],
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
            ParseKeyword::Begin
            | ParseKeyword::Builtin
            | ParseKeyword::Case
            | ParseKeyword::Command
            | ParseKeyword::Else
            | ParseKeyword::End
            | ParseKeyword::Exec
            | ParseKeyword::For
            | ParseKeyword::Function
            | ParseKeyword::If
            | ParseKeyword::In
            | ParseKeyword::Switch
            | ParseKeyword::While => role = HighlightRole::keyword,
            ParseKeyword::And
            | ParseKeyword::Or
            | ParseKeyword::Not
            | ParseKeyword::Exclam
            | ParseKeyword::Time => role = HighlightRole::operat,
            ParseKeyword::None => (),
        };
        self.color_node(node.as_node(), HighlightSpec::with_fg(role));
    }
    fn visit_token(&mut self, tok: &dyn Token) {
        let mut role = HighlightRole::normal;
        match tok.token_type() {
            ParseTokenType::end | ParseTokenType::pipe | ParseTokenType::background => {
                role = HighlightRole::statement_terminator
            }
            ParseTokenType::left_brace | ParseTokenType::right_brace => {
                role = HighlightRole::keyword;
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
        self.color_node(tok.as_node(), HighlightSpec::with_fg(role));
    }
    // Visit an argument, perhaps knowing that our command is cd.
    fn visit_argument(&mut self, arg: &Argument, cmd_is_cd: bool, options_allowed: bool) {
        self.color_as_argument(arg, options_allowed);
        if !self.io_still_ok() {
            return;
        }

        // Underline every valid path.
        let source_range = arg.source_range();
        let is_prefix = self
            .cursor
            .is_some_and(|c| source_range.contains_inclusive(c));
        let token = arg.source(self.buff).to_owned();
        let test_result = if cmd_is_cd {
            self.file_tester.test_cd_path(&token, is_prefix)
        } else {
            let is_path = self.file_tester.test_path(&token, is_prefix);
            Ok(IsFile(is_path))
        };
        match test_result {
            Ok(IsFile(false)) => (),
            Ok(IsFile(true)) => {
                for i in source_range.as_usize() {
                    self.color_array[i].valid_path = true;
                }
            }
            Err(..) => self.color_node(arg, HighlightSpec::with_fg(HighlightRole::error)),
        }
    }

    fn visit_redirection(&mut self, redir: &Redirection) {
        // like 2>
        let oper = PipeOrRedir::try_from(redir.oper.source(self.buff))
            .expect("Should have successfully parsed a pipe_or_redir_t since it was in our ast");
        let target = redir.target.source(self.buff).to_owned(); // like &1 or file path

        // Color the > part.
        // It may have parsed successfully yet still be invalid (e.g. 9999999999999>&1)
        // If so, color the whole thing invalid and stop.
        if !oper.is_valid() {
            self.color_node(redir, HighlightSpec::with_fg(HighlightRole::error));
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
            self.color_as_argument(&redir.target, true);
            return;
        }
        // No command substitution, so we can highlight the target file or fd. For example,
        // disallow redirections into a non-existent directory.
        let target_is_valid = if !self.io_still_ok() {
            // I/O is disallowed, so we don't have much hope of catching anything but gross
            // errors. Assume it's valid.
            true
        } else if contains_pending_variable(&self.pending_variables, &target) {
            true
        } else {
            // Validate the redirection target..
            self.file_tester.test_redirection_target(&target, oper.mode)
        };
        self.color_node(
            &redir.target,
            HighlightSpec::with_fg(if target_is_valid {
                HighlightRole::redirection
            } else {
                HighlightRole::error
            }),
        );
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
        let is_cd = is_veritable_cd(&expanded_cmd);
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
        match &block.header {
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
    fn visit_brace_statement(&mut self, brace_statement: &BraceStatement) {
        self.visit(&brace_statement.left_brace);
        self.visit(&brace_statement.args_or_redirs);
        self.visit(&brace_statement.jobs);
        self.visit(&brace_statement.right_brace);
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
            Type::brace_statement => self.visit_brace_statement(node.as_brace_statement().unwrap()),
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
    // NOTE: pager.rs relies on these being in this order.
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

/// Simple value type describing how a character should be highlighted.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct HighlightSpec {
    pub foreground: HighlightRole,
    pub background: HighlightRole,
    pub valid_path: bool,
    pub force_underline: bool,
}
