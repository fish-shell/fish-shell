/*
 * TINYEXPR - Tiny recursive descent parser and evaluation engine in C
 *
 * Copyright (c) 2015, 2016 Lewis Van Winkle
 *
 * http://CodePlea.com
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgement in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

// This version has been altered and ported to C++, then to Rust, for inclusion in fish.

use std::{
    f64::consts::{E, PI, TAU},
    fmt::Debug,
    ops::{BitAnd, BitOr, BitXor},
};

use crate::{
    wchar::prelude::*,
    wutil::{wcstod::wcstod_underscores, wgettext, Error as wcstodError},
};

#[derive(Clone, Copy)]
enum Function {
    Constant(f64),
    Fn1(fn(f64) -> f64),
    Fn2(fn(f64, f64) -> f64),
    FnN(fn(&[f64]) -> f64),
}

impl Debug for Function {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let variant = match self {
            Function::Constant(n) => return f.debug_tuple("Function::Constant").field(n).finish(),
            Function::Fn1(_) => "Fn1",
            Function::Fn2(_) => "Fn2",
            Function::FnN(_) => "FnN",
        };

        write!(f, "Function::{variant}(_)")
    }
}

impl Function {
    pub fn arity(&self) -> Option<usize> {
        match self {
            Function::Constant(_) => Some(0),
            Function::Fn1(_) => Some(1),
            Function::Fn2(_) => Some(2),
            Function::FnN(_) => None,
        }
    }

    pub fn call(&self, args: &[f64]) -> f64 {
        match (self, args) {
            (Function::Constant(n), []) => *n,
            (Function::Fn1(f), [a]) => f(*a),
            (Function::Fn2(f), [a, b]) => f(*a, *b),
            (Function::FnN(f), args) => f(args),
            (_, _) => panic!("Incorrect number of arguments for function call"),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ErrorKind {
    UnknownFunction,
    MissingClosingParen,
    TooFewArgs,
    TooManyArgs,
    MissingOperator,
    UnexpectedToken,
    LogicalOperator,
    DivByZero,
    NumberTooLarge,
    Unknown,
}

impl ErrorKind {
    pub fn describe_wstr(&self) -> &'static wstr {
        match self {
            ErrorKind::UnknownFunction => wgettext!("Unknown function"),
            ErrorKind::MissingClosingParen => wgettext!("Missing closing parenthesis"),
            ErrorKind::TooFewArgs => wgettext!("Too few arguments"),
            ErrorKind::TooManyArgs => wgettext!("Too many arguments"),
            ErrorKind::MissingOperator => wgettext!("Missing operator"),
            ErrorKind::UnexpectedToken => wgettext!("Unexpected token"),
            ErrorKind::LogicalOperator => {
                wgettext!("Logical operations are not supported, use `test` instead")
            }
            ErrorKind::DivByZero => wgettext!("Division by zero"),
            ErrorKind::NumberTooLarge => wgettext!("Number is too large"),
            ErrorKind::Unknown => wgettext!("Expression is bogus"),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Error {
    pub kind: ErrorKind,
    pub position: usize,
    pub len: usize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Operator {
    Add,
    Sub,
    Mul,
    Div,
    Pow,
    Rem,
}

impl Operator {
    pub fn eval(&self, a: f64, b: f64) -> f64 {
        match self {
            Operator::Add => a + b,
            Operator::Sub => a - b,
            Operator::Mul => a * b,
            Operator::Div => a / b,
            Operator::Pow => a.powf(b),
            Operator::Rem => a % b,
        }
    }
}

#[derive(Debug, Clone, Copy)]
enum Token {
    Error,
    End,
    Sep,
    Open,
    Close,
    Number(f64),
    Function(Function),
    Infix(Operator),
}

struct State<'s> {
    start: &'s wstr,
    pos: usize,
    current: Token,
    error: Option<Error>,
}

fn bitwise_op(a: f64, b: f64, f: fn(u64, u64) -> u64) -> f64 {
    // TODO: bounds checks
    let a = a as u64;
    let b = b as u64;

    let result = f(a, b);

    // TODO: bounds checks
    result as f64
}

fn fac(n: f64) -> f64 {
    if n < 0.0 {
        return f64::NAN;
    }
    if n > (u64::MAX as f64) {
        return f64::INFINITY;
    }

    let n = n as u64;

    (1..=n)
        .try_fold(1_u64, |acc, i| acc.checked_mul(i))
        .map_or(f64::INFINITY, |x| x as f64)
}

fn maximum(n: &[f64]) -> f64 {
    n.iter().fold(f64::NEG_INFINITY, |a, &b| {
        if a.is_nan() {
            return a;
        }
        if b.is_nan() {
            return b;
        }

        if a == b {
            // treat +0 as larger than -0
            if a.is_sign_positive() {
                a
            } else {
                b
            }
        } else if a > b {
            a
        } else {
            b
        }
    })
}

fn minimum(n: &[f64]) -> f64 {
    n.iter().fold(f64::INFINITY, |a, &b| {
        if a.is_nan() {
            return a;
        }
        if b.is_nan() {
            return b;
        }

        if a == b {
            // treat -0 as smaller than +0
            if a.is_sign_negative() {
                a
            } else {
                b
            }
        } else if a < b {
            a
        } else {
            b
        }
    })
}

fn ncr(n: f64, r: f64) -> f64 {
    // Doing this for NAN takes ages - just return the result right away.
    if n.is_nan() {
        return f64::INFINITY;
    }
    if n < 0.0 || r < 0.0 || n < r {
        return f64::NAN;
    }
    if n > (u64::MAX as f64) || r > (u64::MAX as f64) {
        return f64::INFINITY;
    }

    let un = n as u64;
    let mut ur = r as u64;

    if ur > un / 2 {
        ur = un - ur
    };

    let mut result = 1_u64;
    for i in 1..=ur {
        let Some(next_result) = result.checked_mul(un - ur + i) else {
            return f64::INFINITY;
        };
        result = next_result / i;
    }

    result as f64
}

fn npr(n: f64, r: f64) -> f64 {
    ncr(n, r) * fac(r)
}

const BUILTINS: &[(&wstr, Function)] = &[
    // must be in alphabetical order
    (L!("abs"), Function::Fn1(f64::abs)),
    (L!("acos"), Function::Fn1(f64::acos)),
    (L!("asin"), Function::Fn1(f64::asin)),
    (L!("atan"), Function::Fn1(f64::atan)),
    (L!("atan2"), Function::Fn2(f64::atan2)),
    (
        L!("bitand"),
        Function::Fn2(|a, b| bitwise_op(a, b, BitAnd::bitand)),
    ),
    (
        L!("bitor"),
        Function::Fn2(|a, b| bitwise_op(a, b, BitOr::bitor)),
    ),
    (
        L!("bitxor"),
        Function::Fn2(|a, b| bitwise_op(a, b, BitXor::bitxor)),
    ),
    (L!("ceil"), Function::Fn1(f64::ceil)),
    (L!("cos"), Function::Fn1(f64::cos)),
    (L!("cosh"), Function::Fn1(f64::cosh)),
    (L!("e"), Function::Constant(E)),
    (L!("exp"), Function::Fn1(f64::exp)),
    (L!("fac"), Function::Fn1(fac)),
    (L!("floor"), Function::Fn1(f64::floor)),
    (L!("ln"), Function::Fn1(f64::ln)),
    (L!("log"), Function::Fn1(f64::log10)),
    (L!("log10"), Function::Fn1(f64::log10)),
    (L!("log2"), Function::Fn1(f64::log2)),
    (L!("max"), Function::FnN(maximum)),
    (L!("min"), Function::FnN(minimum)),
    (L!("ncr"), Function::Fn2(ncr)),
    (L!("npr"), Function::Fn2(npr)),
    (L!("pi"), Function::Constant(PI)),
    (L!("pow"), Function::Fn2(f64::powf)),
    (L!("round"), Function::Fn1(f64::round)),
    (L!("sin"), Function::Fn1(f64::sin)),
    (L!("sinh"), Function::Fn1(f64::sinh)),
    (L!("sqrt"), Function::Fn1(f64::sqrt)),
    (L!("tan"), Function::Fn1(f64::tan)),
    (L!("tanh"), Function::Fn1(f64::tanh)),
    (L!("tau"), Function::Constant(TAU)),
];

assert_sorted_by_name!(BUILTINS, 0);

fn find_builtin(name: &wstr) -> Option<Function> {
    let idx = BUILTINS
        .binary_search_by_key(&name, |(name, _expr)| name)
        .ok()?;

    Some(BUILTINS[idx].1)
}

impl<'s> State<'s> {
    pub fn new(input: &'s wstr) -> Self {
        let mut state = Self {
            start: input,
            pos: 0,
            current: Token::End,
            error: None,
        };
        state.next_token();
        state
    }

    pub fn error(&self) -> Result<(), Error> {
        if let Token::End = self.current {
            Ok(())
        } else if let Some(error) = self.error {
            Err(error)
        } else {
            // If we're not at the end but there's no error, then that means we have a
            // superfluous token that we have no idea what to do with.
            Err(Error {
                kind: ErrorKind::TooManyArgs,
                position: self.pos,
                len: 0,
            })
        }
    }

    pub fn eval(&mut self) -> f64 {
        return self.expr();
    }

    fn set_error(&mut self, kind: ErrorKind, pos_len: Option<(usize, usize)>) {
        self.current = Token::Error;
        let (position, len) = pos_len.unwrap_or((self.pos, 0));
        self.error = Some(Error {
            kind,
            position,
            len,
        });
    }

    fn no_specific_error(&self) -> bool {
        !matches!(self.current, Token::Error)
            || matches!(
                self.error,
                Some(Error {
                    kind: ErrorKind::Unknown,
                    ..
                })
            )
    }

    /// Tries to get the next token from the input. If the input does not contain enough data for
    /// another token, `None` is returned. Otherwise, the number of consumed characters is returned
    /// along with either the token, or `None` in case of ignored (whitespace) input.
    fn get_token(&mut self) -> Option<(usize, Option<Token>)> {
        debug_assert!(!matches!(self.current, Token::Error));

        let next = &self.start.as_char_slice().get(self.pos..)?;

        // Try reading a number.
        if matches!(next.first(), Some('0'..='9') | Some('.')) {
            let mut consumed = 0;
            match wcstod_underscores(*next, &mut consumed) {
                Ok(num) => Some((consumed, Some(Token::Number(num)))),
                Err(wcstodError::InvalidChar) => {
                    self.set_error(ErrorKind::Unknown, Some((self.pos + consumed, 1)));
                    return Some((consumed, Some(Token::Error)));
                }
                Err(wcstodError::Overflow) => {
                    self.set_error(ErrorKind::NumberTooLarge, Some((self.pos, consumed)));
                    return Some((consumed, Some(Token::Error)));
                }
                Err(wcstodError::Empty) => {
                    // We have a matches! above, this can't be?
                    unreachable!()
                }
            }
        } else {
            // Look for a function call.
            // But not when it's an "x" followed by whitespace
            // - that's the alternative multiplication operator.
            if next.first()?.is_ascii_lowercase()
                && !(*next.first()? == 'x' && next.len() > 1 && next[1].is_whitespace())
            {
                let ident_len = next
                    .iter()
                    .position(|&c| !(c.is_ascii_lowercase() || c.is_ascii_digit() || c == '_'))
                    .unwrap_or(next.len());

                let ident = &next[..ident_len];
                if let Some(var) = find_builtin(wstr::from_char_slice(ident)) {
                    return Some((ident_len, Some(Token::Function(var))));
                } else if self.no_specific_error() {
                    // Our error is more specific, so it takes precedence.
                    self.set_error(ErrorKind::UnknownFunction, Some((self.pos, ident_len)));
                }

                Some((ident_len, Some(Token::Error)))
            } else {
                // Look for an operator or special character.
                let tok = match next.first()? {
                    '+' => Token::Infix(Operator::Add),
                    '-' => Token::Infix(Operator::Sub),
                    'x' | '*' => Token::Infix(Operator::Mul),
                    '/' => Token::Infix(Operator::Div),
                    '^' => Token::Infix(Operator::Pow),
                    '%' => Token::Infix(Operator::Rem),
                    '(' => Token::Open,
                    ')' => Token::Close,
                    ',' => Token::Sep,
                    ' ' | '\t' | '\n' | '\r' => return Some((1, None)),
                    '=' | '>' | '<' | '&' | '|' | '!' => {
                        self.set_error(ErrorKind::LogicalOperator, None);
                        Token::Error
                    }
                    _ => {
                        self.set_error(ErrorKind::MissingOperator, None);
                        Token::Error
                    }
                };

                Some((1, Some(tok)))
            }
        }
    }

    fn next_token(&mut self) {
        self.current = loop {
            let Some((consumed, token)) = self.get_token() else {
                break Token::End;
            };

            self.pos += consumed;
            if let Some(token) = token {
                break token;
            }
        };
    }

    /// ```text
    /// <base>   = <constant> |
    ///            <function-0> {"(" ")"} |
    ///            <function-1> <power> |
    ///            <function-X> "(" <expr> {"," <expr>} ")" |
    ///            "(" <list> ")"
    /// ```
    fn base(&mut self) -> f64 {
        match self.current {
            Token::Number(n) => {
                let after_first = self.pos;

                self.next_token();
                if let Token::Number(_) | Token::Function(_) = self.current {
                    // Two numbers after each other:
                    // math '5 2'
                    // math '3 pi'
                    // (of course 3 pi could also be interpreted as 3 x pi)

                    // The error should be given *between*
                    // the last two tokens.
                    let num_whitespace = self.start[after_first..]
                        .chars()
                        .take_while(|&c| " \t\n\r".contains(c))
                        .count();

                    self.set_error(
                        ErrorKind::MissingOperator,
                        Some((after_first, num_whitespace)),
                    );
                }

                n
            }
            Token::Function(f) => {
                self.next_token();
                let have_open = matches!(self.current, Token::Open);
                if have_open {
                    // If we *have* an opening parenthesis,
                    // we need to consume it and
                    // expect a closing one.
                    self.next_token();
                }

                if f.arity() == Some(0) {
                    if have_open {
                        if let Token::Close = self.current {
                            self.next_token();
                        } else if self.no_specific_error() {
                            self.set_error(ErrorKind::MissingClosingParen, None);
                        }
                    }

                    return match f {
                        Function::Constant(n) => n,
                        _ => unreachable!("unhandled function type with arity 0"),
                    };
                }

                let mut parameters = vec![];
                let mut i = 0;
                let mut first_err = None;
                for j in 0.. {
                    if f.arity() == Some(j) {
                        first_err = Some(self.pos - 1);
                    }
                    parameters.push(self.expr());
                    if !matches!(self.current, Token::Sep) {
                        break;
                    }
                    self.next_token();
                    i += 1;
                }

                if f.arity().is_none() || f.arity() == Some(i + 1) {
                    if !have_open {
                        return f.call(&parameters);
                    }
                    if let Token::Close = self.current {
                        // We have an opening and a closing paren, consume the closing one and done.
                        self.next_token();
                        return f.call(&parameters);
                    }
                    if !matches!(self.current, Token::Error) {
                        // If we had the right number of arguments, we're missing a closing paren.
                        self.set_error(ErrorKind::MissingClosingParen, None);
                    }
                }

                if !matches!(self.current, Token::Error)
                    || matches!(
                        self.error,
                        Some(Error {
                            kind: ErrorKind::UnexpectedToken,
                            ..
                        })
                    )
                {
                    // Otherwise we complain about the number of arguments *first*,
                    // a closing parenthesis should be more obvious.
                    //
                    // Vararg functions need at least one argument.
                    let err = if f.arity().map(|arity| i < arity).unwrap_or(i == 0) {
                        ErrorKind::TooFewArgs
                    } else {
                        ErrorKind::TooManyArgs
                    };

                    let mut err_pos_len = None;
                    if let Some(first_err) = first_err {
                        let mut len = self.pos - first_err;
                        if !matches!(self.current, Token::Close) {
                            // TODO: Rationalize where we put the cursor exactly.
                            // If we have a closing paren it's on it, if we don't it's before the number.
                            len += 1;
                        }
                        if let Token::End = self.current {
                            // Don't place a caret after the end of string
                            len -= 1;
                        }
                        err_pos_len = Some((first_err, len));
                    }

                    self.set_error(err, err_pos_len);
                }

                f64::NAN
            }
            Token::Open => {
                self.next_token();
                let ret = self.expr();
                if let Token::Close = self.current {
                    self.next_token();
                    return ret;
                }

                if !matches!(self.current, Token::Error | Token::End) && self.error.is_none() {
                    self.set_error(ErrorKind::TooManyArgs, None)
                } else if self.no_specific_error() {
                    self.set_error(ErrorKind::MissingClosingParen, None)
                }

                f64::NAN
            }
            Token::End => {
                // The expression ended before we expected it.
                // e.g. `2 - `.
                // This means we have too few things.
                // Instead of introducing another error, just call it
                // "too few args".
                self.set_error(ErrorKind::TooFewArgs, None);

                f64::NAN
            }

            Token::Error | Token::Sep | Token::Close | Token::Infix(_) => {
                if self.no_specific_error() {
                    self.set_error(ErrorKind::UnexpectedToken, None);
                }

                f64::NAN
            }
        }
    }

    /// <power>  = {("-" | "+")} <base>
    fn power(&mut self) -> f64 {
        let mut sign = 1.0;
        while let Token::Infix(op) = self.current {
            if op == Operator::Sub {
                sign = -sign;
                self.next_token();
            } else if op == Operator::Add {
                self.next_token();
            } else {
                break;
            }
        }

        sign * self.base()
    }

    /// <factor> = <power> {"^" <power>}
    fn factor(&mut self) -> f64 {
        let mut ret = self.power();

        if let Token::Infix(Operator::Pow) = self.current {
            self.next_token();
            ret = ret.powf(self.factor());
        }

        ret
    }

    /// <term>   = <factor> {("*" | "/" | "%") <factor>}
    fn term(&mut self) -> f64 {
        let mut ret = self.factor();
        while let Token::Infix(op @ (Operator::Mul | Operator::Div | Operator::Rem)) = self.current
        {
            let op_pos = self.pos - 1;
            self.next_token();
            let ret2 = self.factor();
            if ret2 == 0.0 && [Operator::Div, Operator::Rem].contains(&op) {
                // Division by zero (also for modulo)
                // Error position is the "/" or "%" sign for now
                self.set_error(ErrorKind::DivByZero, Some((op_pos, 1)));
            }
            ret = op.eval(ret, ret2);
        }

        ret
    }

    /// <expr>   = <term> {("+" | "-") <term>}
    fn expr(&mut self) -> f64 {
        let mut ret = self.term();
        while let Token::Infix(op @ (Operator::Add | Operator::Sub)) = self.current {
            self.next_token();
            ret = op.eval(ret, self.term());
        }

        ret
    }
}

pub fn te_interp(expression: &wstr) -> Result<f64, Error> {
    let mut s = State::new(expression);
    let ret = s.eval();

    match s.error() {
        Ok(()) => Ok(ret),
        Err(e) => Err(e),
    }
}
