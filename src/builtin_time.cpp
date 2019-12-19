// Functions for executing the time builtin.
#include "config.h"  // IWYU pragma: keep

#include <cerrno>
#include <ctime>
#include <chrono>
#include <cstddef>

#include "builtin.h"
#include "common.h"
#include "exec.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "parser.h"
#include "proc.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

// Measuring time is always complicated with many caveats. Quite apart from the typical
// gotchas faced by developers attempting to choose between monotonic vs non-monotonic and system vs
// cpu clocks, the fact that we are executing as a shell further complicates matters: we can't just
// observe the elapsed CPU time, because that does not reflect the total execution time for both
// ourselves (internal shell execution time and the time it takes for builtins and functions to
// execute) and any external processes we spawn.

// It would be nice to use the C++1 type-safe <chrono> interfaces to measure elapsed time, but that
// unfortunately is underspecified with regards to user/system time and only provides means of
// querying guaranteed monotonicity and resolution for the various clocks. It can be used to measure
// elapsed wall time nicely, but if we would like to provide information more useful for
// benchmarking and tuning then we must turn to either clock_gettime(2), with extensions for thread-
// and process-specific elapsed CPU time, or times(3) for a standard interface to overall process
// and child user/system time elapsed between snapshots. At least on some systems, times(3) has been
// deprecated in favor of getrusage(2), which offers a wider variety of metrics coalesced for SELF,
// THREAD, or CHILDREN.

static uint64_t micros(struct timeval t) {
    return (static_cast<uint64_t>(t.tv_usec) + static_cast<uint64_t>(t.tv_sec * 1E6));
};
static uint64_t micros(struct timespec t) {
    return (static_cast<uint64_t>(t.tv_nsec) / 1E3 + static_cast<uint64_t>(t.tv_sec * 1E6));
};

/// Implementation of time builtin.
int builtin_time(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    int argc = builtin_count_args(argv);

    bool verbose = false;
    // In the future, we can consider accepting more command-line arguments to dictate the behavior
    // of the `time` builtin and what it measures or reports.
    if (argc > 1 && (argv[1] == wcstring(L"-h") || argv[1] == wcstring(L"--help"))) {
        streams.out.append(L"time <command or expression>\n");
        streams.out.append(L"Measures the elapsed wall, system, and user clocks in the execution of"
                L" the given command or expression");
        return 0;
    }
    if (argc > 1 && (argv[1] == wcstring(L"-v") || argv[1] == wcstring(L"--verbose"))) {
        verbose = true;
        argc -= 1;
        argv += 1;
    }

    wcstring new_cmd;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) new_cmd += L' ';
        new_cmd += argv[i];
    }

    int status = STATUS_CMD_OK;
    if (argc > 1) {
        struct rusage fish_usage[2];
        struct rusage child_usage [2];
        struct timespec wall[2] {};

        // Start counters
        getrusage(RUSAGE_SELF, &fish_usage[0]);
        getrusage(RUSAGE_CHILDREN, &child_usage[0]);
        clock_gettime(CLOCK_MONOTONIC, &wall[0]);

        if (parser.eval(std::move(new_cmd), *streams.io_chain, block_type_t::TOP) !=
            eval_result_t::ok) {
            status = STATUS_CMD_ERROR;
        } else {
            status = parser.get_last_status();
        }

        // Stop counters
        getrusage(RUSAGE_SELF, &fish_usage[1]);
        getrusage(RUSAGE_CHILDREN, &child_usage[1]);
        clock_gettime(CLOCK_MONOTONIC, &wall[1]);


        uint64_t fish_sys_micros = micros(fish_usage[1].ru_stime) - micros(fish_usage[0].ru_stime);
        uint64_t fish_usr_micros = micros(fish_usage[1].ru_utime) - micros(fish_usage[0].ru_utime);
        uint64_t child_sys_micros = micros(child_usage[1].ru_stime) - micros(child_usage[0].ru_stime);
        uint64_t child_usr_micros = micros(child_usage[1].ru_utime) - micros(child_usage[0].ru_utime);

        uint64_t net_sys_micros = fish_sys_micros + child_sys_micros;
        uint64_t net_usr_micros = fish_usr_micros + child_usr_micros;
        uint64_t wall_micros = micros(wall[1]) - micros(wall[0]);

        streams.out.append_format(
            L"\n__________________________________"              \
            L"\nExecution completed in %f seconds"               \
            L"\nuser time     %f seconds"                        \
            L"\nsystem time   %f seconds"                        \
            L"\n\n",
            wall_micros / 1.0E6,
            net_usr_micros / 1.0E6,
            net_sys_micros / 1.0E6
        );

    }

    return status;
}
