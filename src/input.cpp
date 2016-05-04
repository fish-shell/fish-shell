// Functions for reading a character of input from stdin.
#include "config.h"

#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <wchar.h>
#if HAVE_NCURSES_H
#include <ncurses.h>
#elif HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif
#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif
#include <wctype.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "common.h"
#include "env.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "input.h"
#include "input_common.h"
#include "io.h"
#include "output.h"
#include "parser.h"
#include "proc.h"
#include "reader.h"
#include "signal.h"  // IWYU pragma: keep
#include "wutil.h"   // IWYU pragma: keep

#define DEFAULT_TERM L"ansi"
#define MAX_INPUT_FUNCTION_ARGS 20

/// Struct representing a keybinding. Returned by input_get_mappings.
struct input_mapping_t {
    /// Character sequence which generates this event.
    wcstring seq;
    /// Commands that should be evaluated by this mapping.
    wcstring_list_t commands;
    /// We wish to preserve the user-specified order. This is just an incrementing value.
    unsigned int specification_order;
    /// Mode in which this command should be evaluated.
    wcstring mode;
    /// New mode that should be switched to after command evaluation.
    wcstring sets_mode;

    input_mapping_t(const wcstring &s, const std::vector<wcstring> &c,
                    const wcstring &m = DEFAULT_BIND_MODE, const wcstring &sm = DEFAULT_BIND_MODE)
        : seq(s), commands(c), mode(m), sets_mode(sm) {
        static unsigned int s_last_input_mapping_specification_order = 0;
        specification_order = ++s_last_input_mapping_specification_order;
    }
};

/// A struct representing the mapping from a terminfo key name to a terminfo character sequence.
struct terminfo_mapping_t {
    const wchar_t *name;  // name of key
    const char *seq;      // character sequence generated on keypress
};

/// Names of all the input functions supported.
static const wchar_t *const name_arr[] = {L"beginning-of-line",
                                          L"end-of-line",
                                          L"forward-char",
                                          L"backward-char",
                                          L"forward-word",
                                          L"backward-word",
                                          L"forward-bigword",
                                          L"backward-bigword",
                                          L"history-search-backward",
                                          L"history-search-forward",
                                          L"delete-char",
                                          L"backward-delete-char",
                                          L"kill-line",
                                          L"yank",
                                          L"yank-pop",
                                          L"complete",
                                          L"complete-and-search",
                                          L"beginning-of-history",
                                          L"end-of-history",
                                          L"backward-kill-line",
                                          L"kill-whole-line",
                                          L"kill-word",
                                          L"kill-bigword",
                                          L"backward-kill-word",
                                          L"backward-kill-path-component",
                                          L"backward-kill-bigword",
                                          L"history-token-search-backward",
                                          L"history-token-search-forward",
                                          L"self-insert",
                                          L"transpose-chars",
                                          L"transpose-words",
                                          L"upcase-word",
                                          L"downcase-word",
                                          L"capitalize-word",
                                          L"vi-arg-digit",
                                          L"vi-delete-to",
                                          L"execute",
                                          L"beginning-of-buffer",
                                          L"end-of-buffer",
                                          L"repaint",
                                          L"force-repaint",
                                          L"up-line",
                                          L"down-line",
                                          L"suppress-autosuggestion",
                                          L"accept-autosuggestion",
                                          L"begin-selection",
                                          L"swap-selection-start-stop",
                                          L"end-selection",
                                          L"kill-selection",
                                          L"forward-jump",
                                          L"backward-jump",
                                          L"and",
                                          L"cancel"};

wcstring describe_char(wint_t c) {
    wint_t initial_cmd_char = R_BEGINNING_OF_LINE;
    size_t name_count = sizeof name_arr / sizeof *name_arr;
    if (c >= initial_cmd_char && c < initial_cmd_char + name_count) {
        return format_string(L"%02x (%ls)", c, name_arr[c - initial_cmd_char]);
    }
    return format_string(L"%02x", c);
}

