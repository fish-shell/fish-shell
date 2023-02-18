use libc::{c_int};

use crate::builtins::shared::{
    builtin_missing_argument, builtin_print_help, builtin_unknown_option, io_streams_t,
    STATUS_CMD_OK, STATUS_INVALID_ARGS,
};
use crate::ffi::{parser_t};
use crate::wchar::{widestrs, wstr};
use crate::wgetopt::{wgetopter_t, wopt, woption, woption_argument_t};
use crate::wutil::{self, format, fish_wcstoi_radix_all, wgettext_fmt};
use rand::{Rng, SeedableRng};
use rand::rngs::SmallRng;
use once_cell::sync::Lazy;
use std::sync::Mutex;

static seeded_engine: Lazy<Mutex<SmallRng>> = Lazy::new(|| { Mutex::new(SmallRng::from_entropy()) });


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
    const longopts: &[woption] = &[
        wopt("help"L, woption_argument_t::no_argument, 'h'),
    ];

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
            streams.err.append(wgettext_fmt!(
                "%ls: nothing to choose from\n",
                cmd,
            ));
            return STATUS_INVALID_ARGS;
        }

        let rand = engine.gen_range(0..arg_count - 1);
        streams.out.append(format::printf::sprintf!("%ls\n"L, argv[i + 1 + rand]));
        return STATUS_CMD_OK;
    }
    let mut parse_ll = |num : &wstr| {
        let res: Result<i64, wutil::Error> = fish_wcstoi_radix_all(num.chars(), None, true);
        match res {
            Err(_) => {
                streams.err.append(wgettext_fmt!(
                    "%ls: %ls: invalid integer\n",
                    cmd,
                    num,
                ));
            },
            Ok(_) => {}
        }
        return res;
    };

    match arg_count {
        0 => {
            // Keep the defaults
        },
        1 => {
            // Seed the engine persistently
            let num = parse_ll(argv[i]);
            match num {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(x) => *engine = SmallRng::seed_from_u64(x as u64)
            }
            return STATUS_CMD_OK;
        },
        2 => {
            // start is first, end is second
            match parse_ll(argv[i]) {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(x) => start = x
            }

            match parse_ll(argv[i + 1]) {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(x) => end = x
            }
        },
        3 => {
            // start, step, end
            match parse_ll(argv[i]) {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(x) => start = x
            }

            match parse_ll(argv[i + 1]) {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(x) if x < 0 => {
                    // XXX: Historical, this read an "unsigned long long"
                    // This should actually be "must be a positive integer"
                    streams.err.append(wgettext_fmt!(
                        "%ls: %ls: invalid integer\n",
                        cmd,
                        argv[i + 1],
                    ));
                    return STATUS_INVALID_ARGS;
                },
                Ok(0) => {
                    streams.err.append(wgettext_fmt!(
                        "%ls: STEP must be a positive integer\n",
                        cmd,
                    ));
                    return STATUS_INVALID_ARGS;
                }
                Ok(x) => step = x
            }

            match parse_ll(argv[i + 2]) {
                Err(_) => return STATUS_INVALID_ARGS,
                Ok(x) => end = x
            }
        },
        _ => {
            streams.err.append(wgettext_fmt!(
                "%ls: too many arguments\n",
                cmd,
            ));
            return Some(1);
        }
    }

    if end <= start {
        streams.err.append(wgettext_fmt!(
            "%ls: END must be greater than START\n",
            cmd,
        ));
        return STATUS_INVALID_ARGS;
    }

    let real_end : i64 = if start >= 0 || end < 0 {
        // 0 <= start <= end
        let diff : i64 = end - start;
        // 0 <= diff <= LL_MAX
        start + diff / step
    } else {
        // This is based on a "safe_abs" function
        // in the C++ that just casted a "long long" to "-unsigned long long"
        // start < 0 <= end
        let abs_start : u64 = start.abs() as u64;
        let diff : u64 = (end as u64 + abs_start) as u64;
        (diff / (step as u64) - abs_start) as i64
    };

    let a = if start < real_end { start } else { real_end };
    let b = if start > real_end { start } else { real_end };

    if a == b {
        streams.err.append(wgettext_fmt!(
            "%ls: range contains only one possible value\n",
            cmd,
        ));
        return STATUS_INVALID_ARGS;
    }

    let rand = engine.gen_range(a..b);

    let result = if start >= 0 {
        start + (rand - start) * step
    } else if rand < 0 {
        (rand - start) * step - start.abs()
    } else {
        (rand + start.abs()) * step - start.abs()
    };
    streams.out.append(format::printf::sprintf!("%d\n"L, result));

    return STATUS_CMD_OK;
}
