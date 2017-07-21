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
#include "exec.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "wgetopt.h"  // IWYU pragma: keep
#include "wutil.h"    // IWYU pragma: keep

class parser_t;
const wcstring var_name_prefix = L"_flag_";

#define BUILTIN_ERR_INVALID_OPT_SPEC _(L"%ls: Invalid option spec '%ls' at char '%lc'\n")

class option_spec_t {
   public:
    wchar_t short_flag;
    wcstring long_flag;
    wcstring validation_command;
    wcstring_list_t vals;
    bool short_flag_valid;
    int num_allowed;
    int num_seen;

    option_spec_t(wchar_t s)
        : short_flag(s),
          long_flag(),
          validation_command(),
          vals(),
          short_flag_valid(true),
          num_allowed(0),
          num_seen(0) {}
};

class argparse_cmd_opts_t {
   public:
    bool print_help = false;
    bool stop_nonopt = false;
    size_t min_args = 0;
    size_t max_args = SIZE_MAX;
    wchar_t implicit_int_flag = L'\0';
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
                                 const wcstring &option_spec, const wchar_t **opt_spec_str,
                                 io_streams_t &streams) {
    const wchar_t *s = *opt_spec_str;
    if (opt_spec->short_flag == opts.implicit_int_flag && *s && *s != L'!') {
        streams.err.append_format(
            _(L"%ls: Implicit int short flag '%lc' does not allow modifiers like '%lc'\n"),
            opts.name.c_str(), opt_spec->short_flag, *s);
        return false;
    }

    if (*s == L'=') {
        s++;
        if (*s == L'?') {
            opt_spec->num_allowed = -1;  // optional arg
            s++;
        } else if (*s == L'+') {
            opt_spec->num_allowed = 2;  // mandatory arg and can appear more than once
            s++;
        } else {
            opt_spec->num_allowed = 1;  // mandatory arg and can appear only once
        }
    }

    if (*s == L'!') {
        s++;
        opt_spec->validation_command = wcstring(s);
    } else if (*s) {
        streams.err.append_format(BUILTIN_ERR_INVALID_OPT_SPEC, opts.name.c_str(),
                                  option_spec.c_str(), *s);
        return false;
    }

    // Make sure we have some validation for implicit int flags.
    if (opt_spec->short_flag == opts.implicit_int_flag && opt_spec->validation_command.empty()) {
        opt_spec->validation_command = L"_validate_int";
    }

    if (opts.options.find(opt_spec->short_flag) != opts.options.end()) {
        streams.err.append_format(L"%ls: Short flag '%lc' already defined\n", opts.name.c_str(),
                                  opt_spec->short_flag);
        return false;
    }

    opts.options.emplace(opt_spec->short_flag, opt_spec);
    *opt_spec_str = s;
    return true;
}

/// Parse the text following the short flag letter.
static bool parse_option_spec_sep(argparse_cmd_opts_t &opts, option_spec_t *opt_spec,
                                  wcstring &option_spec, const wchar_t **opt_spec_str,
                                  io_streams_t &streams) {
    const wchar_t *s = *opt_spec_str;
    if (*(s - 1) == L'#') {
        if (*s != L'-') {
            streams.err.append_format(
                _(L"%ls: Short flag '#' must be followed by '-' and a long name\n"),
                opts.name.c_str());
            return false;
        }
        if (opts.implicit_int_flag) {
            streams.err.append_format(_(L"%ls: Implicit int flag '%lc' already defined\n"),
                                      opts.name.c_str(), opts.implicit_int_flag);
            return false;
        }
        opts.implicit_int_flag = opt_spec->short_flag;
        opt_spec->short_flag_valid = false;
        s++;
    } else if (*s == L'-') {
        opt_spec->short_flag_valid = false;
        s++;
        if (!*s) {
            streams.err.append_format(BUILTIN_ERR_INVALID_OPT_SPEC, opts.name.c_str(),
                                      option_spec.c_str(), *(s - 1));
            return false;
        }
    } else if (*s == L'/') {
        s++;  // the struct is initialized assuming short_flag_valid should be true
        if (!*s) {
            streams.err.append_format(BUILTIN_ERR_INVALID_OPT_SPEC, opts.name.c_str(),
                                      option_spec.c_str(), *(s - 1));
            return false;
        }
    } else if (*s == L'#') {
        if (opts.implicit_int_flag) {
            streams.err.append_format(_(L"%ls: Implicit int flag '%lc' already defined\n"),
                                      opts.name.c_str(), opts.implicit_int_flag);
            return false;
        }
        opts.implicit_int_flag = opt_spec->short_flag;
        opt_spec->num_allowed = 1;  // mandatory arg and can appear only once
        s++;  // the struct is initialized assuming short_flag_valid should be true
        if (!*s) opts.options.emplace(opt_spec->short_flag, opt_spec);
    } else {
        // Long flag name not allowed if second char isn't '/', '-' or '#' so just check for
        // behavior modifier chars.
        if (!parse_flag_modifiers(opts, opt_spec, option_spec, &s, streams)) return false;
    }

    *opt_spec_str = s;
    return true;
}

