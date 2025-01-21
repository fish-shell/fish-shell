//! Various mostly unrelated utility functions related to parsing, loading and evaluating fish code.
use crate::ast::{self, Ast, Keyword, Leaf, List, Node, NodeVisitor, Token};
use crate::builtins::shared::builtin_exists;
use crate::common::{
    escape_string, unescape_string, valid_var_name, valid_var_name_char, EscapeFlags,
    EscapeStringStyle, UnescapeFlags, UnescapeStringStyle,
};
use crate::expand::{
    expand_one, expand_to_command_and_args, ExpandFlags, ExpandResultCode, BRACE_BEGIN, BRACE_END,
    BRACE_SEP, INTERNAL_SEPARATOR, VARIABLE_EXPAND, VARIABLE_EXPAND_EMPTY, VARIABLE_EXPAND_SINGLE,
};
use crate::future_feature_flags::{feature_test, FeatureFlag};
use crate::operation_context::OperationContext;
use crate::parse_constants::{
    parse_error_offset_source_start, ParseError, ParseErrorCode, ParseErrorList, ParseKeyword,
    ParseTokenType, ParseTreeFlags, ParserTestErrorBits, PipelinePosition, SourceRange,
    StatementDecoration, ERROR_BAD_VAR_CHAR1, ERROR_BRACKETED_VARIABLE1,
    ERROR_BRACKETED_VARIABLE_QUOTED1, ERROR_NOT_ARGV_AT, ERROR_NOT_ARGV_COUNT, ERROR_NOT_ARGV_STAR,
    ERROR_NOT_PID, ERROR_NOT_STATUS, ERROR_NO_VAR_NAME, INVALID_BREAK_ERR_MSG,
    INVALID_CONTINUE_ERR_MSG, INVALID_PIPELINE_CMD_ERR_MSG, UNKNOWN_BUILTIN_ERR_MSG,
};
use crate::tokenizer::{
    comment_end, is_token_delimiter, quote_end, Tok, TokenType, Tokenizer, TOK_ACCEPT_UNFINISHED,
    TOK_SHOW_COMMENTS,
};
use crate::wchar::prelude::*;
use crate::wcstringutil::count_newlines;
use crate::wcstringutil::truncate;
use crate::wildcard::{ANY_CHAR, ANY_STRING, ANY_STRING_RECURSIVE};
use std::ops::Range;
use std::{iter, ops};

/// Handles slices: the square brackets in an expression like $foo[5..4]
/// Return the length of the slice starting at `in`, or 0 if there is no slice, or None on error.
/// This never accepts incomplete slices.
pub fn parse_util_slice_length(input: &wstr) -> Option<usize> {
    const openc: char = '[';
    const closec: char = ']';
    let mut escaped = false;

    // Check for initial opening [
    let mut chars = input.chars();
    if chars.next() != Some(openc) {
        return Some(0);
    }
    let mut bracket_count = 1;

    let mut pos = 0;
    while let Some(c) = chars.next() {
        pos += 1;
        if !escaped {
            if ['\'', '"'].contains(&c) {
                let oldpos = pos;
                pos = quote_end(input, pos, c)?;
                // We need to advance the iterator as well
                if pos - oldpos > 0 {
                    // nth(0) advances by 1
                    chars.nth(pos - oldpos - 1)?;
                } else {
                    // Quotes aren't over, slice is invalid
                    return None;
                }
            } else if c == openc {
                bracket_count += 1;
            } else if c == closec {
                bracket_count -= 1;
                if bracket_count == 0 {
                    // pos points at the closing ], so add 1.
                    return Some(pos + 1);
                }
            }
        }
        if c == '\\' {
            escaped = !escaped;
        } else {
            escaped = false;
        }
    }
    assert!(bracket_count > 0, "Should have unclosed brackets");

    None
}

#[derive(Debug, Default, Eq, PartialEq)]
pub struct Parentheses {
    range: Range<usize>,
    num_closing: usize,
}

impl Parentheses {
    pub fn start(&self) -> usize {
        self.range.start
    }
    pub fn end(&self) -> usize {
        self.range.end
    }
    pub fn opening(&self) -> Range<usize> {
        self.range.start..self.range.start + 1
    }
    pub fn closing(&self) -> Range<usize> {
        self.range.end - self.num_closing..self.range.end
    }
    pub fn command(&self) -> Range<usize> {
        self.range.start + 1..self.range.end - self.num_closing
    }
}

#[derive(Eq, PartialEq, Debug)]
pub enum MaybeParentheses {
    Error,
    None,
    CommandSubstitution(Parentheses),
}

/// Alternative API. Iterate over command substitutions.
///
/// \param str the string to search for subshells
/// \param inout_cursor_offset On input, the location to begin the search. On output, either the end
/// of the string, or just after the closed-paren.
/// \param out_contents On output, the contents of the command substitution
/// \param out_start On output, the offset of the start of the command substitution (open paren)
/// \param out_end On output, the offset of the end of the command substitution (close paren), or
/// the end of the string if it was incomplete
/// \param accept_incomplete whether to permit missing closing parenthesis
/// \param inout_is_quoted whether the cursor is in a double-quoted context.
/// \param out_has_dollar whether the command substitution has the optional leading $.
/// Return -1 on syntax error, 0 if no subshells exist and 1 on success
#[allow(clippy::too_many_arguments)]
pub fn parse_util_locate_cmdsubst_range(
    s: &wstr,
    inout_cursor_offset: &mut usize,
    accept_incomplete: bool,
    inout_is_quoted: Option<&mut bool>,
    out_has_dollar: Option<&mut bool>,
) -> MaybeParentheses {
    // Nothing to do if the offset is at or past the end of the string.
    if *inout_cursor_offset >= s.len() {
        return MaybeParentheses::None;
    }

    // Defer to the wonky version.
    let ret = parse_util_locate_cmdsub(
        s,
        *inout_cursor_offset,
        accept_incomplete,
        inout_is_quoted,
        out_has_dollar,
    );
    match &ret {
        MaybeParentheses::Error | MaybeParentheses::None => (),
        MaybeParentheses::CommandSubstitution(parens) => {
            *inout_cursor_offset = parens.end();
        }
    }
    ret
}

/// Find the beginning and end of the command substitution under the cursor. If no subshell is
/// found, the entire string is returned. If the current command substitution is not ended, i.e. the
/// closing parenthesis is missing, then the string from the beginning of the substitution to the
/// end of the string is returned.
///
/// \param buff the string to search for subshells
/// \param cursor_pos the position of the cursor
/// \param a the start of the searched string
/// \param b the end of the searched string
pub fn parse_util_cmdsubst_extent(buff: &wstr, cursor: usize) -> ops::Range<usize> {
    // The tightest command substitution found so far.
    let mut result = 0..buff.len();
    let mut pos = 0;
    loop {
        let parens = match parse_util_locate_cmdsub(buff, pos, true, None, None) {
            // No subshell found, all done.
            MaybeParentheses::Error | MaybeParentheses::None => break,
            MaybeParentheses::CommandSubstitution(parens) => parens,
        };

        let command = parens.command();
        if command.start <= cursor && command.end >= cursor {
            // This command substitution surrounds the cursor, so it's a tighter fit.
            result = command;
            // pos is where to begin looking for the next one. But if we reached the end there's no
            // next one.
            if result.start >= result.end {
                break;
            }
            pos = result.start + 1;
        } else if cursor < command.start {
            // This command substitution starts at or after the cursor. Since it was the first
            // command substitution in the string, we're done.
            break;
        } else {
            // This command substitution ends before the cursor. Skip it.
            assert!(command.end < cursor);
            pos = parens.end();
            assert!(pos <= buff.len());
        }
    }
    result
}

