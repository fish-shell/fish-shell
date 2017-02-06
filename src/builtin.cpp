// Functions for executing builtin functions.
//
// How to add a new builtin function:
//
// 1). Create a function in builtin.c with the following signature:
//
//     <tt>static int builtin_NAME( parser_t &parser, wchar_t ** args )</tt>
//
// where NAME is the name of the builtin, and args is a zero-terminated list of arguments.
//
// 2). Add a line like { L"NAME", &builtin_NAME, N_(L"Bla bla bla") }, to the builtin_data_t
// variable. The description is used by the completion system. Note that this array is sorted.
//
// 3). Create a file doc_src/NAME.txt, containing the manual for the builtin in Doxygen-format.
// Check the other builtin manuals for proper syntax.
//
// 4). Use 'git add doc_src/NAME.txt' to start tracking changes to the documentation file.
#include "config.h"  // IWYU pragma: keep

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wchar.h>
#include <algorithm>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <utility>

#include "builtin.h"
#include "builtin_commandline.h"
#include "builtin_complete.h"
#include "builtin_jobs.h"
#include "builtin_printf.h"
#include "builtin_set.h"
#include "builtin_set_color.h"
#include "builtin_string.h"
#include "builtin_test.h"
#include "builtin_ulimit.h"
#include "common.h"
#include "complete.h"
#include "env.h"
#include "event.h"
#include "exec.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "function.h"
#include "highlight.h"
#include "history.h"
#include "input.h"
#include "intern.h"
#include "io.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "parser.h"
#include "parser_keywords.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
#include "signal.h"
#include "tokenizer.h"
#include "wcstringutil.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

bool builtin_data_t::operator<(const wcstring &other) const {
    return wcscmp(this->name, other.c_str()) < 0;
}

bool builtin_data_t::operator<(const builtin_data_t *other) const {
    return wcscmp(this->name, other->name) < 0;
}

static void builtin_append_format(wcstring &str, const wchar_t *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    append_formatv(str, fmt, ap);
    va_end(ap);
}

bool builtin_is_valid_varname(const wchar_t *varname, wcstring &errstr, const wchar_t *cmd) {
    const wchar_t *invalid_char = wcsvarname(varname);
    if (!invalid_char) {
        return true;
    }

    if (*invalid_char == L'\0') {
        builtin_append_format(errstr, BUILTIN_ERR_VARNAME_ZERO, cmd);
    } else {
        builtin_append_format(errstr, BUILTIN_ERR_VARCHAR, cmd, *invalid_char);
    }
    return false;
}

/// Counts the number of arguments in the specified null-terminated array
int builtin_count_args(const wchar_t *const *argv) {
    int argc;
    for (argc = 1; argv[argc] != NULL;) {
        argc++;
    }

    assert(argv[argc] == NULL);
    return argc;
}

/// This function works like wperror, but it prints its result into the streams.err string instead
/// to stderr. Used by the builtin commands.
void builtin_wperror(const wchar_t *s, io_streams_t &streams) {
    char *err = strerror(errno);
    if (s != NULL) {
        streams.err.append(s);
        streams.err.append(L": ");
    }
    if (err != NULL) {
        const wcstring werr = str2wcstring(err);
        streams.err.append(werr);
        streams.err.push_back(L'\n');
    }
}

/// Count the number of times the specified character occurs in the specified string.
static int count_char(const wchar_t *str, wchar_t c) {
    int res = 0;
    for (; *str; str++) {
        res += (*str == c);
    }
    return res;
}

/// Obtain help/usage information for the specified builtin from manpage in subshell
///
/// @param  name
///    builtin name to get up help for
///
/// @return
///    A wcstring with a formatted manpage.
///
wcstring builtin_help_get(parser_t &parser, io_streams_t &streams, const wchar_t *name) {
    UNUSED(parser);
    // This won't ever work if no_exec is set.
    if (no_exec) return wcstring();

    wcstring_list_t lst;
    wcstring out;
    const wcstring name_esc = escape_string(name, 1);
    wcstring cmd = format_string(L"__fish_print_help %ls", name_esc.c_str());
    if (!streams.out_is_redirected && isatty(STDOUT_FILENO)) {
        // since we're using a subshell, __fish_print_help can't tell we're in
        // a terminal. Tell it ourselves.
        int cols = common_get_width();
        cmd = format_string(L"__fish_print_help --tty-width %d %ls", cols, name_esc.c_str());
    }
    if (exec_subshell(cmd, lst, false /* don't apply exit status */) >= 0) {
        for (size_t i = 0; i < lst.size(); i++) {
            out.append(lst.at(i));
            out.push_back(L'\n');
        }
    }
    return out;
}

/// Process and print for the specified builtin. If @c b is `sb_err`, also print the line
/// information.
///
/// If @c b is the buffer representing standard error, and the help message is about to be printed
/// to an interactive screen, it may be shortened to fit the screen.
///
void builtin_print_help(parser_t &parser, io_streams_t &streams, const wchar_t *cmd,
                        output_stream_t &b) {
    bool is_stderr = &b == &streams.err;
    if (is_stderr) {
        b.append(parser.current_line());
    }

    const wcstring h = builtin_help_get(parser, streams, cmd);

    if (!h.size()) return;

    wchar_t *str = wcsdup(h.c_str());
    if (str) {
        bool is_short = false;
        if (is_stderr) {
            // Interactive mode help to screen - only print synopsis if the rest won't fit.
            int screen_height, lines;

            screen_height = common_get_height();
            lines = count_char(str, L'\n');
            if (!shell_is_interactive() || (lines > 2 * screen_height / 3)) {
                wchar_t *pos;
                int cut = 0;
                int i;

                is_short = true;

                // First move down 4 lines.
                pos = str;
                for (i = 0; (i < 4) && pos && *pos; i++) {
                    pos = wcschr(pos + 1, L'\n');
                }

                if (pos && *pos) {
                    // Then find the next empty line.
                    for (; *pos; pos++) {
                        if (*pos != L'\n') {
                            continue;
                        }

                        int is_empty = 1;
                        wchar_t *pos2;
                        for (pos2 = pos + 1; *pos2; pos2++) {
                            if (*pos2 == L'\n') break;

                            if (*pos2 != L'\t' && *pos2 != L' ') {
                                is_empty = 0;
                                break;
                            }
                        }
                        if (is_empty) {
                            // And cut it.
                            *(pos2 + 1) = L'\0';
                            cut = 1;
                            break;
                        }
                    }
                }

                // We did not find a good place to cut message to shorten it - so we make sure we
                // don't print anything.
                if (!cut) {
                    *str = 0;
                }
            }
        }

        b.append(str);
        if (is_short) {
            b.append_format(_(L"%ls: Type 'help %ls' for related documentation\n\n"), cmd, cmd);
        }

        free(str);
    }
}

/// Perform error reporting for encounter with unknown option.
void builtin_unknown_option(parser_t &parser, io_streams_t &streams, const wchar_t *cmd,
                            const wchar_t *opt) {
    streams.err.append_format(BUILTIN_ERR_UNKNOWN, cmd, opt);
    builtin_print_help(parser, streams, cmd, streams.err);
}