/// Description of each supported input function.
static const wchar_t *desc_arr[] = {
    L"Move to beginning of line",
    L"Move to end of line",
    L"Move forward one character",
    L"Move backward one character",
    L"Move forward one word",
    L"Move backward one word",
    L"Search backward through list of previous commands",
    L"Search forward through list of previous commands",
    L"Delete one character forward",
    L"Delete one character backward",
    L"Move contents from cursor to end of line to killring",
    L"Paste contents of killring",
    L"Rotate to previous killring entry",
    L"Guess the rest of the next input token",
    L"Move to first item of history",
    L"Move to last item of history",
    L"Clear current line",
    L"Move contents from beginning of line to cursor to killring",
    L"Move entire line to killring",
    L"Move next word to killring",
    L"Move previous word to killring",
    L"Write out key bindings",
    L"Clear entire screen",
    L"Quit the running program",
    L"Search backward through list of previous commands for matching token",
    L"Search forward through list of previous commands for matching token",
    L"Insert the pressed key",
    L"Do nothing",
    L"End of file",
    L"Repeat command"};

/// Internal code for each supported input function.
static const wchar_t code_arr[] = {R_BEGINNING_OF_LINE,
                                   R_END_OF_LINE,
                                   R_FORWARD_CHAR,
                                   R_BACKWARD_CHAR,
                                   R_FORWARD_WORD,
                                   R_BACKWARD_WORD,
                                   R_FORWARD_BIGWORD,
                                   R_BACKWARD_BIGWORD,
                                   R_HISTORY_SEARCH_BACKWARD,
                                   R_HISTORY_SEARCH_FORWARD,
                                   R_DELETE_CHAR,
                                   R_BACKWARD_DELETE_CHAR,
                                   R_KILL_LINE,
                                   R_YANK,
                                   R_YANK_POP,
                                   R_COMPLETE,
                                   R_COMPLETE_AND_SEARCH,
                                   R_BEGINNING_OF_HISTORY,
                                   R_END_OF_HISTORY,
                                   R_BACKWARD_KILL_LINE,
                                   R_KILL_WHOLE_LINE,
                                   R_KILL_WORD,
                                   R_KILL_BIGWORD,
                                   R_BACKWARD_KILL_WORD,
                                   R_BACKWARD_KILL_PATH_COMPONENT,
                                   R_BACKWARD_KILL_BIGWORD,
                                   R_HISTORY_TOKEN_SEARCH_BACKWARD,
                                   R_HISTORY_TOKEN_SEARCH_FORWARD,
                                   R_SELF_INSERT,
                                   R_TRANSPOSE_CHARS,
                                   R_TRANSPOSE_WORDS,
                                   R_UPCASE_WORD,
                                   R_DOWNCASE_WORD,
                                   R_CAPITALIZE_WORD,
                                   R_VI_ARG_DIGIT,
                                   R_VI_DELETE_TO,
                                   R_EXECUTE,
                                   R_BEGINNING_OF_BUFFER,
                                   R_END_OF_BUFFER,
                                   R_REPAINT,
                                   R_FORCE_REPAINT,
                                   R_UP_LINE,
                                   R_DOWN_LINE,
                                   R_SUPPRESS_AUTOSUGGESTION,
                                   R_ACCEPT_AUTOSUGGESTION,
                                   R_BEGIN_SELECTION,
                                   R_SWAP_SELECTION_START_STOP,
                                   R_END_SELECTION,
                                   R_KILL_SELECTION,
                                   R_FORWARD_JUMP,
                                   R_BACKWARD_JUMP,
                                   R_AND,
                                   R_CANCEL};

/// Mappings for the current input mode.
static std::vector<input_mapping_t> mapping_list;

/// Terminfo map list.
static std::vector<terminfo_mapping_t> terminfo_mappings;

