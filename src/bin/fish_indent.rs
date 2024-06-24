//! The fish_indent program.

#![allow(unknown_lints)]
// Delete this once we require Rust 1.74.
#![allow(unstable_name_collisions)]
#![allow(clippy::incompatible_msrv)]
#![allow(clippy::uninlined_format_args)]

use std::ffi::{CString, OsStr};
use std::io::{stdin, Read, Write};
use std::os::unix::ffi::OsStrExt;
use std::sync::atomic::Ordering;

use fish::panic::panic_handler;
use libc::{LC_ALL, STDOUT_FILENO};

use fish::ast::{
    self, Ast, Category, Leaf, List, Node, NodeVisitor, SourceRangeList, Traversal, Type,
};
use fish::builtins::shared::{STATUS_CMD_ERROR, STATUS_CMD_OK};
use fish::common::{
    str2wcstring, unescape_string, wcs2string, wcs2zstring, UnescapeFlags, UnescapeStringStyle,
    PROGRAM_NAME,
};
use fish::env::env_init;
use fish::env::environment::Environment;
use fish::env::EnvStack;
use fish::eprintf;
use fish::expand::INTERNAL_SEPARATOR;
use fish::fds::set_cloexec;
use fish::fprintf;
use fish::future::{IsSomeAnd, IsSorted};
use fish::global_safety::RelaxedAtomicBool;
use fish::highlight::{colorize, highlight_shell, HighlightRole, HighlightSpec};
use fish::libc::setlinebuf;
use fish::operation_context::OperationContext;
use fish::parse_constants::{ParseTokenType, ParseTreeFlags, SourceRange};
use fish::parse_util::{apply_indents, parse_util_compute_indents, SPACES_PER_INDENT};
use fish::print_help::print_help;
use fish::printf;
use fish::threads;
use fish::tokenizer::{TokenType, Tokenizer, TOK_SHOW_BLANK_LINES, TOK_SHOW_COMMENTS};
use fish::topic_monitor::topic_monitor_init;
use fish::wchar::prelude::*;
use fish::wcstringutil::count_preceding_backslashes;
use fish::wgetopt::{wopt, ArgType, WGetopter, WOption};
use fish::wutil::perror;
use fish::wutil::{fish_iswalnum, write_to_fd};
use fish::{
    flog::{self, activate_flog_categories_by_pattern, set_flog_file_fd},
    future_feature_flags,
};

/// Note: this got somewhat more complicated after introducing the new AST, because that AST no
/// longer encodes detailed lexical information (e.g. every newline). This feels more complex
/// than necessary and would probably benefit from a more layered approach where we identify
/// certain runs, weight line breaks, have a cost model, etc.
struct PrettyPrinter<'source, 'ast> {
    /// The parsed ast.
    ast: Ast,

    state: PrettyPrinterState<'source, 'ast>,
}

struct PrettyPrinterState<'source, 'ast> {
    /// Original source.
    source: &'source wstr,

    /// The indents of our string.
    /// This has the same length as 'source' and describes the indentation level.
    indents: Vec<i32>,

    /// The prettifier output.
    output: WString,

    // The indent of the source range which we are currently emitting.
    current_indent: usize,

    // Whether the next gap text should hide the first newline.
    gap_text_mask_newline: bool,

    // The "gaps": a sorted set of ranges between tokens.
    // These contain whitespace, comments, semicolons, and other lexical elements which are not
    // present in the ast.
    gaps: Vec<SourceRange>,

    // The sorted set of source offsets of nl_semi_t which should be set as semis, not newlines.
    // This is computed ahead of time for convenience.
    preferred_semi_locations: Vec<usize>,

    errors: Option<&'ast SourceRangeList>,
}

/// Flags we support.
#[derive(Copy, Clone, Default)]
struct GapFlags {
    /// Whether to allow line splitting via escaped newlines.
    /// For example, in argument lists:
    ///
    ///   echo a \
    ///   b
    ///
    /// If this is not set, then split-lines will be joined.
    allow_escaped_newlines: bool,

    /// Whether to require a space before this token.
    /// This is used when emitting semis:
    ///    echo a; echo b;
    /// No space required between 'a' and ';', or 'b' and ';'.
    skip_space: bool,
}

impl<'source, 'ast> PrettyPrinter<'source, 'ast> {
    fn new(source: &'source wstr, do_indent: bool) -> Self {
        let mut zelf = Self {
            ast: Ast::parse(source, parse_flags(), None),
            state: PrettyPrinterState {
                source,
                indents: if do_indent
                /* Whether to indent, or just insert spaces. */
                {
                    parse_util_compute_indents(source)
                } else {
                    vec![0; source.len()]
                },
                output: WString::default(),
                current_indent: 0,
                // Start with true to ignore leading empty lines.
                gap_text_mask_newline: true,
                gaps: vec![],
                preferred_semi_locations: vec![],
                errors: None,
            },
        };
        zelf.state.gaps = zelf.compute_gaps();
        zelf.state.preferred_semi_locations = zelf.compute_preferred_semi_locations();
        zelf
    }