fn parse_util_locate_cmdsub(
    input: &wstr,
    cursor: usize,
    allow_incomplete: bool,
    mut inout_is_quoted: Option<&mut bool>,
    mut out_has_dollar: Option<&mut bool>,
) -> MaybeParentheses {
    let input = input.as_char_slice();

    let mut escaped = false;
    let mut is_token_begin = true;
    let mut syntax_error = false;
    let mut paran_count = 0;
    let mut quoted_cmdsubs = vec![];

    let mut pos = cursor;
    let mut last_dollar = None;
    let mut paran_begin = None;
    let mut paran_end = None;
    fn process_opening_quote(
        input: &[char],
        inout_is_quoted: &mut Option<&mut bool>,
        paran_count: i32,
        quoted_cmdsubs: &mut Vec<i32>,
        pos: usize,
        last_dollar: &mut Option<usize>,
        quote: char,
    ) -> Option<usize> {
        let q_end = quote_end(input.into(), pos, quote)?;
        if input[q_end] == '$' {
            *last_dollar = Some(q_end);
            quoted_cmdsubs.push(paran_count);
        }
        // We want to report whether the outermost command substitution between
        // paran_begin..paran_end is quoted.
        if paran_count == 0 {
            inout_is_quoted
                .as_mut()
                .map(|is_quoted| **is_quoted = input[q_end] == '$');
        }
        Some(q_end)
    }

    if inout_is_quoted
        .as_ref()
        .is_some_and(|is_quoted| **is_quoted)
        && !input.is_empty()
    {
        pos = process_opening_quote(
            input,
            &mut inout_is_quoted,
            paran_count,
            &mut quoted_cmdsubs,
            pos,
            &mut last_dollar,
            '"',
        )
        .unwrap_or(input.len());
    }

    while pos < input.len() {
        let c = input[pos];
        if !escaped {
            if ['\'', '"'].contains(&c) {
                match process_opening_quote(
                    input,
                    &mut inout_is_quoted,
                    paran_count,
                    &mut quoted_cmdsubs,
                    pos,
                    &mut last_dollar,
                    c,
                ) {
                    Some(q_end) => pos = q_end,
                    None => break,
                }
            } else if c == '\\' {
                escaped = true;
            } else if c == '#' && is_token_begin {
                pos = comment_end(input.into(), pos) - 1;
            } else if c == '$' {
                last_dollar = Some(pos);
            } else if c == '(' {
                if paran_count == 0 && paran_begin.is_none() {
                    paran_begin = Some(pos);
                    out_has_dollar
                        .as_mut()
                        .map(|has_dollar| **has_dollar = last_dollar == Some(pos.wrapping_sub(1)));
                }

                paran_count += 1;
            } else if c == ')' {
                paran_count -= 1;

                if paran_count == 0 && paran_end.is_none() {
                    paran_end = Some(pos);
                    break;
                }

                if paran_count < 0 {
                    syntax_error = true;
                    break;
                }

                // Check if the ) did complete a quoted command substitution.
                if quoted_cmdsubs.last() == Some(&paran_count) {
                    quoted_cmdsubs.pop();
                    // Quoted command substitutions temporarily close double quotes.
                    // In "foo$(bar)baz$(qux)"
                    // We are here ^
                    // After the ) in a quoted command substitution, we need to act as if
                    // there was an invisible double quote.
                    match quote_end(input.into(), pos, '"') {
                        Some(q_end) => {
                            // Found a valid closing quote.
                            // Stop at $(qux), which is another quoted command substitution.
                            if input[q_end] == '$' {
                                quoted_cmdsubs.push(paran_count);
                            }
                            pos = q_end;
                        }
                        None => break,
                    };
                }
            }
            is_token_begin = is_token_delimiter(c, input.get(pos + 1).copied());
        } else {
            escaped = false;
            is_token_begin = false;
        }
        pos += 1;
    }

    syntax_error |= paran_count < 0;
    syntax_error |= paran_count > 0 && !allow_incomplete;

    if syntax_error {
        return MaybeParentheses::Error;
    }

    let Some(paran_begin) = paran_begin else {
        return MaybeParentheses::None;
    };

    let end = if paran_count != 0 {
        input.len()
    } else {
        paran_end.unwrap() + 1
    };

    let parens = Parentheses {
        range: paran_begin..end,
        num_closing: if paran_count == 0 { 1 } else { 0 },
    };

    MaybeParentheses::CommandSubstitution(parens)
}

/// Find the beginning and end of the process definition under the cursor
///
/// \param buff the string to search for subshells
/// \param cursor_pos the position of the cursor
/// \param a the start of the process
/// \param b the end of the process
/// \param tokens the tokens in the process
pub fn parse_util_process_extent(
    buff: &wstr,
    cursor_pos: usize,
    out_tokens: Option<&mut Vec<Tok>>,
) -> ops::Range<usize> {
    job_or_process_extent(true, buff, cursor_pos, out_tokens)
}

/// Find the beginning and end of the process definition under the cursor
///
/// \param buff the string to search for subshells
/// \param cursor_pos the position of the cursor
/// \param a the start of the process
/// \param b the end of the process
/// \param tokens the tokens in the process
pub fn parse_util_job_extent(
    buff: &wstr,
    cursor_pos: usize,
    out_tokens: Option<&mut Vec<Tok>>,
) -> ops::Range<usize> {
    job_or_process_extent(false, buff, cursor_pos, out_tokens)
}

/// Get the beginning and end of the job or process definition under the cursor.
fn job_or_process_extent(
    process: bool,
    buff: &wstr,
    cursor_pos: usize,
    mut out_tokens: Option<&mut Vec<Tok>>,
) -> ops::Range<usize> {
    let mut finished = false;

    let cmdsub_range = parse_util_cmdsubst_extent(buff, cursor_pos);
    assert!(cursor_pos >= cmdsub_range.start);
    let pos = cursor_pos - cmdsub_range.start;

    let mut result = cmdsub_range.clone();
    for token in Tokenizer::new(
        &buff[cmdsub_range.clone()],
        TOK_ACCEPT_UNFINISHED | TOK_SHOW_COMMENTS,
    ) {
        let tok_begin = token.offset();
        if finished {
            break;
        }
        match token.type_ {
            TokenType::pipe
            | TokenType::end
            | TokenType::background
            | TokenType::andand
            | TokenType::oror
            | TokenType::left_brace
            | TokenType::right_brace
                if (token.type_ != TokenType::pipe || process) =>
            {
                if tok_begin >= pos {
                    finished = true;
                    result.end = cmdsub_range.start + tok_begin;
                } else {
                    // Statement at cursor might start after this token.
                    result.start = cmdsub_range.start + tok_begin + token.length();
                    out_tokens.as_mut().map(|tokens| tokens.clear());
                }
                continue; // Do not add this to tokens
            }
            _ => (),
        }
        out_tokens.as_mut().map(|tokens| tokens.push(token));
    }
    result
}

/// Find the beginning and end of the token under the cursor and the token before the current token.
///
/// \param buff the string to search for subshells
/// \param cursor_pos the position of the cursor
pub fn parse_util_token_extent(buff: &wstr, cursor_pos: usize) -> (Range<usize>, Range<usize>) {
    let cmdsubst_range = parse_util_cmdsubst_extent(buff, cursor_pos);
    let cmdsubst_begin = cmdsubst_range.start;

    // pos is equivalent to cursor_pos within the range of the command substitution {begin, end}.
    let offset_within_cmdsubst = cursor_pos - cmdsubst_range.start;

    let mut cur_begin = cmdsubst_begin + offset_within_cmdsubst;
    let mut cur_end = cur_begin;
    let mut prev_begin = cur_begin;
    let mut prev_end = cur_begin;

    assert!(cmdsubst_begin <= buff.len());
    assert!(cmdsubst_range.end <= buff.len());

    for token in Tokenizer::new(&buff[cmdsubst_range], TOK_ACCEPT_UNFINISHED) {
        let tok_begin = token.offset();
        let mut tok_end = tok_begin;

        // Calculate end of token.
        if token.type_ == TokenType::string {
            tok_end += token.length();
        }

        // Cursor was before beginning of this token, means that the cursor is between two tokens,
        // so we set it to a zero element string and break.
        if tok_begin > offset_within_cmdsubst {
            cur_begin = cmdsubst_begin + offset_within_cmdsubst;
            cur_end = cur_begin;
            break;
        }

        // If cursor is inside the token, this is the token we are looking for. If so, set
        // cur_begin and cur_end and break.
        if token.type_ == TokenType::string && tok_end >= offset_within_cmdsubst {
            cur_begin = cmdsubst_begin + token.offset();
            cur_end = cur_begin + token.length();
            break;
        }

        // Remember previous string token.
        if token.type_ == TokenType::string {
            prev_begin = cmdsubst_begin + token.offset();
            prev_end = prev_begin + token.length();
        }
    }

    assert!(prev_begin <= buff.len());
    assert!(prev_end >= prev_begin);
    assert!(prev_end <= buff.len());
    (cur_begin..cur_end, prev_begin..prev_end)
}

