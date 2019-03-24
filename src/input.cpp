// Functions for reading a character of input from stdin.
#include "config.h"

#include <errno.h>
#include <cwchar>
#include <wctype.h>
#if HAVE_TERM_H
#include <curses.h>
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif
#include <termios.h>

#include <algorithm>
#include <atomic>
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

    input_mapping_t(wcstring s, wcstring_list_t c, wcstring m, wcstring sm)
        : seq(std::move(s)), commands(std::move(c)), mode(std::move(m)), sets_mode(std::move(sm)) {
        static unsigned int s_last_input_map_spec_order = 0;
        specification_order = ++s_last_input_map_spec_order;
    }

    /// \return true if this is a generic mapping, i.e. acts as a fallback.
    bool is_generic() const { return seq.empty(); }
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
    readline_cmd_t code;
    const wchar_t *name;
};

static const input_function_metadata_t input_function_metadata[] = {
    {readline_cmd_t::beginning_of_line, L"beginning-of-line"},
    {readline_cmd_t::end_of_line, L"end-of-line"},
    {readline_cmd_t::forward_char, L"forward-char"},
    {readline_cmd_t::backward_char, L"backward-char"},
    {readline_cmd_t::forward_word, L"forward-word"},
    {readline_cmd_t::backward_word, L"backward-word"},
    {readline_cmd_t::forward_bigword, L"forward-bigword"},
    {readline_cmd_t::backward_bigword, L"backward-bigword"},
    {readline_cmd_t::history_search_backward, L"history-search-backward"},
    {readline_cmd_t::history_search_forward, L"history-search-forward"},
    {readline_cmd_t::delete_char, L"delete-char"},
    {readline_cmd_t::backward_delete_char, L"backward-delete-char"},
    {readline_cmd_t::kill_line, L"kill-line"},
    {readline_cmd_t::yank, L"yank"},
    {readline_cmd_t::yank_POP, L"yank-pop"},
    {readline_cmd_t::complete, L"complete"},
    {readline_cmd_t::complete_AND_SEARCH, L"complete-and-search"},
    {readline_cmd_t::pager_toggle_search, L"pager-toggle-search"},
    {readline_cmd_t::beginning_of_history, L"beginning-of-history"},
    {readline_cmd_t::end_of_history, L"end-of-history"},
    {readline_cmd_t::backward_kill_line, L"backward-kill-line"},
    {readline_cmd_t::kill_whole_line, L"kill-whole-line"},
    {readline_cmd_t::kill_word, L"kill-word"},
    {readline_cmd_t::kill_bigword, L"kill-bigword"},
    {readline_cmd_t::backward_kill_word, L"backward-kill-word"},
    {readline_cmd_t::backward_kill_path_component, L"backward-kill-path-component"},
    {readline_cmd_t::backward_kill_bigword, L"backward-kill-bigword"},
    {readline_cmd_t::history_token_search_backward, L"history-token-search-backward"},
    {readline_cmd_t::history_token_search_forward, L"history-token-search-forward"},
    {readline_cmd_t::self_insert, L"self-insert"},
    {readline_cmd_t::transpose_chars, L"transpose-chars"},
    {readline_cmd_t::transpose_words, L"transpose-words"},
    {readline_cmd_t::upcase_word, L"upcase-word"},
    {readline_cmd_t::downcase_word, L"downcase-word"},
    {readline_cmd_t::capitalize_word, L"capitalize-word"},
    {readline_cmd_t::vi_arg_digit, L"vi-arg-digit"},
    {readline_cmd_t::vi_delete_to, L"vi-delete-to"},
    {readline_cmd_t::execute, L"execute"},
    {readline_cmd_t::beginning_of_buffer, L"beginning-of-buffer"},
    {readline_cmd_t::end_of_buffer, L"end-of-buffer"},
    {readline_cmd_t::repaint, L"repaint"},
    {readline_cmd_t::force_repaint, L"force-repaint"},
    {readline_cmd_t::up_line, L"up-line"},
    {readline_cmd_t::down_line, L"down-line"},
    {readline_cmd_t::suppress_autosuggestion, L"suppress-autosuggestion"},
    {readline_cmd_t::accept_autosuggestion, L"accept-autosuggestion"},
    {readline_cmd_t::begin_selection, L"begin-selection"},
    {readline_cmd_t::swap_selection_start_stop, L"swap-selection-start-stop"},
    {readline_cmd_t::end_selection, L"end-selection"},
    {readline_cmd_t::kill_selection, L"kill-selection"},
    {readline_cmd_t::forward_jump, L"forward-jump"},
    {readline_cmd_t::backward_jump, L"backward-jump"},
    {readline_cmd_t::forward_jump_till, L"forward-jump-till"},
    {readline_cmd_t::backward_jump_till, L"backward-jump-till"},
    {readline_cmd_t::repeat_jump, L"repeat-jump"},
    {readline_cmd_t::reverse_repeat_jump, L"repeat-jump-reverse"},
    {readline_cmd_t::func_and, L"and"},
    {readline_cmd_t::expand_abbr, L"expand-abbr"},
    {readline_cmd_t::cancel, L"cancel"}};

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