    // Entry point. Prettify our source code and return it.
    fn prettify(&'ast mut self) -> WString {
        self.state.output.clear();
        self.state.errors = Some(&self.ast.extras.errors);
        self.state.visit(self.ast.top());

        // Trailing gap text.
        self.state.emit_gap_text_before(
            SourceRange::new(self.state.source.len(), 0),
            GapFlags::default(),
        );

        // Replace all trailing newlines with just a single one.
        while !self.state.output.is_empty() && self.state.at_line_start() {
            self.state.output.pop();
        }
        self.state.emit_newline();

        std::mem::replace(&mut self.state.output, WString::new())
    }

    // Return the gap ranges from our ast.
    fn compute_gaps(&self) -> Vec<SourceRange> {
        let range_compare = |r1: SourceRange, r2: SourceRange| {
            (r1.start(), r1.length()).cmp(&(r2.start(), r2.length()))
        };
        // Collect the token ranges into a list.
        let mut tok_ranges = vec![];
        for node in Traversal::new(self.ast.top()) {
            if node.category() == Category::leaf {
                let r = node.source_range();
                if r.length() > 0 {
                    tok_ranges.push(r);
                }
            }
        }
        // Place a zero length range at end to aid in our inverting.
        tok_ranges.push(SourceRange::new(self.state.source.len(), 0));

        // Our tokens should be sorted.
        assert!(tok_ranges.is_sorted_by(|x, y| Some(range_compare(*x, *y))));

        // For each range, add a gap range between the previous range and this range.
        let mut gaps = vec![];
        let mut prev_end = 0;
        for tok_range in tok_ranges {
            assert!(
                tok_range.start() >= prev_end,
                "Token range should not overlap or be out of order"
            );
            if tok_range.start() >= prev_end {
                gaps.push(SourceRange::new(prev_end, tok_range.start() - prev_end));
            }
            prev_end = tok_range.start() + tok_range.length();
        }
        gaps
    }

    // Return sorted list of semi-preferring semi_nl nodes.
    fn compute_preferred_semi_locations(&self) -> Vec<usize> {
        let mut result = vec![];
        let mut mark_semi_from_input = |n: &ast::SemiNl| {
            let Some(range) = n.range() else {
                return;
            };
            if self.state.substr(range) == ";" {
                result.push(range.start());
            }
        };

        // andor_job_lists get semis if the input uses semis.
        for node in Traversal::new(self.ast.top()) {
            // See if we have a condition and an andor_job_list.
            let condition;
            let andors;
            if let Some(ifc) = node.as_if_clause() {
                condition = ifc.condition.semi_nl.as_ref();
                andors = &ifc.andor_tail;
            } else if let Some(wc) = node.as_while_header() {
                condition = wc.condition.semi_nl.as_ref();
                andors = &wc.andor_tail;
            } else {
                continue;
            }

            // If there is no and-or tail then we always use a newline.
            if andors.count() > 0 {
                condition.map(&mut mark_semi_from_input);
                // Mark all but last of the andor list.
                for andor in andors.iter().take(andors.count() - 1) {
                    mark_semi_from_input(andor.job.semi_nl.as_ref().unwrap());
                }
            }
        }

        // `x ; and y` gets semis if it has them already, and they are on the same line.
        for node in Traversal::new(self.ast.top()) {
            let Some(job_list) = node.as_job_list() else {
                continue;
            };
            let mut prev_job_semi_nl = None;
            for job in job_list {
                // Set up prev_job_semi_nl for the next iteration to make control flow easier.
                let prev = prev_job_semi_nl;
                prev_job_semi_nl = job.semi_nl.as_ref();

                // Is this an 'and' or 'or' job?
                let Some(decorator) = job.decorator.as_ref() else {
                    continue;
                };

                // Now see if we want to mark 'prev' as allowing a semi.
                // Did we have a previous semi_nl which was a newline?
                let Some(prev) = prev else {
                    continue;
                };
                if self.state.substr(prev.range().unwrap()) != ";" {
                    continue;
                }

                // Is there a newline between them?
                let prev_start = prev.range().unwrap().start();
                let decorator_range = decorator.range().unwrap();
                assert!(prev_start <= decorator_range.start(), "Ranges out of order");
                if !self.state.source[prev_start..decorator_range.end()].contains('\n') {
                    // We're going to allow the previous semi_nl to be a semi.
                    result.push(prev_start);
                }
            }
        }
        result.sort_unstable();
        result
    }
}

impl<'source, 'ast> PrettyPrinterState<'source, 'ast> {
    fn indent(&self, index: usize) -> usize {
        usize::try_from(self.indents[index]).unwrap()
    }