/// Perform error reporting for encounter with missing argument.
void builtin_missing_argument(parser_t &parser, io_streams_t &streams, const wchar_t *cmd,
                              const wchar_t *opt) {
    streams.err.append_format(BUILTIN_ERR_MISSING, cmd, opt);
    builtin_print_help(parser, streams, cmd, streams.err);
}

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
static bool builtin_bind_list_one(const wcstring &seq, const wcstring &bind_mode,
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
static void builtin_bind_list(const wchar_t *bind_mode, io_streams_t &streams) {
    const std::vector<input_mapping_name_t> lst = input_mapping_get_names();

    for (const input_mapping_name_t &binding : lst) {
        if (bind_mode != NULL && bind_mode != binding.mode) {
            continue;
        }

        builtin_bind_list_one(binding.seq, binding.mode, streams);
    }
}

/// Print terminfo key binding names to string buffer used for standard output.
///
/// \param all if set, all terminfo key binding names will be printed. If not set, only ones that
/// are defined for this terminal are printed.
static void builtin_bind_key_names(int all, io_streams_t &streams) {
    const wcstring_list_t names = input_terminfo_get_names(!all);
    for (size_t i = 0; i < names.size(); i++) {
        const wcstring &name = names.at(i);

        streams.out.append_format(L"%ls\n", name.c_str());
    }
}

/// Print all the special key binding functions to string buffer used for standard output.
static void builtin_bind_function_names(io_streams_t &streams) {
    wcstring_list_t names = input_function_get_names();

    for (size_t i = 0; i < names.size(); i++) {
        const wchar_t *seq = names.at(i).c_str();
        streams.out.append_format(L"%ls\n", seq);
    }
}

/// Wraps input_terminfo_get_sequence(), appending the correct error messages as needed.
static int get_terminfo_sequence(const wchar_t *seq, wcstring *out_seq, io_streams_t &streams) {
    if (input_terminfo_get_sequence(seq, out_seq)) {
        return 1;
    }

    wcstring eseq = escape_string(seq, 0);
    if (errno == ENOENT) {
        streams.err.append_format(_(L"%ls: No key with name '%ls' found\n"), L"bind", eseq.c_str());
    } else if (errno == EILSEQ) {
        streams.err.append_format(_(L"%ls: Key with name '%ls' does not have any mapping\n"),
                                  L"bind", eseq.c_str());
    } else {
        streams.err.append_format(_(L"%ls: Unknown error trying to bind to key named '%ls'\n"),
                                  L"bind", eseq.c_str());
    }
    return 0;
}

/// Add specified key binding.
static int builtin_bind_add(const wchar_t *seq, const wchar_t *const *cmds, size_t cmds_len,
                            const wchar_t *mode, const wchar_t *sets_mode, int terminfo,
                            io_streams_t &streams) {
    if (terminfo) {
        wcstring seq2;
        if (get_terminfo_sequence(seq, &seq2, streams)) {
            input_mapping_add(seq2.c_str(), cmds, cmds_len, mode, sets_mode);
        } else {
            return 1;
        }

    } else {
        input_mapping_add(seq, cmds, cmds_len, mode, sets_mode);
    }

    return 0;
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
static int builtin_bind_erase(wchar_t **seq, int all, const wchar_t *mode, int use_terminfo,
                              io_streams_t &streams) {
    if (all) {
        const std::vector<input_mapping_name_t> lst = input_mapping_get_names();
        for (std::vector<input_mapping_name_t>::const_iterator it = lst.begin(), end = lst.end();
             it != end; ++it) {
            if (mode == NULL || mode == it->mode) {
                input_mapping_erase(it->seq, it->mode);
            }
        }

        return 0;
    }

    int res = 0;
    if (mode == NULL) mode = DEFAULT_BIND_MODE;  //!OCLINT(parameter reassignment)

    while (*seq) {
        if (use_terminfo) {
            wcstring seq2;
            if (get_terminfo_sequence(*seq++, &seq2, streams)) {
                input_mapping_erase(seq2, mode);
            } else {
                res = 1;
            }
        } else {
            input_mapping_erase(*seq++, mode);
        }
    }

    return res;
}

/// The bind builtin, used for setting character sequences.
static int builtin_bind(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wgetopter_t w;
    enum { BIND_INSERT, BIND_ERASE, BIND_KEY_NAMES, BIND_FUNCTION_NAMES };
    int argc = builtin_count_args(argv);
    int mode = BIND_INSERT;
    int res = STATUS_BUILTIN_OK;
    int all = 0;
    const wchar_t *bind_mode = DEFAULT_BIND_MODE;
    bool bind_mode_given = false;
    const wchar_t *sets_bind_mode = L"";
    int use_terminfo = 0;

    w.woptind = 0;

    static const struct woption long_options[] = {{L"all", no_argument, 0, 'a'},
                                                  {L"erase", no_argument, 0, 'e'},
                                                  {L"function-names", no_argument, 0, 'f'},
                                                  {L"help", no_argument, 0, 'h'},
                                                  {L"key", no_argument, 0, 'k'},
                                                  {L"key-names", no_argument, 0, 'K'},
                                                  {L"mode", required_argument, 0, 'M'},
                                                  {L"sets-mode", required_argument, 0, 'm'},
                                                  {0, 0, 0, 0}};

    while (1) {
        int opt_index = 0;
        int opt = w.wgetopt_long_only(argc, argv, L"aehkKfM:m:", long_options, &opt_index);
        if (opt == -1) break;

        switch (opt) {
            case 0: {
                if (long_options[opt_index].flag != 0) break;
                streams.err.append_format(BUILTIN_ERR_UNKNOWN, argv[0],
                                          long_options[opt_index].name);
                builtin_print_help(parser, streams, argv[0], streams.err);
                return STATUS_BUILTIN_ERROR;
            }
            case 'a': {
                all = 1;
                break;
            }
            case 'e': {
                mode = BIND_ERASE;
                break;
            }
            case 'h': {
                builtin_print_help(parser, streams, argv[0], streams.out);
                return STATUS_BUILTIN_OK;
            }
            case 'k': {
                use_terminfo = 1;
                break;
            }
            case 'K': {
                mode = BIND_KEY_NAMES;
                break;
            }
            case 'f': {
                mode = BIND_FUNCTION_NAMES;
                break;
            }
            case 'M': {
                bind_mode = w.woptarg;
                bind_mode_given = true;
                break;
            }
            case 'm': {
                sets_bind_mode = w.woptarg;
                break;
            }
            case '?': {
                builtin_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return STATUS_BUILTIN_ERROR;
            }
            default: {
                DIE("unexpected opt");
                break;
            }
        }
    }

    switch (mode) {
        case BIND_ERASE: {
            if (builtin_bind_erase(&argv[w.woptind], all, bind_mode_given ? bind_mode : NULL,
                                   use_terminfo, streams)) {
                res = STATUS_BUILTIN_ERROR;
            }
            break;
        }
        case BIND_INSERT: {
            int arg_count = argc - w.woptind;
            if (arg_count == 0) {
                builtin_bind_list(bind_mode_given ? bind_mode : NULL, streams);
            } else if (arg_count == 1) {
                wcstring seq;
                if (use_terminfo) {
                    if (!get_terminfo_sequence(argv[w.woptind], &seq, streams)) {
                        res = STATUS_BUILTIN_ERROR;
                        // get_terminfo_sequence already printed the error.
                        break;
                    }
                } else {
                    seq = argv[w.woptind];
                }
                if (!builtin_bind_list_one(seq, bind_mode, streams)) {
                    res = STATUS_BUILTIN_ERROR;
                    wcstring eseq = escape_string(argv[w.woptind], 0);
                    if (use_terminfo) {
                        streams.err.append_format(_(L"%ls: No binding found for key '%ls'\n"),
                                                  argv[0], eseq.c_str());
                    } else {
                        streams.err.append_format(_(L"%ls: No binding found for sequence '%ls'\n"),
                                                  argv[0], eseq.c_str());
                    }
                }
                break;
            } else {
                if (builtin_bind_add(argv[w.woptind], argv + (w.woptind + 1),
                                     argc - (w.woptind + 1), bind_mode, sets_bind_mode,
                                     use_terminfo, streams)) {
                    res = STATUS_BUILTIN_ERROR;
                }
            }
            break;
        }
        case BIND_KEY_NAMES: {
            builtin_bind_key_names(all, streams);
            break;
        }
        case BIND_FUNCTION_NAMES: {
            builtin_bind_function_names(streams);
            break;
        }
        default: {
            res = STATUS_BUILTIN_ERROR;
            streams.err.append_format(_(L"%ls: Invalid state\n"), argv[0]);
            break;
        }
    }

    return res;
}

/// The block builtin, used for temporarily blocking events.
static int builtin_block(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wgetopter_t w;
    enum {
        UNSET,
        GLOBAL,
        LOCAL,
    };

    int scope = UNSET;
    int erase = 0;
    int argc = builtin_count_args(argv);

    w.woptind = 0;

    static const struct woption long_options[] = {{L"erase", no_argument, 0, 'e'},
                                                  {L"local", no_argument, 0, 'l'},
                                                  {L"global", no_argument, 0, 'g'},
                                                  {L"help", no_argument, 0, 'h'},
                                                  {0, 0, 0, 0}};

    while (1) {
        int opt_index = 0;
        int opt = w.wgetopt_long(argc, argv, L"elgh", long_options, &opt_index);
        if (opt == -1) break;

        switch (opt) {
            case 0: {
                if (long_options[opt_index].flag != 0) break;
                streams.err.append_format(BUILTIN_ERR_UNKNOWN, argv[0],
                                          long_options[opt_index].name);
                builtin_print_help(parser, streams, argv[0], streams.err);
                return STATUS_BUILTIN_ERROR;
            }
            case 'h': {
                builtin_print_help(parser, streams, argv[0], streams.out);
                return STATUS_BUILTIN_OK;
            }
            case 'g': {
                scope = GLOBAL;
                break;
            }
            case 'l': {
                scope = LOCAL;
                break;
            }
            case 'e': {
                erase = 1;
                break;
            }
            case '?': {
                builtin_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return STATUS_BUILTIN_ERROR;
            }
            default: {
                DIE("unexpected opt");
                break;
            }
        }
    }

    if (erase) {
        if (scope != UNSET) {
            streams.err.append_format(_(L"%ls: Can not specify scope when removing block\n"),
                                      argv[0]);
            return STATUS_BUILTIN_ERROR;
        }

        if (parser.global_event_blocks.empty()) {
            streams.err.append_format(_(L"%ls: No blocks defined\n"), argv[0]);
            return STATUS_BUILTIN_ERROR;
        }
        parser.global_event_blocks.pop_front();
    } else {
        size_t block_idx = 0;
        block_t *block = parser.block_at_index(block_idx);

        event_blockage_t eb = {};
        eb.typemask = (1 << EVENT_ANY);

        switch (scope) {
            case LOCAL: {
                // If this is the outermost block, then we're global
                if (block_idx + 1 >= parser.block_count()) {
                    block = NULL;
                }
                break;
            }
            case GLOBAL: {
                block = NULL;
                break;
            }
            case UNSET: {
                while (block != NULL && block->type() != FUNCTION_CALL &&
                       block->type() != FUNCTION_CALL_NO_SHADOW) {
                    // Set it in function scope
                    block = parser.block_at_index(++block_idx);
                }
                break;
            }
            default: {
                DIE("unexpected scope");
                break;
            }
        }
        if (block) {
            block->event_blocks.push_front(eb);
        } else {
            parser.global_event_blocks.push_front(eb);
        }
    }

    return STATUS_BUILTIN_OK;
}

/// The builtin builtin, used for giving builtins precedence over functions. Mostly handled by the
/// parser. All this code does is some additional operational modes, such as printing a list of all
/// builtins, printing help, etc.
static int builtin_builtin(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    int argc = builtin_count_args(argv);
    int list = 0;
    wgetopter_t w;

    static const struct woption long_options[] = {
        {L"names", no_argument, 0, 'n'}, {L"help", no_argument, 0, 'h'}, {0, 0, 0, 0}};

    while (1) {
        int opt_index = 0;

        int opt = w.wgetopt_long(argc, argv, L"nh", long_options, &opt_index);
        if (opt == -1) break;

        switch (opt) {
            case 0: {
                if (long_options[opt_index].flag != 0) break;
                streams.err.append_format(BUILTIN_ERR_UNKNOWN, argv[0],
                                          long_options[opt_index].name);
                builtin_print_help(parser, streams, argv[0], streams.err);
                return STATUS_BUILTIN_ERROR;
            }
            case 'h': {
                builtin_print_help(parser, streams, argv[0], streams.out);
                return STATUS_BUILTIN_OK;
            }
            case 'n': {
                list = 1;
                break;
            }
            case '?': {
                builtin_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return STATUS_BUILTIN_ERROR;
            }
            default: {
                DIE("unexpected opt");
                break;
            }
        }
    }

    if (list) {
        wcstring_list_t names = builtin_get_names();
        sort(names.begin(), names.end());

        for (size_t i = 0; i < names.size(); i++) {
            const wchar_t *el = names.at(i).c_str();

            streams.out.append(el);
            streams.out.append(L"\n");
        }
    }
    return STATUS_BUILTIN_OK;
}

/// Implementation of the builtin emit command, used to create events.
static int builtin_emit(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wgetopter_t w;
    int argc = builtin_count_args(argv);

    static const struct woption long_options[] = {{L"help", no_argument, 0, 'h'}, {0, 0, 0, 0}};

    while (1) {
        int opt_index = 0;

        int opt = w.wgetopt_long(argc, argv, L"h", long_options, &opt_index);
        if (opt == -1) break;

        switch (opt) {
            case 0: {
                if (long_options[opt_index].flag != 0) break;
                streams.err.append_format(BUILTIN_ERR_UNKNOWN, argv[0],
                                          long_options[opt_index].name);
                builtin_print_help(parser, streams, argv[0], streams.err);
                return STATUS_BUILTIN_ERROR;
            }
            case 'h': {
                builtin_print_help(parser, streams, argv[0], streams.out);
                return STATUS_BUILTIN_OK;
            }
            case '?': {
                builtin_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return STATUS_BUILTIN_ERROR;
            }
            default: {
                DIE("unexpected opt");
                break;
            }
        }
    }

    if (!argv[w.woptind]) {
        streams.err.append_format(L"%ls: expected event name\n", argv[0]);
        return STATUS_BUILTIN_ERROR;
    }
    const wchar_t *eventname = argv[w.woptind];
    wcstring_list_t args(argv + w.woptind + 1, argv + argc);
    event_fire_generic(eventname, &args);

    return STATUS_BUILTIN_OK;
}

/// Implementation of the builtin 'command'. Actual command running is handled by the parser, this
/// just processes the flags.
static int builtin_command(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wgetopter_t w;
    int argc = builtin_count_args(argv);
    bool find_path = false;
    bool quiet = false;

    w.woptind = 0;

    static const struct woption long_options[] = {{L"quiet", no_argument, NULL, 'q'},
                                                  {L"search", no_argument, NULL, 's'},
                                                  {L"help", no_argument, NULL, 'h'},
                                                  {NULL, 0, NULL, 0}};

    while (1) {
        int opt_index = 0;

        int opt = w.wgetopt_long(argc, argv, L"qsvh", long_options, &opt_index);
        if (opt == -1) break;

        switch (opt) {
            case 0: {
                if (long_options[opt_index].flag != 0) break;
                streams.err.append_format(BUILTIN_ERR_UNKNOWN, argv[0],
                                          long_options[opt_index].name);
                builtin_print_help(parser, streams, argv[0], streams.err);
                return STATUS_BUILTIN_ERROR;
            }
            case 'h': {
                builtin_print_help(parser, streams, argv[0], streams.out);
                return STATUS_BUILTIN_OK;
            }
            case 's':
            case 'v': {
                find_path = true;
                break;
            }
            case 'q': {
                quiet = true;
                break;
            }
            case '?': {
                builtin_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return STATUS_BUILTIN_ERROR;
            }
            default: {
                DIE("unexpected opt");
                break;
            }
        }
    }

    if (!find_path) {
        builtin_print_help(parser, streams, argv[0], streams.out);
        return STATUS_BUILTIN_ERROR;
    }

    int found = 0;

    for (int idx = w.woptind; argv[idx]; ++idx) {
        const wchar_t *command_name = argv[idx];
        wcstring path;
        if (path_get_path(command_name, &path)) {
            if (!quiet) streams.out.append_format(L"%ls\n", path.c_str());
            ++found;
        }
    }
    return found ? STATUS_BUILTIN_OK : STATUS_BUILTIN_ERROR;
}

/// A generic bultin that only supports showing a help message. This is only a placeholder that
/// prints the help message. Useful for commands that live in the parser.
static int builtin_generic(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wgetopter_t w;
    int argc = builtin_count_args(argv);

    // Hackish - if we have no arguments other than the command, we are a "naked invocation" and we
    // just print help.
    if (argc == 1) {
        builtin_print_help(parser, streams, argv[0], streams.out);
        return STATUS_BUILTIN_ERROR;
    }

    static const struct woption long_options[] = {{L"help", no_argument, 0, 'h'}, {0, 0, 0, 0}};

    while (1) {
        int opt_index = 0;

        int opt = w.wgetopt_long(argc, argv, L"h", long_options, &opt_index);
        if (opt == -1) break;

        switch (opt) {
            case 0: {
                if (long_options[opt_index].flag != 0) break;
                streams.err.append_format(BUILTIN_ERR_UNKNOWN, argv[0],
                                          long_options[opt_index].name);
                builtin_print_help(parser, streams, argv[0], streams.err);
                return STATUS_BUILTIN_ERROR;
            }
            case 'h': {
                builtin_print_help(parser, streams, argv[0], streams.out);
                return STATUS_BUILTIN_OK;
            }
            case '?': {
                builtin_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return STATUS_BUILTIN_ERROR;
            }
            default: {
                DIE("unexpected opt");
                break;
            }
        }
    }

    return STATUS_BUILTIN_ERROR;
}

/// Return a definition of the specified function. Used by the functions builtin.
static wcstring functions_def(const wcstring &name) {
    CHECK(!name.empty(), L"");  //!OCLINT(multiple unary operator)
    wcstring out;
    wcstring desc, def;
    function_get_desc(name, &desc);
    function_get_definition(name, &def);
    event_t search(EVENT_ANY);
    search.function_name = name;
    std::vector<std::shared_ptr<event_t>> ev;
    event_get(search, &ev);

    out.append(L"function ");

    // Typically we prefer to specify the function name first, e.g. "function foo --description bar"
    // But If the function name starts with a -, we'll need to output it after all the options.
    bool defer_function_name = (name.at(0) == L'-');
    if (!defer_function_name) {
        out.append(escape_string(name, true));
    }

    if (!desc.empty()) {
        wcstring esc_desc = escape_string(desc, true);
        out.append(L" --description ");
        out.append(esc_desc);
    }

    if (!function_get_shadow_scope(name)) {
        out.append(L" --no-scope-shadowing");
    }

    for (size_t i = 0; i < ev.size(); i++) {
        const event_t *next = ev.at(i).get();
        switch (next->type) {
            case EVENT_SIGNAL: {
                append_format(out, L" --on-signal %ls", sig2wcs(next->param1.signal));
                break;
            }
            case EVENT_VARIABLE: {
                append_format(out, L" --on-variable %ls", next->str_param1.c_str());
                break;
            }
            case EVENT_EXIT: {
                if (next->param1.pid > 0)
                    append_format(out, L" --on-process-exit %d", next->param1.pid);
                else
                    append_format(out, L" --on-job-exit %d", -next->param1.pid);
                break;
            }
            case EVENT_JOB_ID: {
                const job_t *j = job_get(next->param1.job_id);
                if (j) append_format(out, L" --on-job-exit %d", j->pgid);
                break;
            }
            case EVENT_GENERIC: {
                append_format(out, L" --on-event %ls", next->str_param1.c_str());
                break;
            }
            default: {
                DIE("unexpected next->type");
                break;
            }
        }
    }

    wcstring_list_t named = function_get_named_arguments(name);
    if (!named.empty()) {
        append_format(out, L" --argument");
        for (size_t i = 0; i < named.size(); i++) {
            append_format(out, L" %ls", named.at(i).c_str());
        }
    }

    // Output the function name if we deferred it.
    if (defer_function_name) {
        out.append(L" -- ");
        out.append(escape_string(name, true));
    }

    // Output any inherited variables as `set -l` lines.
    std::map<wcstring, env_var_t> inherit_vars = function_get_inherit_vars(name);
    for (std::map<wcstring, env_var_t>::const_iterator it = inherit_vars.begin(),
                                                       end = inherit_vars.end();
         it != end; ++it) {
        wcstring_list_t lst;
        if (!it->second.missing()) {
            tokenize_variable_array(it->second, lst);
        }

        // This forced tab is crummy, but we don't know what indentation style the function uses.
        append_format(out, L"\n\tset -l %ls", it->first.c_str());
        for (wcstring_list_t::const_iterator arg_it = lst.begin(), arg_end = lst.end();
             arg_it != arg_end; ++arg_it) {
            wcstring earg = escape_string(*arg_it, ESCAPE_ALL);
            out.push_back(L' ');
            out.append(earg);
        }
    }

    // This forced tab is sort of crummy - not all functions start with a tab.
    append_format(out, L"\n\t%ls", def.c_str());

    // Append a newline before the 'end', unless there already is one there.
    if (!string_suffixes_string(L"\n", def)) {
        out.push_back(L'\n');
    }
    out.append(L"end\n");
    return out;
}

static int report_function_metadata(const wchar_t *funcname, bool verbose, io_streams_t &streams,
                                    bool metadata_as_comments) {
    const wchar_t *path = L"n/a";
    const wchar_t *autoloaded = L"n/a";
    const wchar_t *shadows_scope = L"n/a";
    int line_number = 0;

    if (function_exists(funcname)) {
        path = function_get_definition_file(funcname);
        if (path) {
            autoloaded = function_is_autoloaded(funcname) ? L"autoloaded" : L"not-autoloaded";
            line_number = function_get_definition_offset(funcname);
        } else {
            path = L"stdin";
        }
        shadows_scope =
            function_get_shadow_scope(funcname) ? L"scope-shadowing" : L"no-scope-shadowing";
    }

    if (metadata_as_comments) {
        if (wcscmp(path, L"stdin")) {
            streams.out.append_format(L"# Defined in %ls @ line %d\n", path, line_number);
        }
    } else {
        streams.out.append_format(L"%ls\n", path);
        if (verbose) {
            streams.out.append_format(L"%ls\n", autoloaded);
            streams.out.append_format(L"%d\n", line_number);
            streams.out.append_format(L"%ls\n", shadows_scope);
        }
    }

    return STATUS_BUILTIN_OK;
}

/// The functions builtin, used for listing and erasing functions.
static int builtin_functions(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wgetopter_t w;
    int i;
    int erase = 0;
    wchar_t *desc = 0;

    int argc = builtin_count_args(argv);
    int list = 0;
    int show_hidden = 0;
    int res = STATUS_BUILTIN_OK;
    int query = 0;
    int copy = 0;
    bool report_metadata = false;
    bool verbose = false;

    static const struct woption long_options[] = {
        {L"erase", no_argument, NULL, 'e'},   {L"description", required_argument, NULL, 'd'},
        {L"names", no_argument, NULL, 'n'},   {L"all", no_argument, NULL, 'a'},
        {L"help", no_argument, NULL, 'h'},    {L"query", no_argument, NULL, 'q'},
        {L"copy", no_argument, NULL, 'c'},    {L"metadata", no_argument, NULL, 'm'},
        {L"verbose", no_argument, NULL, 'v'}, {NULL, 0, NULL, 0}};

    while (1) {
        int opt_index = 0;

        int opt = w.wgetopt_long(argc, argv, L"ed:mnahqcv", long_options, &opt_index);
        if (opt == -1) break;

        switch (opt) {
            case 0: {
                if (long_options[opt_index].flag != 0) break;
                streams.err.append_format(BUILTIN_ERR_UNKNOWN, argv[0],
                                          long_options[opt_index].name);
                builtin_print_help(parser, streams, argv[0], streams.err);
                return STATUS_BUILTIN_ERROR;
            }
            case 'v': {
                verbose = true;
                break;
            }
            case 'e': {
                erase = 1;
                break;
            }
            case 'm': {
                report_metadata = true;
                break;
            }
            case 'd': {
                desc = w.woptarg;
                break;
            }
            case 'n': {
                list = 1;
                break;
            }
            case 'a': {
                show_hidden = 1;
                break;
            }
            case 'h': {
                builtin_print_help(parser, streams, argv[0], streams.out);
                return STATUS_BUILTIN_OK;
            }
            case 'q': {
                query = 1;
                break;
            }
            case 'c': {
                copy = 1;
                break;
            }
            case '?': {
                builtin_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return STATUS_BUILTIN_ERROR;
            }
            default: {
                DIE("unexpected opt");
                break;
            }
        }
    }

    // Erase, desc, query, copy and list are mutually exclusive.
    int describe = desc ? 1 : 0;
    if (erase + describe + list + query + copy > 1) {
        streams.err.append_format(_(L"%ls: Invalid combination of options\n"), argv[0]);

        builtin_print_help(parser, streams, argv[0], streams.err);

        return STATUS_BUILTIN_ERROR;
    }

    if (erase) {
        int i;
        for (i = w.woptind; i < argc; i++) function_remove(argv[i]);
        return STATUS_BUILTIN_OK;
    } else if (desc) {
        wchar_t *func;

        if (argc - w.woptind != 1) {
            streams.err.append_format(_(L"%ls: Expected exactly one function name\n"), argv[0]);
            builtin_print_help(parser, streams, argv[0], streams.err);

            return STATUS_BUILTIN_ERROR;
        }
        func = argv[w.woptind];
        if (!function_exists(func)) {
            streams.err.append_format(_(L"%ls: Function '%ls' does not exist\n"), argv[0], func);

            builtin_print_help(parser, streams, argv[0], streams.err);

            return STATUS_BUILTIN_ERROR;
        }

        function_set_desc(func, desc);
        return STATUS_BUILTIN_OK;
    } else if (report_metadata) {
        if (argc - w.woptind != 1) {
            streams.err.append_format(
                _(L"%ls: Expected exactly one function name for --metadata\n"), argv[0]);
            return STATUS_BUILTIN_ERROR;
        }

        const wchar_t *funcname = argv[w.woptind];
        return report_function_metadata(funcname, verbose, streams, false);
    } else if (list || (argc == w.woptind)) {
        int is_screen = !streams.out_is_redirected && isatty(STDOUT_FILENO);
        size_t i;
        wcstring_list_t names = function_get_names(show_hidden);
        std::sort(names.begin(), names.end());
        if (is_screen) {
            wcstring buff;

            for (i = 0; i < names.size(); i++) {
                buff.append(names.at(i));
                buff.append(L", ");
            }

            streams.out.append(reformat_for_screen(buff));
        } else {
            for (i = 0; i < names.size(); i++) {
                streams.out.append(names.at(i).c_str());
                streams.out.append(L"\n");
            }
        }

        return STATUS_BUILTIN_OK;
    } else if (copy) {
        wcstring current_func;
        wcstring new_func;

        if (argc - w.woptind != 2) {
            streams.err.append_format(_(L"%ls: Expected exactly two names (current function name, "
                                        L"and new function name)\n"),
                                      argv[0]);
            builtin_print_help(parser, streams, argv[0], streams.err);

            return STATUS_BUILTIN_ERROR;
        }
        current_func = argv[w.woptind];
        new_func = argv[w.woptind + 1];

        if (!function_exists(current_func)) {
            streams.err.append_format(_(L"%ls: Function '%ls' does not exist\n"), argv[0],
                                      current_func.c_str());
            builtin_print_help(parser, streams, argv[0], streams.err);

            return STATUS_BUILTIN_ERROR;
        }

        if (!wcsfuncname(new_func) || parser_keywords_is_reserved(new_func)) {
            streams.err.append_format(_(L"%ls: Illegal function name '%ls'\n"), argv[0],
                                      new_func.c_str());
            builtin_print_help(parser, streams, argv[0], streams.err);
            return STATUS_BUILTIN_ERROR;
        }

        // Keep things simple: don't allow existing names to be copy targets.
        if (function_exists(new_func)) {
            streams.err.append_format(
                _(L"%ls: Function '%ls' already exists. Cannot create copy '%ls'\n"), argv[0],
                new_func.c_str(), current_func.c_str());
            builtin_print_help(parser, streams, argv[0], streams.err);

            return STATUS_BUILTIN_ERROR;
        }

        if (function_copy(current_func, new_func)) return STATUS_BUILTIN_OK;
        return STATUS_BUILTIN_ERROR;
    }

    for (i = w.woptind; i < argc; i++) {
        if (!function_exists(argv[i]))
            res++;
        else {
            if (!query) {
                if (i != w.woptind) streams.out.append(L"\n");
                const wchar_t *funcname = argv[w.woptind];
                report_function_metadata(funcname, verbose, streams, true);
                streams.out.append(functions_def(funcname));
            }
        }
    }

    return res;
}

// Convert a octal or hex character to its binary value. Surprisingly a version
// of this function using a lookup table is only ~1.5% faster than the `switch`
// statement version below. Since that requires initializing a table statically
// (which is problematic if we run on an EBCDIC system) we don't use that
// solution. Also, we relax the style rule that `case` blocks should always be
// enclosed in parentheses given the nature of this code.
static unsigned int builtin_echo_digit(wchar_t wc, unsigned int base) {
    assert(base == 8 || base == 16);  // base must be hex or octal
    switch (wc) {
        case L'0':
            return 0;
        case L'1':
            return 1;
        case L'2':
            return 2;
        case L'3':
            return 3;
        case L'4':
            return 4;
        case L'5':
            return 5;
        case L'6':
            return 6;
        case L'7':
            return 7;
        default: { break; }
    }

    if (base != 16) return UINT_MAX;

    switch (wc) {
        case L'8':
            return 8;
        case L'9':
            return 9;
        case L'a':
        case L'A':
            return 10;
        case L'b':
        case L'B':
            return 11;
        case L'c':
        case L'C':
            return 12;
        case L'd':
        case L'D':
            return 13;
        case L'e':
        case L'E':
            return 14;
        case L'f':
        case L'F':
            return 15;
        default: { break; }
    }

    return UINT_MAX;
}

/// Parse a numeric escape sequence in str, returning whether we succeeded. Also return the number
/// of characters consumed and the resulting value. Supported escape sequences:
///
/// \0nnn: octal value, zero to three digits
/// \nnn: octal value, one to three digits
/// \xhh: hex value, one to two digits
static bool builtin_echo_parse_numeric_sequence(const wchar_t *str, size_t *consumed,
                                                unsigned char *out_val) {
    bool success = false;
    unsigned int start = 0;  // the first character of the numeric part of the sequence

    unsigned int base = 0, max_digits = 0;
    if (builtin_echo_digit(str[0], 8) != UINT_MAX) {
        // Octal escape
        base = 8;

        // If the first digit is a 0, we allow four digits (including that zero); otherwise, we
        // allow 3.
        max_digits = (str[0] == L'0' ? 4 : 3);
    } else if (str[0] == L'x') {
        // Hex escape
        base = 16;
        max_digits = 2;

        // Skip the x
        start = 1;
    }

    if (base == 0) {
        return success;
    }

    unsigned int idx;
    unsigned char val = 0;  // resulting character
    for (idx = start; idx < start + max_digits; idx++) {
        unsigned int digit = builtin_echo_digit(str[idx], base);
        if (digit == UINT_MAX) break;
        val = val * base + digit;
    }

    // We succeeded if we consumed at least one digit.
    if (idx > start) {
        *consumed = idx;
        *out_val = val;
        success = true;
    }
    return success;
}

/// The echo builtin.
///
/// Bash only respects -n if it's the first argument. We'll do the same. We also support a new
/// option -s to mean "no spaces"
static int builtin_echo(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    // Skip first arg
    if (!*argv++) return STATUS_BUILTIN_ERROR;

    // Process options. Options must come at the beginning - the first non-option kicks us out.
    bool print_newline = true, print_spaces = true, interpret_special_chars = false;
    size_t option_idx = 0;
    for (option_idx = 0; argv[option_idx] != NULL; option_idx++) {
        const wchar_t *arg = argv[option_idx];
        assert(arg != NULL);
        bool arg_is_valid_option = false;
        if (arg[0] == L'-') {
            // We have a leading dash. Ensure that every subseqnent character is a valid option.
            size_t i = 1;
            while (arg[i] != L'\0' && wcschr(L"nesE", arg[i]) != NULL) {
                i++;
            }
            // We must have at least two characters to be a valid option, and have consumed the
            // whole string.
            arg_is_valid_option = (i >= 2 && arg[i] == L'\0');
        }

        if (!arg_is_valid_option) {
            // This argument is not an option, so there are no more options.
            break;
        }

        // Ok, we are sure the argument is an option. Parse it.
        assert(arg_is_valid_option);
        for (size_t i = 1; arg[i] != L'\0'; i++) {
            switch (arg[i]) {
                case L'n': {
                    print_newline = false;
                    break;
                }
                case L'e': {
                    interpret_special_chars = true;
                    break;
                }
                case L's': {
                    print_spaces = false;
                    break;
                }
                case L'E': {
                    interpret_special_chars = false;
                    break;
                }
                default: {
                    DIE("unexpected character in builtin_echo argument");
                    break;
                }
            }
        }
    }

    // The special character \c can be used to indicate no more output.
    bool continue_output = true;

    /* Skip over the options */
    const wchar_t *const *args_to_echo = argv + option_idx;
    for (size_t idx = 0; continue_output && args_to_echo[idx] != NULL; idx++) {
        if (print_spaces && idx > 0) {
            streams.out.push_back(' ');
        }

        const wchar_t *str = args_to_echo[idx];
        for (size_t j = 0; continue_output && str[j]; j++) {
            if (!interpret_special_chars || str[j] != L'\\') {
                // Not an escape.
                streams.out.push_back(str[j]);
            } else {
                // Most escapes consume one character in addition to the backslash; the numeric
                // sequences may consume more, while an unrecognized escape sequence consumes none.
                wchar_t wc;
                size_t consumed = 1;
                switch (str[j + 1]) {
                    case L'a': {
                        wc = L'\a';
                        break;
                    }
                    case L'b': {
                        wc = L'\b';
                        break;
                    }
                    case L'e': {
                        wc = L'\e';
                        break;
                    }
                    case L'f': {
                        wc = L'\f';
                        break;
                    }
                    case L'n': {
                        wc = L'\n';
                        break;
                    }
                    case L'r': {
                        wc = L'\r';
                        break;
                    }
                    case L't': {
                        wc = L'\t';
                        break;
                    }
                    case L'v': {
                        wc = L'\v';
                        break;
                    }
                    case L'\\': {
                        wc = L'\\';
                        break;
                    }
                    case L'c': {
                        wc = 0;
                        continue_output = false;
                        break;
                    }
                    default: {
                        // Octal and hex escape sequences.
                        unsigned char narrow_val = 0;
                        if (builtin_echo_parse_numeric_sequence(str + j + 1, &consumed,
                                                                &narrow_val)) {
                            // Here consumed must have been set to something. The narrow_val is a
                            // literal byte that we want to output (#1894).
                            wc = ENCODE_DIRECT_BASE + narrow_val % 256;
                        } else {
                            // Not a recognized escape. We consume only the backslash.
                            wc = L'\\';
                            consumed = 0;
                        }
                        break;
                    }
                }

                // Skip over characters that were part of this escape sequence (but not the
                // backslash, which will be handled by the loop increment.
                j += consumed;

                if (continue_output) {
                    streams.out.push_back(wc);
                }
            }
        }
    }
    if (print_newline && continue_output) {
        streams.out.push_back('\n');
    }
    return STATUS_BUILTIN_OK;
}

/// The pwd builtin. We don't respect -P to resolve symbolic links because we
/// try to always resolve them.
static int builtin_pwd(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    if (argv[1] != NULL) {
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT1, argv[0], 0, builtin_count_args(argv));
        return STATUS_BUILTIN_ERROR;
    }

    wcstring res = wgetcwd();
    if (res.empty()) {
        return STATUS_BUILTIN_ERROR;
    }
    streams.out.append(res);
    streams.out.push_back(L'\n');
    return STATUS_BUILTIN_OK;
}

static int validate_function_name(int argc, const wchar_t *const *argv, wcstring &function_name,
                                  const wchar_t *cmd, wcstring *out_err) {
    if (argc < 2) {
        // This is currently impossible but let's be paranoid.
        append_format(*out_err, _(L"%ls: Expected function name"), cmd);
        return STATUS_BUILTIN_ERROR;
    }

    function_name = argv[1];
    if (!wcsfuncname(function_name)) {
        append_format(*out_err, _(L"%ls: Illegal function name '%ls'"), cmd, function_name.c_str());
        return STATUS_BUILTIN_ERROR;
    }

    if (parser_keywords_is_reserved(function_name)) {
        append_format(
            *out_err,
            _(L"%ls: The name '%ls' is reserved,\nand can not be used as a function name"), cmd,
            function_name.c_str());
        return STATUS_BUILTIN_ERROR;
    }

    return STATUS_BUILTIN_OK;
}

/// Define a function. Calls into `function.cpp` to perform the heavy lifting of defining a
/// function.
int builtin_function(parser_t &parser, io_streams_t &streams, const wcstring_list_t &c_args,
                     const wcstring &contents, int definition_line_offset, wcstring *out_err) {
    assert(out_err != NULL);

    // The wgetopt function expects 'function' as the first argument. Make a new wcstring_list with
    // that property. This is needed because this builtin has a different signature than the other
    // builtins.
    wcstring_list_t args;
    args.push_back(L"function");
    args.insert(args.end(), c_args.begin(), c_args.end());

    // Hackish const_cast matches the one in builtin_run.
    const null_terminated_array_t<wchar_t> argv_array(args);
    wchar_t **argv = const_cast<wchar_t **>(argv_array.get());
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    wchar_t *desc = NULL;
    bool shadow_scope = true;
    wcstring function_name;
    std::vector<event_t> events;
    wcstring_list_t named_arguments;
    wcstring_list_t inherit_vars;
    wcstring_list_t wrap_targets;

    // This command is atypical in using the "+" (REQUIRE_ORDER) option for flag parsing.
    // This is needed due to the semantics of the -a/--argument-names flag.
    const wchar_t *short_options = L"+:a:d:e:hj:p:s:v:w:SV:";
    const struct woption long_options[] = {{L"description", required_argument, NULL, 'd'},
                                           {L"on-signal", required_argument, NULL, 's'},
                                           {L"on-job-exit", required_argument, NULL, 'j'},
                                           {L"on-process-exit", required_argument, NULL, 'p'},
                                           {L"on-variable", required_argument, NULL, 'v'},
                                           {L"on-event", required_argument, NULL, 'e'},
                                           {L"wraps", required_argument, NULL, 'w'},
                                           {L"help", no_argument, NULL, 'h'},
                                           {L"argument-names", required_argument, NULL, 'a'},
                                           {L"no-scope-shadowing", no_argument, NULL, 'S'},
                                           {L"inherit-variable", required_argument, NULL, 'V'},
                                           {NULL, 0, NULL, 0}};

    // A valid function name has to be the first argument.
    if (validate_function_name(argc, argv, function_name, cmd, out_err) == STATUS_BUILTIN_OK) {
        argv++;
        argc--;
    } else {
        return STATUS_BUILTIN_ERROR;
    }

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 'd': {
                desc = w.woptarg;
                break;
            }
            case 's': {
                int sig = wcs2sig(w.woptarg);
                if (sig == -1) {
                    append_format(*out_err, _(L"%ls: Unknown signal '%ls'"), cmd, w.woptarg);
                    return STATUS_BUILTIN_ERROR;
                }
                events.push_back(event_t::signal_event(sig));
                break;
            }
            case 'v': {
                if (wcsvarname(w.woptarg)) {
                    append_format(*out_err, _(L"%ls: Invalid variable name '%ls'"), cmd, w.woptarg);
                    return STATUS_BUILTIN_ERROR;
                }

                events.push_back(event_t::variable_event(w.woptarg));
                break;
            }
            case 'e': {
                events.push_back(event_t::generic_event(w.woptarg));
                break;
            }
            case 'j':
            case 'p': {
                pid_t pid;
                event_t e(EVENT_ANY);

                if ((opt == 'j') && (wcscasecmp(w.woptarg, L"caller") == 0)) {
                    job_id_t job_id = -1;

                    if (is_subshell) {
                        size_t block_idx = 0;

                        // Find the outermost substitution block.
                        for (block_idx = 0;; block_idx++) {
                            const block_t *b = parser.block_at_index(block_idx);
                            if (b == NULL || b->type() == SUBST) break;
                        }

                        // Go one step beyond that, to get to the caller.
                        const block_t *caller_block = parser.block_at_index(block_idx + 1);
                        if (caller_block != NULL && caller_block->job != NULL) {
                            job_id = caller_block->job->job_id;
                        }
                    }

                    if (job_id == -1) {
                        append_format(*out_err,
                                      _(L"%ls: Cannot find calling job for event handler"), cmd);
                        return STATUS_BUILTIN_ERROR;
                    }
                    e.type = EVENT_JOB_ID;
                    e.param1.job_id = job_id;
                } else {
                    pid = fish_wcstoi(w.woptarg);
                    if (errno || pid < 0) {
                        append_format(*out_err, _(L"%ls: Invalid process id '%ls'"), cmd,
                                      w.woptarg);
                        return STATUS_BUILTIN_ERROR;
                    }

                    e.type = EVENT_EXIT;
                    e.param1.pid = (opt == 'j' ? -1 : 1) * abs(pid);
                }
                events.push_back(e);
                break;
            }
            case 'a': {
                named_arguments.push_back(w.woptarg);
                break;
            }
            case 'S': {
                shadow_scope = false;
                break;
            }
            case 'w': {
                wrap_targets.push_back(w.woptarg);
                break;
            }
            case 'V': {
                if (wcsvarname(w.woptarg)) {
                    append_format(*out_err, _(L"%ls: Invalid variable name '%ls'"), cmd, w.woptarg);
                    return STATUS_BUILTIN_ERROR;
                }

                inherit_vars.push_back(w.woptarg);
                break;
            }
            case 'h': {
                builtin_print_help(parser, streams, cmd, streams.out);
                return STATUS_BUILTIN_OK;
            }
            case ':': {
                streams.err.append_format(BUILTIN_ERR_MISSING, cmd, argv[w.woptind - 1]);
                return STATUS_BUILTIN_ERROR;
            }
            case '?': {
                builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_BUILTIN_ERROR;
            }
            default: {
                DIE("unexpected retval from wgetopt_long");
                break;
            }
        }
    }

    if (argc != w.woptind) {
        if (named_arguments.size()) {
            for (int i = w.woptind; i < argc; i++) {
                named_arguments.push_back(argv[i]);
            }
        } else {
            append_format(*out_err, _(L"%ls: Unexpected positional argument '%ls'"), cmd,
                          argv[w.woptind]);
            return STATUS_BUILTIN_ERROR;
        }
    }

    // We have what we need to actually define the function.
    function_data_t d;
    d.name = function_name;
    if (desc) d.description = desc;
    d.events.swap(events);
    d.shadow_scope = shadow_scope;
    d.named_arguments.swap(named_arguments);
    d.inherit_vars.swap(inherit_vars);

    for (size_t i = 0; i < d.events.size(); i++) {
        event_t &e = d.events.at(i);
        e.function_name = d.name;
    }

    d.definition = contents.c_str();
    function_add(d, parser, definition_line_offset);

    // Handle wrap targets by creating the appropriate completions.
    for (size_t w = 0; w < wrap_targets.size(); w++) {
        complete_add_wrapper(function_name, wrap_targets.at(w));
    }

    return STATUS_BUILTIN_OK;
}

