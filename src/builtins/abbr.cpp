// Implementation of the read builtin.
#include "config.h"  // IWYU pragma: keep

#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "../abbrs.h"
#include "../builtin.h"
#include "../common.h"
#include "../env.h"
#include "../io.h"
#include "../re.h"
#include "../wcstringutil.h"
#include "../wgetopt.h"
#include "../wutil.h"

namespace {

static const wchar_t *const CMD = L"abbr";

struct abbr_options_t {
    bool add{};
    bool rename{};
    bool show{};
    bool list{};
    bool erase{};
    bool query{};
    bool function{};
    maybe_t<wcstring> regex_pattern;
    maybe_t<abbrs_position_t> position{};
    maybe_t<wcstring> set_cursor_indicator{};

    bool quiet{};

    wcstring_list_t args;

    bool validate(io_streams_t &streams) {
        // Duplicate options?
        wcstring_list_t cmds;
        if (add) cmds.push_back(L"add");
        if (rename) cmds.push_back(L"rename");
        if (show) cmds.push_back(L"show");
        if (list) cmds.push_back(L"list");
        if (erase) cmds.push_back(L"erase");
        if (query) cmds.push_back(L"query");
        if (cmds.size() > 1) {
            streams.err.append_format(_(L"%ls: Cannot combine options %ls\n"), CMD,
                                      join_strings(cmds, L", ").c_str());
            return false;
        }
        // If run with no options, treat it like --add if we have arguments,
        // or --show if we do not have any arguments.
        if (cmds.empty()) {
            show = args.empty();
            add = !args.empty();
        }

        if (!add && position.has_value()) {
            streams.err.append_format(_(L"%ls: --position option requires --add\n"), CMD);
            return false;
        }
        if (!add && regex_pattern.has_value()) {
            streams.err.append_format(_(L"%ls: --regex option requires --add\n"), CMD);
            return false;
        }
        if (!add && function) {
            streams.err.append_format(_(L"%ls: --function option requires --add\n"), CMD);
            return false;
        }
        if (!add && quiet) {
            streams.err.append_format(_(L"%ls: --quiet option requires --add\n"), CMD);
            return false;
        }
        if (!add && set_cursor_indicator.has_value()) {
            streams.err.append_format(_(L"%ls: --set-cursor option requires --add\n"), CMD);
            return false;
        }
        if (set_cursor_indicator.has_value() && quiet) {
            streams.err.append_format(_(L"%ls: --quiet cannot be used with --set-cursor\n"), CMD);
            return false;
        }
        if (set_cursor_indicator.has_value() && set_cursor_indicator->empty()) {
            streams.err.append_format(_(L"%ls: --set-cursor argument cannot be empty\n"), CMD);
            return false;
        }
        return true;
    }
};

// Print abbreviations in a fish-script friendly way.
static int abbr_show(const abbr_options_t &, io_streams_t &streams) {
    const auto abbrs = abbrs_get_set();
    wcstring_list_t comps{};
    for (const auto &abbr : abbrs->list()) {
        comps.clear();
        comps.push_back(L"abbr -a");
        if (abbr.from_universal) comps.push_back(L"-U");
        comps.push_back(L"--");
        // Literal abbreviations have the name and key as the same.
        // Regex abbreviations have a pattern separate from the name.
        comps.push_back(escape_string(abbr.name));
        if (abbr.is_regex()) {
            comps.push_back(L"--regex");
            comps.push_back(escape_string(abbr.key));
        }
        if (abbr.set_cursor_indicator.has_value()) {
            comps.push_back(L"--set-cursor");
            comps.push_back(escape_string(*abbr.set_cursor_indicator));
        }
        if (abbr.is_quiet) {
            comps.push_back(L"--quiet");
        }
        if (abbr.replacement_is_function) {
            comps.push_back(L"--function");
        }
        comps.push_back(escape_string(abbr.replacement));
        wcstring result = join_strings(comps, L' ');
        result.push_back(L'\n');
        streams.out.append(result);
    }
    return STATUS_CMD_OK;
}

// Print the list of abbreviation names.
static int abbr_list(const abbr_options_t &opts, io_streams_t &streams) {
    const wchar_t *const subcmd = L"--list";
    if (opts.args.size() > 0) {
        streams.err.append_format(_(L"%ls %ls: Unexpected argument -- '%ls'\n"), CMD, subcmd,
                                  opts.args.front().c_str());
        return STATUS_INVALID_ARGS;
    }
    const auto abbrs = abbrs_get_set();
    for (const auto &abbr : abbrs->list()) {
        wcstring name = escape_string(abbr.name);
        name.push_back(L'\n');
        streams.out.append(name);
    }
    return STATUS_CMD_OK;
}

// Rename an abbreviation, deleting any existing one with the given name.
static int abbr_rename(const abbr_options_t &opts, io_streams_t &streams) {
    const wchar_t *const subcmd = L"--rename";
    if (opts.args.size() != 2) {
        streams.err.append_format(_(L"%ls %ls: Requires exactly two arguments\n"), CMD, subcmd);
        return STATUS_INVALID_ARGS;
    }
    const wcstring &old_name = opts.args[0];
    const wcstring &new_name = opts.args[1];
    if (old_name.empty() || new_name.empty()) {
        streams.err.append_format(_(L"%ls %ls: Name cannot be empty\n"), CMD, subcmd);
        return STATUS_INVALID_ARGS;
    }

    if (std::any_of(new_name.begin(), new_name.end(), iswspace)) {
        streams.err.append_format(
            _(L"%ls %ls: Abbreviation '%ls' cannot have spaces in the word\n"), CMD, subcmd,
            new_name.c_str());
        return STATUS_INVALID_ARGS;
    }
    auto abbrs = abbrs_get_set();

    if (!abbrs->has_name(old_name)) {
        streams.err.append_format(_(L"%ls %ls: No abbreviation named %ls\n"), CMD, subcmd,
                                  old_name.c_str());
        return STATUS_CMD_ERROR;
    }
    if (abbrs->has_name(new_name)) {
        streams.err.append_format(
            _(L"%ls %ls: Abbreviation %ls already exists, cannot rename %ls\n"), CMD, subcmd,
            new_name.c_str(), old_name.c_str());
        return STATUS_INVALID_ARGS;
    }
    abbrs->rename(old_name, new_name);
    return STATUS_CMD_OK;
}

// Test if any args is an abbreviation.
static int abbr_query(const abbr_options_t &opts, io_streams_t &) {
    // Return success if any of our args matches an abbreviation.
    const auto abbrs = abbrs_get_set();
    for (const auto &arg : opts.args) {
        if (abbrs->has_name(arg)) {
            return STATUS_CMD_OK;
        }
    }
    return STATUS_CMD_ERROR;
}

// Add a named abbreviation.
static int abbr_add(const abbr_options_t &opts, io_streams_t &streams) {
    const wchar_t *const subcmd = L"--add";
    if (opts.args.size() < 2) {
        streams.err.append_format(_(L"%ls %ls: Requires at least two arguments\n"), CMD, subcmd);
        return STATUS_INVALID_ARGS;
    }
    const wcstring &name = opts.args[0];
    if (name.empty()) {
        streams.err.append_format(_(L"%ls %ls: Name cannot be empty\n"), CMD, subcmd);
        return STATUS_INVALID_ARGS;
    }
    if (std::any_of(name.begin(), name.end(), iswspace)) {
        streams.err.append_format(
            _(L"%ls %ls: Abbreviation '%ls' cannot have spaces in the word\n"), CMD, subcmd,
            name.c_str());
        return STATUS_INVALID_ARGS;
    }

    maybe_t<re::regex_t> regex;
    wcstring key;
    if (!opts.regex_pattern.has_value()) {
        // The name plays double-duty as the token to replace.
        key = name;
    } else {
        key = *opts.regex_pattern;
        re::re_error_t error{};
        // Compile the regex as given; if that succeeds then wrap it in our ^$ so it matches the
        // entire token.
        if (!re::regex_t::try_compile(*opts.regex_pattern, re::flags_t{}, &error)) {
            streams.err.append_format(_(L"%ls: Regular expression compile error: %ls\n"), CMD,
                                      error.message().c_str());
            streams.err.append_format(L"%ls: %ls\n", CMD, opts.regex_pattern->c_str());
            streams.err.append_format(L"%ls: %*ls\n", CMD, static_cast<int>(error.offset), L"^");
            return STATUS_INVALID_ARGS;
        }
        wcstring anchored = re::make_anchored(*opts.regex_pattern);
        regex = re::regex_t::try_compile(anchored, re::flags_t{}, &error);
        assert(regex.has_value() && "Anchored compilation should have succeeded");
    }

    wcstring replacement;
    for (auto iter = opts.args.begin() + 1; iter != opts.args.end(); ++iter) {
        if (!replacement.empty()) replacement.push_back(L' ');
        replacement.append(*iter);
    }
    // Abbreviation function names disallow spaces.
    // This is to prevent accidental usage of e.g. `--function 'string replace'`
    if (opts.function &&
        (!valid_func_name(replacement) || replacement.find(L' ') != wcstring::npos)) {
        streams.err.append_format(_(L"%ls: Invalid function name: %ls\n"), CMD,
                                  replacement.c_str());
        return STATUS_INVALID_ARGS;
    }

    abbrs_position_t position = opts.position ? *opts.position : abbrs_position_t::command;

    // Note historically we have allowed overwriting existing abbreviations.
    abbreviation_t abbr{std::move(name), std::move(key), std::move(replacement), position};
    abbr.regex = std::move(regex);
    abbr.replacement_is_function = opts.function;
    abbr.set_cursor_indicator = opts.set_cursor_indicator;
    abbr.is_quiet = opts.quiet;
    abbrs_get_set()->add(std::move(abbr));
    return STATUS_CMD_OK;
}

// Erase the named abbreviations.
static int abbr_erase(const abbr_options_t &opts, io_streams_t &) {
    if (opts.args.empty()) {
        // This has historically been a silent failure.
        return STATUS_CMD_ERROR;
    }

    // Erase each. If any is not found, return ENV_NOT_FOUND which is historical.
    int result = STATUS_CMD_OK;
    auto abbrs = abbrs_get_set();
    for (const auto &arg : opts.args) {
        if (!abbrs->erase(arg)) {
            result = ENV_NOT_FOUND;
        }
    }
    return result;
}

}  // namespace

