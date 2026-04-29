//! Functions for syntax highlighting.
use crate::{
    abbrs::{self, with_abbrs},
    ast::{
        self, Argument, BlockStatement, BlockStatementHeader, BraceStatement, DecoratedStatement,
        Keyword, Kind, Node, NodeVisitor, Redirection, Token, VariableAssignment,
    },
    builtins::shared::builtin_exists,
    common::{valid_var_name, valid_var_name_char},
    complete::complete_wrap_map,
    env::{EnvVar, Environment},
    expand::{ExpandFlags, ExpandResultCode, expand_one, expand_to_command_and_args},
    function,
    highlight::file_tester::FileTester,
    history::all_paths_are_valid,
    operation_context::OperationContext,
    parse_constants::{
        ParseKeyword, ParseTokenType, ParseTreeFlags, SourceRange, StatementDecoration,
    },
    parse_util::{
        MaybeParentheses, get_process_first_token_offset, locate_cmdsubst_range, slice_length,
    },
    path::{path_as_implicit_cd, path_get_cdpath, path_get_path, paths_are_same_file},
    terminal::Outputter,
    text_face::{ResettableStyle, SpecifiedTextFace, TextFace, UnderlineStyle, parse_text_face},
    threads::assert_is_background_thread,
    tokenizer::{PipeOrRedir, variable_assignment_equals_pos},
};
use fish_color::Color;
use fish_feature_flags::{FeatureFlag, feature_test};
use fish_wcstringutil::string_prefixes_string;
use fish_widestring::{
    ASCII_MAX, EXPAND_RESERVED_BASE, EXPAND_RESERVED_END, L, PROCESS_EXPAND_SELF_STR, WExt as _,
    WString, wstr,
};
use std::collections::{HashMap, hash_map::Entry};
use strum_macros::Display;

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
        Self::with_fg_bg(fg, HighlightRole::Normal)
    }
    pub fn with_bg(bg: HighlightRole) -> Self {
        Self::with_fg_bg(HighlightRole::Normal, bg)
    }
    pub fn with_both(role: HighlightRole) -> Self {
        Self::with_fg_bg(role, role)
    }
}

