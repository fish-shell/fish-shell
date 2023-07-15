use super::prelude::*;

use crate::wutil;
use once_cell::sync::Lazy;
use rand::rngs::SmallRng;
use rand::{Rng, SeedableRng};
use std::sync::Mutex;

static RNG: Lazy<Mutex<SmallRng>> = Lazy::new(|| Mutex::new(SmallRng::from_entropy()));

pub fn random(parser: &Parser, streams: &mut IoStreams<'_>, argv: &mut [WString]) -> Option<c_int> {
    let argc = argv.len();
    let print_hints = false;

    const shortopts: &wstr = L!("+:h");
    const longopts: &[woption] = &[wopt(L!("help"), woption_argument_t::no_argument, 'h')];

    let mut w = wgetopter_t::new(shortopts, longopts, argv);
    while let Some(c) = w.wgetopt_long() {
        match c {
            'h' => {
                builtin_print_help(parser, streams, w.cmd());
                return STATUS_CMD_OK;
            }
            ':' => {
                builtin_missing_argument(
                    parser,
                    streams,
                    w.cmd(),
                    &w.argv()[w.woptind - 1],
                    print_hints,
                );
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(
                    parser,
                    streams,
                    w.cmd(),
                    &w.argv()[w.woptind - 1],
                    print_hints,
                );
                return STATUS_INVALID_ARGS;
            }
            _ => {
                panic!("unexpected retval from wgeopter.next()");
            }
        }
    }

    let mut start = 0;
    let mut end = 32767;
    let mut step = 1;
    let arg_count = argc - w.woptind;
    let i = w.woptind;
    if arg_count >= 1 && &w.argv()[i] == "choice" {
        if arg_count == 1 {
            streams
                .err
                .append(&wgettext_fmt!("%ls: nothing to choose from\n", w.cmd()));
            return STATUS_INVALID_ARGS;
        }

        let rand = RNG.lock().unwrap().gen_range(0..arg_count - 1);
        streams
            .out
            .append(&sprintf!(L!("%ls\n"), argv[i + 1 + rand]));
        return STATUS_CMD_OK;
    }
    fn parse_ll(streams: &mut IoStreams<'_>, cmd: &wstr, num: &wstr) -> Result<i64, wutil::Error> {
        let res = fish_wcstol(num);
        if res.is_err() {
            streams
                .err
                .append(&wgettext_fmt!("%ls: %ls: invalid integer\n", cmd, num));
        }
        return res;
    }
    fn parse_ull(streams: &mut IoStreams<'_>, cmd: &wstr, num: &wstr) -> Result<u64, wutil::Error> {
        let res = fish_wcstoul(num);
        if res.is_err() {
            streams
                .err
                .append(&wgettext_fmt!("%ls: %ls: invalid integer\n", cmd, num));
        }
        return res;
    }

    match arg_count {
        0 => {
            // Keep the defaults
        }
        1 => {
            // Seed the engine persistently
            let num = parse_ll(streams, w.cmd(), &w.argv()[i]);
            match num {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(x) => {
                    let mut engine = RNG.lock().unwrap();
                    *engine = SmallRng::seed_from_u64(x as u64);
                }
            }
            return STATUS_CMD_OK;
        }
        2 => {
            // start is first, end is second
            match parse_ll(streams, w.cmd(), &w.argv()[i]) {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(x) => start = x,
            }

            match parse_ll(streams, w.cmd(), &w.argv()[i + 1]) {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(x) => end = x,
            }
        }
        3 => {
            // start, step, end
            match parse_ll(streams, w.cmd(), &w.argv()[i]) {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(x) => start = x,
            }

            // start, step, end
            match parse_ull(streams, w.cmd(), &w.argv()[i + 1]) {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(0) => {
                    streams.err.append(&wgettext_fmt!(
                        "%ls: STEP must be a positive integer\n",
                        w.cmd(),
                    ));
                    return STATUS_INVALID_ARGS;
                }
                Ok(x) => step = x,
            }

            match parse_ll(streams, w.cmd(), &w.argv()[i + 2]) {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(x) => end = x,
            }
        }
        _ => {
            streams
                .err
                .append(&wgettext_fmt!("%ls: too many arguments\n", w.cmd(),));
            return Some(1);
        }
    }

    if end <= start {
        streams.err.append(&wgettext_fmt!(
            "%ls: END must be greater than START\n",
            w.cmd(),
        ));
        return STATUS_INVALID_ARGS;
    }

    // Using abs_diff() avoids an i64 overflow if start is i64::MIN and end is i64::MAX
    let possibilities = end.abs_diff(start) / step;
    if possibilities == 0 {
        streams.err.append(&wgettext_fmt!(
            "%ls: range contains only one possible value\n",
            w.cmd(),
        ));
        return STATUS_INVALID_ARGS;
    }

    let rand = {
        let mut engine = RNG.lock().unwrap();
        engine.gen_range(0..=possibilities)
    };

    // Safe because end was a valid i64 and the result here is in the range start..=end.
    let result: i64 = start.checked_add_unsigned(rand * step).unwrap();

    streams.out.appendln_owned(result.to_wstring());
    return STATUS_CMD_OK;
}
