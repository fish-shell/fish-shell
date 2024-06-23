use num_traits::pow;
use widestring::utf32str;

use super::prelude::*;
use crate::tinyexpr::te_interp;

/// The maximum number of points after the decimal that we'll print.
const DEFAULT_SCALE: usize = 6;

const DEFAULT_SCALE_MODE: ScaleMode = ScaleMode::Default;

/// The end of the range such that every integer is representable as a double.
/// i.e. this is the first value such that x + 1 == x (or == x + 2, depending on rounding mode).
const MAX_CONTIGUOUS_INTEGER: f64 = (1_u64 << f64::MANTISSA_DIGITS) as f64;

enum ScaleMode {
    Truncate,
    Round,
    Floor,
    Ceiling,
    Default,
}

struct Options {
    print_help: bool,
    scale: usize,
    base: usize,
    scale_mode: ScaleMode,
}

fn parse_cmd_opts(
    args: &mut [&wstr],
    parser: &Parser,
    streams: &mut IoStreams,
) -> Result<(Options, usize), Option<c_int>> {
    const cmd: &wstr = L!("math");
    let print_hints = true;

    // This command is atypical in using the "+" (REQUIRE_ORDER) option for flag parsing.
    // This is needed because of the minus, `-`, operator in math expressions.
    const SHORT_OPTS: &wstr = L!("+:hs:b:m:");
    const LONG_OPTS: &[WOption] = &[
        wopt(L!("scale"), ArgType::RequiredArgument, 's'),
        wopt(L!("base"), ArgType::RequiredArgument, 'b'),
        wopt(L!("help"), ArgType::NoArgument, 'h'),
        wopt(L!("scale-mode"), ArgType::RequiredArgument, 'm'),
    ];

    let mut opts = Options {
        print_help: false,
        scale: DEFAULT_SCALE,
        base: 10,
        scale_mode: DEFAULT_SCALE_MODE,
    };

    let mut have_scale = false;

    let mut w = WGetopter::new(SHORT_OPTS, LONG_OPTS, args);
    while let Some(c) = w.next_opt() {
        match c {
            's' => {
                let optarg = w.woptarg.unwrap();
                have_scale = true;
                // "max" is the special value that tells us to pick the maximum scale.
                if optarg == "max" {
                    opts.scale = 15;
                } else {
                    let scale = fish_wcstoi(optarg).unwrap_or(-1);
                    if scale < 0 || scale > 15 {
                        streams
                            .err
                            .append(wgettext_fmt!("%ls: %ls: invalid scale\n", cmd, optarg));
                        return Err(STATUS_INVALID_ARGS);
                    }
                    // We know the value is in the range [0, 15]
                    opts.scale = scale as usize;
                }
            }
            'm' => {
                let optarg = w.woptarg.unwrap();
                if optarg.eq(utf32str!("truncate")) || optarg.eq(utf32str!("trunc")) {
                    opts.scale_mode = ScaleMode::Truncate;
                } else if optarg.eq(utf32str!("round")) {
                    opts.scale_mode = ScaleMode::Round;
                } else if optarg.eq(utf32str!("floor")) {
                    opts.scale_mode = ScaleMode::Floor;
                } else if optarg.eq(utf32str!("ceiling")) || optarg.eq(utf32str!("ceil")) {
                    opts.scale_mode = ScaleMode::Ceiling;
                } else {
                    streams
                        .err
                        .append(wgettext_fmt!("%ls: %ls: invalid mode\n", cmd, optarg));
                    return Err(STATUS_INVALID_ARGS);
                }
            }
            'b' => {
                let optarg = w.woptarg.unwrap();
                if optarg == "hex" {
                    opts.base = 16;
                } else if optarg == "octal" {
                    opts.base = 8;
                } else {
                    let base = fish_wcstoi(optarg).unwrap_or(-1);
                    if base != 8 && base != 16 {
                        streams.err.append(wgettext_fmt!(
                            "%ls: %ls: invalid base value\n",
                            cmd,
                            optarg
                        ));
                        return Err(STATUS_INVALID_ARGS);
                    }
                    // We know the value is 8 or 16.
                    opts.base = base as usize;
                }
            }
            'h' => {
                opts.print_help = true;
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, args[w.wopt_index - 1], print_hints);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                // For most commands this is an error. We ignore it because a math expression
                // can begin with a minus sign.
                return Ok((opts, w.wopt_index - 1));
            }
            _ => {
                panic!("unexpected retval from wgeopter.next()");
            }
        }
    }

    if have_scale && opts.scale != 0 && opts.base != 10 {
        streams.err.append(wgettext_fmt!(
            BUILTIN_ERR_COMBO2,
            cmd,
            "non-zero scale value only valid
            for base 10"
        ));
        return Err(STATUS_INVALID_ARGS);
    }

    Ok((opts, w.wopt_index))
}

