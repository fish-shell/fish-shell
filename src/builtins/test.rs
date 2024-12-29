use super::prelude::*;
use crate::common;
use crate::future_feature_flags::{feature_test, FeatureFlag};
use crate::should_flog;

mod test_expressions {
    use super::*;

    use crate::nix::isatty;
    use crate::wutil::{
        file_id_for_path, fish_wcswidth, lwstat, waccess, wcstod::wcstod, wcstoi_opts, wstat,
        Error, Options,
    };
    use once_cell::sync::Lazy;
    use std::collections::HashMap;
    use std::os::unix::prelude::*;

    #[derive(Copy, Clone, PartialEq, Eq)]
    pub(super) enum Token {
        Unknown,                         // Arbitrary string
        Unary(UnaryToken),               // Takes one string/file
        Binary(BinaryToken),             // Takes two strings/files/numbers
        UnaryBoolean(UnaryBooleanToken), // Unary truth function
        BinaryBoolean(Combiner),         // Binary truth function
        ParenOpen,                       // (
        ParenClose,                      // )
    }

    impl From<BinaryToken> for Token {
        fn from(value: BinaryToken) -> Self {
            Self::Binary(value)
        }
    }

    impl From<UnaryToken> for Token {
        fn from(value: UnaryToken) -> Self {
            Self::Unary(value)
        }
    }

    #[derive(Copy, Clone, PartialEq, Eq)]
    pub(super) enum UnaryBooleanToken {
        Bang, // "!", inverts sense
    }

    #[derive(Copy, Clone, PartialEq, Eq)]
    pub(super) enum Combiner {
        And, // "-a", true if left and right are both true
        Or,  // "-o", true if either left or right is true
    }

    macro_rules! define_token {
        (
            enum $enum:ident;
            $(
                $variant:ident($sub_type:ident) {
                    $($sub_variant:ident)+
                }
            )*
        ) => {
            #[derive(Copy, Clone, PartialEq, Eq)]
            pub(super) enum $enum {
                $($variant($sub_type),)*
            }

            $(
                #[derive(Copy, Clone, PartialEq, Eq)]
                pub(super) enum $sub_type { $($sub_variant,)+ }

                impl From<$sub_type> for Token {
                    fn from(value: $sub_type) -> Token {
                        $enum::$variant(value).into()
                    }
                }
            )*
        };
    }

    define_token! {
        enum UnaryToken;

        // based on stat()
        FileStat(StatPredicate) {
            b // "-b", for block special files
            c // "-c", for character special files
            d // "-d", for directories
            e // "-e", for files that exist
            f // "-f", for for regular files
            G // "-G", for check effective group id
            g // "-g", for set-group-id
            k // "-k", for sticky bit
            O // "-O", for check effective user id
            p // "-p", for FIFO
            S // "-S", socket
            s // "-s", size greater than zero
            u // "-u", whether file is setuid
        }

        // based on access()
        FilePerm(FilePermission) {
            r // "-r", read permission
            w // "-w", whether file write permission is allowed
            x // "-x", whether file execute/search is allowed
        }

        // miscellaneous
        FileType(FilePredicate) {
            h // "-h", for symbolic links
            L // "-L", same as -h
            t // "-t", whether the fd is associated with a terminal
        }

        String(StringPredicate) {
            n // "-n", non-empty string
            z // "-z", true if length of string is 0
        }
    }

    define_token! {
        enum BinaryToken;

        // based on inode + more distinguishing info (see FileId struct)
        FileId(FileComparison) {
            Newer // f1 -nt f2, true if f1 exists and is newer than f2, or there is no f2
            Older // f1 -ot f2, true if f2 exists and f1 does not, or f1 is older than f2
            Same  // f1 -ef f2, true if f1 and f2 exist and refer to same file
        }

        String(StringComparison) {
            Equal    // "=", true if strings are identical
            NotEqual // "!=", true if strings are not identical
        }

        Number(NumberComparison) {
            Equal        // "-eq", true if numbers are equal
            NotEqual     // "-ne", true if numbers are not equal
            Greater      // "-gt", true if first number is larger than second
            GreaterEqual // "-ge", true if first number is at least second
            Lesser       // "-lt", true if first number is smaller than second
            LesserEqual  // "-le", true if first number is at most second
        }
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
        fn isatty(&self, streams: &mut IoStreams) -> bool {
            if self.delta != 0.0 || self.base > i32::MAX as i64 || self.base < i32::MIN as i64 {
                return false;
            }
            let bint = self.base as i32;
            if bint == 0 {
                match streams.stdin_fd {
                    -1 => false,
                    fd => isatty(fd),
                }
            } else if bint == 1 {
                !streams.out_is_redirected && isatty(libc::STDOUT_FILENO)
            } else if bint == 2 {
                !streams.err_is_redirected && isatty(libc::STDERR_FILENO)
            } else {
                isatty(bint)
            }
        }
    }