/// Get the line number at the specified character offset.
pub fn parse_util_lineno(s: &wstr, offset: usize) -> usize {
    // Return the line number of position offset, starting with 1.
    if s.is_empty() {
        return 1;
    }

    let end = offset.min(s.len());
    count_newlines(&s[..end]) + 1
}

/// Calculate the line number of the specified cursor position.
pub fn parse_util_get_line_from_offset(s: &wstr, pos: usize) -> isize {
    // Return the line pos is on, or -1 if it's after the end.
    if pos > s.len() {
        return -1;
    }
    count_newlines(&s[..pos]).try_into().unwrap()
}

/// Get the offset of the first character on the specified line.
pub fn parse_util_get_offset_from_line(s: &wstr, line: i32) -> Option<usize> {
    // Return the first position on line X, counting from 0.
    if line < 0 {
        return None;
    }
    if line == 0 {
        return Some(0);
    }

    let mut count = 0;
    for (pos, _) in s.chars().enumerate().filter(|(_, c)| *c == '\n') {
        count += 1;
        if count == line {
            return Some(pos + 1);
        }
    }
    None
}

/// Return the total offset of the buffer for the cursor position nearest to the specified position.
pub fn parse_util_get_offset(s: &wstr, line: i32, line_offset: isize) -> Option<usize> {
    let off = parse_util_get_offset_from_line(s, line)?;
    let off2 = parse_util_get_offset_from_line(s, line + 1).unwrap_or(s.len() + 1);

    let mut line_offset = line_offset as usize;
    if line_offset >= off2 - off - 1 {
        line_offset = off2 - off - 1;
    }

    Some(off + line_offset)
}

/// Return the given string, unescaping wildcard characters but not performing any other character
/// transformation.
pub fn parse_util_unescape_wildcards(s: &wstr) -> WString {
    let mut result = WString::with_capacity(s.len());
    let unesc_qmark = !feature_test(FeatureFlag::qmark_noglob);

    let mut i = 0;
    while i < s.len() {
        let c = s.char_at(i);
        if c == '*' {
            result.push(ANY_STRING);
        } else if c == '?' && unesc_qmark {
            result.push(ANY_CHAR);
        } else if (c == '\\' && s.char_at(i + 1) == '*')
            || (unesc_qmark && c == '\\' && s.char_at(i + 1) == '?')
        {
            result.push(s.char_at(i + 1));
            i += 1;
        } else if c == '\\' && s.char_at(i + 1) == '\\' {
            // Not a wildcard, but ensure the next iteration doesn't see this escaped backslash.
            result.push_utfstr(L!("\\\\"));
            i += 1;
        } else {
            result.push(c);
        }
        i += 1;
    }
    result
}

/// Return if the given string contains wildcard characters.
pub fn parse_util_contains_wildcards(s: &wstr) -> bool {
    let unesc_qmark = !feature_test(FeatureFlag::qmark_noglob);

    let mut i = 0;
    while i < s.len() {
        let c = s.as_char_slice()[i];
        if c == '*' {
            return true;
        } else if unesc_qmark && c == '?' {
            return true;
        } else if c == '\\' {
            if s.char_at(i + 1) == '*' {
                i += 1;
            } else if unesc_qmark && s.char_at(i + 1) == '?' {
                i += 1;
            } else if s.char_at(i + 1) == '\\' {
                // Not a wildcard, but ensure the next iteration doesn't see this escaped backslash.
                i += 1;
            }
        }
        i += 1;
    }
    false
}

/// Escape any wildcard characters in the given string. e.g. convert
/// "a*b" to "a\*b".
pub fn parse_util_escape_wildcards(s: &wstr) -> WString {
    let mut result = WString::with_capacity(s.len());
    let unesc_qmark = !feature_test(FeatureFlag::qmark_noglob);

    for c in s.chars() {
        if c == '*' {
            result.push_str("\\*");
        } else if unesc_qmark && c == '?' {
            result.push_str("\\?");
        } else if c == '\\' {
            result.push_str("\\\\");
        } else {
            result.push(c);
        }
    }
    result
}

/// Checks if the specified string is a help option.
pub fn parse_util_argument_is_help(s: &wstr) -> bool {
    [L!("-h"), L!("--help")].contains(&s)
}

/// Returns true if the specified command is a builtin that may not be used in a pipeline.
fn parser_is_pipe_forbidden(word: &wstr) -> bool {
    [
        L!("exec"),
        L!("case"),
        L!("break"),
        L!("return"),
        L!("continue"),
    ]
    .contains(&word)
}

// Return a pointer to the first argument node of an argument_or_redirection_list_t, or nullptr if
// there are no arguments.
fn get_first_arg(list: &ast::ArgumentOrRedirectionList) -> Option<&ast::Argument> {
    for v in list.iter() {
        if v.is_argument() {
            return Some(v.argument());
        }
    }
    None
}

/// Given a wide character immediately after a dollar sign, return the appropriate error message.
/// For example, if wc is @, then the variable name was $@ and we suggest $argv.
fn error_for_character(c: char) -> WString {
    match c {
        '?' => wgettext!(ERROR_NOT_STATUS).to_owned(),
        '#' => wgettext!(ERROR_NOT_ARGV_COUNT).to_owned(),
        '@' => wgettext!(ERROR_NOT_ARGV_AT).to_owned(),
        '*' => wgettext!(ERROR_NOT_ARGV_STAR).to_owned(),
        _ if [
            '$',
            VARIABLE_EXPAND,
            VARIABLE_EXPAND_SINGLE,
            VARIABLE_EXPAND_EMPTY,
        ]
        .contains(&c) =>
        {
            wgettext!(ERROR_NOT_PID).to_owned()
        }
        _ if [BRACE_END, '}', ',', BRACE_SEP].contains(&c) => {
            wgettext!(ERROR_NO_VAR_NAME).to_owned()
        }
        _ => wgettext_fmt!(ERROR_BAD_VAR_CHAR1, c),
    }
}

/// Attempts to escape the string 'cmd' using the given quote type, as determined by the quote
/// character. The quote can be a single quote or double quote, or L'\0' to indicate no quoting (and
/// thus escaping should be with backslashes). Optionally do not escape tildes.
pub fn parse_util_escape_string_with_quote(
    cmd: &wstr,
    quote: Option<char>,
    escape_flags: EscapeFlags,
) -> WString {
    let Some(quote) = quote else {
        return escape_string(cmd, EscapeStringStyle::Script(escape_flags));
    };
    // Here we are going to escape a string with quotes.
    // A few characters cannot be represented inside quotes, e.g. newlines. In that case,
    // terminate the quote and then re-enter it.
    let mut result = WString::new();
    result.reserve(cmd.len());
    for c in cmd.chars() {
        match c {
            '\n' => {
                for c in [quote, '\\', 'n', quote] {
                    result.push(c);
                }
            }
            '\t' => {
                for c in [quote, '\\', 't', quote] {
                    result.push(c);
                }
            }
            '\x08' => {
                for c in [quote, '\\', 'b', quote] {
                    result.push(c);
                }
            }
            '\r' => {
                for c in [quote, '\\', 'r', quote] {
                    result.push(c);
                }
            }
            '\\' => {
                result.push_str("\\\\");
            }
            '$' => {
                if quote == '"' {
                    result.push('\\');
                }
                result.push('$');
            }
            _ => {
                if c == quote {
                    result.push('\\');
                }
                result.push(c);
            }
        }
    }
    result
}

