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
    prelude::*,
    wutil::{self, wcstod::wcstod_underscores},
};
use fish_common::assert_sorted_by_name;

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

impl PartialEq for Function {
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (Self::Constant(l0), Self::Constant(r0)) => l0 == r0,
            (Self::Fn1(l0), Self::Fn1(r0)) => std::ptr::fn_addr_eq(*l0, *r0),
            (Self::Fn2(l0), Self::Fn2(r0)) => std::ptr::fn_addr_eq(*l0, *r0),
            (Self::FnN(l0), Self::FnN(r0)) => std::ptr::fn_addr_eq(*l0, *r0),
            _ => false,
        }
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

#[derive(Debug, Clone, Copy, PartialEq)]
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

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Error {
    pub kind: ErrorKind,
    pub position: usize,
    pub len: usize,
}

#[derive(Debug, Clone, Copy, PartialEq)]
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

#[derive(Debug, Clone, Copy, PartialEq)]
enum Token {
    Error(Error),
    End,
    Sep,
    Open,
    Close,
    Number(f64),
    Function(Function),
    Op(Operator),
}

struct State<'s> {
    start: &'s wstr,
    pos: usize,
    token: Token,
    token_start: usize,
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
            if a.is_sign_positive() { a } else { b }
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
            if a.is_sign_negative() { a } else { b }
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
        ur = un - ur;
    }

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
    #[allow(clippy::incompatible_msrv)]
    (L!("ceil"), Function::Fn1(f64::ceil)),
    (L!("cos"), Function::Fn1(f64::cos)),
    (L!("cosh"), Function::Fn1(f64::cosh)),
    (L!("e"), Function::Constant(E)),
    (L!("exp"), Function::Fn1(f64::exp)),
    (L!("fac"), Function::Fn1(fac)),
    #[allow(clippy::incompatible_msrv)]
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
    #[allow(clippy::incompatible_msrv)]
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
            token: Token::End,
            token_start: 0,
        };
        state.next_token();
        state
    }

    pub fn eval(&mut self) -> Result<f64, Error> {
        let val = self.expr()?;
        if self.token == Token::End {
            Ok(val)
        } else {
            Err(Error {
                kind: ErrorKind::TooManyArgs,
                position: self.pos,
                len: 0,
            })
        }
    }

    fn build_error(&self, kind: ErrorKind, pos_len: Option<(usize, usize)>) -> Error {
        let (position, len) = pos_len.unwrap_or((self.pos, 0));
        Error {
            kind,
            position,
            len,
        }
    }

    fn build_error_token(&self, kind: ErrorKind, pos_len: Option<(usize, usize)>) -> Token {
        Token::Error(self.build_error(kind, pos_len))
    }

    fn set_error(&mut self, kind: ErrorKind, pos_len: Option<(usize, usize)>) -> Error {
        let error = self.build_error(kind, pos_len);
        self.token = Token::Error(error);
        error
    }

    fn specific_error(&self) -> Option<Error> {
        let Token::Error(error) = self.token else {
            return None;
        };
        if error.kind == ErrorKind::Unknown {
            None
        } else {
            Some(error)
        }
    }

    /// Get the next token from the input.
    /// Leading whitespaces must have been pre-filtered.
    fn get_token(&mut self) -> (usize, Token) {
        debug_assert!(!matches!(self.token, Token::Error(_)));

        let next = &self.start.as_char_slice().get(self.pos..).unwrap_or(&[]);
        if next.is_empty() {
            return (0, Token::End);
        }
        let next_char = next[0];

        // Try reading a number.
        if matches!(next_char, '0'..='9' | '.') {
            let mut consumed = 0;
            let token = match wcstod_underscores(*next, &mut consumed) {
                Ok(num) => Token::Number(num),
                Err(wutil::Error::InvalidChar) => {
                    self.build_error_token(ErrorKind::Unknown, Some((self.pos + consumed, 1)))
                }
                Err(wutil::Error::Overflow) => {
                    self.build_error_token(ErrorKind::NumberTooLarge, Some((self.pos, consumed)))
                }
                Err(wutil::Error::Empty) => {
                    // We have a matches! above, this can't be?
                    unreachable!()
                }
            };
            (consumed, token)
        } else {
            // Look for a function call.
            // But not when it's an "x" followed by whitespace
            // - that's the alternative multiplication operator.
            // We look for alphabetic here even tho all our function names are ASCII,
            // in order to give a nicer error.
            if next_char.is_alphabetic()
                && !(next_char == 'x' && next.len() > 1 && next[1].is_whitespace())
            {
                let ident_len = next
                    .iter()
                    .position(|&c| !(c.is_alphabetic() || c.is_ascii_digit() || c == '_'))
                    .unwrap_or(next.len());

                let ident = &next[..ident_len];
                let token = if let Some(var) = find_builtin(wstr::from_char_slice(ident)) {
                    Token::Function(var)
                } else {
                    self.build_error_token(ErrorKind::UnknownFunction, Some((self.pos, ident_len)))
                };

                (ident_len, token)
            } else {
                // Look for an operator or special character.
                let tok = match next_char {
                    '+' => Token::Op(Operator::Add),
                    '-' => Token::Op(Operator::Sub),
                    'x' | '*' => Token::Op(Operator::Mul),
                    '/' => Token::Op(Operator::Div),
                    '^' => Token::Op(Operator::Pow),
                    '%' => Token::Op(Operator::Rem),
                    '(' => Token::Open,
                    ')' => Token::Close,
                    ',' => Token::Sep,
                    ' ' | '\t' | '\n' | '\r' => unreachable!(),
                    '=' | '>' | '<' | '&' | '|' | '!' => {
                        self.build_error_token(ErrorKind::LogicalOperator, None)
                    }
                    _ => self.build_error_token(ErrorKind::MissingOperator, None),
                };

                (1, tok)
            }
        }
    }

    fn next_token(&mut self) {
        // Consume whitespaces before next token
        let first_char_pos = self
            .start
            .as_char_slice()
            .iter()
            .skip(self.pos)
            .take_while(|c| [' ', '\t', '\n', '\r'].contains(*c))
            .count();
        self.pos += first_char_pos;

        let (consumed, token) = self.get_token();
        self.token = token;
        self.token_start = self.pos;
        self.pos += consumed;
    }

    /// ```text
    /// <base>   = <constant> |
    ///            <function-0> {"(" ")"} |
    ///            <function-1> <power> |
    ///            <function-X> "(" <expr> {"," <expr>} ")" |
    ///            "(" <list> ")"
    /// ```
    fn base(&mut self) -> Result<f64, Error> {
        match self.token {
            Token::Number(n) => {
                let after_first = self.pos;

                self.next_token();
                match self.token {
                    Token::Error(err) => Err(err),
                    Token::Number(_) | Token::Function(_) => {
                        // Two numbers after each other:
                        // math '5 2'
                        // math '3 pi'
                        // (of course 3 pi could also be interpreted as 3 x pi)

                        // The error should be given *between*
                        // the last two tokens.
                        Err(self.set_error(
                            ErrorKind::MissingOperator,
                            Some((after_first, self.token_start - after_first)),
                        ))
                    }
                    _ => Ok(n),
                }
            }
            Token::Function(f) => {
                self.next_token();
                let have_open = (self.token == Token::Open).then(|| {
                    let pos = self.token_start;
                    // If we *have* an opening parenthesis,
                    // we need to consume it and
                    // expect a closing one.
                    self.next_token();
                    pos
                });

                // Handle arity 0 separately since, unlike other functions, they don't bind
                // with the remaining parameters ("ncr pi, 1" is equivalent to "ncr(pi(),1)",
                // not "ncr(pi(,1))")
                if f.arity() == Some(0) {
                    if let Some(open_pos) = have_open {
                        if self.token == Token::Close {
                            self.next_token();
                        } else if self.specific_error().is_none() {
                            return Err(
                                self.set_error(ErrorKind::MissingClosingParen, Some((open_pos, 0)))
                            );
                        }
                    }

                    return match f {
                        Function::Constant(n) => Ok(n),
                        _ => unreachable!("unhandled function type with arity 0"),
                    };
                }

                // Special case for `)` in `()` since this is a valid parameter list while `expr()`
                // in the loop below would return an `UnexpectedToken` error instead.
                // For the function format without paren, the existing code already works because
                // `expr()` already returns `TooFewArgs` when there is no further input
                if have_open.is_some() && self.token == Token::Close {
                    return Err(self.set_error(ErrorKind::TooFewArgs, None));
                }

                let mut parameters = vec![];
                loop {
                    parameters.push(self.expr()?);
                    if self.token != Token::Sep {
                        break;
                    }
                    if f.arity().is_some_and(|l| l == parameters.len()) {
                        return Err(
                            self.set_error(ErrorKind::TooManyArgs, Some((self.token_start, 0)))
                        );
                    }
                    self.next_token();
                }

                if let Some(open_pos) = have_open {
                    if self.token != Token::Close {
                        return Err(
                            self.set_error(ErrorKind::MissingClosingParen, Some((open_pos, 0)))
                        );
                    } else {
                        self.next_token();
                    }
                }

                if let Token::Error(error) = self.token {
                    Err(error)
                } else if f.arity().is_some_and(|l| l != parameters.len()) {
                    // `TooManyArgs` is already handled in the loop
                    Err(self.set_error(ErrorKind::TooFewArgs, None))
                } else {
                    Ok(f.call(&parameters))
                }
            }
            Token::Open => {
                let open_pos = self.token_start;
                self.next_token();
                let ret = self.expr()?;
                if self.token == Token::Close {
                    self.next_token();
                    return Ok(ret);
                }

                let error = if !matches!(self.token, Token::End | Token::Error(_)) {
                    self.set_error(ErrorKind::TooManyArgs, Some((self.token_start, 0)))
                } else if let Some(error) = self.specific_error() {
                    error
                } else {
                    self.set_error(ErrorKind::MissingClosingParen, Some((open_pos, 0)))
                };

                Err(error)
            }
            Token::End => {
                // The expression ended before we expected it.
                // e.g. `2 - `.
                // This means we have too few things.
                // Instead of introducing another error, just call it
                // "too few args".
                Err(self.set_error(ErrorKind::TooFewArgs, None))
            }

            Token::Sep
            | Token::Close
            | Token::Op(_)
            | Token::Error(Error {
                kind: ErrorKind::Unknown,
                ..
            }) => Err(self.set_error(
                ErrorKind::UnexpectedToken,
                Some((self.token_start, self.pos - self.token_start)),
            )),
            Token::Error(err) => Err(err),
        }
    }

    /// \<power\>  = {("-" | "+")} \<base\>
    fn power(&mut self) -> Result<f64, Error> {
        let mut sign = 1.0;
        while let Token::Op(op) = self.token {
            if op == Operator::Sub {
                sign = -sign;
                self.next_token();
            } else if op == Operator::Add {
                self.next_token();
            } else {
                break;
            }
        }

        Ok(sign * self.base()?)
    }

    /// \<factor\> = \<power\> {"^" \<power\>}
    fn factor(&mut self) -> Result<f64, Error> {
        let mut ret = self.power()?;

        if let Token::Op(Operator::Pow) = self.token {
            self.next_token();
            ret = ret.powf(self.factor()?);
        }

        Ok(ret)
    }

    /// \<term\>   = \<factor\> {("*" | "/" | "%") \<factor\>}
    fn term(&mut self) -> Result<f64, Error> {
        let mut ret = self.factor()?;
        while let Token::Op(op @ (Operator::Mul | Operator::Div | Operator::Rem)) = self.token {
            let op_pos = self.token_start;
            self.next_token();
            let ret2 = self.factor()?;
            if ret2 == 0.0 && [Operator::Div, Operator::Rem].contains(&op) {
                // Division by zero (also for modulo)
                // Error position is the "/" or "%" sign for now
                let err = self.set_error(ErrorKind::DivByZero, Some((op_pos, 1)));
                return Err(err);
            }
            ret = op.eval(ret, ret2);
        }

        Ok(ret)
    }

    /// \<expr\>   = \<term\> {("+" | "-") \<term\>}
    fn expr(&mut self) -> Result<f64, Error> {
        let mut ret = self.term()?;
        while let Token::Op(op @ (Operator::Add | Operator::Sub)) = self.token {
            self.next_token();
            ret = op.eval(ret, self.term()?);
        }

        Ok(ret)
    }
}

pub fn te_interp(expression: &wstr) -> Result<f64, Error> {
    let mut s = State::new(expression);
    s.eval()
}
