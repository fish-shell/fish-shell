// Functions for reading a character of input from stdin.
#include "config.h"

#include <errno.h>
#include <wctype.h>

#include <cwchar>
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
#include <utility>
#include <vector>

#include "common.h"
#include "env.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "global_safety.h"
#include "input.h"
#include "input_common.h"
#include "io.h"
#include "parser.h"
#include "proc.h"
#include "reader.h"
#include "signal.h"  // IWYU pragma: keep
#include "wutil.h"   // IWYU pragma: keep

/// A name for our own key mapping for nul.
static const wchar_t *k_nul_mapping_name = L"nul";

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
    // name of key
    const wchar_t *name;

    // character sequence generated on keypress, or none if there was no mapping.
    maybe_t<std::string> seq;

    terminfo_mapping_t(const wchar_t *name, const char *s) : name(name) {
        if (s) seq.emplace(s);
    }

    terminfo_mapping_t(const wchar_t *name, std::string s) : name(name), seq(std::move(s)) {}
};

static constexpr size_t input_function_count = R_END_INPUT_FUNCTIONS;

/// Input function metadata. This list should be kept in sync with the key code list in
/// input_common.h.
struct input_function_metadata_t {
    const wchar_t *name;
    readline_cmd_t code;
};

/// A static mapping of all readline commands as strings to their readline_cmd_t equivalent.
/// Keep this list sorted alphabetically!
static constexpr const input_function_metadata_t input_function_metadata[] = {
    // NULL makes it unusable - this is specially inserted when we detect mouse input
    {L"", readline_cmd_t::disable_mouse_tracking},
    {L"accept-autosuggestion", readline_cmd_t::accept_autosuggestion},
    {L"and", readline_cmd_t::func_and},
    {L"backward-bigword", readline_cmd_t::backward_bigword},
    {L"backward-char", readline_cmd_t::backward_char},
    {L"backward-delete-char", readline_cmd_t::backward_delete_char},
    {L"backward-jump", readline_cmd_t::backward_jump},
    {L"backward-jump-till", readline_cmd_t::backward_jump_till},
    {L"backward-kill-bigword", readline_cmd_t::backward_kill_bigword},
    {L"backward-kill-line", readline_cmd_t::backward_kill_line},
    {L"backward-kill-path-component", readline_cmd_t::backward_kill_path_component},
    {L"backward-kill-word", readline_cmd_t::backward_kill_word},
    {L"backward-word", readline_cmd_t::backward_word},
    {L"begin-selection", readline_cmd_t::begin_selection},
    {L"begin-undo-group", readline_cmd_t::begin_undo_group},
    {L"beginning-of-buffer", readline_cmd_t::beginning_of_buffer},
    {L"beginning-of-history", readline_cmd_t::beginning_of_history},
    {L"beginning-of-line", readline_cmd_t::beginning_of_line},
    {L"cancel", readline_cmd_t::cancel},
    {L"cancel-commandline", readline_cmd_t::cancel_commandline},
    {L"capitalize-word", readline_cmd_t::capitalize_word},
    {L"complete", readline_cmd_t::complete},
    {L"complete-and-search", readline_cmd_t::complete_and_search},
    {L"delete-char", readline_cmd_t::delete_char},
    {L"delete-or-exit", readline_cmd_t::delete_or_exit},
    {L"down-line", readline_cmd_t::down_line},
    {L"downcase-word", readline_cmd_t::downcase_word},
    {L"end-of-buffer", readline_cmd_t::end_of_buffer},
    {L"end-of-history", readline_cmd_t::end_of_history},
    {L"end-of-line", readline_cmd_t::end_of_line},
    {L"end-selection", readline_cmd_t::end_selection},
    {L"end-undo-group", readline_cmd_t::end_undo_group},
    {L"execute", readline_cmd_t::execute},
    {L"exit", readline_cmd_t::exit},
    {L"expand-abbr", readline_cmd_t::expand_abbr},
    {L"force-repaint", readline_cmd_t::force_repaint},
    {L"forward-bigword", readline_cmd_t::forward_bigword},
    {L"forward-char", readline_cmd_t::forward_char},
    {L"forward-jump", readline_cmd_t::forward_jump},
    {L"forward-jump-till", readline_cmd_t::forward_jump_till},
    {L"forward-single-char", readline_cmd_t::forward_single_char},
    {L"forward-word", readline_cmd_t::forward_word},
    {L"history-prefix-search-backward", readline_cmd_t::history_prefix_search_backward},
    {L"history-prefix-search-forward", readline_cmd_t::history_prefix_search_forward},
    {L"history-search-backward", readline_cmd_t::history_search_backward},
    {L"history-search-forward", readline_cmd_t::history_search_forward},
    {L"history-token-search-backward", readline_cmd_t::history_token_search_backward},
    {L"history-token-search-forward", readline_cmd_t::history_token_search_forward},
    {L"insert-line-over", readline_cmd_t::insert_line_over},
    {L"insert-line-under", readline_cmd_t::insert_line_under},
    {L"kill-bigword", readline_cmd_t::kill_bigword},
    {L"kill-line", readline_cmd_t::kill_line},
    {L"kill-selection", readline_cmd_t::kill_selection},
    {L"kill-whole-line", readline_cmd_t::kill_whole_line},
    {L"kill-word", readline_cmd_t::kill_word},
    {L"nextd-or-forward-word", readline_cmd_t::nextd_or_forward_word},
    {L"or", readline_cmd_t::func_or},
    {L"pager-toggle-search", readline_cmd_t::pager_toggle_search},
    {L"prevd-or-backward-word", readline_cmd_t::prevd_or_backward_word},
    {L"redo", readline_cmd_t::redo},
    {L"repaint", readline_cmd_t::repaint},
    {L"repaint-mode", readline_cmd_t::repaint_mode},
    {L"repeat-jump", readline_cmd_t::repeat_jump},
    {L"repeat-jump-reverse", readline_cmd_t::reverse_repeat_jump},
    {L"self-insert", readline_cmd_t::self_insert},
    {L"self-insert-notfirst", readline_cmd_t::self_insert_notfirst},
    {L"suppress-autosuggestion", readline_cmd_t::suppress_autosuggestion},
    {L"swap-selection-start-stop", readline_cmd_t::swap_selection_start_stop},
    {L"togglecase-char", readline_cmd_t::togglecase_char},
    {L"togglecase-selection", readline_cmd_t::togglecase_selection},
    {L"transpose-chars", readline_cmd_t::transpose_chars},
    {L"transpose-words", readline_cmd_t::transpose_words},
    {L"undo", readline_cmd_t::undo},
    {L"up-line", readline_cmd_t::up_line},
    {L"upcase-word", readline_cmd_t::upcase_word},
    {L"yank", readline_cmd_t::yank},
    {L"yank-pop", readline_cmd_t::yank_pop},
};