/// Initialize terminfo.
static void init_input_terminfo();

static wchar_t input_function_args[MAX_INPUT_FUNCTION_ARGS];
static bool input_function_status;
static int input_function_args_index = 0;

/// Return the current bind mode.
wcstring input_get_bind_mode(const environment_t &vars) {
    auto mode = vars.get(FISH_BIND_MODE_VAR);
    return mode ? mode->as_string() : DEFAULT_BIND_MODE;
}

/// Set the current bind mode.
void input_set_bind_mode(const wcstring &bm) {
    // Only set this if it differs to not execute variable handlers all the time.
    // modes may not be empty - empty is a sentinel value meaning to not change the mode
    ASSERT_IS_MAIN_THREAD();
    auto &vars = parser_t::principal_parser().vars();
    assert(!bm.empty());
    if (input_get_bind_mode(vars) != bm) {
        vars.set_one(FISH_BIND_MODE_VAR, ENV_GLOBAL, bm);
    }
}

/// Returns the arity of a given input function.
static int input_function_arity(readline_cmd_t function) {
    switch (function) {
        case readline_cmd_t::forward_jump:
        case readline_cmd_t::backward_jump:
        case readline_cmd_t::forward_jump_till:
        case readline_cmd_t::backward_jump_till:
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

    // Remove existing mappings with this sequence.
    const wcstring_list_t commands_vector(commands, commands + commands_len);

    auto& ml = user ? mapping_list : preset_mapping_list;

    for (input_mapping_t& m : ml) {
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
static maybe_t<char_event_t> interrupt_handler() {
    // Fire any pending events.
    event_fire_delayed();
    // Reap stray processes, including printing exit status messages.
    if (job_reap(true)) reader_repaint_needed();
    // Tell the reader an event occured.
    if (reader_reading_interrupted()) {
        auto vintr = shell_modes.c_cc[VINTR];
        if (vintr == 0) {
            return none();
        }
        return char_event_t{vintr};
    }

    return char_event_t{char_event_type_t::check_exit};
}

static std::atomic<bool> input_initialized{false};

/// Set up arrays used by readch to detect escape sequences for special keys and perform related
/// initializations for our input subsystem.
void init_input() {
    ASSERT_IS_MAIN_THREAD();
    if (input_initialized.load(std::memory_order_relaxed)) return;
    input_initialized.store(true, std::memory_order_relaxed);

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
}

void input_function_push_arg(wchar_t arg) {
    input_function_args[input_function_args_index++] = arg;
}

wchar_t input_function_pop_arg() { return input_function_args[--input_function_args_index]; }

void input_function_push_args(readline_cmd_t code) {
    int arity = input_function_arity(code);
    std::vector<char_event_t> skipped;

    for (int i = 0; i < arity; i++) {
        // Skip and queue up any function codes. See issue #2357.
        wchar_t arg{};
        for (;;) {
            auto evt = input_common_readch();
            if (evt.is_char()) {
                arg = evt.get_char();
                break;
            }
            skipped.push_back(evt);
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
        if (input_function_get_code(cmd))
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
        // We don't want to run commands yet. Put the characters back and return check_exit.
        for (wcstring::const_reverse_iterator it = m.seq.rbegin(), end = m.seq.rend(); it != end;
             ++it) {
            input_common_next_ch(*it);
        }
        input_common_next_ch(char_event_type_t::check_exit);
        return;  // skip the input_set_bind_mode
    } else if (has_functions && !has_commands) {
        // Functions are added at the head of the input queue.
        for (wcstring_list_t::const_reverse_iterator it = m.commands.rbegin(),
                                                     end = m.commands.rend();
             it != end; ++it) {
            readline_cmd_t code = input_function_get_code(*it).value();
            input_function_push_args(code);
            input_common_next_ch(code);
        }
    } else if (has_commands && !has_functions) {
        // Execute all commands.
        //
        // FIXME(snnw): if commands add stuff to input queue (e.g. commandline -f execute), we won't
        // see that until all other commands have also been run.
        auto last_statuses = proc_get_last_statuses();
        for (const wcstring &cmd : m.commands) {
            parser_t::principal_parser().eval(cmd, io_chain_t(), TOP);
        }
        proc_set_last_statuses(std::move(last_statuses));
        input_common_next_ch(char_event_type_t::check_exit);
    } else {
        // Invalid binding, mixed commands and functions.  We would need to execute these one by
        // one.
        input_common_next_ch(char_event_type_t::check_exit);
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
        auto evt = timed ? input_common_readch_timed() : input_common_readch();
        if (!evt.is_char() || evt.get_char() != str[i]) {
            // We didn't match the bind sequence/input mapping, (it timed out or they entered
            // something else) Undo consumption of the read characters since we didn't match the
            // bind sequence and abort.
            input_common_next_ch(evt);
            while (i--) input_common_next_ch(str[i]);
            return false;
        }

        // If we just read an escape, we need to add a timeout for the next char,
        // to distinguish between the actual escape key and an "alt"-modifier.
        timed = (str[i] == L'\x1B');
    }

    return true;
}

void input_queue_ch(char_event_t ch) { input_common_queue_ch(ch); }

/// \return the first mapping that matches, walking first over the user's mapping list, then the
/// preset list. \return null if nothing matches.
static const input_mapping_t *find_mapping() {
    const input_mapping_t *generic = NULL;
    const auto &vars = parser_t::principal_parser().vars();
    const wcstring bind_mode = input_get_bind_mode(vars);

    const auto lists = {&mapping_list, &preset_mapping_list};
    for (const auto *listp : lists) {
        for (const auto &m : *listp) {
            if (m.mode != bind_mode) {
                continue;
            }

            if (m.is_generic()) {
                if (!generic) generic = &m;
            } else if (input_mapping_is_match(m)) {
                return &m;
            }
        }
    }
    return generic;
}

static void input_mapping_execute_matching_or_generic(bool allow_commands) {
    const input_mapping_t *mapping = find_mapping();
    if (mapping) {
        input_mapping_execute(*mapping, allow_commands);
    } else {
        debug(2, L"no generic found, ignoring char...");
        auto evt = input_common_readch();
        if (evt.is_eof()) {
            input_common_next_ch(evt);
        }
    }
}

/// Helper function. Picks through the queue of incoming characters until we get to one that's not a
/// readline function.
static char_event_t input_read_characters_no_readline() {
    std::vector<char_event_t> saved_events;
    char_event_t evt_to_return{0};
    for (;;) {
        auto evt = input_common_readch();
        if (evt.is_readline()) {
            saved_events.push_back(evt);
        } else {
            evt_to_return = evt;
            break;
        }
    }
    // Restore any readline functions, in reverse to preserve their original order.
    for (auto iter = saved_events.rbegin(); iter != saved_events.rend(); ++iter) {
        input_common_next_ch(*iter);
    }
    return evt_to_return;
}

char_event_t input_readch(bool allow_commands) {
    // Clear the interrupted flag.
    reader_reset_interrupted();
    // Search for sequence in mapping tables.
    while (true) {
        auto evt = input_common_readch();

        if (evt.is_readline()) {
            switch (evt.get_readline()) {
                case readline_cmd_t::self_insert: {
                    // Issue #1595: ensure we only insert characters, not readline functions. The
                    // common case is that this will be empty.
                    return input_read_characters_no_readline();
                }
                case readline_cmd_t::func_and: {
                    if (input_function_status) {
                        return input_readch();
                    }
                    do {
                        evt = input_common_readch();
                    } while (evt.is_readline());
                    input_common_next_ch(evt);
                    return input_readch();
                }
                default: { return evt; }
            }
        } else if (evt.is_eof()) {
            // If we have EOF, we need to immediately quit.
            // There's no need to go through the input functions.
            return evt;
        } else {
            input_common_next_ch(evt);
            input_mapping_execute_matching_or_generic(allow_commands);
            // Regarding allow_commands, we're in a loop, but if a fish command is executed,
            // check_exit is unread, so the next pass through the loop we'll break out and return
            // it.
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

    for (const terminfo_mapping_t &m : terminfo_mappings) {
        if (!std::wcscmp(name, m.name)) {
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

    for (const terminfo_mapping_t &m : terminfo_mappings) {
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

    for (const terminfo_mapping_t &m : terminfo_mappings) {
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

maybe_t<readline_cmd_t> input_function_get_code(const wcstring &name) {
    for (const auto &md : input_function_metadata) {
        if (name == md.name) {
            return md.code;
        }
    }
    return none();
}