/// Given a string and list of colors of the same size, return the string with ANSI escape sequences
/// representing the colors.
pub fn colorize(text: &wstr, colors: &[HighlightSpec], vars: &dyn Environment) -> Vec<u8> {
    assert_eq!(colors.len(), text.len());
    let mut rv = HighlightColorResolver::new();
    let mut outp = Outputter::new_buffering();

    let mut last_color = HighlightSpec::with_fg(HighlightRole::Normal);
    for (i, c) in text.chars().enumerate() {
        let color = colors[i];
        if color != last_color {
            let mut face = rv.resolve_spec(&color, vars);
            face.bg = Color::Normal; // Historical behavior.
            outp.set_text_face(face);
            last_color = color;
        }
        if i + 1 == text.char_count() && c == '\n' {
            outp.set_text_face(TextFace::terminal_default());
        }
        outp.writech(c);
    }
    outp.set_text_face(TextFace::terminal_default());
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
pub fn highlight_shell<'src, 'ctx>(
    buff: &'src wstr,
    color: &mut Vec<HighlightSpec>,
    ctx: &'ctx mut OperationContext<'src>,
    io_ok: bool, /* = false */
    cursor: Option<usize>,
) {
    let working_directory = ctx.vars().get_pwd_slash();
    let mut highlighter = Highlighter::new(buff, cursor, ctx, working_directory, io_ok);
    *color = highlighter.highlight();
}

pub fn highlight_and_colorize<'src, 'ctx>(
    text: &'src wstr,
    ctx: &'ctx mut OperationContext<'src>,
) -> Vec<u8> {
    let mut colors = Vec::new();
    highlight_shell(
        text,
        &mut colors,
        ctx,
        /*io_ok=*/ false,
        /*cursor=*/ None,
    );
    let vars = ctx.vars();
    colorize(text, &colors, vars)
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
    fn resolve_spec_uncached(highlight: &HighlightSpec, vars: &dyn Environment) -> TextFace {
        let resolve_role = |role| {
            let mut roles: &[HighlightRole] = &[role, get_fallback(role), HighlightRole::Normal];
            // TODO(MSRV>=?) partition_dedup
            for i in [2, 1] {
                if roles[i - 1] == roles[i] {
                    roles = &roles[..i];
                }
            }
            for &role in roles {
                if let Some(face) = vars
                    .get_unless_empty(get_highlight_var_name(role))
                    .as_ref()
                    .and_then(parse_text_face_for_highlight)
                {
                    return face;
                }
            }
            TextFace::terminal_default()
        };
        let mut face = resolve_role(highlight.foreground);

        // Optimization: no need to parse again if both colors are the same.
        if highlight.background != highlight.foreground {
            let bg_face = resolve_role(highlight.background);
            face.bg = bg_face.bg;
            // In case the background role is different from the foreground one, we ignore its style
            // except for reverse mode.
            if face.style.reverse != ResettableStyle::On(()) {
                face.style.reverse = bg_face.style.reverse;
            }
        }

        // Handle modifiers.
        if highlight.valid_path {
            if let Some(valid_path_var) = vars.get(L!("fish_color_valid_path")) {
                let valid_path_face = parse_text_face(valid_path_var.as_list());
                if let Some(fg) = valid_path_face.fg {
                    face.fg = fg;
                }
                if let Some(bg) = valid_path_face.bg {
                    face.bg = bg;
                }
                if let Some(underline_color) = valid_path_face.underline_color {
                    face.underline_color = underline_color;
                }
                face.style = face.style.union_prefer_right(valid_path_face.style);
            }
        }

        if highlight.force_underline {
            face.style
                .inject_underline(ResettableStyle::On(UnderlineStyle::Single));
        }

        face
    }
}

/// Return the internal color code representing the specified color.
pub(crate) fn parse_text_face_for_highlight(var: &EnvVar) -> Option<TextFace> {
    let face = parse_text_face(var.as_list());
    (face != SpecifiedTextFace::default()).then(|| {
        let default = TextFace::terminal_default();
        let fg = face.fg.unwrap_or(default.fg);
        let bg = face.bg.unwrap_or(default.bg);
        let underline_color = face.underline_color.unwrap_or(default.underline_color);
        let style = default.style.union_prefer_right(face.style);
        TextFace {
            fg,
            bg,
            underline_color,
            style,
        }
    })
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
        StatementDecoration::Command | StatementDecoration::Exec
    ) {
        builtin_ok = false;
        function_ok = false;
        abbreviation_ok = false;
        command_ok = true;
        implicit_cd_ok = false;
    } else if decoration == StatementDecoration::Builtin {
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
        is_valid = builtin_exists(cmd);
    }

    // Functions
    if !is_valid && function_ok {
        is_valid = function::exists_no_autoload(cmd);
    }

    // Abbreviations
    if !is_valid && abbreviation_ok {
        is_valid = with_abbrs(|set| set.has_match(cmd, abbrs::Position::Command, L!("")));
    }

    // Regular commands
    if !is_valid && command_ok {
        is_valid = path_get_path(cmd, vars).is_some();
    }

    // Implicit cd
    if !is_valid && implicit_cd_ok {
        is_valid = path_as_implicit_cd(cmd, working_directory, vars).is_some();
    }

    // Return what we got.
    is_valid
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
    ctx: &mut OperationContext<'_>,
) -> Option<(WString, WString)> {
    let flags = ParseTreeFlags {
        continue_after_error: true,
        accept_incomplete_tokens: true,
        ..Default::default()
    };
    let ast = ast::parse(buff, flags, None);

    // Find the first statement.
    let job_list: &ast::JobList = ast.top();
    let jc = job_list.first()?;
    let first_statement = jc.job.statement.as_decorated_statement()?;

    if let Some(expanded_command) = statement_get_expanded_command(buff, first_statement, ctx) {
        let mut arg = WString::new();
        // Check if the first argument or redirection is, in fact, an argument.
        if let Some(arg_or_redir) = first_statement.args_or_redirs.first() {
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
    item_commandline: &wstr,
    suggested_range: std::ops::Range<usize>,
    required_paths: &[WString],
    working_directory: &wstr,
    ctx: &mut OperationContext<'_>,
) -> bool {
    assert_is_background_thread();

    if suggested_range != (0..item_commandline.char_count())
        && get_process_first_token_offset(item_commandline, suggested_range.start)
            .is_some_and(|offset| offset != suggested_range.start)
    {
        return false;
    }

    // Parse the string.
    let suggested_command = &item_commandline[suggested_range];
    let Some((parsed_command, mut cd_dir)) = autosuggest_parse_command(suggested_command, ctx)
    else {
        // This is for autosuggestions which are not decorated commands, e.g. function declarations.
        return true;
    };

    // We handle cd specially.
    if is_veritable_cd(&parsed_command)
        && !cd_dir.is_empty()
        && expand_one(&mut cd_dir, ExpandFlags::FAIL_ON_CMDSUBST, ctx, None)
    {
        if string_prefixes_string(&cd_dir, L!("--help"))
            || string_prefixes_string(&cd_dir, L!("-h"))
        {
            // cd --help is always valid.
            return true;
        } else {
            // Check the directory target, respecting CDPATH.
            // Permit the autosuggestion if the path is valid and not our directory.
            let path = path_get_cdpath(&cd_dir, working_directory, ctx.vars());
            return path.is_some_and(|p| !paths_are_same_file(working_directory, &p));
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
    if !all_paths_are_valid(required_paths, ctx) {
        return false;
    }

    true
}

// Highlights the variable starting with 'in', setting colors within the 'colors' array. Returns the
// number of characters consumed.
fn color_variable(inp: &wstr, colors: &mut [HighlightSpec]) -> usize {
    assert_eq!(inp.char_at(0), '$');

    // Handle an initial run of $s.
    let mut idx = 0;
    let mut dollar_count = 0;
    while inp.char_at(idx) == '$' {
        // Our color depends on the next char.
        let next = inp.char_at(idx + 1);
        if next == '$' || valid_var_name_char(next) {
            colors[idx] = HighlightSpec::with_fg(HighlightRole::Operat);
        } else if next == '(' {
            colors[idx] = HighlightSpec::with_fg(HighlightRole::Operat);
            return idx + 1;
        } else {
            colors[idx] = HighlightSpec::with_fg(HighlightRole::Error);
        }
        idx += 1;
        dollar_count += 1;
    }

    // Handle a sequence of variable characters.
    // It may contain an escaped newline - see #8444.
    loop {
        if valid_var_name_char(inp.char_at(idx)) {
            colors[idx] = HighlightSpec::with_fg(HighlightRole::Operat);
            idx += 1;
        } else if inp.char_at(idx) == '\\' && inp.char_at(idx + 1) == '\n' {
            colors[idx] = HighlightSpec::with_fg(HighlightRole::Operat);
            idx += 1;
            colors[idx] = HighlightSpec::with_fg(HighlightRole::Operat);
            idx += 1;
        } else {
            break;
        }
    }

    // Handle a slice, up to dollar_count of them. Note that we currently don't do any validation of
    // the slice's contents, e.g. $foo[blah] will not show an error even though it's invalid.
    for _slice_count in 0..dollar_count {
        match slice_length(&inp[idx..]) {
            Some(slice_len) if slice_len > 0 => {
                colors[idx] = HighlightSpec::with_fg(HighlightRole::Operat);
                colors[idx + slice_len - 1] = HighlightSpec::with_fg(HighlightRole::Operat);
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
                colors[..=idx].fill(HighlightSpec::with_fg(HighlightRole::Error));
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
            HighlightSpec::with_fg(HighlightRole::Param),
            HighlightSpec::with_fg(HighlightRole::Option),
            HighlightSpec::with_fg(HighlightRole::Command)
        ]
        .contains(&base_color),
        "Unexpected base color"
    );
    let buff_len = buffstr.len();
    colors.fill(base_color);

    // Hacky support for %self which must be an unquoted literal argument.
    if buffstr == PROCESS_EXPAND_SELF_STR {
        colors[..PROCESS_EXPAND_SELF_STR.len()].fill(HighlightSpec::with_fg(HighlightRole::Operat));
        return;
    }

    #[derive(Eq, PartialEq)]
    enum Mode {
        Unquoted,
        SingleQuoted,
        DoubleQuoted,
    }
    let mut mode = Mode::Unquoted;
    let mut unclosed_quote_offset = None;
    let mut bracket_count = 0;
    let mut in_pos = 0;
    while in_pos < buff_len {
        let c = buffstr.as_char_slice()[in_pos];
        match mode {
            Mode::Unquoted => {
                if c == '\\' {
                    let mut fill_color = HighlightRole::Escape; // may be set to highlight_error
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
                        fill_color = HighlightRole::Error;
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
                            fill_color = HighlightRole::Error;
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
                        '~' if in_pos == 0 => {
                            colors[in_pos] = HighlightSpec::with_fg(HighlightRole::Operat);
                        }
                        '$' => {
                            assert!(in_pos < buff_len);
                            in_pos += color_variable(&buffstr[in_pos..], &mut colors[in_pos..]);
                            // Subtract one to account for the upcoming loop increment.
                            in_pos -= 1;
                        }
                        '?' if !feature_test(FeatureFlag::QuestionMarkNoGlob) => {
                            colors[in_pos] = HighlightSpec::with_fg(HighlightRole::Operat);
                        }
                        '*' | '(' | ')' => {
                            colors[in_pos] = HighlightSpec::with_fg(HighlightRole::Operat);
                        }
                        '{' => {
                            colors[in_pos] = HighlightSpec::with_fg(HighlightRole::Operat);
                            bracket_count += 1;
                        }
                        '}' => {
                            colors[in_pos] = HighlightSpec::with_fg(HighlightRole::Operat);
                            bracket_count -= 1;
                        }
                        ',' if bracket_count > 0 => {
                            colors[in_pos] = HighlightSpec::with_fg(HighlightRole::Operat);
                        }
                        '\'' => {
                            colors[in_pos] = HighlightSpec::with_fg(HighlightRole::Quote);
                            unclosed_quote_offset = Some(in_pos);
                            mode = Mode::SingleQuoted;
                        }
                        '"' => {
                            colors[in_pos] = HighlightSpec::with_fg(HighlightRole::Quote);
                            unclosed_quote_offset = Some(in_pos);
                            mode = Mode::DoubleQuoted;
                        }
                        _ => (), // we ignore all other characters
                    }
                }
            }
            // Mode 1 means single quoted string, i.e 'foo'.
            Mode::SingleQuoted => {
                colors[in_pos] = HighlightSpec::with_fg(HighlightRole::Quote);
                if c == '\\' {
                    // backslash
                    if in_pos + 1 < buff_len {
                        let escaped_char = buffstr.as_char_slice()[in_pos + 1];
                        if matches!(escaped_char, '\\' | '\'') {
                            colors[in_pos] = HighlightSpec::with_fg(HighlightRole::Escape); // backslash
                            colors[in_pos + 1] = HighlightSpec::with_fg(HighlightRole::Escape); // escaped char
                            in_pos += 1; // skip over backslash
                        }
                    }
                } else if c == '\'' {
                    mode = Mode::Unquoted;
                }
            }
            // Mode 2 means double quoted string, i.e. "foo".
            Mode::DoubleQuoted => {
                // Slices are colored in advance, past `in_pos`, and we don't want to overwrite
                // that.
                if colors[in_pos] == base_color {
                    colors[in_pos] = HighlightSpec::with_fg(HighlightRole::Quote);
                }
                match c {
                    '"' => {
                        mode = Mode::Unquoted;
                    }
                    '\\'
                        // Backslash
                        if in_pos + 1 < buff_len => {
                            let escaped_char = buffstr.as_char_slice()[in_pos + 1];
                            if matches!(escaped_char, '\\' | '"' | '\n' | '$') {
                                colors[in_pos] = HighlightSpec::with_fg(HighlightRole::Escape); // backslash
                                colors[in_pos + 1] = HighlightSpec::with_fg(HighlightRole::Escape); // escaped char
                                in_pos += 1; // skip over backslash
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
    if mode != Mode::Unquoted {
        colors[unclosed_quote_offset.unwrap()] = HighlightSpec::with_fg(HighlightRole::Error);
    }
}

pub type ColorArray = Vec<HighlightSpec>;

/// Syntax highlighter helper.
struct Highlighter<'src, 'ctx> {
    // The string we're highlighting. Note this is a reference member variable (to avoid copying)!
    buff: &'src wstr,
    // The position of the cursor within the string.
    cursor: Option<usize>,
    // Whether it's OK to do I/O.
    io_ok: bool,
    // Working directory.
    working_directory: WString,
    // Our component for testing strings for being potential file paths.
    file_tester: FileTester<'src, 'ctx>,
    // The resulting colors.
    color_array: ColorArray,
    // A stack of variables that the current commandline probably defines.  We mark redirections
    // as valid if they use one of these variables, to avoid marking valid targets as error.
    pending_variables: Vec<&'src wstr>,
    done: bool,
}

impl<'src, 'ctx> Highlighter<'src, 'ctx> {
    pub fn new(
        buff: &'src wstr,
        cursor: Option<usize>,
        ctx: &'ctx mut OperationContext<'src>,
        working_directory: WString,
        can_do_io: bool,
    ) -> Self {
        let file_tester = FileTester::new(working_directory.clone(), ctx);
        Self {
            buff,
            cursor,
            io_ok: can_do_io,
            working_directory,
            file_tester,
            color_array: vec![],
            pending_variables: vec![],
            done: false,
        }
    }

    fn ctx(&self) -> &OperationContext<'src> {
        self.file_tester.ctx
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
        let ast_flags = ParseTreeFlags {
            continue_after_error: true,
            include_comments: true,
            accept_incomplete_tokens: true,
            leave_unterminated: true,
            show_extra_semis: true,
            ..Default::default()
        };
        let ast = ast::parse(self.buff, ast_flags, None);

        self.visit_children(ast.top());
        if self.file_tester.ctx.check_cancel() {
            return std::mem::take(&mut self.color_array);
        }

        // Color every comment.
        let extras = &ast.extras;
        for range in &extras.comments {
            self.color_range(*range, HighlightSpec::with_fg(HighlightRole::Comment));
        }

        // Color every extra semi.
        for range in &extras.semis {
            self.color_range(
                *range,
                HighlightSpec::with_fg(HighlightRole::StatementTerminator),
            );
        }

        // Color every error range.
        for range in &extras.errors {
            self.color_range(*range, HighlightSpec::with_fg(HighlightRole::Error));
        }

        std::mem::take(&mut self.color_array)
    }

    /// Return a substring of our buffer.
    pub fn get_source(&self, r: SourceRange) -> &'src wstr {
        assert!(r.end() >= r.start(), "Overflow");
        assert!(r.end() <= self.buff.len(), "Out of range");
        &self.buff[r.start()..r.end()]
    }

    fn io_still_ok(&self) -> bool {
        self.io_ok && !self.ctx().check_cancel()
    }

    // Color a command.
    fn color_command(&mut self, node: &ast::String_) {
        let source_range = node.source_range();
        let cmd_str = self.get_source(source_range);

        color_string_internal(
            cmd_str,
            HighlightSpec::with_fg(HighlightRole::Command),
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
                HighlightSpec::with_fg(HighlightRole::Option),
                &mut self.color_array[source_range.as_usize()],
            );
        } else {
            color_string_internal(
                arg_str,
                HighlightSpec::with_fg(HighlightRole::Param),
                &mut self.color_array[source_range.as_usize()],
            );
        }

        // Now do command substitutions.
        let mut cmdsub_cursor = 0;
        let mut is_quoted = false;
        while let MaybeParentheses::CommandSubstitution(parens) = locate_cmdsubst_range(
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
                .fill(HighlightSpec::with_fg(HighlightRole::Operat));
            self.color_array[arg_start..][parens.closing()]
                .fill(HighlightSpec::with_fg(HighlightRole::Operat));

            // Highlight it recursively.
            let arg_cursor = self
                .cursor
                .map(|c| c.wrapping_sub(arg_start + parens.start()));
            let cmdsub_contents = &arg_str[parens.command()];
            let mut cmdsub_highlighter = Highlighter::new(
                cmdsub_contents,
                arg_cursor,
                self.file_tester.ctx,
                self.working_directory.clone(),
                self.io_still_ok(),
            );
            let subcolors = cmdsub_highlighter.highlight();

            // Copy out the subcolors back into our array.
            assert_eq!(subcolors.len(), cmdsub_contents.len());
            self.color_array[arg_start..][parens.command()].copy_from_slice(&subcolors);
        }
    }
    // Colors the source range of a node with a given color.
    fn color_node(&mut self, node: &dyn ast::Node, color: HighlightSpec) {
        self.color_range(node.source_range(), color);
    }
    // Colors a range with a given color.
    fn color_range(&mut self, range: SourceRange, color: HighlightSpec) {
        assert!(range.end() <= self.color_array.len(), "Range out of bounds");
        self.color_array[range.start()..range.end()].fill(color);
    }

    // Visit the children of a node.
    fn visit_children(&mut self, node: &dyn Node) {
        node.accept(self);
    }
    // AST visitor implementations.
    fn visit_keyword(&mut self, node: &dyn Keyword) {
        let mut role = HighlightRole::Normal;
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
            | ParseKeyword::While => role = HighlightRole::Keyword,
            ParseKeyword::And
            | ParseKeyword::Or
            | ParseKeyword::Not
            | ParseKeyword::Exclam
            | ParseKeyword::Time => role = HighlightRole::Operat,
            ParseKeyword::None => (),
        }
        self.color_node(node.as_node(), HighlightSpec::with_fg(role));
    }
    fn visit_token(&mut self, tok: &dyn Token) {
        let mut role = HighlightRole::Normal;
        match tok.token_type() {
            ParseTokenType::End | ParseTokenType::Pipe | ParseTokenType::Background => {
                role = HighlightRole::StatementTerminator;
            }
            ParseTokenType::LeftBrace | ParseTokenType::RightBrace => {
                role = HighlightRole::Keyword;
            }
            ParseTokenType::AndAnd | ParseTokenType::OrOr => role = HighlightRole::Operat,
            ParseTokenType::String => {
                // Assume all strings are params. This handles e.g. the variables a for header or
                // function header. Other strings (like arguments to commands) need more complex
                // handling, which occurs in their respective overrides of visit().
                role = HighlightRole::Param;
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
            Err(..) => self.color_node(arg, HighlightSpec::with_fg(HighlightRole::Error)),
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
            self.color_node(redir, HighlightSpec::with_fg(HighlightRole::Error));
            return;
        }

        // Color the operator part like 2>.
        self.color_node(
            &redir.oper,
            HighlightSpec::with_fg(HighlightRole::Redirection),
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
        let (role, file_exists) = if !self.io_still_ok() {
            // I/O is disallowed, so we don't have much hope of catching anything but gross
            // errors. Assume it's valid.
            (HighlightRole::Redirection, false)
        } else if contains_pending_variable(&self.pending_variables, &target) {
            // Target uses a variable defined by the current commandline. Assume it's valid.
            (HighlightRole::Redirection, false)
        } else {
            // Validate the redirection target..
            if let Ok(IsFile(file_exists)) =
                self.file_tester.test_redirection_target(&target, oper.mode)
            {
                (HighlightRole::Redirection, file_exists)
            } else {
                (HighlightRole::Error, false)
            }
        };
        self.color_node(&redir.target, HighlightSpec::with_fg(role));
        if file_exists {
            for i in redir.target.source_range().as_usize() {
                self.color_array[i].valid_path = true;
            }
        }
    }

    fn visit_variable_assignment(&mut self, varas: &VariableAssignment) {
        self.color_as_argument(varas, true);
        // Highlight the '=' in variable assignments as an operator.
        if let Some(offset) = variable_assignment_equals_pos(varas.source(self.buff)) {
            let equals_loc = varas.source_range().start() + offset;
            self.color_array[equals_loc] = HighlightSpec::with_fg(HighlightRole::Operat);
            let var_name = &varas.source(self.buff)[..offset];
            self.pending_variables.push(var_name);
        }
    }
    fn visit_semi_nl(&mut self, node: &dyn Node) {
        self.color_node(
            node,
            HighlightSpec::with_fg(HighlightRole::StatementTerminator),
        );
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
            if let Some(expanded) =
                statement_get_expanded_command(self.buff, stmt, self.file_tester.ctx)
            {
                expanded_cmd = expanded;
                if !has_expand_reserved(&expanded_cmd) {
                    is_valid_cmd = command_is_valid(
                        &expanded_cmd,
                        stmt.decoration(),
                        &self.working_directory,
                        self.file_tester.ctx.vars(),
                    );
                }
            }
        }

        // Color our statement.
        if is_valid_cmd {
            self.color_command(&stmt.command);
        } else {
            self.color_node(&stmt.command, HighlightSpec::with_fg(HighlightRole::Error));
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
            BlockStatementHeader::For(node) => self.visit(node),
            BlockStatementHeader::While(node) => self.visit(node),
            BlockStatementHeader::Function(node) => self.visit(node),
            BlockStatementHeader::Begin(node) => self.visit(node),
        }
        self.visit(&block.args_or_redirs);
        let pending_variables_count = self.pending_variables.len();
        if let BlockStatementHeader::For(fh) = &block.header {
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
    match locate_cmdsubst_range(src, &mut cursor, true, None, None) {
        MaybeParentheses::Error => false,
        MaybeParentheses::None => false,
        MaybeParentheses::CommandSubstitution(_) => true,
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

impl<'src, 'ctx, 'a> NodeVisitor<'a> for Highlighter<'src, 'ctx> {
    fn visit(&mut self, node: &'a dyn Node) {
        if let Some(keyword) = node.as_keyword() {
            return self.visit_keyword(keyword);
        }
        if let Some(token) = node.as_token() {
            if token.token_type() == ParseTokenType::End {
                self.visit_semi_nl(node);
                return;
            }
            self.visit_token(token);
            return;
        }
        match node.kind() {
            Kind::Argument(node) => self.visit_argument(node, false, true),
            Kind::Redirection(node) => self.visit_redirection(node),
            Kind::VariableAssignment(node) => self.visit_variable_assignment(node),
            Kind::DecoratedStatement(node) => self.visit_decorated_statement(node),
            Kind::BlockStatement(node) => self.visit_block_statement(node),
            Kind::BraceStatement(node) => self.visit_brace_statement(node),
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
    ctx: &mut OperationContext<'_>,
) -> Option<WString> {
    // Get the command. Try expanding it. If we cannot, it's an error.
    let cmd = stmt.command.try_source(src)?;
    let mut out_cmd = WString::new();
    let err = expand_to_command_and_args(cmd, ctx, &mut out_cmd, None, None, false);
    (err == ExpandResultCode::ok).then_some(out_cmd)
}

fn get_highlight_var_name(role: HighlightRole) -> &'static wstr {
    match role {
        HighlightRole::Normal => L!("fish_color_normal"),
        HighlightRole::Error => L!("fish_color_error"),
        HighlightRole::Command => L!("fish_color_command"),
        HighlightRole::Keyword => L!("fish_color_keyword"),
        HighlightRole::StatementTerminator => L!("fish_color_end"),
        HighlightRole::Param => L!("fish_color_param"),
        HighlightRole::Option => L!("fish_color_option"),
        HighlightRole::Comment => L!("fish_color_comment"),
        HighlightRole::SearchMatch => L!("fish_color_search_match"),
        HighlightRole::Operat => L!("fish_color_operator"),
        HighlightRole::Escape => L!("fish_color_escape"),
        HighlightRole::Quote => L!("fish_color_quote"),
        HighlightRole::Redirection => L!("fish_color_redirection"),
        HighlightRole::Autosuggestion => L!("fish_color_autosuggestion"),
        HighlightRole::Selection => L!("fish_color_selection"),
        HighlightRole::PagerProgress => L!("fish_pager_color_progress"),
        HighlightRole::PagerBackground => L!("fish_pager_color_background"),
        HighlightRole::PagerPrefix => L!("fish_pager_color_prefix"),
        HighlightRole::PagerCompletion => L!("fish_pager_color_completion"),
        HighlightRole::PagerDescription => L!("fish_pager_color_description"),
        HighlightRole::PagerSecondaryBackground => L!("fish_pager_color_secondary_background"),
        HighlightRole::PagerSecondaryPrefix => L!("fish_pager_color_secondary_prefix"),
        HighlightRole::PagerSecondaryCompletion => L!("fish_pager_color_secondary_completion"),
        HighlightRole::PagerSecondaryDescription => L!("fish_pager_color_secondary_description"),
        HighlightRole::PagerSelectedBackground => L!("fish_pager_color_selected_background"),
        HighlightRole::PagerSelectedPrefix => L!("fish_pager_color_selected_prefix"),
        HighlightRole::PagerSelectedCompletion => L!("fish_pager_color_selected_completion"),
        HighlightRole::PagerSelectedDescription => L!("fish_pager_color_selected_description"),
    }
}

// Table used to fetch fallback highlights in case the specified one
// wasn't set.
fn get_fallback(role: HighlightRole) -> HighlightRole {
    match role {
        HighlightRole::Normal
        | HighlightRole::Error
        | HighlightRole::Command
        | HighlightRole::StatementTerminator
        | HighlightRole::Param
        | HighlightRole::SearchMatch
        | HighlightRole::Comment
        | HighlightRole::Operat
        | HighlightRole::Escape
        | HighlightRole::Quote
        | HighlightRole::Redirection
        | HighlightRole::Autosuggestion
        | HighlightRole::Selection
        | HighlightRole::PagerProgress
        | HighlightRole::PagerBackground
        | HighlightRole::PagerPrefix
        | HighlightRole::PagerCompletion
        | HighlightRole::PagerDescription => HighlightRole::Normal,
        HighlightRole::Keyword => HighlightRole::Command,
        HighlightRole::Option => HighlightRole::Param,
        HighlightRole::PagerSecondaryBackground => HighlightRole::PagerBackground,
        HighlightRole::PagerSecondaryPrefix | HighlightRole::PagerSelectedPrefix => {
            HighlightRole::PagerPrefix
        }
        HighlightRole::PagerSecondaryCompletion | HighlightRole::PagerSelectedCompletion => {
            HighlightRole::PagerCompletion
        }
        HighlightRole::PagerSecondaryDescription | HighlightRole::PagerSelectedDescription => {
            HighlightRole::PagerDescription
        }
        HighlightRole::PagerSelectedBackground => HighlightRole::SearchMatch,
    }
}

/// Describes the role of a span of text.
#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq, Display)]
#[strum(serialize_all = "snake_case")]
#[repr(u8)]
pub enum HighlightRole {
    #[default]
    Normal, // normal text
    Error,   // error
    Command, // command
    Keyword,
    StatementTerminator, // process separator
    Param,               // command parameter (argument)
    Option,              // argument starting with "-", up to a "--"
    Comment,             // comment
    SearchMatch,         // search match
    Operat,              // operator
    Escape,              // escape sequences
    Quote,               // quoted string
    Redirection,         // redirection
    Autosuggestion,      // autosuggestion
    Selection,

    // Pager support.
    // NOTE: pager.rs relies on these being in this order.
    PagerProgress,
    PagerBackground,
    PagerPrefix,
    PagerCompletion,
    PagerDescription,
    PagerSecondaryBackground,
    PagerSecondaryPrefix,
    PagerSecondaryCompletion,
    PagerSecondaryDescription,
    PagerSelectedBackground,
    PagerSelectedPrefix,
    PagerSelectedCompletion,
    PagerSelectedDescription,
}

/// Simple value type describing how a character should be highlighted.
#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
pub struct HighlightSpec {
    pub foreground: HighlightRole,
    pub background: HighlightRole,
    pub valid_path: bool,
    pub force_underline: bool,
}

#[cfg(test)]
mod tests {
    use super::{HighlightColorResolver, HighlightRole, HighlightSpec, highlight_shell};
    use crate::env::{EnvMode, EnvSetMode, EnvVar, EnvVarFlags, Environment as _};
    use crate::highlight::parse_text_face_for_highlight;
    use crate::operation_context::{EXPANSION_LIMIT_BACKGROUND, OperationContext};
    use crate::prelude::*;
    use crate::tests::prelude::*;
    use crate::text_face::{ResettableStyle, UnderlineStyle};
    use fish_common::ScopeGuard;
    use fish_feature_flags::{FeatureFlag, with_overridden_feature};
    use libc::PATH_MAX;

    // Helper to return a string whose length greatly exceeds PATH_MAX.
    fn get_overlong_path() -> String {
        let path_max = usize::try_from(PATH_MAX).unwrap();
        let mut longpath = String::with_capacity(path_max * 2 + 10);
        while longpath.len() <= path_max * 2 {
            longpath += "/overlong";
        }
        longpath
    }

    #[test]
    #[serial]
    fn test_highlighting() {
        test_init();
        let TestParser {
            ref mut parser,
            ref mut pushed_dirs,
        } = TestParser::new();
        // Testing syntax highlighting
        parser.pushd(pushed_dirs, "test/fish_highlight_test/");
        let parser = &mut **ScopeGuard::new(parser, |parser| parser.popd(pushed_dirs));
        std::fs::create_dir_all("dir").unwrap();
        std::fs::create_dir_all("cdpath-entry/dir-in-cdpath").unwrap();
        std::fs::write("foo", []).unwrap();
        std::fs::write("bar", []).unwrap();

        // Here are the components of our source and the colors we expect those to be.
        #[derive(Debug)]
        struct HighlightComponent<'a> {
            text: &'a str,
            color: HighlightSpec,
            nospace: bool,
        }

        macro_rules! component {
            ( ( $text:expr, $color:expr) ) => {
                HighlightComponent {
                    text: $text,
                    color: $color,
                    nospace: false,
                }
            };
            ( ( $text:literal, $color:expr, ns ) ) => {
                HighlightComponent {
                    text: $text,
                    color: $color,
                    nospace: true,
                }
            };
        }

        macro_rules! validate {
            ( $($comp:tt),* $(,)? ) => {
                let components = [
                    $(
                        component!($comp),
                    )*
                ];
                let vars = parser.vars();
                // Generate the text.
                let mut text = WString::new();
                let mut expected_colors = vec![];
                for comp in &components {
                    if !text.is_empty() && !comp.nospace {
                        text.push(' ');
                        expected_colors.push(HighlightSpec::new());
                    }
                    text.push_str(comp.text);
                    expected_colors.resize(text.len(), comp.color);
                }
                assert_eq!(text.len(), expected_colors.len());

                let mut colors = vec![];
                highlight_shell(
                    &text,
                    &mut colors,
                    &mut OperationContext::background(vars, EXPANSION_LIMIT_BACKGROUND),
                    true, /* io_ok */
                    Some(text.len()),
                );
                assert_eq!(colors.len(), expected_colors.len());

                for (i, c) in text.chars().enumerate() {
                    // Hackish space handling. We don't care about the colors in spaces.
                    if c == ' ' {
                        continue;
                    }

                    assert_eq!(colors[i], expected_colors[i], "Failed at position {i}, char {c}");
                }
            };
        }

        let mut param_valid_path = HighlightSpec::with_fg(HighlightRole::Param);
        param_valid_path.valid_path = true;

        let mut redirection_valid_path = HighlightSpec::with_fg(HighlightRole::Redirection);
        redirection_valid_path.valid_path = true;

        with_overridden_feature(FeatureFlag::AmpersandNoBgInToken, true, || {
            let fg = HighlightSpec::with_fg;

            // Verify variables and wildcards in commands using /bin/cat.
            let vars = parser.vars();
            let local_mode = EnvSetMode::new_at_early_startup(EnvMode::LOCAL);
            vars.set_one(L!("CDPATH"), local_mode, L!("./cdpath-entry").to_owned());

            // NOTE n, nv are suffix of /usr/bin/env
            vars.set_one(L!("VARIABLE_IN_COMMAND"), local_mode, L!("n").to_owned());
            vars.set_one(L!("VARIABLE_IN_COMMAND2"), local_mode, L!("nv").to_owned());

            let _cleanup = ScopeGuard::new((), |_| {
                vars.remove(L!("VARIABLE_IN_COMMAND"), EnvSetMode::default());
                vars.remove(L!("VARIABLE_IN_COMMAND2"), EnvSetMode::default());
            });

            validate!(
                ("echo", fg(HighlightRole::Command)),
                ("./foo", param_valid_path),
                ("&", fg(HighlightRole::StatementTerminator)),
            );

            validate!(
                ("command", fg(HighlightRole::Keyword)),
                ("echo", fg(HighlightRole::Command)),
                ("abc", fg(HighlightRole::Param)),
                ("foo", param_valid_path),
                ("&", fg(HighlightRole::StatementTerminator)),
            );

            validate!(
                ("echo", fg(HighlightRole::Command)),
                ("foo&bar", fg(HighlightRole::Param)),
                ("foo", fg(HighlightRole::Param), ns),
                ("&", fg(HighlightRole::StatementTerminator)),
                ("echo", fg(HighlightRole::Command)),
                ("&>", fg(HighlightRole::Redirection)),
            );

            validate!(
                ("if command", fg(HighlightRole::Keyword)),
                ("ls", fg(HighlightRole::Command)),
                ("; ", fg(HighlightRole::StatementTerminator)),
                ("echo", fg(HighlightRole::Command)),
                ("abc", fg(HighlightRole::Param)),
                ("; ", fg(HighlightRole::StatementTerminator)),
                ("/bin/definitely_not_a_command", fg(HighlightRole::Error)),
                ("; ", fg(HighlightRole::StatementTerminator)),
                ("end", fg(HighlightRole::Keyword)),
            );

            validate!(
                ("if", fg(HighlightRole::Keyword)),
                ("true", fg(HighlightRole::Command)),
                (";", fg(HighlightRole::StatementTerminator)),
                ("else", fg(HighlightRole::Keyword)),
                ("true", fg(HighlightRole::Command)),
                (";", fg(HighlightRole::StatementTerminator)),
                ("end", fg(HighlightRole::Keyword)),
            );

            // Verify that cd shows errors for non-directories.
            validate!(
                ("cd", fg(HighlightRole::Command)),
                ("dir", param_valid_path),
            );

            validate!(
                ("cd", fg(HighlightRole::Command)),
                ("foo", fg(HighlightRole::Error)),
            );

            validate!(
                ("cd", fg(HighlightRole::Command)),
                ("--help", fg(HighlightRole::Option)),
                ("-h", fg(HighlightRole::Option)),
                ("definitely_not_a_directory", fg(HighlightRole::Error)),
            );

            validate!(
                ("cd", fg(HighlightRole::Command)),
                ("dir-in-cdpath", param_valid_path),
            );

            // Command substitutions.
            validate!(
                ("echo", fg(HighlightRole::Command)),
                ("param1", fg(HighlightRole::Param)),
                ("-l", fg(HighlightRole::Option)),
                ("--", fg(HighlightRole::Option)),
                ("-l", fg(HighlightRole::Param)),
                ("(", fg(HighlightRole::Operat)),
                ("ls", fg(HighlightRole::Command)),
                ("-l", fg(HighlightRole::Option)),
                ("--", fg(HighlightRole::Option)),
                ("-l", fg(HighlightRole::Param)),
                ("param2", fg(HighlightRole::Param)),
                (")", fg(HighlightRole::Operat)),
                ("|", fg(HighlightRole::StatementTerminator)),
                ("cat", fg(HighlightRole::Command)),
            );
            validate!(
                ("true", fg(HighlightRole::Command)),
                ("$(", fg(HighlightRole::Operat)),
                ("true", fg(HighlightRole::Command)),
                (")", fg(HighlightRole::Operat)),
            );
            validate!(
                ("true", fg(HighlightRole::Command)),
                ("\"before", fg(HighlightRole::Quote)),
                ("$(", fg(HighlightRole::Operat)),
                ("true", fg(HighlightRole::Command)),
                ("param1", fg(HighlightRole::Param)),
                (")", fg(HighlightRole::Operat)),
                ("after\"", fg(HighlightRole::Quote)),
                ("param2", fg(HighlightRole::Param)),
            );
            validate!(
                ("true", fg(HighlightRole::Command)),
                ("\"", fg(HighlightRole::Error)),
                ("unclosed quote", fg(HighlightRole::Quote)),
                ("$(", fg(HighlightRole::Operat)),
                ("true", fg(HighlightRole::Command)),
                (")", fg(HighlightRole::Operat)),
            );

            // Redirections substitutions.
            validate!(
                ("echo", fg(HighlightRole::Command)),
                ("param1", fg(HighlightRole::Param)),
                // Input redirection.
                ("<", fg(HighlightRole::Redirection)),
                ("/dev/null", redirection_valid_path),
                // Output redirection to a valid fd.
                ("1>&2", fg(HighlightRole::Redirection)),
                // Output redirection to an invalid fd.
                ("2>&", fg(HighlightRole::Redirection)),
                ("LO", fg(HighlightRole::Error)),
                // Just a param, not a redirection.
                ("test/blah", fg(HighlightRole::Param)),
                // Input redirection from directory.
                ("<", fg(HighlightRole::Redirection)),
                ("test/", fg(HighlightRole::Error)),
                // Output redirection to an invalid path.
                ("3>", fg(HighlightRole::Redirection)),
                ("/not/a/valid/path/nope", fg(HighlightRole::Error)),
                // Output redirection to directory.
                ("3>", fg(HighlightRole::Redirection)),
                ("test/nope/", fg(HighlightRole::Error)),
                // Redirections to overflow fd.
                ("99999999999999999999>&2", fg(HighlightRole::Error)),
                ("2>&", fg(HighlightRole::Redirection)),
                ("99999999999999999999", fg(HighlightRole::Error)),
                // Output redirection containing a command substitution.
                ("4>", fg(HighlightRole::Redirection)),
                ("(", fg(HighlightRole::Operat)),
                ("echo", fg(HighlightRole::Command)),
                ("test/somewhere", fg(HighlightRole::Param)),
                (")", fg(HighlightRole::Operat)),
                // Just another param.
                ("param2", fg(HighlightRole::Param)),
            );

            validate!(
                ("for", fg(HighlightRole::Keyword)),
                ("x", fg(HighlightRole::Param)),
                ("in", fg(HighlightRole::Keyword)),
                ("set-by-for-1", fg(HighlightRole::Param)),
                ("set-by-for-2", fg(HighlightRole::Param)),
                (";", fg(HighlightRole::StatementTerminator)),
                ("echo", fg(HighlightRole::Command)),
                (">", fg(HighlightRole::Redirection)),
                ("$x", fg(HighlightRole::Redirection)),
                (";", fg(HighlightRole::StatementTerminator)),
                ("end", fg(HighlightRole::Keyword)),
            );

            validate!(
                ("set", fg(HighlightRole::Command)),
                ("x", fg(HighlightRole::Param)),
                ("set-by-set", fg(HighlightRole::Param)),
                (";", fg(HighlightRole::StatementTerminator)),
                ("echo", fg(HighlightRole::Command)),
                (">", fg(HighlightRole::Redirection)),
                ("$x", fg(HighlightRole::Redirection)),
                ("2>", fg(HighlightRole::Redirection)),
                ("$totally_not_x", fg(HighlightRole::Error)),
                ("<", fg(HighlightRole::Redirection)),
                ("$x_but_its_an_impostor", fg(HighlightRole::Error)),
            );

            validate!(
                ("x", fg(HighlightRole::Param), ns),
                ("=", fg(HighlightRole::Operat), ns),
                ("set-by-variable-override", fg(HighlightRole::Param), ns),
                ("echo", fg(HighlightRole::Command)),
                (">", fg(HighlightRole::Redirection)),
                ("$x", fg(HighlightRole::Redirection)),
            );

            validate!(
                ("end", fg(HighlightRole::Error)),
                (";", fg(HighlightRole::StatementTerminator)),
                ("if", fg(HighlightRole::Keyword)),
                ("end", fg(HighlightRole::Error)),
            );

            validate!(
                ("echo", fg(HighlightRole::Command)),
                ("'", fg(HighlightRole::Error)),
                ("single_quote", fg(HighlightRole::Quote)),
                ("$stuff", fg(HighlightRole::Quote)),
            );

            validate!(
                ("echo", fg(HighlightRole::Command)),
                ("\"", fg(HighlightRole::Error)),
                ("double_quote", fg(HighlightRole::Quote)),
                ("$stuff", fg(HighlightRole::Operat)),
            );

            validate!(
                ("echo", fg(HighlightRole::Command)),
                ("$foo", fg(HighlightRole::Operat)),
                ("\"", fg(HighlightRole::Quote)),
                ("$bar", fg(HighlightRole::Operat)),
                ("\"", fg(HighlightRole::Quote)),
                ("$baz[", fg(HighlightRole::Operat)),
                ("1 2..3", fg(HighlightRole::Param)),
                ("]", fg(HighlightRole::Operat)),
            );

            validate!(
                ("for", fg(HighlightRole::Keyword)),
                ("i", fg(HighlightRole::Param)),
                ("in", fg(HighlightRole::Keyword)),
                ("1 2 3", fg(HighlightRole::Param)),
                (";", fg(HighlightRole::StatementTerminator)),
                ("end", fg(HighlightRole::Keyword)),
            );

            validate!(
                ("echo", fg(HighlightRole::Command)),
                ("$$foo[", fg(HighlightRole::Operat)),
                ("1", fg(HighlightRole::Param)),
                ("][", fg(HighlightRole::Operat)),
                ("2", fg(HighlightRole::Param)),
                ("]", fg(HighlightRole::Operat)),
                ("[3]", fg(HighlightRole::Param)), // two dollar signs, so last one is not an expansion
            );

            validate!(
                ("cat", fg(HighlightRole::Command)),
                ("/dev/null", param_valid_path),
                ("|", fg(HighlightRole::StatementTerminator)),
                // This is bogus, but we used to use "less" here and that doesn't have to be installed.
                ("cat", fg(HighlightRole::Command)),
                ("2>", fg(HighlightRole::Redirection)),
            );

            // Highlight path-prefixes only at the cursor.
            validate!(
                ("cat", fg(HighlightRole::Command)),
                ("/dev/nu", fg(HighlightRole::Param)),
                ("/dev/nu", param_valid_path),
            );

            validate!(
                ("if", fg(HighlightRole::Keyword)),
                ("true", fg(HighlightRole::Command)),
                ("&&", fg(HighlightRole::Operat)),
                ("false", fg(HighlightRole::Command)),
                (";", fg(HighlightRole::StatementTerminator)),
                ("or", fg(HighlightRole::Operat)),
                ("false", fg(HighlightRole::Command)),
                ("||", fg(HighlightRole::Operat)),
                ("true", fg(HighlightRole::Command)),
                (";", fg(HighlightRole::StatementTerminator)),
                ("and", fg(HighlightRole::Operat)),
                ("not", fg(HighlightRole::Operat)),
                ("!", fg(HighlightRole::Operat)),
                ("true", fg(HighlightRole::Command)),
                (";", fg(HighlightRole::StatementTerminator)),
                ("end", fg(HighlightRole::Keyword)),
            );

            validate!(
                ("echo", fg(HighlightRole::Command)),
                ("%self", fg(HighlightRole::Operat)),
                ("not%self", fg(HighlightRole::Param)),
                ("self%not", fg(HighlightRole::Param)),
            );

            validate!(
                ("false", fg(HighlightRole::Command)),
                ("&|", fg(HighlightRole::StatementTerminator)),
                ("true", fg(HighlightRole::Command)),
            );

            validate!(
                ("HOME", fg(HighlightRole::Param)),
                ("=", fg(HighlightRole::Operat), ns),
                (".", fg(HighlightRole::Param), ns),
                ("VAR1", fg(HighlightRole::Param)),
                ("=", fg(HighlightRole::Operat), ns),
                ("VAL1", fg(HighlightRole::Param), ns),
                ("VAR", fg(HighlightRole::Param)),
                ("=", fg(HighlightRole::Operat), ns),
                ("false", fg(HighlightRole::Command)),
                ("|&", fg(HighlightRole::StatementTerminator)),
                ("true", fg(HighlightRole::Command)),
                ("stuff", fg(HighlightRole::Param)),
            );

            validate!(
                ("echo", fg(HighlightRole::Command)), // (
                (")", fg(HighlightRole::Error)),
            );

            validate!(
                ("echo", fg(HighlightRole::Command)),
                ("stuff", fg(HighlightRole::Param)),
                ("# comment", fg(HighlightRole::Comment)),
            );

            validate!(
                ("echo", fg(HighlightRole::Command)),
                ("--", fg(HighlightRole::Option)),
                ("-s", fg(HighlightRole::Param)),
            );

            // Overlong paths don't crash (#7837).
            let overlong = get_overlong_path();
            validate!(
                ("touch", fg(HighlightRole::Command)),
                (&overlong, fg(HighlightRole::Param)),
            );

            validate!(
                ("a", fg(HighlightRole::Param)),
                ("=", fg(HighlightRole::Operat), ns),
            );

            // Highlighting works across escaped line breaks (#8444).
            validate!(
                ("echo", fg(HighlightRole::Command)),
                ("$FISH_\\\n", fg(HighlightRole::Operat)),
                ("VERSION", fg(HighlightRole::Operat), ns),
            );

            // NOTE: we assume /usr/bin/env exists on the system here
            validate!(
                ("/usr/bin/en", fg(HighlightRole::Command), ns),
                ("*", fg(HighlightRole::Operat), ns)
            );

            validate!(
                ("/usr/bin/e", fg(HighlightRole::Command), ns),
                ("*", fg(HighlightRole::Operat), ns)
            );

            validate!(
                ("/usr/bin/e", fg(HighlightRole::Command), ns),
                ("{$VARIABLE_IN_COMMAND}", fg(HighlightRole::Operat), ns),
                ("*", fg(HighlightRole::Operat), ns)
            );

            validate!(
                ("/usr/bin/e", fg(HighlightRole::Command), ns),
                ("$VARIABLE_IN_COMMAND2", fg(HighlightRole::Operat), ns)
            );

            validate!(("$EMPTY_VARIABLE", fg(HighlightRole::Error)));
            validate!(("\"$EMPTY_VARIABLE\"", fg(HighlightRole::Error)));

            validate!(
                ("echo", fg(HighlightRole::Command)),
                ("\\UFDFD", fg(HighlightRole::Escape)),
            );
            validate!(
                ("echo", fg(HighlightRole::Command)),
                ("\\U10FFFF", fg(HighlightRole::Escape)),
            );
            validate!(
                ("echo", fg(HighlightRole::Command)),
                ("\\U110000", fg(HighlightRole::Error)),
            );

            validate!(
                (">", fg(HighlightRole::Error)),
                ("echo", fg(HighlightRole::Error)),
            );
        });
    }

    /// Tests that trailing spaces after a command don't inherit the underline formatting of the
    /// command.
    #[test]
    #[serial]
    #[allow(clippy::needless_range_loop)]
    fn test_trailing_spaces_after_command() {
        test_init();
        let parser = &mut TestParser::new();
        let vars = parser.vars();

        // First, set up fish_color_command to include underline
        vars.set_one(
            L!("fish_color_command"),
            EnvSetMode::new_at_early_startup(EnvMode::LOCAL),
            L!("--underline").to_owned(),
        );

        // Prepare a command with trailing spaces for highlighting
        let text = L!("echo   ").to_owned(); // Command 'echo' followed by 3 spaces
        let mut colors = vec![];
        highlight_shell(
            &text,
            &mut colors,
            &mut OperationContext::background(vars, EXPANSION_LIMIT_BACKGROUND),
            true, /* io_ok */
            Some(text.len()),
        );

        // Verify we have the right number of colors
        assert_eq!(colors.len(), text.len());

        // Create a resolver to check actual RGB colors with their attributes
        let mut resolver = HighlightColorResolver::new();

        // Check that 'echo' is underlined
        for i in 0..4 {
            let face = resolver.resolve_spec(&colors[i], vars);
            assert_eq!(
                face.style.underline_style(),
                ResettableStyle::On(UnderlineStyle::Single),
                "Character at position {} of 'echo' should be underlined",
                i
            );
        }

        // Check that trailing spaces are NOT underlined
        for i in 4..text.len() {
            let face = resolver.resolve_spec(&colors[i], vars);
            assert_eq!(
                face.style.underline_style(),
                ResettableStyle::Off,
                "Trailing space at position {} should NOT be underlined",
                i
            );
        }
    }

    #[test]
    #[serial]
    fn test_resolve_role() {
        test_init();
        let parser = &mut TestParser::new();
        let vars = parser.vars();

        let set = |var: &wstr, value: Vec<WString>| {
            vars.set(var, EnvSetMode::new(EnvMode::LOCAL, false), value);
        };
        set(L!("fish_color_normal"), vec![L!("normal").into()]);
        set(
            L!("fish_color_command"),
            vec![L!("red").into(), L!("--bold").into()],
        );
        set(L!("fish_color_keyword"), vec![L!("--theme=default").into()]);

        let keyword_spec = HighlightSpec::with_both(HighlightRole::Keyword);
        let face = HighlightColorResolver::resolve_spec_uncached(&keyword_spec, vars);

        let command_face =
            parse_text_face_for_highlight(&vars.get(L!("fish_color_command")).unwrap()).unwrap();
        assert_eq!(face, command_face);
    }

    #[test]
    fn test_parse_text_face_for_highlight_fully_specified() {
        let assert_all_set = |values: Vec<WString>| {
            let var = EnvVar::new_vec(values.clone(), EnvVarFlags::empty());
            let face = parse_text_face_for_highlight(&var);
            assert!(
                face.is_some_and(|face| face.all_set()),
                "Underspecified result for {:?}\n => {:?}",
                values,
                face
            );
        };

        assert_all_set(vec![L!("--reset").into()]);
        assert_all_set(vec![L!("normal").into()]);
        assert_all_set(vec![L!("green").into()]);
        assert_all_set(vec![L!("--foreground=normal").into()]);
        assert_all_set(vec![L!("--foreground=green").into()]);
        assert_all_set(vec![L!("--background=normal").into()]);
        assert_all_set(vec![L!("--background=green").into()]);
        assert_all_set(vec![L!("--underline-color=normal").into()]);
        assert_all_set(vec![L!("--underline-color=green").into()]);
        assert_all_set(vec![L!("--italics").into()]);
        assert_all_set(vec![L!("--italics=off").into()]);
        assert_all_set(vec![L!("--reverse").into()]);
        assert_all_set(vec![L!("--reverse=off").into()]);
        assert_all_set(vec![L!("--strikethrough").into()]);
        assert_all_set(vec![L!("--strikethrough=off").into()]);
        assert_all_set(vec![L!("--underline").into()]);
        assert_all_set(vec![L!("--underline=off").into()]);
    }
}