/// Given a string, parse it as fish code and then return the indents. The return value has the same
/// size as the string.
pub fn parse_util_compute_indents(src: &wstr) -> Vec<i32> {
    compute_indents(src, 0)
}

fn compute_indents(src: &wstr, initial_indent: i32) -> Vec<i32> {
    // Make a vector the same size as the input string, which contains the indents. Initialize them
    // to 0.
    let mut indents = vec![0; src.len()];

    // Simple trick: if our source does not contain a newline, then all indents are 0.
    if !src.chars().any(|c| c == '\n') {
        return indents;
    }

    // Parse the string. We pass continue_after_error to produce a forest; the trailing indent of
    // the last node we visited becomes the input indent of the next. I.e. in the case of 'switch
    // foo ; cas', we get an invalid parse tree (since 'cas' is not valid) but we indent it as if it
    // were a case item list.
    let ast = Ast::parse(
        src,
        ParseTreeFlags::CONTINUE_AFTER_ERROR
            | ParseTreeFlags::INCLUDE_COMMENTS
            | ParseTreeFlags::ACCEPT_INCOMPLETE_TOKENS
            | ParseTreeFlags::LEAVE_UNTERMINATED,
        None,
    );
    {
        let mut iv = IndentVisitor::new(src, &mut indents, initial_indent);
        iv.visit(ast.top());
        iv.record_line_continuations_until(iv.indents.len());
        iv.indents[iv.last_leaf_end..].fill(iv.last_indent);

        // All newlines now get the *next* indent.
        // For example, in this code:
        //    if true
        //       stuff
        // the newline "belongs" to the if statement as it ends its job.
        // But when rendered, it visually belongs to the job list.

        let mut idx = src.len();
        let mut next_indent = iv.last_indent;
        let src = src.as_char_slice();
        while idx != 0 {
            idx -= 1;
            if src[idx] == '\n' {
                let empty_middle_line = src.get(idx + 1) == Some(&'\n');
                let is_trailing_unclosed = idx == src.len() - 1 && iv.unclosed;
                if !empty_middle_line && !is_trailing_unclosed {
                    iv.indents[idx] = next_indent;
                }
            } else {
                next_indent = iv.indents[idx];
            }
        }
        // Add an extra level of indentation to continuation lines.
        for mut idx in iv.line_continuations {
            loop {
                indents[idx] = indents[idx].wrapping_add(1);
                idx += 1;
                if idx == src.len() || src[idx] == '\n' {
                    break;
                }
            }
        }
    }

    indents
}

// The number of spaces per indent isn't supposed to be configurable.
// See discussion at https://github.com/fish-shell/fish-shell/pull/6790
pub const SPACES_PER_INDENT: usize = 4;

pub fn apply_indents(src: &wstr, indents: &[i32]) -> WString {
    let mut indented = WString::new();
    for (i, c) in src.chars().enumerate() {
        indented.push(c);
        if c != '\n' || i + 1 == src.len() {
            continue;
        }
        indented.extend(
            std::iter::repeat(' ').take(SPACES_PER_INDENT * usize::try_from(indents[i]).unwrap()),
        );
    }
    indented
}

// Visit all of our nodes. When we get a job_list or case_item_list, increment indent while
// visiting its children.
struct IndentVisitor<'a> {
    // companion: Pin<&'a mut indent_visitor_t>,
    // The one-past-the-last index of the most recently encountered leaf node.
    // We use this to populate the indents even if there's no tokens in the range.
    last_leaf_end: usize,

    // The last indent which we assigned.
    last_indent: i32,

    // Whether we have an unfinished quote or command substitution.
    unclosed: bool,

    // The source we are indenting.
    src: &'a wstr,

    // List of indents, which we populate.
    indents: &'a mut Vec<i32>,

    // Initialize our starting indent to -1, as our top-level node is a job list which
    // will immediately increment it.
    indent: i32,

    // List of locations of escaped newline characters.
    line_continuations: Vec<usize>,
}
impl<'a> IndentVisitor<'a> {
    fn new(src: &'a wstr, indents: &'a mut Vec<i32>, initial_indent: i32) -> Self {
        Self {
            last_leaf_end: 0,
            last_indent: initial_indent - 1,
            unclosed: false,
            src,
            indents,
            indent: initial_indent - 1,
            line_continuations: vec![],
        }
    }
    /// Return whether a maybe_newlines node contains at least one newline.
    fn has_newline(&self, nls: &ast::MaybeNewlines) -> bool {
        nls.source(self.src).chars().any(|c| c == '\n')
    }
    fn record_line_continuations_until(&mut self, offset: usize) {
        let gap_text = &self.src[self.last_leaf_end..offset];
        let gap_text = gap_text.as_char_slice();
        let Some(escaped_nl) = gap_text.windows(2).position(|w| *w == ['\\', '\n']) else {
            return;
        };
        if gap_text[..escaped_nl].contains(&'#') {
            return;
        }
        let mut newline = escaped_nl + 1;
        // The gap text might contain multiple newlines if there are multiple lines that
        // don't contain an AST node, for example, comment lines, or lines containing only
        // the escaped newline.
        loop {
            self.line_continuations.push(self.last_leaf_end + newline);
            match gap_text[newline + 1..].iter().position(|c| *c == '\n') {
                Some(nextnl) => newline = newline + 1 + nextnl,
                None => break,
            }
        }
    }

    fn indent_leaf(&mut self, range: SourceRange) {
        let node_src = &self.src[range.start()..range.end()];
        // Common case optimization.
        if node_src.contains('(') /*)*/ && !node_src.contains('\n') {
            self.indents[range.start()..range.end()].fill(self.indent);
            return;
        }

        let mut done = range.start();
        let mut cursor = 0;
        let mut is_double_quoted = false;
        let mut was_double_quoted;
        loop {
            was_double_quoted = is_double_quoted;
            let parens = match parse_util_locate_cmdsubst_range(
                node_src,
                &mut cursor,
                /*accept_incomplete=*/ true,
                Some(&mut is_double_quoted),
                None,
            ) {
                MaybeParentheses::Error => break,
                MaybeParentheses::None => {
                    break;
                }
                MaybeParentheses::CommandSubstitution(parens) => parens,
            };

            let command = parens.command();
            self.indent_string_part(done..range.start() + command.start, was_double_quoted);
            let cmdsub_contents = &node_src[command.clone()];
            let indents = compute_indents(cmdsub_contents, self.indent + 1);
            self.indents[range.start() + command.start..range.start() + command.end]
                .copy_from_slice(&indents);

            done = range.start() + command.end;
            if parens.closing().is_empty() {
                self.unclosed = true;
            }
        }
        self.indent_string_part(done..range.end(), was_double_quoted);
    }

