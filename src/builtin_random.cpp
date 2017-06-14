// Implementation of the random builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#include <algorithm>
#include <random>

#include "builtin.h"
#include "builtin_random.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

struct cmd_opts {
    bool print_help = false;
};

static const wchar_t *short_options = L"h";
static const struct woption long_options[] = {{L"help", no_argument, NULL, 'h'},
                                              {NULL, 0, NULL, 0}};

static int parse_cmd_opts(struct cmd_opts *opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {  //!OCLINT(too few branches)
            case 'h': {
                opts->print_help = true;
                return STATUS_CMD_OK;
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

    *optind = w.woptind;
    return STATUS_CMD_OK;
}

/// The random builtin generates random numbers.
int builtin_random(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    struct cmd_opts opts;

    int optind;
    int retval = parse_cmd_opts(&opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    static bool seeded = false;
    static std::minstd_rand engine;
    if (!seeded) {
        // seed engine with 2*32 bits of random data
        // for the 64 bits of internal state of minstd_rand
        std::random_device rd;
        std::seed_seq seed{rd(), rd()};
        engine.seed(seed);
        seeded = true;
    }

    int arg_count = argc - optind;
    long long start, end;
    unsigned long long step;
    bool choice = false;
    if (arg_count >= 1 && !wcscmp(argv[optind], L"choice")) {
        if (arg_count == 1) {
            streams.err.append_format(L"%ls: nothing to choose from\n", cmd);
            return STATUS_INVALID_ARGS;
        }
        choice = true;
        start = 1;
        step = 1;
        end = arg_count - 1;
    } else {
        bool parse_error = false;
        auto parse_ll = [&](const wchar_t *str) {
            long long ll = fish_wcstoll(str);
            if (errno) {
                streams.err.append_format(L"%ls: %ls is not a valid integer\n", cmd, str);
                parse_error = true;
            }
            return ll;
        };
        auto parse_ull = [&](const wchar_t *str) {
            unsigned long long ull = fish_wcstoull(str);
            if (errno) {
                streams.err.append_format(L"%ls: %ls is not a valid integer\n", cmd, str);
                parse_error = true;
            }
            return ull;
        };
        if (arg_count == 0) {
            start = 0;
            end = 32767;
            step = 1;
        } else if (arg_count == 1) {
            long long seed = parse_ll(argv[optind]);
            if (parse_error) return STATUS_INVALID_ARGS;
            engine.seed(static_cast<uint32_t>(seed));
            return STATUS_CMD_OK;
        } else if (arg_count == 2) {
            start = parse_ll(argv[optind]);
            step = 1;
            end = parse_ll(argv[optind + 1]);
        } else if (arg_count == 3) {
            start = parse_ll(argv[optind]);
            step = parse_ull(argv[optind + 1]);
            end = parse_ll(argv[optind + 2]);
        } else {
            streams.err.append_format(BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd);
            return STATUS_INVALID_ARGS;
        }

        if (parse_error) {
            return STATUS_INVALID_ARGS;
        } else if (start >= end) {
            streams.err.append_format(L"%ls: END must be greater than START\n", cmd);
            return STATUS_INVALID_ARGS;
        } else if (step == 0) {
            streams.err.append_format(L"%ls: STEP must be a positive integer\n", cmd);
            return STATUS_INVALID_ARGS;
        }
    }

    // only for negative argument
    auto safe_abs = [](long long ll) -> unsigned long long {
        return -static_cast<unsigned long long>(ll);
    };
    long long real_end;
    if (start >= 0 || end < 0) {
        // 0 <= start <= end
        long long diff = end - start;
        // 0 <= diff <= LL_MAX
        real_end = start + static_cast<long long>(diff / step);
    } else {
        // start < 0 <= end
        unsigned long long abs_start = safe_abs(start);
        unsigned long long diff = (end + abs_start);
        real_end = diff / step - abs_start;
    }

    if (!choice && start == real_end) {
        streams.err.append_format(L"%ls: range contains only one possible value\n", cmd);
        return STATUS_INVALID_ARGS;
    }

    std::uniform_int_distribution<long long> dist(start, real_end);
    long long random = dist(engine);
    long long result;
    if (start >= 0) {
        // 0 <= start <= random <= end
        long long diff = random - start;
        // 0 < step * diff <= end - start <= LL_MAX
        result = start + static_cast<long long>(diff * step);
    } else if (random < 0) {
        // start <= random < 0
        long long diff = random - start;
        result = diff * step - safe_abs(start);
    } else {
        // start < 0 <= random
        unsigned long long abs_start = safe_abs(start);
        unsigned long long diff = (random + abs_start);
        result = diff * step - abs_start;
    }

    if (choice) {
        streams.out.append_format(L"%ls\n", argv[optind + result]);
    } else {
        streams.out.append_format(L"%lld\n", result);
    }
    return STATUS_CMD_OK;
}
