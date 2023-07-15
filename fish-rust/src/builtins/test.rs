use super::prelude::*;
use crate::common;

mod test_expressions {
    use super::*;

    use crate::wutil::{
        file_id_for_path, fish_wcswidth, lwstat, waccess, wcstod::wcstod, wcstoi_opts, wstat,
        Error, Options,
    };
    use once_cell::sync::Lazy;
    use std::collections::HashMap;
    use std::os::unix::prelude::*;

    #[derive(Copy, Clone, PartialEq, Eq)]
    pub(super) enum Token {
        unknown, // arbitrary string

        bang, // "!", inverts sense

        filetype_b, // "-b", for block special files
        filetype_c, // "-c", for character special files
        filetype_d, // "-d", for directories
        filetype_e, // "-e", for files that exist
        filetype_f, // "-f", for for regular files
        filetype_G, // "-G", for check effective group id
        filetype_g, // "-g", for set-group-id
        filetype_h, // "-h", for symbolic links
        filetype_k, // "-k", for sticky bit
        filetype_L, // "-L", same as -h
        filetype_O, // "-O", for check effective user id
        filetype_p, // "-p", for FIFO
        filetype_S, // "-S", socket

        filesize_s, // "-s", size greater than zero

        filedesc_t, // "-t", whether the fd is associated with a terminal

        fileperm_r, // "-r", read permission
        fileperm_u, // "-u", whether file is setuid
        fileperm_w, // "-w", whether file write permission is allowed
        fileperm_x, // "-x", whether file execute/search is allowed

        string_n,         // "-n", non-empty string
        string_z,         // "-z", true if length of string is 0
        string_equal,     // "=", true if strings are identical
        string_not_equal, // "!=", true if strings are not identical

        file_newer, // f1 -nt f2, true if f1 exists and is newer than f2, or there is no f2
        file_older, // f1 -ot f2, true if f2 exists and f1 does not, or f1 is older than f2
        file_same,  // f1 -ef f2, true if f1 and f2 exist and refer to same file

        number_equal,         // "-eq", true if numbers are equal
        number_not_equal,     // "-ne", true if numbers are not equal
        number_greater,       // "-gt", true if first number is larger than second
        number_greater_equal, // "-ge", true if first number is at least second
        number_lesser,        // "-lt", true if first number is smaller than second
        number_lesser_equal,  // "-le", true if first number is at most second

        combine_and, // "-a", true if left and right are both true
        combine_or,  // "-o", true if either left or right is true

        paren_open,  // "(", open paren
        paren_close, // ")", close paren
    }

    /// Our number type. We support both doubles and long longs. We have to support these separately
    /// because some integers are not representable as doubles; these may come up in practice (e.g.
    /// inodes).
    #[derive(Copy, Clone, Default, PartialEq, PartialOrd)]
    struct Number {
        // A number has an integral base and a floating point delta.
        // Conceptually the number is base + delta.
        // We enforce the property that 0 <= delta < 1.
        base: i64,
        delta: f64,
    }
    impl Number {
        pub(super) fn new(base: i64, delta: f64) -> Self {
            assert!((0.0..1.0).contains(&delta), "Invalid delta");
            Self { base, delta }
        }

        // Return true if the number is a tty().
        fn isatty(&self, streams: &mut IoStreams<'_>) -> bool {
            fn istty(fd: libc::c_int) -> bool {
                // Safety: isatty cannot crash.
                unsafe { libc::isatty(fd) > 0 }
            }
            if self.delta != 0.0 || self.base > i32::MAX as i64 || self.base < i32::MIN as i64 {
                return false;
            }
            let bint = self.base as i32;
            if bint == 0 {
                match streams.stdin_fd {
                    -1 => false,
                    fd => istty(fd),
                }
            } else if bint == 1 {
                !streams.out_is_redirected && istty(libc::STDOUT_FILENO)
            } else if bint == 2 {
                !streams.err_is_redirected && istty(libc::STDERR_FILENO)
            } else {
                istty(bint)
            }
        }
    }

    const UNARY_PRIMARY: u32 = 1 << 0;
    const BINARY_PRIMARY: u32 = 1 << 1;

    struct TokenInfo {
        tok: Token,
        flags: u32,
    }

    impl TokenInfo {
        fn new(tok: Token, flags: u32) -> Self {
            Self { tok, flags }
        }
    }