    fn indent_string_part(&mut self, range: Range<usize>, is_double_quoted: bool) {
        let mut start = range.start;
        let mut quoted = false;
        if is_double_quoted {
            match quote_end(self.src, range.start, '"') {
                Some(q_end) => {
                    // We may be (in) a multi-line string, so don't indent.
                    start = q_end + 1;
                }
                None => quoted = true,
            }
        }
        let mut done = start;
        if !quoted {
            let part = &self.src[done..range.end];
            let mut callback = |offset| {
                if !quoted {
                    // Quote open event. Indent unquoted part, including the opening quote.
                    self.indents[done..start + offset + 1].fill(self.indent);
                    done = start + offset + 1;
                } else {
                    // Quote close. Don't indent, in case it's a multiline string.
                    // Mark the first line as indented but only to make tests look prettier.
                    let first_line_length = self.src[start..start + offset]
                        .chars()
                        .take_while(|&c| c != '\n')
                        .count();
                    self.indents[start..start + first_line_length].fill(self.indent);
                    done = start + offset;
                }
                quoted = !quoted;
            };
            for _token in Tokenizer::with_quote_events(part, TOK_ACCEPT_UNFINISHED, &mut callback) {
            }
        }
        if !quoted {
            self.indents[done..range.end].fill(self.indent);
        } else {
            self.unclosed = true;
        }
    }
}
impl<'a> NodeVisitor<'a> for IndentVisitor<'a> {
    // Default implementation is to just visit children.
    fn visit(&mut self, node: &'a dyn Node) {
        let mut inc = 0;
        let mut dec = 0;
        use ast::{Category, Type};
        match node.typ() {
            Type::job_list | Type::andor_job_list => {
                // Job lists are never unwound.
                inc = 1;
                dec = 1;
            }

            // Increment indents for conditions in headers (#1665).
            Type::job_conjunction => {
                if [Type::while_header, Type::if_clause].contains(&node.parent().unwrap().typ()) {
                    inc = 1;
                    dec = 1;
                }
            }

            // Increment indents for job_continuation_t if it contains a newline.
            // This is a bit of a hack - it indents cases like:
            //    cmd1 |
            //    ....cmd2
            // but avoids "double indenting" if there's no newline:
            //   cmd1 | while cmd2
            //   ....cmd3
            //   end
            // See #7252.
            Type::job_continuation => {
                if self.has_newline(&node.as_job_continuation().unwrap().newlines) {
                    inc = 1;
                    dec = 1;
                }
            }

            // Likewise for && and ||.
            Type::job_conjunction_continuation => {
                if self.has_newline(&node.as_job_conjunction_continuation().unwrap().newlines) {
                    inc = 1;
                    dec = 1;
                }
            }

            Type::case_item_list => {
                // Here's a hack. Consider:
                // switch abc
                //    cas
                //
                // fish will see that 'cas' is not valid inside a switch statement because it is
                // not "case". It will then unwind back to the top level job list, producing a
                // parse tree like:
                //
                //   job_list
                //      switch_job
                //         <err>
                //      normal_job
                //         cas
                //
                // And so we will think that the 'cas' job is at the same level as the switch.
                // To address this, if we see that the switch statement was not closed, do not
                // decrement the indent afterwards.
                inc = 1;
                let switchs = node.parent().unwrap().as_switch_statement().unwrap();
                dec = if switchs.end.has_source() { 1 } else { 0 };
            }
            Type::token_base => {
                let token_type = node.as_token().unwrap().token_type();
                let parent_type = node.parent().unwrap().typ();
                if parent_type == Type::begin_header && token_type == ParseTokenType::end {
                    // The newline after "begin" is optional, so it is part of the header.
                    // The header is not in the indented block, so indent the newline here.
                    if node.source(self.src) == "\n" {
                        inc = 1;
                        dec = 1;
                    }
                }
                // if token_type == ParseTokenType::right_brace && parent_type == Type::brace_statement
                // {
                //     inc = 1;
                //     dec = 1;
                // }
            }
            _ => (),
        }

        let range = node.source_range();
        if range.length() > 0 && node.category() == Category::leaf {
            self.record_line_continuations_until(range.start());
            self.indents[self.last_leaf_end..range.start()].fill(self.last_indent);
        }

        self.indent += inc;

        // If we increased the indentation, apply it to the remainder of the string, even if the
        // list is empty. For example (where _ represents the cursor):
        //
        //    if foo
        //       _
        //
        // we want to indent the newline.
        if inc != 0 {
            self.last_indent = self.indent;
        }

        // If this is a leaf node, apply the current indentation.
        if node.category() == Category::leaf && range.length() != 0 {
            let leading_spaces = self.src[..range.start()]
                .chars()
                .rev()
                .take_while(|&c| c == ' ')
                .count();
            self.indents[range.start() - leading_spaces..range.start()].fill(self.indent);
            self.indent_leaf(range);
            self.last_leaf_end = range.end();
            self.last_indent = self.indent;
        }

        node.accept(self, false);
        self.indent -= dec;
    }
}

/// Given a string, detect parse errors in it. If allow_incomplete is set, then if the string is
/// incomplete (e.g. an unclosed quote), an error is not returned and the ParserTestErrorBits::INCOMPLETE bit
/// is set in the return value. If allow_incomplete is not set, then incomplete strings result in an
/// error.
pub fn parse_util_detect_errors(
    buff_src: &wstr,
    mut out_errors: Option<&mut ParseErrorList>,
    allow_incomplete: bool, /*=false*/
) -> Result<(), ParserTestErrorBits> {
    // Whether there's an unclosed quote or subshell, and therefore unfinished. This is only set if
    // allow_incomplete is set.
    let mut has_unclosed_quote_or_subshell = false;

    let parse_flags = if allow_incomplete {
        ParseTreeFlags::LEAVE_UNTERMINATED
    } else {
        ParseTreeFlags::empty()
    };

    // Parse the input string into an ast. Some errors are detected here.
    let mut parse_errors = ParseErrorList::new();
    let ast = Ast::parse(buff_src, parse_flags, Some(&mut parse_errors));
    if allow_incomplete {
        // Issue #1238: If the only error was unterminated quote, then consider this to have parsed
        // successfully.
        parse_errors.retain(|parse_error| {
            if [
                ParseErrorCode::tokenizer_unterminated_quote,
                ParseErrorCode::tokenizer_unterminated_subshell,
            ]
            .contains(&parse_error.code)
            {
                // Remove this error, since we don't consider it a real error.
                has_unclosed_quote_or_subshell = true;
                false
            } else {
                true
            }
        });
    }

    // has_unclosed_quote_or_subshell may only be set if allow_incomplete is true.
    assert!(!has_unclosed_quote_or_subshell || allow_incomplete);
    if has_unclosed_quote_or_subshell {
        // We do not bother to validate the rest of the tree in this case.
        return Err(ParserTestErrorBits::INCOMPLETE);
    }

    // Early parse error, stop here.
    if !parse_errors.is_empty() {
        if let Some(errors) = out_errors.as_mut() {
            errors.extend(parse_errors);
        }
        return Err(ParserTestErrorBits::ERROR);
    }

    // Defer to the tree-walking version.
    parse_util_detect_errors_in_ast(&ast, buff_src, out_errors)
}

