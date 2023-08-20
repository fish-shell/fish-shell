use super::prelude::*;
use crate::tinyexpr::te_interp;

/// The maximum number of points after the decimal that we'll print.
const DEFAULT_SCALE: usize = 6;

/// The end of the range such that every integer is representable as a double.
/// i.e. this is the first value such that x + 1 == x (or == x + 2, depending on rounding mode).
const MAX_CONTIGUOUS_INTEGER: f64 = (1_u64 << f64::MANTISSA_DIGITS) as f64;

struct Options {
    print_help: bool,
    scale: usize,
    base: usize,
}

#[widestrs]
fn parse_cmd_opts(
    args: &mut [&wstr],
    parser: &mut parser_t,
    streams: &mut io_streams_t,
) -> Result<(Options, usize), Option<c_int>> {
    const cmd: &wstr = "math"L;
    let print_hints = true;

    // This command is atypical in using the "+" (REQUIRE_ORDER) option for flag parsing.
    // This is needed because of the minus, `-`, operator in math expressions.
    const SHORT_OPTS: &wstr = "+:hs:b:"L;
    const LONG_OPTS: &[woption] = &[
        wopt("scale"L, woption_argument_t::required_argument, 's'),
        wopt("base"L, woption_argument_t::required_argument, 'b'),
        wopt("help"L, woption_argument_t::no_argument, 'h'),
    ];

    let mut opts = Options {
        print_help: false,
        scale: DEFAULT_SCALE,
        base: 10,
    };

    let mut have_scale = false;

    let mut w = wgetopter_t::new(SHORT_OPTS, LONG_OPTS, args);
    while let Some(c) = w.wgetopt_long() {
        match c {
            's' => {
                let optarg = w.woptarg.unwrap();
                have_scale = true;
                // "max" is the special value that tells us to pick the maximum scale.
                if optarg == "max" {
                    opts.scale = 15;
                } else {
                    let scale = fish_wcstoi(optarg);
                    if scale.is_err() || scale.unwrap() < 0 || scale.unwrap() > 15 {
                        streams.err.append(wgettext_fmt!(
                            "%ls: %ls: invalid base value\n",
                            cmd,
                            optarg
                        ));
                        return Err(STATUS_INVALID_ARGS);
                    }
                    // We know the value is in the range [0, 15]
                    opts.scale = scale.unwrap() as usize;
                }
            }
            'b' => {
                let optarg = w.woptarg.unwrap();
                if optarg == "hex" {
                    opts.base = 16;
                } else if optarg == "octal" {
                    opts.base = 8;
                } else {
                    let base = fish_wcstoi(optarg);
                    if base.is_err() || (base.unwrap() != 8 && base.unwrap() != 16) {
                        streams.err.append(wgettext_fmt!(
                            "%ls: %ls: invalid base value\n",
                            cmd,
                            optarg
                        ));
                        return Err(STATUS_INVALID_ARGS);
                    }
                    // We know the value is 8 or 16.
                    opts.base = base.unwrap() as usize;
                }
            }
            'h' => {
                opts.print_help = true;
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, args[w.woptind - 1], print_hints);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                // For most commands this is an error. We ignore it because a math expression
                // can begin with a minus sign.
                return Ok((opts, w.woptind - 1));
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

    Ok((opts, w.woptind))
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

    // As a special-case, a scale of 0 means to truncate to an integer
    // instead of rounding.
    if opts.scale == 0 {
        v = v.trunc();
        return sprintf!("%.*f", opts.scale, v);
    }

    let mut ret = sprintf!("%.*f", opts.scale, v);
    // If we contain a decimal separator, trim trailing zeros after it, and then the separator
    // itself if there's nothing after it. Detect a decimal separator as a non-digit.
    if ret.chars().any(|c| !c.is_ascii_digit()) {
        let trailing_zeroes = ret.chars().rev().take_while(|&c| c == '0').count();
        let mut to_keep = ret.len() - trailing_zeroes;
        if ret.as_char_slice()[to_keep - 1] == '.' {
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

#[widestrs]
fn evaluate_expression(
    cmd: &wstr,
    streams: &mut io_streams_t,
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
                "Result is infinite"L
            } else if n.is_nan() {
                "Result is not a number"L
            } else if n.abs() >= MAX_CONTIGUOUS_INTEGER {
                "Result magnitude is too large"L
            } else {
                let mut s = format_double(n, opts);
                s.push('\n');

                streams.out.append(s);
                return STATUS_CMD_OK;
            };

            streams
                .err
                .append(sprintf!("%ls: Error: %ls\n"L, cmd, error_message));
            streams.err.append(sprintf!("'%ls'\n"L, expression));

            STATUS_CMD_ERROR
        }
        Err(err) => {
            streams.err.append(sprintf!(
                "%ls: Error: %ls\n"L,
                cmd,
                err.kind.describe_wstr()
            ));
            streams.err.append(sprintf!("'%ls'\n"L, expression));
            let padding = WString::from_chars(vec![' '; err.position + 1]);
            if err.len >= 2 {
                let tildes = WString::from_chars(vec!['~'; err.len - 2]);
                streams.err.append(sprintf!("%ls^%ls^\n"L, padding, tildes));
            } else {
                streams.err.append(sprintf!("%ls^\n"L, padding));
            }

            STATUS_CMD_ERROR
        }
    }
}

/// How much math reads at one. We don't expect very long input.
const MATH_CHUNK_SIZE: usize = 1024;

/// The math builtin evaluates math expressions.
#[widestrs]
pub fn math(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    argv: &mut [&wstr],
) -> Option<c_int> {
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
