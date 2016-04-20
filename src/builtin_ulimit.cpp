// Functions used for implementing the ulimit builtin.
#include <errno.h>
#include <sys/resource.h>
#include <wchar.h>

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
static int set(int resource, int hard, int soft, rlim_t value, io_streams_t &streams) {
    struct rlimit ls;
    getrlimit(resource, &ls);

    if (hard) {
        ls.rlim_max = value;
    }

    if (soft) {
        ls.rlim_cur = value;

        // Do not attempt to set the soft limit higher than the hard limit.
        if ((value == RLIM_INFINITY && ls.rlim_max != RLIM_INFINITY) ||
            (value != RLIM_INFINITY && ls.rlim_max != RLIM_INFINITY && value > ls.rlim_max)) {
            ls.rlim_cur = ls.rlim_max;
        }
    }

    if (setrlimit(resource, &ls)) {
        if (errno == EPERM)
            streams.err.append_format(
                L"ulimit: Permission denied when changing resource of type '%ls'\n",
                get_desc(resource));
        else
            builtin_wperror(L"ulimit", streams);
        return 1;
    }
    return 0;
}

/// The ulimit builtin, used for setting resource limits.
int builtin_ulimit(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wgetopter_t w;
    int hard = 0;
    int soft = 0;

    int what = RLIMIT_FSIZE;
    int report_all = 0;

    int argc = builtin_count_args(argv);

    w.woptind = 0;

    while (1) {
        static const struct woption long_options[] = {
            {L"all", no_argument, 0, 'a'},
            {L"hard", no_argument, 0, 'H'},
            {L"soft", no_argument, 0, 'S'},
            {L"core-size", no_argument, 0, 'c'},
            {L"data-size", no_argument, 0, 'd'},
            {L"file-size", no_argument, 0, 'f'},
            {L"lock-size", no_argument, 0, 'l'},
            {L"resident-set-size", no_argument, 0, 'm'},
            {L"file-descriptor-count", no_argument, 0, 'n'},
            {L"stack-size", no_argument, 0, 's'},
            {L"cpu-time", no_argument, 0, 't'},
            {L"process-count", no_argument, 0, 'u'},
            {L"virtual-memory-size", no_argument, 0, 'v'},
            {L"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}};

        int opt_index = 0;

        int opt = w.wgetopt_long(argc, argv, L"aHScdflmnstuvh", long_options, &opt_index);
        if (opt == -1) break;

        switch (opt) {
            case 0: {
                if (long_options[opt_index].flag != 0) break;
                streams.err.append_format(BUILTIN_ERR_UNKNOWN, argv[0],
                                          long_options[opt_index].name);
                builtin_print_help(parser, streams, argv[0], streams.err);
                return 1;
            }
            case L'a': {
                report_all = 1;
                break;
            }
            case L'H': {
                hard = 1;
                break;
            }
            case L'S': {
                soft = 1;
                break;
            }
            case L'c': {
                what = RLIMIT_CORE;
                break;
            }
            case L'd': {
                what = RLIMIT_DATA;
                break;
            }
            case L'f': {
                what = RLIMIT_FSIZE;
                break;
            }
#ifdef RLIMIT_MEMLOCK
            case L'l': {
                what = RLIMIT_MEMLOCK;
                break;
            }
#endif
#ifdef RLIMIT_RSS
            case L'm': {
                what = RLIMIT_RSS;
                break;
            }
#endif
            case L'n': {
                what = RLIMIT_NOFILE;
                break;
            }
            case L's': {
                what = RLIMIT_STACK;
                break;
            }
            case L't': {
                what = RLIMIT_CPU;
                break;
            }
#ifdef RLIMIT_NPROC
            case L'u': {
                what = RLIMIT_NPROC;
                break;
            }
#endif
#ifdef RLIMIT_AS
            case L'v': {
                what = RLIMIT_AS;
                break;
            }
#endif
            case L'h': {
                builtin_print_help(parser, streams, argv[0], streams.out);
                return 0;
            }
            case L'?': {
                builtin_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return 1;
            }
        }
    }

    if (report_all) {
        if (argc - w.woptind == 0) {
            print_all(hard, streams);
        } else {
            streams.err.append(argv[0]);
            streams.err.append(L": Too many arguments\n");
            builtin_print_help(parser, streams, argv[0], streams.err);
            return 1;
        }

        return 0;
    }

    switch (argc - w.woptind) {
        case 0: {  // show current limit value
            print(what, hard, streams);
            break;
        }
        case 1: {  // change current limit value
            rlim_t new_limit;
            wchar_t *end;

            // Set both hard and soft limits if nothing else was specified.
            if (!(hard + soft)) {
                hard = soft = 1;
            }

            if (wcscasecmp(argv[w.woptind], L"unlimited") == 0) {
                new_limit = RLIM_INFINITY;
            } else if (wcscasecmp(argv[w.woptind], L"hard") == 0) {
                new_limit = get(what, 1);
            } else if (wcscasecmp(argv[w.woptind], L"soft") == 0) {
                new_limit = get(what, soft);
            } else {
                errno = 0;
                new_limit = wcstol(argv[w.woptind], &end, 10);
                if (errno || *end) {
                    streams.err.append_format(L"%ls: Invalid limit '%ls'\n", argv[0],
                                              argv[w.woptind]);
                    builtin_print_help(parser, streams, argv[0], streams.err);
                    return 1;
                }
                new_limit *= get_multiplier(what);
            }

            return set(what, hard, soft, new_limit, streams);
        }
        default: {
            streams.err.append(argv[0]);
            streams.err.append(L": Too many arguments\n");
            builtin_print_help(parser, streams, argv[0], streams.err);
            return 1;
        }
    }
    return 0;
}