/// Like parse_util_detect_errors but accepts an already-parsed ast.
/// The top of the ast is assumed to be a job list.
pub fn parse_util_detect_errors_in_ast(
    ast: &Ast,
    buff_src: &wstr,
    mut out_errors: Option<&mut ParseErrorList>,
) -> Result<(), ParserTestErrorBits> {
    let mut res = ParserTestErrorBits::default();

    // Whether we encountered a parse error.
    let mut errored = false;

    // Whether we encountered an unclosed block. We detect this via an 'end_command' block without
    // source.
    let mut has_unclosed_block = false;

    // Whether we encounter a missing statement, i.e. a newline after a pipe. This is found by
    // detecting job_continuations that have source for pipes but not the statement.
    let mut has_unclosed_pipe = false;

    // Whether we encounter a missing job, i.e. a newline after && or ||. This is found by
    // detecting job_conjunction_continuations that have source for && or || but not the job.
    let mut has_unclosed_conjunction = false;

    // Expand all commands.
    // Verify 'or' and 'and' not used inside pipelines.
    // Verify return only within a function.
    // Verify no variable expansions.

    for node in ast::Traversal::new(ast.top()) {
        if let Some(jc) = node.as_job_continuation() {
            // Somewhat clumsy way of checking for a statement without source in a pipeline.
            // See if our pipe has source but our statement does not.
            if jc.pipe.has_source() && jc.statement.try_source_range().is_none() {
                has_unclosed_pipe = true;
            }
        } else if let Some(job_conjunction) = node.as_job_conjunction() {
            errored |= detect_errors_in_job_conjunction(job_conjunction, &mut out_errors);
        } else if let Some(jcc) = node.as_job_conjunction_continuation() {
            // Somewhat clumsy way of checking for a job without source in a conjunction.
            // See if our conjunction operator (&& or ||) has source but our job does not.
            if jcc.conjunction.has_source() && jcc.job.try_source_range().is_none() {
                has_unclosed_conjunction = true;
            }
        } else if let Some(arg) = node.as_argument() {
            let arg_src = arg.source(buff_src);
            res |= parse_util_detect_errors_in_argument(arg, arg_src, &mut out_errors)
                .err()
                .unwrap_or_default();
        } else if let Some(job) = node.as_job_pipeline() {
            // Disallow background in the following cases:
            //
            // foo & ; and bar
            // foo & ; or bar
            // if foo & ; end
            // while foo & ; end
            // If it's not a background job, nothing to do.
            if job.bg.is_some() {
                errored |= detect_errors_in_backgrounded_job(job, &mut out_errors);
            }
        } else if let Some(stmt) = node.as_decorated_statement() {
            errored |= detect_errors_in_decorated_statement(buff_src, stmt, &mut out_errors);
        } else if let Some(block) = node.as_block_statement() {
            // If our 'end' had no source, we are unsourced.
            if !block.end.has_source() {
                has_unclosed_block = true;
            }
            errored |=
                detect_errors_in_block_redirection_list(&block.args_or_redirs, &mut out_errors);
        } else if let Some(brace_statement) = node.as_brace_statement() {
            // If our closing brace had no source, we are unsourced.
            if !brace_statement.right_brace.has_source() {
                has_unclosed_block = true;
            }
            errored |= detect_errors_in_block_redirection_list(
                &brace_statement.args_or_redirs,
                &mut out_errors,
            );
        } else if let Some(ifs) = node.as_if_statement() {
            // If our 'end' had no source, we are unsourced.
            if !ifs.end.has_source() {
                has_unclosed_block = true;
            }
            errored |=
                detect_errors_in_block_redirection_list(&ifs.args_or_redirs, &mut out_errors);
        } else if let Some(switchs) = node.as_switch_statement() {
            // If our 'end' had no source, we are unsourced.
            if !switchs.end.has_source() {
                has_unclosed_block = true;
            }
            errored |=
                detect_errors_in_block_redirection_list(&switchs.args_or_redirs, &mut out_errors);
        }
    }

    if errored {
        res |= ParserTestErrorBits::ERROR;
    }

    if has_unclosed_block || has_unclosed_pipe || has_unclosed_conjunction {
        res |= ParserTestErrorBits::INCOMPLETE;
    }
    if res == ParserTestErrorBits::default() {
        Ok(())
    } else {
        Err(res)
    }
}

/// Detect errors in the specified string when parsed as an argument list. Returns the text of an
/// error, or none if no error occurred.
pub fn parse_util_detect_errors_in_argument_list(
    arg_list_src: &wstr,
    prefix: &wstr,
) -> Result<(), WString> {
    // Helper to return a description of the first error.
    let get_error_text = |errors: &ParseErrorList| {
        assert!(!errors.is_empty(), "Expected an error");
        Err(errors[0].describe_with_prefix(
            arg_list_src,
            prefix,
            false, /* not interactive */
            false, /* don't skip caret */
        ))
    };

    // Parse the string as a freestanding argument list.
    let mut errors = ParseErrorList::new();
    let ast = Ast::parse_argument_list(arg_list_src, ParseTreeFlags::empty(), Some(&mut errors));
    if !errors.is_empty() {
        return get_error_text(&errors);
    }

    // Get the root argument list and extract arguments from it.
    // Test each of these.
    let args = &ast.top().as_freestanding_argument_list().unwrap().arguments;
    for arg in args.iter() {
        let arg_src = arg.source(arg_list_src);
        if parse_util_detect_errors_in_argument(arg, arg_src, &mut Some(&mut errors)).is_err() {
            return get_error_text(&errors);
        }
    }
    Ok(())
}

/// Append a syntax error to the given error list.
macro_rules! append_syntax_error {
    (
        $errors:expr, $source_location:expr,
        $source_length:expr, $fmt:expr
        $(, $arg:expr)* $(,)?
    ) => {
        {
            append_syntax_error_formatted!(
                $errors, $source_location, $source_length,
                wgettext_maybe_fmt!($fmt $(, $arg)*))
        }
    }
}

macro_rules! append_syntax_error_formatted {
    (
        $errors:expr, $source_location:expr,
        $source_length:expr, $text:expr
    ) => {{
        if let Some(ref mut errors) = $errors {
            let mut error = ParseError::default();
            error.source_start = $source_location;
            error.source_length = $source_length;
            error.code = ParseErrorCode::syntax;
            error.text = $text;
            errors.push(error);
        }
        true
    }};
}

/// Test if this argument contains any errors. Detected errors include syntax errors in command
/// substitutions, improperly escaped characters and improper use of the variable expansion
/// operator.
pub fn parse_util_detect_errors_in_argument(
    arg: &ast::Argument,
    arg_src: &wstr,
    out_errors: &mut Option<&mut ParseErrorList>,
) -> Result<(), ParserTestErrorBits> {
    let Some(source_range) = arg.try_source_range() else {
        return Ok(());
    };

    let source_start = source_range.start();
    let mut err = ParserTestErrorBits::default();

    let check_subtoken =
        |begin: usize, end: usize, out_errors: &mut Option<&mut ParseErrorList>| {
            let Some(unesc) = unescape_string(
                &arg_src[begin..end],
                UnescapeStringStyle::Script(UnescapeFlags::SPECIAL),
            ) else {
                if out_errors.is_some() {
                    let src = arg_src.as_char_slice();
                    if src.len() == 2
                        && src[0] == '\\'
                        && (src[1] == 'c'
                            || src[1].to_lowercase().eq(['u'])
                            || src[1].to_lowercase().eq(['x']))
                    {
                        append_syntax_error!(
                            out_errors,
                            source_start + begin,
                            end - begin,
                            "Incomplete escape sequence '%ls'",
                            arg_src
                        );
                        return ParserTestErrorBits::ERROR;
                    }
                    append_syntax_error!(
                        out_errors,
                        source_start + begin,
                        end - begin,
                        "Invalid token '%ls'",
                        arg_src
                    );
                }
                return ParserTestErrorBits::ERROR;
            };

            let mut err = ParserTestErrorBits::default();
            // Check for invalid variable expansions.
            let unesc = unesc.as_char_slice();
            for (idx, c) in unesc.iter().enumerate() {
                if ![VARIABLE_EXPAND, VARIABLE_EXPAND_SINGLE].contains(c) {
                    continue;
                }
                let next_char = unesc.get(idx + 1).copied().unwrap_or('\0');
                if ![VARIABLE_EXPAND, VARIABLE_EXPAND_SINGLE, '('].contains(&next_char)
                    && !valid_var_name_char(next_char)
                {
                    err = ParserTestErrorBits::ERROR;
                    if let Some(ref mut out_errors) = out_errors {
                        let mut first_dollar = idx;
                        while first_dollar > 0
                            && [VARIABLE_EXPAND, VARIABLE_EXPAND_SINGLE]
                                .contains(&unesc[first_dollar - 1])
                        {
                            first_dollar -= 1;
                        }
                        parse_util_expand_variable_error(
                            unesc.into(),
                            source_start,
                            first_dollar,
                            out_errors,
                        );
                    }
                }
            }

            err
        };

    let mut cursor = 0;
    let mut checked = 0;

    let mut do_loop = true;
    let mut is_quoted = false;
    while do_loop {
        let mut has_dollar = false;
        match parse_util_locate_cmdsubst_range(
            arg_src,
            &mut cursor,
            false,
            Some(&mut is_quoted),
            Some(&mut has_dollar),
        ) {
            MaybeParentheses::Error => {
                err |= ParserTestErrorBits::ERROR;
                append_syntax_error!(out_errors, source_start, 1, "Mismatched parenthesis");
                return Err(err);
            }
            MaybeParentheses::None => {
                do_loop = false;
            }
            MaybeParentheses::CommandSubstitution(parens) => {
                err |= check_subtoken(
                    checked,
                    parens.start() - if has_dollar { 1 } else { 0 },
                    out_errors,
                );
                let mut subst_errors = ParseErrorList::new();
                if let Err(subst_err) = parse_util_detect_errors(
                    &arg_src[parens.command()],
                    Some(&mut subst_errors),
                    false,
                ) {
                    err |= subst_err;
                }

                // Our command substitution produced error offsets relative to its source. Tweak the
                // offsets of the errors in the command substitution to account for both its offset
                // within the string, and the offset of the node.
                let error_offset = parens.start() + 1 + source_start;
                parse_error_offset_source_start(&mut subst_errors, error_offset);
                if let Some(ref mut out_errors) = out_errors {
                    out_errors.extend(subst_errors);
                }

                checked = parens.end();
            }
        }
    }

    err |= check_subtoken(checked, arg_src.len(), out_errors);

    if err.is_empty() {
        Ok(())
    } else {
        Err(err)
    }
}