/// This parses an option spec string into a struct option_spec.
static bool parse_option_spec(argparse_cmd_opts_t &opts,  //!OCLINT(high npath complexity)
                              wcstring option_spec, io_streams_t &streams) {
    if (option_spec.empty()) {
        streams.err.append_format(_(L"%ls: An option spec must have a short flag letter\n"),
                                  opts.name.c_str());
        return false;
    }

    const wchar_t *s = option_spec.c_str();
    if (!iswalnum(*s) && *s != L'#') {
        streams.err.append_format(_(L"%ls: Short flag '%lc' invalid, must be alphanum or '#'\n"),
                                  opts.name.c_str(), *s);
        return false;
    }

    option_spec_t *opt_spec = new option_spec_t(*s++);
    if (!*s) {
        // Bool short flag only.
        opts.options.emplace(opt_spec->short_flag, opt_spec);
        return true;
    }
    if (!parse_option_spec_sep(opts, opt_spec, option_spec, &s, streams)) return false;
    if (!*s) return true;  // parsed the entire string so the option spec doesn't have a long flag

    // Collect the long flag name.
    const wchar_t *e = s;
    while (*e && (*e == L'-' || *e == L'_' || iswalnum(*e))) e++;
    if (e != s) {
        opt_spec->long_flag = wcstring(s, e - s);
        if (opts.long_to_short_flag.find(opt_spec->long_flag) != opts.long_to_short_flag.end()) {
            streams.err.append_format(L"%ls: Long flag '%ls' already defined\n", opts.name.c_str(),
                                      opt_spec->long_flag.c_str());
            return false;
        }
        opts.long_to_short_flag.emplace(opt_spec->long_flag, opt_spec->short_flag);
    }

    return parse_flag_modifiers(opts, opt_spec, option_spec, &e, streams);
}

