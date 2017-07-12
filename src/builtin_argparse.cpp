// Implementation of the argparse builtin.
//
// See issue #4190 for the rationale behind the original behavior of this builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

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
    bool stop_nonopt = false;
    size_t min_args = 0;
    size_t max_args = SIZE_T_MAX;
    wcstring name = L"argparse";
    wcstring_list_t raw_exclusive_flags;
    wcstring_list_t argv;
    std::map<wchar_t, option_spec_t *> options;
    std::map<wcstring, wchar_t> long_to_short_flag;
    std::vector<std::vector<wchar_t>> exclusive_flag_sets;

    ~argparse_cmd_opts_t() {
        for (auto it : options) {
            delete it.second;
        }
    }
};

static const wchar_t *short_options = L"+:hn:sx:N:X:";
static const struct woption long_options[] = {{L"stop-nonopt", no_argument, NULL, 's'},
                                              {L"name", required_argument, NULL, 'n'},
                                              {L"exclusive", required_argument, NULL, 'x'},
                                              {L"help", no_argument, NULL, 'h'},
                                              {L"min-args", required_argument, NULL, 'N'},
                                              {L"max-args", required_argument, NULL, 'X'},
                                              {NULL, 0, NULL, 0}};

// Check if any pair of mutually exclusive options was seen. Note that since every option must have
// a short name we only need to check those.
static int check_for_mutually_exclusive_flags(argparse_cmd_opts_t &opts, io_streams_t &streams) {
    for (auto it : opts.options) {
        auto opt_spec = it.second;
        if (opt_spec->num_seen == 0) continue;

        // We saw this option at least once. Check all the sets of mutually exclusive options to see
        // if this option appears in any of them.
        for (auto xarg_set : opts.exclusive_flag_sets) {
            auto found = std::find(xarg_set.begin(), xarg_set.end(), opt_spec->short_flag);
            if (found != xarg_set.end()) {
                // Okay, this option is in a mutually exclusive set of options. Check if any of the
                // other mutually exclusive options have been seen.
                for (auto xflag : xarg_set) {
                    auto xopt_spec = opts.options[xflag];
                    // Ignore this flag in the list of mutually exclusive flags.
                    if (xopt_spec->short_flag == opt_spec->short_flag) continue;

                    // If it is a different flag check if it has been seen.
                    if (xopt_spec->num_seen) {
                        wcstring flag1;
                        if (opt_spec->short_flag_valid) flag1 = wcstring(1, opt_spec->short_flag);
                        if (!opt_spec->long_flag.empty()) {
                            if (opt_spec->short_flag_valid) flag1 += L"/";
                            flag1 += opt_spec->long_flag;
                        }
                        wcstring flag2;
                        if (xopt_spec->short_flag_valid) flag2 = wcstring(1, xopt_spec->short_flag);
                        if (!xopt_spec->long_flag.empty()) {
                            if (xopt_spec->short_flag_valid) flag2 += L"/";
                            flag2 += xopt_spec->long_flag;
                        }
                        streams.err.append_format(
                            _(L"%ls: Mutually exclusive flags '%ls' and `%ls` seen\n"),
                            opts.name.c_str(), flag1.c_str(), flag2.c_str());
                        return STATUS_CMD_ERROR;
                    }
                }
            }
        }
    }

    return STATUS_CMD_OK;
}

// This is used as a specialization to allow us to force operator>> to split the input on something
// other than spaces.
class WordDelimitedByComma : public wcstring {};

// Cppcheck incorrectly complains this is unused. It is indirectly used by std:istream_iterator.
std::wistream &operator>>(std::wistream &is, WordDelimitedByComma &output) {  // cppcheck-suppress
    std::getline(is, output, L',');
    return is;
}

