// Implementation of the bind builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "builtin.h"
#include "builtin_bind.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "input.h"     // IWYU pragma: keep
#include "io.h"        // IWYU pragma: keep
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

enum { BIND_INSERT, BIND_ERASE, BIND_KEY_NAMES, BIND_FUNCTION_NAMES };
struct bind_cmd_opts_t {
    bool all = false;
    bool bind_mode_given = false;
    bool list_modes = false;
    bool print_help = false;
    bool silent = false;
    bool use_terminfo = false;
    int mode = BIND_INSERT;
    const wchar_t *bind_mode = DEFAULT_BIND_MODE;
    const wchar_t *sets_bind_mode = L"";
};

// Here follows the definition of all builtin commands. The function names are all of the form
// builtin_NAME where NAME is the name of the builtin. so the function name for the builtin 'fg' is
// 'builtin_fg'.
//
// A few builtins, including 'while', 'command' and 'builtin' are not defined here as they are
// handled directly by the parser. (They are not parsed as commands, instead they only alter the
// parser state)
//
// The builtins 'break' and 'continue' are so closely related that they share the same
// implementation, namely 'builtin_break_continue.
//
// Several other builtins, including jobs, ulimit and set are so big that they have been given their
// own module. These files are all named 'builtin_NAME.cpp', where NAME is the name of the builtin.

/// List a single key binding.
/// Returns false if no binding with that sequence and mode exists.
bool builtin_bind_t::list_one(const wcstring &seq, const wcstring &bind_mode,
                                  io_streams_t &streams) {
    std::vector<wcstring> ecmds;
    wcstring sets_mode;

    if (!input_mapping_get(seq, bind_mode, &ecmds, &sets_mode)) {
        return false;
    }

    streams.out.append(L"bind");

    // Append the mode flags if applicable.
    if (bind_mode != DEFAULT_BIND_MODE) {
        const wcstring emode = escape_string(bind_mode, ESCAPE_ALL);
        streams.out.append(L" -M ");
        streams.out.append(emode);
    }
    if (!sets_mode.empty() && sets_mode != bind_mode) {
        const wcstring esets_mode = escape_string(sets_mode, ESCAPE_ALL);
        streams.out.append(L" -m ");
        streams.out.append(esets_mode);
    }

    // Append the name.
    wcstring tname;
    if (input_terminfo_get_name(seq, &tname)) {
        // Note that we show -k here because we have an input key name.
        streams.out.append_format(L" -k %ls", tname.c_str());
    } else {
        // No key name, so no -k; we show the escape sequence directly.
        const wcstring eseq = escape_string(seq, ESCAPE_ALL);
        streams.out.append_format(L" %ls", eseq.c_str());
    }

    // Now show the list of commands.
    for (size_t i = 0; i < ecmds.size(); i++) {
        const wcstring &ecmd = ecmds.at(i);
        const wcstring escaped_ecmd = escape_string(ecmd, ESCAPE_ALL);
        streams.out.push_back(' ');
        streams.out.append(escaped_ecmd);
    }
    streams.out.push_back(L'\n');

    return true;
}

/// List all current key bindings.
void builtin_bind_t::list(const wchar_t *bind_mode, io_streams_t &streams) {
    const std::vector<input_mapping_name_t> lst = input_mapping_get_names();

    for (const input_mapping_name_t &binding : lst) {
        if (bind_mode && bind_mode != binding.mode) {
            continue;
        }

        list_one(binding.seq, binding.mode, streams);
    }
}

/// Print terminfo key binding names to string buffer used for standard output.
///
/// \param all if set, all terminfo key binding names will be printed. If not set, only ones that
/// are defined for this terminal are printed.
void builtin_bind_t::key_names(int all, io_streams_t &streams) {
    const wcstring_list_t names = input_terminfo_get_names(!all);
    for (size_t i = 0; i < names.size(); i++) {
        const wcstring &name = names.at(i);

        streams.out.append_format(L"%ls\n", name.c_str());
    }
}