    // Return gap text flags for the gap text that comes *before* a given node type.
    fn gap_text_flags_before_node(&self, node: &dyn Node) -> GapFlags {
        let mut result = GapFlags::default();
        match node.typ() {
            // Allow escaped newlines before leaf nodes that can be part of a long command.
            Type::argument | Type::redirection | Type::variable_assignment => {
                result.allow_escaped_newlines = true
            }
            Type::token_base => {
                // Allow escaped newlines before && and ||, and also pipes.
                match node.as_token().unwrap().token_type() {
                    ParseTokenType::andand | ParseTokenType::oror | ParseTokenType::pipe => {
                        result.allow_escaped_newlines = true;
                    }
                    ParseTokenType::string => {
                        // Allow escaped newlines before commands that follow a variable assignment
                        // since both can be long (#7955).
                        let p = node.parent().unwrap();
                        if p.typ() != Type::decorated_statement {
                            return result;
                        }
                        let p = p.parent().unwrap();
                        assert_eq!(p.typ(), Type::statement);
                        let p = p.parent().unwrap();
                        if let Some(job) = p.as_job_pipeline() {
                            if !job.variables.is_empty() {
                                result.allow_escaped_newlines = true;
                            }
                        } else if let Some(job_cnt) = p.as_job_continuation() {
                            if !job_cnt.variables.is_empty() {
                                result.allow_escaped_newlines = true;
                            }
                        } else if let Some(not_stmt) = p.as_not_statement() {
                            if !not_stmt.variables.is_empty() {
                                result.allow_escaped_newlines = true;
                            }
                        }
                    }
                    _ => (),
                }
            }
            _ => (),
        }
        result
    }

    // Return whether we are at the start of a new line.
    fn at_line_start(&self) -> bool {
        self.output.chars().next_back().is_none_or(|c| c == '\n')
    }

    // Return whether we have a space before the output.
    // This ignores escaped spaces and escaped newlines.
    fn has_preceding_space(&self) -> bool {
        let mut idx = isize::try_from(self.output.len()).unwrap() - 1;
        // Skip escaped newlines.
        // This is historical. Example:
        //
        // cmd1 \
        // | cmd2
        //
        // we want the pipe to "see" the space after cmd1.
        // TODO: this is too tricky, we should factor this better.
        while idx >= 0 && self.output.as_char_slice()[usize::try_from(idx).unwrap()] == '\n' {
            let backslashes =
                count_preceding_backslashes(self.source, usize::try_from(idx).unwrap());
            if backslashes % 2 == 0 {
                // Not escaped.
                return false;
            }
            idx -= 1 + isize::try_from(backslashes).unwrap();
        }
        usize::try_from(idx).is_ok_and(|idx| {
            self.output.as_char_slice()[idx] == ' ' && !char_is_escaped(&self.output, idx)
        })
    }

    // Return a substring of source.
    fn substr(&self, r: SourceRange) -> &wstr {
        &self.source[r.start()..r.end()]
    }

    // Emit a space or indent as necessary, depending on the previous output.
    fn emit_space_or_indent(&mut self, flags: GapFlags) {
        if self.at_line_start() {
            self.output
                .extend(std::iter::repeat(' ').take(SPACES_PER_INDENT * self.current_indent));
        } else if !flags.skip_space && !self.has_preceding_space() {
            self.output.push(' ');
        }
    }

    // Emit "gap text:" newlines and comments from the original source.
    // Gap text may be a few things:
    //
    // 1. Just a space is common. We will trim the spaces to be empty.
    //
    // Here the gap text is the comment, followed by the newline:
    //
    //    echo abc # arg
    //    echo def
    //
    // 2. It may also be an escaped newline:
    // Here the gap text is a space, backslash, newline, space.
    //
    //     echo \
    //       hi
    //
    // 3. Lastly it may be an error, if there was an error token. Here the gap text is the pipe:
    //
    //   begin | stuff
    //
    //  We do not handle errors here - instead our caller does.
    fn emit_gap_text(&mut self, range: SourceRange, flags: GapFlags) -> bool {
        let gap_text = &self.source[range.start()..range.end()];
        // Common case: if we are only spaces, do nothing.
        if !gap_text.chars().any(|c| c != ' ') {
            return false;
        }

        // Look to see if there is an escaped newline.
        // Emit it if either we allow it, or it comes before the first comment.
        // Note we do not have to be concerned with escaped backslashes or escaped #s. This is gap
        // text - we already know it has no semantic significance.
        if let Some(escaped_nl) = gap_text.find(L!("\\\n")) {
            let comment_idx = gap_text.find(L!("#"));
            if flags.allow_escaped_newlines
                || comment_idx.is_some_and(|comment_idx| escaped_nl < comment_idx)
            {
                // Emit a space before the escaped newline.
                if !self.at_line_start() && !self.has_preceding_space() {
                    self.output.push_str(" ");
                }
                self.output.push_str("\\\n");
                // Indent the continuation line and any leading comments (#7252).
                // Use the indentation level of the next newline.
                self.current_indent = self.indent(range.start() + escaped_nl + 1);
                self.emit_space_or_indent(GapFlags::default());
            }
        }

        // It seems somewhat ambiguous whether we always get a newline after a comment. Ensure we
        // always emit one.
        let mut needs_nl = false;

        let mut tokenizer = Tokenizer::new(gap_text, TOK_SHOW_COMMENTS | TOK_SHOW_BLANK_LINES);
        while let Some(tok) = tokenizer.next() {
            let tok_text = tokenizer.text_of(&tok);

            if needs_nl {
                self.emit_newline();
                needs_nl = false;
                self.gap_text_mask_newline = false;
                if tok_text == "\n" {
                    continue;
                }
            } else if self.gap_text_mask_newline {
                // When told to mask newlines, we do it as long as we get semicolon or newline.
                if tok.type_ == TokenType::end {
                    continue;
                }
                self.gap_text_mask_newline = false;
            }

            if tok.type_ == TokenType::comment {
                self.emit_space_or_indent(GapFlags::default());
                self.output.push_utfstr(tok_text);
                needs_nl = true;
            } else if tok.type_ == TokenType::end {
                // This may be either a newline or semicolon.
                // Semicolons found here are not part of the ast and can simply be removed.
                // Newlines are preserved unless mask_newline is set.
                if tok_text == "\n" {
                    self.emit_newline();
                    // Ignore successive ends.
                    self.gap_text_mask_newline = true;
                }
            } else {
                // Anything else we write a space.
                self.emit_space_or_indent(GapFlags::default());
                self.output.push_utfstr(tok_text);
            }
        }
        if needs_nl {
            self.emit_newline();
        }
        needs_nl
    }

