use std::cmp::Ordering;

use libc::{c_uint, rlim_t, RLIM_INFINITY};
use nix::errno::Errno;
use once_cell::sync::Lazy;

use crate::fallback::{fish_wcswidth, wcscasecmp};
use crate::libc::*;
use crate::wutil::perror;

use super::prelude::*;

/// Calls getrlimit.
fn getrlimit(resource: c_uint) -> Option<(rlim_t, rlim_t)> {
    let resource: i32 = resource.try_into().unwrap();

    // Resource is #[repr(i32)] so this is ok
    let resource = unsafe { std::mem::transmute::<i32, nix::sys::resource::Resource>(resource) };
    nix::sys::resource::getrlimit(resource)
        .map_err(|_| perror("getrlimit"))
        .ok()
}

fn setrlimit(resource: c_uint, rlim_cur: rlim_t, rlim_max: rlim_t) -> Result<(), Errno> {
    let resource: i32 = resource.try_into().unwrap();
    // Resource is #[repr(i32)] so this is ok
    let resource = unsafe { std::mem::transmute::<i32, nix::sys::resource::Resource>(resource) };
    nix::sys::resource::setrlimit(resource, rlim_cur, rlim_max)
}

/// Print the value of the specified resource limit.
fn print(resource: c_uint, hard: bool, streams: &mut IoStreams) {
    let Some(l) = get(resource, hard) else {
        streams.out.append(wgettext!("error\n"));
        return;
    };

    if l == RLIM_INFINITY {
        streams.out.append(wgettext!("unlimited\n"));
    } else {
        streams
            .out
            .append(wgettext_fmt!("%lu\n", l / get_multiplier(resource)));
    }
}

/// Print values of all resource limits.
fn print_all(hard: bool, streams: &mut IoStreams) {
    let mut w = 0;

    for resource in RESOURCE_ARR.iter() {
        w = w.max(fish_wcswidth(resource.desc));
    }
    for resource in RESOURCE_ARR.iter() {
        let Some((rlim_cur, rlim_max)) = getrlimit(resource.resource) else {
            continue;
        };
        let l = if hard { rlim_max } else { rlim_cur };

        let unit = if resource.resource == RLIMIT_CPU() as c_uint {
            "(seconds, "
        } else if get_multiplier(resource.resource) == 1 {
            "("
        } else {
            "(kB, "
        };
        streams.out.append(sprintf!(
            "%-*ls %10ls-%lc) ",
            w,
            resource.desc,
            unit,
            resource.switch_char,
        ));

        if l == RLIM_INFINITY {
            streams.out.append(wgettext!("unlimited\n"));
        } else {
            streams.out.append(wgettext_fmt!(
                "%lu\n",
                l / get_multiplier(resource.resource)
            ));
        }
    }
}

/// Returns the description for the specified resource limit.
fn get_desc(what: c_uint) -> &'static wstr {
    for resource in RESOURCE_ARR.iter() {
        if resource.resource == what {
            return resource.desc;
        }
    }
    unreachable!()
}

/// Set the new value of the specified resource limit. This function does _not_ multiply the limit
/// value by the multiplier constant used by the commandline ulimit.
fn set_limit(
    resource: c_uint,
    hard: bool,
    soft: bool,
    value: rlim_t,
    streams: &mut IoStreams,
) -> Option<c_int> {
    let Some((mut rlim_cur, mut rlim_max)) = getrlimit(resource) else {
        return STATUS_CMD_ERROR;
    };
    if hard {
        rlim_max = value;
    }
    if soft {
        rlim_cur = value;

        // Do not attempt to set the soft limit higher than the hard limit.
        if (value > rlim_max || value == RLIM_INFINITY) && rlim_max != RLIM_INFINITY {
            rlim_cur = rlim_max;
        }
    }

    if let Err(errno) = setrlimit(resource, rlim_cur, rlim_max) {
        if errno == Errno::EPERM {
            streams.err.append(wgettext_fmt!(
                "ulimit: Permission denied when changing resource of type '%ls'\n",
                get_desc(resource)
            ));
        } else {
            builtin_wperror(L!("ulimit"), streams);
        }

        STATUS_CMD_ERROR
    } else {
        STATUS_CMD_OK
    }
}

/// Get the implicit multiplication factor for the specified resource limit.
fn get_multiplier(what: c_uint) -> rlim_t {
    for resource in RESOURCE_ARR.iter() {
        if resource.resource == what {
            return resource.multiplier as rlim_t;
        }
    }
    unreachable!()
}

fn get(resource: c_uint, hard: bool) -> Option<rlim_t> {
    let (rlim_cur, rlim_max) = getrlimit(resource)?;

    Some(if hard { rlim_max } else { rlim_cur })
}

#[derive(Debug, Clone, Copy)]
struct Options {
    what: c_int,
    report_all: bool,
    hard: bool,
    soft: bool,
}

