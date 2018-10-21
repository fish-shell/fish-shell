// Functions used for implementing the ulimit builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stddef.h>
#include <sys/resource.h>

#include "builtin.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "util.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

class parser_t;

/// Struct describing a resource limit.
struct resource_t {
    int resource;         // resource ID
    const wchar_t *desc;  // description of resource
    wchar_t switch_char;  // switch used on commandline to specify resource
    int multiplier;       // the implicit multiplier used when setting getting values
};

/// Array of resource_t structs, describing all known resource types.
static const struct resource_t resource_arr[] = {
    {RLIMIT_CORE, L"Maximum size of core files created", L'c', 1024},
    {RLIMIT_DATA, L"Maximum size of a process’s data segment", L'd', 1024},
    {RLIMIT_FSIZE, L"Maximum size of files created by the shell", L'f', 1024},
#ifdef RLIMIT_MEMLOCK
    {RLIMIT_MEMLOCK, L"Maximum size that may be locked into memory", L'l', 1024},
#endif
#ifdef RLIMIT_RSS
    {RLIMIT_RSS, L"Maximum resident set size", L'm', 1024},
#endif
    {RLIMIT_NOFILE, L"Maximum number of open file descriptors", L'n', 1},
    {RLIMIT_STACK, L"Maximum stack size", L's', 1024},
    {RLIMIT_CPU, L"Maximum amount of cpu time in seconds", L't', 1},
#ifdef RLIMIT_NPROC
    {RLIMIT_NPROC, L"Maximum number of processes available to a single user", L'u', 1},
#endif
#ifdef RLIMIT_AS
    {RLIMIT_AS, L"Maximum amount of virtual memory available to the shell", L'v', 1024},
#endif
    {0, 0, 0, 0}};

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
        streams.out.append(L"unlimited\n");
    else
        streams.out.append_format(L"%d\n", l / get_multiplier(resource));
}

/// Print values of all resource limits.
static void print_all(int hard, io_streams_t &streams) {
    int i;
    int w = 0;

    for (i = 0; resource_arr[i].desc; i++) {
        w = maxi(w, fish_wcswidth(resource_arr[i].desc));
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

        streams.out.append_format(L"%-*ls %10ls-%lc) ", w, resource_arr[i].desc, unit,
                                  resource_arr[i].switch_char);

        if (l == RLIM_INFINITY) {
            streams.out.append(L"unlimited\n");
        } else {
            streams.out.append_format(L"%d\n", l / get_multiplier(resource_arr[i].resource));
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
            streams.err.append_format(
                L"ulimit: Permission denied when changing resource of type '%ls'\n",
                get_desc(resource));
        } else {
            builtin_wperror(L"ulimit", streams);
        }
        return STATUS_CMD_ERROR;
    }
    return STATUS_CMD_OK;
}

/// The ulimit builtin, used for setting resource limits.
int builtin_ulimit(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    bool report_all = false;
    bool hard = false;
    bool soft = false;
    int what = RLIMIT_FSIZE;

    static const wchar_t *const short_options = L":HSacdflmnstuvh";
    static const struct woption long_options[] = {
        {L"all", no_argument, NULL, 'a'},
        {L"hard", no_argument, NULL, 'H'},
        {L"soft", no_argument, NULL, 'S'},
        {L"core-size", no_argument, NULL, 'c'},
        {L"data-size", no_argument, NULL, 'd'},
        {L"file-size", no_argument, NULL, 'f'},
        {L"lock-size", no_argument, NULL, 'l'},
        {L"resident-set-size", no_argument, NULL, 'm'},
        {L"file-descriptor-count", no_argument, NULL, 'n'},
        {L"stack-size", no_argument, NULL, 's'},
        {L"cpu-time", no_argument, NULL, 't'},
        {L"process-count", no_argument, NULL, 'u'},
        {L"virtual-memory-size", no_argument, NULL, 'v'},
        {L"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}};

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
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
            case 'c': {
                what = RLIMIT_CORE;
                break;
            }
            case 'd': {
                what = RLIMIT_DATA;
                break;
            }
            case 'f': {
                what = RLIMIT_FSIZE;
                break;
            }
#ifdef RLIMIT_MEMLOCK
            case 'l': {
                what = RLIMIT_MEMLOCK;
                break;
            }
#endif
#ifdef RLIMIT_RSS
            case 'm': {
                what = RLIMIT_RSS;
                break;
            }
#endif
            case 'n': {
                what = RLIMIT_NOFILE;
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
#ifdef RLIMIT_NPROC
            case 'u': {
                what = RLIMIT_NPROC;
                break;
            }
#endif
#ifdef RLIMIT_AS
            case 'v': {
                what = RLIMIT_AS;
                break;
            }
#endif
            case 'h': {
                builtin_print_help(parser, streams, cmd, streams.out);
                return STATUS_CMD_OK;
            }
            case ':': {
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            case '?': {
                builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            default: {
                DIE("unexpected retval from wgetopt_long");
                break;
            }
        }
    }

    if (report_all) {
        print_all(hard, streams);
        return STATUS_CMD_OK;
    }

    int arg_count = argc - w.woptind;
    if (arg_count == 0) {
        // Show current limit value.
        print(what, hard, streams);
        return STATUS_CMD_OK;
    } else if (arg_count != 1) {
        streams.err.append_format(BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
        return STATUS_INVALID_ARGS;
    }

    // Change current limit value.
    if (!hard && !soft) {
        // Set both hard and soft limits if neither was specified.
        hard = soft = true;
    }

    rlim_t new_limit;
    if (*argv[w.woptind] == L'\0') {
        streams.err.append_format(_(L"%ls: New limit cannot be an empty string\n"), cmd);
        builtin_print_help(parser, streams, cmd, streams.err);
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
            streams.err.append_format(_(L"%ls: Invalid limit '%ls'\n"), cmd, argv[w.woptind]);
            builtin_print_help(parser, streams, cmd, streams.err);
            return STATUS_INVALID_ARGS;
        }
        new_limit *= get_multiplier(what);
    }

    return set_limit(what, hard, soft, new_limit, streams);
}