    /// Return the gap text ending at a given index into the string, or empty if none.
    fn gap_text_to(&self, end: usize) -> SourceRange {
        match self.gaps.binary_search_by(|r| r.end().cmp(&end)) {
            Ok(pos) => self.gaps[pos],
            Err(_) => {
                // Not found.
                SourceRange::new(0, 0)
            }
        }
    }

    /// Return whether a range `r` overlaps an error range from our ast.
    fn range_contained_error(&self, r: SourceRange) -> bool {
        let errs = self.errors.as_ref().unwrap();
        let range_is_before = |x: SourceRange, y: SourceRange| x.end().cmp(&y.start());
        // FIXME: We want to have the errors sorted, but in some cases they aren't.
        // I suspect this is when the ast is unwinding because the source is fudged up.
        if errs.is_sorted_by(|&x, &y| Some(range_is_before(x, y))) {
            errs.partition_point(|&range| range_is_before(range, r).is_lt()) != errs.len()
        } else {
            false
        }
    }

    // Emit the gap text before a source range.
    fn emit_gap_text_before(&mut self, r: SourceRange, flags: GapFlags) -> bool {
        assert!(r.start() <= self.source.len(), "source out of bounds");
        let mut added_newline = false;

        // Find the gap text which ends at start.
        let range = self.gap_text_to(r.start());
        if range.length() > 0 {
            // Set the indent from the beginning of this gap text.
            // For example:
            // begin
            //    cmd
            //    # comment
            // end
            // Here the comment is the gap text before the end, but we want the indent from the
            // command.
            if range.start() < self.indents.len() {
                self.current_indent = self.indent(range.start());
            }

            // If this range contained an error, append the gap text without modification.
            // For example in: echo foo "
            // We don't want to mess with the quote.
            if self.range_contained_error(range) {
                self.output
                    .push_utfstr(&self.source[range.start()..range.end()]);
            } else {
                added_newline = self.emit_gap_text(range, flags);
            }
        }
        // Always clear gap_text_mask_newline after emitting even empty gap text.
        self.gap_text_mask_newline = false;
        added_newline
    }

    /// Given a string `input`, remove unnecessary quotes, etc.
    fn clean_text(&self, input: &wstr) -> WString {
        // Unescape the string - this leaves special markers around if there are any
        // expansions or anything. We specifically tell it to not compute backslash-escapes
        // like \U or \x, because we want to leave them intact.
        let Some(mut unescaped) = unescape_string(
            input,
            UnescapeStringStyle::Script(UnescapeFlags::SPECIAL | UnescapeFlags::NO_BACKSLASHES),
            // TODO: If we cannot unescape that means there's something fishy,
            // like a NUL in the source.
        ) else {
            return input.into();
        };

        // Remove INTERNAL_SEPARATOR because that's a quote.
        let quote = |ch| ch == INTERNAL_SEPARATOR;
        unescaped.retain(|c| !quote(c));

        // If only "good" chars are left, use the unescaped version.
        // This can be extended to other characters, but giving the precise list is tough,
        // can change over time (see "^", "%" and "?", in some cases "{}") and it just makes
        // people feel more at ease.
        let goodchars = |ch| fish_iswalnum(ch) || matches!(ch, '_' | '-' | '/');
        if unescaped.chars().all(goodchars) && !unescaped.is_empty() {
            unescaped
        } else {
            input.to_owned()
        }
    }

    // Emit a range of original text. This indents as needed, and also inserts preceding gap text.
    // If `tolerate_line_splitting` is set, then permit escaped newlines; otherwise collapse such
    // lines.
    fn emit_text(&mut self, r: SourceRange, flags: GapFlags) {
        self.emit_gap_text_before(r, flags);
        self.current_indent = self.indent(r.start());
        if r.length() > 0 {
            self.emit_space_or_indent(flags);
            self.output.push_utfstr(&self.clean_text(self.substr(r)));
        }
    }

