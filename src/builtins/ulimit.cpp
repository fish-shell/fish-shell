// Functions used for implementing the ulimit builtin.
#include "config.h"  // IWYU pragma: keep

#include "ulimit.h"

#include <sys/resource.h>

#include <algorithm>
#include <cerrno>
#include <cwchar>

#include "../builtin.h"
#include "../common.h"
#include "../fallback.h"  // IWYU pragma: keep
#include "../io.h"
#include "../maybe.h"
#include "../wgetopt.h"
#include "../wutil.h"  // IWYU pragma: keep
#include "builtins/shared.rs.h"
#include "builtins/ulimit.h"

/// Struct describing a resource limit.
struct resource_t {
    int resource;         // resource ID
    const wchar_t *desc;  // description of resource
    wchar_t switch_char;  // switch used on commandline to specify resource
    int multiplier;       // the implicit multiplier used when setting getting values
};

/// Array of resource_t structs, describing all known resource types.
static const struct resource_t resource_arr[] = {
#ifdef RLIMIT_SBSIZE
    {RLIMIT_SBSIZE, L"Maximum size of socket buffers", L'b', 1024},
#endif
    {RLIMIT_CORE, L"Maximum size of core files created", L'c', 1024},
    {RLIMIT_DATA, L"Maximum size of a process’s data segment", L'd', 1024},
#ifdef RLIMIT_NICE
    {RLIMIT_NICE, L"Control of maximum nice priority", L'e', 1},
#endif
    {RLIMIT_FSIZE, L"Maximum size of files created by the shell", L'f', 1024},
#ifdef RLIMIT_SIGPENDING
    {RLIMIT_SIGPENDING, L"Maximum number of pending signals", L'i', 1},
#endif
#ifdef RLIMIT_MEMLOCK
    {RLIMIT_MEMLOCK, L"Maximum size that may be locked into memory", L'l', 1024},
#endif
#ifdef RLIMIT_RSS
    {RLIMIT_RSS, L"Maximum resident set size", L'm', 1024},
#endif
    {RLIMIT_NOFILE, L"Maximum number of open file descriptors", L'n', 1},
#ifdef RLIMIT_MSGQUEUE
    {RLIMIT_MSGQUEUE, L"Maximum bytes in POSIX message queues", L'q', 1024},
#endif
#ifdef RLIMIT_RTPRIO
    {RLIMIT_RTPRIO, L"Maximum realtime scheduling priority", L'r', 1},
#endif
    {RLIMIT_STACK, L"Maximum stack size", L's', 1024},
    {RLIMIT_CPU, L"Maximum amount of CPU time in seconds", L't', 1},
#ifdef RLIMIT_NPROC
    {RLIMIT_NPROC, L"Maximum number of processes available to current user", L'u', 1},
#endif
#ifdef RLIMIT_AS
    {RLIMIT_AS, L"Maximum amount of virtual memory available to each process", L'v', 1024},
#endif
#ifdef RLIMIT_SWAP
    {RLIMIT_SWAP, L"Maximum swap space", L'w', 1024},
#endif
#ifdef RLIMIT_RTTIME
    {RLIMIT_RTTIME, L"Maximum contiguous realtime CPU time", L'y', 1},
#endif
#ifdef RLIMIT_KQUEUES
    {RLIMIT_KQUEUES, L"Maximum number of kqueues", L'K', 1},
#endif
#ifdef RLIMIT_NPTS
    {RLIMIT_NPTS, L"Maximum number of pseudo-terminals", L'P', 1},
#endif
#ifdef RLIMIT_NTHR
    {RLIMIT_NTHR, L"Maximum number of simultaneous threads", L'T', 1},
#endif
    {0, nullptr, 0, 0}};

/// This is likely to be the same as RLIMIT_INFINITY, but it shouldn't get used
/// in the same context (that is, compared to the result of a getrlimit call).
#define RLIMIT_UNKNOWN -1

/// Get the implicit multiplication factor for the specified resource limit.
static int get_multiplier(int what) {
    for (int i = 0; resource_arr[i].desc; i++) {
        if (resource_arr[i].resource == what) {
            return resource_arr[i].multiplier;
        }
    }
    return -1;
}

/// Return the value for the specified resource limit. This function does _not_ multiply the limit
/// value by the multiplier constant used by the commandline ulimit.
static rlim_t get(int resource, int hard) {
    struct rlimit ls;

    getrlimit(resource, &ls);

    return hard ? ls.rlim_max : ls.rlim_cur;
}

/// Print the value of the specified resource limit.
static void print(int resource, int hard, io_streams_t &streams) {
    rlim_t l = get(resource, hard);

    if (l == RLIM_INFINITY)
        streams.out()->append(format_string(L"unlimited\n"));
    else
        streams.out()->append(format_string(L"%lu\n", l / get_multiplier(resource)));
}