fn detect_errors_in_job_conjunction(
    job_conjunction: &ast::JobConjunction,
    parse_errors: &mut Option<&mut ParseErrorList>,
) -> bool {
    // Disallow background immediately before conjunction continuations. For example:
    // foo & && bar
    // foo & || baz
    let continuations = &job_conjunction.continuations;
    let jobs = iter::once(&job_conjunction.job)
        .chain(continuations.iter().map(|continuation| &continuation.job));
    for (job, continuation) in jobs.zip(continuations.iter()) {
        if job.bg.is_some() {
            let conjunction = &continuation.conjunction;
            return append_syntax_error!(
                parse_errors,
                conjunction.source_range().start(),
                conjunction.source_range().length(),
                BOOL_AFTER_BACKGROUND_ERROR_MSG,
                if conjunction.token_type() == ParseTokenType::andand {
                    L!("&&")
                } else {
                    L!("||")
                }
            );
        }
    }

    false
}

/// Given that the job given by node should be backgrounded, return true if we detect any errors.
fn detect_errors_in_backgrounded_job(
    job: &ast::JobPipeline,
    parse_errors: &mut Option<&mut ParseErrorList>,
) -> bool {
    let Some(source_range) = job.try_source_range() else {
        return false;
    };

    let mut errored = false;
    // Disallow background in the following cases:
    // foo & ; and bar
    // foo & ; or bar
    // if foo & ; end
    // while foo & ; end
    let Some(job_conj) = job.parent().unwrap().as_job_conjunction() else {
        return false;
    };

    if job_conj.parent().unwrap().as_if_clause().is_some()
        || job_conj.parent().unwrap().as_while_header().is_some()
    {
        errored = append_syntax_error!(
            parse_errors,
            source_range.start(),
            source_range.length(),
            BACKGROUND_IN_CONDITIONAL_ERROR_MSG
        );
    } else if let Some(jlist) = job_conj.parent().unwrap().as_job_list() {
        // This isn't very complete, e.g. we don't catch 'foo & ; not and bar'.
        // Find the index of ourselves in the job list.
        let index = jlist
            .iter()
            .position(|job| job.pointer_eq(job_conj))
            .expect("Should have found the job in the list");

        // Try getting the next job and check its decorator.
        if let Some(next) = jlist.get(index + 1) {
            if let Some(deco) = &next.decorator {
                assert!(
                    [ParseKeyword::kw_and, ParseKeyword::kw_or].contains(&deco.keyword()),
                    "Unexpected decorator keyword"
                );
                let deco_name = if deco.keyword() == ParseKeyword::kw_and {
                    L!("and")
                } else {
                    L!("or")
                };
                errored = append_syntax_error!(
                    parse_errors,
                    deco.source_range().start(),
                    deco.source_range().length(),
                    BOOL_AFTER_BACKGROUND_ERROR_MSG,
                    deco_name
                );
            }
        }
    }
    errored
}

/// Given a source buffer `buff_src` and decorated statement `dst` within it, return true if there
/// is an error and false if not. `storage` may be used to reduce allocations.
fn detect_errors_in_decorated_statement(
    buff_src: &wstr,
    dst: &ast::DecoratedStatement,
    parse_errors: &mut Option<&mut ParseErrorList>,
) -> bool {
    let mut errored = false;
    let source_start = dst.source_range().start();
    let source_length = dst.source_range().length();
    let decoration = dst.decoration();

    // Determine if the first argument is help.
    let mut first_arg_is_help = false;
    if let Some(arg) = get_first_arg(&dst.args_or_redirs) {
        let arg_src = arg.source(buff_src);
        first_arg_is_help = parse_util_argument_is_help(arg_src);
    }

    // Get the statement we are part of.
    let st = dst.parent().unwrap().as_statement().unwrap();

    // Walk up to the job.
    let mut job = None;
    let mut cursor = dst.parent();
    while job.is_none() {
        let c = cursor.expect("Reached root without finding a job");
        job = c.as_job_pipeline();
        cursor = c.parent();
    }
    let job = job.expect("Should have found the job");

    // Check our pipeline position.
    let pipe_pos = if job.continuation.is_empty() {
        PipelinePosition::none
    } else if job.statement.pointer_eq(st) {
        PipelinePosition::first
    } else {
        PipelinePosition::subsequent
    };

    // Check that we don't try to pipe through exec.
    let is_in_pipeline = pipe_pos != PipelinePosition::none;
    if is_in_pipeline && decoration == StatementDecoration::exec {
        errored = append_syntax_error!(
            parse_errors,
            source_start,
            source_length,
            INVALID_PIPELINE_CMD_ERR_MSG,
            "exec"
        );
    }

    // This is a somewhat stale check that 'and' and 'or' are not in pipelines, except at the
    // beginning. We can't disallow them as commands entirely because we need to support 'and
    // --help', etc.
    if pipe_pos == PipelinePosition::subsequent {
        // We only reject it if we have no decoration.
        // `echo foo | command time something`
        // is entirely fair and valid.
        // Other current decorations like "exec"
        // are already forbidden.
        if dst.decoration() == StatementDecoration::none {
            // check if our command is 'and' or 'or'. This is very clumsy; we don't catch e.g. quoted
            // commands.
            let command = dst.command.source(buff_src);
            if [L!("and"), L!("or")].contains(&command) {
                errored = append_syntax_error!(
                    parse_errors,
                    source_start,
                    source_length,
                    INVALID_PIPELINE_CMD_ERR_MSG,
                    command
                );
            }

            // Similarly for time (#8841).
            if command == "time" {
                errored = append_syntax_error!(
                    parse_errors,
                    source_start,
                    source_length,
                    TIME_IN_PIPELINE_ERR_MSG
                );
            }
        }
    }

    // $status specifically is invalid as a command,
    // to avoid people trying `if $status`.
    // We see this surprisingly regularly.
    let com = dst.command.source(buff_src);
    if com == "$status" {
        errored = append_syntax_error!(
            parse_errors,
            source_start,
            source_length,
            "$status is not valid as a command. See `help conditions`"
        );
    }

    let unexp_command = com;
    if !unexp_command.is_empty() {
        // Check that we can expand the command.
        // Make a new error list so we can fix the offset for just those, then append later.
        let mut new_errors = ParseErrorList::new();
        let mut command = WString::new();
        if matches!(
            expand_to_command_and_args(
                unexp_command,
                &OperationContext::empty(),
                &mut command,
                None,
                Some(&mut new_errors),
                true, /* skip wildcards */
            )
            .result,
            ExpandResultCode::error | ExpandResultCode::overflow
        ) {
            errored = true;
        }

        // Check that pipes are sound.
        if !errored && parser_is_pipe_forbidden(&command) && is_in_pipeline {
            errored = append_syntax_error!(
                parse_errors,
                source_start,
                source_length,
                INVALID_PIPELINE_CMD_ERR_MSG,
                command
            );
        }

        // Check that we don't break or continue from outside a loop.
        if !errored && [L!("break"), L!("continue")].contains(&&command[..]) && !first_arg_is_help {
            // Walk up until we hit a 'for' or 'while' loop. If we hit a function first,
            // stop the search; we can't break an outer loop from inside a function.
            // This is a little funny because we can't tell if it's a 'for' or 'while'
            // loop from the ancestor alone; we need the header. That is, we hit a
            // block_statement, and have to check its header.
            let mut found_loop = false;
            let mut ancestor: Option<&dyn Node> = Some(dst);
            while let Some(anc) = ancestor {
                if let Some(block) = anc.as_block_statement() {
                    if [ast::Type::for_header, ast::Type::while_header]
                        .contains(&block.header.typ())
                    {
                        // This is a loop header, so we can break or continue.
                        found_loop = true;
                        break;
                    } else if block.header.typ() == ast::Type::function_header {
                        // This is a function header, so we cannot break or
                        // continue. We stop our search here.
                        found_loop = false;
                        break;
                    }
                }
                ancestor = anc.parent();
            }

            if !found_loop {
                errored = if command == "break" {
                    append_syntax_error!(
                        parse_errors,
                        source_start,
                        source_length,
                        INVALID_BREAK_ERR_MSG
                    )
                } else {
                    append_syntax_error!(
                        parse_errors,
                        source_start,
                        source_length,
                        INVALID_CONTINUE_ERR_MSG
                    )
                }
            }
        }

        // Check that we don't do an invalid builtin (issue #1252).
        if !errored && decoration == StatementDecoration::builtin {
            let mut command = unexp_command.to_owned();
            if expand_one(
                &mut command,
                ExpandFlags::FAIL_ON_CMDSUBST,
                &OperationContext::empty(),
                match parse_errors {
                    Some(pe) => Some(pe),
                    None => None,
                },
            ) && !builtin_exists(unexp_command)
            {
                errored = append_syntax_error!(
                    parse_errors,
                    source_start,
                    source_length,
                    UNKNOWN_BUILTIN_ERR_MSG,
                    unexp_command
                );
            }
        }

        if let Some(ref mut parse_errors) = parse_errors {
            // The expansion errors here go from the *command* onwards,
            // so we need to offset them by the *command* offset,
            // excluding the decoration.
            parse_error_offset_source_start(&mut new_errors, dst.command.source_range().start());
            parse_errors.extend(new_errors);
        }
    }
    errored
}