impl Default for Options {
    fn default() -> Self {
        Options {
            what: RLIMIT_FSIZE(),
            report_all: false,
            hard: false,
            soft: false,
        }
    }
}

pub fn ulimit(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> Option<c_int> {
    let cmd = args[0];

    const SHORT_OPTS: &wstr = L!(":HSabcdefilmnqrstuvwyKPTh");

    const LONG_OPTS: &[WOption] = &[
        wopt(L!("all"), ArgType::NoArgument, 'a'),
        wopt(L!("hard"), ArgType::NoArgument, 'H'),
        wopt(L!("soft"), ArgType::NoArgument, 'S'),
        wopt(L!("socket-buffers"), ArgType::NoArgument, 'b'),
        wopt(L!("core-size"), ArgType::NoArgument, 'c'),
        wopt(L!("data-size"), ArgType::NoArgument, 'd'),
        wopt(L!("nice"), ArgType::NoArgument, 'e'),
        wopt(L!("file-size"), ArgType::NoArgument, 'f'),
        wopt(L!("pending-signals"), ArgType::NoArgument, 'i'),
        wopt(L!("lock-size"), ArgType::NoArgument, 'l'),
        wopt(L!("resident-set-size"), ArgType::NoArgument, 'm'),
        wopt(L!("file-descriptor-count"), ArgType::NoArgument, 'n'),
        wopt(L!("queue-size"), ArgType::NoArgument, 'q'),
        wopt(L!("realtime-priority"), ArgType::NoArgument, 'r'),
        wopt(L!("stack-size"), ArgType::NoArgument, 's'),
        wopt(L!("cpu-time"), ArgType::NoArgument, 't'),
        wopt(L!("process-count"), ArgType::NoArgument, 'u'),
        wopt(L!("virtual-memory-size"), ArgType::NoArgument, 'v'),
        wopt(L!("swap-size"), ArgType::NoArgument, 'w'),
        wopt(L!("realtime-maxtime"), ArgType::NoArgument, 'y'),
        wopt(L!("kernel-queues"), ArgType::NoArgument, 'K'),
        wopt(L!("ptys"), ArgType::NoArgument, 'P'),
        wopt(L!("threads"), ArgType::NoArgument, 'T'),
        wopt(L!("help"), ArgType::NoArgument, 'h'),
    ];

    let mut opts = Options::default();

    let mut w = WGetopter::new(SHORT_OPTS, LONG_OPTS, args);

    while let Some(c) = w.next_opt() {
        match c {
            'a' => opts.report_all = true,
            'H' => opts.hard = true,
            'S' => opts.soft = true,
            'b' => opts.what = RLIMIT_SBSIZE(),
            'c' => opts.what = RLIMIT_CORE(),
            'd' => opts.what = RLIMIT_DATA(),
            'e' => opts.what = RLIMIT_NICE(),
            'f' => opts.what = RLIMIT_FSIZE(),
            'i' => opts.what = RLIMIT_SIGPENDING(),
            'l' => opts.what = RLIMIT_MEMLOCK(),
            'm' => opts.what = RLIMIT_RSS(),
            'n' => opts.what = RLIMIT_NOFILE(),
            'q' => opts.what = RLIMIT_MSGQUEUE(),
            'r' => opts.what = RLIMIT_RTPRIO(),
            's' => opts.what = RLIMIT_STACK(),
            't' => opts.what = RLIMIT_CPU(),
            'u' => opts.what = RLIMIT_NPROC(),
            'v' => opts.what = RLIMIT_AS(),
            'w' => opts.what = RLIMIT_SWAP(),
            'y' => opts.what = RLIMIT_RTTIME(),
            'K' => opts.what = RLIMIT_KQUEUES(),
            'P' => opts.what = RLIMIT_NPTS(),
            'T' => opts.what = RLIMIT_NTHR(),
            'h' => {
                builtin_print_help(parser, streams, cmd);
                return STATUS_CMD_OK;
            }
            ':' => {
                builtin_missing_argument(parser, streams, cmd, w.argv[w.wopt_index - 1], true);
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                builtin_unknown_option(parser, streams, cmd, w.argv[w.wopt_index - 1], true);
                return STATUS_INVALID_ARGS;
            }
            _ => {
                panic!("unexpected retval from WGetopter");
            }
        }
    }

    if opts.report_all {
        print_all(opts.hard, streams);
    }

    if opts.what == -1 {
        streams.err.append(wgettext_fmt!(
            "%ls: Resource limit not available on this operating system\n",
            cmd
        ));

        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    let what: c_uint = opts.what.try_into().unwrap();

    let argc = w.argv.len();
    let arg_count = argc - w.wopt_index;
    if arg_count == 0 {
        print(what, opts.hard, streams);
        return STATUS_CMD_OK;
    } else if arg_count != 1 {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd));

        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    let mut hard = opts.hard;
    let mut soft = opts.soft;
    if !hard && !soft {
        // Set both hard and soft limits if neither was specified.
        hard = true;
        soft = true;
    }

    let new_limit: rlim_t = if w.wopt_index == argc {
        streams.err.append(wgettext_fmt!(
            "%ls: New limit cannot be an empty string\n",
            cmd
        ));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    } else if wcscasecmp(w.argv[w.wopt_index], L!("unlimited")) == Ordering::Equal {
        RLIM_INFINITY
    } else if wcscasecmp(w.argv[w.wopt_index], L!("hard")) == Ordering::Equal {
        match get(what, true) {
            Some(limit) => limit,
            None => return STATUS_CMD_ERROR,
        }
    } else if wcscasecmp(w.argv[w.wopt_index], L!("soft")) == Ordering::Equal {
        match get(what, soft) {
            Some(limit) => limit,
            None => return STATUS_CMD_ERROR,
        }
    } else if let Ok(limit) = fish_wcstol(w.argv[w.wopt_index]) {
        limit as rlim_t * get_multiplier(what)
    } else {
        streams.err.append(wgettext_fmt!(
            "%ls: Invalid limit '%ls'\n",
            cmd,
            w.argv[w.wopt_index]
        ));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    };

    set_limit(what, hard, soft, new_limit, streams)
}

/// Struct describing a resource limit.
struct Resource {
    resource: c_uint,    // resource ID
    desc: &'static wstr, // description of resource
    switch_char: char,   // switch used on commandline to specify resource
    multiplier: c_uint,  // the implicit multiplier used when setting getting values
}

impl Resource {
    fn new(
        resource: c_uint,
        desc: &'static wstr,
        switch_char: char,
        multiplier: c_uint,
    ) -> Resource {
        Resource {
            resource,
            desc,
            switch_char,
            multiplier,
        }
    }
}

/// Array of resource_t structs, describing all known resource types.
static RESOURCE_ARR: Lazy<Box<[Resource]>> = Lazy::new(|| {
    let resources_info = [
        (
            RLIMIT_SBSIZE(),
            L!("Maximum size of socket buffers"),
            'b',
            1024,
        ),
        (
            RLIMIT_CORE(),
            L!("Maximum size of core files created"),
            'c',
            1024,
        ),
        (
            RLIMIT_DATA(),
            L!("Maximum size of a process’s data segment"),
            'd',
            1024,
        ),
        (
            RLIMIT_NICE(),
            L!("Control of maximum nice priority"),
            'e',
            1,
        ),
        (
            RLIMIT_FSIZE(),
            L!("Maximum size of files created by the shell"),
            'f',
            1024,
        ),
        (
            RLIMIT_SIGPENDING(),
            L!("Maximum number of pending signals"),
            'i',
            1,
        ),
        (
            RLIMIT_MEMLOCK(),
            L!("Maximum size that may be locked into memory"),
            'l',
            1024,
        ),
        (RLIMIT_RSS(), L!("Maximum resident set size"), 'm', 1024),
        (
            RLIMIT_NOFILE(),
            L!("Maximum number of open file descriptors"),
            'n',
            1,
        ),
        (
            RLIMIT_MSGQUEUE(),
            L!("Maximum bytes in POSIX message queues"),
            'q',
            1024,
        ),
        (
            RLIMIT_RTPRIO(),
            L!("Maximum realtime scheduling priority"),
            'r',
            1,
        ),
        (RLIMIT_STACK(), L!("Maximum stack size"), 's', 1024),
        (
            RLIMIT_CPU(),
            L!("Maximum amount of CPU time in seconds"),
            't',
            1,
        ),
        (
            RLIMIT_NPROC(),
            L!("Maximum number of processes available to current user"),
            'u',
            1,
        ),
        (
            RLIMIT_AS(),
            L!("Maximum amount of virtual memory available to each process"),
            'v',
            1024,
        ),
        (RLIMIT_SWAP(), L!("Maximum swap space"), 'w', 1024),
        (
            RLIMIT_RTTIME(),
            L!("Maximum contiguous realtime CPU time"),
            'y',
            1,
        ),
        (RLIMIT_KQUEUES(), L!("Maximum number of kqueues"), 'K', 1),
        (
            RLIMIT_NPTS(),
            L!("Maximum number of pseudo-terminals"),
            'P',
            1,
        ),
        (
            RLIMIT_NTHR(),
            L!("Maximum number of simultaneous threads"),
            'T',
            1,
        ),
    ];

    let unknown = -1;

    let mut resources = Vec::new();
    for resource in resources_info {
        let (resource, desc, switch_char, multiplier) = resource;
        if resource != unknown {
            resources.push(Resource::new(
                resource as c_uint,
                desc,
                switch_char,
                multiplier,
            ));
        }
    }
    resources.into_boxed_slice()
});
