//! Implementation of the `sequence` builtin.

use super::prelude::*;

const SHORT_OPTIONS: &wstr = L!("f:t:s:");
const LONG_OPTIONS: &[WOption] = &[
    wopt(L!("from"), ArgType::RequiredArgument, 'f'),
    wopt(L!("to"), ArgType::RequiredArgument, 't'),
    wopt(L!("step"), ArgType::RequiredArgument, 's'),
];

fn do_sequence(streams: &mut IoStreams, from: i32, to: i32, step: i32) -> BuiltinResult {
    let mut i = from;
    if step > 0 {
        while i <= to {
            streams.out.appendln(&i.to_wstring());
            match i.checked_add(step) {
                Some(next) => i = next,
                None => break,
            }
        }
    } else {
        while i >= to {
            streams.out.appendln(&i.to_wstring());
            match i.checked_add(step) {
                Some(next) => i = next,
                None => break,
            }
        }
    }
    Ok(SUCCESS)
}

pub fn sequence(parser: &mut Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> BuiltinResult {
    let cmd = argv[0];
    let argc = argv.len();

    // If just invoked as 'sequence N'.
    if argc == 2 {
        let Ok(to) = fish_wcstoi(argv[1]) else {
            err_fmt!("%s: invalid --to value", argv[1])
                .cmd(cmd)
                .finish(streams);
            return Err(STATUS_INVALID_ARGS);
        };
        return if to < 0 {
            do_sequence(streams, -1, to, -1)
        } else {
            do_sequence(streams, 1, to, 1)
        };
    }

    let mut to: Option<i32> = None;
    let mut from: Option<i32> = None;
    let mut step: Option<i32> = None;

    let mut w = WGetopter::new(SHORT_OPTIONS, LONG_OPTIONS, argv);
    while let Some(opt) = w.next_opt() {
        match opt {
            'f' => {
                let optarg = w.woptarg.unwrap();
                match fish_wcstoi(optarg) {
                    Ok(v) => from = Some(v),
                    Err(_) => {
                        err_fmt!("%s: invalid --from value", optarg)
                            .cmd(cmd)
                            .finish(streams);
                        return Err(STATUS_INVALID_ARGS);
                    }
                }
            }
            't' => {
                let optarg = w.woptarg.unwrap();
                match fish_wcstoi(optarg) {
                    Ok(v) => to = Some(v),
                    Err(_) => {
                        err_fmt!("%s: invalid --to value", optarg)
                            .cmd(cmd)
                            .finish(streams);
                        return Err(STATUS_INVALID_ARGS);
                    }
                }
            }
            's' => {
                let optarg = w.woptarg.unwrap();
                match fish_wcstoi(optarg) {
                    Ok(v) => step = Some(v),
                    Err(_) => {
                        err_fmt!("%s: invalid --step value", optarg)
                            .cmd(cmd)
                            .finish(streams);
                        return Err(STATUS_INVALID_ARGS);
                    }
                }
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, None, argv[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            ';' => {
                builtin_unexpected_argument(parser, streams, cmd, argv[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, argv[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            _ => {
                panic!("unexpected retval from WGetopter");
            }
        }
    }

    if w.wopt_index != argc {
        err_fmt!(Error::UNEXP_ARG_COUNT, 0, argc - w.wopt_index)
            .cmd(cmd)
            .finish(streams);
        return Err(STATUS_INVALID_ARGS);
    }

    let Some(to) = to else {
        err_fmt!("--to is required").cmd(cmd).finish(streams);
        return Err(STATUS_INVALID_ARGS);
    };

    if let Some(step) = step {
        if let Some(from) = from {
            if from == to {
                streams.out.appendln(&from.to_wstring());
                return Ok(SUCCESS);
            }
            if step == 0 {
                err_fmt!("--step argument must not be 0")
                    .cmd(cmd)
                    .finish(streams);
                return Err(STATUS_INVALID_ARGS);
            }
            if (from < to) != (step > 0) {
                err_fmt!("%d: step argument counts in opposite direction", step)
                    .cmd(cmd)
                    .finish(streams);
                return Err(STATUS_INVALID_ARGS);
            }
            do_sequence(streams, from, to, step)
        } else {
            err_fmt!("--to and --from must be specified if --step is used")
                .cmd(cmd)
                .finish(streams);
            Err(STATUS_INVALID_ARGS)
        }
    } else if let Some(from) = from {
        let step = if to < from { -1 } else { 1 };
        do_sequence(streams, from, to, step)
    } else if to < 0 {
        do_sequence(streams, -1, to, -1)
    } else {
        do_sequence(streams, 1, to, 1)
    }
}