#define TERMINFO_ADD(key) \
    { (L## #key) + 4, key }

/// List of all terminfo mappings.
static std::vector<terminfo_mapping_t> mappings;

/// Set to one when the input subsytem has been initialized.
static bool is_init = false;

/// Initialize terminfo.
static void input_terminfo_init();

static wchar_t input_function_args[MAX_INPUT_FUNCTION_ARGS];
static bool input_function_status;
static int input_function_args_index = 0;

/// Return the current bind mode.
wcstring input_get_bind_mode() {
    env_var_t mode = env_get_string(FISH_BIND_MODE_VAR);
    return mode.missing() ? DEFAULT_BIND_MODE : mode;
}

/// Set the current bind mode.
void input_set_bind_mode(const wcstring &bm) {
    env_set(FISH_BIND_MODE_VAR, bm.c_str(), ENV_GLOBAL);
}

/// Returns the arity of a given input function.
int input_function_arity(int function) {
    switch (function) {
        case R_FORWARD_JUMP:
        case R_BACKWARD_JUMP: {
            return 1;
        }
        default: { return 0; }
    }
}

/// Sets the return status of the most recently executed input function.
void input_function_set_status(bool status) { input_function_status = status; }

/// Helper function to compare the lengths of sequences.
static bool length_is_greater_than(const input_mapping_t &m1, const input_mapping_t &m2) {
    return m1.seq.size() > m2.seq.size();
}

static bool specification_order_is_less_than(const input_mapping_t &m1, const input_mapping_t &m2) {
    return m1.specification_order < m2.specification_order;
}

/// Inserts an input mapping at the correct position. We sort them in descending order by length, so
/// that we test longer sequences first.
static void input_mapping_insert_sorted(const input_mapping_t &new_mapping) {
    std::vector<input_mapping_t>::iterator loc = std::lower_bound(
        mapping_list.begin(), mapping_list.end(), new_mapping, length_is_greater_than);
    mapping_list.insert(loc, new_mapping);
}

/// Adds an input mapping.
void input_mapping_add(const wchar_t *sequence, const wchar_t *const *commands, size_t commands_len,
                       const wchar_t *mode, const wchar_t *sets_mode) {
    CHECK(sequence, );
    CHECK(commands, );
    CHECK(mode, );
    CHECK(sets_mode, );

    // debug( 0, L"Add mapping from %ls to %ls in mode %ls", escape(sequence, ESCAPE_ALL).c_str(),
    // escape(command, ESCAPE_ALL).c_str(), mode);

    // Remove existing mappings with this sequence.
    const wcstring_list_t commands_vector(commands, commands + commands_len);

    for (size_t i = 0; i < mapping_list.size(); i++) {
        input_mapping_t &m = mapping_list.at(i);
        if (m.seq == sequence && m.mode == mode) {
            m.commands = commands_vector;
            m.sets_mode = sets_mode;
            return;
        }
    }

    // Add a new mapping, using the next order.
    const input_mapping_t new_mapping = input_mapping_t(sequence, commands_vector, mode, sets_mode);
    input_mapping_insert_sorted(new_mapping);
}

void input_mapping_add(const wchar_t *sequence, const wchar_t *command, const wchar_t *mode,
                       const wchar_t *sets_mode) {
    input_mapping_add(sequence, &command, 1, mode, sets_mode);
}

/// Handle interruptions to key reading by reaping finshed jobs and propagating the interrupt to the
/// reader.
static int interrupt_handler() {
    // Fire any pending events.
    event_fire(NULL);
    // Reap stray processes, including printing exit status messages.
    if (job_reap(1)) reader_repaint_needed();
    // Tell the reader an event occured.
    if (reader_reading_interrupted()) {
        // Return 3, i.e. the character read by a Control-C.
        return 3;
    }

    return R_NULL;
}

void update_fish_color_support(void) {
    // Infer term256 support. If fish_term256 is set, we respect it; otherwise try to detect it from
    // the TERM variable.
    env_var_t fish_term256 = env_get_string(L"fish_term256");
    bool support_term256;
    if (!fish_term256.missing_or_empty()) {
        support_term256 = from_string<bool>(fish_term256);
    } else {
        env_var_t term = env_get_string(L"TERM");
        if (term.missing()) {
            support_term256 = false;
        } else if (term.find(L"256color") != wcstring::npos) {
            // Explicitly supported.
            support_term256 = true;
        } else if (term.find(L"xterm") != wcstring::npos) {
            // Assume that all xterms are 256, except for OS X SnowLeopard.
            env_var_t prog = env_get_string(L"TERM_PROGRAM");
            support_term256 = (prog != L"Apple_Terminal");
        } else {
            // Don't know, default to false.
            support_term256 = false;
        }
    }

    env_var_t fish_term24bit = env_get_string(L"fish_term24bit");
    bool support_term24bit;
    if (!fish_term24bit.missing_or_empty()) {
        support_term24bit = from_string<bool>(fish_term24bit);
    } else {
        // We don't attempt to infer term24 bit support yet.
        support_term24bit = false;
    }

    color_support_t support = (support_term256 ? color_support_term256 : 0) |
                              (support_term24bit ? color_support_term24bit : 0);
    output_set_color_support(support);
}

int input_init() {
    if (is_init) return 1;

    is_init = true;

    input_common_init(&interrupt_handler);

    const env_var_t term = env_get_string(L"TERM");
    int errret;
    if (setupterm(const_cast<char *>(wcs2string(term).c_str()), STDOUT_FILENO, &errret) == ERR) {
        debug(0, _(L"Could not set up terminal"));
        if (errret == 0) {
            debug(0, _(L"Check that your terminal type, '%ls', is supported on this system"),
                  term.c_str());
            debug(0, _(L"Attempting to use '%ls' instead"), DEFAULT_TERM);
            env_set(L"TERM", DEFAULT_TERM, ENV_GLOBAL | ENV_EXPORT);
            const std::string default_term = wcs2string(DEFAULT_TERM);
            if (setupterm(const_cast<char *>(default_term.c_str()), STDOUT_FILENO, &errret) ==
                ERR) {
                debug(0, _(L"Could not set up terminal"));
                exit_without_destructors(1);
            }
        } else {
            exit_without_destructors(1);
        }
    }
    assert(!term.missing());
    output_set_term(term);

    input_terminfo_init();

    update_fish_color_support();

    // If we have no keybindings, add a few simple defaults.
    if (mapping_list.empty()) {
        input_mapping_add(L"", L"self-insert");
        input_mapping_add(L"\n", L"execute");
        input_mapping_add(L"\r", L"execute");
        input_mapping_add(L"\t", L"complete");
        input_mapping_add(L"\x3", L"commandline \"\"");
        input_mapping_add(L"\x4", L"exit");
        input_mapping_add(L"\x5", L"bind");
    }

    return 1;
}

void input_destroy() {
    if (!is_init) return;
    is_init = false;
    input_common_destroy();

    if (del_curterm(cur_term) == ERR) {
        debug(0, _(L"Error while closing terminfo"));
    }
}

void input_function_push_arg(wchar_t arg) {
    input_function_args[input_function_args_index++] = arg;
}

wchar_t input_function_pop_arg() { return input_function_args[--input_function_args_index]; }

void input_function_push_args(int code) {
    int arity = input_function_arity(code);
    std::vector<wchar_t> skipped;

    for (int i = 0; i < arity; i++) {
        wchar_t arg;

        // Skip and queue up any function codes. See issue #2357.
        while (((arg = input_common_readch(0)) >= R_MIN) && (arg <= R_MAX)) {
            skipped.push_back(arg);
        }

        input_function_push_arg(arg);
    }

    // Push the function codes back into the input stream.
    size_t idx = skipped.size();
    while (idx--) {
        input_common_next_ch(skipped.at(idx));
    }
}

/// Perform the action of the specified binding. allow_commands controls whether fish commands
/// should be executed, or should be deferred until later.
static void input_mapping_execute(const input_mapping_t &m, bool allow_commands) {
    // has_functions: there are functions that need to be put on the input queue
    // has_commands: there are shell commands that need to be evaluated
    bool has_commands = false, has_functions = false;

    for (wcstring_list_t::const_iterator it = m.commands.begin(), end = m.commands.end(); it != end;
         ++it) {
        if (input_function_get_code(*it) != INPUT_CODE_NONE)
            has_functions = true;
        else
            has_commands = true;
    }

    // !has_functions && !has_commands: only set bind mode
    if (!has_commands && !has_functions) {
        input_set_bind_mode(m.sets_mode);
        return;
    }

    if (has_commands && !allow_commands) {
        // We don't want to run commands yet. Put the characters back and return R_NULL.
        for (wcstring::const_reverse_iterator it = m.seq.rbegin(), end = m.seq.rend(); it != end;
             ++it) {
            input_common_next_ch(*it);
        }
        input_common_next_ch(R_NULL);
        return;  // skip the input_set_bind_mode
    } else if (has_functions && !has_commands) {
        // Functions are added at the head of the input queue.
        for (wcstring_list_t::const_reverse_iterator it = m.commands.rbegin(),
                                                     end = m.commands.rend();
             it != end; ++it) {
            wchar_t code = input_function_get_code(*it);
            input_function_push_args(code);
            input_common_next_ch(code);
        }
    } else if (has_commands && !has_functions) {
        // Execute all commands.
        //
        // FIXME(snnw): if commands add stuff to input queue (e.g. commandline -f execute), we won't
        // see that until all other commands have also been run.
        int last_status = proc_get_last_status();
        for (wcstring_list_t::const_iterator it = m.commands.begin(), end = m.commands.end();
             it != end; ++it) {
            parser_t::principal_parser().eval(it->c_str(), io_chain_t(), TOP);
        }
        proc_set_last_status(last_status);
        input_common_next_ch(R_NULL);
    } else {
        // Invalid binding, mixed commands and functions.  We would need to execute these one by
        // one.
        input_common_next_ch(R_NULL);
    }

    input_set_bind_mode(m.sets_mode);
}

/// Try reading the specified function mapping.
static bool input_mapping_is_match(const input_mapping_t &m) {
    wint_t c = 0;
    int j;

    // debug(0, L"trying mapping %ls\n", escape(m.seq.c_str(), ESCAPE_ALL).c_str());
    const wchar_t *str = m.seq.c_str();
    for (j = 0; str[j] != L'\0'; j++) {
        bool timed = (j > 0 && iswcntrl(str[0]));

        c = input_common_readch(timed);
        if (str[j] != c) {
            break;
        }
    }

    if (str[j] == L'\0') {
        // debug(0, L"matched mapping %ls (%ls)\n", escape(m.seq.c_str(), ESCAPE_ALL).c_str(),
        // m.command.c_str());
        // We matched the entire sequence.
        return true;
    }

    // Return the read characters.
    input_common_next_ch(c);
    for (int k = j - 1; k >= 0; k--) {
        input_common_next_ch(m.seq[k]);
    }

    return false;
}

void input_queue_ch(wint_t ch) { input_common_queue_ch(ch); }

static void input_mapping_execute_matching_or_generic(bool allow_commands) {
    const input_mapping_t *generic = NULL;

    const wcstring bind_mode = input_get_bind_mode();

    for (int i = 0; i < mapping_list.size(); i++) {
        const input_mapping_t &m = mapping_list.at(i);

        // debug(0, L"trying mapping (%ls,%ls,%ls)\n", escape(m.seq.c_str(), ESCAPE_ALL).c_str(),
        //           m.mode.c_str(), m.sets_mode.c_str());

        if (m.mode != bind_mode) {
            // debug(0, L"skipping mapping because mode %ls != %ls\n", m.mode.c_str(),
            // input_get_bind_mode().c_str());
            continue;
        }

        if (m.seq.length() == 0) {
            generic = &m;
        } else if (input_mapping_is_match(m)) {
            input_mapping_execute(m, allow_commands);
            return;
        }
    }

    if (generic) {
        input_mapping_execute(*generic, allow_commands);
    } else {
        // debug(0, L"no generic found, ignoring...");
        wchar_t c = input_common_readch(0);
        if (c == R_EOF) input_common_next_ch(c);
    }
}

/// Helper function. Picks through the queue of incoming characters until we get to one that's not a
/// readline function.
static wchar_t input_read_characters_only() {
    std::vector<wchar_t> functions_to_put_back;
    wchar_t char_to_return;
    for (;;) {
        char_to_return = input_common_readch(0);
        bool is_readline_function = (char_to_return >= R_MIN && char_to_return <= R_MAX);
        // R_NULL and R_EOF are more control characters than readline functions, so check specially
        // for those.
        if (!is_readline_function || char_to_return == R_NULL || char_to_return == R_EOF) {
            break;
        }
        // This is a readline function; save it off for later re-enqueing and try again.
        functions_to_put_back.push_back(char_to_return);
    }
    // Restore any readline functions, in reverse to preserve their original order.
    size_t idx = functions_to_put_back.size();
    while (idx--) {
        input_common_next_ch(functions_to_put_back.at(idx));
    }
    return char_to_return;
}

wint_t input_readch(bool allow_commands) {
    CHECK_BLOCK(R_NULL);

    // Clear the interrupted flag.
    reader_reset_interrupted();
    // Search for sequence in mapping tables.
    while (1) {
        wchar_t c = input_common_readch(0);

        if (c >= R_MIN && c <= R_MAX) {
            switch (c) {
                case R_EOF:  // if it's closed, then just return
                {
                    return R_EOF;
                }
                case R_SELF_INSERT: {
                    // Issue #1595: ensure we only insert characters, not readline functions. The
                    // common case is that this will be empty.
                    return input_read_characters_only();
                }
                case R_AND: {
                    if (input_function_status) {
                        return input_readch();
                    }
                    while ((c = input_common_readch(0)) && c >= R_MIN && c <= R_MAX) {
                        // do nothing
                    }
                    input_common_next_ch(c);
                    return input_readch();
                }
                default: { return c; }
            }
        } else {
            input_common_next_ch(c);
            input_mapping_execute_matching_or_generic(allow_commands);
            // Regarding allow_commands, we're in a loop, but if a fish command
            // is executed, R_NULL is unread, so the next pass through the loop
            // we'll break out and return it.
        }
    }
}

std::vector<input_mapping_name_t> input_mapping_get_names() {
    // Sort the mappings by the user specification order, so we can return them in the same order
    // that the user specified them in.
    std::vector<input_mapping_t> local_list = mapping_list;
    std::sort(local_list.begin(), local_list.end(), specification_order_is_less_than);
    std::vector<input_mapping_name_t> result;
    result.reserve(local_list.size());

    for (size_t i = 0; i < local_list.size(); i++) {
        const input_mapping_t &m = local_list.at(i);
        result.push_back((input_mapping_name_t){m.seq, m.mode});
    }
    return result;
}

bool input_mapping_erase(const wcstring &sequence, const wcstring &mode) {
    ASSERT_IS_MAIN_THREAD();
    bool result = false;

    for (std::vector<input_mapping_t>::iterator it = mapping_list.begin(), end = mapping_list.end();
         it != end; ++it) {
        if (sequence == it->seq && mode == it->mode) {
            mapping_list.erase(it);
            result = true;
            break;
        }
    }
    return result;
}

bool input_mapping_get(const wcstring &sequence, const wcstring &mode, wcstring_list_t *out_cmds,
                       wcstring *out_sets_mode) {
    bool result = false;
    for (std::vector<input_mapping_t>::const_iterator it = mapping_list.begin(),
                                                      end = mapping_list.end();
         it != end; ++it) {
        if (sequence == it->seq && mode == it->mode) {
            *out_cmds = it->commands;
            *out_sets_mode = it->sets_mode;
            result = true;
            break;
        }
    }
    return result;
}

/// Add all terminfo mappings.
static void input_terminfo_init() {
    const terminfo_mapping_t tinfos[] = {
        TERMINFO_ADD(key_a1),
        TERMINFO_ADD(key_a3),
        TERMINFO_ADD(key_b2),
        TERMINFO_ADD(key_backspace),
        TERMINFO_ADD(key_beg),
        TERMINFO_ADD(key_btab),
        TERMINFO_ADD(key_c1),
        TERMINFO_ADD(key_c3),
        TERMINFO_ADD(key_cancel),
        TERMINFO_ADD(key_catab),
        TERMINFO_ADD(key_clear),
        TERMINFO_ADD(key_close),
        TERMINFO_ADD(key_command),
        TERMINFO_ADD(key_copy),
        TERMINFO_ADD(key_create),
        TERMINFO_ADD(key_ctab),
        TERMINFO_ADD(key_dc),
        TERMINFO_ADD(key_dl),
        TERMINFO_ADD(key_down),
        TERMINFO_ADD(key_eic),
        TERMINFO_ADD(key_end),
        TERMINFO_ADD(key_enter),
        TERMINFO_ADD(key_eol),
        TERMINFO_ADD(key_eos),
        TERMINFO_ADD(key_exit),
        TERMINFO_ADD(key_f0),
        TERMINFO_ADD(key_f1),
        TERMINFO_ADD(key_f2),
        TERMINFO_ADD(key_f3),
        TERMINFO_ADD(key_f4),
        TERMINFO_ADD(key_f5),
        TERMINFO_ADD(key_f6),
        TERMINFO_ADD(key_f7),
        TERMINFO_ADD(key_f8),
        TERMINFO_ADD(key_f9),
        TERMINFO_ADD(key_f10),
        TERMINFO_ADD(key_f11),
        TERMINFO_ADD(key_f12),
        TERMINFO_ADD(key_f13),
        TERMINFO_ADD(key_f14),
        TERMINFO_ADD(key_f15),
        TERMINFO_ADD(key_f16),
        TERMINFO_ADD(key_f17),
        TERMINFO_ADD(key_f18),
        TERMINFO_ADD(key_f19),
        TERMINFO_ADD(key_f20),
#if 0
        // I know of no keyboard with more than 20 function keys, so adding the rest here makes very
        // little sense, since it will take up a lot of room in any listings (like tab completions),
        // but with no benefit.
        TERMINFO_ADD(key_f21),
        TERMINFO_ADD(key_f22),
        TERMINFO_ADD(key_f23),
        TERMINFO_ADD(key_f24),
        TERMINFO_ADD(key_f25),
        TERMINFO_ADD(key_f26),
        TERMINFO_ADD(key_f27),
        TERMINFO_ADD(key_f28),
        TERMINFO_ADD(key_f29),
        TERMINFO_ADD(key_f30),
        TERMINFO_ADD(key_f31),
        TERMINFO_ADD(key_f32),
        TERMINFO_ADD(key_f33),
        TERMINFO_ADD(key_f34),
        TERMINFO_ADD(key_f35),
        TERMINFO_ADD(key_f36),
        TERMINFO_ADD(key_f37),
        TERMINFO_ADD(key_f38),
        TERMINFO_ADD(key_f39),
        TERMINFO_ADD(key_f40),
        TERMINFO_ADD(key_f41),
        TERMINFO_ADD(key_f42),
        TERMINFO_ADD(key_f43),
        TERMINFO_ADD(key_f44),
        TERMINFO_ADD(key_f45),
        TERMINFO_ADD(key_f46),
        TERMINFO_ADD(key_f47),
        TERMINFO_ADD(key_f48),
        TERMINFO_ADD(key_f49),
        TERMINFO_ADD(key_f50),
        TERMINFO_ADD(key_f51),
        TERMINFO_ADD(key_f52),
        TERMINFO_ADD(key_f53),
        TERMINFO_ADD(key_f54),
        TERMINFO_ADD(key_f55),
        TERMINFO_ADD(key_f56),
        TERMINFO_ADD(key_f57),
        TERMINFO_ADD(key_f58),
        TERMINFO_ADD(key_f59),
        TERMINFO_ADD(key_f60),
        TERMINFO_ADD(key_f61),
        TERMINFO_ADD(key_f62),
        TERMINFO_ADD(key_f63),
#endif
        TERMINFO_ADD(key_find),
        TERMINFO_ADD(key_help),
        TERMINFO_ADD(key_home),
        TERMINFO_ADD(key_ic),
        TERMINFO_ADD(key_il),
        TERMINFO_ADD(key_left),
        TERMINFO_ADD(key_ll),
        TERMINFO_ADD(key_mark),
        TERMINFO_ADD(key_message),
        TERMINFO_ADD(key_move),
        TERMINFO_ADD(key_next),
        TERMINFO_ADD(key_npage),
        TERMINFO_ADD(key_open),
        TERMINFO_ADD(key_options),
        TERMINFO_ADD(key_ppage),
        TERMINFO_ADD(key_previous),
        TERMINFO_ADD(key_print),
        TERMINFO_ADD(key_redo),
        TERMINFO_ADD(key_reference),
        TERMINFO_ADD(key_refresh),
        TERMINFO_ADD(key_replace),
        TERMINFO_ADD(key_restart),
        TERMINFO_ADD(key_resume),
        TERMINFO_ADD(key_right),
        TERMINFO_ADD(key_save),
        TERMINFO_ADD(key_sbeg),
        TERMINFO_ADD(key_scancel),
        TERMINFO_ADD(key_scommand),
        TERMINFO_ADD(key_scopy),
        TERMINFO_ADD(key_screate),
        TERMINFO_ADD(key_sdc),
        TERMINFO_ADD(key_sdl),
        TERMINFO_ADD(key_select),
        TERMINFO_ADD(key_send),
        TERMINFO_ADD(key_seol),
        TERMINFO_ADD(key_sexit),
        TERMINFO_ADD(key_sf),
        TERMINFO_ADD(key_sfind),
        TERMINFO_ADD(key_shelp),
        TERMINFO_ADD(key_shome),
        TERMINFO_ADD(key_sic),
        TERMINFO_ADD(key_sleft),
        TERMINFO_ADD(key_smessage),
        TERMINFO_ADD(key_smove),
        TERMINFO_ADD(key_snext),
        TERMINFO_ADD(key_soptions),
        TERMINFO_ADD(key_sprevious),
        TERMINFO_ADD(key_sprint),
        TERMINFO_ADD(key_sr),
        TERMINFO_ADD(key_sredo),
        TERMINFO_ADD(key_sreplace),
        TERMINFO_ADD(key_sright),
        TERMINFO_ADD(key_srsume),
        TERMINFO_ADD(key_ssave),
        TERMINFO_ADD(key_ssuspend),
        TERMINFO_ADD(key_stab),
        TERMINFO_ADD(key_sundo),
        TERMINFO_ADD(key_suspend),
        TERMINFO_ADD(key_undo),
        TERMINFO_ADD(key_up)
    };
    const size_t count = sizeof tinfos / sizeof *tinfos;
    terminfo_mappings.reserve(terminfo_mappings.size() + count);
    terminfo_mappings.insert(terminfo_mappings.end(), tinfos, tinfos + count);
}

bool input_terminfo_get_sequence(const wchar_t *name, wcstring *out_seq) {
    ASSERT_IS_MAIN_THREAD();

    const char *res = 0;
    int err = ENOENT;

    CHECK(name, 0);
    input_init();

    for (size_t i = 0; i < terminfo_mappings.size(); i++) {
        const terminfo_mapping_t &m = terminfo_mappings.at(i);
        if (!wcscmp(name, m.name)) {
            res = m.seq;
            err = EILSEQ;
            break;
        }
    }

    if (!res) {
        errno = err;
        return false;
    }

    *out_seq = format_string(L"%s", res);
    return true;
}

bool input_terminfo_get_name(const wcstring &seq, wcstring *out_name) {
    input_init();

    for (size_t i = 0; i < terminfo_mappings.size(); i++) {
        terminfo_mapping_t &m = terminfo_mappings.at(i);

        if (!m.seq) {
            continue;
        }

        const wcstring map_buf = format_string(L"%s", m.seq);
        if (map_buf == seq) {
            out_name->assign(m.name);
            return true;
        }
    }

    return false;
}

wcstring_list_t input_terminfo_get_names(bool skip_null) {
    wcstring_list_t result;
    result.reserve(terminfo_mappings.size());

    input_init();

    for (size_t i = 0; i < terminfo_mappings.size(); i++) {
        terminfo_mapping_t &m = terminfo_mappings.at(i);

        if (skip_null && !m.seq) {
            continue;
        }
        result.push_back(wcstring(m.name));
    }
    return result;
}

wcstring_list_t input_function_get_names(void) {
    size_t count = sizeof name_arr / sizeof *name_arr;
    return wcstring_list_t(name_arr, name_arr + count);
}

wchar_t input_function_get_code(const wcstring &name) {
    for (size_t i = 0; i < sizeof code_arr / sizeof *code_arr; i++) {
        if (name == name_arr[i]) {
            return code_arr[i];
        }
    }
    return INPUT_CODE_NONE;
}