static int collect_option_specs(argparse_cmd_opts_t &opts, int *optind, int argc, wchar_t **argv,
                                io_streams_t &streams) {
    wchar_t *cmd = argv[0];

    while (true) {
        if (wcscmp(L"--", argv[*optind]) == 0) {
            ++*optind;
            break;
        }

        if (!parse_option_spec(opts, argv[*optind], streams)) {
            return STATUS_CMD_ERROR;
        }

        if (++*optind == argc) {
            streams.err.append_format(_(L"%ls: Missing -- separator\n"), cmd);
            return STATUS_INVALID_ARGS;
        }
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

    if (opts.print_help) return STATUS_CMD_OK;

    if (argc == w.woptind || wcscmp(L"--", argv[w.woptind - 1]) == 0) {
        // The user didn't specify any option specs.
        streams.err.append_format(_(L"%ls: No option specs were provided\n"), cmd);
        return STATUS_INVALID_ARGS;
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

static int validate_arg(argparse_cmd_opts_t &opts, option_spec_t *opt_spec, bool is_long_flag,
                        const wchar_t *woptarg, io_streams_t &streams) {
    // Obviously if there is no arg validation command we assume the arg is okay.
    if (opt_spec->validation_command.empty()) return STATUS_CMD_OK;

    wcstring_list_t cmd_output;

    env_push(true);
    env_set(L"_argparse_cmd", opts.name.c_str(), ENV_LOCAL);
    if (is_long_flag) {
        env_set(var_name_prefix + L"name", opt_spec->long_flag.c_str(), ENV_LOCAL);
    } else {
        env_set(var_name_prefix + L"name", wcstring(1, opt_spec->short_flag).c_str(), ENV_LOCAL);
    }
    env_set(var_name_prefix + L"value", woptarg, ENV_LOCAL);

    int retval = exec_subshell(opt_spec->validation_command, cmd_output, false);
    for (auto it : cmd_output) {
        streams.err.append(it);
        streams.err.push_back(L'\n');
    }
    env_pop();
    return retval;
}

// Check if it is an implicit integer option. Try to parse and validate it as a valid integer. The
// code that parses option specs has already ensured that implicit integers have either an explicit
// or implicit validation function.
static int check_for_implicit_int(argparse_cmd_opts_t &opts, const wchar_t *val, const wchar_t *cmd,
                                  const wchar_t *const *argv, wgetopter_t &w, int opt, int long_idx,
                                  parser_t &parser, io_streams_t &streams) {
    if (opts.implicit_int_flag == L'\0') {
        // There is no implicit integer option so report an error.
        builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
        return STATUS_INVALID_ARGS;
    }

    if (opt == '?') {
        // It's a flag that might be an implicit integer flag. Try to parse it as an integer to
        // decide if it is in fact something like `-NNN` or an invalid flag.
        (void)fish_wcstol(val);
        if (errno) {
            builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
            return STATUS_INVALID_ARGS;
        }
    }

    // Okay, it looks like an integer. See if it passes the validation checks.
    auto found = opts.options.find(opts.implicit_int_flag);
    assert(found != opts.options.end());
    auto opt_spec = found->second;
    int retval = validate_arg(opts, opt_spec, long_idx != -1, val, streams);
    if (retval != STATUS_CMD_OK) return retval;

    // It's a valid integer so store it and return success.
    opt_spec->vals.clear();
    opt_spec->vals.push_back(wcstring(val));
    opt_spec->num_seen++;
    w.nextchar = NULL;
    return STATUS_CMD_OK;
}

static int handle_flag(argparse_cmd_opts_t &opts, option_spec_t *opt_spec, int long_idx,
                       const wchar_t *woptarg, io_streams_t &streams) {
    opt_spec->num_seen++;
    if (opt_spec->num_allowed == 0) {
        // It's a boolean flag. Save the flag we saw since it might be useful to know if the
        // short or long flag was given.
        assert(!woptarg);
        if (long_idx == -1) {
            opt_spec->vals.push_back(wcstring(1, L'-') + opt_spec->short_flag);
        } else {
            opt_spec->vals.push_back(L"--" + opt_spec->long_flag);
        }
        return STATUS_CMD_OK;
    }

    if (woptarg) {
        int retval = validate_arg(opts, opt_spec, long_idx != -1, woptarg, streams);
        if (retval != STATUS_CMD_OK) return retval;
    }

    if (opt_spec->num_allowed == -1 || opt_spec->num_allowed == 1) {
        // We're depending on `wgetopt_long()` to report that a mandatory value is missing if
        // `opt_spec->num_allowed == 1` and thus return ':' so that we don't take this branch if
        // the mandatory arg is missing.
        opt_spec->vals.clear();
        if (woptarg) {
            opt_spec->vals.push_back(woptarg);
        }
    } else {
        assert(woptarg);
        opt_spec->vals.push_back(woptarg);
    }

    return STATUS_CMD_OK;
}

static int argparse_parse_flags(argparse_cmd_opts_t &opts, const wchar_t *short_options,
                                const woption *long_options, const wchar_t *cmd, int argc,
                                wchar_t **argv, int *optind, parser_t &parser,
                                io_streams_t &streams) {
    int opt;
    int long_idx = -1;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, &long_idx)) != -1) {
        if (opt == ':') {
            builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1]);
            return STATUS_INVALID_ARGS;
        }

        if (opt == '?') {
            // It's not a recognized flag. See if it's an implicit int flag.
            int retval = check_for_implicit_int(opts, argv[w.woptind - 1] + 1, cmd, argv, w, opt,
                                                long_idx, parser, streams);
            if (retval != STATUS_CMD_OK) return retval;
            long_idx = -1;
            continue;
        }

        // It's a recognized flag.
        auto found = opts.options.find(opt);
        assert(found != opts.options.end());

        option_spec_t *opt_spec = found->second;
        int retval = handle_flag(opts, opt_spec, long_idx, w.woptarg, streams);
        if (retval != STATUS_CMD_OK) return retval;
        long_idx = -1;
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

    // We need to convert our wcstring_list_t to a <wchar_t **> that can be used by wgetopt_long().
    // This ensures the memory for the data structure is freed when we leave this scope.
    null_terminated_array_t<wchar_t> argv_container(args);
    auto argv = (wchar_t **)argv_container.get();

    int optind;
    int retval = argparse_parse_flags(opts, short_options.c_str(), long_options.get(), cmd, argc,
                                      argv, &optind, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    retval = check_for_mutually_exclusive_flags(opts, streams);
    if (retval != STATUS_CMD_OK) return retval;

    for (int i = optind; argv[i]; i++) opts.argv.push_back(argv[i]);

    return STATUS_CMD_OK;
}

