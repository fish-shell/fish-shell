// Functions for reading a character of input from stdin.
#include "config.h"

#include <errno.h>
#include <wchar.h>
#include <wctype.h>
#if HAVE_TERM_H
#include <curses.h>
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif
#include <termios.h>

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
#include "parser.h"
#include "proc.h"
#include "reader.h"
#include "signal.h"  // IWYU pragma: keep
#include "wutil.h"   // IWYU pragma: keep

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

    input_mapping_t(wcstring s, std::vector<wcstring> c, wcstring m,
                    wcstring sm)
        : seq(std::move(s)), commands(std::move(c)), mode(std::move(m)), sets_mode(std::move(sm)) {
        static unsigned int s_last_input_map_spec_order = 0;
        specification_order = ++s_last_input_map_spec_order;
    }
};

/// A struct representing the mapping from a terminfo key name to a terminfo character sequence.
struct terminfo_mapping_t {
    const wchar_t *name;  // name of key
    const char *seq;      // character sequence generated on keypress
};

static constexpr size_t input_function_count = R_END_INPUT_FUNCTIONS - R_BEGIN_INPUT_FUNCTIONS;

/// Input function metadata. This list should be kept in sync with the key code list in
/// input_common.h.
struct input_function_metadata_t {
    wchar_t code;
    const wchar_t *name;
};
static const input_function_metadata_t input_function_metadata[] = {
    {R_BEGINNING_OF_LINE, L"beginning-of-line"},
    {R_END_OF_LINE, L"end-of-line"},
    {R_FORWARD_CHAR, L"forward-char"},
    {R_BACKWARD_CHAR, L"backward-char"},
    {R_FORWARD_WORD, L"forward-word"},
    {R_BACKWARD_WORD, L"backward-word"},
    {R_FORWARD_BIGWORD, L"forward-bigword"},
    {R_BACKWARD_BIGWORD, L"backward-bigword"},
    {R_HISTORY_SEARCH_BACKWARD, L"history-search-backward"},
    {R_HISTORY_SEARCH_FORWARD, L"history-search-forward"},
    {R_DELETE_CHAR, L"delete-char"},
    {R_BACKWARD_DELETE_CHAR, L"backward-delete-char"},
    {R_KILL_LINE, L"kill-line"},
    {R_YANK, L"yank"},
    {R_YANK_POP, L"yank-pop"},
    {R_COMPLETE, L"complete"},
    {R_COMPLETE_AND_SEARCH, L"complete-and-search"},
    {R_PAGER_TOGGLE_SEARCH, L"pager-toggle-search"},
    {R_BEGINNING_OF_HISTORY, L"beginning-of-history"},
    {R_END_OF_HISTORY, L"end-of-history"},
    {R_BACKWARD_KILL_LINE, L"backward-kill-line"},
    {R_KILL_WHOLE_LINE, L"kill-whole-line"},
    {R_KILL_WORD, L"kill-word"},
    {R_KILL_BIGWORD, L"kill-bigword"},
    {R_BACKWARD_KILL_WORD, L"backward-kill-word"},
    {R_BACKWARD_KILL_PATH_COMPONENT, L"backward-kill-path-component"},
    {R_BACKWARD_KILL_BIGWORD, L"backward-kill-bigword"},
    {R_HISTORY_TOKEN_SEARCH_BACKWARD, L"history-token-search-backward"},
    {R_HISTORY_TOKEN_SEARCH_FORWARD, L"history-token-search-forward"},
    {R_SELF_INSERT, L"self-insert"},
    {R_TRANSPOSE_CHARS, L"transpose-chars"},
    {R_TRANSPOSE_WORDS, L"transpose-words"},
    {R_UPCASE_WORD, L"upcase-word"},
    {R_DOWNCASE_WORD, L"downcase-word"},
    {R_CAPITALIZE_WORD, L"capitalize-word"},
    {R_VI_ARG_DIGIT, L"vi-arg-digit"},
    {R_VI_DELETE_TO, L"vi-delete-to"},
    {R_EXECUTE, L"execute"},
    {R_BEGINNING_OF_BUFFER, L"beginning-of-buffer"},
    {R_END_OF_BUFFER, L"end-of-buffer"},
    {R_REPAINT, L"repaint"},
    {R_FORCE_REPAINT, L"force-repaint"},
    {R_UP_LINE, L"up-line"},
    {R_DOWN_LINE, L"down-line"},
    {R_SUPPRESS_AUTOSUGGESTION, L"suppress-autosuggestion"},
    {R_ACCEPT_AUTOSUGGESTION, L"accept-autosuggestion"},
    {R_BEGIN_SELECTION, L"begin-selection"},
    {R_SWAP_SELECTION_START_STOP, L"swap-selection-start-stop"},
    {R_END_SELECTION, L"end-selection"},
    {R_KILL_SELECTION, L"kill-selection"},
    {R_FORWARD_JUMP, L"forward-jump"},
    {R_BACKWARD_JUMP, L"backward-jump"},
    {R_FORWARD_JUMP_TILL, L"forward-jump-till"},
    {R_BACKWARD_JUMP_TILL, L"backward-jump-till"},
    {R_REPEAT_JUMP, L"repeat-jump"},
    {R_REVERSE_REPEAT_JUMP, L"repeat-jump-reverse"},
    {R_AND, L"and"},
    {R_CANCEL, L"cancel"}};