/// Print values of all resource limits.
static void print_all(int hard, io_streams_t &streams) {
    int i;
    int w = 0;

    for (i = 0; resource_arr[i].desc; i++) {
        w = std::max(w, fish_wcswidth(resource_arr[i].desc));
    }

    for (i = 0; resource_arr[i].desc; i++) {
        struct rlimit ls;
        rlim_t l;
        getrlimit(resource_arr[i].resource, &ls);
        l = hard ? ls.rlim_max : ls.rlim_cur;

        const wchar_t *unit =
            ((resource_arr[i].resource == RLIMIT_CPU)
                 ? L"(seconds, "
                 : (get_multiplier(resource_arr[i].resource) == 1 ? L"(" : L"(kB, "));

        streams.out()->append(format_string(L"%-*ls %10ls-%lc) ", w, resource_arr[i].desc, unit,
                                            resource_arr[i].switch_char));

        if (l == RLIM_INFINITY) {
            streams.out()->append(format_string(L"unlimited\n"));
        } else {
            streams.out()->append(
                format_string(L"%lu\n", l / get_multiplier(resource_arr[i].resource)));
        }
    }
}

/// Returns the description for the specified resource limit.
static const wchar_t *get_desc(int what) {
    int i;

    for (i = 0; resource_arr[i].desc; i++) {
        if (resource_arr[i].resource == what) {
            return resource_arr[i].desc;
        }
    }
    return L"Not a resource";
}

/// Set the new value of the specified resource limit. This function does _not_ multiply the limit
// value by the multiplier constant used by the commandline ulimit.
static int set_limit(int resource, int hard, int soft, rlim_t value, io_streams_t &streams) {
    struct rlimit ls;

    getrlimit(resource, &ls);
    if (hard) ls.rlim_max = value;
    if (soft) {
        ls.rlim_cur = value;

        // Do not attempt to set the soft limit higher than the hard limit.
        if ((value == RLIM_INFINITY && ls.rlim_max != RLIM_INFINITY) ||
            (value != RLIM_INFINITY && ls.rlim_max != RLIM_INFINITY && value > ls.rlim_max)) {
            ls.rlim_cur = ls.rlim_max;
        }
    }

    if (setrlimit(resource, &ls)) {
        if (errno == EPERM) {
            streams.err()->append(
                format_string(L"ulimit: Permission denied when changing resource of type '%ls'\n",
                              get_desc(resource)));
        } else {
            builtin_wperror(L"ulimit", streams);
        }
        return STATUS_CMD_ERROR;
    }
    return STATUS_CMD_OK;
}