static int check_min_max_args_constraints(argparse_cmd_opts_t &opts, parser_t &parser,
                                          io_streams_t &streams) {
    UNUSED(parser);
    const wchar_t *cmd = opts.name.c_str();

    if (opts.argv.size() < opts.min_args) {
        streams.err.append_format(BUILTIN_ERR_MIN_ARG_COUNT1, cmd, opts.min_args, opts.argv.size());
        return STATUS_CMD_ERROR;
    }
    if (opts.max_args != SIZE_MAX && opts.argv.size() > opts.max_args) {
        streams.err.append_format(BUILTIN_ERR_MAX_ARG_COUNT1, cmd, opts.max_args, opts.argv.size());
        return STATUS_CMD_ERROR;
    }

    return STATUS_CMD_OK;
}

/// Put the result of parsing the supplied args into the caller environment as local vars.
static void set_argparse_result_vars(argparse_cmd_opts_t &opts) {
    for (auto it : opts.options) {
        option_spec_t *opt_spec = it.second;
        if (!opt_spec->num_seen) continue;

        auto val = list_to_array_val(opt_spec->vals);
        if (opt_spec->short_flag_valid) {
            env_set(var_name_prefix + opt_spec->short_flag, *val == ENV_NULL ? NULL : val->c_str(),
                    ENV_LOCAL);
        }
        if (!opt_spec->long_flag.empty()) {
            // We do a simple replacement of all non alphanum chars rather than calling
            // escape_string(long_flag, 0, STRING_STYLE_VAR).
            wcstring long_flag = opt_spec->long_flag;
            for (size_t pos = 0; pos < long_flag.size(); pos++) {
                if (!iswalnum(long_flag[pos])) long_flag[pos] = L'_';
            }
            env_set(var_name_prefix + long_flag, *val == ENV_NULL ? NULL : val->c_str(), ENV_LOCAL);
        }
    }

    auto val = list_to_array_val(opts.argv);
    env_set(L"argv", *val == ENV_NULL ? NULL : val->c_str(), ENV_LOCAL);
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

    wcstring_list_t args;
    args.push_back(opts.name);
    while (optind < argc) args.push_back(argv[optind++]);

    retval = parse_exclusive_args(opts, streams);
    if (retval != STATUS_CMD_OK) return retval;

    retval = argparse_parse_args(opts, args, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    retval = check_min_max_args_constraints(opts, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    set_argparse_result_vars(opts);
    return retval;
}
