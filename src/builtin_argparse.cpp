// Implementation of the argparse builtin.
//
// See issue #4190 for the rationale behind the original behavior of this builtin.
#include "config.h"  // IWYU pragma: keep

#if 0
#include <malloc/malloc.h>
#endif

#include <stddef.h>
#include <wchar.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "builtin.h"
#include "builtin_argparse.h"
#include "common.h"
#include "env.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "wgetopt.h"  // IWYU pragma: keep
#include "wutil.h"    // IWYU pragma: keep

class parser_t;

#define BUILTIN_ERR_INVALID_OPT_SPEC _(L"%ls: Invalid option spec '%ls' at char '%lc'\n")

class option_spec_t {
   public:
    wchar_t short_flag;
    wcstring long_flag;
    wcstring_list_t vals;
    bool short_flag_valid;
    int num_allowed;
    int num_seen;

    option_spec_t(wchar_t s)
        : short_flag(s), long_flag(), vals(), short_flag_valid(true), num_allowed(0), num_seen(0) {}
};

class argparse_cmd_opts_t {
   public:
    bool print_help = false;
    bool require_order = false;
    wcstring name = L"argparse";
    wcstring_list_t argv;
    struct std::map<wchar_t, option_spec_t *> options;

    ~argparse_cmd_opts_t() {
        for (auto it : options) {
            delete it.second;
        }
    }
};

static const wchar_t *short_options = L"+:hn:r";
static const struct woption long_options[] = {{L"require-order", no_argument, NULL, 'r'},
                                              {L"name", required_argument, NULL, 'n'},
                                              {L"help", no_argument, NULL, 'h'},
                                              {NULL, 0, NULL, 0}};

static bool parse_flag_modifiers(argparse_cmd_opts_t &opts, option_spec_t *opt_spec,
                                 const wcstring &option_spec, const wchar_t *s,
                                 io_streams_t &streams) {
    if (*s == '+') {
        opt_spec->num_allowed = 2;  // mandatory arg and can appear more than once
        s++;
    } else if (*s == ':') {
        s++;
        if (*s == ':') {
            opt_spec->num_allowed = -1;  // optional arg
            s++;
        } else {
            opt_spec->num_allowed = 1;  // mandatory arg and can appear only once
        }
    }

    if (*s) {
        streams.err.append_format(BUILTIN_ERR_INVALID_OPT_SPEC, opts.name.c_str(),
                                  option_spec.c_str(), *s);
        return false;
    }

    opts.options.emplace(opt_spec->short_flag, opt_spec);
    return true;
}

// This parses an option spec string into a struct option_spec.
static bool parse_option_spec(argparse_cmd_opts_t &opts, wcstring option_spec,
                              io_streams_t &streams) {
    if (option_spec.empty()) {
        streams.err.append_format(_(L"%s: An option spec must have a short flag letter\n"),
                                  opts.name.c_str());
        return false;
    }

    const wchar_t *s = option_spec.c_str();
    option_spec_t *opt_spec = new option_spec_t(*s++);

    if (*s == '/') {
        s++;  // the struct is initialized assuming short_flag_valid should be true
    } else if (*s == '-') {
        opt_spec->short_flag_valid = false;
        s++;
    } else {
        // Long flag name not allowed if second char isn't '/' or '-' so just check for
        // behavior modifier chars.
        return parse_flag_modifiers(opts, opt_spec, option_spec, s, streams);
    }

    const wchar_t *e = s;
    while (*e && *e != '+' && *e != ':') e++;
    if (e == s) {
        streams.err.append_format(BUILTIN_ERR_INVALID_OPT_SPEC, opts.name.c_str(),
                                  option_spec.c_str(), *(s - 1));
        return false;
    }

    opt_spec->long_flag = wcstring(s, e - s);
    return parse_flag_modifiers(opts, opt_spec, option_spec, e, streams);
}