/// The ulimit builtin, used for setting resource limits.
int builtin_ulimit(const void *_parser, void *_streams, void *_argv) {
    const auto &parser = *static_cast<const parser_t *>(_parser);
    auto &streams = *static_cast<io_streams_t *>(_streams);
    auto argv = static_cast<const wchar_t **>(_argv);
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    bool report_all = false;
    bool hard = false;
    bool soft = false;
    int what = RLIMIT_FSIZE;

    static const wchar_t *const short_options = L":HSabcdefilmnqrstuvwyKPTh";
    static const struct woption long_options[] = {{L"all", no_argument, 'a'},
                                                  {L"hard", no_argument, 'H'},
                                                  {L"soft", no_argument, 'S'},
                                                  {L"socket-buffers", no_argument, 'b'},
                                                  {L"core-size", no_argument, 'c'},
                                                  {L"data-size", no_argument, 'd'},
                                                  {L"nice", no_argument, 'e'},
                                                  {L"file-size", no_argument, 'f'},
                                                  {L"pending-signals", no_argument, 'i'},
                                                  {L"lock-size", no_argument, 'l'},
                                                  {L"resident-set-size", no_argument, 'm'},
                                                  {L"file-descriptor-count", no_argument, 'n'},
                                                  {L"queue-size", no_argument, 'q'},
                                                  {L"realtime-priority", no_argument, 'r'},
                                                  {L"stack-size", no_argument, 's'},
                                                  {L"cpu-time", no_argument, 't'},
                                                  {L"process-count", no_argument, 'u'},
                                                  {L"virtual-memory-size", no_argument, 'v'},
                                                  {L"swap-size", no_argument, 'w'},
                                                  {L"realtime-maxtime", no_argument, 'y'},
                                                  {L"kernel-queues", no_argument, 'K'},
                                                  {L"ptys", no_argument, 'P'},
                                                  {L"threads", no_argument, 'T'},
                                                  {L"help", no_argument, 'h'},
                                                  {}};

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case 'a': {
                report_all = true;
                break;
            }
            case 'H': {
                hard = true;
                break;
            }
            case 'S': {
                soft = true;
                break;
            }
            case 'b': {
#ifdef RLIMIT_SBSIZE
                what = RLIMIT_SBSIZE;
#else
                what = RLIMIT_UNKNOWN;
#endif
                break;
            }
            case 'c': {
                what = RLIMIT_CORE;
                break;
            }
            case 'd': {
                what = RLIMIT_DATA;
                break;
            }
            case 'e': {
#ifdef RLIMIT_NICE
                what = RLIMIT_NICE;
#else
                what = RLIMIT_UNKNOWN;
#endif
                break;
            }
            case 'f': {
                what = RLIMIT_FSIZE;
                break;
            }
            case 'i': {
#ifdef RLIMIT_SIGPENDING
                what = RLIMIT_SIGPENDING;
#else
                what = RLIMIT_UNKNOWN;
#endif
                break;
            }
            case 'l': {
#ifdef RLIMIT_MEMLOCK
                what = RLIMIT_MEMLOCK;
#else
                what = RLIMIT_UNKNOWN;
#endif
                break;
            }
            case 'm': {
#ifdef RLIMIT_RSS
                what = RLIMIT_RSS;
#else
                what = RLIMIT_UNKNOWN;
#endif
                break;
            }
            case 'n': {
                what = RLIMIT_NOFILE;
                break;
            }
            case 'q': {
#ifdef RLIMIT_MSGQUEUE
                what = RLIMIT_MSGQUEUE;
#else
                what = RLIMIT_UNKNOWN;
#endif
                break;
            }
            case 'r': {
#ifdef RLIMIT_RTPRIO
                what = RLIMIT_RTPRIO;
#else
                what = RLIMIT_UNKNOWN;
#endif
                break;
            }
            case 's': {
                what = RLIMIT_STACK;
                break;
            }
            case 't': {
                what = RLIMIT_CPU;
                break;
            }
            case 'u': {
#ifdef RLIMIT_NPROC
                what = RLIMIT_NPROC;
#else
                what = RLIMIT_UNKNOWN;
#endif
                break;
            }
            case 'v': {
#ifdef RLIMIT_AS
                what = RLIMIT_AS;
#else
                what = RLIMIT_UNKNOWN;
#endif
                break;
            }
            case 'w': {
#ifdef RLIMIT_SWAP
                what = RLIMIT_SWAP;
#else
                what = RLIMIT_UNKNOWN;
#endif
                break;
            }
            case 'y': {
#ifdef RLIMIT_RTTIME
                what = RLIMIT_RTTIME;
#else
                what = RLIMIT_UNKNOWN;
#endif
                break;
            }
            case 'K': {
#ifdef RLIMIT_KQUEUES
                what = RLIMIT_KQUEUES;
#else
                what = RLIMIT_UNKNOWN;
#endif
                break;
            }
            case 'P': {
#ifdef RLIMIT_NPTS
                what = RLIMIT_NPTS;
#else
                what = RLIMIT_UNKNOWN;
#endif
                break;
            }
            case 'T': {
#ifdef RLIMIT_NTHR
                what = RLIMIT_NTHR;
#else
                what = RLIMIT_UNKNOWN;
#endif
                break;
            }
            case 'h': {
                builtin_print_help(parser, streams, cmd);
                return STATUS_CMD_OK;
            }
            case ':': {
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1], true);
                return STATUS_INVALID_ARGS;
            }
            case '?': {
                builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1], true);
                return STATUS_INVALID_ARGS;
            }
            default: {
                DIE("unexpected retval from wgetopt_long");
            }
        }
    }

    if (report_all) {
        print_all(hard, streams);
        return STATUS_CMD_OK;
    }

    if (what == RLIMIT_UNKNOWN) {
        streams.err()->append(
            format_string(_(L"%ls: Resource limit not available on this operating system\n"), cmd));
        builtin_print_error_trailer(parser, *streams.err(), cmd);
        return STATUS_INVALID_ARGS;
    }

    int arg_count = argc - w.woptind;
    if (arg_count == 0) {
        // Show current limit value.
        print(what, hard, streams);
        return STATUS_CMD_OK;
    } else if (arg_count != 1) {
        streams.err()->append(format_string(BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd));
        builtin_print_error_trailer(parser, *streams.err(), cmd);
        return STATUS_INVALID_ARGS;
    }

    // Change current limit value.
    if (!hard && !soft) {
        // Set both hard and soft limits if neither was specified.
        hard = soft = true;
    }

    rlim_t new_limit;
    if (*argv[w.woptind] == L'\0') {
        streams.err()->append(format_string(_(L"%ls: New limit cannot be an empty string\n"), cmd));
        builtin_print_error_trailer(parser, *streams.err(), cmd);
        return STATUS_INVALID_ARGS;
    } else if (wcscasecmp(argv[w.woptind], L"unlimited") == 0) {
        new_limit = RLIM_INFINITY;
    } else if (wcscasecmp(argv[w.woptind], L"hard") == 0) {
        new_limit = get(what, 1);
    } else if (wcscasecmp(argv[w.woptind], L"soft") == 0) {
        new_limit = get(what, soft);
    } else {
        new_limit = fish_wcstol(argv[w.woptind]);
        if (errno) {
            streams.err()->append(
                format_string(_(L"%ls: Invalid limit '%ls'\n"), cmd, argv[w.woptind]));
            builtin_print_error_trailer(parser, *streams.err(), cmd);
            return STATUS_INVALID_ARGS;
        }
        new_limit *= get_multiplier(what);
    }

    return set_limit(what, hard, soft, new_limit, streams);
}