static_assert(sizeof(input_function_metadata) / sizeof(input_function_metadata[0]) ==
                  input_function_count,
              "input_function_metadata size mismatch with input_common. Did you forget to update "
              "input_function_metadata?");

wcstring describe_char(wint_t c) {
    if (c >= R_BEGIN_INPUT_FUNCTIONS && c < R_END_INPUT_FUNCTIONS) {
        size_t idx = c - R_BEGIN_INPUT_FUNCTIONS;
        return format_string(L"%02x (%ls)", c, input_function_metadata[idx].name);
    }
    return format_string(L"%02x", c);
}

/// Mappings for the current input mode.
static std::vector<input_mapping_t> mapping_list;
static std::vector<input_mapping_t> preset_mapping_list;

/// Terminfo map list.
static std::vector<terminfo_mapping_t> terminfo_mappings;

#define TERMINFO_ADD(key) \
    { (L## #key) + 4, key }

/// List of all terminfo mappings.
static std::vector<terminfo_mapping_t> mappings;

/// Set to true when the input subsystem has been initialized.
bool input_initialized = false;

/// Initialize terminfo.
static void init_input_terminfo();

static wchar_t input_function_args[MAX_INPUT_FUNCTION_ARGS];
static bool input_function_status;
static int input_function_args_index = 0;

/// Return the current bind mode.
wcstring input_get_bind_mode() {
    auto mode = env_get(FISH_BIND_MODE_VAR);
    return mode ? mode->as_string() : DEFAULT_BIND_MODE;
}

/// Set the current bind mode.
void input_set_bind_mode(const wcstring &bm) {
    // Only set this if it differs to not execute variable handlers all the time.
    // modes may not be empty - empty is a sentinel value meaning to not change the mode
    assert(!bm.empty());
    if (input_get_bind_mode() != bm.c_str()) {
        env_set_one(FISH_BIND_MODE_VAR, ENV_GLOBAL, bm);
    }
}

/// Returns the arity of a given input function.
static int input_function_arity(int function) {
    switch (function) {
        case R_FORWARD_JUMP:
        case R_BACKWARD_JUMP:
        case R_FORWARD_JUMP_TILL:
        case R_BACKWARD_JUMP_TILL:
            return 1;
        default:
            return 0;
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
static void input_mapping_insert_sorted(const input_mapping_t &new_mapping, bool user = true) {
    auto& ml = user ? mapping_list : preset_mapping_list;
    
    std::vector<input_mapping_t>::iterator loc = std::lower_bound(
        ml.begin(), ml.end(), new_mapping, length_is_greater_than);
    ml.insert(loc, new_mapping);
}

/// Adds an input mapping.
void input_mapping_add(const wchar_t *sequence, const wchar_t *const *commands, size_t commands_len,
                       const wchar_t *mode, const wchar_t *sets_mode, bool user) {
    CHECK(sequence, );
    CHECK(commands, );
    CHECK(mode, );
    CHECK(sets_mode, );

    // debug( 0, L"Add mapping from %ls to %ls in mode %ls", escape_string(sequence,
    // ESCAPE_ALL).c_str(),
    // escape_string(command, ESCAPE_ALL).c_str(), mode);

    // Remove existing mappings with this sequence.
    const wcstring_list_t commands_vector(commands, commands + commands_len);

    auto& ml = user ? mapping_list : preset_mapping_list;
    
    for (size_t i = 0; i < ml.size(); i++) {
        input_mapping_t &m = ml.at(i);
        if (m.seq == sequence && m.mode == mode) {
            m.commands = commands_vector;
            m.sets_mode = sets_mode;
            return;
        }
    }

    // Add a new mapping, using the next order.
    const input_mapping_t new_mapping = input_mapping_t(sequence, commands_vector, mode, sets_mode);
    input_mapping_insert_sorted(new_mapping, user);
}

void input_mapping_add(const wchar_t *sequence, const wchar_t *command, const wchar_t *mode,
                       const wchar_t *sets_mode, bool user) {
    input_mapping_add(sequence, &command, 1, mode, sets_mode, user);
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
        return shell_modes.c_cc[VINTR];
    }

    return R_NULL;
}

/// Set up arrays used by readch to detect escape sequences for special keys and perform related
/// initializations for our input subsystem.
void init_input() {
    input_common_init(&interrupt_handler);
    init_input_terminfo();

    // If we have no keybindings, add a few simple defaults.
    if (preset_mapping_list.empty()) {
        input_mapping_add(L"", L"self-insert", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping_add(L"\n", L"execute", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping_add(L"\r", L"execute", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping_add(L"\t", L"complete", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping_add(L"\x3", L"commandline ''", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping_add(L"\x4", L"exit", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping_add(L"\x5", L"bind", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping_add(L"\x7f", L"backward-delete-char", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        // Arrows - can't have functions, so *-or-search isn't available.
        input_mapping_add(L"\x1B[A", L"up-line", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping_add(L"\x1B[B", L"down-line", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping_add(L"\x1B[C", L"forward-char", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping_add(L"\x1B[D", L"backward-char", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
    }

    input_initialized = true;
}

void input_destroy() {
    if (!input_initialized) return;
    input_initialized = false;
    input_common_destroy();
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
        while ((arg = input_common_readch(0)) >= R_BEGIN_INPUT_FUNCTIONS &&
               arg < R_END_INPUT_FUNCTIONS) {
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

    for (const wcstring &cmd : m.commands) {
        if (input_function_get_code(cmd) != INPUT_CODE_NONE)
            has_functions = true;
        else
            has_commands = true;
    }

    // !has_functions && !has_commands: only set bind mode
    if (!has_commands && !has_functions) {
        if (!m.sets_mode.empty()) input_set_bind_mode(m.sets_mode);
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
        for (const wcstring &cmd : m.commands) {
            parser_t::principal_parser().eval(cmd.c_str(), io_chain_t(), TOP);
        }
        proc_set_last_status(last_status);
        input_common_next_ch(R_NULL);
    } else {
        // Invalid binding, mixed commands and functions.  We would need to execute these one by
        // one.
        input_common_next_ch(R_NULL);
    }

    // Empty bind mode indicates to not reset the mode (#2871)
    if (!m.sets_mode.empty()) input_set_bind_mode(m.sets_mode);
}

/// Try reading the specified function mapping.
static bool input_mapping_is_match(const input_mapping_t &m) {
    const wcstring &str = m.seq;

    assert(str.size() > 0 && "zero-length input string passed to input_mapping_is_match!");

    bool timed = false;
    for (size_t i = 0; i < str.size(); ++i) {
        wchar_t read = input_common_readch(timed);

        if (read != str[i]) {
            // We didn't match the bind sequence/input mapping, (it timed out or they entered something else)
            // Undo consumption of the read characters since we didn't match the bind sequence and abort.
            input_common_next_ch(read);
            while (i--) {
                input_common_next_ch(str[i]);
            }
            return false;
        }

        // If we just read an escape, we need to add a timeout for the next char,
        // to distinguish between the actual escape key and an "alt"-modifier.
        timed = (str[i] == L'\x1B');
    }

    return true;
}

void input_queue_ch(wint_t ch) { input_common_queue_ch(ch); }

static void input_mapping_execute_matching_or_generic(bool allow_commands) {
    const input_mapping_t *generic = NULL;

    const wcstring bind_mode = input_get_bind_mode();

    for (auto& m : mapping_list) {
        if (m.mode != bind_mode) {
            continue;
        }

        if (m.seq.length() == 0) {
            generic = &m;
        } else if (input_mapping_is_match(m)) {
            input_mapping_execute(m, allow_commands);
            return;
        }
    }

    // HACK: This is ugly duplication.
    for (auto& m : preset_mapping_list) {
        if (m.mode != bind_mode) {
            continue;
        }

        if (m.seq.length() == 0) {
            // Only use this generic if the user list didn't have one.
            if (!generic) generic = &m;
        } else if (input_mapping_is_match(m)) {
            input_mapping_execute(m, allow_commands);
            return;
        }
    }

    if (generic) {
        input_mapping_execute(*generic, allow_commands);
    } else {
        debug(2, L"no generic found, ignoring char...");
        wchar_t c = input_common_readch(0);
        if (c == R_EOF) {
            input_common_next_ch(c);
        }
    }
}

/// Helper function. Picks through the queue of incoming characters until we get to one that's not a
/// readline function.
static wchar_t input_read_characters_only() {
    std::vector<wchar_t> functions_to_put_back;
    wchar_t char_to_return;
    for (;;) {
        char_to_return = input_common_readch(0);
        bool is_readline_function =
            (char_to_return >= R_BEGIN_INPUT_FUNCTIONS && char_to_return < R_END_INPUT_FUNCTIONS);
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

        if (c >= R_BEGIN_INPUT_FUNCTIONS && c < R_END_INPUT_FUNCTIONS) {
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
                    c = input_common_readch(0);
                    while (c >= R_BEGIN_INPUT_FUNCTIONS && c < R_END_INPUT_FUNCTIONS) {
                        c = input_common_readch(0);
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

std::vector<input_mapping_name_t> input_mapping_get_names(bool user) {
    // Sort the mappings by the user specification order, so we can return them in the same order
    // that the user specified them in.
    std::vector<input_mapping_t> local_list = user ? mapping_list : preset_mapping_list;
    std::sort(local_list.begin(), local_list.end(), specification_order_is_less_than);
    std::vector<input_mapping_name_t> result;
    result.reserve(local_list.size());

    for (size_t i = 0; i < local_list.size(); i++) {
        const input_mapping_t &m = local_list.at(i);
        result.push_back((input_mapping_name_t){m.seq, m.mode});
    }
    return result;
}

void input_mapping_clear(const wchar_t *mode, bool user) {
    ASSERT_IS_MAIN_THREAD();
    auto &ml = user ? mapping_list : preset_mapping_list;
    auto should_erase = [=](const input_mapping_t &m) { return mode == NULL || mode == m.mode; };
    ml.erase(std::remove_if(ml.begin(), ml.end(), should_erase), ml.end());
}

bool input_mapping_erase(const wcstring &sequence, const wcstring &mode, bool user) {
    ASSERT_IS_MAIN_THREAD();
    bool result = false;
    auto& ml = user ? mapping_list : preset_mapping_list;
    for (std::vector<input_mapping_t>::iterator it = ml.begin(), end = ml.end();
         it != end; ++it) {
        if (sequence == it->seq && mode == it->mode) {
            ml.erase(it);
            result = true;
            break;
        }
    }
    return result;
}

bool input_mapping_get(const wcstring &sequence, const wcstring &mode, wcstring_list_t *out_cmds, bool user,
                       wcstring *out_sets_mode) {
    bool result = false;
    auto& ml = user ? mapping_list : preset_mapping_list;
    for (const input_mapping_t &m : ml) {
        if (sequence == m.seq && mode == m.mode) {
            *out_cmds = m.commands;
            *out_sets_mode = m.sets_mode;
            result = true;
            break;
        }
    }
    return result;
}

/// Add all terminfo mappings and cache other terminfo facts we care about.
static void init_input_terminfo() {
    assert(curses_initialized);
    if (!cur_term) return;  // setupterm() failed so we can't referency any key definitions
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
    assert(input_initialized);
    CHECK(name, 0);

    const char *res = 0;
    int err = ENOENT;

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
    assert(input_initialized);

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
    assert(input_initialized);
    wcstring_list_t result;
    result.reserve(terminfo_mappings.size());

    for (size_t i = 0; i < terminfo_mappings.size(); i++) {
        terminfo_mapping_t &m = terminfo_mappings.at(i);

        if (skip_null && !m.seq) {
            continue;
        }
        result.push_back(wcstring(m.name));
    }
    return result;
}

wcstring_list_t input_function_get_names() {
    wcstring_list_t result;
    result.reserve(input_function_count);
    for (const auto &md : input_function_metadata) {
        result.push_back(md.name);
    }
    return result;
}

wchar_t input_function_get_code(const wcstring &name) {
    for (const auto &md : input_function_metadata) {
        if (name == md.name) {
            return md.code;
        }
    }
    return INPUT_CODE_NONE;
}
