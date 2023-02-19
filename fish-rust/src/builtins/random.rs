use libc::c_int;

use crate::builtins::shared::{
    builtin_missing_argument, builtin_print_help, builtin_unknown_option, io_streams_t,
    STATUS_CMD_OK, STATUS_INVALID_ARGS,
};
use crate::ffi::parser_t;
use crate::wchar::{widestrs, wstr};
use crate::wgetopt::{wgetopter_t, wopt, woption, woption_argument_t};
use crate::wutil::{self, fish_wcstoi_radix_all, format::printf::sprintf, wgettext_fmt};
use num_traits::PrimInt;
use once_cell::sync::Lazy;
use rand::rngs::SmallRng;
use rand::{Rng, SeedableRng};
use std::sync::Mutex;

static seeded_engine: Lazy<Mutex<SmallRng>> = Lazy::new(|| Mutex::new(SmallRng::from_entropy()));

#[widestrs]
pub fn random(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    argv: &mut [&wstr],
) -> Option<c_int> {
    let cmd = argv[0];
    let argc = argv.len();
    let print_hints = false;

    const shortopts: &wstr = "+:h"L;
    const longopts: &[woption] = &[wopt("help"L, woption_argument_t::no_argument, 'h')];

    let mut w = wgetopter_t::new(shortopts, longopts, argv);
    while let Some(c) = w.wgetopt_long() {
        match c {
            'h' => {
                builtin_print_help(parser, streams, cmd);
                return STATUS_CMD_OK;
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1], print_hints);
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1], print_hints);
                return STATUS_INVALID_ARGS;
            }
            _ => {
                panic!("unexpected retval from wgeopter.next()");
            }
        }
    }

    let mut engine = seeded_engine.lock().unwrap();
    let mut start = 0;
    let mut end = 32767;
    let mut step = 1;
    let arg_count = argc - w.woptind;
    let i = w.woptind;
    if arg_count >= 1 && argv[i] == "choice" {
        if arg_count == 1 {
            streams
                .err
                .append(wgettext_fmt!("%ls: nothing to choose from\n", cmd,));
            return STATUS_INVALID_ARGS;
        }

        let rand = engine.gen_range(0..arg_count - 1);
        streams.out.append(sprintf!("%ls\n"L, argv[i + 1 + rand]));
        return STATUS_CMD_OK;
    }
    fn parse<T: PrimInt>(
        streams: &mut io_streams_t,
        cmd: &wstr,
        num: &wstr,
    ) -> Result<T, wutil::Error> {
        let res = fish_wcstoi_radix_all(num.chars(), None, true);
        if res.is_err() {
            streams
                .err
                .append(wgettext_fmt!("%ls: %ls: invalid integer\n", cmd, num,));
        }
        return res;
    }

    match arg_count {
        0 => {
            // Keep the defaults
        }
        1 => {
            // Seed the engine persistently
            let num = parse::<i64>(streams, cmd, argv[i]);
            match num {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(x) => *engine = SmallRng::seed_from_u64(x as u64),
            }
            return STATUS_CMD_OK;
        }
        2 => {
            // start is first, end is second
            match parse::<i64>(streams, cmd, argv[i]) {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(x) => start = x,
            }

            match parse::<i64>(streams, cmd, argv[i + 1]) {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(x) => end = x,
            }
        }
        3 => {
            // start, step, end
            match parse::<i64>(streams, cmd, argv[i]) {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(x) => start = x,
            }

            // start, step, end
            match parse::<u64>(streams, cmd, argv[i + 1]) {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(0) => {
                    streams
                        .err
                        .append(wgettext_fmt!("%ls: STEP must be a positive integer\n", cmd,));
                    return STATUS_INVALID_ARGS;
                }
                Ok(x) => step = x,
            }

            match parse::<i64>(streams, cmd, argv[i + 2]) {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(x) => end = x,
            }
        }
        _ => {
            streams
                .err
                .append(wgettext_fmt!("%ls: too many arguments\n", cmd,));
            return Some(1);
        }
    }

    if end <= start {
        streams
            .err
            .append(wgettext_fmt!("%ls: END must be greater than START\n", cmd,));
        return STATUS_INVALID_ARGS;
    }

    // Possibilities can be abs(i64::MIN) + i64::MAX,
    // so we do this as i128
    let possibilities = end.abs_diff(start) / step;

    if possibilities == 0 {
        streams.err.append(wgettext_fmt!(
            "%ls: range contains only one possible value\n",
            cmd,
        ));
        return STATUS_INVALID_ARGS;
    }

    let rand = engine.gen_range(0..=possibilities);

    let result = start.checked_add_unsigned(rand * step).unwrap();

    // We do our math as i128,
    // and then we check if it fits in 64 bit - signed or unsigned!
    streams.out.append(sprintf!("%d\n"L, result));
    return STATUS_CMD_OK;
}