/// Return a formatted version of the value `v` respecting the given `opts`.
fn format_double(mut v: f64, opts: &Options) -> WString {
    if opts.base == 16 {
        v = v.trunc();
        let mneg = if v.is_sign_negative() { "-" } else { "" };
        return sprintf!("%s0x%lx", mneg, v.abs() as u64);
    } else if opts.base == 8 {
        v = v.trunc();
        if v == 0.0 {
            // not 00
            return WString::from_str("0");
        }
        let mneg = if v.is_sign_negative() { "-" } else { "" };
        return sprintf!("%s0%lo", mneg, v.abs() as u64);
    }

    v *= pow(10f64, opts.scale);

    v = match opts.scale_mode {
        ScaleMode::Truncate => v.trunc(),
        ScaleMode::Round => v.round(),
        ScaleMode::Floor => v.floor(),
        ScaleMode::Ceiling => v.ceil(),
        ScaleMode::Default => {
            if opts.scale == 0 {
                v.trunc()
            } else {
                v
            }
        }
    };

    // if we don't add check here, the result of 'math -s 0 "22 / 5 - 5"' will be '0', not '-0'
    if opts.scale != 0 {
        v /= pow(10f64, opts.scale);
    } else {
        return sprintf!("%.*f", opts.scale, v);
    }

    let mut ret = sprintf!("%.*f", opts.scale, v);
    // If we contain a decimal separator, trim trailing zeros after it, and then the separator
    // itself if there's nothing after it. Detect a decimal separator as a non-digit.
    if ret.chars().any(|c| !c.is_ascii_digit()) {
        let trailing_zeroes = ret.chars().rev().take_while(|&c| c == '0').count();
        let mut to_keep = ret.len() - trailing_zeroes;
        // Check for the decimal separator (we don't know what character it is)
        if !ret.as_char_slice()[to_keep - 1].is_ascii_digit() {
            to_keep -= 1;
        }
        ret.truncate(to_keep);
    }

    // If we trimmed everything it must have just been zero.
    // TODO: can this ever happen?
    if ret.is_empty() {
        ret.push('0');
    }

    ret
}

fn evaluate_expression(
    cmd: &wstr,
    streams: &mut IoStreams,
    opts: &Options,
    expression: &wstr,
) -> Option<c_int> {
    let ret = te_interp(expression);

    match ret {
        Ok(n) => {
            // Check some runtime errors after the fact.
            // TODO: Really, this should be done in tinyexpr
            // (e.g. infinite is the result of "x / 0"),
            // but that's much more work.
            let error_message = if n.is_infinite() {
                L!("Result is infinite")
            } else if n.is_nan() {
                L!("Result is not a number")
            } else if n.abs() >= MAX_CONTIGUOUS_INTEGER {
                L!("Result magnitude is too large")
            } else {
                let mut s = format_double(n, opts);
                s.push('\n');

                streams.out.append(s);
                return STATUS_CMD_OK;
            };

            streams
                .err
                .append(sprintf!("%ls: Error: %ls\n", cmd, error_message));
            streams.err.append(sprintf!("'%ls'\n", expression));

            STATUS_CMD_ERROR
        }
        Err(err) => {
            streams.err.append(sprintf!(
                L!("%ls: Error: %ls\n"),
                cmd,
                err.kind.describe_wstr()
            ));
            streams.err.append(sprintf!("'%ls'\n", expression));
            let padding = WString::from_chars(vec![' '; err.position + 1]);
            if err.len >= 2 {
                let tildes = WString::from_chars(vec!['~'; err.len - 2]);
                streams.err.append(sprintf!("%ls^%ls^\n", padding, tildes));
            } else {
                streams.err.append(sprintf!("%ls^\n", padding));
            }

            STATUS_CMD_ERROR
        }
    }
}

/// How much math reads at one. We don't expect very long input.
const MATH_CHUNK_SIZE: usize = 1024;

/// The math builtin evaluates math expressions.
pub fn math(parser: &Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> Option<c_int> {
    let cmd = argv[0];

    let (opts, mut optind) = match parse_cmd_opts(argv, parser, streams) {
        Ok(x) => x,
        Err(e) => return e,
    };

    if opts.print_help {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    let mut expression = WString::new();
    for (arg, _) in Arguments::new(argv, &mut optind, streams, MATH_CHUNK_SIZE) {
        if !expression.is_empty() {
            expression.push(' ')
        }
        expression.push_utfstr(&arg);
    }

    if expression.is_empty() {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_MIN_ARG_COUNT1, cmd, 1, 0));
        return STATUS_CMD_ERROR;
    }

    evaluate_expression(cmd, streams, &opts, &expression)
}