// This should be called after all the option specs have been parsed. At that point we have enough
// information to parse the values associated with any `--exclusive` flags.
static int parse_exclusive_args(argparse_cmd_opts_t &opts, io_streams_t &streams) {
    for (auto raw_xflags : opts.raw_exclusive_flags) {
        // This is an advanced technique that leverages the C++ STL to split the string on commas.
        std::wistringstream iss(raw_xflags);
        wcstring_list_t xflags((std::istream_iterator<WordDelimitedByComma, wchar_t>(iss)),
                std::istream_iterator<WordDelimitedByComma, wchar_t>());
        if (xflags.size() < 2) {
            streams.err.append_format(_(L"%ls: exclusive flag string '%ls' is not valid\n"),
                    opts.name.c_str(), raw_xflags.c_str());
            return STATUS_CMD_ERROR;
        }

        std::vector<wchar_t> exclusive_set;
        for (auto flag : xflags) {
            if (flag.size() == 1 && opts.options.find(flag[0]) != opts.options.end()) {
                // It's a short flag.
                exclusive_set.push_back(flag[0]);
            } else {
                auto x = opts.long_to_short_flag.find(flag);
                if (x != opts.long_to_short_flag.end()) {
                    // It's a long flag we store as its short flag equivalent.
                    exclusive_set.push_back(x->second);
                } else {
                    streams.err.append_format(_(L"%ls: exclusive flag '%ls' is not valid\n"),
                            opts.name.c_str(), flag.c_str());
                    return STATUS_CMD_ERROR;
                }
            }
        }

        // Store the set of exclusive flags for use when parsing the supplied set of arguments.
        opts.exclusive_flag_sets.push_back(exclusive_set);
    }

    return STATUS_CMD_OK;
}