maybe_t<int> builtin_abbr(parser_t &parser, io_streams_t &streams, const wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    abbr_options_t opts;
    // Note 1 is returned by wgetopt to indicate a non-option argument.
    enum { NON_OPTION_ARGUMENT = 1, REGEX_SHORT, EXPAND_ON_SHORT, QUIET_SHORT };

    // Note the leading '-' causes wgetopter to return arguments in order, instead of permuting
    // them. We need this behavior for compatibility with pre-builtin abbreviations where options
    // could be given literally, for example `abbr e emacs -nw`.
    static const wchar_t *const short_options = L"-afrseqgUh";
    static const struct woption long_options[] = {{L"add", no_argument, 'a'},
                                                  {L"position", required_argument, 'p'},
                                                  {L"regex", required_argument, REGEX_SHORT},
                                                  {L"quiet", no_argument, QUIET_SHORT},
                                                  {L"set-cursor", required_argument, 'C'},
                                                  {L"function", no_argument, 'f'},
                                                  {L"rename", no_argument, 'r'},
                                                  {L"erase", no_argument, 'e'},
                                                  {L"query", no_argument, 'q'},
                                                  {L"show", no_argument, 's'},
                                                  {L"list", no_argument, 'l'},
                                                  {L"global", no_argument, 'g'},
                                                  {L"universal", no_argument, 'U'},
                                                  {L"help", no_argument, 'h'},
                                                  {}};

    int argc = builtin_count_args(argv);
    int opt;
    wgetopter_t w;
    bool unrecognized_options_are_args = false;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case NON_OPTION_ARGUMENT:
                // If --add is specified (or implied by specifying no other commands), all
                // unrecognized options after the *second* non-option argument are considered part
                // of the abbreviation expansion itself, rather than options to the abbr command.
                // For example, `abbr e emacs -nw` works, because `-nw` occurs after the second
                // non-option, and --add is implied.
                opts.args.push_back(w.woptarg);
                if (opts.args.size() >= 2 &&
                    !(opts.rename || opts.show || opts.list || opts.erase || opts.query)) {
                    unrecognized_options_are_args = true;
                }
                break;
            case 'a':
                opts.add = true;
                break;
            case 'p': {
                if (opts.position.has_value()) {
                    streams.err.append_format(_(L"%ls: Cannot specify multiple positions\n"), CMD);
                    return STATUS_INVALID_ARGS;
                }
                if (!wcscmp(w.woptarg, L"command")) {
                    opts.position = abbrs_position_t::command;
                } else if (!wcscmp(w.woptarg, L"anywhere")) {
                    opts.position = abbrs_position_t::anywhere;
                } else {
                    streams.err.append_format(_(L"%ls: Invalid position '%ls'\nPosition must be "
                                                L"one of: command, anywhere.\n"),
                                              CMD, w.woptarg);
                    return STATUS_INVALID_ARGS;
                }
                break;
            }
            case REGEX_SHORT: {
                if (opts.regex_pattern.has_value()) {
                    streams.err.append_format(_(L"%ls: Cannot specify multiple regex patterns\n"),
                                              CMD);
                    return STATUS_INVALID_ARGS;
                }
                opts.regex_pattern = w.woptarg;
                break;
            }
            case QUIET_SHORT: {
                opts.quiet = true;
                break;
            }
            case 'C': {
                if (opts.set_cursor_indicator.has_value()) {
                    streams.err.append_format(
                        _(L"%ls: Cannot specify multiple set-cursor options\n"), CMD);
                    return STATUS_INVALID_ARGS;
                }
                opts.set_cursor_indicator = w.woptarg;
                break;
            }
            case 'f':
                opts.function = true;
                break;
            case 'r':
                opts.rename = true;
                break;
            case 'e':
                opts.erase = true;
                break;
            case 'q':
                opts.query = true;
                break;
            case 's':
                opts.show = true;
                break;
            case 'l':
                opts.list = true;
                break;
            case 'g':
            case 'U':
                // Kept for backwards compatibility but ignored.
                break;
            case 'h': {
                builtin_print_help(parser, streams, cmd);
                return STATUS_CMD_OK;
            }
            case '?': {
                if (unrecognized_options_are_args) {
                    opts.args.push_back(argv[w.woptind - 1]);
                } else {
                    builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
                    return STATUS_INVALID_ARGS;
                }
            }
        }
    }
    opts.args.insert(opts.args.end(), argv + w.woptind, argv + argc);
    if (!opts.validate(streams)) {
        return STATUS_INVALID_ARGS;
    }

    if (opts.add) return abbr_add(opts, streams);
    if (opts.show) return abbr_show(opts, streams);
    if (opts.list) return abbr_list(opts, streams);
    if (opts.rename) return abbr_rename(opts, streams);
    if (opts.erase) return abbr_erase(opts, streams);
    if (opts.query) return abbr_query(opts, streams);

    // validate() should error or ensure at least one path is set.
    DIE("unreachable");
    return STATUS_INVALID_ARGS;
}