/// The random builtin generates random numbers.
static int builtin_random(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
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

    wgetopter_t w;
    int argc = builtin_count_args(argv);
    static const struct woption long_options[] = {{L"help", no_argument, NULL, 'h'},
                                                  {NULL, 0, NULL, 0}};
    while (1) {
        int opt_index = 0;

        int opt = w.wgetopt_long(argc, argv, L"h", long_options, &opt_index);
        if (opt == -1) break;

        switch (opt) {
            case 0: {
                if (long_options[opt_index].flag != 0) break;
                streams.err.append_format(BUILTIN_ERR_UNKNOWN, argv[0],
                                          long_options[opt_index].name);
                builtin_print_help(parser, streams, argv[0], streams.err);
                return STATUS_BUILTIN_ERROR;
            }
            case 'h': {
                builtin_print_help(parser, streams, argv[0], streams.out);
                return STATUS_BUILTIN_OK;
            }
            case '?': {
                builtin_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return STATUS_BUILTIN_ERROR;
            }
            default: {
                DIE("unexpected opt");
                break;
            }
        }
    }

    int arg_count = argc - w.woptind;
    long long start, end;
    unsigned long long step;
    bool choice = false;
    if (arg_count >= 1 && !wcscmp(argv[w.woptind], L"choice")) {
        if (arg_count == 1) {
            streams.err.append_format(L"%ls: nothing to choose from\n", argv[0]);
            return STATUS_BUILTIN_ERROR;
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
                streams.err.append_format(L"%ls: %ls is not a valid integer\n", argv[0], str);
                parse_error = true;
            }
            return ll;
        };
        auto parse_ull = [&](const wchar_t *str) {
            unsigned long long ull = fish_wcstoull(str);
            if (errno) {
                streams.err.append_format(L"%ls: %ls is not a valid integer\n", argv[0], str);
                parse_error = true;
            }
            return ull;
        };
        if (arg_count == 0) {
            start = 0;
            end = 32767;
            step = 1;
        } else if (arg_count == 1) {
            long long seed = parse_ll(argv[w.woptind]);
            if (parse_error) return STATUS_BUILTIN_ERROR;
            engine.seed(static_cast<uint32_t>(seed));
            return STATUS_BUILTIN_OK;
        } else if (arg_count == 2) {
            start = parse_ll(argv[w.woptind]);
            step = 1;
            end = parse_ll(argv[w.woptind + 1]);
        } else if (arg_count == 3) {
            start = parse_ll(argv[w.woptind]);
            step = parse_ull(argv[w.woptind + 1]);
            end = parse_ll(argv[w.woptind + 2]);
        } else {
            streams.err.append_format(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
            return STATUS_BUILTIN_ERROR;
        }

        if (parse_error) {
            return STATUS_BUILTIN_ERROR;
        } else if (start >= end) {
            streams.err.append_format(L"%ls: END must be greater than START\n", argv[0]);
            return STATUS_BUILTIN_ERROR;
        } else if (step == 0) {
            streams.err.append_format(L"%ls: STEP must be a positive integer\n", argv[0]);
            return STATUS_BUILTIN_ERROR;
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
        streams.err.append_format(L"%ls: range contains only one possible value\n", argv[0]);
        return STATUS_BUILTIN_ERROR;
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
        streams.out.append_format(L"%ls\n", argv[w.woptind + result]);
    } else {
        streams.out.append_format(L"%lld\n", result);
    }
    return STATUS_BUILTIN_OK;
}

/// Read from the tty. This is only valid when the stream is stdin and it is attached to a tty and
/// we weren't asked to split on null characters.
static int read_interactive(wcstring &buff, int nchars, bool shell, const wchar_t *mode_name,
                            const wchar_t *prompt, const wchar_t *right_prompt,
                            const wchar_t *commandline) {
    int exit_res = STATUS_BUILTIN_OK;
    const wchar_t *line;

    reader_push(mode_name);
    reader_set_left_prompt(prompt);
    reader_set_right_prompt(right_prompt);
    if (shell) {
        reader_set_complete_function(&complete);
        reader_set_highlight_function(&highlight_shell);
        reader_set_test_function(&reader_shell_test);
    }
    // No autosuggestions or abbreviations in builtin_read.
    reader_set_allow_autosuggesting(false);
    reader_set_expand_abbreviations(false);
    reader_set_exit_on_interrupt(true);

    reader_set_buffer(commandline, wcslen(commandline));
    proc_push_interactive(1);

    event_fire_generic(L"fish_prompt");
    line = reader_readline(nchars);
    proc_pop_interactive();
    if (line) {
        if (0 < nchars && (size_t)nchars < wcslen(line)) {
            // Line may be longer than nchars if a keybinding used `commandline -i`
            // note: we're deliberately throwing away the tail of the commandline.
            // It shouldn't be unread because it was produced with `commandline -i`,
            // not typed.
            buff = wcstring(line, nchars);
        } else {
            buff = wcstring(line);
        }
    } else {
        exit_res = STATUS_BUILTIN_ERROR;
    }
    reader_pop();
    return exit_res;
}

/// Bash uses 128 bytes for its chunk size. Very informal testing I did suggested that a smaller
/// chunk size performed better. However, we're going to use the bash value under the assumption
/// they've done more extensive testing.
#define READ_CHUNK_SIZE 128

/// Read from the fd in chunks until we see newline or null, as appropriate, is seen. This is only
/// used when the fd is seekable (so not from a tty or pipe) and we're not reading a specific number
/// of chars.
/// Returns an exit status
static int read_in_chunks(int fd, wcstring &buff, bool split_null) {
    int exit_res = STATUS_BUILTIN_OK;
    std::string str;
    bool eof = false;
    bool finished = false;

    while (!finished) {
        char inbuf[READ_CHUNK_SIZE];
        long bytes_read = read_blocked(fd, inbuf, READ_CHUNK_SIZE);

        if (bytes_read <= 0) {
            eof = true;
            break;
        }

        const char *end = std::find(inbuf, inbuf + bytes_read, split_null ? L'\0' : L'\n');
        long bytes_consumed = end - inbuf;  // note: must be signed for use in lseek
        assert(bytes_consumed <= bytes_read);
        str.append(inbuf, bytes_consumed);
        if (bytes_consumed < bytes_read) {
            // We found a splitter
            // +1 because we need to treat the splitter as consumed, but not append it to the string
            CHECK(lseek(fd, bytes_consumed - bytes_read + 1, SEEK_CUR) != -1, STATUS_BUILTIN_ERROR)
            finished = true;
        }
    }

    buff = str2wcstring(str);
    if (buff.empty() && eof) {
        exit_res = STATUS_BUILTIN_ERROR;
    }

    return exit_res;
}

/// Read from the fd on char at a time until we've read the requested number of characters or a
/// newline or null, as appropriate, is seen. This is inefficient so should only be used when the
/// fd is not seekable.
static int read_one_char_at_a_time(int fd, wcstring &buff, int nchars, bool split_null) {
    int exit_res = STATUS_BUILTIN_OK;
    bool eof = false;

    while (true) {
        bool finished = false;
        wchar_t res = 0;
        mbstate_t state = {};

        while (!finished) {
            char b;
            if (read_blocked(fd, &b, 1) <= 0) {
                eof = true;
                break;
            }

            if (MB_CUR_MAX == 1) {
                res = (unsigned char)b;
                finished = true;
            } else {
                size_t sz = mbrtowc(&res, &b, 1, &state);
                if (sz == (size_t)-1) {
                    memset(&state, 0, sizeof(state));
                } else if (sz != (size_t)-2) {
                    finished = true;
                }
            }
        }

        if (eof) break;
        if (!split_null && res == L'\n') break;
        if (split_null && res == L'\0') break;

        buff.push_back(res);
        if (0 < nchars && (size_t)nchars <= buff.size()) {
            break;
        }
    }

    if (buff.empty() && eof) {
        exit_res = STATUS_BUILTIN_ERROR;
    }

    return exit_res;
}

/// The read builtin. Reads from stdin and stores the values in environment variables.
static int builtin_read(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wgetopter_t w;
    wcstring buff;
    int argc = builtin_count_args(argv);
    int place = ENV_USER;
    const wchar_t *prompt = DEFAULT_READ_PROMPT;
    const wchar_t *right_prompt = L"";
    const wchar_t *commandline = L"";
    int exit_res = STATUS_BUILTIN_OK;
    const wchar_t *mode_name = READ_MODE_NAME;
    int nchars = 0;
    bool shell = false;
    bool array = false;
    bool split_null = false;

    while (1) {
        static const struct woption long_options[] = {{L"export", no_argument, 0, 'x'},
                                                      {L"global", no_argument, 0, 'g'},
                                                      {L"local", no_argument, 0, 'l'},
                                                      {L"universal", no_argument, 0, 'U'},
                                                      {L"unexport", no_argument, 0, 'u'},
                                                      {L"prompt", required_argument, 0, 'p'},
                                                      {L"right-prompt", required_argument, 0, 'R'},
                                                      {L"command", required_argument, 0, 'c'},
                                                      {L"mode-name", required_argument, 0, 'm'},
                                                      {L"nchars", required_argument, 0, 'n'},
                                                      {L"shell", no_argument, 0, 's'},
                                                      {L"array", no_argument, 0, 'a'},
                                                      {L"null", no_argument, 0, 'z'},
                                                      {L"help", no_argument, 0, 'h'},
                                                      {0, 0, 0, 0}};

        int opt_index = 0;

        int opt = w.wgetopt_long(argc, argv, L"xglUup:R:c:hm:n:saz", long_options, &opt_index);
        if (opt == -1) break;

        switch (opt) {
            case 0: {
                if (long_options[opt_index].flag != 0) break;
                streams.err.append_format(BUILTIN_ERR_UNKNOWN, argv[0],
                                          long_options[opt_index].name);
                builtin_print_help(parser, streams, argv[0], streams.err);
                return STATUS_BUILTIN_ERROR;
            }
            case L'x': {
                place |= ENV_EXPORT;
                break;
            }
            case L'g': {
                place |= ENV_GLOBAL;
                break;
            }
            case L'l': {
                place |= ENV_LOCAL;
                break;
            }
            case L'U': {
                place |= ENV_UNIVERSAL;
                break;
            }
            case L'u': {
                place |= ENV_UNEXPORT;
                break;
            }
            case L'p': {
                prompt = w.woptarg;
                break;
            }
            case L'R': {
                right_prompt = w.woptarg;
                break;
            }
            case L'c': {
                commandline = w.woptarg;
                break;
            }
            case L'm': {
                mode_name = w.woptarg;
                break;
            }
            case L'n': {
                nchars = fish_wcstoi(w.woptarg);
                if (errno) {
                    if (errno == ERANGE) {
                        streams.err.append_format(_(L"%ls: Argument '%ls' is out of range\n"),
                                                  argv[0], w.woptarg);
                        builtin_print_help(parser, streams, argv[0], streams.err);
                        return STATUS_BUILTIN_ERROR;
                    }

                    streams.err.append_format(_(L"%ls: Argument '%ls' must be an integer\n"),
                                              argv[0], w.woptarg);
                    builtin_print_help(parser, streams, argv[0], streams.err);
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 's': {
                shell = true;
                break;
            }
            case 'a': {
                array = true;
                break;
            }
            case L'z': {
                split_null = true;
                break;
            }
            case 'h': {
                builtin_print_help(parser, streams, argv[0], streams.out);
                return STATUS_BUILTIN_OK;
            }
            case L'?': {
                builtin_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return STATUS_BUILTIN_ERROR;
            }
            default: {
                DIE("unexpected opt");
                break;
            }
        }
    }

    if ((place & ENV_UNEXPORT) && (place & ENV_EXPORT)) {
        streams.err.append_format(BUILTIN_ERR_EXPUNEXP, argv[0]);

        builtin_print_help(parser, streams, argv[0], streams.err);
        return STATUS_BUILTIN_ERROR;
    }

    if ((place & ENV_LOCAL ? 1 : 0) + (place & ENV_GLOBAL ? 1 : 0) +
            (place & ENV_UNIVERSAL ? 1 : 0) >
        1) {
        streams.err.append_format(BUILTIN_ERR_GLOCAL, argv[0]);
        builtin_print_help(parser, streams, argv[0], streams.err);

        return STATUS_BUILTIN_ERROR;
    }

    if (array && w.woptind + 1 != argc) {
        streams.err.append_format(_(L"%ls: --array option requires a single variable name.\n"),
                                  argv[0]);
        builtin_print_help(parser, streams, argv[0], streams.err);

        return STATUS_BUILTIN_ERROR;
    }

    // Verify all variable names.
    wcstring errstr;
    for (int i = w.woptind; i < argc; i++) {
        if (!builtin_is_valid_varname(argv[i], errstr, argv[0])) {
            streams.err.append(errstr);
            builtin_print_help(parser, streams, argv[0], streams.err);
            return STATUS_BUILTIN_ERROR;
        }
    }

    // TODO: Determine if the original set of conditions for interactive reads should be reinstated:
    // if (isatty(0) && streams.stdin_fd == STDIN_FILENO && !split_null) {
    int stream_stdin_is_a_tty = isatty(streams.stdin_fd);
    if (stream_stdin_is_a_tty && !split_null) {
        // We should read interactively using reader_readline(). This does not support splitting on
        // null. The call to reader_readline may change woptind, so we save and restore it.
        int saved_woptind = w.woptind;
        exit_res =
            read_interactive(buff, nchars, shell, mode_name, prompt, right_prompt, commandline);
        w.woptind = saved_woptind;
    } else if (!nchars && !stream_stdin_is_a_tty && lseek(streams.stdin_fd, 0, SEEK_CUR) != -1) {
        exit_res = read_in_chunks(streams.stdin_fd, buff, split_null);
    } else {
        exit_res = read_one_char_at_a_time(streams.stdin_fd, buff, nchars, split_null);
    }

    if (w.woptind == argc || exit_res != STATUS_BUILTIN_OK) {
        return exit_res;
    }

    env_var_t ifs = env_get_string(L"IFS");
    if (ifs.missing_or_empty()) {
        // Every character is a separate token.
        size_t bufflen = buff.size();
        if (array) {
            if (bufflen > 0) {
                wcstring chars(bufflen + (bufflen - 1), ARRAY_SEP);
                wcstring::iterator out = chars.begin();
                for (wcstring::const_iterator it = buff.begin(), end = buff.end(); it != end;
                     ++it) {
                    *out = *it;
                    out += 2;
                }
                env_set(argv[w.woptind], chars.c_str(), place);
            } else {
                env_set(argv[w.woptind], NULL, place);
            }
        } else {  // not array
            size_t j = 0;
            for (; w.woptind + 1 < argc; ++w.woptind) {
                if (j < bufflen) {
                    wchar_t buffer[2] = {buff[j++], 0};
                    env_set(argv[w.woptind], buffer, place);
                } else {
                    env_set(argv[w.woptind], L"", place);
                }
            }
            if (w.woptind < argc) env_set(argv[w.woptind], &buff[j], place);
        }
    } else if (array) {
        wcstring tokens;
        tokens.reserve(buff.size());
        bool empty = true;

        for (wcstring_range loc = wcstring_tok(buff, ifs); loc.first != wcstring::npos;
             loc = wcstring_tok(buff, ifs, loc)) {
            if (!empty) tokens.push_back(ARRAY_SEP);
            tokens.append(buff, loc.first, loc.second);
            empty = false;
        }
        env_set(argv[w.woptind], empty ? NULL : tokens.c_str(), place);
    } else {  // not array
        wcstring_range loc = wcstring_range(0, 0);

        while (w.woptind < argc) {
            loc = wcstring_tok(buff, (w.woptind + 1 < argc) ? ifs : wcstring(), loc);
            env_set(argv[w.woptind], loc.first == wcstring::npos ? L"" : &buff.c_str()[loc.first],
                    place);
            ++w.woptind;
        }
    }

    return exit_res;
}

enum status_cmd_t {
    STATUS_IS_LOGIN = 1,
    STATUS_IS_INTERACTIVE,
    STATUS_IS_BLOCK,
    STATUS_IS_COMMAND_SUB,
    STATUS_IS_FULL_JOB_CTRL,
    STATUS_IS_INTERACTIVE_JOB_CTRL,
    STATUS_IS_NO_JOB_CTRL,
    STATUS_CURRENT_FILENAME,
    STATUS_CURRENT_LINE_NUMBER,
    STATUS_SET_JOB_CONTROL,
    STATUS_PRINT_STACK_TRACE,
    STATUS_UNDEF
};
// Must be sorted by string, not enum or random.
const enum_map<status_cmd_t> status_enum_map[] = {
    {STATUS_CURRENT_FILENAME, L"current-filename"},
    {STATUS_CURRENT_LINE_NUMBER, L"current-line-number"},
    {STATUS_IS_BLOCK, L"is-block"},
    {STATUS_IS_COMMAND_SUB, L"is-command-substitution"},
    {STATUS_IS_FULL_JOB_CTRL, L"is-full-job-control"},
    {STATUS_IS_INTERACTIVE, L"is-interactive"},
    {STATUS_IS_INTERACTIVE_JOB_CTRL, L"is-interactive-job-control"},
    {STATUS_IS_LOGIN, L"is-login"},
    {STATUS_IS_NO_JOB_CTRL, L"is-no-job-control"},
    {STATUS_SET_JOB_CONTROL, L"job-control"},
    {STATUS_PRINT_STACK_TRACE, L"print-stack-trace"},
    {STATUS_UNDEF, NULL}};
#define status_enum_map_len (sizeof status_enum_map / sizeof *status_enum_map)

/// Remember the status subcommand and disallow selecting more than one status subcommand.
static bool set_status_cmd(wchar_t *const cmd, status_cmd_t *status_cmd, status_cmd_t sub_cmd,
                           io_streams_t &streams) {
    if (*status_cmd != STATUS_UNDEF) {
        wchar_t err_text[1024];
        const wchar_t *subcmd_str1 = enum_to_str(*status_cmd, status_enum_map);
        const wchar_t *subcmd_str2 = enum_to_str(sub_cmd, status_enum_map);
        swprintf(err_text, sizeof(err_text) / sizeof(wchar_t),
                 _(L"you cannot do both '%ls' and '%ls' in the same invocation"), subcmd_str1,
                 subcmd_str2);
        streams.err.append_format(BUILTIN_ERR_COMBO2, cmd, err_text);
        return false;
    }

    *status_cmd = sub_cmd;
    return true;
}

#define CHECK_FOR_UNEXPECTED_STATUS_ARGS(status_cmd)                                        \
    if (args.size() != 0) {                                                                 \
        const wchar_t *subcmd_str = enum_to_str(status_cmd, status_enum_map);               \
        if (!subcmd_str) subcmd_str = L"default";                                           \
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT2, cmd, subcmd_str, 0, args.size()); \
        status = STATUS_BUILTIN_ERROR;                                                      \
        break;                                                                              \
    }

int job_control_str_to_mode(const wchar_t *mode, wchar_t *cmd, io_streams_t &streams) {
    if (wcscmp(mode, L"full") == 0) {
        return JOB_CONTROL_ALL;
    } else if (wcscmp(mode, L"interactive") == 0) {
        return JOB_CONTROL_INTERACTIVE;
    } else if (wcscmp(mode, L"none") == 0) {
        return JOB_CONTROL_NONE;
    }
    streams.err.append_format(L"%ls: Invalid job control mode '%ls'\n", cmd, mode);
    return -1;
}

/// The status builtin. Gives various status information on fish.
static int builtin_status(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    status_cmd_t status_cmd = STATUS_UNDEF;
    int status = STATUS_BUILTIN_OK;
    int new_job_control_mode = -1;

    /// Note: Do not add new flags that represent subcommands. We're encouraging people to switch to
    /// the non-flag subcommand form. While these flags are deprecated they must be supported at
    /// least until fish 3.0 and possibly longer to avoid breaking everyones config.fish and other
    /// scripts.
    const wchar_t *short_options = L":cbilfnhj:t";
    const struct woption long_options[] = {{L"help", no_argument, 0, 'h'},
                                           {L"is-command-substitution", no_argument, 0, 'c'},
                                           {L"is-block", no_argument, 0, 'b'},
                                           {L"is-interactive", no_argument, 0, 'i'},
                                           {L"is-login", no_argument, 0, 'l'},
                                           {L"is-full-job-control", no_argument, 0, 1},
                                           {L"is-interactive-job-control", no_argument, 0, 2},
                                           {L"is-no-job-control", no_argument, 0, 3},
                                           {L"current-filename", no_argument, 0, 'f'},
                                           {L"current-line-number", no_argument, 0, 'n'},
                                           {L"job-control", required_argument, 0, 'j'},
                                           {L"print-stack-trace", no_argument, 0, 't'},
                                           {0, 0, 0, 0}};

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 1: {
                if (!set_status_cmd(cmd, &status_cmd, STATUS_IS_FULL_JOB_CTRL, streams)) {
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 2: {
                if (!set_status_cmd(cmd, &status_cmd, STATUS_IS_INTERACTIVE_JOB_CTRL, streams)) {
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 3: {
                if (!set_status_cmd(cmd, &status_cmd, STATUS_IS_NO_JOB_CTRL, streams)) {
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 'c': {
                if (!set_status_cmd(cmd, &status_cmd, STATUS_IS_COMMAND_SUB, streams)) {
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 'b': {
                if (!set_status_cmd(cmd, &status_cmd, STATUS_IS_BLOCK, streams)) {
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 'i': {
                if (!set_status_cmd(cmd, &status_cmd, STATUS_IS_INTERACTIVE, streams)) {
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 'l': {
                if (!set_status_cmd(cmd, &status_cmd, STATUS_IS_LOGIN, streams)) {
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 'f': {
                if (!set_status_cmd(cmd, &status_cmd, STATUS_CURRENT_FILENAME, streams)) {
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 'n': {
                if (!set_status_cmd(cmd, &status_cmd, STATUS_CURRENT_LINE_NUMBER, streams)) {
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 'j': {
                if (!set_status_cmd(cmd, &status_cmd, STATUS_SET_JOB_CONTROL, streams)) {
                    return STATUS_BUILTIN_ERROR;
                }
                new_job_control_mode = job_control_str_to_mode(w.woptarg, cmd, streams);
                if (new_job_control_mode == -1) {
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 't': {
                if (!set_status_cmd(cmd, &status_cmd, STATUS_PRINT_STACK_TRACE, streams)) {
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 'h': {
                builtin_print_help(parser, streams, argv[0], streams.out);
                return STATUS_BUILTIN_OK;
            }
            case ':': {
                builtin_missing_argument(parser, streams, argv[0], argv[w.woptind - 1]);
                return STATUS_BUILTIN_ERROR;
            }
            case '?': {
                builtin_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return STATUS_BUILTIN_ERROR;
            }
            default: {
                DIE("unexpected retval from wgetopt_long");
                break;
            }
        }
    }

    // If a status command hasn't already been specified via a flag check the first word.
    // Note that this can be simplified after we eliminate allowing subcommands as flags.
    if (w.woptind < argc) {
        status_cmd_t subcmd = str_to_enum(argv[w.woptind], status_enum_map, status_enum_map_len);
        if (subcmd != STATUS_UNDEF) {
            if (!set_status_cmd(cmd, &status_cmd, subcmd, streams)) {
                return STATUS_BUILTIN_ERROR;
            }
            w.woptind++;
        }
    }

    // Every argument that we haven't consumed already is an argument for a subcommand.
    const wcstring_list_t args(argv + w.woptind, argv + argc);

    switch (status_cmd) {
        case STATUS_UNDEF: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(status_cmd)
            if (is_login) {
                streams.out.append_format(_(L"This is a login shell\n"));
            } else {
                streams.out.append_format(_(L"This is not a login shell\n"));
            }

            streams.out.append_format(
                _(L"Job control: %ls\n"),
                job_control_mode == JOB_CONTROL_INTERACTIVE
                    ? _(L"Only on interactive jobs")
                    : (job_control_mode == JOB_CONTROL_NONE ? _(L"Never") : _(L"Always")));
            streams.out.append(parser.stack_trace());
            break;
        }
        case STATUS_SET_JOB_CONTROL: {
            if (new_job_control_mode != -1) {
                // Flag form was used.
                CHECK_FOR_UNEXPECTED_STATUS_ARGS(status_cmd)
            } else {
                if (args.size() != 1) {
                    const wchar_t *subcmd_str = enum_to_str(status_cmd, status_enum_map);
                    streams.err.append_format(BUILTIN_ERR_ARG_COUNT2, cmd, subcmd_str, 1,
                                              args.size());
                    status = STATUS_BUILTIN_ERROR;
                    break;
                }
                new_job_control_mode = job_control_str_to_mode(args[0].c_str(), cmd, streams);
                if (new_job_control_mode == -1) {
                    return STATUS_BUILTIN_ERROR;
                }
            }
            job_control_mode = new_job_control_mode;
            break;
        }
        case STATUS_CURRENT_FILENAME: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(status_cmd)
            const wchar_t *fn = parser.current_filename();

            if (!fn) fn = _(L"Standard input");
            streams.out.append_format(L"%ls\n", fn);
            break;
        }
        case STATUS_CURRENT_LINE_NUMBER: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(status_cmd)
            streams.out.append_format(L"%d\n", parser.get_lineno());
            break;
        }
        case STATUS_IS_INTERACTIVE: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(status_cmd)
            status = !is_interactive_session;
            break;
        }
        case STATUS_IS_COMMAND_SUB: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(status_cmd)
            status = !is_subshell;
            break;
        }
        case STATUS_IS_BLOCK: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(status_cmd)
            status = !is_block;
            break;
        }
        case STATUS_IS_LOGIN: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(status_cmd)
            status = !is_login;
            break;
        }
        case STATUS_IS_FULL_JOB_CTRL: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(status_cmd)
            status = job_control_mode != JOB_CONTROL_ALL;
            break;
        }
        case STATUS_IS_INTERACTIVE_JOB_CTRL: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(status_cmd)
            status = job_control_mode != JOB_CONTROL_INTERACTIVE;
            break;
        }
        case STATUS_IS_NO_JOB_CTRL: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(status_cmd)
            status = job_control_mode != JOB_CONTROL_NONE;
            break;
        }
        case STATUS_PRINT_STACK_TRACE: {
            CHECK_FOR_UNEXPECTED_STATUS_ARGS(status_cmd)
            streams.out.append(parser.stack_trace());
            break;
        }
    }

    return status;
}

/// The exit builtin. Calls reader_exit to exit and returns the value specified.
static int builtin_exit(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    int argc = builtin_count_args(argv);

    if (argc > 2) {
        streams.err.append_format(_(L"%ls: Too many arguments\n"), argv[0]);
        builtin_print_help(parser, streams, argv[0], streams.err);
        return STATUS_BUILTIN_ERROR;
    }

    long ec;
    if (argc == 1) {
        ec = proc_get_last_status();
    } else {
        ec = fish_wcstol(argv[1]);
        if (errno) {
            streams.err.append_format(_(L"%ls: Argument '%ls' must be an integer\n"), argv[0],
                                      argv[1]);
            builtin_print_help(parser, streams, argv[0], streams.err);
            return STATUS_BUILTIN_ERROR;
        }
    }
    reader_exit(1, 0);
    return (int)ec;
}

/// The cd builtin. Changes the current directory to the one specified or to $HOME if none is
/// specified. The directory can be relative to any directory in the CDPATH variable.
/// The cd builtin. Changes the current directory to the one specified or to $HOME if none is
/// specified. The directory can be relative to any directory in the CDPATH variable.
static int builtin_cd(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    env_var_t dir_in;
    wcstring dir;

    if (argv[1] == NULL) {
        dir_in = env_get_string(L"HOME");
        if (dir_in.missing_or_empty()) {
            streams.err.append_format(_(L"%ls: Could not find home directory\n"), argv[0]);
            return STATUS_BUILTIN_ERROR;
        }
    } else {
        dir_in = env_var_t(argv[1]);
    }

    bool got_cd_path = false;
    if (!dir_in.missing()) {
        got_cd_path = path_get_cdpath(dir_in, &dir);
    }

    if (!got_cd_path) {
        if (errno == ENOTDIR) {
            streams.err.append_format(_(L"%ls: '%ls' is not a directory\n"), argv[0],
                                      dir_in.c_str());
        } else if (errno == ENOENT) {
            streams.err.append_format(_(L"%ls: The directory '%ls' does not exist\n"), argv[0],
                                      dir_in.c_str());
        } else if (errno == EROTTEN) {
            streams.err.append_format(_(L"%ls: '%ls' is a rotten symlink\n"), argv[0],
                                      dir_in.c_str());
        } else {
            streams.err.append_format(_(L"%ls: Unknown error trying to locate directory '%ls'\n"),
                                      argv[0], dir_in.c_str());
        }

        if (!shell_is_interactive()) streams.err.append(parser.current_line());

        return STATUS_BUILTIN_ERROR;
    }

    if (wchdir(dir) != 0) {
        struct stat buffer;
        int status;

        status = wstat(dir, &buffer);
        if (!status && S_ISDIR(buffer.st_mode)) {
            streams.err.append_format(_(L"%ls: Permission denied: '%ls'\n"), argv[0], dir.c_str());

        } else {
            streams.err.append_format(_(L"%ls: '%ls' is not a directory\n"), argv[0], dir.c_str());
        }

        if (!shell_is_interactive()) {
            streams.err.append(parser.current_line());
        }

        return STATUS_BUILTIN_ERROR;
    }

    if (!env_set_pwd()) {
        streams.err.append_format(_(L"%ls: Could not set PWD variable\n"), argv[0]);
        return STATUS_BUILTIN_ERROR;
    }

    return STATUS_BUILTIN_OK;
}

/// Implementation of the builtin count command, used to count the number of arguments sent to it.
static int builtin_count(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    int argc;
    argc = builtin_count_args(argv);
    streams.out.append_format(L"%d\n", argc - 1);
    return !(argc - 1);
}

/// Implementation of the builtin contains command, used to check if a specified string is part of
/// a list.
static int builtin_contains(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wgetopter_t w;
    int argc;
    argc = builtin_count_args(argv);
    wchar_t *needle;
    bool should_output_index = false;

    const struct woption long_options[] = {
        {L"help", no_argument, 0, 'h'}, {L"index", no_argument, 0, 'i'}, {0, 0, 0, 0}};

    while (1) {
        int opt_index = 0;

        int opt = w.wgetopt_long(argc, argv, L"+hi", long_options, &opt_index);
        if (opt == -1) break;

        switch (opt) {
            case 0: {
                assert(opt_index >= 0 &&
                       (size_t)opt_index < sizeof long_options / sizeof *long_options);
                if (long_options[opt_index].flag != 0) break;
                streams.err.append_format(BUILTIN_ERR_UNKNOWN, argv[0],
                                          long_options[opt_index].name);
                builtin_print_help(parser, streams, argv[0], streams.err);
                return STATUS_BUILTIN_ERROR;
            }
            case 'h': {
                builtin_print_help(parser, streams, argv[0], streams.out);
                return STATUS_BUILTIN_OK;
            }
            case ':': {
                builtin_missing_argument(parser, streams, argv[0], argv[w.woptind - 1]);
                return STATUS_BUILTIN_ERROR;
            }
            case '?': {
                builtin_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return STATUS_BUILTIN_ERROR;
            }
            case 'i': {
                should_output_index = true;
                break;
            }
            default: {
                DIE("unexpected opt");
                break;
            }
        }
    }

    needle = argv[w.woptind];
    if (!needle) {
        streams.err.append_format(_(L"%ls: Key not specified\n"), argv[0]);
    } else {
        for (int i = w.woptind + 1; i < argc; i++) {
            if (!wcscmp(needle, argv[i])) {
                if (should_output_index) streams.out.append_format(L"%d\n", i - w.woptind);
                return 0;
            }
        }
    }
    return 1;
}

/// The  . (dot) builtin, sometimes called source. Evaluates the contents of a file.
static int builtin_source(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    ASSERT_IS_MAIN_THREAD();
    int fd;
    int res = STATUS_BUILTIN_OK;
    struct stat buf;
    int argc;

    argc = builtin_count_args(argv);

    const wchar_t *fn, *fn_intern;

    if (argc < 2 || (wcscmp(argv[1], L"-") == 0)) {
        fn = L"-";
        fn_intern = fn;
        fd = dup(streams.stdin_fd);
    } else {
        if ((fd = wopen_cloexec(argv[1], O_RDONLY)) == -1) {
            streams.err.append_format(_(L"%ls: Error encountered while sourcing file '%ls':\n"),
                                      argv[0], argv[1]);
            builtin_wperror(L"source", streams);
            return STATUS_BUILTIN_ERROR;
        }

        if (fstat(fd, &buf) == -1) {
            close(fd);
            streams.err.append_format(_(L"%ls: Error encountered while sourcing file '%ls':\n"),
                                      argv[0], argv[1]);
            builtin_wperror(L"source", streams);
            return STATUS_BUILTIN_ERROR;
        }

        if (!S_ISREG(buf.st_mode)) {
            close(fd);
            streams.err.append_format(_(L"%ls: '%ls' is not a file\n"), argv[0], argv[1]);
            return STATUS_BUILTIN_ERROR;
        }

        fn_intern = intern(argv[1]);
    }

    const source_block_t *sb = parser.push_block<source_block_t>(fn_intern);
    reader_push_current_filename(fn_intern);

    env_set_argv(argc > 1 ? argv + 2 : argv + 1);

    res = reader_read(fd, streams.io_chain ? *streams.io_chain : io_chain_t());

    parser.pop_block(sb);

    if (res) {
        streams.err.append_format(_(L"%ls: Error while reading file '%ls'\n"), argv[0],
                                  fn_intern == intern_static(L"-") ? L"<stdin>" : fn_intern);
    } else {
        res = proc_get_last_status();
    }

    // Do not close fd after calling reader_read. reader_read automatically closes it before calling
    // eval.
    reader_pop_current_filename();

    return res;
}

/// Builtin for putting a job in the foreground.
static int builtin_fg(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    job_t *j = NULL;

    if (argv[1] == 0) {
        // Select last constructed job (I.e. first job in the job que) that is possible to put in
        // the foreground.
        job_iterator_t jobs;
        while ((j = jobs.next())) {
            if (j->get_flag(JOB_CONSTRUCTED) && (!job_is_completed(j)) &&
                ((job_is_stopped(j) || (!j->get_flag(JOB_FOREGROUND))) &&
                 j->get_flag(JOB_CONTROL))) {
                break;
            }
        }
        if (!j) {
            streams.err.append_format(_(L"%ls: There are no suitable jobs\n"), argv[0]);
        }
    } else if (argv[2] != 0) {
        // Specifying more than one job to put to the foreground is a syntax error, we still
        // try to locate the job argv[1], since we want to know if this is an ambigous job
        // specification or if this is an malformed job id.
        int pid;
        int found_job = 0;

        pid = fish_wcstoi(argv[1]);
        if (!(errno || pid < 0)) {
            j = job_get_from_pid(pid);
            if (j) found_job = 1;
        }

        if (found_job) {
            streams.err.append_format(_(L"%ls: Ambiguous job\n"), argv[0]);
        } else {
            streams.err.append_format(_(L"%ls: '%ls' is not a job\n"), argv[0], argv[1]);
        }

        builtin_print_help(parser, streams, argv[0], streams.err);

        j = 0;

    } else {
        int pid = abs(fish_wcstoi(argv[1]));
        if (errno) {
            streams.err.append_format(BUILTIN_ERR_NOT_NUMBER, argv[0], argv[1]);
            builtin_print_help(parser, streams, argv[0], streams.err);
        } else {
            j = job_get_from_pid(pid);
            if (!j || !j->get_flag(JOB_CONSTRUCTED) || job_is_completed(j)) {
                streams.err.append_format(_(L"%ls: No suitable job: %d\n"), argv[0], pid);
                j = 0;
            } else if (!j->get_flag(JOB_CONTROL)) {
                streams.err.append_format(_(L"%ls: Can't put job %d, '%ls' to foreground because "
                                            L"it is not under job control\n"),
                                          argv[0], pid, j->command_wcstr());
                j = 0;
            }
        }
    }

    if (!j) {
        return STATUS_BUILTIN_ERROR;
    }

    if (streams.err_is_redirected) {
        streams.err.append_format(FG_MSG, j->job_id, j->command_wcstr());
    } else {
        // If we aren't redirecting, send output to real stderr, since stuff in sb_err won't get
        // printed until the command finishes.
        fwprintf(stderr, FG_MSG, j->job_id, j->command_wcstr());
    }

    const wcstring ft = tok_first(j->command());
    if (!ft.empty()) env_set(L"_", ft.c_str(), ENV_EXPORT);
    reader_write_title(j->command());

    job_promote(j);
    j->set_flag(JOB_FOREGROUND, true);

    job_continue(j, job_is_stopped(j));
    return STATUS_BUILTIN_OK;
}

/// Helper function for builtin_bg().
static int send_to_bg(parser_t &parser, io_streams_t &streams, job_t *j, const wchar_t *name) {
    if (j == 0) {
        streams.err.append_format(_(L"%ls: Unknown job '%ls'\n"), L"bg", name);
        builtin_print_help(parser, streams, L"bg", streams.err);
        return STATUS_BUILTIN_ERROR;
    } else if (!j->get_flag(JOB_CONTROL)) {
        streams.err.append_format(
            _(L"%ls: Can't put job %d, '%ls' to background because it is not under job control\n"),
            L"bg", j->job_id, j->command_wcstr());
        builtin_print_help(parser, streams, L"bg", streams.err);
        return STATUS_BUILTIN_ERROR;
    }

    streams.err.append_format(_(L"Send job %d '%ls' to background\n"), j->job_id,
                              j->command_wcstr());
    job_promote(j);
    j->set_flag(JOB_FOREGROUND, false);
    job_continue(j, job_is_stopped(j));
    return STATUS_BUILTIN_OK;
}

/// Builtin for putting a job in the background.
static int builtin_bg(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    int res = STATUS_BUILTIN_OK;

    if (argv[1] == 0) {
        job_t *j;
        job_iterator_t jobs;
        while ((j = jobs.next())) {
            if (job_is_stopped(j) && j->get_flag(JOB_CONTROL) && (!job_is_completed(j))) {
                break;
            }
        }

        if (!j) {
            streams.err.append_format(_(L"%ls: There are no suitable jobs\n"), argv[0]);
            res = STATUS_BUILTIN_ERROR;
        } else {
            res = send_to_bg(parser, streams, j, _(L"(default)"));
        }
    } else {
        int i;
        int pid;

        for (i = 1; argv[i]; i++) {
            pid = fish_wcstoi(argv[i]);
            if (errno || pid < 0 || !job_get_from_pid(pid)) {
                streams.err.append_format(_(L"%ls: '%ls' is not a job\n"), argv[0], argv[i]);
                return STATUS_BUILTIN_ERROR;
            }
            res |= send_to_bg(parser, streams, job_get_from_pid(pid), *argv);
        }
    }

    return res;
}

/// This function handles both the 'continue' and the 'break' builtins that are used for loop
/// control.
static int builtin_break_continue(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    int is_break = (wcscmp(argv[0], L"break") == 0);
    int argc = builtin_count_args(argv);

    if (argc != 1) {
        streams.err.append_format(BUILTIN_ERR_UNKNOWN, argv[0], argv[1]);

        builtin_print_help(parser, streams, argv[0], streams.err);
        return STATUS_BUILTIN_ERROR;
    }

    // Find the index of the enclosing for or while loop. Recall that incrementing loop_idx goes
    // 'up' to outer blocks.
    size_t loop_idx;
    for (loop_idx = 0; loop_idx < parser.block_count(); loop_idx++) {
        const block_t *b = parser.block_at_index(loop_idx);
        if (b->type() == WHILE || b->type() == FOR) break;
    }

    if (loop_idx >= parser.block_count()) {
        streams.err.append_format(_(L"%ls: Not inside of loop\n"), argv[0]);
        builtin_print_help(parser, streams, argv[0], streams.err);
        return STATUS_BUILTIN_ERROR;
    }

    // Skip blocks interior to the loop (but not the loop itself)
    size_t block_idx = loop_idx;
    while (block_idx--) {
        parser.block_at_index(block_idx)->skip = true;
    }

    // Mark the loop's status
    block_t *loop_block = parser.block_at_index(loop_idx);
    loop_block->loop_status = is_break ? LOOP_BREAK : LOOP_CONTINUE;
    return STATUS_BUILTIN_OK;
}

/// Implementation of the builtin breakpoint command, used to launch the interactive debugger.
static int builtin_breakpoint(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    if (argv[1] != NULL) {
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT1, argv[0], 0, builtin_count_args(argv));
        return STATUS_BUILTIN_ERROR;
    }

    const breakpoint_block_t *bpb = parser.push_block<breakpoint_block_t>();

    reader_read(STDIN_FILENO, streams.io_chain ? *streams.io_chain : io_chain_t());

    parser.pop_block(bpb);

    return proc_get_last_status();
}

/// Function for handling the \c return builtin.
static int builtin_return(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    int argc = builtin_count_args(argv);

    if (argc > 2) {
        streams.err.append_format(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        builtin_print_help(parser, streams, argv[0], streams.err);
        return STATUS_BUILTIN_ERROR;
    }

    int status;
    if (argc == 2) {
        status = fish_wcstoi(argv[1]);
        if (errno) {
            streams.err.append_format(_(L"%ls: Argument '%ls' must be an integer\n"), argv[0],
                                      argv[1]);
            builtin_print_help(parser, streams, argv[0], streams.err);
            return STATUS_BUILTIN_ERROR;
        }
    } else {
        status = proc_get_last_status();
    }

    // Find the function block.
    size_t function_block_idx;
    for (function_block_idx = 0; function_block_idx < parser.block_count(); function_block_idx++) {
        const block_t *b = parser.block_at_index(function_block_idx);
        if (b->type() == FUNCTION_CALL || b->type() == FUNCTION_CALL_NO_SHADOW) break;
    }

    if (function_block_idx >= parser.block_count()) {
        streams.err.append_format(_(L"%ls: Not inside of function\n"), argv[0]);
        builtin_print_help(parser, streams, argv[0], streams.err);
        return STATUS_BUILTIN_ERROR;
    }

    // Skip everything up to and including the function block.
    for (size_t i = 0; i <= function_block_idx; i++) {
        block_t *b = parser.block_at_index(i);
        b->skip = true;
    }
    return status;
}

enum hist_cmd_t { HIST_SEARCH = 1, HIST_DELETE, HIST_CLEAR, HIST_MERGE, HIST_SAVE, HIST_UNDEF };
// Must be sorted by string, not enum or random.
const enum_map<hist_cmd_t> hist_enum_map[] = {{HIST_CLEAR, L"clear"},   {HIST_DELETE, L"delete"},
                                              {HIST_MERGE, L"merge"},   {HIST_SAVE, L"save"},
                                              {HIST_SEARCH, L"search"}, {HIST_UNDEF, NULL}};
#define hist_enum_map_len (sizeof hist_enum_map / sizeof *hist_enum_map)

/// Remember the history subcommand and disallow selecting more than one history subcommand.
static bool set_hist_cmd(wchar_t *const cmd, hist_cmd_t *hist_cmd, hist_cmd_t sub_cmd,
                         io_streams_t &streams) {
    if (*hist_cmd != HIST_UNDEF) {
        wchar_t err_text[1024];
        const wchar_t *subcmd_str1 = enum_to_str(*hist_cmd, hist_enum_map);
        const wchar_t *subcmd_str2 = enum_to_str(sub_cmd, hist_enum_map);
        swprintf(err_text, sizeof(err_text) / sizeof(wchar_t),
                 _(L"you cannot do both '%ls' and '%ls' in the same invocation"), subcmd_str1,
                 subcmd_str2);
        streams.err.append_format(BUILTIN_ERR_COMBO2, cmd, err_text);
        return false;
    }

    *hist_cmd = sub_cmd;
    return true;
}

#define CHECK_FOR_UNEXPECTED_HIST_ARGS(hist_cmd)                                                \
    if (history_search_type_defined || show_time_format || null_terminate) {                    \
        const wchar_t *subcmd_str = enum_to_str(hist_cmd, hist_enum_map);                       \
        streams.err.append_format(_(L"%ls: you cannot use any options with the %ls command\n"), \
                                  cmd, subcmd_str);                                             \
        status = STATUS_BUILTIN_ERROR;                                                          \
        break;                                                                                  \
    }                                                                                           \
    if (args.size() != 0) {                                                                     \
        const wchar_t *subcmd_str = enum_to_str(hist_cmd, hist_enum_map);                       \
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT2, cmd, subcmd_str, 0, args.size());     \
        status = STATUS_BUILTIN_ERROR;                                                          \
        break;                                                                                  \
    }

/// Manipulate history of interactive commands executed by the user.
static int builtin_history(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    hist_cmd_t hist_cmd = HIST_UNDEF;
    history_search_type_t search_type = (history_search_type_t)-1;
    long max_items = LONG_MAX;
    bool history_search_type_defined = false;
    const wchar_t *show_time_format = NULL;
    bool case_sensitive = false;
    bool null_terminate = false;

    /// Note: Do not add new flags that represent subcommands. We're encouraging people to switch to
    /// the non-flag subcommand form. While many of these flags are deprecated they must be
    /// supported at least until fish 3.0 and possibly longer to avoid breaking everyones
    /// config.fish and other scripts.
    const wchar_t *short_options = L":Cmn:epchtz";
    const struct woption long_options[] = {{L"prefix", no_argument, NULL, 'p'},
                                           {L"contains", no_argument, NULL, 'c'},
                                           {L"help", no_argument, NULL, 'h'},
                                           {L"show-time", optional_argument, NULL, 't'},
                                           {L"with-time", optional_argument, NULL, 't'},
                                           {L"exact", no_argument, NULL, 'e'},
                                           {L"max", required_argument, NULL, 'n'},
                                           {L"null", no_argument, 0, 'z'},
                                           {L"case-sensitive", no_argument, 0, 'C'},
                                           {L"delete", no_argument, NULL, 1},
                                           {L"search", no_argument, NULL, 2},
                                           {L"save", no_argument, NULL, 3},
                                           {L"clear", no_argument, NULL, 4},
                                           {L"merge", no_argument, NULL, 5},
                                           {NULL, 0, NULL, 0}};

    history_t *history = reader_get_history();
    // Use the default history if we have none (which happens if invoked non-interactively, e.g.
    // from webconfig.py.
    if (!history) history = &history_t::history_with_name(L"fish");

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 1: {
                if (!set_hist_cmd(cmd, &hist_cmd, HIST_DELETE, streams)) {
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 2: {
                if (!set_hist_cmd(cmd, &hist_cmd, HIST_SEARCH, streams)) {
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 3: {
                if (!set_hist_cmd(cmd, &hist_cmd, HIST_SAVE, streams)) {
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 4: {
                if (!set_hist_cmd(cmd, &hist_cmd, HIST_CLEAR, streams)) {
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 5: {
                if (!set_hist_cmd(cmd, &hist_cmd, HIST_MERGE, streams)) {
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 'C': {
                case_sensitive = true;
                break;
            }
            case 'p': {
                search_type = HISTORY_SEARCH_TYPE_PREFIX;
                history_search_type_defined = true;
                break;
            }
            case 'c': {
                search_type = HISTORY_SEARCH_TYPE_CONTAINS;
                history_search_type_defined = true;
                break;
            }
            case 'e': {
                search_type = HISTORY_SEARCH_TYPE_EXACT;
                history_search_type_defined = true;
                break;
            }
            case 't': {
                show_time_format = w.woptarg ? w.woptarg : L"# %c%n";
                break;
            }
            case 'n': {
                max_items = fish_wcstol(w.woptarg);
                if (errno) {
                    streams.err.append_format(_(L"%ls: max value '%ls' is not a valid number\n"),
                                              argv[0], w.woptarg);
                    return STATUS_BUILTIN_ERROR;
                }
                break;
            }
            case 'z': {
                null_terminate = true;
                break;
            }
            case 'h': {
                builtin_print_help(parser, streams, cmd, streams.out);
                return STATUS_BUILTIN_OK;
            }
            case ':': {
                streams.err.append_format(BUILTIN_ERR_MISSING, cmd, argv[w.woptind - 1]);
                return STATUS_BUILTIN_ERROR;
            }
            case '?': {
                // Try to parse it as a number; e.g., "-123".
                max_items = fish_wcstol(argv[w.woptind - 1] + 1);
                if (errno) {
                    streams.err.append_format(BUILTIN_ERR_UNKNOWN, cmd, argv[w.woptind - 1]);
                    return STATUS_BUILTIN_ERROR;
                }
                w.nextchar = NULL;
                break;
            }
            default: {
                DIE("unexpected retval from wgetopt_long");
                break;
            }
        }
    }

    if (max_items <= 0) {
        streams.err.append_format(_(L"%ls: max value '%ls' is not a valid number\n"), argv[0],
                                  w.woptarg);
        return STATUS_BUILTIN_ERROR;
    }

    // If a history command hasn't already been specified via a flag check the first word.
    // Note that this can be simplified after we eliminate allowing subcommands as flags.
    // See the TODO above regarding the `long_options` array.
    if (w.woptind < argc) {
        hist_cmd_t subcmd = str_to_enum(argv[w.woptind], hist_enum_map, hist_enum_map_len);
        if (subcmd != HIST_UNDEF) {
            if (!set_hist_cmd(cmd, &hist_cmd, subcmd, streams)) {
                return STATUS_BUILTIN_ERROR;
            }
            w.woptind++;
        }
    }

    // Every argument that we haven't consumed already is an argument for a subcommand (e.g., a
    // search term).
    const wcstring_list_t args(argv + w.woptind, argv + argc);

    // Establish appropriate defaults.
    if (hist_cmd == HIST_UNDEF) hist_cmd = HIST_SEARCH;
    if (!history_search_type_defined) {
        if (hist_cmd == HIST_SEARCH) search_type = HISTORY_SEARCH_TYPE_CONTAINS;
        if (hist_cmd == HIST_DELETE) search_type = HISTORY_SEARCH_TYPE_EXACT;
    }

    int status = STATUS_BUILTIN_OK;
    switch (hist_cmd) {
        case HIST_SEARCH: {
            if (!history->search(search_type, args, show_time_format, max_items, case_sensitive,
                                 null_terminate, streams)) {
                status = STATUS_BUILTIN_ERROR;
            }
            break;
        }
        case HIST_DELETE: {
            // TODO: Move this code to the history module and support the other search types
            // including case-insensitive matches. At this time we expect the non-exact deletions to
            // be handled only by the history function's interactive delete feature.
            if (search_type != HISTORY_SEARCH_TYPE_EXACT) {
                streams.err.append_format(_(L"builtin history delete only supports --exact\n"));
                status = STATUS_BUILTIN_ERROR;
                break;
            }
            if (!case_sensitive) {
                streams.err.append_format(
                    _(L"builtin history delete only supports --case-sensitive\n"));
                status = STATUS_BUILTIN_ERROR;
                break;
            }
            for (wcstring_list_t::const_iterator iter = args.begin(); iter != args.end(); ++iter) {
                wcstring delete_string = *iter;
                if (delete_string[0] == '"' && delete_string[delete_string.length() - 1] == '"') {
                    delete_string = delete_string.substr(1, delete_string.length() - 2);
                }
                history->remove(delete_string);
            }
            break;
        }
        case HIST_CLEAR: {
            CHECK_FOR_UNEXPECTED_HIST_ARGS(hist_cmd)
            history->clear();
            history->save();
            break;
        }
        case HIST_MERGE: {
            CHECK_FOR_UNEXPECTED_HIST_ARGS(hist_cmd)
            history->incorporate_external_changes();
            break;
        }
        case HIST_SAVE: {
            CHECK_FOR_UNEXPECTED_HIST_ARGS(hist_cmd)
            history->save();
            break;
        }
        case HIST_UNDEF: {
            DIE("Unexpected HIST_UNDEF seen");
            break;
        }
    }

    return status;
}

#if 0
// Disabled for the 2.2.0 release: https://github.com/fish-shell/fish-shell/issues/1809.
int builtin_parse(parser_t &parser, io_streams_t &streams, wchar_t **argv)
{
    struct sigaction act;
    sigemptyset(& act.sa_mask);
    act.sa_flags=0;
    act.sa_handler=SIG_DFL;
    sigaction(SIGINT, &act, 0);

    std::vector<char> txt;
    for (;;)
    {
        char buff[256];
        ssize_t amt = read_loop(streams.stdin_fd, buff, sizeof buff);
        if (amt <= 0) break;
        txt.insert(txt.end(), buff, buff + amt);
    }
    if (! txt.empty())
    {
        const wcstring src = str2wcstring(&txt.at(0), txt.size());
        parse_node_tree_t parse_tree;
        parse_error_list_t errors;
        bool success = parse_tree_from_string(src, parse_flag_include_comments, &parse_tree,
                                              &errors);
        if (! success)
        {
            streams.out.append(L"Parsing failed:\n");
            for (size_t i=0; i < errors.size(); i++)
            {
                streams.out.append(errors.at(i).describe(src));
                streams.out.push_back(L'\n');
            }

            streams.out.append(L"(Reparsed with continue after error)\n");
            parse_tree.clear();
            errors.clear();
            parse_tree_from_string(src, parse_flag_continue_after_error |
                                   parse_flag_include_comments, &parse_tree, &errors);
        }
        const wcstring dump = parse_dump_tree(parse_tree, src);
        streams.out.append(dump);
    }
    return STATUS_BUILTIN_OK;
}
#endif

int builtin_true(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    UNUSED(streams);
    if (argv[1] != NULL) {
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT1, argv[0], 0, builtin_count_args(argv));
        return STATUS_BUILTIN_ERROR;
    }
    return STATUS_BUILTIN_OK;
}

int builtin_false(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    UNUSED(streams);
    if (argv[1] != NULL) {
        streams.err.append_format(BUILTIN_ERR_ARG_COUNT1, argv[0], 0, builtin_count_args(argv));
        return STATUS_BUILTIN_ERROR;
    }
    return STATUS_BUILTIN_ERROR;
}

/// An implementation of the external realpath command that doesn't support any options. It's meant
/// to be used only by scripts which need to be portable. In general scripts shouldn't invoke this
/// directly. They should just use `realpath` which will fallback to this builtin if an external
/// command cannot be found. This behaves like the external `realpath --canonicalize-existing`;
/// that is, it requires all path components, including the final, to exist.
int builtin_realpath(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    int argc = builtin_count_args(argv);

    if (argc != 2) {
        builtin_print_help(parser, streams, argv[0], streams.out);
        return STATUS_BUILTIN_ERROR;
    }

    wchar_t *real_path = wrealpath(argv[1], NULL);
    if (real_path) {
        streams.out.append(real_path);
        free((void *)real_path);
    } else {
        // We don't actually know why it failed. We should check errno.
        streams.err.append_format(_(L"%ls: Invalid path: %ls\n"), argv[0], argv[1]);
        return STATUS_BUILTIN_ERROR;
    }
    streams.out.append(L"\n");
    return STATUS_BUILTIN_OK;
}

// END OF BUILTIN COMMANDS
// Below are functions for handling the builtin commands.
// THESE MUST BE SORTED BY NAME! Completion lookup uses binary search.

// Data about all the builtin commands in fish.
// Functions that are bound to builtin_generic are handled directly by the parser.
// NOTE: These must be kept in sorted order!
static const builtin_data_t builtin_datas[] = {
    {L"[", &builtin_test, N_(L"Test a condition")},
#if 0
    // Disabled for the 2.2.0 release: https://github.com/fish-shell/fish-shell/issues/1809.
    {       L"__fish_parse",  &builtin_parse, N_(L"Try out the new parser")  },
#endif
    {L"and", &builtin_generic, N_(L"Execute command if previous command suceeded")},
    {L"begin", &builtin_generic, N_(L"Create a block of code")},
    {L"bg", &builtin_bg, N_(L"Send job to background")},
    {L"bind", &builtin_bind, N_(L"Handle fish key bindings")},
    {L"block", &builtin_block, N_(L"Temporarily block delivery of events")},
    {L"break", &builtin_break_continue, N_(L"Stop the innermost loop")},
    {L"breakpoint", &builtin_breakpoint,
     N_(L"Temporarily halt execution of a script and launch an interactive debug prompt")},
    {L"builtin", &builtin_builtin, N_(L"Run a builtin command instead of a function")},
    {L"case", &builtin_generic, N_(L"Conditionally execute a block of commands")},
    {L"cd", &builtin_cd, N_(L"Change working directory")},
    {L"command", &builtin_command, N_(L"Run a program instead of a function or builtin")},
    {L"commandline", &builtin_commandline, N_(L"Set or get the commandline")},
    {L"complete", &builtin_complete, N_(L"Edit command specific completions")},
    {L"contains", &builtin_contains, N_(L"Search for a specified string in a list")},
    {L"continue", &builtin_break_continue,
     N_(L"Skip the rest of the current lap of the innermost loop")},
    {L"count", &builtin_count, N_(L"Count the number of arguments")},
    {L"echo", &builtin_echo, N_(L"Print arguments")},
    {L"else", &builtin_generic, N_(L"Evaluate block if condition is false")},
    {L"emit", &builtin_emit, N_(L"Emit an event")},
    {L"end", &builtin_generic, N_(L"End a block of commands")},
    {L"exec", &builtin_generic, N_(L"Run command in current process")},
    {L"exit", &builtin_exit, N_(L"Exit the shell")},
    {L"false", &builtin_false, N_(L"Return an unsuccessful result")},
    {L"fg", &builtin_fg, N_(L"Send job to foreground")},
    {L"for", &builtin_generic, N_(L"Perform a set of commands multiple times")},
    {L"function", &builtin_generic, N_(L"Define a new function")},
    {L"functions", &builtin_functions, N_(L"List or remove functions")},
    {L"history", &builtin_history, N_(L"History of commands executed by user")},
    {L"if", &builtin_generic, N_(L"Evaluate block if condition is true")},
    {L"jobs", &builtin_jobs, N_(L"Print currently running jobs")},
    {L"not", &builtin_generic, N_(L"Negate exit status of job")},
    {L"or", &builtin_generic, N_(L"Execute command if previous command failed")},
    {L"printf", &builtin_printf, N_(L"Prints formatted text")},
    {L"pwd", &builtin_pwd, N_(L"Print the working directory")},
    {L"random", &builtin_random, N_(L"Generate random number")},
    {L"read", &builtin_read, N_(L"Read a line of input into variables")},
    {L"realpath", &builtin_realpath, N_(L"Convert path to absolute path without symlinks")},
    {L"return", &builtin_return, N_(L"Stop the currently evaluated function")},
    {L"set", &builtin_set, N_(L"Handle environment variables")},
    {L"set_color", &builtin_set_color, N_(L"Set the terminal color")},
    {L"source", &builtin_source, N_(L"Evaluate contents of file")},
    {L"status", &builtin_status, N_(L"Return status information about fish")},
    {L"string", &builtin_string, N_(L"Manipulate strings")},
    {L"switch", &builtin_generic, N_(L"Conditionally execute a block of commands")},
    {L"test", &builtin_test, N_(L"Test a condition")},
    {L"true", &builtin_true, N_(L"Return a successful result")},
    {L"ulimit", &builtin_ulimit, N_(L"Set or get the shells resource usage limits")},
    {L"while", &builtin_generic, N_(L"Perform a command multiple times")}};

#define BUILTIN_COUNT (sizeof builtin_datas / sizeof *builtin_datas)

/// Look up a builtin_data_t for a specified builtin
///
/// @param  name
///    Name of the builtin
///
/// @return
///    Pointer to a builtin_data_t
///
static const builtin_data_t *builtin_lookup(const wcstring &name) {
    const builtin_data_t *array_end = builtin_datas + BUILTIN_COUNT;
    const builtin_data_t *found = std::lower_bound(builtin_datas, array_end, name);
    if (found != array_end && name == found->name) {
        return found;
    }
    return NULL;
}

/// Initialize builtin data.
void builtin_init() {
    for (size_t i = 0; i < BUILTIN_COUNT; i++) {
        intern_static(builtin_datas[i].name);
    }
}

/// Destroy builtin data.
void builtin_destroy() {}

/// Is there a builtin command with the given name?
bool builtin_exists(const wcstring &cmd) { return static_cast<bool>(builtin_lookup(cmd)); }

/// If builtin takes care of printing help itself
static bool builtin_handles_help(const wchar_t *cmd) {
    CHECK(cmd, 0);
    return contains(cmd, L"for", L"while", L"function", L"if", L"end", L"switch", L"case", L"count",
                    L"printf");
}

/// Execute a builtin command
int builtin_run(parser_t &parser, const wchar_t *const *argv, io_streams_t &streams) {
    UNUSED(parser);
    UNUSED(streams);
    if (argv == NULL || argv[0] == NULL) return STATUS_BUILTIN_ERROR;

    const builtin_data_t *data = builtin_lookup(argv[0]);
    if (argv[1] != NULL && !builtin_handles_help(argv[0]) && argv[2] == NULL &&
        parse_util_argument_is_help(argv[1], 0)) {
        builtin_print_help(parser, streams, argv[0], streams.out);
        return STATUS_BUILTIN_OK;
    }

    if (data != NULL) {
        // Warning: layering violation and naughty cast. The code originally had a much more
        // complicated solution to achieve exactly the same result: lie about the constness of argv.
        // Some of the builtins we call do mutate the array via their calls to wgetopt() which could
        // result in the pointers being reordered. This is harmless because we only get called once
        // with a given argv array and nothing else will look at the contents of the array after we
        // return.
        return data->func(parser, streams, (wchar_t **)argv);
    }

    debug(0, UNKNOWN_BUILTIN_ERR_MSG, argv[0]);
    return STATUS_BUILTIN_ERROR;
}

/// Returns a list of all builtin names.
wcstring_list_t builtin_get_names(void) {
    wcstring_list_t result;
    result.reserve(BUILTIN_COUNT);
    for (size_t i = 0; i < BUILTIN_COUNT; i++) {
        result.push_back(builtin_datas[i].name);
    }
    return result;
}

/// Insert all builtin names into list.
void builtin_get_names(std::vector<completion_t> *list) {
    assert(list != NULL);
    list->reserve(list->size() + BUILTIN_COUNT);
    for (size_t i = 0; i < BUILTIN_COUNT; i++) {
        append_completion(list, builtin_datas[i].name);
    }
}

/// Return a one-line description of the specified builtin.
wcstring builtin_get_desc(const wcstring &name) {
    wcstring result;
    const builtin_data_t *builtin = builtin_lookup(name);
    if (builtin) {
        result = _(builtin->desc);
    }
    return result;
}