    fn emit_node_text(&mut self, node: &dyn Node) {
        // Weird special-case: a token may end in an escaped newline. Notably, the newline is
        // not part of the following gap text, handle indentation here (#8197).
        let mut range = node.source_range();
        let ends_with_escaped_nl = self.substr(range).ends_with("\\\n");
        if ends_with_escaped_nl {
            range.length -= 2;
        }

        self.emit_text(range, self.gap_text_flags_before_node(node));

        if ends_with_escaped_nl {
            // By convention, escaped newlines are preceded with a space.
            self.output.push_str(" \\\n");
            // TODO Maybe check "allow_escaped_newlines" and use the precomputed indents.
            // The cases where this matters are probably very rare.
            self.current_indent += 1;
            self.emit_space_or_indent(GapFlags::default());
            self.current_indent -= 1;
        }
    }

    // Emit one newline.
    fn emit_newline(&mut self) {
        self.output.push('\n');
    }

    // Emit a semicolon.
    fn emit_semi(&mut self) {
        self.output.push(';');
    }

    fn visit_semi_nl(&mut self, node: &dyn ast::Token) {
        // These are semicolons or newlines which are part of the ast. That means it includes e.g.
        // ones terminating a job or 'if' header, but not random semis in job lists. We respect
        // preferred_semi_locations to decide whether or not these should stay as newlines or
        // become semicolons.
        let range = node.source_range();

        // Check if we should prefer a semicolon.
        let prefer_semi = range.length() > 0
            && self
                .preferred_semi_locations
                .binary_search(&range.start())
                .is_ok();

        self.emit_gap_text_before(range, self.gap_text_flags_before_node(node.as_node()));

        // Don't emit anything if the gap text put us on a newline (because it had a comment).
        if !self.at_line_start() {
            if prefer_semi {
                self.emit_semi();
            } else {
                self.emit_newline();
            }
        }
    }

    fn visit_redirection(&mut self, node: &ast::Redirection) {
        // No space between a redirection operator and its target (#2899).
        let Some(orange) = node.oper.range() else {
            return;
        };
        self.emit_text(orange, GapFlags::default());

        // (target is None if the source ends in a `<` or `>`
        let Some(trange) = node.target.range() else {
            return;
        };
        self.emit_text(
            trange,
            GapFlags {
                skip_space: true,
                ..Default::default()
            },
        );
    }

    fn visit_maybe_newlines(&mut self, node: &ast::MaybeNewlines) {
        // Our newlines may have comments embedded in them, example:
        //    cmd |
        //    # something
        //    cmd2
        // Treat it as gap text.
        let Some(range) = node.range() else {
            return;
        };
        if range.length() == 0 {
            return;
        }
        let flags = self.gap_text_flags_before_node(node);
        self.current_indent = self.indent(range.start());
        let added_newline = self.emit_gap_text_before(range, flags);
        let mut gap_range = range;
        if added_newline && gap_range.length() > 0 && self.source.char_at(gap_range.start()) == '\n'
        {
            gap_range.start += 1;
        }
        self.emit_gap_text(gap_range, flags);
    }

    fn visit_begin_header(&mut self) {
        if !self.at_line_start() {
            self.emit_newline();
        }
    }
}

// The flags we use to parse.
fn parse_flags() -> ParseTreeFlags {
    ParseTreeFlags::CONTINUE_AFTER_ERROR
        | ParseTreeFlags::INCLUDE_COMMENTS
        | ParseTreeFlags::LEAVE_UNTERMINATED
        | ParseTreeFlags::SHOW_BLANK_LINES
}

impl<'source, 'ast> NodeVisitor<'_> for PrettyPrinterState<'source, 'ast> {
    // Default implementation is to just visit children.
    fn visit(&mut self, node: &'_ dyn Node) {
        // Leaf nodes we just visit their text.
        if node.as_keyword().is_some() {
            self.emit_node_text(node);
            return;
        }
        if let Some(token) = node.as_token() {
            if token.token_type() == ParseTokenType::end {
                self.visit_semi_nl(token);
                return;
            }
            self.emit_node_text(node);
            return;
        }
        match node.typ() {
            Type::argument | Type::variable_assignment => {
                self.emit_node_text(node);
            }
            Type::redirection => {
                self.visit_redirection(node.as_redirection().unwrap());
            }
            Type::maybe_newlines => {
                self.visit_maybe_newlines(node.as_maybe_newlines().unwrap());
            }
            Type::begin_header => {
                // 'begin' does not require a newline after it, but we insert one.
                node.accept(self, false);
                self.visit_begin_header();
            }
            _ => {
                // For branch and list nodes, default is to visit their children.
                if [Category::branch, Category::list].contains(&node.category()) {
                    node.accept(self, false);
                    return;
                }
                panic!("unexpected node type");
            }
        }
    }
}