/// Print all the special key binding functions to string buffer used for standard output.
void builtin_bind_t::function_names(io_streams_t &streams) {
    wcstring_list_t names = input_function_get_names();

    for (size_t i = 0; i < names.size(); i++) {
        const wchar_t *seq = names.at(i).c_str();
        streams.out.append_format(L"%ls\n", seq);
    }
}

/// Wraps input_terminfo_get_sequence(), appending the correct error messages as needed.
bool builtin_bind_t::get_terminfo_sequence(const wchar_t *seq, wcstring *out_seq, io_streams_t &streams) {
    if (input_terminfo_get_sequence(seq, out_seq)) {
        return true;
    }

    wcstring eseq = escape_string(seq, 0);
    if (!opts->silent) {
        if (errno == ENOENT) {
            streams.err.append_format(_(L"%ls: No key with name '%ls' found\n"), L"bind", eseq.c_str());
        } else if (errno == EILSEQ) {
            streams.err.append_format(_(L"%ls: Key with name '%ls' does not have any mapping\n"),
                                      L"bind", eseq.c_str());
        } else {
            streams.err.append_format(_(L"%ls: Unknown error trying to bind to key named '%ls'\n"),
                                      L"bind", eseq.c_str());
        }
    }
    return false;
}

/// Add specified key binding.
bool builtin_bind_t::add(const wchar_t *seq, const wchar_t *const *cmds, size_t cmds_len,
                             const wchar_t *mode, const wchar_t *sets_mode, int terminfo,
                             io_streams_t &streams) {
    if (terminfo) {
        wcstring seq2;
        if (get_terminfo_sequence(seq, &seq2, streams)) {
            input_mapping_add(seq2.c_str(), cmds, cmds_len, mode, sets_mode);
        } else {
            return true;
        }

    } else {
        input_mapping_add(seq, cmds, cmds_len, mode, sets_mode);
    }

    return false;
}

/// Erase specified key bindings
///
/// @param  seq
///    an array of all key bindings to erase
/// @param  all
///    if specified, _all_ key bindings will be erased
/// @param  mode
///    if specified, only bindings from that mode will be erased. If not given
///    and @c all is @c false, @c DEFAULT_BIND_MODE will be used.
/// @param  use_terminfo
///    Whether to look use terminfo -k name
///
bool builtin_bind_t::erase(wchar_t **seq, int all, const wchar_t *mode, int use_terminfo,
                               io_streams_t &streams) {
    if (all) {
        const std::vector<input_mapping_name_t> lst = input_mapping_get_names();
        for (std::vector<input_mapping_name_t>::const_iterator it = lst.begin(), end = lst.end();
             it != end; ++it) {
            if (mode == NULL || mode == it->mode) {
                input_mapping_erase(it->seq, it->mode);
            }
        }

        return false;
    }

    bool res = false;
    if (mode == NULL) mode = DEFAULT_BIND_MODE;  //!OCLINT(parameter reassignment)

    while (*seq) {
        if (use_terminfo) {
            wcstring seq2;
            if (get_terminfo_sequence(*seq++, &seq2, streams)) {
                input_mapping_erase(seq2, mode);
            } else {
                res = true;
            }
        } else {
            input_mapping_erase(*seq++, mode);
        }
    }

    return res;
}

bool builtin_bind_t::insert(int optind, int argc, wchar_t **argv,
                                io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    int arg_count = argc - optind;

    if (arg_count == 0) {
        list(opts->bind_mode_given ? opts->bind_mode : NULL, streams);
    } else if (arg_count == 1) {
        wcstring seq;
        if (opts->use_terminfo) {
            if (!get_terminfo_sequence(argv[optind], &seq, streams)) {
                // get_terminfo_sequence already printed the error.
                return true;
            }
        } else {
            seq = argv[optind];
        }

        if (!list_one(seq, opts->bind_mode, streams)) {
            wcstring eseq = escape_string(argv[optind], 0);
            if (!opts->silent) {
                if (opts->use_terminfo) {
                    streams.err.append_format(_(L"%ls: No binding found for key '%ls'\n"), cmd,
                                              eseq.c_str());
                } else {
                    streams.err.append_format(_(L"%ls: No binding found for sequence '%ls'\n"), cmd,
                                              eseq.c_str());
                }
            }
            return true;
        }
    } else {
        if (add(argv[optind], argv + (optind + 1), argc - (optind + 1), opts->bind_mode,
                             opts->sets_bind_mode, opts->use_terminfo, streams)) {
            return true;
        }
    }

    return false;
}