static int parse_cmd_opts(argparse_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 'n': {
                opts.name = w.woptarg;
                break;
            }
            case 'r': {
                opts.require_order = true;
                break;
            }
            case 'h': {
                opts.print_help = true;
                break;
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

    *optind = w.woptind;
    return STATUS_CMD_OK;
}

static void populate_option_strings(
    argparse_cmd_opts_t &opts, wcstring &short_options,
    std::unique_ptr<woption, std::function<void(woption *)>> &long_options) {
    int i = 0;
    for (auto it : opts.options) {
        option_spec_t *opt_spec = it.second;
        if (opt_spec->short_flag_valid) short_options.push_back(opt_spec->short_flag);

        int arg_type = no_argument;
        if (opt_spec->num_allowed == -1) {
            arg_type = optional_argument;
            if (opt_spec->short_flag_valid) short_options.append(L"::");
        } else if (opt_spec->num_allowed > 0) {
            arg_type = required_argument;
            if (opt_spec->short_flag_valid) short_options.append(L":");
        }

        if (!opt_spec->long_flag.empty()) {
            long_options.get()[i++] = {opt_spec->long_flag.c_str(), arg_type, NULL,
                                       opt_spec->short_flag};
        }
    }
    long_options.get()[i] = {NULL, 0, NULL, 0};
}

// This function mimics the `wgetopt_long()` usage found elsewhere in our other builtin commands.
// It's different in that the short and long option structures are constructed dynamically based on
// arguments provided to the `argparse` command.
static int argparse_parse_args(argparse_cmd_opts_t &opts, const wcstring_list_t &args,
                                  parser_t &parser, io_streams_t &streams) {
    if (args.empty()) return STATUS_CMD_OK;

    wcstring short_options = opts.require_order ? L"+:" : L":";
    int nflags = opts.options.size();
    // This assumes every option has a long flag. Which is the worst case and isn't worth optimizing
    // since the number of options is always quite small. Thus the size of the array will never be
    // much larger than the minimum size required for all the long options.
    auto long_options = std::unique_ptr<woption, std::function<void(woption *)>>(
        new woption[nflags + 1], [](woption *p) { delete[] p; });
    populate_option_strings(opts, short_options, long_options);

    const wchar_t *cmd = opts.name.c_str();
    int argc = args.size();

    // This is awful but we need to convert our wcstring_list_t to a <wchar_t **> that can be passed
    // to w.wgetopt_long(). Furthermore, because we're dynamically allocating the array of pointers
    // we need to ensure the memory for the data structure is freed when we leave this scope.
    null_terminated_array_t<wchar_t> argv_container(args);
    auto argv = (wchar_t **)argv_container.get();

    int opt;
    wgetopter_t w;
    auto long_opts = long_options.get();
    while ((opt = w.wgetopt_long(argc, argv, short_options.c_str(), long_opts, NULL)) != -1) {
        switch (opt) {
            case ':': {
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            case '?': {
                builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            default: {
                auto found = opts.options.find(opt);
                assert(found != opts.options.end());

                option_spec_t *opt_spec = found->second;
                opt_spec->num_seen++;
                if (opt_spec->num_allowed == 0) {
                    assert(!w.woptarg);
                } else if (opt_spec->num_allowed == -1 || opt_spec->num_allowed == 1) {
                    // This is subtle. We're depending on `wgetopt_long()` to report that a
                    // mandatory value is missing if `opt_spec->num_allowed == 1` and thus return
                    // ':' so that we don't take this branch if the mandatory arg is missing.
                    // Otherwise we can treat the optional and mandatory arg cases the same. That
                    // is, store the arg as the only value for the flag. Even if we've seen earlier
                    // instances of the flag.
                    opt_spec->vals.clear();
                    if (w.woptarg) {
                        opt_spec->vals.push_back(w.woptarg);
                    }
                } else {
                    assert(w.woptarg);
                    opt_spec->vals.push_back(w.woptarg);
                }
                break;
            }
        }
    }

    // Add a count for how many times we saw each boolean flag but only if we saw the flag at least
    // once.
    for (auto it : opts.options) {
        auto opt_spec = it.second;
        if (opt_spec->num_allowed != 0 || opt_spec->num_seen == 0) continue;
        wchar_t count[20];
        swprintf(count, sizeof count / sizeof count[0], L"%d", opt_spec->num_seen);
        opt_spec->vals.push_back(wcstring(count));
    }

    for (int i = w.woptind; argv[i]; i++) opts.argv.push_back(argv[i]);
    return STATUS_CMD_OK;
}

/// The argparse builtin. This is explicitly not compatible with the BSD or GNU version of this
/// command. That's because fish doesn't have the weird quoting problems of POSIX shells. So we
/// don't need to support flags like `--unquoted`. Similarly we don't want to support introducing
/// long options with a single dash so we don't support the `--alternative` flag. That `getopt` is
/// an external command also means its output has to be in a form that can be eval'd. Because our
/// version is a builtin it can directly set variables local to the current scope (e.g., a
/// function). It doesn't need to write anything to stdout that then needs to be eval'd.
int builtin_argparse(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    argparse_cmd_opts_t opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    if (optind == argc) {
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT1, cmd, 2, 0);
        return STATUS_INVALID_ARGS;
    }

    while (true) {
        if (wcscmp(L"--", argv[optind]) == 0) {
            optind++;
            break;
        }

        if (!parse_option_spec(opts, argv[optind], streams)) {
            return STATUS_CMD_ERROR;
        }

        if (++optind == argc) {
            streams.err.append_format(BUILTIN_ERR_ARG_COUNT1, cmd, 2, 0);
            return STATUS_INVALID_ARGS;
        }
    }

#if 0
    auto stats = mstats();
    fwprintf(stderr, L"WTF %d  bytes_total %lu  chunks_used %lu  bytes_used %lu  chunks_free %lu  bytes_free %lu\n", __LINE__, stats.bytes_total, stats.chunks_used, stats.bytes_used, stats.chunks_free);
#endif

    wcstring_list_t args;
    args.push_back(opts.name);
    while (optind < argc) args.push_back(argv[optind++]);

    retval = argparse_parse_args(opts, args, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    for (auto it : opts.options) {
        option_spec_t *opt_spec = it.second;
        if (!opt_spec->num_seen) continue;

        wcstring var_name_prefix = L"_flag_";
        auto val = list_to_array_val(opt_spec->vals);
        env_set(var_name_prefix + opt_spec->short_flag, *val == ENV_NULL ? NULL : val->c_str(),
                ENV_LOCAL);
        if (!opt_spec->long_flag.empty()) {
            // We do a simple replacement of all non alphanum chars rather than calling
            // escape_string(long_flag, 0, STRING_STYLE_VAR).
            wcstring long_flag = opt_spec->long_flag;
            for (size_t pos = 0; pos < long_flag.size(); pos++) {
                if (!iswalnum(long_flag[pos])) long_flag[pos] = L'_';
            }
            env_set(var_name_prefix + long_flag, *val == ENV_NULL ? NULL : val->c_str(),
                    ENV_LOCAL);
        }
    }

    auto val = list_to_array_val(opts.argv);
    env_set(L"argv", *val == ENV_NULL ? NULL : val->c_str(), ENV_LOCAL);

#if 0
    stats = mstats();
    fwprintf(stderr, L"WTF %d  bytes_total %lu  chunks_used %lu  bytes_used %lu  chunks_free %lu  bytes_free %lu\n", __LINE__, stats.bytes_total, stats.chunks_used, stats.bytes_used, stats.chunks_free);
#endif

    return retval;
}
