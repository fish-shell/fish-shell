use std::{cmp::max, mem::MaybeUninit};

use libc::{c_int, rlim_t, rlimit};
use nix::{
    errno::Errno,
    errno::Errno::EPERM,
    sys::resource::{self, getrlimit, setrlimit},
};

use crate::{
    builtins::shared::{
        builtin_missing_argument, builtin_print_error_trailer, builtin_print_help,
        builtin_unknown_option, BUILTIN_ERR_TOO_MANY_ARGUMENTS, STATUS_CMD_ERROR,
        STATUS_INVALID_ARGS,
    },
    ffi::parser_t,
    wchar::{wstr, L},
    wgetopt::{wgetopter_t, wopt, woption, woption_argument_t::no_argument},
    wutil::{fish_wcstoi, fish_wcswidth, wgettext_fmt},
};

use super::shared::{builtin_wperror, io_streams_t, STATUS_CMD_OK};

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
enum ResourceLimit {
    Nix(resource::Resource),
    Other(c_int),
}

impl ResourceLimit {
    fn raw_value(self) -> i32 {
        match self {
            ResourceLimit::Nix(known) => known as i32,
            ResourceLimit::Other(unknown) => unknown,
        }
    }
}

fn getrlimitx(resource: ResourceLimit) -> nix::Result<(rlim_t, rlim_t)> {
    match resource {
        ResourceLimit::Nix(known) => getrlimit(known),
        ResourceLimit::Other(unknown) => {
            let mut old_rlim = MaybeUninit::<rlimit>::uninit();

            let res = unsafe { libc::getrlimit(unknown, old_rlim.as_mut_ptr()) };

            Errno::result(res).map(|_| {
                let rlimit { rlim_cur, rlim_max } = unsafe { old_rlim.assume_init() };
                (rlim_cur, rlim_max)
            })
        }
    }
}

fn setrlimitx(resource: ResourceLimit, soft_limit: rlim_t, hard_limit: rlim_t) -> nix::Result<()> {
    match resource {
        ResourceLimit::Nix(known) => setrlimit(known, soft_limit, hard_limit),
        ResourceLimit::Other(unknown) => {
            let new_rlim = rlimit {
                rlim_cur: soft_limit,
                rlim_max: hard_limit,
            };

            let res = unsafe { libc::setrlimit(unknown, &new_rlim as *const rlimit) };

            Errno::result(res).map(drop)
        }
    }
}

/// Struct describing a resource limit.
#[derive(Debug, Copy, Clone)]
struct Resource<'a> {
    /// resource ID
    resource: ResourceLimit,
    /// description of resource
    desc: &'a wstr,
    /// switch used on commandline to specify resource
    switch_char: char,
    /// the implicit multiplier used when setting getting values
    multiplier: i32,
}

/// Array of `Resource` structs, describing all known resource types.
const RESOURCE_ARR: &[Resource] = &[
    #[cfg(any(target_os = "freebsd", target_os = "dragonfly"))]
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_SBSIZE),
        desc: L!("Maximum size of socket buffers"),
        switch_char: 'b',
        multiplier: 1024,
    },
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_CORE),
        desc: L!("Maximum size of core files created"),
        switch_char: 'c',
        multiplier: 1024,
    },
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_DATA),
        desc: L!("Maximum size of a process’s data segment"),
        switch_char: 'd',
        multiplier: 1024,
    },
    #[cfg(any(target_os = "android", target_os = "linux"))]
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_NICE),
        desc: L!("Control of maximum nice priority"),
        switch_char: 'e',
        multiplier: 1,
    },
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_FSIZE),
        desc: L!("Maximum size of files created by the shell"),
        switch_char: 'f',
        multiplier: 1024,
    },
    #[cfg(any(target_os = "android", target_os = "linux"))]
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_SIGPENDING),
        desc: L!("Maximum number of pending signals"),
        switch_char: 'i',
        multiplier: 1,
    },
    #[cfg(any(
        target_os = "android",
        target_os = "freebsd",
        target_os = "openbsd",
        target_os = "linux",
        target_os = "netbsd"
    ))]
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_MEMLOCK),
        desc: L!("Maximum size that may be locked into memory"),
        switch_char: 'l',
        multiplier: 1024,
    },
    #[cfg(any(
        target_os = "android",
        target_os = "freebsd",
        target_os = "netbsd",
        target_os = "openbsd",
        target_os = "linux",
    ))]
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_RSS),
        desc: L!("Maximum resident set size"),
        switch_char: 'm',
        multiplier: 1024,
    },
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_NOFILE),
        desc: L!("Maximum number of open file descriptors"),
        switch_char: 'n',
        multiplier: 1,
    },
    #[cfg(any(target_os = "android", target_os = "linux"))]
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_MSGQUEUE),
        desc: L!("Maximum bytes in POSIX message queues"),
        switch_char: 'q',
        multiplier: 1024,
    },
    #[cfg(any(target_os = "android", target_os = "linux"))]
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_RTPRIO),
        desc: L!("Maximum realtime scheduling priority"),
        switch_char: 'r',
        multiplier: 1,
    },
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_STACK),
        desc: L!("Maximum stack size"),
        switch_char: 's',
        multiplier: 1024,
    },
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_CPU),
        desc: L!("Maximum amount of CPU time in seconds"),
        switch_char: 't',
        multiplier: 1,
    },
    #[cfg(any(
        target_os = "android",
        target_os = "freebsd",
        target_os = "netbsd",
        target_os = "openbsd",
        target_os = "linux",
    ))]
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_NPROC),
        desc: L!("Maximum number of processes available to current user"),
        switch_char: 'u',
        multiplier: 1,
    },
    #[cfg(not(any(target_os = "freebsd", target_os = "netbsd", target_os = "openbsd")))]
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_AS),
        desc: L!("Maximum amount of virtual memory available to each process"),
        switch_char: 'v',
        multiplier: 1024,
    },
    #[cfg(target_os = "freebsd")]
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_SWAP),
        desc: L!("Maximum swap space"),
        switch_char: 'w',
        multiplier: 1024,
    },
    #[cfg(any(target_os = "linux"))]
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_RTTIME),
        desc: L!("Maximum contiguous realtime CPU time"),
        switch_char: 'y',
        multiplier: 1,
    },
    #[cfg(target_os = "freebsd")]
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_KQUEUES),
        desc: L!("Maximum number of kqueues"),
        switch_char: 'K',
        multiplier: 1,
    },
    #[cfg(target_os = "freebsd")]
    Resource {
        resource: ResourceLimit::Nix(resource::Resource::RLIMIT_NPTS),
        desc: L!("Maximum number of pseudo-terminals"),
        switch_char: 'P',
        multiplier: 1,
    },
];