/// List all current bind modes.
void builtin_bind_t::list_modes(io_streams_t &streams) {
    const std::vector<input_mapping_name_t> lst = input_mapping_get_names();
    // A set accomplishes two things for us here:
    // - It removes duplicates (no twenty "default" entries).
    // - It sorts it, which makes it nicer on the user.
    std::set<wcstring> modes;

    for (const input_mapping_name_t &binding : lst) {
        modes.insert(binding.mode);
    }
    for (const auto &mode : modes) {
        streams.out.append_format(L"%ls\n", mode.c_str());
    }
}

int parse_cmd_opts(bind_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    static const wchar_t *short_options = L":aehkKfM:Lm:s";
    static const struct woption long_options[] = {{L"all", no_argument, NULL, 'a'},
                                                  {L"erase", no_argument, NULL, 'e'},
                                                  {L"function-names", no_argument, NULL, 'f'},
                                                  {L"help", no_argument, NULL, 'h'},
                                                  {L"key", no_argument, NULL, 'k'},
                                                  {L"key-names", no_argument, NULL, 'K'},
                                                  {L"list-modes", no_argument, NULL, 'L'},
                                                  {L"mode", required_argument, NULL, 'M'},
                                                  {L"sets-mode", required_argument, NULL, 'm'},
                                                  {L"silent", no_argument, NULL, 's'},
                                                  {NULL, 0, NULL, 0}};

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case L'a': {
                opts.all = true;
                break;
            }
            case L'e': {
                opts.mode = BIND_ERASE;
                break;
            }
            case L'f': {
                opts.mode = BIND_FUNCTION_NAMES;
                break;
            }
            case L'h': {
                opts.print_help = true;
                break;
            }
            case L'k': {
                opts.use_terminfo = true;
                break;
            }
            case L'K': {
                opts.mode = BIND_KEY_NAMES;
                break;
            }
            case L'L': {
                opts.list_modes = true;
                return STATUS_CMD_OK;
            }
            case L'M': {
                if (!valid_var_name(w.woptarg)) {
                    streams.err.append_format(BUILTIN_ERR_BIND_MODE, cmd, w.woptarg);
                    return STATUS_INVALID_ARGS;
                }
                opts.bind_mode = w.woptarg;
                opts.bind_mode_given = true;
                break;
            }
            case L'm': {
                if (!valid_var_name(w.woptarg)) {
                    streams.err.append_format(BUILTIN_ERR_BIND_MODE, cmd, w.woptarg);
                    return STATUS_INVALID_ARGS;
                }
                opts.sets_bind_mode = w.woptarg;
                break;
            }
            case L's': {
                opts.silent = true;
                break;
            }
            case ':': {
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            case L'?': {
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

/// The bind builtin, used for setting character sequences.
int builtin_bind_t::builtin_bind(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    bind_cmd_opts_t opts;
    this->opts = &opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.list_modes) {
        list_modes(streams);
        return STATUS_CMD_OK;
    }
    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    switch (opts.mode) {
        case BIND_ERASE: {
            const wchar_t *bind_mode = opts.bind_mode_given ? opts.bind_mode : NULL;
            if (erase(&argv[optind], opts.all, bind_mode, opts.use_terminfo,
                                   streams)) {
                return STATUS_CMD_ERROR;
            }
            break;
        }
        case BIND_INSERT: {
            if (insert(optind, argc, argv, streams)) {
                return STATUS_CMD_ERROR;
            }
            break;
        }
        case BIND_KEY_NAMES: {
            key_names(opts.all, streams);
            break;
        }
        case BIND_FUNCTION_NAMES: {
            function_names(streams);
            break;
        }
        default: {
            streams.err.append_format(_(L"%ls: Invalid state\n"), cmd);
            return STATUS_CMD_ERROR;
        }
    }

    return STATUS_CMD_OK;
}