/// Return whether a character at a given index is escaped.
/// A character is escaped if it has an odd number of backslashes.
fn char_is_escaped(text: &wstr, idx: usize) -> bool {
    count_preceding_backslashes(text, idx) % 2 == 1
}

fn main() {
    PROGRAM_NAME.set(L!("fish_indent")).unwrap();
    panic_handler(throwing_main)
}

fn throwing_main() -> i32 {
    topic_monitor_init();
    threads::init();
    // Using the user's default locale could be a problem if it doesn't use UTF-8 encoding. That's
    // because the fish project assumes Unicode UTF-8 encoding in all of its scripts.
    //
    // TODO: Auto-detect the encoding of the script. We should look for a vim style comment
    // (e.g., "# vim: set fileencoding=<encoding-name>:") or an emacs style comment
    // (e.g., "# -*- coding: <encoding-name> -*-").
    {
        let s = CString::new("").unwrap();
        unsafe { libc::setlocale(LC_ALL, s.as_ptr()) };
    }
    env_init(None, true, false);

    if let Some(features_var) = EnvStack::globals().get(L!("fish_features")) {
        for s in features_var.as_list() {
            future_feature_flags::set_from_string(s.as_utfstr());
        }
    }

    // Types of output we support.
    #[derive(Eq, PartialEq)]
    enum OutputType {
        PlainText,
        File,
        Ansi,
        PygmentsCsv,
        Check,
        Html,
    }

    let mut output_type = OutputType::PlainText;
    let mut output_location = L!("");
    let mut do_indent = true;
    let mut only_indent = false;
    let mut only_unindent = false;
    // File path for debug output.
    let mut debug_output = None;

    let short_opts: &wstr = L!("+d:hvwicD:");
    let long_opts: &[WOption] = &[
        wopt(L!("debug"), ArgType::RequiredArgument, 'd'),
        wopt(L!("debug-output"), ArgType::RequiredArgument, 'o'),
        wopt(L!("debug-stack-frames"), ArgType::RequiredArgument, 'D'),
        wopt(L!("dump-parse-tree"), ArgType::NoArgument, 'P'),
        wopt(L!("no-indent"), ArgType::NoArgument, 'i'),
        wopt(L!("only-indent"), ArgType::NoArgument, '\x04'),
        wopt(L!("only-unindent"), ArgType::NoArgument, '\x05'),
        wopt(L!("help"), ArgType::NoArgument, 'h'),
        wopt(L!("version"), ArgType::NoArgument, 'v'),
        wopt(L!("write"), ArgType::NoArgument, 'w'),
        wopt(L!("html"), ArgType::NoArgument, '\x01'),
        wopt(L!("ansi"), ArgType::NoArgument, '\x02'),
        wopt(L!("pygments"), ArgType::NoArgument, '\x03'),
        wopt(L!("check"), ArgType::NoArgument, 'c'),
    ];

    let args: Vec<WString> = std::env::args_os()
        .map(|osstr| str2wcstring(osstr.as_bytes()))
        .collect();
    let mut shim_args: Vec<&wstr> = args.iter().map(|s| s.as_ref()).collect();
    let mut w = WGetopter::new(short_opts, long_opts, &mut shim_args);

    while let Some(c) = w.next_opt() {
        match c {
            'P' => DUMP_PARSE_TREE.store(true),
            'h' => {
                print_help("fish_indent");
                return STATUS_CMD_OK.unwrap();
            }
            'v' => {
                printf!(
                    "%s",
                    wgettext_fmt!(
                        "%s, version %s\n",
                        PROGRAM_NAME.get().unwrap(),
                        fish::BUILD_VERSION
                    )
                );
                return STATUS_CMD_OK.unwrap();
            }
            'w' => output_type = OutputType::File,
            'i' => do_indent = false,
            '\x04' => only_indent = true,
            '\x05' => only_unindent = true,
            '\x01' => output_type = OutputType::Html,
            '\x02' => output_type = OutputType::Ansi,
            '\x03' => output_type = OutputType::PygmentsCsv,
            'c' => output_type = OutputType::Check,
            'd' => {
                activate_flog_categories_by_pattern(w.woptarg.unwrap());
                for cat in flog::categories::all_categories() {
                    if cat.enabled.load(Ordering::Relaxed) {
                        printf!("Debug enabled for category: %s\n", cat.name);
                    }
                }
            }
            'D' => {
                // TODO: Option is currently useless.
                // Either remove it or make it work with FLOG.
            }
            'o' => {
                debug_output = Some(w.woptarg.unwrap());
            }
            _ => return STATUS_CMD_ERROR.unwrap(),
        }
    }

    let args = &w.argv[w.wopt_index..];

    // Direct any debug output right away.
    if let Some(debug_output) = debug_output {
        let file = {
            let debug_output = wcs2zstring(debug_output);
            let mode = CString::new("w").unwrap();
            unsafe { libc::fopen(debug_output.as_ptr(), mode.as_ptr()) }
        };
        if file.is_null() {
            eprintf!("Could not open file %s\n", debug_output);
            perror("fopen");
            return -1;
        }
        let fd = unsafe { libc::fileno(file) };
        set_cloexec(fd, true);
        unsafe { setlinebuf(file) };
        set_flog_file_fd(fd);
    }

    let mut retval = 0;

    let mut src;
    let mut i = 0;
    while i < args.len() || (args.is_empty() && i == 0) {
        if args.is_empty() && i == 0 {
            if output_type == OutputType::File {
                eprintf!(
                    "%s",
                    wgettext_fmt!(
                        "Expected file path to read/write for -w:\n\n $ %ls -w foo.fish\n",
                        PROGRAM_NAME.get().unwrap()
                    )
                );
                return STATUS_CMD_ERROR.unwrap();
            }
            match read_file(stdin()) {
                Ok(s) => src = s,
                Err(()) => return STATUS_CMD_ERROR.unwrap(),
            }
        } else {
            let arg = args[i];
            match std::fs::File::open(OsStr::from_bytes(&wcs2string(arg))) {
                Ok(file) => {
                    match read_file(file) {
                        Ok(s) => src = s,
                        Err(()) => return STATUS_CMD_ERROR.unwrap(),
                    }
                    output_location = arg;
                }
                Err(err) => {
                    eprintf!(
                        "%s",
                        wgettext_fmt!("Opening \"%s\" failed: %s\n", arg, err.to_string())
                    );
                    return STATUS_CMD_ERROR.unwrap();
                }
            }
        }

        if output_type == OutputType::PygmentsCsv {
            let output = make_pygments_csv(&src);
            let _ = write_to_fd(&output, STDOUT_FILENO);
            i += 1;
            continue;
        }

        let output_wtext = if only_indent || only_unindent {
            let indents = parse_util_compute_indents(&src);
            if only_indent {
                apply_indents(&src, &indents)
            } else {
                // Only unindent.
                let mut indented_everywhere = true;
                for (i, c) in src.chars().enumerate() {
                    if c != '\n' || i + 1 == src.len() {
                        continue;
                    }
                    let num_spaces = SPACES_PER_INDENT * usize::try_from(indents[i + 1]).unwrap();
                    if src.len() < i + 1 + num_spaces
                        || !src[i + 1..].chars().take(num_spaces).all(|c| c == ' ')
                    {
                        indented_everywhere = false;
                        break;
                    }
                }
                if indented_everywhere {
                    let mut out = WString::new();
                    let mut i = 0;
                    while i < src.len() {
                        let c = src.as_char_slice()[i];
                        out.push(c);
                        i += 1;
                        if c != '\n' || i == src.len() {
                            continue;
                        }
                        i += SPACES_PER_INDENT * usize::try_from(indents[i]).unwrap();
                    }
                    out
                } else {
                    src.clone()
                }
            }
        } else {
            prettify(&src, do_indent)
        };

        // Maybe colorize.
        let mut colors = vec![];
        if output_type != OutputType::PlainText {
            highlight_shell(
                &output_wtext,
                &mut colors,
                &OperationContext::globals(),
                false,
                None,
            );
        }

        let mut colored_output = vec![];
        match output_type {
            OutputType::PlainText => {
                colored_output = no_colorize(&output_wtext);
            }
            OutputType::File => {
                match std::fs::File::create(OsStr::from_bytes(&wcs2string(output_location))) {
                    Ok(mut file) => {
                        let _ = file.write_all(&wcs2string(&output_wtext));
                    }
                    Err(err) => {
                        eprintf!(
                            "%s",
                            wgettext_fmt!(
                                "Opening \"%s\" failed: %s\n",
                                output_location,
                                err.to_string()
                            )
                        );
                        return STATUS_CMD_ERROR.unwrap();
                    }
                }
            }
            OutputType::Ansi => {
                colored_output = colorize(&output_wtext, &colors, EnvStack::globals());
            }
            OutputType::Html => {
                colored_output = html_colorize(&output_wtext, &colors);
            }
            OutputType::PygmentsCsv => {
                unreachable!()
            }
            OutputType::Check => {
                if output_wtext != src {
                    if let Some(arg) = args.get(i) {
                        eprintf!("%s\n", arg);
                    }
                    retval += 1;
                }
            }
        }

        let _ = write_to_fd(&colored_output, STDOUT_FILENO);
        i += 1;
    }
    retval
}