/// This constant is more platform specific
#[allow(clippy::unnecessary_cast)]
const RLIM_INFINITY: u64 = libc::RLIM_INFINITY as u64;

/// This is likely to be the same as RLIMIT_INFINITY, but it shouldn't get used
/// in the same context (that is, compared to the result of a getrlimit call).
const RLIMIT_UNKNOWN: i32 = -1;

/// Get the implicit multiplication factor for the specified resource limit.
fn get_multiplier(what: ResourceLimit) -> i32 {
    for resource in RESOURCE_ARR {
        if resource.resource == what {
            return resource.multiplier;
        }
    }
    return -1;
}

/// Return the value for the specified resource limit. This function does _not_ multiply the limit
/// value by the multiplier constant used by the commandline ulimit.
fn get(resource: ResourceLimit, hard: bool) -> u64 {
    let (soft_limit, hard_limit) =
        getrlimitx(resource).expect("getrlimit should return valid limits");
    return if hard { hard_limit } else { soft_limit };
}

/// Print the value of the specified resource limit.
fn print(resource: ResourceLimit, hard: bool, streams: &mut io_streams_t) {
    let l = get(resource, hard);

    if l == RLIM_INFINITY {
        streams.out.append(L!("unlimited\n"));
    } else {
        streams.out.append(wgettext_fmt!(
            "%lu\n",
            l / (get_multiplier(resource) as u64)
        ));
    }
}

fn print_all(hard: bool, streams: &mut io_streams_t) {
    let mut w = 0;

    for resource in RESOURCE_ARR {
        w = max(w, fish_wcswidth(resource.desc));
    }

    for resource in RESOURCE_ARR {
        let (soft_limit, hard_limit) =
            getrlimitx(resource.resource).expect("getrlimit should return valid limits");
        let l = if hard { hard_limit } else { soft_limit };

        let unit = if resource.resource == ResourceLimit::Nix(resource::Resource::RLIMIT_CPU) {
            L!("(seconds, ")
        } else if get_multiplier(resource.resource) == 1 {
            L!("(")
        } else {
            L!("(kB, ")
        };

        streams.out.append(wgettext_fmt!(
            "%-*ls %10ls-%lc) ",
            w,
            resource.desc,
            unit,
            resource.switch_char
        ));

        if l == RLIM_INFINITY {
            streams.out.append(L!("unlimited\n"));
        } else {
            streams.out.append(wgettext_fmt!(
                "%lu\n",
                l / (get_multiplier(resource.resource) as u64)
            ));
        }
    }
}

fn get_desc(what: ResourceLimit) -> &'static wstr {
    for resource in RESOURCE_ARR {
        if resource.resource == what {
            return resource.desc;
        }
    }

    return L!("Not a resource");
}

/// Set the new value of the specified resource limit. This function does _not_ multiply the limit
/// value by the multiplier constant used by the commandline ulimit.
fn set_limit(
    resource: ResourceLimit,
    hard: bool,
    soft: bool,
    value: u64,
    streams: &mut io_streams_t,
) -> Option<c_int> {
    let Ok((mut soft_limit, mut hard_limit)) = getrlimitx(resource) else {
        return STATUS_CMD_ERROR;
    };

    if hard {
        hard_limit = value
    }
    if soft {
        soft_limit = value;

        // Do not attempt to set the soft limit higher than the hard limit.
        #[allow(clippy::nonminimal_bool)]
        if (value == RLIM_INFINITY && hard_limit != RLIM_INFINITY)
            || (value != RLIM_INFINITY && hard_limit != RLIM_INFINITY && value > hard_limit)
        {
            soft_limit = hard_limit;
        }
    }

    if let Err(errno) = setrlimitx(resource, soft_limit, hard_limit) {
        if errno == EPERM {
            streams.err.append(wgettext_fmt!(
                "ulimit: Permission denied when changing resource of type '%ls'\n",
                get_desc(resource)
            ));
        } else {
            builtin_wperror(L!("ulimit"), streams);
        }

        return STATUS_CMD_ERROR;
    }

    STATUS_CMD_OK
}