// Given we have a trailing argument_or_redirection_list, like `begin; end > /dev/null`, verify that
// there are no arguments in the list.
fn detect_errors_in_block_redirection_list(
    args_or_redirs: &ast::ArgumentOrRedirectionList,
    out_errors: &mut Option<&mut ParseErrorList>,
) -> bool {
    let Some(first_arg) = get_first_arg(args_or_redirs) else {
        return false;
    };
    if args_or_redirs
        .parent()
        .unwrap()
        .as_brace_statement()
        .is_some()
    {
        return append_syntax_error!(
            out_errors,
            first_arg.source_range().start(),
            first_arg.source_range().length(),
            RIGHT_BRACE_ARG_ERR_MSG
        );
    }
    append_syntax_error!(
        out_errors,
        first_arg.source_range().start(),
        first_arg.source_range().length(),
        END_ARG_ERR_MSG
    )
}

/// Given a string containing a variable expansion error, append an appropriate error to the errors
/// list. The global_token_pos is the offset of the token in the larger source, and the dollar_pos
/// is the offset of the offending dollar sign within the token.
pub fn parse_util_expand_variable_error(
    token: &wstr,
    global_token_pos: usize,
    dollar_pos: usize,
    errors: &mut ParseErrorList,
) {
    let mut errors = Some(errors);
    // Note that dollar_pos is probably VARIABLE_EXPAND or VARIABLE_EXPAND_SINGLE, not a literal
    // dollar sign.
    let token = token.as_char_slice();
    let double_quotes = token[dollar_pos] == VARIABLE_EXPAND_SINGLE;
    let start_error_count = errors.as_ref().unwrap().len();
    let global_dollar_pos = global_token_pos + dollar_pos;
    let global_after_dollar_pos = global_dollar_pos + 1;
    let char_after_dollar = token.get(dollar_pos + 1).copied().unwrap_or('\0');

    match char_after_dollar {
        BRACE_BEGIN | '{' => {
            // The BRACE_BEGIN is for unquoted, the { is for quoted. Anyways we have (possible
            // quoted) ${. See if we have a }, and the stuff in between is variable material. If so,
            // report a bracket error. Otherwise just complain about the ${.
            let mut looks_like_variable = false;
            let closing_bracket = token
                .iter()
                .skip(dollar_pos + 2)
                .position(|c| {
                    *c == if char_after_dollar == '{' {
                        '}'
                    } else {
                        BRACE_END
                    }
                })
                .map(|p| p + dollar_pos + 2);
            let mut var_name = L!("");
            if let Some(var_end) = closing_bracket {
                let var_start = dollar_pos + 2;
                var_name = (&token[var_start..var_end]).into();
                looks_like_variable = valid_var_name(var_name);
            }
            if looks_like_variable {
                if double_quotes {
                    append_syntax_error!(
                        errors,
                        global_after_dollar_pos,
                        1,
                        ERROR_BRACKETED_VARIABLE_QUOTED1,
                        truncate(var_name, var_err_len, None)
                    );
                } else {
                    append_syntax_error!(
                        errors,
                        global_after_dollar_pos,
                        1,
                        ERROR_BRACKETED_VARIABLE1,
                        truncate(var_name, var_err_len, None),
                    );
                }
            } else {
                append_syntax_error!(errors, global_after_dollar_pos, 1, ERROR_BAD_VAR_CHAR1, '{');
            }
        }
        INTERNAL_SEPARATOR => {
            // e.g.: echo foo"$"baz
            // These are only ever quotes, not command substitutions. Command substitutions are
            // handled earlier.
            append_syntax_error!(errors, global_dollar_pos, 1, ERROR_NO_VAR_NAME);
        }
        '\0' => {
            append_syntax_error!(errors, global_dollar_pos, 1, ERROR_NO_VAR_NAME);
        }
        _ => {
            let mut token_stop_char = char_after_dollar;
            // Unescape (see issue #50).
            if token_stop_char == ANY_CHAR {
                token_stop_char = '?';
            } else if [ANY_STRING, ANY_STRING_RECURSIVE].contains(&token_stop_char) {
                token_stop_char = '*';
            }

            // Determine which error message to use. The format string may not consume all the
            // arguments we pass but that's harmless.
            append_syntax_error_formatted!(
                errors,
                global_after_dollar_pos,
                1,
                error_for_character(token_stop_char)
            );
        }
    }

    // We should have appended exactly one error.
    assert!(errors.as_ref().unwrap().len() == start_error_count + 1);
}

/// Error message for use of backgrounded commands before and/or.
pub(crate) const BOOL_AFTER_BACKGROUND_ERROR_MSG: &str =
    "The '%ls' command can not be used immediately after a backgrounded job";

/// Error message for backgrounded commands as conditionals.
const BACKGROUND_IN_CONDITIONAL_ERROR_MSG: &str =
    "Backgrounded commands can not be used as conditionals";

/// Error message for arguments to 'end'
const END_ARG_ERR_MSG: &str = "'end' does not take arguments. Did you forget a ';'?";
const RIGHT_BRACE_ARG_ERR_MSG: &str = "'}' does not take arguments. Did you forget a ';'?";

/// Error message when 'time' is in a pipeline.
const TIME_IN_PIPELINE_ERR_MSG: &str =
    "The 'time' command may only be at the beginning of a pipeline";

/// Maximum length of a variable name to show in error reports before truncation
const var_err_len: usize = 16;