static DUMP_PARSE_TREE: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

// Read the entire contents of a file into the specified string.
fn read_file(mut f: impl Read) -> Result<WString, ()> {
    let mut buf = vec![];
    f.read_to_end(&mut buf).map_err(|_| ())?;
    Ok(str2wcstring(&buf))
}

fn highlight_role_to_string(role: HighlightRole) -> &'static wstr {
    match role {
        HighlightRole::normal => L!("normal"),
        HighlightRole::error => L!("error"),
        HighlightRole::command => L!("command"),
        HighlightRole::keyword => L!("keyword"),
        HighlightRole::statement_terminator => L!("statement_terminator"),
        HighlightRole::param => L!("param"),
        HighlightRole::option => L!("option"),
        HighlightRole::comment => L!("comment"),
        HighlightRole::search_match => L!("search_match"),
        HighlightRole::operat => L!("operat"),
        HighlightRole::escape => L!("escape"),
        HighlightRole::quote => L!("quote"),
        HighlightRole::redirection => L!("redirection"),
        HighlightRole::autosuggestion => L!("autosuggestion"),
        HighlightRole::selection => L!("selection"),
        HighlightRole::pager_progress => L!("pager_progress"),
        HighlightRole::pager_background => L!("pager_background"),
        HighlightRole::pager_prefix => L!("pager_prefix"),
        HighlightRole::pager_completion => L!("pager_completion"),
        HighlightRole::pager_description => L!("pager_description"),
        HighlightRole::pager_secondary_background => L!("pager_secondary_background"),
        HighlightRole::pager_secondary_prefix => L!("pager_secondary_prefix"),
        HighlightRole::pager_secondary_completion => L!("pager_secondary_completion"),
        HighlightRole::pager_secondary_description => L!("pager_secondary_description"),
        HighlightRole::pager_selected_background => L!("pager_selected_background"),
        HighlightRole::pager_selected_prefix => L!("pager_selected_prefix"),
        HighlightRole::pager_selected_completion => L!("pager_selected_completion"),
        HighlightRole::pager_selected_description => L!("pager_selected_description"),
    }
}