const short_options: &wstr = L!(":HSabcdefilmnqrstuvwyKPTh");
const long_options: &[woption] = &[
    wopt(L!("all"), no_argument, 'a'),
    wopt(L!("hard"), no_argument, 'H'),
    wopt(L!("soft"), no_argument, 'S'),
    wopt(L!("socket-buffers"), no_argument, 'b'),
    wopt(L!("core-size"), no_argument, 'c'),
    wopt(L!("data-size"), no_argument, 'd'),
    wopt(L!("nice"), no_argument, 'e'),
    wopt(L!("file-size"), no_argument, 'f'),
    wopt(L!("pending-signals"), no_argument, 'i'),
    wopt(L!("lock-size"), no_argument, 'l'),
    wopt(L!("resident-set-size"), no_argument, 'm'),
    wopt(L!("file-descriptor-count"), no_argument, 'n'),
    wopt(L!("queue-size"), no_argument, 'q'),
    wopt(L!("realtime-priority"), no_argument, 'r'),
    wopt(L!("stack-size"), no_argument, 's'),
    wopt(L!("cpu-time"), no_argument, 't'),
    wopt(L!("process-count"), no_argument, 'u'),
    wopt(L!("virtual-memory-size"), no_argument, 'v'),
    wopt(L!("swap-size"), no_argument, 'w'),
    wopt(L!("realtime-maxtime"), no_argument, 'y'),
    wopt(L!("kernel-queues"), no_argument, 'K'),
    wopt(L!("ptys"), no_argument, 'P'),
    wopt(L!("threads"), no_argument, 'T'),
    wopt(L!("help"), no_argument, 'h'),
];

pub fn ulimit(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    args: &mut [&wstr],
) -> Option<c_int> {
    let cmd = args[0];
    let argc = args.len();
    let report_all = false;
    let mut hard = false;
    let mut soft = false;
    let what = ResourceLimit::Nix(resource::Resource::RLIMIT_FSIZE);

    let woptind;

    {
        let mut w = wgetopter_t::new(short_options, long_options, args);

        while let Some(c) = w.wgetopt_long() {
            match c {
                'h' => {
                    builtin_print_help(parser, streams, cmd);
                    return STATUS_CMD_OK;
                }
                ':' => {
                    builtin_missing_argument(parser, streams, cmd, args[w.woptind - 1], false);
                    return STATUS_INVALID_ARGS;
                }
                '?' => {
                    builtin_unknown_option(parser, streams, cmd, args[w.woptind - 1], false);
                    return STATUS_INVALID_ARGS;
                }
                _ => panic!("unexpected value from wgetopt_long"),
            }
        }

        woptind = w.woptind;
    }

    if report_all {
        print_all(hard, streams);
        return STATUS_CMD_OK;
    }

    if what.raw_value() == RLIMIT_UNKNOWN {
        streams.err.append(wgettext_fmt!(
            "%ls: Resource limit not available on this operating system",
            cmd
        ));
        builtin_print_error_trailer(parser, streams, cmd);
        return STATUS_INVALID_ARGS;
    }

    let arg_count = argc - woptind;
    if arg_count == 0 {
        print(what, hard, streams);
        return STATUS_CMD_OK;
    } else if arg_count != 1 {
        streams
            .err
            .append(wgettext_fmt!(BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd));
        builtin_print_error_trailer(parser, streams, cmd);
        return STATUS_INVALID_ARGS;
    }

    if !hard && !soft {
        soft = true;
        hard = true;
    }

    let mut new_limit: u64;
    if args[woptind].is_empty() {
        streams.err.append(wgettext_fmt!(
            "%ls: New limit cannot be an empty string\n",
            cmd
        ));
        builtin_print_error_trailer(parser, streams, cmd);
        return STATUS_INVALID_ARGS;
    } else if args[woptind] == L!("unlimited") {
        new_limit = RLIM_INFINITY;
    } else if args[woptind] == L!("hard") {
        new_limit = get(what, true);
    } else if args[woptind] == L!("soft") {
        new_limit = get(what, soft);
    } else {
        match fish_wcstoi(args[woptind]) {
            Ok(lim) => {
                new_limit = lim;
                new_limit *= get_multiplier(what) as u64
            }
            Err(_) => {
                streams
                    .err
                    .append(wgettext_fmt!("%ls: Invalid limit '%ls'\n", cmd, woptind));
                builtin_print_error_trailer(parser, streams, cmd);
                return STATUS_INVALID_ARGS;
            }
        }
    }

    return set_limit(what, hard, soft, new_limit, streams);
}
