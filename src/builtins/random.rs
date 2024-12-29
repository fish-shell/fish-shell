use super::prelude::*;

use crate::util::get_rng;
use crate::wutil;
use once_cell::sync::Lazy;
use rand::rngs::SmallRng;
use rand::{Rng, SeedableRng};
use std::sync::Mutex;

static RNG: Lazy<Mutex<SmallRng>> = Lazy::new(|| Mutex::new(get_rng()));

pub fn random(
    parser: &Parser,
    streams: &mut IoStreams,
    argv: &mut [&wstr],
) -> Result<StatusOk, StatusError> {
    let cmd = argv[0];
    let argc = argv.len();
    let print_hints = false;

    const shortopts: &wstr = L!("+:h");
    const longopts: &[WOption] = &[wopt(L!("help"), ArgType::NoArgument, 'h')];

    let mut w = WGetopter::new(shortopts, longopts, argv);
    #[allow(clippy::never_loop)]
    while let Some(c) = w.next_opt() {
        match c {
            'h' => {
                builtin_print_help(parser, streams, cmd);
                return Ok(StatusOk::OK);
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, argv[w.wopt_index - 1], print_hints);
                return Err(StatusError::STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, argv[w.wopt_index - 1], print_hints);
                return Err(StatusError::STATUS_INVALID_ARGS);
            }
            _ => {
                panic!("unexpected retval from WGetopter");
            }
        }
    }

    let mut start = 0;
    let mut end = 32767;
    let mut step = 1;
    let arg_count = argc - w.wopt_index;
    let i = w.wopt_index;
    if arg_count >= 1 && argv[i] == "choice" {
        if arg_count == 1 {
            streams
                .err
                .append(wgettext_fmt!("%ls: nothing to choose from\n", cmd));
            return Err(StatusError::STATUS_INVALID_ARGS);
        }

        let rand = RNG.lock().unwrap().gen_range(0..arg_count - 1);
        streams.out.appendln(argv[i + 1 + rand]);
        return Ok(StatusOk::OK);
    }
    fn parse_ll(streams: &mut IoStreams, cmd: &wstr, num: &wstr) -> Result<i64, wutil::Error> {
        let res = fish_wcstol(num);
        if res.is_err() {
            streams
                .err
                .append(wgettext_fmt!("%ls: %ls: invalid integer\n", cmd, num));
        }
        return res;
    }
    fn parse_ull(streams: &mut IoStreams, cmd: &wstr, num: &wstr) -> Result<u64, wutil::Error> {
        let res = fish_wcstoul(num);
        if res.is_err() {
            streams
                .err
                .append(wgettext_fmt!("%ls: %ls: invalid integer\n", cmd, num));
        }
        return res;
    }

    match arg_count {
        0 => {
            // Keep the defaults
        }
        1 => {
            // Seed the engine persistently
            let num = parse_ll(streams, cmd, argv[i]);
            match num {
                Err(_) => return Err(StatusError::STATUS_INVALID_ARGS),
                Ok(x) => {
                    let mut engine = RNG.lock().unwrap();
                    *engine = SmallRng::seed_from_u64(x as u64);
                }
            }
            return Ok(StatusOk::OK);
        }
        2 => {
            // start is first, end is second
            match parse_ll(streams, cmd, argv[i]) {
                Err(_) => return Err(StatusError::STATUS_INVALID_ARGS),
                Ok(x) => start = x,
            }

            match parse_ll(streams, cmd, argv[i + 1]) {
                Err(_) => return Err(StatusError::STATUS_INVALID_ARGS),
                Ok(x) => end = x,
            }
        }
        3 => {
            // start, step, end
            match parse_ll(streams, cmd, argv[i]) {
                Err(_) => return Err(StatusError::STATUS_INVALID_ARGS),
                Ok(x) => start = x,
            }

            // start, step, end
            match parse_ull(streams, cmd, argv[i + 1]) {
                Err(_) => return Err(StatusError::STATUS_INVALID_ARGS),
                Ok(0) => {
                    streams
                        .err
                        .append(wgettext_fmt!("%ls: STEP must be a positive integer\n", cmd,));
                    return Err(StatusError::STATUS_INVALID_ARGS);
                }
                Ok(x) => step = x,
            }

            match parse_ll(streams, cmd, argv[i + 2]) {
                Err(_) => return Err(StatusError::STATUS_INVALID_ARGS),
                Ok(x) => end = x,
            }
        }
        _ => {
            streams
                .err
                .append(wgettext_fmt!("%ls: too many arguments\n", cmd,));
            return Err(StatusError::STATUS_CMD_ERROR);
        }
    }

    let (start, end) = if start <= end {
        (start, end)
    } else {
        (end, start)
    };

    // Using abs_diff() avoids an i64 overflow if start is i64::MIN and end is i64::MAX
    let possibilities = end.abs_diff(start) / step;

    let rand = {
        let mut engine = RNG.lock().unwrap();
        engine.gen_range(0..=possibilities)
    };

    // Safe because end was a valid i64 and the result here is in the range start..=end.
    let result: i64 = start.checked_add_unsigned(rand * step).unwrap();

    streams.out.appendln(result.to_wstring());
    return Ok(StatusOk::OK);
}