    fn token_for_string(str: &wstr) -> &'static TokenInfo {
        if let Some(res) = TOKEN_INFOS.get(str) {
            res
        } else {
            TOKEN_INFOS
                .get(L!(""))
                .expect("Should have token for empty string")
        }
    }

    static TOKEN_INFOS: Lazy<HashMap<&'static wstr, TokenInfo>> = Lazy::new(|| {
        #[rustfmt::skip]
        let pairs = [
            (L!(""), TokenInfo::new(Token::unknown, 0)),
            (L!("!"), TokenInfo::new(Token::bang, 0)),
            (L!("-b"), TokenInfo::new(Token::filetype_b, UNARY_PRIMARY)),
            (L!("-c"), TokenInfo::new(Token::filetype_c, UNARY_PRIMARY)),
            (L!("-d"), TokenInfo::new(Token::filetype_d, UNARY_PRIMARY)),
            (L!("-e"), TokenInfo::new(Token::filetype_e, UNARY_PRIMARY)),
            (L!("-f"), TokenInfo::new(Token::filetype_f, UNARY_PRIMARY)),
            (L!("-G"), TokenInfo::new(Token::filetype_G, UNARY_PRIMARY)),
            (L!("-g"), TokenInfo::new(Token::filetype_g, UNARY_PRIMARY)),
            (L!("-h"), TokenInfo::new(Token::filetype_h, UNARY_PRIMARY)),
            (L!("-k"), TokenInfo::new(Token::filetype_k, UNARY_PRIMARY)),
            (L!("-L"), TokenInfo::new(Token::filetype_L, UNARY_PRIMARY)),
            (L!("-O"), TokenInfo::new(Token::filetype_O, UNARY_PRIMARY)),
            (L!("-p"), TokenInfo::new(Token::filetype_p, UNARY_PRIMARY)),
            (L!("-S"), TokenInfo::new(Token::filetype_S, UNARY_PRIMARY)),
            (L!("-s"), TokenInfo::new(Token::filesize_s, UNARY_PRIMARY)),
            (L!("-t"), TokenInfo::new(Token::filedesc_t, UNARY_PRIMARY)),
            (L!("-r"), TokenInfo::new(Token::fileperm_r, UNARY_PRIMARY)),
            (L!("-u"), TokenInfo::new(Token::fileperm_u, UNARY_PRIMARY)),
            (L!("-w"), TokenInfo::new(Token::fileperm_w, UNARY_PRIMARY)),
            (L!("-x"), TokenInfo::new(Token::fileperm_x, UNARY_PRIMARY)),
            (L!("-n"), TokenInfo::new(Token::string_n, UNARY_PRIMARY)),
            (L!("-z"), TokenInfo::new(Token::string_z, UNARY_PRIMARY)),
            (L!("="), TokenInfo::new(Token::string_equal, BINARY_PRIMARY)),
            (L!("!="), TokenInfo::new(Token::string_not_equal, BINARY_PRIMARY)),
            (L!("-nt"), TokenInfo::new(Token::file_newer, BINARY_PRIMARY)),
            (L!("-ot"), TokenInfo::new(Token::file_older, BINARY_PRIMARY)),
            (L!("-ef"), TokenInfo::new(Token::file_same, BINARY_PRIMARY)),
            (L!("-eq"), TokenInfo::new(Token::number_equal, BINARY_PRIMARY)),
            (L!("-ne"), TokenInfo::new(Token::number_not_equal, BINARY_PRIMARY)),
            (L!("-gt"), TokenInfo::new(Token::number_greater, BINARY_PRIMARY)),
            (L!("-ge"), TokenInfo::new(Token::number_greater_equal, BINARY_PRIMARY)),
            (L!("-lt"), TokenInfo::new(Token::number_lesser, BINARY_PRIMARY)),
            (L!("-le"), TokenInfo::new(Token::number_lesser_equal, BINARY_PRIMARY)),
            (L!("-a"), TokenInfo::new(Token::combine_and, 0)),
            (L!("-o"), TokenInfo::new(Token::combine_or, 0)),
            (L!("("), TokenInfo::new(Token::paren_open, 0)),
            (L!(")"), TokenInfo::new(Token::paren_close, 0))
        ];
        pairs.into_iter().collect()
    });

    // Grammar.
    //
    //  <expr> = <combining_expr>
    //
    //  <combining_expr> = <unary_expr> and/or <combining_expr> |
    //                     <unary_expr>
    //
    //  <unary_expr> = bang <unary_expr> |
    //                <primary>
    //
    //  <primary> = <unary_primary> arg |
    //              arg <binary_primary> arg |
    //              '(' <expr> ')'

    #[derive(Default)]
    pub(super) struct TestParser<'a> {
        strings: &'a [WString],
        errors: Vec<WString>,
        error_idx: usize,
    }

    impl<'a> TestParser<'a> {
        fn arg(&self, idx: usize) -> &'a wstr {
            &self.strings[idx]
        }

        fn add_error(&mut self, idx: usize, text: WString) {
            self.errors.push(text);
            if self.errors.len() == 1 {
                self.error_idx = idx;
            }
        }
    }

    type Range = std::ops::Range<usize>;

    /// Base trait for expressions.
    pub(super) trait Expression {
        /// Evaluate returns true if the expression is true (i.e. STATUS_CMD_OK).
        fn evaluate(&self, streams: &mut IoStreams<'_>, errors: &mut Vec<WString>) -> bool;

        /// Return base.range.
        fn range(&self) -> Range;

        // Helper to convert ourselves into Some Box.
        fn into_some_box(self) -> Option<Box<dyn Expression>>
        where
            Self: Sized + 'static,
        {
            Some(Box::new(self))
        }
    }

    /// Single argument like -n foo or "just a string".
    struct UnaryPrimary {
        arg: WString,
        token: Token,
        range: Range,
    }

    /// Two argument primary like foo != bar.
    struct BinaryPrimary {
        arg_left: WString,
        arg_right: WString,
        token: Token,
        range: Range,
    }

    /// Unary operator like bang.
    struct UnaryOperator {
        subject: Box<dyn Expression>,
        token: Token,
        range: Range,
    }

    /// Combining expression. Contains a list of AND or OR expressions. It takes more than two so that
    /// we don't have to worry about precedence in the parser.
    struct CombiningExpression {
        subjects: Vec<Box<dyn Expression>>,
        combiners: Vec<Token>,
        token: Token,
        range: Range,
    }

    /// Parenthentical expression.
    struct ParentheticalExpression {
        contents: Box<dyn Expression>,
        token: Token,
        range: Range,
    }

    impl Expression for UnaryPrimary {
        fn evaluate(&self, streams: &mut IoStreams<'_>, errors: &mut Vec<WString>) -> bool {
            unary_primary_evaluate(self.token, &self.arg, streams, errors)
        }

        fn range(&self) -> Range {
            self.range.clone()
        }
    }

    impl Expression for BinaryPrimary {
        fn evaluate(&self, _streams: &mut IoStreams<'_>, errors: &mut Vec<WString>) -> bool {
            binary_primary_evaluate(self.token, &self.arg_left, &self.arg_right, errors)
        }

        fn range(&self) -> Range {
            self.range.clone()
        }
    }

    impl Expression for UnaryOperator {
        fn evaluate(&self, streams: &mut IoStreams<'_>, errors: &mut Vec<WString>) -> bool {
            if self.token == Token::bang {
                !self.subject.evaluate(streams, errors)
            } else {
                errors.push(L!("Unknown token type in unary_operator_evaluate").to_owned());
                false
            }
        }

        fn range(&self) -> Range {
            self.range.clone()
        }
    }

    impl Expression for CombiningExpression {
        fn evaluate(&self, streams: &mut IoStreams<'_>, errors: &mut Vec<WString>) -> bool {
            let _res = self.subjects[0].evaluate(streams, errors);
            if self.token == Token::combine_and || self.token == Token::combine_or {
                assert!(!self.subjects.is_empty());
                assert!(self.combiners.len() + 1 == self.subjects.len());

                // One-element case.
                if self.subjects.len() == 1 {
                    return self.subjects[0].evaluate(streams, errors);
                }

                // Evaluate our lists, remembering that AND has higher precedence than OR. We can
                // visualize this as a sequence of OR expressions of AND expressions.
                let mut idx = 0;
                let max = self.subjects.len();
                let mut or_result = false;
                while idx < max {
                    if or_result {
                        // short circuit
                        break;
                    }
                    // Evaluate a stream of AND starting at given subject index. It may only have one
                    // element.
                    let mut and_result = true;
                    while idx < max {
                        // Evaluate it, short-circuiting.
                        and_result = and_result && self.subjects[idx].evaluate(streams, errors);

                        // If the combiner at this index (which corresponding to how we combine with the
                        // next subject) is not AND, then exit the loop.
                        if idx + 1 < max && self.combiners[idx] != Token::combine_and {
                            idx += 1;
                            break;
                        }

                        idx += 1;
                    }

                    // OR it in.
                    or_result = or_result || and_result;
                }
                return or_result;
            }
            errors.push(L!("Unknown token type in CombiningExpression.evaluate").to_owned());
            false
        }

        fn range(&self) -> Range {
            self.range.clone()
        }
    }

    impl Expression for ParentheticalExpression {
        fn evaluate(&self, streams: &mut IoStreams<'_>, errors: &mut Vec<WString>) -> bool {
            self.contents.evaluate(streams, errors)
        }

        fn range(&self) -> Range {
            self.range.clone()
        }
    }

    impl<'a> TestParser<'a> {
        fn error(&mut self, idx: usize, text: WString) -> Option<Box<dyn Expression>> {
            self.add_error(idx, text);
            None
        }

        fn parse_unary_expression(
            &mut self,
            start: usize,
            end: usize,
        ) -> Option<Box<dyn Expression>> {
            if start >= end {
                return self.error(start, sprintf!("Missing argument at index %u", start + 1));
            }
            let tok = token_for_string(self.arg(start)).tok;
            if tok == Token::bang {
                let subject = self.parse_unary_expression(start + 1, end);
                if let Some(subject) = subject {
                    let range = start..subject.range().end;
                    return UnaryOperator {
                        subject,
                        token: tok,
                        range,
                    }
                    .into_some_box();
                }
                return None;
            }
            self.parse_primary(start, end)
        }

        /// Parse a combining expression (AND, OR).
        fn parse_combining_expression(
            &mut self,
            start: usize,
            end: usize,
        ) -> Option<Box<dyn Expression>> {
            if start >= end {
                return None;
            }
            let mut subjects = Vec::new();
            let mut combiners = Vec::new();
            let mut idx = start;
            let mut first = true;
            while idx < end {
                if !first {
                    // This is not the first expression, so we expect a combiner.
                    let combiner = token_for_string(self.arg(idx)).tok;
                    if combiner != Token::combine_and && combiner != Token::combine_or {
                        /* Not a combiner, we're done */
                        self.errors.insert(
                            0,
                            sprintf!(
                                "Expected a combining operator like '-a' at index %u",
                                idx + 1
                            ),
                        );
                        self.error_idx = idx;
                        break;
                    }
                    combiners.push(combiner);
                    idx += 1;
                }

                // Parse another expression.
                let expr = self.parse_unary_expression(idx, end);
                if expr.is_none() {
                    self.add_error(idx, sprintf!("Missing argument at index %u", idx + 1));
                    if !first {
                        // Clean up the dangling combiner, since it never got its right hand expression.
                        combiners.pop();
                    }
                    break;
                }
                // Go to the end of this expression.
                let expr = expr.unwrap();
                idx = expr.range().end;
                subjects.push(expr);
                first = false;
            }

            if subjects.is_empty() {
                return None; // no subjects
            }

            // Our new expression takes ownership of all expressions we created. The base token we pass is
            // irrelevant.
            CombiningExpression {
                subjects,
                combiners,
                token: Token::combine_and,
                range: start..idx,
            }
            .into_some_box()
        }

        fn parse_unary_primary(&mut self, start: usize, end: usize) -> Option<Box<dyn Expression>> {
            // We need two arguments.
            if start >= end {
                return self.error(start, sprintf!("Missing argument at index %u", start + 1));
            }
            if start + 1 >= end {
                return self.error(
                    start + 1,
                    sprintf!("Missing argument at index %u", start + 2),
                );
            }

            // All our unary primaries are prefix, so the operator is at start.
            let info: &TokenInfo = token_for_string(self.arg(start));
            if info.flags & UNARY_PRIMARY == 0 {
                return None;
            }
            UnaryPrimary {
                arg: self.arg(start + 1).to_owned(),
                token: info.tok,
                range: start..start + 2,
            }
            .into_some_box()
        }

        fn parse_just_a_string(&mut self, start: usize, end: usize) -> Option<Box<dyn Expression>> {
            // Handle a string as a unary primary that is not a token of any other type. e.g. 'test foo -a
            // bar' should evaluate to true. We handle this with a unary primary of test_string_n.

            // We need one argument.
            if start >= end {
                return self.error(start, sprintf!("Missing argument at index %u", start + 1));
            }

            let info = token_for_string(self.arg(start));
            if info.tok != Token::unknown {
                return self.error(
                    start,
                    sprintf!("Unexpected argument type at index %u", start + 1),
                );
            }

            // This is hackish; a nicer way to implement this would be with a "just a string" expression
            // type.
            return UnaryPrimary {
                arg: self.arg(start).to_owned(),
                token: Token::string_n,
                range: start..start + 1,
            }
            .into_some_box();
        }

        fn parse_binary_primary(
            &mut self,
            start: usize,
            end: usize,
        ) -> Option<Box<dyn Expression>> {
            // We need three arguments.
            for idx in start..start + 3 {
                if idx >= end {
                    return self.error(idx, sprintf!("Missing argument at index %u", idx + 1));
                }
            }

            // All our binary primaries are infix, so the operator is at start + 1.
            let info = token_for_string(self.arg(start + 1));
            if info.flags & BINARY_PRIMARY == 0 {
                return None;
            }
            BinaryPrimary {
                arg_left: self.arg(start).to_owned(),
                arg_right: self.arg(start + 2).to_owned(),
                token: info.tok,
                range: start..start + 3,
            }
            .into_some_box()
        }

        fn parse_parenthetical(&mut self, start: usize, end: usize) -> Option<Box<dyn Expression>> {
            // We need at least three arguments: open paren, argument, close paren.
            if start + 3 >= end {
                return None;
            }

            // Must start with an open expression.
            let open_paren = token_for_string(self.arg(start));
            if open_paren.tok != Token::paren_open {
                return None;
            }

            // Parse a subexpression.
            let Some(subexpr) = self.parse_expression(start + 1, end) else {
                return None;
            };

            // Parse a close paren.
            let close_index = subexpr.range().end;
            assert!(close_index <= end);
            if close_index == end {
                return self.error(
                    close_index,
                    sprintf!("Missing close paren at index %u", close_index + 1),
                );
            }
            let close_paren = token_for_string(self.arg(close_index));
            if close_paren.tok != Token::paren_close {
                return self.error(
                    close_index,
                    sprintf!("Expected close paren at index %u", close_index + 1),
                );
            }

            // Success.
            ParentheticalExpression {
                contents: subexpr,
                token: Token::paren_open,
                range: start..close_index + 1,
            }
            .into_some_box()
        }

        fn parse_primary(&mut self, start: usize, end: usize) -> Option<Box<dyn Expression>> {
            if start >= end {
                return self.error(start, sprintf!("Missing argument at index %u", start + 1));
            }
            let mut expr = None;
            if expr.is_none() {
                expr = self.parse_parenthetical(start, end);
            }
            if expr.is_none() {
                expr = self.parse_unary_primary(start, end);
            }
            if expr.is_none() {
                expr = self.parse_binary_primary(start, end);
            }
            if expr.is_none() {
                expr = self.parse_just_a_string(start, end);
            }
            expr
        }

        // See IEEE 1003.1 breakdown of the behavior for different parameter counts.
        fn parse_3_arg_expression(
            &mut self,
            start: usize,
            end: usize,
        ) -> Option<Box<dyn Expression>> {
            assert!(end - start == 3);
            let mut result = None;
            let center_token = token_for_string(self.arg(start + 1));
            if center_token.flags & BINARY_PRIMARY != 0 {
                result = self.parse_binary_primary(start, end);
            } else if center_token.tok == Token::combine_and
                || center_token.tok == Token::combine_or
            {
                let left = self.parse_unary_expression(start, start + 1);
                let right = self.parse_unary_expression(start + 2, start + 3);
                if left.is_some() && right.is_some() {
                    // Transfer ownership to the vector of subjects.
                    let combiners = vec![center_token.tok];
                    let subjects = vec![left.unwrap(), right.unwrap()];
                    result = CombiningExpression {
                        subjects,
                        combiners,
                        token: center_token.tok,
                        range: start..end,
                    }
                    .into_some_box()
                }
            } else {
                result = self.parse_unary_expression(start, end);
            }
            result
        }

        fn parse_4_arg_expression(
            &mut self,
            start: usize,
            end: usize,
        ) -> Option<Box<dyn Expression>> {
            assert!(end - start == 4);
            let mut result = None;
            let first_token = token_for_string(self.arg(start)).tok;
            if first_token == Token::bang {
                let subject = self.parse_3_arg_expression(start + 1, end);
                if let Some(subject) = subject {
                    result = UnaryOperator {
                        subject,
                        token: first_token,
                        range: start..end,
                    }
                    .into_some_box();
                }
            } else if first_token == Token::paren_open {
                result = self.parse_parenthetical(start, end);
            } else {
                result = self.parse_combining_expression(start, end);
            }
            result
        }

        fn parse_expression(&mut self, start: usize, end: usize) -> Option<Box<dyn Expression>> {
            if start >= end {
                return self.error(start, sprintf!("Missing argument at index %u", start + 1));
            }
            let argc = end - start;
            match argc {
                0 => {
                    panic!("argc should not be zero"); // should have been caught by the above test
                }
                1 => self.error(
                    start + 1,
                    sprintf!("Missing argument at index %u", start + 2),
                ),
                2 => self.parse_unary_expression(start, end),
                3 => self.parse_3_arg_expression(start, end),
                4 => self.parse_4_arg_expression(start, end),
                _ => self.parse_combining_expression(start, end),
            }
        }

        pub fn parse_args(
            args: &[WString],
            err: &mut WString,
            program_name: &wstr,
        ) -> Option<Box<dyn Expression>> {
            // Empty list and one-arg list should be handled by caller.
            assert!(args.len() > 1);

            let mut parser = TestParser {
                strings: args,
                errors: Vec::new(),
                error_idx: 0,
            };
            let mut result = parser.parse_expression(0, args.len());

            // Historic assumption from C++: if we have no errors then we must have a result.
            assert!(!parser.errors.is_empty() || result.is_some());

            // Handle errors.
            // For now we only show the first error.
            if !parser.errors.is_empty() || result.as_ref().unwrap().range().end < args.len() {
                let mut narg = 0;
                let mut len_to_err = 0;
                if parser.errors.is_empty() {
                    parser.error_idx = result.as_ref().unwrap().range().end;
                }
                let mut commandline = WString::new();
                for arg in args {
                    if narg > 0 {
                        commandline.push(' ');
                    }
                    commandline.push_utfstr(arg);
                    narg += 1;
                    if narg == parser.error_idx {
                        len_to_err = fish_wcswidth(&commandline);
                    }
                }
                err.push_utfstr(program_name);
                err.push_str(": ");
                if !parser.errors.is_empty() {
                    err.push_utfstr(&parser.errors[0]);
                } else {
                    sprintf!(=> err, "unexpected argument at index %lu: '%ls'",
                             result.as_ref().unwrap().range().end + 1,
                             args[result.as_ref().unwrap().range().end]);
                }
                err.push('\n');
                err.push_utfstr(&commandline);
                err.push('\n');
                err.push_utfstr(&sprintf!("%*ls%ls\n", len_to_err + 1, " ", "^"));
            }

            if result.is_some() {
                // It's also an error if there are any unused arguments. This is not detected by
                // parse_expression().
                assert!(result.as_ref().unwrap().range().end <= args.len());
                if result.as_ref().unwrap().range().end < args.len() {
                    result = None;
                }
            }
            result
        }
    }

    // Parse a double from arg.
    fn parse_double(argstr: &wstr) -> Result<f64, Error> {
        let mut arg = argstr;

        // Consume leading spaces.
        while !arg.is_empty() && arg.char_at(0).is_whitespace() {
            arg = arg.slice_from(1);
        }
        if arg.is_empty() {
            return Err(Error::Empty);
        }
        let mut consumed = 0;
        let res = wcstod(arg, '.', &mut consumed)?;

        // Consume trailing spaces.
        let mut end = arg.slice_from(consumed);
        while !end.is_empty() && end.char_at(0).is_whitespace() {
            end = end.slice_from(1);
        }
        if end.len() < argstr.len() && end.is_empty() {
            Ok(res)
        } else {
            Err(Error::InvalidChar)
        }
    }

    // IEEE 1003.1 says nothing about what it means for two strings to be "algebraically equal". For
    // example, should we interpret 0x10 as 0, 10, or 16? Here we use only base 10 and use wcstoll,
    // which allows for leading + and -, and whitespace. This is consistent, albeit a bit more lenient
    // since we allow trailing whitespace, with other implementations such as bash.
    fn parse_number(arg: &wstr, number: &mut Number, errors: &mut Vec<WString>) -> bool {
        let floating = parse_double(arg);
        let integral: Result<i64, Error> = fish_wcstol(arg);
        let got_int = integral.is_ok();
        if got_int {
            // Here the value is just an integer; ignore the floating point parse because it may be
            // invalid (e.g. not a representable integer).
            *number = Number::new(integral.unwrap(), 0.0);
            true
        } else if floating.is_ok()
            && integral.unwrap_err() != Error::Overflow
            && floating.unwrap().is_finite()
        {
            // Here we parsed an (in range) floating point value that could not be parsed as an integer.
            // Break the floating point value into base and delta. Ensure that base is <= the floating
            // point value.
            //
            // Note that a non-finite number like infinity or NaN doesn't work for us, so we checked
            // above.
            let floating = floating.unwrap();
            let intpart = floating.floor();
            let delta = floating - intpart;
            *number = Number::new(intpart as i64, delta);
            true
        } else {
            // We could not parse a float or an int.
            // Check for special fish_wcsto* value or show standard EINVAL/ERANGE error.
            // TODO: the C++ here was pretty confusing. In particular we used an errno of -1 to mean
            // "invalid char" but the input string may be something like "inf".
            if integral == Err(Error::InvalidChar) && floating.is_err() {
                // Historically fish has printed a special message if a prefix of the invalid string was an integer.
                // Compute that now.
                let options = Options {
                    mradix: Some(10),
                    ..Default::default()
                };
                if let Ok(prefix_int) = wcstoi_opts(arg, options) {
                    let _: i64 = prefix_int; // to help type inference
                    errors.push(wgettext_fmt!(
                        "Integer %lld in '%ls' followed by non-digit",
                        prefix_int,
                        arg
                    ));
                } else {
                    errors.push(wgettext_fmt!("Argument is not a number: '%ls'", arg));
                }
            } else if floating.is_ok() && floating.unwrap().is_nan() {
                // NaN is an error as far as we're concerned.
                errors.push(wgettext!("Not a number").to_owned());
            } else if floating.is_ok() && floating.unwrap().is_infinite() {
                errors.push(wgettext!("Number is infinite").to_owned());
            } else if integral == Err(Error::Overflow) {
                errors.push(wgettext_fmt!("Result too large: %ls", arg));
            } else {
                errors.push(wgettext_fmt!("Invalid number: %ls", arg));
            }
            false
        }
    }

    fn binary_primary_evaluate(
        token: Token,
        left: &wstr,
        right: &wstr,
        errors: &mut Vec<WString>,
    ) -> bool {
        let mut ln = Number::default();
        let mut rn = Number::default();
        match token {
            Token::string_equal => left == right,
            Token::string_not_equal => left != right,
            Token::file_newer => file_id_for_path(right).older_than(&file_id_for_path(left)),
            Token::file_older => file_id_for_path(left).older_than(&file_id_for_path(right)),
            Token::file_same => file_id_for_path(left) == file_id_for_path(right),
            Token::number_equal => {
                parse_number(left, &mut ln, errors)
                    && parse_number(right, &mut rn, errors)
                    && ln == rn
            }
            Token::number_not_equal => {
                parse_number(left, &mut ln, errors)
                    && parse_number(right, &mut rn, errors)
                    && ln != rn
            }
            Token::number_greater => {
                parse_number(left, &mut ln, errors)
                    && parse_number(right, &mut rn, errors)
                    && ln > rn
            }
            Token::number_greater_equal => {
                parse_number(left, &mut ln, errors)
                    && parse_number(right, &mut rn, errors)
                    && ln >= rn
            }
            Token::number_lesser => {
                parse_number(left, &mut ln, errors)
                    && parse_number(right, &mut rn, errors)
                    && ln < rn
            }
            Token::number_lesser_equal => {
                parse_number(left, &mut ln, errors)
                    && parse_number(right, &mut rn, errors)
                    && ln <= rn
            }
            _ => {
                errors.push(L!("Unknown token type in binary_primary_evaluate").to_owned());
                false
            }
        }
    }

    fn unary_primary_evaluate(
        token: Token,
        arg: &wstr,
        streams: &mut IoStreams<'_>,
        errors: &mut Vec<WString>,
    ) -> bool {
        const S_ISGID: u32 = 0o2000;
        const S_ISVTX: u32 = 0o1000;

        // Helper to call wstat and then apply a function to the result.
        fn stat_and<F>(arg: &wstr, f: F) -> bool
        where
            F: FnOnce(std::fs::Metadata) -> bool,
        {
            wstat(arg).map_or(false, f)
        }

        match token {
            Token::filetype_b => {
                // "-b", for block special files
                stat_and(arg, |buf| buf.file_type().is_block_device())
            }
            Token::filetype_c => {
                // "-c", for character special files
                stat_and(arg, |buf: std::fs::Metadata| {
                    buf.file_type().is_char_device()
                })
            }
            Token::filetype_d => {
                // "-d", for directories
                stat_and(arg, |buf: std::fs::Metadata| buf.file_type().is_dir())
            }
            Token::filetype_e => {
                // "-e", for files that exist
                stat_and(arg, |_| true)
            }
            Token::filetype_f => {
                // "-f", for regular files
                stat_and(arg, |buf| buf.file_type().is_file())
            }
            Token::filetype_G => {
                // "-G", for check effective group id
                // Safety: getegid cannot fail.
                stat_and(arg, |buf| unsafe { libc::getegid() } == buf.gid())
            }
            Token::filetype_g => {
                // "-g", for set-group-id
                stat_and(arg, |buf| buf.permissions().mode() & S_ISGID != 0)
            }
            Token::filetype_h | Token::filetype_L => {
                // "-h", for symbolic links
                // "-L", same as -h
                lwstat(arg).map_or(false, |buf| buf.file_type().is_symlink())
            }
            Token::filetype_k => {
                // "-k", for sticky bit
                stat_and(arg, |buf| buf.permissions().mode() & S_ISVTX != 0)
            }
            Token::filetype_O => {
                // "-O", for check effective user id
                stat_and(
                    arg,
                    |buf: std::fs::Metadata| unsafe { libc::geteuid() } == buf.uid(),
                )
            }
            Token::filetype_p => {
                // "-p", for FIFO
                stat_and(arg, |buf: std::fs::Metadata| buf.file_type().is_fifo())
            }
            Token::filetype_S => {
                // "-S", socket
                stat_and(arg, |buf| buf.file_type().is_socket())
            }
            Token::filesize_s => {
                // "-s", size greater than zero
                stat_and(arg, |buf| buf.len() > 0)
            }
            Token::filedesc_t => {
                // "-t", whether the fd is associated with a terminal
                let mut num = Number::default();
                parse_number(arg, &mut num, errors) && num.isatty(streams)
            }
            Token::fileperm_r => {
                // "-r", read permission
                waccess(arg, libc::R_OK) == 0
            }
            Token::fileperm_u => {
                // "-u", whether file is setuid
                #[allow(clippy::unnecessary_cast)]
                stat_and(arg, |buf| {
                    buf.permissions().mode() & (libc::S_ISUID as u32) != 0
                })
            }
            Token::fileperm_w => {
                // "-w", whether file write permission is allowed
                waccess(arg, libc::W_OK) == 0
            }
            Token::fileperm_x => {
                // "-x", whether file execute/search is allowed
                waccess(arg, libc::X_OK) == 0
            }
            Token::string_n => {
                // "-n", non-empty string
                !arg.is_empty()
            }
            Token::string_z => {
                // "-z", true if length of string is 0
                arg.is_empty()
            }
            _ => {
                // Unknown token.
                errors.push(L!("Unknown token type in unary_primary_evaluate").to_owned());
                false
            }
        }
    }
}
/// Evaluate a conditional expression given the arguments. For POSIX conformance this
/// supports a more limited range of functionality.
/// Return status is the final shell status, i.e. 0 for true, 1 for false and 2 for error.
pub fn test(parser: &Parser, streams: &mut IoStreams<'_>, argv: &mut [WString]) -> Option<c_int> {
    // The first argument should be the name of the command ('test').
    if argv.is_empty() {
        return STATUS_INVALID_ARGS;
    }

    // Whether we are invoked with bracket '[' or not.
    let program_name = &argv[0];
    let is_bracket = program_name == "[";

    let mut argc = argv.len() - 1;

    // If we're bracket, the last argument ought to be ]; we ignore it. Note that argc is the number
    // of arguments after the command name; thus argv[argc] is the last argument.
    if is_bracket {
        if argv[argc] == "]" {
            // Ignore the closing bracket from now on.
            argc -= 1;
        } else {
            streams
                .err
                .appendln(wgettext!("[: the last argument must be ']'"));
            builtin_print_error_trailer(parser, streams.err, program_name);
            return STATUS_INVALID_ARGS;
        }
    }

    // Collect the arguments into a list.
    let args: Vec<WString> = argv[1..argc + 1].iter().map(|arg| arg.to_owned()).collect();

    if argc == 0 {
        return STATUS_INVALID_ARGS; // Per 1003.1, exit false.
    } else if argc == 1 {
        // Per 1003.1, exit true if the arg is non-empty.
        return if args[0].is_empty() {
            STATUS_CMD_ERROR
        } else {
            STATUS_CMD_OK
        };
    }

    // Try parsing
    let mut err = WString::new();
    let expr = test_expressions::TestParser::parse_args(&args, &mut err, program_name);
    let Some(expr) = expr else {
        streams.err.append(&err);
        streams.err.append(&parser.current_line());
        return STATUS_CMD_ERROR;
    };

    let mut eval_errors = Vec::new();
    let result = expr.evaluate(streams, &mut eval_errors);
    if !eval_errors.is_empty() {
        if !common::should_suppress_stderr_for_tests() {
            for eval_error in eval_errors {
                streams.err.appendln(&eval_error);
            }
            // Add a backtrace but not the "see help" message
            // because this isn't about passing the wrong options.
            streams.err.append(&parser.current_line());
        }
        return STATUS_INVALID_ARGS;
    }

    if result {
        STATUS_CMD_OK
    } else {
        STATUS_CMD_ERROR
    }
}