static bool parse_flag_modifiers(argparse_cmd_opts_t &opts, option_spec_t *opt_spec,
                                 const wcstring &option_spec, const wchar_t *s,
                                 io_streams_t &streams) {
    if (*s == '=') {
        s++;
        if (*s == '?') {
            opt_spec->num_allowed = -1;  // optional arg
            s++;
        } else if (*s == '+') {
            opt_spec->num_allowed = 2;  // mandatory arg and can appear more than once
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
    while (*e && *e != '+' && *e != '=') e++;
    if (e == s) {
        streams.err.append_format(BUILTIN_ERR_INVALID_OPT_SPEC, opts.name.c_str(),
                                  option_spec.c_str(), *(s - 1));
        return false;
    }

    opt_spec->long_flag = wcstring(s, e - s);
    opts.long_to_short_flag.emplace(opt_spec->long_flag, opt_spec->short_flag);
    return parse_flag_modifiers(opts, opt_spec, option_spec, e, streams);
}

static int collect_option_specs(argparse_cmd_opts_t &opts, int *optind, int argc, wchar_t **argv,
                                io_streams_t &streams) {
    wchar_t *cmd = argv[0];

    while (*optind < argc) {
        if (wcscmp(L"--", argv[*optind]) == 0) {
            ++*optind;
            break;
        }

        if (!parse_option_spec(opts, argv[*optind], streams)) {
            return STATUS_CMD_ERROR;
        }

        ++*optind;
    }

    if (opts.options.empty()) {
        streams.err.append_format(_(L"%ls: No option specs were provided\n"), cmd);
        return STATUS_INVALID_ARGS;
    }

    return STATUS_CMD_OK;
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
            case 's': {
                opts.stop_nonopt = true;
                break;
            }
            case 'x': {
                // Just save the raw string here. Later, when we have all the short and long flag
                // definitions we'll parse these strings into a more useful data structure.
                opts.raw_exclusive_flags.push_back(w.woptarg);
                break;
            }
            case 'h': {
                opts.print_help = true;
                break;
            }
            case 'N': {
                long x = fish_wcstol(w.woptarg);
                if (errno || x < 0) {
                    streams.err.append_format(_(L"%ls: Invalid --min-args value '%ls'\n"), cmd,
                                              w.woptarg);
                    return STATUS_INVALID_ARGS;
                }
                opts.min_args = x;
                break;
            }
            case 'X': {
                long x = fish_wcstol(w.woptarg);
                if (errno || x < 0) {
                    streams.err.append_format(_(L"%ls: Invalid --max-args value '%ls'\n"), cmd,
                                              w.woptarg);
                    return STATUS_INVALID_ARGS;
                }
                opts.max_args = x;
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
    return collect_option_specs(opts, optind, argc, argv, streams);
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

// Add a count for how many times we saw each boolean flag but only if we saw the flag at least
// once.
static void update_bool_flag_counts(argparse_cmd_opts_t &opts) {
    for (auto it : opts.options) {
        auto opt_spec = it.second;
        if (opt_spec->num_allowed != 0 || opt_spec->num_seen == 0) continue;
        wchar_t count[20];
        swprintf(count, sizeof count / sizeof count[0], L"%d", opt_spec->num_seen);
        opt_spec->vals.push_back(wcstring(count));
    }
}

static int argparse_parse_flags(argparse_cmd_opts_t &opts, const wchar_t *short_options,
                                const woption *long_options, const wchar_t *cmd, int argc,
                                wchar_t **argv, int *optind, parser_t &parser,
                                io_streams_t &streams) {
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        if (opt == ':') {
            builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1]);
            return STATUS_INVALID_ARGS;
        }
        if (opt == '?') {
            builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
            return STATUS_INVALID_ARGS;
        }

        auto found = opts.options.find(opt);
        assert(found != opts.options.end());

        option_spec_t *opt_spec = found->second;
        opt_spec->num_seen++;
        if (opt_spec->num_allowed == 0) {
            assert(!w.woptarg);
        } else if (opt_spec->num_allowed == -1 || opt_spec->num_allowed == 1) {
            // We're depending on `wgetopt_long()` to report that a mandatory value is missing if
            // `opt_spec->num_allowed == 1` and thus return ':' so that we don't take this branch if
            // the mandatory arg is missing.
            opt_spec->vals.clear();
            if (w.woptarg) {
                opt_spec->vals.push_back(w.woptarg);
            }
        } else {
            assert(w.woptarg);
            opt_spec->vals.push_back(w.woptarg);
        }
    }

    *optind = w.woptind;
    return STATUS_CMD_OK;
}

// This function mimics the `wgetopt_long()` usage found elsewhere in our other builtin commands.
// It's different in that the short and long option structures are constructed dynamically based on
// arguments provided to the `argparse` command.
static int argparse_parse_args(argparse_cmd_opts_t &opts, const wcstring_list_t &args,
                               parser_t &parser, io_streams_t &streams) {
    if (args.empty()) return STATUS_CMD_OK;

    wcstring short_options = opts.stop_nonopt ? L"+:" : L":";
    size_t nflags = opts.options.size();
    // This assumes every option has a long flag. Which is the worst case and isn't worth optimizing
    // since the number of options is always quite small.
    auto long_options = std::unique_ptr<woption, std::function<void(woption *)>>(
        new woption[nflags + 1], [](woption *p) { delete[] p; });
    populate_option_strings(opts, short_options, long_options);

    const wchar_t *cmd = opts.name.c_str();
    int argc = static_cast<int>(args.size());

    // This is awful but we need to convert our wcstring_list_t to a <wchar_t **> that can be passed
    // to w.wgetopt_long(). Furthermore, because we're dynamically allocating the array of pointers
    // we need to ensure the memory for the data structure is freed when we leave this scope.
    null_terminated_array_t<wchar_t> argv_container(args);
    auto argv = (wchar_t **)argv_container.get();

    int optind;
    int retval = argparse_parse_flags(opts, short_options.c_str(), long_options.get(), cmd, argc,
                                      argv, &optind, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    retval = check_for_mutually_exclusive_flags(opts, streams);
    if (retval != STATUS_CMD_OK) return retval;
    update_bool_flag_counts(opts);

    for (int i = optind; argv[i]; i++) opts.argv.push_back(argv[i]);
    if (opts.min_args > 0 && opts.argv.size() < opts.min_args) {
        streams.err.append_format(BUILTIN_ERR_MIN_ARG_COUNT1, cmd, opts.min_args, opts.argv.size());
        return STATUS_CMD_ERROR;
    }
    if (opts.max_args != SIZE_T_MAX && opts.argv.size() > opts.max_args) {
        streams.err.append_format(BUILTIN_ERR_MAX_ARG_COUNT1, cmd, opts.max_args, opts.argv.size());
        return STATUS_CMD_ERROR;
    }
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
        // Apparently we weren't handed any arguments to be parsed according to the option specs we
        // just collected. So there isn't anything for us to do.
        return STATUS_CMD_OK;
    }

    wcstring_list_t args;
    args.push_back(opts.name);
    while (optind < argc) args.push_back(argv[optind++]);

    retval = parse_exclusive_args(opts, streams);
    if (retval != STATUS_CMD_OK) return retval;

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

    return retval;
}
