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

#include <algorithm>
#include <string.h>
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

// Linux makes available CLOCK_MONOTONIC_RAW, which is monotonic even in the presence of NTP
// adjustments.
#ifdef CLOCK_MONOTONIC_RAW
    #define CLOCK_SRC CLOCK_MONOTONIC_RAW
#else
    #define CLOCK_SRC CLOCK_MONOTONIC
#endif

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
        clock_gettime(CLOCK_SRC, &wall[0]);

        if (parser.eval(std::move(new_cmd), *streams.io_chain, block_type_t::TOP) !=
            eval_result_t::ok) {
            status = STATUS_CMD_ERROR;
        } else {
            status = parser.get_last_status();
        }

        // Stop counters
        getrusage(RUSAGE_SELF, &fish_usage[1]);
        getrusage(RUSAGE_CHILDREN, &child_usage[1]);
        clock_gettime(CLOCK_SRC, &wall[1]);

        int64_t fish_sys_micros = micros(fish_usage[1].ru_stime) - micros(fish_usage[0].ru_stime);
        int64_t fish_usr_micros = micros(fish_usage[1].ru_utime) - micros(fish_usage[0].ru_utime);
        int64_t child_sys_micros = micros(child_usage[1].ru_stime) - micros(child_usage[0].ru_stime);
        int64_t child_usr_micros = micros(child_usage[1].ru_utime) - micros(child_usage[0].ru_utime);

        // The result from getrusage is not necessarily realtime, it may be cached a few
        // microseconds behind. In the event that execution completes extremely quickly or there is
        // no data (say, we are measuring external execution time but no external processes have
        // been launched), it can incorrectly appear to be negative.
        fish_sys_micros = std::max(int64_t(0), fish_sys_micros);
        fish_usr_micros = std::max(int64_t(0), fish_usr_micros);
        child_sys_micros = std::max(int64_t(0), child_sys_micros);
        child_usr_micros = std::max(int64_t(0), child_usr_micros);

        int64_t net_sys_micros = fish_sys_micros + child_sys_micros;
        int64_t net_usr_micros = fish_usr_micros + child_usr_micros;
        int64_t net_wall_micros = micros(wall[1]) - micros(wall[0]);

        enum class tunit {
            minutes,
            seconds,
            milliseconds,
            microseconds,
        };

        auto get_unit = [](int64_t micros) {
            if (micros > 900 * 1E6) {
                return tunit::minutes;
            } else if (micros > 1 * 1E6) {
                return tunit::seconds;
            } else if (micros > 1E3) {
                return tunit::milliseconds;
            } else {
                return tunit::microseconds;
            }
        };

        auto unit_name = [](tunit unit) {
            switch (unit) {
                case tunit::minutes: return "minutes";
                case tunit::seconds: return "seconds";
                case tunit::milliseconds: return "milliseconds";
                case tunit::microseconds: return "microseconds";
            }
        };

        auto unit_short_name = [](tunit unit) {
            switch (unit) {
                case tunit::minutes: return "mins";
                case tunit::seconds: return "secs";
                case tunit::milliseconds: return "millis";
                case tunit::microseconds: return "micros";
            }
        };

        auto convert = [](int64_t micros, tunit unit) {
            switch (unit) {
                case tunit::minutes: return micros / 1.0E6 / 60.0;
                case tunit::seconds: return micros / 1.0E6;
                case tunit::milliseconds: return micros / 1.0E3;
                case tunit::microseconds: return micros / 1.0;
            }
        };

        auto wall_unit = get_unit(net_wall_micros);
        auto cpu_unit = get_unit((net_sys_micros + net_usr_micros) / 2);
        auto wall_time = convert(net_wall_micros, wall_unit);
        auto usr_time = convert(net_usr_micros, cpu_unit);
        auto sys_time = convert(net_sys_micros, cpu_unit);

        if (!verbose) {
            streams.err.append_format(
                L"\n_______________________________"    \
                L"\nExecuted in  %6.2F %s"              \
                L"\n   usr time  %6.2F %s"              \
                L"\n   sys time  %6.2F %s"              \
                L"\n\n",
                wall_time, unit_name(wall_unit),
                usr_time, unit_name(cpu_unit),
                sys_time, unit_name(cpu_unit)
            );
        } else {
            auto fish_unit = get_unit((fish_sys_micros + fish_usr_micros) / 2);
            auto child_unit = get_unit((child_sys_micros + child_usr_micros) / 2);
            auto fish_usr_time = convert(fish_usr_micros, fish_unit);
            auto fish_sys_time = convert(fish_sys_micros, fish_unit);
            auto child_usr_time = convert(child_usr_micros, child_unit);
            auto child_sys_time = convert(child_sys_micros, child_unit);

            streams.err.append_format(
                L"\n________________________________________________________"    \
                L"\nExecuted in  %6.2F %s   %*s           %*s "                  \
                L"\n   usr time  %6.2F %s  %6.2F %s  %6.2F %s "                  \
                L"\n   sys time  %6.2F %s  %6.2F %s  %6.2F %s "                  \
                L"\n\n",
                wall_time, unit_short_name(wall_unit),
                strlen(unit_short_name(wall_unit)) - 1, "fish",
                strlen(unit_short_name(fish_unit)) - 1, "external",
                usr_time, unit_short_name(cpu_unit),
                fish_usr_time, unit_short_name(fish_unit),
                child_usr_time, unit_short_name(child_unit),
                sys_time, unit_short_name(cpu_unit),
                fish_sys_time, unit_short_name(fish_unit),
                child_sys_time, unit_short_name(child_unit)
            );

        }

    }

    return status;
}