// Entry point for Pygments CSV output.
// Our output is a newline-separated string.
// Each line is of the form `start,end,role`
// start and end is the half-open token range, value is a string from highlight_role_t.
// Example:
// 3,7,command
fn make_pygments_csv(src: &wstr) -> Vec<u8> {
    let mut colors = vec![];
    highlight_shell(src, &mut colors, &OperationContext::globals(), false, None);
    assert_eq!(
        colors.len(),
        src.len(),
        "Colors and src should have same size"
    );

    struct TokenRange {
        start: usize,
        end: usize,
        role: HighlightRole,
    }

    let mut token_ranges: Vec<TokenRange> = vec![];
    for (i, color) in colors.iter().cloned().enumerate() {
        let role = color.foreground;
        // See if we can extend the last range.
        if let Some(last) = token_ranges.last_mut() {
            if last.role == role && last.end == i {
                last.end = i + 1;
                continue;
            }
        }
        // We need a new range.
        token_ranges.push(TokenRange {
            start: i,
            end: i + 1,
            role,
        });
    }

    // Now render these to a string.
    let mut result = String::new();
    for range in token_ranges {
        result += &format!(
            "{},{},{}\n",
            range.start,
            range.end,
            highlight_role_to_string(range.role)
        );
    }
    result.into_bytes()
}

// Entry point for prettification.
fn prettify(src: &wstr, do_indent: bool) -> WString {
    if DUMP_PARSE_TREE.load() {
        let ast = Ast::parse(
            src,
            ParseTreeFlags::LEAVE_UNTERMINATED
                | ParseTreeFlags::INCLUDE_COMMENTS
                | ParseTreeFlags::SHOW_EXTRA_SEMIS,
            None,
        );
        let ast_dump = ast.dump(src);
        eprintf!("%s\n", ast_dump);
    }
    let mut printer = PrettyPrinter::new(src, do_indent);
    printer.prettify()
}

/// Given a string and list of colors of the same size, return the string with HTML span elements
/// for the various colors.
fn html_class_name_for_color(spec: HighlightSpec) -> &'static wstr {
    match spec.foreground {
        HighlightRole::normal => L!("fish_color_normal"),
        HighlightRole::error => L!("fish_color_error"),
        HighlightRole::command => L!("fish_color_command"),
        HighlightRole::statement_terminator => L!("fish_color_statement_terminator"),
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
        _ => L!("fish_color_other"),
    }
}

fn html_colorize(text: &wstr, colors: &[HighlightSpec]) -> Vec<u8> {
    if text.is_empty() {
        return vec![];
    }

    assert_eq!(colors.len(), text.len());
    let mut html = L!("<pre><code>").to_owned();
    let mut last_color = HighlightSpec::new();
    for (i, (wc, &color)) in text.chars().zip(colors).enumerate() {
        // Handle colors.
        if i > 0 && color != last_color {
            html.push_str("</span>");
        }
        if i == 0 || color != last_color {
            sprintf!(=> &mut html, "<span class=\"%ls\">", html_class_name_for_color(color));
        }
        last_color = color;

        // Handle text.
        match wc {
            '&' => html.push_str("&amp;"),
            '\'' => html.push_str("&apos;"),
            '"' => html.push_str("&quot;"),
            '<' => html.push_str("&lt;"),
            '>' => html.push_str("&gt;"),
            _ => html.push(wc),
        }
    }
    html.push_str("</span></code></pre>");
    wcs2string(&html)
}

fn no_colorize(text: &wstr) -> Vec<u8> {
    wcs2string(text)
}