    fn token_for_string(str: &wstr) -> Token {
        TOKEN_INFOS.get(str).copied().unwrap_or(Token::Unknown)
    }

    static TOKEN_INFOS: Lazy<HashMap<&'static wstr, Token>> = Lazy::new(|| {
        let pairs = [
            (L!(""), Token::Unknown),
            (L!("!"), Token::UnaryBoolean(UnaryBooleanToken::Bang)),
            (L!("-b"), StatPredicate::b.into()),
            (L!("-c"), StatPredicate::c.into()),
            (L!("-d"), StatPredicate::d.into()),
            (L!("-e"), StatPredicate::e.into()),
            (L!("-f"), StatPredicate::f.into()),
            (L!("-G"), StatPredicate::G.into()),
            (L!("-g"), StatPredicate::g.into()),
            (L!("-h"), FilePredicate::h.into()),
            (L!("-k"), StatPredicate::k.into()),
            (L!("-L"), FilePredicate::L.into()),
            (L!("-O"), StatPredicate::O.into()),
            (L!("-p"), StatPredicate::p.into()),
            (L!("-S"), StatPredicate::S.into()),
            (L!("-s"), StatPredicate::s.into()),
            (L!("-t"), FilePredicate::t.into()),
            (L!("-r"), FilePermission::r.into()),
            (L!("-u"), StatPredicate::u.into()),
            (L!("-w"), FilePermission::w.into()),
            (L!("-x"), FilePermission::x.into()),
            (L!("-n"), StringPredicate::n.into()),
            (L!("-z"), StringPredicate::z.into()),
            (L!("="), StringComparison::Equal.into()),
            (L!("!="), StringComparison::NotEqual.into()),
            (L!("-nt"), FileComparison::Newer.into()),
            (L!("-ot"), FileComparison::Older.into()),
            (L!("-ef"), FileComparison::Same.into()),
            (L!("-eq"), NumberComparison::Equal.into()),
            (L!("-ne"), NumberComparison::NotEqual.into()),
            (L!("-gt"), NumberComparison::Greater.into()),
            (L!("-ge"), NumberComparison::GreaterEqual.into()),
            (L!("-lt"), NumberComparison::Lesser.into()),
            (L!("-le"), NumberComparison::LesserEqual.into()),
            (L!("-a"), Token::BinaryBoolean(Combiner::And)),
            (L!("-o"), Token::BinaryBoolean(Combiner::Or)),
            (L!("("), Token::ParenOpen),
            (L!(")"), Token::ParenClose),
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
        fn evaluate(&self, streams: &mut IoStreams, errors: &mut Vec<WString>) -> bool;

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

    /// Something that is not a token of any other type.
    struct JustAString {
        arg: WString,
        range: Range,
    }

    /// Single argument like -n foo.
    struct UnaryPrimary {
        arg: WString,
        token: UnaryToken,
        range: Range,
    }

    /// Two argument primary like foo != bar.
    struct BinaryPrimary {
        arg_left: WString,
        arg_right: WString,
        token: BinaryToken,
        range: Range,
    }

    /// Unary operator like bang.
    struct UnaryOperator {
        subject: Box<dyn Expression>,
        token: UnaryBooleanToken,
        range: Range,
    }

    /// Combining expression. Contains a list of AND or OR expressions. It takes more than two so that
    /// we don't have to worry about precedence in the parser.
    struct CombiningExpression {
        subjects: Vec<Box<dyn Expression>>,
        combiners: Vec<Combiner>,
        range: Range,
    }

    /// Parenthentical expression.
    struct ParentheticalExpression {
        contents: Box<dyn Expression>,
        range: Range,
    }

    impl Expression for JustAString {
        fn evaluate(&self, _streams: &mut IoStreams, _errors: &mut Vec<WString>) -> bool {
            !self.arg.is_empty()
        }

        fn range(&self) -> Range {
            self.range.clone()
        }
    }

    impl Expression for UnaryPrimary {
        fn evaluate(&self, streams: &mut IoStreams, errors: &mut Vec<WString>) -> bool {
            unary_primary_evaluate(self.token, &self.arg, streams, errors)
        }

        fn range(&self) -> Range {
            self.range.clone()
        }
    }

    impl Expression for BinaryPrimary {
        fn evaluate(&self, _streams: &mut IoStreams, errors: &mut Vec<WString>) -> bool {
            binary_primary_evaluate(self.token, &self.arg_left, &self.arg_right, errors)
        }

        fn range(&self) -> Range {
            self.range.clone()
        }
    }

    impl Expression for UnaryOperator {
        fn evaluate(&self, streams: &mut IoStreams, errors: &mut Vec<WString>) -> bool {
            match self.token {
                UnaryBooleanToken::Bang => !self.subject.evaluate(streams, errors),
            }
        }

        fn range(&self) -> Range {
            self.range.clone()
        }
    }

    impl Expression for CombiningExpression {
        fn evaluate(&self, streams: &mut IoStreams, errors: &mut Vec<WString>) -> bool {
            let _res = self.subjects[0].evaluate(streams, errors);
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
                    if idx + 1 < max && self.combiners[idx] != Combiner::And {
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

        fn range(&self) -> Range {
            self.range.clone()
        }
    }

    impl Expression for ParentheticalExpression {
        fn evaluate(&self, streams: &mut IoStreams, errors: &mut Vec<WString>) -> bool {
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
            if let Token::UnaryBoolean(token) = token_for_string(self.arg(start)) {
                let subject = self.parse_unary_expression(start + 1, end)?;
                let range = start..subject.range().end;
                return UnaryOperator {
                    subject,
                    token,
                    range,
                }
                .into_some_box();
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
                    let Token::BinaryBoolean(combiner) = token_for_string(self.arg(idx)) else {
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
                    };
                    combiners.push(combiner);
                    idx += 1;
                }

                // Parse another expression.
                let Some(expr) = self.parse_unary_expression(idx, end) else {
                    self.add_error(idx, sprintf!("Missing argument at index %u", idx + 1));
                    if !first {
                        // Clean up the dangling combiner, since it never got its right hand expression.
                        combiners.pop();
                    }
                    break;
                };
                // Go to the end of this expression.
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
            let Token::Unary(token) = token_for_string(self.arg(start)) else {
                return None;
            };
            UnaryPrimary {
                arg: self.arg(start + 1).to_owned(),
                token,
                range: start..start + 2,
            }
            .into_some_box()
        }

        fn parse_just_a_string(&mut self, start: usize, end: usize) -> Option<Box<dyn Expression>> {
            // Handle a string as a unary primary that is not a token of any other type.
            // e.g. 'test foo -a bar' should evaluate to true.

            // We need one argument.
            if start >= end {
                return self.error(start, sprintf!("Missing argument at index %u", start + 1));
            }

            let tok = token_for_string(self.arg(start));
            if tok != Token::Unknown {
                return self.error(
                    start,
                    sprintf!("Unexpected argument type at index %u", start + 1),
                );
            }

            if feature_test(FeatureFlag::test_require_arg) {
                return self.error(start, sprintf!("Unknown option at index %u", start));
            }

            return JustAString {
                arg: self.arg(start).to_owned(),
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
            let Token::Binary(token) = token_for_string(self.arg(start + 1)) else {
                return None;
            };
            BinaryPrimary {
                arg_left: self.arg(start).to_owned(),
                arg_right: self.arg(start + 2).to_owned(),
                token,
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
            if token_for_string(self.arg(start)) != Token::ParenOpen {
                return None;
            }

            // Parse a subexpression.
            let subexpr = self.parse_expression(start + 1, end)?;

            // Parse a close paren.
            let close_index = subexpr.range().end;
            assert!(close_index <= end);
            if close_index == end {
                return self.error(
                    close_index,
                    sprintf!("Missing close paren at index %u", close_index + 1),
                );
            }
            if token_for_string(self.arg(close_index)) != Token::ParenClose {
                return self.error(
                    close_index,
                    sprintf!("Expected close paren at index %u", close_index + 1),
                );
            }

            // Success.
            ParentheticalExpression {
                contents: subexpr,
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

            let center_token = token_for_string(self.arg(start + 1));

            if matches!(center_token, Token::Binary(_)) {
                self.parse_binary_primary(start, end)
            } else if let Token::BinaryBoolean(combiner) = center_token {
                let left = self.parse_unary_expression(start, start + 1)?;
                let right = self.parse_unary_expression(start + 2, start + 3)?;
                // Transfer ownership to the vector of subjects.
                CombiningExpression {
                    subjects: vec![left, right],
                    combiners: vec![combiner],
                    range: start..end,
                }
                .into_some_box()
            } else {
                self.parse_unary_expression(start, end)
            }
        }

        fn parse_4_arg_expression(
            &mut self,
            start: usize,
            end: usize,
        ) -> Option<Box<dyn Expression>> {
            assert!(end - start == 4);

            let first_token = token_for_string(self.arg(start));

            if let Token::UnaryBoolean(token) = first_token {
                let subject = self.parse_3_arg_expression(start + 1, end)?;
                UnaryOperator {
                    subject,
                    token,
                    range: start..end,
                }
                .into_some_box()
            } else if first_token == Token::ParenOpen {
                self.parse_parenthetical(start, end)
            } else {
                self.parse_combining_expression(start, end)
            }
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
        if let Ok(int) = integral {
            // Here the value is just an integer; ignore the floating point parse because it may be
            // invalid (e.g. not a representable integer).
            *number = Number::new(int, 0.0);
            true
        } else if floating.is_ok_and(|f| f.is_finite())
            && integral.is_err_and(|i| i != Error::Overflow)
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
            } else if floating.is_ok_and(|x| x.is_nan()) {
                // NaN is an error as far as we're concerned.
                errors.push(wgettext!("Not a number").to_owned());
            } else if floating.is_ok_and(|x| x.is_infinite()) {
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
        token: BinaryToken,
        left: &wstr,
        right: &wstr,
        errors: &mut Vec<WString>,
    ) -> bool {
        match token {
            BinaryToken::String(StringComparison::Equal) => left == right,
            BinaryToken::String(StringComparison::NotEqual) => left != right,
            BinaryToken::FileId(comparison) => {
                let left = file_id_for_path(left);
                let right = file_id_for_path(right);
                match comparison {
                    FileComparison::Newer => right.older_than(&left),
                    FileComparison::Older => left.older_than(&right),
                    FileComparison::Same => left == right,
                }
            }
            BinaryToken::Number(comparison) => {
                let mut ln = Number::default();
                let mut rn = Number::default();
                if !parse_number(left, &mut ln, errors) || !parse_number(right, &mut rn, errors) {
                    return false;
                }
                match comparison {
                    NumberComparison::Equal => ln == rn,
                    NumberComparison::NotEqual => ln != rn,
                    NumberComparison::Greater => ln > rn,
                    NumberComparison::GreaterEqual => ln >= rn,
                    NumberComparison::Lesser => ln < rn,
                    NumberComparison::LesserEqual => ln <= rn,
                }
            }
        }
    }

    fn unary_primary_evaluate(
        token: UnaryToken,
        arg: &wstr,
        streams: &mut IoStreams,
        errors: &mut Vec<WString>,
    ) -> bool {
        match token {
            #[allow(clippy::unnecessary_cast)] // mode_t is u32 on many platforms, but not all
            UnaryToken::FileStat(stat_token) => {
                let Ok(md) = wstat(arg) else {
                    return false;
                };

                const S_ISUID: u32 = libc::S_ISUID as u32;
                const S_ISGID: u32 = libc::S_ISGID as u32;
                const S_ISVTX: u32 = libc::S_ISVTX as u32;

                match stat_token {
                    // "-b", for block special files
                    StatPredicate::b => md.file_type().is_block_device(),
                    // "-c", for character special files
                    StatPredicate::c => md.file_type().is_char_device(),
                    // "-d", for directories
                    StatPredicate::d => md.file_type().is_dir(),
                    // "-e", for files that exist
                    StatPredicate::e => true,
                    // "-f", for regular files
                    StatPredicate::f => md.file_type().is_file(),
                    // "-G", for check effective group id
                    StatPredicate::G => md.gid() == crate::nix::getegid(),
                    // "-g", for set-group-id
                    StatPredicate::g => md.permissions().mode() & S_ISGID != 0,
                    // "-k", for sticky bit
                    StatPredicate::k => md.permissions().mode() & S_ISVTX != 0,
                    // "-O", for check effective user id
                    StatPredicate::O => md.uid() == crate::nix::geteuid(),
                    // "-p", for FIFO
                    StatPredicate::p => md.file_type().is_fifo(),
                    // "-S", socket
                    StatPredicate::S => md.file_type().is_socket(),
                    // "-s", size greater than zero
                    StatPredicate::s => md.len() > 0,
                    // "-u", whether file is setuid
                    StatPredicate::u => md.permissions().mode() & S_ISUID != 0,
                }
            }
            UnaryToken::FileType(file_type) => {
                match file_type {
                    // "-h", for symbolic links
                    // "-L", same as -h
                    FilePredicate::h | FilePredicate::L => {
                        lwstat(arg).is_ok_and(|md| md.file_type().is_symlink())
                    }
                    // "-t", whether the fd is associated with a terminal
                    FilePredicate::t => {
                        let mut num = Number::default();
                        parse_number(arg, &mut num, errors) && num.isatty(streams)
                    }
                }
            }
            UnaryToken::FilePerm(permission) => {
                let mode = match permission {
                    // "-r", read permission
                    FilePermission::r => libc::R_OK,
                    // "-w", whether file write permission is allowed
                    FilePermission::w => libc::W_OK,
                    // "-x", whether file execute/search is allowed
                    FilePermission::x => libc::X_OK,
                };
                waccess(arg, mode) == 0
            }
            UnaryToken::String(predicate) => match predicate {
                // "-n", non-empty string
                StringPredicate::n => !arg.is_empty(),
                // "-z", true if length of string is 0
                StringPredicate::z => arg.is_empty(),
            },
        }
    }
}
/// Evaluate a conditional expression given the arguments. For POSIX conformance this
/// supports a more limited range of functionality.
/// Return status is the final shell status, i.e. 0 for true, 1 for false and 2 for error.
pub fn test(parser: &Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> BuiltinResult {
    // The first argument should be the name of the command ('test').
    if argv.is_empty() {
        return Err(STATUS_INVALID_ARGS);
    }

    // Whether we are invoked with bracket '[' or not.
    let program_name = argv[0];
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
            return Err(STATUS_INVALID_ARGS);
        }
    }

    // Collect the arguments into a list.
    let args: Vec<WString> = argv[1..argc + 1]
        .iter()
        .map(|&arg| arg.to_owned())
        .collect();
    let args: &[WString] = &args;

    if feature_test(FeatureFlag::test_require_arg) {
        if argc == 0 {
            streams.err.appendln(wgettext_fmt!(
                "%ls: Expected at least one argument",
                program_name
            ));
            builtin_print_error_trailer(parser, streams.err, program_name);
            return Err(STATUS_INVALID_ARGS);
        } else if argc == 1 {
            if args[0] == "-n" {
                return Err(STATUS_CMD_ERROR);
            } else if args[0] == "-z" {
                return Ok(SUCCESS);
            }
        }
    } else if argc == 0 {
        if should_flog!(deprecated_test) {
            streams.err.appendln(wgettext_fmt!(
                "%ls: called with no arguments. This will be an error in future.",
                program_name
            ));
            streams.err.append(parser.current_line());
        }
        return Err(STATUS_INVALID_ARGS); // Per 1003.1, exit false.
    } else if argc == 1 {
        if should_flog!(deprecated_test) {
            if args[0] != "-z" {
                streams.err.appendln(wgettext_fmt!(
                    "%ls: called with one argument. This will return false in future.",
                    program_name
                ));
                streams.err.append(parser.current_line());
            }
        }
        // Per 1003.1, exit true if the arg is non-empty.
        return if args[0].is_empty() {
            Err(STATUS_CMD_ERROR)
        } else {
            Ok(SUCCESS)
        };
    }

    // Try parsing
    let mut err = WString::new();
    let expr = test_expressions::TestParser::parse_args(args, &mut err, program_name);
    let Some(expr) = expr else {
        streams.err.append(err);
        streams.err.append(parser.current_line());
        return Err(STATUS_CMD_ERROR);
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
            streams.err.append(parser.current_line());
        }
        return Err(STATUS_INVALID_ARGS);
    }

    if result {
        Ok(SUCCESS)
    } else {
        Err(STATUS_CMD_ERROR)
    }
}