ASSERT_SORTED_BY_NAME(input_function_metadata);
static_assert(sizeof(input_function_metadata) / sizeof(input_function_metadata[0]) ==
                  input_function_count,
              "input_function_metadata size mismatch with input_common. Did you forget to update "
              "input_function_metadata?");

using mapping_list_t = std::vector<input_mapping_t>;
input_mapping_set_t::input_mapping_set_t() = default;
input_mapping_set_t::~input_mapping_set_t() = default;

acquired_lock<input_mapping_set_t> input_mappings() {
    static owning_lock<input_mapping_set_t> s_mappings{input_mapping_set_t()};
    return s_mappings.acquire();
}

/// Terminfo map list.
static latch_t<std::vector<terminfo_mapping_t>> s_terminfo_mappings;

/// \return the input terminfo.
static std::vector<terminfo_mapping_t> create_input_terminfo();

/// Return the current bind mode.
static wcstring input_get_bind_mode(const environment_t &vars) {
    auto mode = vars.get(FISH_BIND_MODE_VAR);
    return mode ? mode->as_string() : DEFAULT_BIND_MODE;
}

/// Set the current bind mode.
static void input_set_bind_mode(parser_t &parser, const wcstring &bm) {
    // Only set this if it differs to not execute variable handlers all the time.
    // modes may not be empty - empty is a sentinel value meaning to not change the mode
    assert(!bm.empty());
    if (input_get_bind_mode(parser.vars()) != bm) {
        // Must send events here - see #6653.
        parser.set_var_and_fire(FISH_BIND_MODE_VAR, ENV_GLOBAL, bm);
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

/// Helper function to compare the lengths of sequences.
static bool length_is_greater_than(const input_mapping_t &m1, const input_mapping_t &m2) {
    return m1.seq.size() > m2.seq.size();
}

static bool specification_order_is_less_than(const input_mapping_t &m1, const input_mapping_t &m2) {
    return m1.specification_order < m2.specification_order;
}

/// Inserts an input mapping at the correct position. We sort them in descending order by length, so
/// that we test longer sequences first.
static void input_mapping_insert_sorted(mapping_list_t &ml, input_mapping_t new_mapping) {
    auto loc = std::lower_bound(ml.begin(), ml.end(), new_mapping, length_is_greater_than);
    ml.insert(loc, std::move(new_mapping));
}

/// Adds an input mapping.
void input_mapping_set_t::add(wcstring sequence, const wchar_t *const *commands,
                              size_t commands_len, const wchar_t *mode, const wchar_t *sets_mode,
                              bool user) {
    assert(commands && mode && sets_mode && "Null parameter");

    // Clear cached mappings.
    all_mappings_cache_.reset();

    // Remove existing mappings with this sequence.
    const wcstring_list_t commands_vector(commands, commands + commands_len);

    mapping_list_t &ml = user ? mapping_list_ : preset_mapping_list_;

    for (input_mapping_t &m : ml) {
        if (m.seq == sequence && m.mode == mode) {
            m.commands = commands_vector;
            m.sets_mode = sets_mode;
            return;
        }
    }

    // Add a new mapping, using the next order.
    input_mapping_t new_mapping =
        input_mapping_t(std::move(sequence), commands_vector, mode, sets_mode);
    input_mapping_insert_sorted(ml, std::move(new_mapping));
}

void input_mapping_set_t::add(wcstring sequence, const wchar_t *command, const wchar_t *mode,
                              const wchar_t *sets_mode, bool user) {
    input_mapping_set_t::add(std::move(sequence), &command, 1, mode, sets_mode, user);
}

static relaxed_atomic_bool_t s_input_initialized{false};

/// Set up arrays used by readch to detect escape sequences for special keys and perform related
/// initializations for our input subsystem.
void init_input() {
    ASSERT_IS_MAIN_THREAD();
    if (s_input_initialized) return;
    s_input_initialized = true;

    s_terminfo_mappings = create_input_terminfo();

    auto input_mapping = input_mappings();

    // If we have no keybindings, add a few simple defaults.
    if (input_mapping->preset_mapping_list_.empty()) {
        input_mapping->add(L"", L"self-insert", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping->add(L"\n", L"execute", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping->add(L"\r", L"execute", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping->add(L"\t", L"complete", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping->add(L"\x3", L"cancel-commandline", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE,
                           false);
        input_mapping->add(L"\x4", L"exit", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping->add(L"\x5", L"bind", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        // ctrl-s
        input_mapping->add(L"\x13", L"pager-toggle-search", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE,
                           false);
        // ctrl-u
        input_mapping->add(L"\x15", L"backward-kill-line", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE,
                           false);
        // del/backspace
        input_mapping->add(L"\x7f", L"backward-delete-char", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE,
                           false);
        // Arrows - can't have functions, so *-or-search isn't available.
        input_mapping->add(L"\x1B[A", L"up-line", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping->add(L"\x1B[B", L"down-line", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping->add(L"\x1B[C", L"forward-char", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping->add(L"\x1B[D", L"backward-char", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE,
                           false);
        // emacs-style ctrl-p/n/b/f
        input_mapping->add(L"\x10", L"up-line", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping->add(L"\x0e", L"down-line", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping->add(L"\x02", L"backward-char", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
        input_mapping->add(L"\x06", L"forward-char", DEFAULT_BIND_MODE, DEFAULT_BIND_MODE, false);
    }
}

inputter_t::inputter_t(parser_t &parser, int in)
    : input_event_queue_t(in), parser_(parser.shared()) {}

void inputter_t::prepare_to_select() /* override */ {
    // Fire any pending events and reap stray processes, including printing exit status messages.
    auto &parser = *this->parser_;
    event_fire_delayed(parser);
    if (job_reap(parser, true)) reader_schedule_prompt_repaint();
}

void inputter_t::select_interrupted() /* override */ {
    // Readline commands may be bound to \cc which also sets the cancel flag.
    // See #6937, #8125.
    signal_clear_cancel();

    // Fire any pending events and reap stray processes, including printing exit status messages.
    auto &parser = *this->parser_;
    event_fire_delayed(parser);
    if (job_reap(parser, true)) reader_schedule_prompt_repaint();

    // Tell the reader an event occurred.
    if (reader_reading_interrupted()) {
        auto vintr = shell_modes.c_cc[VINTR];
        if (vintr != 0) {
            this->push_front(char_event_t{vintr});
        }
        return;
    }
    this->push_front(char_event_t{char_event_type_t::check_exit});
}

void inputter_t::function_push_arg(wchar_t arg) { input_function_args_.push_back(arg); }

wchar_t inputter_t::function_pop_arg() {
    assert(!input_function_args_.empty() && "function_pop_arg underflow");
    auto result = input_function_args_.back();
    input_function_args_.pop_back();
    return result;
}

void inputter_t::function_push_args(readline_cmd_t code) {
    int arity = input_function_arity(code);
    assert(event_storage_.empty() && "event_storage_ should be empty");
    auto &skipped = event_storage_;

    for (int i = 0; i < arity; i++) {
        // Skip and queue up any function codes. See issue #2357.
        wchar_t arg{};
        for (;;) {
            auto evt = this->readch();
            if (evt.is_char()) {
                arg = evt.get_char();
                break;
            }
            skipped.push_back(evt);
        }
        function_push_arg(arg);
    }

    // Push the function codes back into the input stream.
    this->insert_front(skipped.begin(), skipped.end());
    event_storage_.clear();
}

/// Perform the action of the specified binding. allow_commands controls whether fish commands
/// should be executed, or should be deferred until later.
void inputter_t::mapping_execute(const input_mapping_t &m,
                                 const command_handler_t &command_handler) {
    // has_functions: there are functions that need to be put on the input queue
    // has_commands: there are shell commands that need to be evaluated
    bool has_commands = false, has_functions = false;

    for (const wcstring &cmd : m.commands) {
        if (input_function_get_code(cmd)) {
            has_functions = true;
        } else {
            has_commands = true;
        }

        if (has_functions && has_commands) {
            break;
        }
    }

    // !has_functions && !has_commands: only set bind mode
    if (!has_commands && !has_functions) {
        if (!m.sets_mode.empty()) input_set_bind_mode(*parser_, m.sets_mode);
        return;
    }

    if (has_commands && !command_handler) {
        // We don't want to run commands yet. Put the characters back and return check_exit.
        this->insert_front(m.seq.cbegin(), m.seq.cend());
        this->push_front(char_event_type_t::check_exit);
        return;  // skip the input_set_bind_mode
    } else if (has_functions && !has_commands) {
        // Functions are added at the head of the input queue.
        for (auto it = m.commands.rbegin(), end = m.commands.rend(); it != end; ++it) {
            readline_cmd_t code = input_function_get_code(*it).value();
            function_push_args(code);
            this->push_front(char_event_t(code, m.seq));
        }
    } else if (has_commands && !has_functions) {
        // Execute all commands.
        //
        // FIXME(snnw): if commands add stuff to input queue (e.g. commandline -f execute), we won't
        // see that until all other commands have also been run.
        command_handler(m.commands);
        this->push_front(char_event_type_t::check_exit);
    } else {
        // Invalid binding, mixed commands and functions.  We would need to execute these one by
        // one.
        this->push_front(char_event_type_t::check_exit);
    }

    // Empty bind mode indicates to not reset the mode (#2871)
    if (!m.sets_mode.empty()) input_set_bind_mode(*parser_, m.sets_mode);
}

void inputter_t::queue_char(const char_event_t &ch) {
    if (ch.is_readline()) {
        function_push_args(ch.get_readline());
    }
    this->push_back(ch);
}

/// A class which allows accumulating input events, or returns them to the queue.
/// This contains a list of events which have been dequeued, and a current index into that list.
class event_queue_peeker_t {
   public:
    explicit event_queue_peeker_t(input_event_queue_t &event_queue) : event_queue_(event_queue) {}

    /// \return the next event.
    char_event_t next() {
        assert(idx_ <= peeked_.size() && "Index must not be larger than dequeued event count");
        if (idx_ == peeked_.size()) {
            auto event = event_queue_.readch();
            peeked_.push_back(event);
        }
        return peeked_.at(idx_++);
    }

    /// Check if the next event is the given character. This advances the index on success only.
    /// If \p timed is set, then return false if this (or any other) character had a timeout.
    bool next_is_char(wchar_t c, bool timed = false) {
        assert(idx_ <= peeked_.size() && "Index must not be larger than dequeued event count");
        // See if we had a timeout already.
        if (timed && had_timeout_) {
            return false;
        }
        // Grab a new event if we have exhausted what we have already peeked.
        // Use either readch or readch_timed, per our param.
        if (idx_ == peeked_.size()) {
            char_event_t newevt{L'\0'};
            if (!timed) {
                newevt = event_queue_.readch();
            } else if (auto mevt = event_queue_.readch_timed()) {
                newevt = mevt.acquire();
            } else {
                had_timeout_ = true;
                return false;
            }
            peeked_.push_back(newevt);
        }
        // Now we have peeked far enough; check the event.
        // If it matches the char, then increment the index.
        if (peeked_.at(idx_).maybe_char() == c) {
            idx_++;
            return true;
        }
        return false;
    }

    /// \return the current index.
    size_t len() const { return idx_; }

    /// Consume all events up to the current index.
    /// Remaining events are returned to the queue.
    void consume() {
        event_queue_.insert_front(peeked_.cbegin() + idx_, peeked_.cend());
        peeked_.clear();
        idx_ = 0;
    }

    /// Test if any of our peeked events are readline or check_exit.
    bool char_sequence_interrupted() const {
        for (const auto &evt : peeked_) {
            if (evt.is_readline() || evt.is_check_exit()) return true;
        }
        return false;
    }

    /// Reset our index back to 0.
    void restart() { idx_ = 0; }

    ~event_queue_peeker_t() {
        assert(idx_ == 0 && "Events left on the queue - missing restart or consume?");
        consume();
    }

   private:
    /// The list of events which have been dequeued.
    std::vector<char_event_t> peeked_{};

    /// If set, then some previous timed event timed out.
    bool had_timeout_{false};

    /// The current index. This never exceeds peeked_.size().
    size_t idx_{0};

    /// The queue from which to read more events.
    input_event_queue_t &event_queue_;
};

/// Try reading a mouse-tracking CSI sequence, using the given \p peeker.
/// Events are left on the peeker and the caller must restart or consume it.
/// \return true if matched, false if not.
static bool have_mouse_tracking_csi(event_queue_peeker_t *peeker) {
    // Maximum length of any CSI is NPAR (which is nominally 16), although this does not account for
    // user input intermixed with pseudo input generated by the tty emulator.
    // Check for the CSI first.
    if (!peeker->next_is_char(L'\x1b') || !peeker->next_is_char(L'[', true /* timed */)) {
        return false;
    }

    auto next = peeker->next().maybe_char();
    size_t length = 0;
    if (next == L'M') {
        // Generic X10 or modified VT200 sequence. It doesn't matter which, they're both 6 chars
        // (although in mode 1005, the characters may be unicode and not necessarily just one byte
        // long) reporting the button that was clicked and its location.
        length = 6;
    } else if (next == L'<') {
        // Extended (SGR/1006) mouse reporting mode, with semicolon-separated parameters for button
        // code, Px, and Py, ending with 'M' for button press or 'm' for button release.
        while (true) {
            next = peeker->next().maybe_char();
            if (next == L'M' || next == L'm') {
                // However much we've read, we've consumed the CSI in its entirety.
                length = peeker->len();
                break;
            }
            if (peeker->len() >= 16) {
                // This is likely a malformed mouse-reporting CSI but we can't do anything about it.
                return false;
            }
        }
    } else if (next == L't') {
        // VT200 button released in mouse highlighting mode at valid text location. 5 chars.
        length = 5;
    } else if (next == L'T') {
        // VT200 button released in mouse highlighting mode past end-of-line. 9 characters.
        length = 9;
    } else {
        return false;
    }

    // Consume however many characters it takes to prevent the mouse tracking sequence from reaching
    // the prompt, dependent on the class of mouse reporting as detected above.
    while (peeker->len() < length) {
        (void)peeker->next();
    }
    return true;
}

/// \return true if a given \p peeker matches a given sequence of char events given by \p str.
static bool try_peek_sequence(event_queue_peeker_t *peeker, const wcstring &str) {
    assert(!str.empty() && "Empty string passed to try_peek_sequence");
    wchar_t prev = L'\0';
    for (wchar_t c : str) {
        // If we just read an escape, we need to add a timeout for the next char,
        // to distinguish between the actual escape key and an "alt"-modifier.
        bool timed = prev == L'\x1B';
        if (!peeker->next_is_char(c, timed)) {
            return false;
        }
        prev = c;
    }
    return true;
}

/// \return the first mapping that matches, walking first over the user's mapping list, then the
/// preset list.
/// \return none if nothing matches, or if we may have matched a longer sequence but it was
/// interrupted by a readline event.
maybe_t<input_mapping_t> inputter_t::find_mapping(event_queue_peeker_t *peeker) {
    const input_mapping_t *generic = nullptr;
    const auto &vars = parser_->vars();
    const wcstring bind_mode = input_get_bind_mode(vars);
    const input_mapping_t *escape = nullptr;

    auto ml = input_mappings()->all_mappings();
    for (const auto &m : *ml) {
        if (m.mode != bind_mode) {
            continue;
        }

        // Defer generic mappings until the end.
        if (m.is_generic()) {
            if (!generic) generic = &m;
            continue;
        }

        if (try_peek_sequence(peeker, m.seq)) {
            // A binding for just escape should also be deferred
            // so escape sequences take precedence.
            if (m.seq == L"\x1B") {
                if (!escape) {
                    escape = &m;
                }
            } else {
                return m;
            }
        }
        peeker->restart();
    }

    if (peeker->char_sequence_interrupted()) {
        // We might have matched a longer sequence, but we were interrupted, e.g. by a signal.
        FLOG(reader, "torn sequence, rearranging events");
        return none();
    }

    if (escape) {
        // We need to reconsume the escape.
        peeker->next();
        return *escape;
    }

    return generic ? maybe_t<input_mapping_t>(*generic) : none();
}

void inputter_t::mapping_execute_matching_or_generic(const command_handler_t &command_handler) {
    event_queue_peeker_t peeker(*this);

    // Check for mouse-tracking CSI before mappings to prevent the generic mapping handler from
    // taking over.
    if (have_mouse_tracking_csi(&peeker)) {
        // fish recognizes but does not actually support mouse reporting. We never turn it on, and
        // it's only ever enabled if a program we spawned enabled it and crashed or forgot to turn
        // it off before exiting. We swallow the events to prevent garbage from piling up at the
        // prompt, but don't do anything further with the received codes. To prevent this from
        // breaking user interaction with the tty emulator, wasting CPU, and adding latency to the
        // event queue, we turn off mouse reporting here.
        //
        // Since this is only called when we detect an incoming mouse reporting payload, we know the
        // terminal emulator supports the xterm ANSI extensions for mouse reporting and can safely
        // issue this without worrying about termcap.
        FLOGF(reader, "Disabling mouse tracking");

        // We can't/shouldn't directly manipulate stdout from `input.cpp`, so request the execution
        // of a helper function to disable mouse tracking.
        // writembs(outputter_t::stdoutput(), "\x1B[?1000l");
        peeker.consume();
        this->push_front(char_event_t(readline_cmd_t::disable_mouse_tracking, L""));
        return;
    }
    peeker.restart();

    // Check for ordinary mappings.
    if (auto mapping = find_mapping(&peeker)) {
        peeker.consume();
        mapping_execute(*mapping, command_handler);
        return;
    }
    peeker.restart();

    if (peeker.char_sequence_interrupted()) {
        // This may happen if we received a signal in the middle of an escape sequence or other
        // multi-char binding. Move these non-char events to the front of the queue, handle them
        // first, and then later we'll return and try the sequence again. See #8628.
        peeker.consume();
        this->promote_interruptions_to_front();
        return;
    }

    FLOGF(reader, L"no generic found, ignoring char...");
    peeker.next();
    peeker.consume();
}

/// Helper function. Picks through the queue of incoming characters until we get to one that's not a
/// readline function.
char_event_t inputter_t::read_characters_no_readline() {
    assert(event_storage_.empty() && "saved_events_storage should be empty");
    auto &saved_events = event_storage_;

    char_event_t evt_to_return{0};
    for (;;) {
        auto evt = this->readch();
        if (evt.is_readline()) {
            saved_events.push_back(evt);
        } else {
            evt_to_return = evt;
            break;
        }
    }

    // Restore any readline functions
    this->insert_front(saved_events.cbegin(), saved_events.cend());
    event_storage_.clear();
    return evt_to_return;
}

char_event_t inputter_t::read_char(const command_handler_t &command_handler) {
    // Clear the interrupted flag.
    reader_reset_interrupted();
    // Search for sequence in mapping tables.
    while (true) {
        auto evt = this->readch();

        if (evt.is_readline()) {
            switch (evt.get_readline()) {
                case readline_cmd_t::self_insert:
                case readline_cmd_t::self_insert_notfirst: {
                    // Typically self-insert is generated by the generic (empty) binding.
                    // However if it is generated by a real sequence, then insert that sequence.
                    this->insert_front(evt.seq.cbegin(), evt.seq.cend());
                    // Issue #1595: ensure we only insert characters, not readline functions. The
                    // common case is that this will be empty.
                    char_event_t res = read_characters_no_readline();

                    // Hackish: mark the input style.
                    res.input_style = evt.get_readline() == readline_cmd_t::self_insert_notfirst
                                          ? char_input_style_t::notfirst
                                          : char_input_style_t::normal;
                    return res;
                }
                case readline_cmd_t::func_and:
                case readline_cmd_t::func_or: {
                    // If previous function has right status, we keep reading tokens
                    if (evt.get_readline() == readline_cmd_t::func_and) {
                        if (function_status_) return readch();
                    } else {
                        assert(evt.get_readline() == readline_cmd_t::func_or);
                        if (!function_status_) return readch();
                    }
                    // Else we flush remaining tokens
                    do {
                        evt = this->readch();
                    } while (evt.is_readline());
                    this->push_front(evt);
                    return readch();
                }
                default: {
                    return evt;
                }
            }
        } else if (evt.is_eof()) {
            // If we have EOF, we need to immediately quit.
            // There's no need to go through the input functions.
            return evt;
        } else if (evt.is_check_exit()) {
            // Allow the reader to check for exit conditions.
            return evt;
        } else {
            assert(evt.is_char() && "Should be char event");
            this->push_front(evt);
            mapping_execute_matching_or_generic(command_handler);
            // Regarding allow_commands, we're in a loop, but if a fish command is executed,
            // check_exit is unread, so the next pass through the loop we'll break out and return
            // it.
        }
    }
}

std::vector<input_mapping_name_t> input_mapping_set_t::get_names(bool user) const {
    // Sort the mappings by the user specification order, so we can return them in the same order
    // that the user specified them in.
    std::vector<input_mapping_t> local_list = user ? mapping_list_ : preset_mapping_list_;
    std::sort(local_list.begin(), local_list.end(), specification_order_is_less_than);
    std::vector<input_mapping_name_t> result;
    result.reserve(local_list.size());

    for (const auto &m : local_list) {
        result.push_back((input_mapping_name_t){m.seq, m.mode});
    }
    return result;
}

void input_mapping_set_t::clear(const wchar_t *mode, bool user) {
    all_mappings_cache_.reset();
    mapping_list_t &ml = user ? mapping_list_ : preset_mapping_list_;
    auto should_erase = [=](const input_mapping_t &m) { return mode == nullptr || mode == m.mode; };
    ml.erase(std::remove_if(ml.begin(), ml.end(), should_erase), ml.end());
}

bool input_mapping_set_t::erase(const wcstring &sequence, const wcstring &mode, bool user) {
    // Clear cached mappings.
    all_mappings_cache_.reset();

    bool result = false;
    mapping_list_t &ml = user ? mapping_list_ : preset_mapping_list_;
    for (auto it = ml.begin(), end = ml.end(); it != end; ++it) {
        if (sequence == it->seq && mode == it->mode) {
            ml.erase(it);
            result = true;
            break;
        }
    }
    return result;
}

bool input_mapping_set_t::get(const wcstring &sequence, const wcstring &mode,
                              wcstring_list_t *out_cmds, bool user, wcstring *out_sets_mode) const {
    bool result = false;
    const auto &ml = user ? mapping_list_ : preset_mapping_list_;
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

std::shared_ptr<const mapping_list_t> input_mapping_set_t::all_mappings() {
    // Populate the cache if needed.
    if (!all_mappings_cache_) {
        mapping_list_t all_mappings = mapping_list_;
        all_mappings.insert(all_mappings.end(), preset_mapping_list_.begin(),
                            preset_mapping_list_.end());
        all_mappings_cache_ = std::make_shared<const mapping_list_t>(std::move(all_mappings));
    }
    return all_mappings_cache_;
}

/// Create a list of terminfo mappings.
static std::vector<terminfo_mapping_t> create_input_terminfo() {
    assert(curses_initialized);
    if (!cur_term) return {};  // setupterm() failed so we can't referency any key definitions

#define TERMINFO_ADD(key) \
    { (L## #key) + 4, key }

    return {
        TERMINFO_ADD(key_a1), TERMINFO_ADD(key_a3), TERMINFO_ADD(key_b2),
        TERMINFO_ADD(key_backspace), TERMINFO_ADD(key_beg), TERMINFO_ADD(key_btab),
        TERMINFO_ADD(key_c1), TERMINFO_ADD(key_c3), TERMINFO_ADD(key_cancel),
        TERMINFO_ADD(key_catab), TERMINFO_ADD(key_clear), TERMINFO_ADD(key_close),
        TERMINFO_ADD(key_command), TERMINFO_ADD(key_copy), TERMINFO_ADD(key_create),
        TERMINFO_ADD(key_ctab), TERMINFO_ADD(key_dc), TERMINFO_ADD(key_dl), TERMINFO_ADD(key_down),
        TERMINFO_ADD(key_eic), TERMINFO_ADD(key_end), TERMINFO_ADD(key_enter),
        TERMINFO_ADD(key_eol), TERMINFO_ADD(key_eos), TERMINFO_ADD(key_exit), TERMINFO_ADD(key_f0),
        TERMINFO_ADD(key_f1), TERMINFO_ADD(key_f2), TERMINFO_ADD(key_f3), TERMINFO_ADD(key_f4),
        TERMINFO_ADD(key_f5), TERMINFO_ADD(key_f6), TERMINFO_ADD(key_f7), TERMINFO_ADD(key_f8),
        TERMINFO_ADD(key_f9), TERMINFO_ADD(key_f10), TERMINFO_ADD(key_f11), TERMINFO_ADD(key_f12),
        TERMINFO_ADD(key_f13), TERMINFO_ADD(key_f14), TERMINFO_ADD(key_f15), TERMINFO_ADD(key_f16),
        TERMINFO_ADD(key_f17), TERMINFO_ADD(key_f18), TERMINFO_ADD(key_f19), TERMINFO_ADD(key_f20),
        // Note key_f21 through key_f63 are available but no actual keyboard supports them.
        TERMINFO_ADD(key_find), TERMINFO_ADD(key_help), TERMINFO_ADD(key_home),
        TERMINFO_ADD(key_ic), TERMINFO_ADD(key_il), TERMINFO_ADD(key_left), TERMINFO_ADD(key_ll),
        TERMINFO_ADD(key_mark), TERMINFO_ADD(key_message), TERMINFO_ADD(key_move),
        TERMINFO_ADD(key_next), TERMINFO_ADD(key_npage), TERMINFO_ADD(key_open),
        TERMINFO_ADD(key_options), TERMINFO_ADD(key_ppage), TERMINFO_ADD(key_previous),
        TERMINFO_ADD(key_print), TERMINFO_ADD(key_redo), TERMINFO_ADD(key_reference),
        TERMINFO_ADD(key_refresh), TERMINFO_ADD(key_replace), TERMINFO_ADD(key_restart),
        TERMINFO_ADD(key_resume), TERMINFO_ADD(key_right), TERMINFO_ADD(key_save),
        TERMINFO_ADD(key_sbeg), TERMINFO_ADD(key_scancel), TERMINFO_ADD(key_scommand),
        TERMINFO_ADD(key_scopy), TERMINFO_ADD(key_screate), TERMINFO_ADD(key_sdc),
        TERMINFO_ADD(key_sdl), TERMINFO_ADD(key_select), TERMINFO_ADD(key_send),
        TERMINFO_ADD(key_seol), TERMINFO_ADD(key_sexit), TERMINFO_ADD(key_sf),
        TERMINFO_ADD(key_sfind), TERMINFO_ADD(key_shelp), TERMINFO_ADD(key_shome),
        TERMINFO_ADD(key_sic), TERMINFO_ADD(key_sleft), TERMINFO_ADD(key_smessage),
        TERMINFO_ADD(key_smove), TERMINFO_ADD(key_snext), TERMINFO_ADD(key_soptions),
        TERMINFO_ADD(key_sprevious), TERMINFO_ADD(key_sprint), TERMINFO_ADD(key_sr),
        TERMINFO_ADD(key_sredo), TERMINFO_ADD(key_sreplace), TERMINFO_ADD(key_sright),
        TERMINFO_ADD(key_srsume), TERMINFO_ADD(key_ssave), TERMINFO_ADD(key_ssuspend),
        TERMINFO_ADD(key_stab), TERMINFO_ADD(key_sundo), TERMINFO_ADD(key_suspend),
        TERMINFO_ADD(key_undo), TERMINFO_ADD(key_up),

        // We introduce our own name for the string containing only the nul character - see
        // #3189. This can typically be generated via control-space.
        terminfo_mapping_t(k_nul_mapping_name, std::string{'\0'})};
#undef TERMINFO_ADD
}

bool input_terminfo_get_sequence(const wcstring &name, wcstring *out_seq) {
    assert(s_input_initialized);
    for (const terminfo_mapping_t &m : *s_terminfo_mappings) {
        if (name == m.name) {
            // Found the mapping.
            if (!m.seq) {
                errno = EILSEQ;
                return false;
            } else {
                *out_seq = str2wcstring(*m.seq);
                return true;
            }
        }
    }
    errno = ENOENT;
    return false;
}

bool input_terminfo_get_name(const wcstring &seq, wcstring *out_name) {
    assert(s_input_initialized);

    for (const terminfo_mapping_t &m : *s_terminfo_mappings) {
        if (m.seq && seq == str2wcstring(*m.seq)) {
            out_name->assign(m.name);
            return true;
        }
    }

    return false;
}

wcstring_list_t input_terminfo_get_names(bool skip_null) {
    assert(s_input_initialized);

    wcstring_list_t result;
    const auto &mappings = *s_terminfo_mappings;
    result.reserve(mappings.size());
    for (const terminfo_mapping_t &m : mappings) {
        if (skip_null && !m.seq) {
            continue;
        }
        result.emplace_back(m.name);
    }
    return result;
}

const wcstring_list_t &input_function_get_names() {
    // The list and names of input functions are hard-coded and never change
    static wcstring_list_t result = ([&]() {
        wcstring_list_t result;
        result.reserve(input_function_count);
        for (const auto &md : input_function_metadata) {
            if (md.name[0]) {
                result.push_back(md.name);
            }
        }
        return result;
    })();

    return result;
}

maybe_t<readline_cmd_t> input_function_get_code(const wcstring &name) {
    // `input_function_metadata` is required to be kept in asciibetical order, making it OK to do
    // a binary search for the matching name.
    if (const input_function_metadata_t *md = get_by_sorted_name(name, input_function_metadata)) {
        return md->code;
    }
    return none();
}
