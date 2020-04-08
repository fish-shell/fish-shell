// Functions for reading data from stdin and passing to the parser. If stdin is a keyboard, it
// supplies a killring, history, syntax highlighting, tab-completion and various other interactive
// features.
//
// Internally the interactive mode functions rely in the functions of the input library to read
// individual characters of input.
//
// Token search is handled incrementally. Actual searches are only done on when searching backwards,
// since the previous results are saved. The last search position is remembered and a new search
// continues from the last search position. All search results are saved in the list 'search_prev'.
// When the user searches forward, i.e. presses Alt-down, the list is consulted for previous search
// result, and subsequent backwards searches are also handled by consulting the list up until the
// end of the list is reached, at which point regular searching will commence.
#include "config.h"

// IWYU pragma: no_include <type_traits>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstring>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <wctype.h>

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cwchar>
#include <deque>
#include <functional>
#include <memory>
#include <set>
#include <stack>

#include "color.h"
#include "common.h"
#include "complete.h"
#include "env.h"
#include "event.h"
#include "exec.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "flog.h"
#include "function.h"
#include "global_safety.h"
#include "highlight.h"
#include "history.h"
#include "input.h"
#include "input_common.h"
#include "intern.h"
#include "io.h"
#include "iothread.h"
#include "kill.h"
#include "output.h"
#include "pager.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "parser.h"
#include "proc.h"
#include "reader.h"
#include "sanity.h"
#include "screen.h"
#include "signal.h"
#include "tnode.h"
#include "tokenizer.h"
#include "wutil.h"  // IWYU pragma: keep

// Name of the variable that tells how long it took, in milliseconds, for the previous
// interactive command to complete.
#define ENV_CMD_DURATION L"CMD_DURATION"

/// Maximum length of prefix string when printing completion list. Longer prefixes will be
/// ellipsized.
#define PREFIX_MAX_LEN 9

/// A simple prompt for reading shell commands that does not rely on fish specific commands, meaning
/// it will work even if fish is not installed. This is used by read_i.
#define DEFAULT_PROMPT L"echo -n \"$USER@$hostname $PWD \"'> '"

/// The name of the function that prints the fish prompt.
#define LEFT_PROMPT_FUNCTION_NAME L"fish_prompt"

/// The name of the function that prints the fish right prompt (RPROMPT).
#define RIGHT_PROMPT_FUNCTION_NAME L"fish_right_prompt"

/// The name of the function to use in place of the left prompt if we're in the debugger context.
#define DEBUG_PROMPT_FUNCTION_NAME L"fish_breakpoint_prompt"

/// The name of the function for getting the input mode indicator.
#define MODE_PROMPT_FUNCTION_NAME L"fish_mode_prompt"

/// The default title for the reader. This is used by reader_readline.
#define DEFAULT_TITLE L"echo (status current-command) \" \" $PWD"

/// The maximum number of characters to read from the keyboard without repainting. Note that this
/// readahead will only occur if new characters are available for reading, fish will never block for
/// more input without repainting.
static constexpr size_t READAHEAD_MAX = 256;

/// A mode for calling the reader_kill function. In this mode, the new string is appended to the
/// current contents of the kill buffer.
#define KILL_APPEND 0

/// A mode for calling the reader_kill function. In this mode, the new string is prepended to the
/// current contents of the kill buffer.
#define KILL_PREPEND 1

enum class history_search_direction_t { forward, backward };

enum class jump_direction_t { forward, backward };
enum class jump_precision_t { till, to };

/// Any time the contents of a buffer changes, we update the generation count. This allows for our
/// background threads to notice it and skip doing work that they would otherwise have to do.
static std::atomic<unsigned> s_generation;

/// Helper to get the generation count
static inline unsigned read_generation_count() {
    return s_generation.load(std::memory_order_relaxed);
}

/// \return an operation context for a background operation..
/// Crucially the operation context itself does not contain a parser.
/// It is the caller's responsibility to ensure the environment lives as long as the result.
operation_context_t get_bg_context(const std::shared_ptr<environment_t> &env,
                                   unsigned int generation_count) {
    cancel_checker_t cancel_checker = [generation_count] {
        // Cancel if the generation count changed.
        return generation_count != read_generation_count();
    };
    return operation_context_t{nullptr, *env, std::move(cancel_checker)};
}

/// Get the debouncer for autosuggestions and background highlighting.
/// These are deliberately leaked to avoid shutdown dtor registration.
static debounce_t &debounce_autosuggestions() {
    const long kAutosuggetTimeoutMs = 500;
    static auto res = new debounce_t(kAutosuggetTimeoutMs);
    return *res;
}

static debounce_t &debounce_highlighting() {
    const long kHighlightTimeoutMs = 500;
    static auto res = new debounce_t(kHighlightTimeoutMs);
    return *res;
}

bool edit_t::operator==(const edit_t &other) const {
    return cursor_position_before_edit == other.cursor_position_before_edit &&
           offset == other.offset && length == other.length && old == other.old &&
           replacement == other.replacement;
}

void undo_history_t::clear() {
    edits.clear();
    edits_applied = 0;
    may_coalesce = false;
}

void apply_edit(wcstring *target, const edit_t &edit) {
    target->replace(edit.offset, edit.length, edit.replacement);
}

/// Returns the number of characters left of the cursor that are removed by the
/// deletion in the given edit.
static size_t chars_deleted_left_of_cursor(const edit_t &edit) {
    if (edit.cursor_position_before_edit > edit.offset) {
        return std::min(edit.length, edit.cursor_position_before_edit - edit.offset);
    }
    return 0;
}

/// Compute the position of the cursor after the given edit.
static size_t cursor_position_after_edit(const edit_t &edit) {
    size_t cursor = edit.cursor_position_before_edit + edit.replacement.size();
    size_t removed = chars_deleted_left_of_cursor(edit);
    return cursor > removed ? cursor - removed : 0;
}

/// Whether we want to append this string to the previous edit.
static bool want_to_coalesce_insertion_of(const editable_line_t &el, const wcstring &str) {
    // The previous edit must support coalescing.
    if (!el.undo_history.may_coalesce) return false;
    // Only consolidate single character inserts.
    if (str.size() != 1) return false;
    // Make an undo group after every space.
    if (str.at(0) == L' ') return false;
    assert(!el.undo_history.edits.empty());
    const edit_t &last_edit = el.undo_history.edits.back();
    // Don't add to the last edit if it deleted something.
    if (last_edit.length != 0) return false;
    // Must not have moved the cursor!
    if (cursor_position_after_edit(last_edit) != el.position()) return false;
    return true;
}

void editable_line_t::insert_string(const wcstring &str, size_t start, size_t len) {
    // Clamp the range to something valid.
    size_t string_length = str.size();
    start = std::min(start, string_length);      //!OCLINT(parameter reassignment)
    len = std::min(len, string_length - start);  //!OCLINT(parameter reassignment)
    if (want_to_coalesce_insertion_of(*this, str)) {
        edit_t &edit = undo_history.edits.back();
        edit.replacement.append(str);
        apply_edit(&text_, edit_t(position(), 0, str));
        set_position(position() + len);
    } else {
        push_edit(edit_t(position(), 0, str.substr(start, len)));
    }
    undo_history.may_coalesce = (str.size() == 1);
}

void editable_line_t::erase_substring(size_t offset, size_t length) {
    push_edit(edit_t(offset, length, L""));
    undo_history.may_coalesce = false;
}

void editable_line_t::replace_substring(size_t offset, size_t length, wcstring &&replacement) {
    push_edit(edit_t(offset, length, replacement));
    undo_history.may_coalesce = false;
}

bool editable_line_t::undo() {
    if (undo_history.edits_applied == 0) return false;  // nothing to undo
    const edit_t &edit = undo_history.edits.at(undo_history.edits_applied - 1);
    undo_history.edits_applied--;
    edit_t inverse = edit_t(edit.offset, edit.replacement.size(), L"");
    inverse.replacement = edit.old;
    size_t old_position = edit.cursor_position_before_edit;
    apply_edit(&text_, inverse);
    set_position(old_position);
    undo_history.may_coalesce = false;
    return true;
}

void editable_line_t::push_edit(edit_t &&edit) {
    bool edit_does_nothing = edit.length == 0 && edit.replacement.empty();
    if (edit_does_nothing) return;
    if (undo_history.edits_applied != undo_history.edits.size()) {
        // After undoing some edits, the user is making a new edit;
        // we are about to create a new edit branch.
        // Discard all edits that were undone because we only support
        // linear undo/redo, they will be unreachable.
        undo_history.edits.erase(undo_history.edits.begin() + undo_history.edits_applied,
                                 undo_history.edits.end());
    }
    edit.cursor_position_before_edit = position();
    edit.old = text_.substr(edit.offset, edit.length);
    apply_edit(&text_, edit);
    set_position(cursor_position_after_edit(edit));
    assert(undo_history.edits_applied == undo_history.edits.size());
    undo_history.edits_applied++;
    undo_history.edits.emplace_back(edit);
}

bool editable_line_t::redo() {
    if (undo_history.edits_applied >= undo_history.edits.size()) return false;  // nothing to redo
    const edit_t &edit = undo_history.edits.at(undo_history.edits_applied);
    undo_history.edits_applied++;
    apply_edit(&text_, edit);
    set_position(cursor_position_after_edit(edit));
    undo_history.may_coalesce = false;  // Make a new undo group here.
    return true;
}

namespace {

/// Test if the given string contains error. Since this is the error detection for general purpose,
/// there are no invalid strings, so this function always returns false.
parser_test_error_bits_t default_test(parser_t &parser, const wcstring &b) {
    UNUSED(parser);
    UNUSED(b);
    return 0;
}

/// Encapsulation of the reader's history search functionality.
class reader_history_search_t {
   public:
    enum mode_t {
        inactive,  // no search
        line,      // searching by line
        prefix,    // searching by prefix
        token      // searching by token
    };

   private:
    /// The type of search performed.
    mode_t mode_{inactive};

    /// Our history search itself.
    history_search_t search_;

    /// The ordered list of matches. This may grow long.
    std::deque<wcstring> matches_;

    /// A set of new items to skip, corresponding to matches_ and anything added in skip().
    std::set<wcstring> skips_;

    /// Index into our matches list.
    size_t match_index_{0};

    /// Adds the given match if we haven't seen it before.
    void add_if_new(wcstring text) {
        if (add_skip(text)) {
            matches_.push_back(std::move(text));
        }
    }

    /// Attempt to append matches from the current history item.
    /// \return true if something was appended.
    bool append_matches_from_search() {
        const size_t before = matches_.size();
        wcstring text = search_.current_string();
        if (mode_ == line || mode_ == prefix) {
            add_if_new(std::move(text));
        } else if (mode_ == token) {
            const wcstring &needle = search_string();
            tokenizer_t tok(text.c_str(), TOK_ACCEPT_UNFINISHED);

            wcstring_list_t local_tokens;
            while (auto token = tok.next()) {
                if (token->type != token_type_t::string) continue;
                wcstring text = tok.text_of(*token);
                if (text.find(needle) != wcstring::npos) {
                    local_tokens.emplace_back(std::move(text));
                }
            }

            // Make sure tokens are added in reverse order. See #5150
            for (auto i = local_tokens.rbegin(); i != local_tokens.rend(); ++i) {
                add_if_new(std::move(*i));
            }
        }
        return matches_.size() > before;
    }

    bool move_forwards() {
        // Try to move within our previously discovered matches.
        if (match_index_ > 0) {
            match_index_--;
            return true;
        }
        return false;
    }

    bool move_backwards() {
        // Try to move backwards within our previously discovered matches.
        if (match_index_ + 1 < matches_.size()) {
            match_index_++;
            return true;
        }

        // Add more items from our search.
        while (search_.go_backwards()) {
            if (append_matches_from_search()) {
                match_index_++;
                assert(match_index_ < matches_.size() && "Should have found more matches");
                return true;
            }
        }

        // Here we failed to go backwards past the last history item.
        return false;
    }

   public:
    reader_history_search_t() = default;
    ~reader_history_search_t() = default;

    bool active() const { return mode_ != inactive; }

    bool by_token() const { return mode_ == token; }

    bool by_line() const { return mode_ == line; }

    bool by_prefix() const { return mode_ == prefix; }

    /// Move the history search in the given direction \p dir.
    bool move_in_direction(history_search_direction_t dir) {
        return dir == history_search_direction_t::forward ? move_forwards() : move_backwards();
    }

    /// Go to the beginning (earliest) of the search.
    void go_to_beginning() {
        while (move_forwards())
            ;
    }

    /// Go to the end (most recent) of the search.
    void go_to_end() { match_index_ = 0; }

    /// \return the current search result.
    const wcstring &current_result() const {
        assert(match_index_ < matches_.size() && "Invalid match index");
        return matches_.at(match_index_);
    }

    /// \return the string we are searching for.
    const wcstring &search_string() const { return search_.original_term(); }

    /// \return whether we are at the end (most recent) of our search.
    bool is_at_end() const { return match_index_ == 0; }

    // Add an item to skip.
    // \return true if it was added, false if already present.
    bool add_skip(const wcstring &str) { return skips_.insert(str).second; }

    /// Reset, beginning a new line or token mode search.
    void reset_to_mode(const wcstring &text, history_t *hist, mode_t mode) {
        assert(mode != inactive && "mode cannot be inactive in this setter");
        skips_ = {text};
        matches_ = {text};
        match_index_ = 0;
        mode_ = mode;
        // We can skip dedup in history_search_t because we do it ourselves in skips_.
        search_ = history_search_t(
            *hist, text,
            by_prefix() ? history_search_type_t::prefix : history_search_type_t::contains,
            history_search_no_dedup);
    }

    /// Reset to inactive search.
    void reset() {
        matches_.clear();
        skips_.clear();
        match_index_ = 0;
        mode_ = inactive;
        search_ = history_search_t();
    }
};

struct autosuggestion_result_t {
    wcstring suggestion;
    wcstring search_string;
};

struct highlight_result_t {
    std::vector<highlight_spec_t> colors;
    wcstring text;
};

}  // namespace

struct readline_loop_state_t;

/// A struct describing the state of the interactive reader. These states can be stacked, in case
/// reader_readline() calls are nested. This happens when the 'read' builtin is used.
class reader_data_t : public std::enable_shared_from_this<reader_data_t> {
   public:
    /// The parser being used.
    std::shared_ptr<parser_t> parser_ref;
    /// String containing the whole current commandline.
    editable_line_t command_line;
    /// Whether the most recent modification to the command line was done by either history search
    /// or a pager selection change. When this is true and another transient change is made, the
    /// old transient change will be removed from the undo history.
    bool command_line_has_transient_edit = false;
    /// String containing the autosuggestion.
    wcstring autosuggestion;
    /// Current pager.
    pager_t pager;
    /// Current page rendering.
    page_rendering_t current_page_rendering;
    /// Whether autosuggesting is allowed at all.
    bool allow_autosuggestion{false};
    /// When backspacing, we temporarily suppress autosuggestions.
    bool suppress_autosuggestion{false};
    /// Whether abbreviations are expanded.
    bool expand_abbreviations{false};
    /// Silent mode used for password input on the read command
    bool silent{false};
    /// The representation of the current screen contents.
    screen_t screen;
    /// The source of input events.
    inputter_t inputter;
    /// The history.
    history_t *history{nullptr};
    /// The history search.
    reader_history_search_t history_search{};
    /// Indicates whether a selection is currently active.
    bool sel_active{false};
    /// The position of the cursor, when selection was initiated.
    size_t sel_begin_pos{0};
    /// The start position of the current selection, if one.
    size_t sel_start_pos{0};
    /// The stop position of the current selection, if one.
    size_t sel_stop_pos{0};
    /// The prompt commands.
    wcstring left_prompt;
    wcstring right_prompt;
    /// The output of the last evaluation of the prompt command.
    wcstring left_prompt_buff;
    wcstring mode_prompt_buff;
    /// The output of the last evaluation of the right prompt command.
    wcstring right_prompt_buff;
    /// Completion support.
    wcstring cycle_command_line;
    size_t cycle_cursor_pos{0};
    /// Color is the syntax highlighting for buff.  The format is that color[i] is the
    /// classification (according to the enum in highlight.h) of buff[i].
    std::vector<highlight_spec_t> colors;
    /// An array defining the block level at each character.
    std::vector<int> indents;
    /// Whether tab completion is allowed.
    bool complete_ok{false};
    /// Function for syntax highlighting.
    highlight_function_t highlight_func{highlight_universal};
    /// Function for testing if the string can be returned.
    test_function_t test_func{default_test};
    /// If this is true, exit reader even if there are running jobs. This happens if we press e.g.
    /// ^D twice.
    bool prev_end_loop{false};
    /// The current contents of the top item in the kill ring.
    wcstring kill_item;
    /// Keep track of whether any internal code has done something which is known to require a
    /// repaint.
    bool repaint_needed{false};
    /// Whether a screen reset is needed after a repaint.
    bool screen_reset_needed{false};
    /// Whether the reader should exit on ^C.
    bool exit_on_interrupt{false};
    /// The target character of the last jump command.
    wchar_t last_jump_target{0};
    jump_direction_t last_jump_direction{jump_direction_t::forward};
    jump_precision_t last_jump_precision{jump_precision_t::to};

    bool is_navigating_pager_contents() const { return this->pager.is_navigating_contents(); }

    /// The line that is currently being edited. Typically the command line, but may be the search
    /// field.
    const editable_line_t *active_edit_line() const {
        if (this->is_navigating_pager_contents() && this->pager.is_search_field_shown()) {
            return &this->pager.search_field_line;
        }
        return &this->command_line;
    }

    editable_line_t *active_edit_line() {
        auto cthis = reinterpret_cast<const reader_data_t *>(this);
        return const_cast<editable_line_t *>(cthis->active_edit_line());
    }

    /// Do what we need to do whenever our command line changes.
    void command_line_changed(const editable_line_t *el);

    /// Do what we need to do whenever our pager selection changes.
    void pager_selection_changed();

    /// Expand abbreviations at the current cursor position, minus backtrack_amt.
    bool expand_abbreviation_as_necessary(size_t cursor_backtrack);

    void repaint_if_needed();

    /// Return the variable set used for e.g. command duration.
    env_stack_t &vars() { return parser_ref->vars(); }
    const env_stack_t &vars() const { return parser_ref->vars(); }

    /// Access the parser.
    parser_t &parser() { return *parser_ref; }

    reader_data_t(std::shared_ptr<parser_t> parser, history_t *hist)
        : parser_ref(std::move(parser)), inputter(*parser_ref), history(hist) {}

    void update_buff_pos(editable_line_t *el, maybe_t<size_t> new_pos = none_t());
    void repaint();
    void kill(editable_line_t *el, size_t begin_idx, size_t length, int mode, int newv);
    bool insert_string(editable_line_t *el, const wcstring &str);

    /// Insert the character into the command line buffer and print it to the screen using syntax
    /// highlighting, etc.
    bool insert_char(editable_line_t *el, wchar_t c) { return insert_string(el, wcstring{c}); }

    void move_word(editable_line_t *el, bool move_right, bool erase, enum move_word_style_t style,
                   bool newv);

    maybe_t<wcstring> readline(int nchars);
    maybe_t<char_event_t> read_normal_chars(readline_loop_state_t &rls);
    void handle_readline_command(readline_cmd_t cmd, readline_loop_state_t &rls);

    void clear_pager();
    void select_completion_in_direction(selection_motion_t dir);
    void flash();

    void mark_repaint_needed() { repaint_needed = true; }

    void completion_insert(const wchar_t *val, size_t token_end, complete_flags_t flags);

    bool can_autosuggest() const;
    void autosuggest_completed(autosuggestion_result_t result);
    void update_autosuggestion();
    void accept_autosuggestion(bool full, move_word_style_t style = move_word_style_punctuation);
    void super_highlight_me_plenty(int highlight_pos_adjust = 0, bool no_io = false);

    void highlight_search();
    void highlight_complete(highlight_result_t result);
    void exec_mode_prompt();
    void exec_prompt();

    bool jump(jump_direction_t dir, jump_precision_t precision, editable_line_t *el,
              wchar_t target);

    bool handle_completions(const completion_list_t &comp, size_t token_begin, size_t token_end);

    void sanity_check() const;
    void set_command_line_and_position(editable_line_t *el, wcstring &&new_str, size_t pos);
    void clear_transient_edit();
    void replace_current_token(wcstring &&new_token);
    void update_command_line_from_history_search();
    void set_buffer_maintaining_pager(const wcstring &b, size_t pos, bool transient = false);
    void delete_char(bool backward = true);
};

/// This variable is set to a signal by the signal handler when ^C is pressed.
static volatile sig_atomic_t interrupted = 0;

// Prototypes for a bunch of functions defined later on.
static bool is_backslashed(const wcstring &str, size_t pos);
static wchar_t unescaped_quote(const wcstring &str, size_t pos);

/// Mode on startup, which we restore on exit.
static struct termios terminal_mode_on_startup;

/// Mode we use to execute programs.
static struct termios tty_modes_for_external_cmds;

/// Tracks a currently pending exit. This may be manipulated from a signal handler.

/// Whether we should exit the current reader loop.
static relaxed_atomic_bool_t s_end_current_loop{false};

/// Whether we should exit all reader loops. This is set in response to a HUP signal and it
/// latches (once set it is never cleared). This should never be reset to false.
static volatile sig_atomic_t s_exit_forced{false};

static bool should_exit() { return s_end_current_loop || s_exit_forced; }

/// Give up control of terminal.
static void term_donate(outputter_t &outp) {
    outp.set_color(rgb_color_t::normal(), rgb_color_t::normal());

    while (true) {
        if (tcsetattr(STDIN_FILENO, TCSANOW, &tty_modes_for_external_cmds) == -1) {
            if (errno == EIO) redirect_tty_output();
            if (errno != EINTR) {
                FLOGF(warning, _(L"Could not set terminal mode for new job"));
                wperror(L"tcsetattr");
                break;
            }
        } else
            break;
    }
}

/// Grab control of terminal.
static void term_steal() {
    while (true) {
        if (tcsetattr(STDIN_FILENO, TCSANOW, &shell_modes) == -1) {
            if (errno == EIO) redirect_tty_output();
            if (errno != EINTR) {
                FLOGF(warning, _(L"Could not set terminal mode for shell"));
                perror("tcsetattr");
                break;
            }
        } else
            break;
    }

    invalidate_termsize();
}

bool reader_exit_forced() { return s_exit_forced; }

/// Given a command line and an autosuggestion, return the string that gets shown to the user.
wcstring combine_command_and_autosuggestion(const wcstring &cmdline,
                                            const wcstring &autosuggestion) {
    // We want to compute the full line, containing the command line and the autosuggestion They may
    // disagree on whether characters are uppercase or lowercase Here we do something funny: if the
    // last token of the command line contains any uppercase characters, we use its case. Otherwise
    // we use the case of the autosuggestion. This is an idea from issue #335.
    wcstring full_line;
    if (autosuggestion.size() <= cmdline.size() || cmdline.empty()) {
        // No or useless autosuggestion, or no command line.
        full_line = cmdline;
    } else if (string_prefixes_string(cmdline, autosuggestion)) {
        // No case disagreements, or no extra characters in the autosuggestion.
        full_line = autosuggestion;
    } else {
        // We have an autosuggestion which is not a prefix of the command line, i.e. a case
        // disagreement. Decide whose case we want to use.
        const wchar_t *begin = nullptr, *cmd = cmdline.c_str();
        parse_util_token_extent(cmd, cmdline.size() - 1, &begin, nullptr, nullptr, nullptr);
        bool last_token_contains_uppercase = false;
        if (begin) {
            const wchar_t *end = begin + std::wcslen(begin);
            last_token_contains_uppercase = (std::find_if(begin, end, iswupper) != end);
        }
        if (!last_token_contains_uppercase) {
            // Use the autosuggestion's case.
            full_line = autosuggestion;
        } else {
            // Use the command line case for its characters, then append the remaining characters in
            // the autosuggestion. Note that we know that autosuggestion.size() > cmdline.size() due
            // to the first test above.
            full_line = cmdline;
            full_line.append(autosuggestion, cmdline.size(),
                             autosuggestion.size() - cmdline.size());
        }
    }
    return full_line;
}

/// Update the cursor position.
void reader_data_t::update_buff_pos(editable_line_t *el, maybe_t<size_t> new_pos) {
    if (new_pos) {
        el->set_position(*new_pos);
    }
    size_t buff_pos = el->position();
    if (el == &command_line && sel_active) {
        if (sel_begin_pos <= buff_pos) {
            sel_start_pos = sel_begin_pos;
            sel_stop_pos = buff_pos + 1;
        } else {
            sel_start_pos = buff_pos;
            sel_stop_pos = sel_begin_pos + 1;
        }
    }
}

/// Repaint the entire commandline. This means reset and clear the commandline, write the prompt,
/// perform syntax highlighting, write the commandline and move the cursor.
void reader_data_t::repaint() {
    editable_line_t *cmd_line = &command_line;
    // Update the indentation.
    indents = parse_util_compute_indents(cmd_line->text());

    wcstring full_line;
    if (silent) {
        full_line = wcstring(cmd_line->text().length(), get_obfuscation_read_char());
    } else {
        // Combine the command and autosuggestion into one string.
        full_line = combine_command_and_autosuggestion(cmd_line->text(), autosuggestion);
    }

    size_t len = full_line.size();
    if (len < 1) len = 1;

    std::vector<highlight_spec_t> colors = this->colors;
    colors.resize(len, highlight_role_t::autosuggestion);

    if (sel_active) {
        highlight_spec_t selection_color = {highlight_role_t::normal, highlight_role_t::selection};
        for (size_t i = sel_start_pos; i < std::min(len, sel_stop_pos); i++) {
            colors[i] = selection_color;
        }
    }

    std::vector<int> indents = this->indents;
    indents.resize(len);

    // Re-render our completions page if necessary. Limit the term size of the pager to the true
    // term size, minus the number of lines consumed by our string. (Note this doesn't yet consider
    // wrapping).
    int full_line_count = 1 + std::count(full_line.cbegin(), full_line.cend(), L'\n');
    pager.set_term_size(std::max(1, common_get_width()),
                        std::max(1, common_get_height() - full_line_count));
    pager.update_rendering(&current_page_rendering);

    bool focused_on_pager = active_edit_line() == &pager.search_field_line;
    size_t cursor_position = focused_on_pager ? pager.cursor_position() : cmd_line->position();

    // Prepend the mode prompt to the left prompt.
    s_write(&screen, mode_prompt_buff + left_prompt_buff, right_prompt_buff, full_line,
            cmd_line->size(), colors, indents, cursor_position, current_page_rendering,
            focused_on_pager);

    repaint_needed = false;
}

/// Internal helper function for handling killing parts of text.
void reader_data_t::kill(editable_line_t *el, size_t begin_idx, size_t length, int mode, int newv) {
    const wchar_t *begin = el->text().c_str() + begin_idx;
    if (newv) {
        kill_item = wcstring(begin, length);
        kill_add(kill_item);
    } else {
        wcstring old = kill_item;
        if (mode == KILL_APPEND) {
            kill_item.append(begin, length);
        } else {
            kill_item = wcstring(begin, length);
            kill_item.append(old);
        }

        kill_replace(old, kill_item);
    }
    el->erase_substring(begin_idx, length);
    update_buff_pos(el);
    command_line_changed(el);
    super_highlight_me_plenty();
    repaint();
}

// This is called from a signal handler!
void reader_handle_sigint() {
    parser_t::cancel_requested(SIGINT);
    interrupted = SIGINT;
}

/// Make sure buffers are large enough to hold the current string length.
void reader_data_t::command_line_changed(const editable_line_t *el) {
    ASSERT_IS_MAIN_THREAD();
    if (el == &this->command_line) {
        size_t len = this->command_line.size();

        // When we grow colors, propagate the last color (if any), under the assumption that usually
        // it will be correct. If it is, it avoids a repaint.
        highlight_spec_t last_color = colors.empty() ? highlight_spec_t() : colors.back();
        colors.resize(len, last_color);

        indents.resize(len);

        // Update the gen count.
        s_generation.store(1 + read_generation_count(), std::memory_order_relaxed);
    } else if (el == &this->pager.search_field_line) {
        this->pager.refilter_completions();
        this->pager_selection_changed();
    }
}

void reader_data_t::pager_selection_changed() {
    ASSERT_IS_MAIN_THREAD();

    const completion_t *completion = this->pager.selected_completion(this->current_page_rendering);

    // Update the cursor and command line.
    size_t cursor_pos = this->cycle_cursor_pos;
    wcstring new_cmd_line;

    if (completion == nullptr) {
        new_cmd_line = this->cycle_command_line;
    } else {
        new_cmd_line =
            completion_apply_to_command_line(completion->completion, completion->flags,
                                             this->cycle_command_line, &cursor_pos, false);
    }

    // Only update if something changed, to avoid useless edits in the undo history.
    if (new_cmd_line != command_line.text()) {
        set_buffer_maintaining_pager(new_cmd_line, cursor_pos, true /* transient */);
    }

    // Trigger repaint (see issue #765).
    mark_repaint_needed();
}

/// Expand abbreviations at the given cursor position. Does NOT inspect 'data'.
maybe_t<edit_t> reader_expand_abbreviation_in_command(const wcstring &cmdline, size_t cursor_pos,
                                                      const environment_t &vars) {
    // See if we are at "command position". Get the surrounding command substitution, and get the
    // extent of the first token.
    const wchar_t *const buff = cmdline.c_str();
    const wchar_t *cmdsub_begin = nullptr, *cmdsub_end = nullptr;
    parse_util_cmdsubst_extent(buff, cursor_pos, &cmdsub_begin, &cmdsub_end);
    assert(cmdsub_begin != nullptr && cmdsub_begin >= buff);
    assert(cmdsub_end != nullptr && cmdsub_end >= cmdsub_begin);

    // Determine the offset of this command substitution.
    const size_t subcmd_offset = cmdsub_begin - buff;

    const wcstring subcmd = wcstring(cmdsub_begin, cmdsub_end - cmdsub_begin);
    const size_t subcmd_cursor_pos = cursor_pos - subcmd_offset;

    // Parse this subcmd.
    parse_node_tree_t parse_tree;
    parse_tree_from_string(subcmd,
                           parse_flag_continue_after_error | parse_flag_accept_incomplete_tokens,
                           &parse_tree, nullptr);

    // Look for plain statements where the cursor is at the end of the command.
    using namespace grammar;
    tnode_t<tok_string> matching_cmd_node;
    for (const parse_node_t &node : parse_tree) {
        // Only interested in plain statements with source.
        if (node.type != symbol_plain_statement || !node.has_source()) continue;

        // Get the command node. Skip it if we can't or it has no source.
        tnode_t<plain_statement> statement(&parse_tree, &node);
        tnode_t<tok_string> cmd_node = statement.child<0>();

        // Skip decorated statements.
        if (get_decoration(statement) != parse_statement_decoration_none) continue;

        auto msource = cmd_node.source_range();
        if (!msource) continue;

        // Now see if its source range contains our cursor, including at the end.
        if (subcmd_cursor_pos >= msource->start &&
            subcmd_cursor_pos <= msource->start + msource->length) {
            // Success!
            matching_cmd_node = cmd_node;
            break;
        }
    }

    // Now if we found a command node, expand it.
    maybe_t<edit_t> result{};
    if (matching_cmd_node) {
        const wcstring token = matching_cmd_node.get_source(subcmd);
        if (auto abbreviation = expand_abbreviation(token, vars)) {
            // There was an abbreviation! Replace the token in the full command. Maintain the
            // relative position of the cursor.
            source_range_t r = *matching_cmd_node.source_range();
            result = edit_t(subcmd_offset + r.start, r.length, std::move(*abbreviation));
        }
    }
    return result;
}

/// Expand abbreviations at the current cursor position, minus the given  cursor backtrack. This may
/// change the command line but does NOT repaint it. This is to allow the caller to coalesce
/// repaints.
bool reader_data_t::expand_abbreviation_as_necessary(size_t cursor_backtrack) {
    bool result = false;
    editable_line_t *el = active_edit_line();

    if (expand_abbreviations && el == &command_line) {
        // Try expanding abbreviations.
        size_t cursor_pos = el->position() - std::min(el->position(), cursor_backtrack);

        if (auto edit = reader_expand_abbreviation_in_command(el->text(), cursor_pos, vars())) {
            el->push_edit(std::move(*edit));
            update_buff_pos(el);
            el->undo_history.may_coalesce = false;
            command_line_changed(el);
            result = true;
        }
    }
    return result;
}

void reader_data_t::repaint_if_needed() {
    bool needs_reset = screen_reset_needed;
    bool needs_repaint = needs_reset || repaint_needed;

    if (needs_reset) {
        exec_prompt();
        s_reset(&screen, screen_reset_mode_t::current_line_and_prompt);
        screen_reset_needed = false;
    }

    if (needs_repaint) {
        repaint();  // reader_repaint clears repaint_needed
    }
}

void reader_reset_interrupted() { interrupted = 0; }

int reader_test_and_clear_interrupted() {
    int res = interrupted;
    if (res) {
        interrupted = 0;
    }
    return res;
}

void reader_write_title(const wcstring &cmd, parser_t &parser, bool reset_cursor_position) {
    if (!term_supports_setting_title()) return;

    scoped_push<bool> noninteractive{&parser.libdata().is_interactive, false};
    scoped_push<bool> in_title(&parser.libdata().suppress_fish_trace, true);

    wcstring fish_title_command = DEFAULT_TITLE;
    if (function_exists(L"fish_title", parser)) {
        fish_title_command = L"fish_title";
        if (!cmd.empty()) {
            fish_title_command.append(L" ");
            fish_title_command.append(
                escape_string(cmd, ESCAPE_ALL | ESCAPE_NO_QUOTED | ESCAPE_NO_TILDE));
        }
    }

    wcstring_list_t lst;
    (void)exec_subshell(fish_title_command, parser, lst, false /* ignore exit status */);
    if (!lst.empty()) {
        std::fputws(L"\x1B]0;", stdout);
        for (const auto &i : lst) {
            std::fputws(i.c_str(), stdout);
        }
        ignore_result(write(STDOUT_FILENO, "\a", 1));
    }

    outputter_t::stdoutput().set_color(rgb_color_t::reset(), rgb_color_t::reset());
    if (reset_cursor_position && !lst.empty()) {
        // Put the cursor back at the beginning of the line (issue #2453).
        ignore_result(write(STDOUT_FILENO, "\r", 1));
    }
}

void reader_data_t::exec_mode_prompt() {
    mode_prompt_buff.clear();
    if (function_exists(MODE_PROMPT_FUNCTION_NAME, parser())) {
        wcstring_list_t mode_indicator_list;
        exec_subshell(MODE_PROMPT_FUNCTION_NAME, parser(), mode_indicator_list, false);
        // We do not support multiple lines in the mode indicator, so just concatenate all of
        // them.
        for (const auto &i : mode_indicator_list) {
            mode_prompt_buff += i;
        }
    }
}

/// Reexecute the prompt command. The output is inserted into prompt_buff.
void reader_data_t::exec_prompt() {
    // Clear existing prompts.
    left_prompt_buff.clear();
    right_prompt_buff.clear();

    // Suppress fish_trace while in the prompt.
    scoped_push<bool> in_prompt(&parser().libdata().suppress_fish_trace, true);

    // Do not allow the exit status of the prompts to leak through.
    const bool apply_exit_status = false;

    // HACK: Query winsize again because it might have changed.
    // This allows prompts to react to $COLUMNS.
    (void)get_current_winsize();

    // If we have any prompts, they must be run non-interactively.
    if (!left_prompt.empty() || !right_prompt.empty()) {
        scoped_push<bool> noninteractive{&parser().libdata().is_interactive, false};

        exec_mode_prompt();

        if (!left_prompt.empty()) {
            wcstring_list_t prompt_list;
            // Ignore return status.
            exec_subshell(left_prompt, parser(), prompt_list, apply_exit_status);
            for (size_t i = 0; i < prompt_list.size(); i++) {
                if (i > 0) left_prompt_buff += L'\n';
                left_prompt_buff += prompt_list.at(i);
            }
        }

        if (!right_prompt.empty()) {
            wcstring_list_t prompt_list;
            // Status is ignored.
            exec_subshell(right_prompt, parser(), prompt_list, apply_exit_status);
            for (const auto &i : prompt_list) {
                // Right prompt does not support multiple lines, so just concatenate all of them.
                right_prompt_buff += i;
            }
        }
    }

    // Write the screen title. Do not reset the cursor position: exec_prompt is called when there
    // may still be output on the line from the previous command (#2499) and we need our PROMPT_SP
    // hack to work.
    reader_write_title(L"", parser(), false);
}

void reader_init() {
    auto &vars = parser_t::principal_parser().vars();

    // Ensure this var is present even before an interactive command is run so that if it is used
    // in a function like `fish_prompt` or `fish_right_prompt` it is defined at the time the first
    // prompt is written.
    vars.set_one(ENV_CMD_DURATION, ENV_UNEXPORT, L"0");

    // Save the initial terminal mode.
    tcgetattr(STDIN_FILENO, &terminal_mode_on_startup);

    // Set the mode used for program execution, initialized to the current mode.
    std::memcpy(&tty_modes_for_external_cmds, &terminal_mode_on_startup,
                sizeof tty_modes_for_external_cmds);
    tty_modes_for_external_cmds.c_iflag &= ~IXON;   // disable flow control
    tty_modes_for_external_cmds.c_iflag &= ~IXOFF;  // disable flow control

    // Set the mode used for the terminal, initialized to the current mode.
    std::memcpy(&shell_modes, &terminal_mode_on_startup, sizeof shell_modes);

    shell_modes.c_iflag &= ~ICRNL;  // disable mapping CR (\cM) to NL (\cJ)
    shell_modes.c_iflag &= ~INLCR;  // disable mapping NL (\cJ) to CR (\cM)
    shell_modes.c_iflag &= ~IXON;   // disable flow control
    shell_modes.c_iflag &= ~IXOFF;  // disable flow control

    shell_modes.c_lflag &= ~ICANON;  // turn off canonical mode
    shell_modes.c_lflag &= ~ECHO;    // turn off echo mode
    shell_modes.c_lflag &= ~IEXTEN;  // turn off handling of discard and lnext characters

    shell_modes.c_cc[VMIN] = 1;
    shell_modes.c_cc[VTIME] = 0;

    // We do this not because we actually need the window size but for its side-effect of correctly
    // setting the COLUMNS and LINES env vars.
    get_current_winsize();
}

/// Restore the term mode if we own the terminal. It's important we do this before
/// restore_foreground_process_group, otherwise we won't think we own the terminal.
void restore_term_mode() {
    if (getpgrp() != tcgetpgrp(STDIN_FILENO)) return;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &terminal_mode_on_startup) == -1 && errno == EIO) {
        redirect_tty_output();
    }
}

/// Exit the current reader loop. This may be invoked from a signal handler.
void reader_set_end_loop(bool flag) { s_end_current_loop = flag; }

void reader_force_exit() {
    // Beware, we may be in a signal handler.
    s_exit_forced = true;
}

/// Indicates if the given command char ends paging.
static bool command_ends_paging(readline_cmd_t c, bool focused_on_search_field) {
    using rl = readline_cmd_t;
    switch (c) {
        case rl::history_prefix_search_backward:
        case rl::history_prefix_search_forward:
        case rl::history_search_backward:
        case rl::history_search_forward:
        case rl::history_token_search_backward:
        case rl::history_token_search_forward:
        case rl::accept_autosuggestion:
        case rl::delete_or_exit:
        case rl::cancel: {
            // These commands always end paging.
            return true;
        }
        case rl::complete:
        case rl::complete_and_search:
        case rl::backward_char:
        case rl::forward_char:
        case rl::up_line:
        case rl::down_line:
        case rl::repaint:
        case rl::suppress_autosuggestion:
        case rl::beginning_of_history:
        case rl::end_of_history: {
            // These commands never end paging.
            return false;
        }
        case rl::execute: {
            // execute does end paging, but only executes if it was not paging. So it's handled
            // specially.
            return false;
        }
        case rl::beginning_of_line:
        case rl::end_of_line:
        case rl::forward_word:
        case rl::backward_word:
        case rl::forward_bigword:
        case rl::backward_bigword:
        case rl::delete_char:
        case rl::backward_delete_char:
        case rl::kill_line:
        case rl::yank:
        case rl::yank_pop:
        case rl::backward_kill_line:
        case rl::kill_whole_line:
        case rl::kill_word:
        case rl::kill_bigword:
        case rl::backward_kill_word:
        case rl::backward_kill_path_component:
        case rl::backward_kill_bigword:
        case rl::self_insert:
        case rl::self_insert_notfirst:
        case rl::transpose_chars:
        case rl::transpose_words:
        case rl::upcase_word:
        case rl::downcase_word:
        case rl::capitalize_word:
        case rl::beginning_of_buffer:
        case rl::end_of_buffer:
        case rl::undo:
        case rl::redo:
            // These commands operate on the search field if that's where the focus is.
            return !focused_on_search_field;
        default:
            return false;
    }
}

/// Indicates if the given command ends the history search.
static bool command_ends_history_search(readline_cmd_t c) {
    switch (c) {
        case readline_cmd_t::history_prefix_search_backward:
        case readline_cmd_t::history_prefix_search_forward:
        case readline_cmd_t::history_search_backward:
        case readline_cmd_t::history_search_forward:
        case readline_cmd_t::history_token_search_backward:
        case readline_cmd_t::history_token_search_forward:
        case readline_cmd_t::repaint:
        case readline_cmd_t::force_repaint:
            return false;
        default:
            return true;
    }
}

/// Remove the previous character in the character buffer and on the screen using syntax
/// highlighting, etc.
void reader_data_t::delete_char(bool backward) {
    editable_line_t *el = active_edit_line();

    size_t pos = el->position();
    if (!backward) {
        pos++;
    }
    size_t pos_end = pos;

    if (el->position() == 0 && backward) return;

    // Fake composed character sequences by continuing to delete until we delete a character of
    // width at least 1.
    int width;
    do {
        pos--;
        width = fish_wcwidth(el->text().at(pos));
    } while (width == 0 && pos > 0);
    el->erase_substring(pos, pos_end - pos);
    update_buff_pos(el);
    command_line_changed(el);
    suppress_autosuggestion = true;

    super_highlight_me_plenty();
    mark_repaint_needed();
}

/// Insert the characters of the string into the command line buffer and print them to the screen
/// using syntax highlighting, etc.
/// Returns true if the string changed.
bool reader_data_t::insert_string(editable_line_t *el, const wcstring &str) {
    if (str.empty()) return false;

    el->insert_string(str, 0, str.size());
    update_buff_pos(el);
    command_line_changed(el);

    if (el == &command_line) {
        suppress_autosuggestion = false;

        // Syntax highlight. Note we must have that buff_pos > 0 because we just added something
        // nonzero to its length.
        assert(el->position() > 0);
        super_highlight_me_plenty(-1);
    }

    repaint();
    return true;
}

/// Insert the string in the given command line at the given cursor position. The function checks if
/// the string is quoted or not and correctly escapes the string.
///
/// \param val the string to insert
/// \param flags A union of all flags describing the completion to insert. See the completion_t
/// struct for more information on possible values.
/// \param command_line The command line into which we will insert
/// \param inout_cursor_pos On input, the location of the cursor within the command line. On output,
/// the new desired position.
/// \param append_only Whether we can only append to the command line, or also modify previous
/// characters. This is used to determine whether we go inside a trailing quote.
///
/// \return The completed string
wcstring completion_apply_to_command_line(const wcstring &val, complete_flags_t flags,
                                          const wcstring &command_line, size_t *inout_cursor_pos,
                                          bool append_only) {
    bool add_space = !bool(flags & COMPLETE_NO_SPACE);
    bool do_replace = bool(flags & COMPLETE_REPLACES_TOKEN);
    bool do_escape = !bool(flags & COMPLETE_DONT_ESCAPE);
    bool no_tilde = bool(flags & COMPLETE_DONT_ESCAPE_TILDES);

    const size_t cursor_pos = *inout_cursor_pos;
    bool back_into_trailing_quote = false;
    bool have_space_after_token = command_line[cursor_pos] == L' ';

    if (do_replace) {
        size_t move_cursor;
        const wchar_t *begin, *end;

        const wchar_t *buff = command_line.c_str();
        parse_util_token_extent(buff, cursor_pos, &begin, nullptr, nullptr, nullptr);
        end = buff + cursor_pos;

        wcstring sb(buff, begin - buff);

        if (do_escape) {
            wcstring escaped = escape_string(
                val, ESCAPE_ALL | ESCAPE_NO_QUOTED | (no_tilde ? ESCAPE_NO_TILDE : 0));
            sb.append(escaped);
            move_cursor = escaped.size();
        } else {
            sb.append(val);
            move_cursor = val.length();
        }

        if (add_space) {
            if (!have_space_after_token) sb.append(L" ");
            move_cursor += 1;
        }
        sb.append(end);

        size_t new_cursor_pos = (begin - buff) + move_cursor;
        *inout_cursor_pos = new_cursor_pos;
        return sb;
    }

    wchar_t quote = L'\0';
    wcstring replaced;
    if (do_escape) {
        // We need to figure out whether the token we complete has unclosed quotes. Since the token
        // may be inside a command substitutions we must first determine the extents of the
        // innermost command substitution.
        const wchar_t *cmdsub_begin, *cmdsub_end;
        parse_util_cmdsubst_extent(command_line.c_str(), cursor_pos, &cmdsub_begin, &cmdsub_end);
        size_t cmdsub_offset = cmdsub_begin - command_line.c_str();
        // Find the last quote in the token to complete. By parsing only the string inside any
        // command substitution, we prevent the tokenizer from treating the entire command
        // substitution as one token.
        parse_util_get_parameter_info(
            command_line.substr(cmdsub_offset, (cmdsub_end - cmdsub_begin)),
            cursor_pos - cmdsub_offset, &quote, nullptr, nullptr);

        // If the token is reported as unquoted, but ends with a (unescaped) quote, and we can
        // modify the command line, then delete the trailing quote so that we can insert within
        // the quotes instead of after them. See issue #552.
        if (quote == L'\0' && !append_only && cursor_pos > 0) {
            // The entire token is reported as unquoted...see if the last character is an
            // unescaped quote.
            wchar_t trailing_quote = unescaped_quote(command_line, cursor_pos - 1);
            if (trailing_quote != L'\0') {
                quote = trailing_quote;
                back_into_trailing_quote = true;
            }
        }

        replaced = parse_util_escape_string_with_quote(val, quote, no_tilde);
    } else {
        replaced = val;
    }

    size_t insertion_point = cursor_pos;
    if (back_into_trailing_quote) {
        // Move the character back one so we enter the terminal quote.
        assert(insertion_point > 0);
        insertion_point--;
    }

    // Perform the insertion and compute the new location.
    wcstring result = command_line;
    result.insert(insertion_point, replaced);
    size_t new_cursor_pos = insertion_point + replaced.size() + (back_into_trailing_quote ? 1 : 0);
    if (add_space) {
        if (quote != L'\0' && unescaped_quote(command_line, insertion_point) != quote) {
            // This is a quoted parameter, first print a quote.
            result.insert(new_cursor_pos++, wcstring(&quote, 1));
        }
        if (!have_space_after_token) result.insert(new_cursor_pos, L" ");
        new_cursor_pos++;
    }
    *inout_cursor_pos = new_cursor_pos;
    return result;
}

/// Insert the string at the current cursor position. The function checks if the string is quoted or
/// not and correctly escapes the string.
///
/// \param val the string to insert
/// \param token_end the position after the token to complete
/// \param flags A union of all flags describing the completion to insert. See the completion_t
/// struct for more information on possible values.
void reader_data_t::completion_insert(const wchar_t *val, size_t token_end,
                                      complete_flags_t flags) {
    editable_line_t *el = active_edit_line();

    // Move the cursor to the end of the token.
    if (el->position() != token_end) {
        update_buff_pos(el, token_end);  // repaint() is done later
    }

    size_t cursor = el->position();
    wcstring new_command_line = completion_apply_to_command_line(val, flags, el->text(), &cursor,
                                                                 false /* not append only */);
    set_buffer_maintaining_pager(new_command_line, cursor);
}

static bool may_add_to_history(const wcstring &commandline_prefix) {
    return !commandline_prefix.empty() && commandline_prefix.at(0) != L' ';
}

// Returns a function that can be invoked (potentially
// on a background thread) to determine the autosuggestion
static std::function<autosuggestion_result_t(void)> get_autosuggestion_performer(
    parser_t &parser, const wcstring &search_string, size_t cursor_pos, history_t *history) {
    const unsigned int generation_count = read_generation_count();
    auto vars = parser.vars().snapshot();
    const wcstring working_directory = vars->get_pwd_slash();
    // TODO: suspicious use of 'history' here
    // This is safe because histories are immortal, but perhaps
    // this should use shared_ptr
    return [=]() -> autosuggestion_result_t {
        ASSERT_IS_BACKGROUND_THREAD();
        const autosuggestion_result_t nothing = {};
        operation_context_t ctx = get_bg_context(vars, generation_count);
        if (ctx.check_cancel()) {
            return nothing;
        }

        // Let's make sure we aren't using the empty string.
        if (search_string.empty()) {
            return nothing;
        }

        if (may_add_to_history(search_string)) {
            history_search_t searcher(*history, search_string, history_search_type_t::prefix,
                                      history_search_flags_t{});
            while (!ctx.check_cancel() && searcher.go_backwards()) {
                const history_item_t &item = searcher.current_item();

                // Skip items with newlines because they make terrible autosuggestions.
                if (item.str().find(L'\n') != wcstring::npos) continue;

                if (autosuggest_validate_from_history(item, working_directory, ctx)) {
                    // The command autosuggestion was handled specially, so we're done.
                    return {searcher.current_string(), search_string};
                }
            }
        }

        // Maybe cancel here.
        if (ctx.check_cancel()) return nothing;

        // Here we do something a little funny. If the line ends with a space, and the cursor is not
        // at the end, don't use completion autosuggestions. It ends up being pretty weird seeing
        // stuff get spammed on the right while you go back to edit a line
        const wchar_t last_char = search_string.at(search_string.size() - 1);
        const bool cursor_at_end = (cursor_pos == search_string.size());
        if (!cursor_at_end && iswspace(last_char)) return nothing;

        // On the other hand, if the line ends with a quote, don't go dumping stuff after the quote.
        if (std::wcschr(L"'\"", last_char) && cursor_at_end) return nothing;

        // Try normal completions.
        completion_request_flags_t complete_flags = completion_request_t::autosuggestion;
        completion_list_t completions = complete(search_string, complete_flags, ctx);
        completions_sort_and_prioritize(&completions, complete_flags);
        if (!completions.empty()) {
            const completion_t &comp = completions.at(0);
            size_t cursor = cursor_pos;
            wcstring suggestion = completion_apply_to_command_line(
                comp.completion, comp.flags, search_string, &cursor, true /* append only */);
            return {std::move(suggestion), search_string};
        }

        return nothing;
    };
}

bool reader_data_t::can_autosuggest() const {
    // We autosuggest if suppress_autosuggestion is not set, if we're not doing a history search,
    // and our command line contains a non-whitespace character.
    const editable_line_t *el = active_edit_line();
    const wchar_t *whitespace = L" \t\r\n\v";
    return allow_autosuggestion && !suppress_autosuggestion && history_search.is_at_end() &&
           el == &command_line && el->text().find_first_not_of(whitespace) != wcstring::npos;
}

// Called after an autosuggestion has been computed on a background thread
void reader_data_t::autosuggest_completed(autosuggestion_result_t result) {
    if (!result.suggestion.empty() && can_autosuggest() &&
        result.search_string == command_line.text() &&
        string_prefixes_string_case_insensitive(result.search_string, result.suggestion)) {
        // Autosuggestion is active and the search term has not changed, so we're good to go.
        autosuggestion = std::move(result.suggestion);
        sanity_check();
        repaint();
    }
}

void reader_data_t::update_autosuggestion() {
    // Updates autosuggestion. We look for an autosuggestion if the command line is non-empty and if
    // we're not doing a history search.
    autosuggestion.clear();
    if (can_autosuggest()) {
        const editable_line_t *el = active_edit_line();
        auto performer =
            get_autosuggestion_performer(parser(), el->text(), el->position(), history);
        auto shared_this = this->shared_from_this();
        debounce_autosuggestions().perform(
            performer, [shared_this](autosuggestion_result_t result) {
                shared_this->autosuggest_completed(std::move(result));
            });
    }
}

// Accept any autosuggestion by replacing the command line with it. If full is true, take the whole
// thing; if it's false, then respect the passed in style.
void reader_data_t::accept_autosuggestion(bool full, move_word_style_t style) {
    if (!autosuggestion.empty()) {
        // Accepting an autosuggestion clears the pager.
        clear_pager();

        // Accept the autosuggestion.
        if (full) {
            // Just take the whole thing.
            command_line.replace_substring(0, command_line.size(), std::move(autosuggestion));
        } else {
            // Accept characters according to the specified style.
            move_word_state_machine_t state(style);
            size_t want;
            for (want = command_line.size(); want < autosuggestion.size(); want++) {
                wchar_t wc = autosuggestion.at(want);
                if (!state.consume_char(wc)) break;
            }
            size_t have = command_line.size();
            command_line.replace_substring(command_line.size(), 0,
                                           autosuggestion.substr(have, want - have));
        }
        update_buff_pos(&command_line);
        command_line_changed(&command_line);
        super_highlight_me_plenty();
        repaint();
    }
}

// Ensure we have no pager contents.
void reader_data_t::clear_pager() {
    pager.clear();
    current_page_rendering = page_rendering_t();
    mark_repaint_needed();
}

void reader_data_t::select_completion_in_direction(selection_motion_t dir) {
    bool selection_changed = pager.select_next_completion_in_direction(dir, current_page_rendering);
    if (selection_changed) {
        pager_selection_changed();
    }
}

/// Flash the screen. This function changes the color of the current line momentarily and sends a
/// BEL to maybe flash the screen or emite a sound, depending on how it is configured.
void reader_data_t::flash() {
    struct timespec pollint;
    editable_line_t *el = &command_line;
    for (size_t i = 0; i < el->position(); i++) {
        colors.at(i) = highlight_spec_t::make_background(highlight_role_t::search_match);
    }

    repaint();
    ignore_result(write(STDOUT_FILENO, "\a", 1));
    // The write above changed the timestamp of stdout; ensure we don't therefore reset our screen.
    // See #3693.
    s_save_status(&screen);

    pollint.tv_sec = 0;
    pollint.tv_nsec = 100 * 1000000;
    nanosleep(&pollint, nullptr);

    super_highlight_me_plenty();
    repaint();
}

/// Characters that may not be part of a token that is to be replaced by a case insensitive
/// completion.
#define REPLACE_UNCLEAN L"$*?({})"

/// Check if the specified string can be replaced by a case insensitive completion with the
/// specified flags.
///
/// Advanced tokens like those containing {}-style expansion can not at the moment be replaced,
/// other than if the new token is already an exact replacement, e.g. if the COMPLETE_DONT_ESCAPE
/// flag is set.
static bool reader_can_replace(const wcstring &in, int flags) {
    const wchar_t *str = in.c_str();

    if (flags & COMPLETE_DONT_ESCAPE) {
        return true;
    }

    // Test characters that have a special meaning in any character position.
    while (*str) {
        if (std::wcschr(REPLACE_UNCLEAN, *str)) return false;
        str++;
    }

    return true;
}

/// Determine the best match type for a set of completions.
static fuzzy_match_type_t get_best_match_type(const completion_list_t &comp) {
    fuzzy_match_type_t best_type = fuzzy_match_none;
    for (const auto &i : comp) {
        best_type = std::min(best_type, i.match.type);
    }
    // If the best type is an exact match, reduce it to prefix match. Otherwise a tab completion
    // will only show one match if it matches a file exactly. (see issue #959).
    if (best_type == fuzzy_match_exact) {
        best_type = fuzzy_match_prefix;
    }
    return best_type;
}

/// Handle the list of completions. This means the following:
///
/// - If the list is empty, flash the terminal.
/// - If the list contains one element, write the whole element, and if the element does not end on
/// a '/', '@', ':', '.', ',' or a '=', also write a trailing space.
/// - If the list contains multiple elements, insert their common prefix, if any and display
/// the list in the pager.  Depending on terminal size and the length of the list, the pager
/// may either show less than a screenfull and exit or use an interactive pager to allow the
/// user to scroll through the completions.
///
/// \param comp the list of completion strings
/// \param token_begin the position of the token to complete
/// \param token_end the position after the token to complete
///
/// Return true if we inserted text into the command line, false if we did not.
bool reader_data_t::handle_completions(const completion_list_t &comp, size_t token_begin,
                                       size_t token_end) {
    bool done = false;
    bool success = false;
    const editable_line_t *el = &command_line;

    const wcstring tok(el->text().c_str() + token_begin, token_end - token_begin);

    // Check trivial cases.
    size_t size = comp.size();
    if (size == 0) {
        // No suitable completions found, flash screen and return.
        flash();
        done = true;
        success = false;
    } else if (size == 1) {
        // Exactly one suitable completion found - insert it.
        const completion_t &c = comp.at(0);

        // If this is a replacement completion, check that we know how to replace it, e.g. that
        // the token doesn't contain evil operators like {}.
        if (!(c.flags & COMPLETE_REPLACES_TOKEN) || reader_can_replace(tok, c.flags)) {
            completion_insert(c.completion.c_str(), token_end, c.flags);
        }
        done = true;
        success = true;
    }

    if (done) {
        return success;
    }

    fuzzy_match_type_t best_match_type = get_best_match_type(comp);

    // Determine whether we are going to replace the token or not. If any commands of the best
    // type do not require replacement, then ignore all those that want to use replacement.
    bool will_replace_token = true;
    for (const completion_t &el : comp) {
        if (el.match.type <= best_match_type && !(el.flags & COMPLETE_REPLACES_TOKEN)) {
            will_replace_token = false;
            break;
        }
    }

    // Decide which completions survived. There may be a lot of them; it would be nice if we could
    // figure out how to avoid copying them here.
    completion_list_t surviving_completions;
    for (const completion_t &el : comp) {
        // Ignore completions with a less suitable match type than the best.
        if (el.match.type > best_match_type) continue;

        // Only use completions that match replace_token.
        bool completion_replace_token = static_cast<bool>(el.flags & COMPLETE_REPLACES_TOKEN);
        if (completion_replace_token != will_replace_token) continue;

        // Don't use completions that want to replace, if we cannot replace them.
        if (completion_replace_token && !reader_can_replace(tok, el.flags)) continue;

        // This completion survived.
        surviving_completions.push_back(el);
    }

    bool use_prefix = false;
    wcstring common_prefix;
    if (match_type_shares_prefix(best_match_type)) {
        // Try to find a common prefix to insert among the surviving completions.
        complete_flags_t flags = 0;
        bool prefix_is_partial_completion = false;
        bool first = true;
        for (const completion_t &el : surviving_completions) {
            if (first) {
                // First entry, use the whole string.
                common_prefix = el.completion;
                flags = el.flags;
                first = false;
            } else {
                // Determine the shared prefix length.
                size_t idx, max = std::min(common_prefix.size(), el.completion.size());

                for (idx = 0; idx < max; idx++) {
                    wchar_t ac = common_prefix.at(idx), bc = el.completion.at(idx);
                    bool matches = (ac == bc);
                    // If we are replacing the token, allow case to vary.
                    if (will_replace_token && !matches) {
                        // Hackish way to compare two strings in a case insensitive way,
                        // hopefully better than towlower().
                        matches = (wcsncasecmp(&ac, &bc, 1) == 0);
                    }
                    if (!matches) break;
                }

                // idx is now the length of the new common prefix.
                common_prefix.resize(idx);
                prefix_is_partial_completion = true;

                // Early out if we decide there's no common prefix.
                if (idx == 0) break;
            }
        }

        // Determine if we use the prefix. We use it if it's non-empty and it will actually make
        // the command line longer. It may make the command line longer by virtue of not using
        // REPLACE_TOKEN (so it always appends to the command line), or by virtue of replacing
        // the token but being longer than it.
        use_prefix = common_prefix.size() > (will_replace_token ? tok.size() : 0);
        assert(!use_prefix || !common_prefix.empty());

        if (use_prefix) {
            // We got something. If more than one completion contributed, then it means we have
            // a prefix; don't insert a space after it.
            if (prefix_is_partial_completion) flags |= COMPLETE_NO_SPACE;
            completion_insert(common_prefix.c_str(), token_end, flags);
            cycle_command_line = command_line.text();
            cycle_cursor_pos = command_line.position();
            success = true;
        }
    }

    if (use_prefix) {
        for (completion_t &c : surviving_completions) {
            wcstring &comp = c.completion;
            comp.erase(comp.begin(), comp.begin() + common_prefix.size());
        }
    }

    // Print the completion list.
    wcstring prefix;
    if (will_replace_token || match_type_requires_full_replacement(best_match_type)) {
        prefix.clear();  // no prefix
    } else if (tok.size() + common_prefix.size() <= PREFIX_MAX_LEN) {
        prefix = tok + common_prefix;
    } else {
        // Append just the end of the string.
        prefix = wcstring{get_ellipsis_char()};
        prefix.append(tok + common_prefix, tok.size() + common_prefix.size() - PREFIX_MAX_LEN,
                      PREFIX_MAX_LEN);
    }

    // Update the pager data.
    pager.set_prefix(prefix);
    pager.set_completions(surviving_completions);
    // Invalidate our rendering.
    current_page_rendering = page_rendering_t();
    // Modify the command line to reflect the new pager.
    pager_selection_changed();
    mark_repaint_needed();
    return false;
}

/// Return true if we believe ourselves to be orphaned. loop_count is how many times we've tried to
/// stop ourselves via SIGGTIN.
static bool check_for_orphaned_process(unsigned long loop_count, pid_t shell_pgid) {
    bool we_think_we_are_orphaned = false;
    // Try kill-0'ing the process whose pid corresponds to our process group ID. It's possible this
    // will fail because we don't have permission to signal it. But more likely it will fail because
    // it no longer exists, and we are orphaned.
    if (loop_count % 64 == 0 && kill(shell_pgid, 0) < 0 && errno == ESRCH) {
        we_think_we_are_orphaned = true;
    }

    // Try reading from the tty; if we get EIO we are orphaned. This is sort of bad because it
    // may block.
    if (!we_think_we_are_orphaned && loop_count % 128 == 0) {
#ifdef HAVE_CTERMID_R
        char buf[L_ctermid];
        char *tty = ctermid_r(buf);
#else
        char *tty = ctermid(nullptr);
#endif
        if (!tty) {
            wperror(L"ctermid");
            exit_without_destructors(1);
        }

        // Open the tty. Presumably this is stdin, but maybe not?
        autoclose_fd_t tty_fd{open(tty, O_RDONLY | O_NONBLOCK)};
        if (!tty_fd.valid()) {
            wperror(L"open");
            exit_without_destructors(1);
        }

        char tmp;
        if (read(tty_fd.fd(), &tmp, 1) < 0 && errno == EIO) {
            we_think_we_are_orphaned = true;
        }
    }

    // Just give up if we've done it a lot times.
    if (loop_count > 4096) {
        we_think_we_are_orphaned = true;
    }

    return we_think_we_are_orphaned;
}

// Ensure that fish owns the terminal, possibly waiting. If we cannot acquire the terminal, then
// report an error and exit.
static void acquire_tty_or_exit(pid_t shell_pgid) {
    ASSERT_IS_MAIN_THREAD();

    // Check if we are in control of the terminal, so that we don't do semi-expensive things like
    // reset signal handlers unless we really have to, which we often don't.
    // Common case.
    if (tcgetpgrp(STDIN_FILENO) == shell_pgid) {
        return;
    }

    // Bummer, we are not in control of the terminal. Stop until parent has given us control of
    // it.
    //
    // In theory, reseting signal handlers could cause us to miss signal deliveries. In
    // practice, this code should only be run during startup, when we're not waiting for any
    // signals.
    signal_reset_handlers();
    cleanup_t restore_sigs([] { signal_set_handlers(true); });

    // Ok, signal handlers are taken out of the picture. Stop ourself in a loop until we are in
    // control of the terminal. However, the call to signal(SIGTTIN) may silently not do
    // anything if we are orphaned.
    //
    // As far as I can tell there's no really good way to detect that we are orphaned. One way
    // is to just detect if the group leader exited, via kill(shell_pgid, 0). Another
    // possibility is that read() from the tty fails with EIO - this is more reliable but it's
    // harder, because it may succeed or block. So we loop for a while, trying those strategies.
    // Eventually we just give up and assume we're orphaend.
    for (unsigned loop_count = 0;; loop_count++) {
        pid_t owner = tcgetpgrp(STDIN_FILENO);
        // 0 is a valid return code from `tcgetpgrp()` under at least FreeBSD and testing
        // indicates that a subsequent call to `tcsetpgrp()` will succeed. 0 is the
        // pid of the top-level kernel process, so I'm not sure if this means ownership
        // of the terminal has gone back to the kernel (i.e. it's not owned) or if it is
        // just an "invalid" pid for all intents and purposes.
        if (owner == 0) {
            tcsetpgrp(STDIN_FILENO, shell_pgid);
            // Since we expect the above to work, call `tcgetpgrp()` immediately to
            // avoid a second pass through this loop.
            owner = tcgetpgrp(STDIN_FILENO);
        }
        if (owner == -1 && errno == ENOTTY) {
            if (session_interactivity() == session_interactivity_t::not_interactive) {
                // It's OK if we're not able to take control of the terminal. We handle
                // the fallout from this in a few other places.
                break;
            }
            // No TTY, cannot be interactive?
            redirect_tty_output();
            FLOGF(warning, _(L"No TTY for interactive shell (tcgetpgrp failed)"));
            wperror(L"setpgid");
            exit_without_destructors(1);
        }
        if (owner == shell_pgid) {
            break;  // success
        } else {
            if (check_for_orphaned_process(loop_count, shell_pgid)) {
                // We're orphaned, so we just die. Another sad statistic.
                const wchar_t *fmt =
                    _(L"I appear to be an orphaned process, so I am quitting politely. "
                      L"My pid is %d.");
                FLOGF(warning, fmt, static_cast<int>(getpid()));
                exit_without_destructors(1);
            }

            // Try stopping us.
            int ret = killpg(shell_pgid, SIGTTIN);
            if (ret < 0) {
                wperror(L"killpg(shell_pgid, SIGTTIN)");
                exit_without_destructors(1);
            }
        }
    }
}

/// Initialize data for interactive use.
static void reader_interactive_init(parser_t &parser) {
    ASSERT_IS_MAIN_THREAD();

    pid_t shell_pgid = getpgrp();

    // Set up key bindings.
    init_input();

    // Ensure interactive signal handling is enabled.
    signal_set_handlers_once(true);

    // Wait until we own the terminal.
    acquire_tty_or_exit(shell_pgid);

    // It shouldn't be necessary to place fish in its own process group and force control
    // of the terminal, but that works around fish being started with an invalid pgroup,
    // such as when launched via firejail (#5295)
    // Also become the process group leader if flag -i/--interactive was given (#5909).
    if (shell_pgid == 0 || session_interactivity() == session_interactivity_t::explicit_) {
        shell_pgid = getpid();
        if (setpgid(shell_pgid, shell_pgid) < 0) {
            // If we're session leader setpgid returns EPERM. The other cases where we'd get EPERM
            // don't apply as we passed our own pid.
            //
            // This should be harmless, so we ignore it.
            if (errno != EPERM) {
                FLOG(error, _(L"Failed to assign shell to its own process group"));
                wperror(L"setpgid");
                exit_without_destructors(1);
            }
        }

        // Take control of the terminal
        if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
            if (errno == ENOTTY) {
                redirect_tty_output();
            }
            FLOG(error, _(L"Failed to take control of the terminal"));
            wperror(L"tcsetpgrp");
            exit_without_destructors(1);
        }

        // Configure terminal attributes
        if (tcsetattr(0, TCSANOW, &shell_modes) == -1) {
            if (errno == EIO) {
                redirect_tty_output();
            }
            FLOGF(warning, _(L"Failed to set startup terminal mode!"));
            wperror(L"tcsetattr");
        }
    }

    invalidate_termsize();

    // For compatibility with fish 2.0's $_, now replaced with `status current-command`
    parser.vars().set_one(L"_", ENV_GLOBAL, L"fish");
}

/// Destroy data for interactive use.
static void reader_interactive_destroy() {
    outputter_t::stdoutput().set_color(rgb_color_t::reset(), rgb_color_t::reset());
}

void reader_data_t::sanity_check() const {
    if (command_line.position() > command_line.size()) sanity_lose();
    if (colors.size() != command_line.size()) sanity_lose();
    if (indents.size() != command_line.size()) sanity_lose();
}

/// Set the specified string as the current buffer.
void reader_data_t::set_command_line_and_position(editable_line_t *el, wcstring &&new_str,
                                                  size_t pos) {
    el->push_edit(edit_t(0, el->size(), std::move(new_str)));
    el->set_position(pos);
    el->undo_history.may_coalesce = false;
    update_buff_pos(el, pos);
    command_line_changed(el);
    super_highlight_me_plenty();
    mark_repaint_needed();
}

/// Undo the transient edit und update commandline accordingly.
void reader_data_t::clear_transient_edit() {
    if (!command_line_has_transient_edit) {
        return;
    }
    command_line.undo();
    update_buff_pos(&command_line);
    command_line_changed(&command_line);
    super_highlight_me_plenty();
    mark_repaint_needed();
    command_line_has_transient_edit = false;
}

void reader_data_t::replace_current_token(wcstring &&new_token) {
    const wchar_t *begin, *end;

    // Find current token.
    editable_line_t *el = active_edit_line();
    const wchar_t *buff = el->text().c_str();
    parse_util_token_extent(buff, el->position(), &begin, &end, nullptr, nullptr);

    if (!begin || !end) return;

    size_t offset = begin - buff;
    size_t length = end - begin;
    el->replace_substring(offset, length, std::move(new_token));
}

/// Apply the history search to the command line.
void reader_data_t::update_command_line_from_history_search() {
    wcstring new_text = history_search.is_at_end() ? history_search.search_string()
                                                   : history_search.current_result();
    editable_line_t *el = active_edit_line();
    if (command_line_has_transient_edit) {
        el->undo();
    }
    if (history_search.by_token()) {
        replace_current_token(std::move(new_text));
    } else if (history_search.by_line() || history_search.by_prefix()) {
        el->replace_substring(0, el->size(), std::move(new_text));
    } else {
        return;
    }
    command_line_has_transient_edit = true;
    assert(el == &command_line);
    update_buff_pos(el);
    command_line_changed(el);
    super_highlight_me_plenty();
    mark_repaint_needed();
}

enum move_word_dir_t { MOVE_DIR_LEFT, MOVE_DIR_RIGHT };

/// Move buffer position one word or erase one word. This function updates both the internal buffer
/// and the screen. It is used by M-left, M-right and ^W to do block movement or block erase.
///
/// \param move_right true if moving right
/// \param erase Whether to erase the characters along the way or only move past them.
/// \param newv if the new kill item should be appended to the previous kill item or not.
void reader_data_t::move_word(editable_line_t *el, bool move_right, bool erase,
                              enum move_word_style_t style, bool newv) {
    // Return if we are already at the edge.
    const size_t boundary = move_right ? el->size() : 0;
    if (el->position() == boundary) return;

    // When moving left, a value of 1 means the character at index 0.
    move_word_state_machine_t state(style);
    const wchar_t *const command_line = el->text().c_str();
    const size_t start_buff_pos = el->position();

    size_t buff_pos = el->position();
    while (buff_pos != boundary) {
        size_t idx = (move_right ? buff_pos : buff_pos - 1);
        wchar_t c = command_line[idx];
        if (!state.consume_char(c)) break;
        buff_pos = (move_right ? buff_pos + 1 : buff_pos - 1);
    }

    // Always consume at least one character.
    if (buff_pos == start_buff_pos) buff_pos = (move_right ? buff_pos + 1 : buff_pos - 1);

    // If we are moving left, buff_pos-1 is the index of the first character we do not delete
    // (possibly -1). If we are moving right, then buff_pos is that index - possibly el->size().
    if (erase) {
        // Don't autosuggest after a kill.
        if (el == &this->command_line) {
            suppress_autosuggestion = true;
        }

        if (move_right) {
            kill(el, start_buff_pos, buff_pos - start_buff_pos, KILL_APPEND, newv);
        } else {
            kill(el, buff_pos, start_buff_pos - buff_pos, KILL_PREPEND, newv);
        }
    } else {
        update_buff_pos(el, buff_pos);
        repaint();
    }
}

/// Sets the command line contents, without clearing the pager.
void reader_data_t::set_buffer_maintaining_pager(const wcstring &b, size_t pos, bool transient) {
    // Callers like to pass us pointers into ourselves, so be careful! I don't know if we can use
    // operator= with a pointer to our interior, so use an intermediate.
    size_t command_line_len = b.size();
    if (transient) {
        if (command_line_has_transient_edit) {
            command_line.undo();
        }
        command_line_has_transient_edit = true;
    }
    command_line.replace_substring(0, command_line.size(), wcstring(b));
    command_line_changed(&command_line);

    // Don't set a position past the command line length.
    if (pos > command_line_len) pos = command_line_len;  //!OCLINT(parameter reassignment)

    update_buff_pos(&command_line, pos);

    // Clear history search and pager contents.
    history_search.reset();
    super_highlight_me_plenty();
    mark_repaint_needed();
}

void set_env_cmd_duration(struct timeval *after, struct timeval *before, env_stack_t &vars) {
    time_t secs = after->tv_sec - before->tv_sec;
    suseconds_t usecs = after->tv_usec - before->tv_usec;

    if (after->tv_usec < before->tv_usec) {
        usecs += 1000000;
        secs -= 1;
    }

    vars.set_one(ENV_CMD_DURATION, ENV_UNEXPORT, std::to_wstring((secs * 1000) + (usecs / 1000)));
}

void reader_run_command(parser_t &parser, const wcstring &cmd) {
    struct timeval time_before, time_after;

    wcstring ft = tok_command(cmd);

    // For compatibility with fish 2.0's $_, now replaced with `status current-command`
    if (!ft.empty()) parser.vars().set_one(L"_", ENV_GLOBAL, ft);

    outputter_t &outp = outputter_t::stdoutput();
    reader_write_title(cmd, parser);
    term_donate(outp);

    gettimeofday(&time_before, nullptr);

    parser.eval(cmd, io_chain_t{});
    job_reap(parser, true);

    gettimeofday(&time_after, nullptr);

    // update the execution duration iff a command is requested for execution
    // issue - #4926
    if (!ft.empty()) set_env_cmd_duration(&time_after, &time_before, parser.vars());

    term_steal();

    // For compatibility with fish 2.0's $_, now replaced with `status current-command`
    parser.vars().set_one(L"_", ENV_GLOBAL, program_name);

    if (have_proc_stat()) {
        proc_update_jiffies(parser);
    }
}

parser_test_error_bits_t reader_shell_test(parser_t &parser, const wcstring &b) {
    wcstring bstr = b;

    // Append a newline, to act as a statement terminator.
    bstr.push_back(L'\n');

    parse_error_list_t errors;
    parser_test_error_bits_t res =
        parse_util_detect_errors(bstr, &errors, true /* do accept incomplete */);

    if (res & PARSER_TEST_ERROR) {
        wcstring error_desc;
        parser.get_backtrace(bstr, errors, error_desc);

        // Ensure we end with a newline. Also add an initial newline, because it's likely the user
        // just hit enter and so there's junk on the current line.
        if (!string_suffixes_string(L"\n", error_desc)) {
            error_desc.push_back(L'\n');
        }
        std::fwprintf(stderr, L"\n%ls", error_desc.c_str());
    }
    return res;
}

/// Called to set the highlight flag for search results.
void reader_data_t::highlight_search() {
    if (history_search.is_at_end()) {
        return;
    }
    const wcstring &needle = history_search.search_string();
    const editable_line_t *el = &command_line;
    size_t match_pos = el->text().find(needle);
    if (match_pos != wcstring::npos) {
        size_t end = match_pos + needle.size();
        for (size_t i = match_pos; i < end; i++) {
            colors.at(i).background = highlight_role_t::search_match;
        }
    }
}

void reader_data_t::highlight_complete(highlight_result_t result) {
    ASSERT_IS_MAIN_THREAD();
    if (result.text == command_line.text()) {
        // The data hasn't changed, so swap in our colors. The colors may not have changed, so do
        // nothing if they have not.
        assert(result.colors.size() == command_line.size());
        if (colors != result.colors) {
            colors = std::move(result.colors);
            sanity_check();
            highlight_search();
            repaint();
        }
    }
}

// Given text, bracket matching position, and whether IO is allowed,
// return a function that performs highlighting. The function may be invoked on a background thread.
static std::function<highlight_result_t(void)> get_highlight_performer(
    parser_t &parser, const wcstring &text, long match_highlight_pos,
    highlight_function_t highlight_func) {
    auto vars = parser.vars().snapshot();
    unsigned generation_count = read_generation_count();
    return [=]() -> highlight_result_t {
        if (text.empty()) return {};
        operation_context_t ctx = get_bg_context(vars, generation_count);
        std::vector<highlight_spec_t> colors(text.size(), highlight_spec_t{});
        highlight_func(text, colors, match_highlight_pos, ctx);
        return {std::move(colors), text};
    };
}

/// Call specified external highlighting function and then do search highlighting. Lastly, clear the
/// background color under the cursor to avoid repaint issues on terminals where e.g. syntax
/// highlighting maykes characters under the sursor unreadable.
///
/// \param match_highlight_pos_adjust the adjustment to the position to use for bracket matching.
///        This is added to the current cursor position and may be negative.
/// \param no_io if true, do a highlight that does not perform I/O, synchronously. If false, perform
///        an asynchronous highlight in the background, which may perform disk I/O.
void reader_data_t::super_highlight_me_plenty(int match_highlight_pos_adjust, bool no_io) {
    const editable_line_t *el = &command_line;
    assert(el != nullptr);
    long match_highlight_pos = static_cast<long>(el->position()) + match_highlight_pos_adjust;
    assert(match_highlight_pos >= 0);

    sanity_check();

    auto highlight_performer = get_highlight_performer(
        parser(), el->text(), match_highlight_pos, no_io ? highlight_shell_no_io : highlight_func);
    if (no_io) {
        // Highlighting without IO, we just do it.
        highlight_complete(highlight_performer());
    } else {
        // Highlighting including I/O proceeds in the background.
        auto shared_this = this->shared_from_this();
        debounce_highlighting().perform(highlight_performer,
                                        [shared_this](highlight_result_t result) {
                                            shared_this->highlight_complete(std::move(result));
                                        });
    }
    highlight_search();

    // Here's a hack. Check to see if our autosuggestion still applies; if so, don't recompute it.
    // Since the autosuggestion computation is asynchronous, this avoids "flashing" as you type into
    // the autosuggestion.
    const wcstring &cmd = el->text(), &suggest = autosuggestion;
    if (can_autosuggest() && !suggest.empty() &&
        string_prefixes_string_case_insensitive(cmd, suggest)) {
        // the autosuggestion is still reasonable, so do nothing
    } else {
        update_autosuggestion();
    }
}

/// The stack of current interactive reading contexts.
static std::vector<std::shared_ptr<reader_data_t>> reader_data_stack;

/// Access the top level reader data.
static reader_data_t *current_data_or_null() {
    ASSERT_IS_MAIN_THREAD();
    return reader_data_stack.empty() ? nullptr : reader_data_stack.back().get();
}

static reader_data_t *current_data() {
    ASSERT_IS_MAIN_THREAD();
    assert(!reader_data_stack.empty() && "no current reader");
    return reader_data_stack.back().get();
}

void reader_change_history(const wcstring &name) {
    // We don't need to _change_ if we're not initialized yet.
    reader_data_t *data = current_data_or_null();
    if (data && data->history) {
        data->history->save();
        data->history = &history_t::history_with_name(name);
    }
}

void reader_push(parser_t &parser, const wcstring &name) {
    history_t *hist = &history_t::history_with_name(name);
    reader_data_stack.push_back(std::make_shared<reader_data_t>(parser.shared(), hist));
    reader_data_t *data = current_data();
    data->command_line_changed(&data->command_line);
    if (reader_data_stack.size() == 1) {
        reader_interactive_init(parser);
    }

    data->exec_prompt();
}

void reader_pop() {
    assert(!reader_data_stack.empty() && "empty stack in reader_data_stack");
    reader_data_stack.pop_back();
    reader_data_t *new_reader = current_data_or_null();
    if (new_reader == nullptr) {
        reader_interactive_destroy();
    } else {
        s_end_current_loop = false;
        s_reset(&new_reader->screen, screen_reset_mode_t::abandon_line);
    }
}

void reader_set_left_prompt(const wcstring &new_prompt) {
    current_data()->left_prompt = new_prompt;
}

void reader_set_right_prompt(const wcstring &new_prompt) {
    current_data()->right_prompt = new_prompt;
}

void reader_set_allow_autosuggesting(bool flag) { current_data()->allow_autosuggestion = flag; }

void reader_set_expand_abbreviations(bool flag) { current_data()->expand_abbreviations = flag; }

void reader_set_complete_ok(bool flag) { current_data()->complete_ok = flag; }

void reader_set_highlight_function(highlight_function_t func) {
    current_data()->highlight_func = func;
}

void reader_set_test_function(test_function_t f) { current_data()->test_func = f; }

void reader_set_exit_on_interrupt(bool i) { current_data()->exit_on_interrupt = i; }

void reader_set_silent_status(bool flag) { current_data()->silent = flag; }

void reader_import_history_if_necessary() {
    // Import history from older location (config path) if our current history is empty.
    reader_data_t *data = current_data();
    if (data->history && data->history->is_empty()) {
        data->history->populate_from_config_path();
    }

    // Import history from bash, etc. if our current history is still empty and is the default
    // history.
    if (data->history && data->history->is_empty() && data->history->is_default()) {
        // Try opening a bash file. We make an effort to respect $HISTFILE; this isn't very complete
        // (AFAIK it doesn't have to be exported), and to really get this right we ought to ask bash
        // itself. But this is better than nothing.
        const auto var = data->vars().get(L"HISTFILE");
        wcstring path = (var ? var->as_string() : L"~/.bash_history");
        expand_tilde(path, data->vars());
        int fd = wopen_cloexec(path, O_RDONLY);
        if (fd >= 0) {
            FILE *f = fdopen(fd, "r");
            data->history->populate_from_bash(f);
            fclose(f);
        }
    }
}

bool shell_is_exiting() { return should_exit(); }

void reader_bg_job_warning(const job_list_t &jobs) {
    std::fputws(_(L"There are still jobs active:\n"), stdout);
    std::fputws(_(L"\n   PID  Command\n"), stdout);

    for (const auto &j : jobs) {
        std::fwprintf(stdout, L"%6d  %ls\n", j->processes[0]->pid, j->command_wcstr());
    }
    fputws(L"\n", stdout);
    fputws(_(L"A second attempt to exit will terminate them.\n"), stdout);
    fputws(_(L"Use 'disown PID' to remove jobs from the list without terminating them.\n"), stdout);
}

/// This function is called when the main loop notices that end_loop has been set while in
/// interactive mode. It checks if it is ok to exit.
static void handle_end_loop(const parser_t &parser) {
    if (!reader_exit_forced()) {
        for (const auto &b : parser.blocks()) {
            if (b.type() == block_type_t::breakpoint) {
                // We're executing within a breakpoint so we do not want to terminate the shell and
                // kill background jobs.
                return;
            }
        }

        // Perhaps print a warning before exiting.
        reader_data_t *data = current_data();
        auto bg_jobs = jobs_requiring_warning_on_exit(parser);
        if (!data->prev_end_loop && !bg_jobs.empty()) {
            print_exit_warning_for_jobs(bg_jobs);
            reader_set_end_loop(false);
            data->prev_end_loop = true;
            return;
        }
    }

    // Kill remaining jobs before exiting.
    hup_jobs(parser.jobs());
}

static bool selection_is_at_top() {
    reader_data_t *data = current_data();
    const pager_t *pager = &data->pager;
    size_t row = pager->get_selected_row(data->current_page_rendering);
    if (row != 0 && row != PAGER_SELECTION_NONE) return false;

    size_t col = pager->get_selected_column(data->current_page_rendering);
    return !(col != 0 && col != PAGER_SELECTION_NONE);
}

static relaxed_atomic_t<uint64_t> run_count{0};

/// Returns the current interactive loop count
uint64_t reader_run_count() { return run_count; }

/// Read interactively. Read input from stdin while providing editing facilities.
static int read_i(parser_t &parser) {
    reader_push(parser, history_session_id(parser.vars()));
    reader_set_complete_ok(true);
    reader_set_highlight_function(&highlight_shell);
    reader_set_test_function(&reader_shell_test);
    reader_set_allow_autosuggesting(true);
    reader_set_expand_abbreviations(true);
    reader_import_history_if_necessary();

    reader_data_t *data = current_data();
    data->prev_end_loop = false;

    while (!shell_is_exiting()) {
        run_count++;

        if (parser.libdata().is_breakpoint && function_exists(DEBUG_PROMPT_FUNCTION_NAME, parser)) {
            reader_set_left_prompt(DEBUG_PROMPT_FUNCTION_NAME);
        } else if (function_exists(LEFT_PROMPT_FUNCTION_NAME, parser)) {
            reader_set_left_prompt(LEFT_PROMPT_FUNCTION_NAME);
        } else {
            reader_set_left_prompt(DEFAULT_PROMPT);
        }

        if (function_exists(RIGHT_PROMPT_FUNCTION_NAME, parser)) {
            reader_set_right_prompt(RIGHT_PROMPT_FUNCTION_NAME);
        } else {
            reader_set_right_prompt(L"");
        }

        // Put buff in temporary string and clear buff, so that we can handle a call to
        // reader_set_buffer during evaluation.
        maybe_t<wcstring> tmp = reader_readline(0);

        if (shell_is_exiting()) {
            handle_end_loop(parser);
        } else if (tmp) {
            const wcstring command = tmp.acquire();
            data->update_buff_pos(&data->command_line, 0);
            data->command_line.clear();
            data->command_line_changed(&data->command_line);
            wcstring_list_t argv(1, command);
            event_fire_generic(parser, L"fish_preexec", &argv);
            reader_run_command(parser, command);
            parser.clear_cancel();
            event_fire_generic(parser, L"fish_postexec", &argv);
            // Allow any pending history items to be returned in the history array.
            if (data->history) {
                data->history->resolve_pending();
            }
            if (shell_is_exiting()) {
                handle_end_loop(parser);
            } else {
                data->prev_end_loop = false;
            }
        }
    }

    reader_pop();
    return 0;
}

/// Test if there are bytes available for reading on the specified file descriptor.
static int can_read(int fd) {
    struct timeval can_read_timeout = {0, 0};
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    return select(fd + 1, &fds, nullptr, nullptr, &can_read_timeout) == 1;
}

/// Test if the specified character in the specified string is backslashed. pos may be at the end of
/// the string, which indicates if there is a trailing backslash.
static bool is_backslashed(const wcstring &str, size_t pos) {
    // note pos == str.size() is OK.
    if (pos > str.size()) return false;

    size_t count = 0, idx = pos;
    while (idx--) {
        if (str.at(idx) != L'\\') break;
        count++;
    }

    return (count % 2) == 1;
}

static wchar_t unescaped_quote(const wcstring &str, size_t pos) {
    wchar_t result = L'\0';
    if (pos < str.size()) {
        wchar_t c = str.at(pos);
        if ((c == L'\'' || c == L'"') && !is_backslashed(str, pos)) {
            result = c;
        }
    }
    return result;
}

/// Returns true if the last token is a comment.
static bool text_ends_in_comment(const wcstring &text) {
    tokenizer_t tok(text.c_str(), TOK_ACCEPT_UNFINISHED | TOK_SHOW_COMMENTS);
    bool is_comment = false;
    while (auto token = tok.next()) {
        is_comment = token->type == token_type_t::comment;
    }
    return is_comment;
}

/// \return true if an event is a normal character that should be inserted into the buffer.
static bool event_is_normal_char(const char_event_t &evt) {
    if (!evt.is_char()) return false;
    auto c = evt.get_char();
    return !fish_reserved_codepoint(c) && c > 31 && c != 127;
}

/// readline_loop_state_t encapsulates the state used in a readline loop.
/// It is always stack allocated transient. This state should not be "publicly visible"; public
/// state should be in reader_data_t.
struct readline_loop_state_t {
    /// The last command that was executed.
    maybe_t<readline_cmd_t> last_cmd{};

    /// If the last command was a yank, the length of yanking that occurred.
    size_t yank_len{0};

    /// If the last "complete" readline command has inserted text into the command line.
    bool complete_did_insert{true};

    /// List of completions.
    completion_list_t comp;

    /// Whether we are skipping redundant repaints.
    bool coalescing_repaints = false;

    /// Whether the loop has finished, due to reaching the character limit or through executing a
    /// command.
    bool finished{false};

    /// Maximum number of characters to read.
    size_t nchars{std::numeric_limits<size_t>::max()};
};

/// Read normal characters, inserting them into the command line.
/// \return the next unhandled event.
maybe_t<char_event_t> reader_data_t::read_normal_chars(readline_loop_state_t &rls) {
    maybe_t<char_event_t> event_needing_handling{};
    wcstring accumulated_chars;
    size_t limit = std::min(rls.nchars - command_line.size(), READAHEAD_MAX);
    while (accumulated_chars.size() < limit) {
        bool allow_commands = (accumulated_chars.empty());
        auto evt = inputter.readch(allow_commands);
        if (!event_is_normal_char(evt) || !can_read(STDIN_FILENO)) {
            event_needing_handling = std::move(evt);
            break;
        } else if (evt.input_style == char_input_style_t::notfirst && accumulated_chars.empty() &&
                   active_edit_line()->position() == 0) {
            // The cursor is at the beginning and nothing is accumulated, so skip this character.
            continue;
        } else {
            accumulated_chars.push_back(evt.get_char());
        }
    }

    if (!accumulated_chars.empty()) {
        editable_line_t *el = active_edit_line();
        insert_string(el, accumulated_chars);

        // End paging upon inserting into the normal command line.
        if (el == &command_line) {
            clear_pager();
        }

        // Since we handled a normal character, we don't have a last command.
        rls.last_cmd.reset();
    }
    return event_needing_handling;
}

/// Handle a readline command \p c, updating the state \p rls.
void reader_data_t::handle_readline_command(readline_cmd_t c, readline_loop_state_t &rls) {
    const auto &vars = this->vars();
    using rl = readline_cmd_t;
    switch (c) {
        // Go to beginning of line.
        case rl::beginning_of_line: {
            editable_line_t *el = active_edit_line();
            while (el->position() > 0 && el->text().at(el->position() - 1) != L'\n') {
                update_buff_pos(el, el->position() - 1);
            }
            mark_repaint_needed();
            break;
        }
        case rl::end_of_line: {
            editable_line_t *el = active_edit_line();
            if (el->position() < el->size()) {
                const wchar_t *buff = el->text().c_str();
                while (buff[el->position()] && buff[el->position()] != L'\n') {
                    update_buff_pos(el, el->position() + 1);
                }
            } else {
                accept_autosuggestion(true);
            }

            mark_repaint_needed();
            break;
        }
        case rl::beginning_of_buffer: {
            update_buff_pos(&command_line, 0);
            mark_repaint_needed();
            break;
        }
        case rl::end_of_buffer: {
            update_buff_pos(&command_line, command_line.size());
            mark_repaint_needed();
            break;
        }
        case rl::cancel: {
            // The only thing we can cancel right now is paging, which we handled up above.
            break;
        }
        case rl::repaint_mode: {
            // Repaint the mode-prompt only if possible.
            // This is an optimization basically exclusively for vi-mode, since the prompt
            // may sometimes take a while but when switching the mode all we care about is the
            // mode-prompt.
            //
            // Because some users set `fish_mode_prompt` to an empty function and display the mode
            // elsewhere, we detect if the mode output is empty.
            exec_mode_prompt();
            if (!mode_prompt_buff.empty()) {
                s_reset(&screen, screen_reset_mode_t::current_line_and_prompt);
                screen_reset_needed = false;
                repaint();
                break;
            }
            // Else we repaint as normal.
            /* fallthrough */
        }
        case rl::force_repaint:
        case rl::repaint: {
            if (!rls.coalescing_repaints) {
                rls.coalescing_repaints = true;
                exec_prompt();
                s_reset(&screen, screen_reset_mode_t::current_line_and_prompt);
                screen_reset_needed = false;
                repaint();
            }
            break;
        }
        case rl::complete:
        case rl::complete_and_search: {
            if (!complete_ok) break;

            // Use the command line only; it doesn't make sense to complete in any other line.
            editable_line_t *el = &command_line;
            if (is_navigating_pager_contents() ||
                (!rls.complete_did_insert && rls.last_cmd == rl::complete)) {
                // The user typed complete more than once in a row. If we are not yet fully
                // disclosed, then become so; otherwise cycle through our available completions.
                if (current_page_rendering.remaining_to_disclose > 0) {
                    pager.set_fully_disclosed(true);
                    mark_repaint_needed();
                } else {
                    select_completion_in_direction(c == rl::complete ? selection_motion_t::next
                                                                     : selection_motion_t::prev);
                }
            } else {
                // Either the user hit tab only once, or we had no visible completion list.
                // Remove a trailing backslash. This may trigger an extra repaint, but this is
                // rare.
                if (is_backslashed(el->text(), el->position())) {
                    delete_char();
                }

                // Get the string; we have to do this after removing any trailing backslash.
                const wchar_t *const buff = el->text().c_str();

                // Figure out the extent of the command substitution surrounding the cursor.
                // This is because we only look at the current command substitution to form
                // completions - stuff happening outside of it is not interesting.
                const wchar_t *cmdsub_begin, *cmdsub_end;
                parse_util_cmdsubst_extent(buff, el->position(), &cmdsub_begin, &cmdsub_end);

                // Figure out the extent of the token within the command substitution. Note we
                // pass cmdsub_begin here, not buff.
                const wchar_t *token_begin, *token_end;
                parse_util_token_extent(cmdsub_begin, el->position() - (cmdsub_begin - buff),
                                        &token_begin, &token_end, nullptr, nullptr);

                // Hack: the token may extend past the end of the command substitution, e.g. in
                // (echo foo) the last token is 'foo)'. Don't let that happen.
                if (token_end > cmdsub_end) token_end = cmdsub_end;

                // Construct a copy of the string from the beginning of the command substitution
                // up to the end of the token we're completing.
                const wcstring buffcpy = wcstring(cmdsub_begin, token_end);

                // std::fwprintf(stderr, L"Complete (%ls)\n", buffcpy.c_str());
                completion_request_flags_t complete_flags = {completion_request_t::descriptions,
                                                             completion_request_t::fuzzy_match};
                rls.comp = complete(buffcpy, complete_flags, parser_ref->context());

                // User-supplied completions may have changed the commandline - prevent buffer
                // overflow.
                if (token_begin > buff + el->text().size()) token_begin = buff + el->text().size();
                if (token_end > buff + el->text().size()) token_end = buff + el->text().size();

                // Munge our completions.
                completions_sort_and_prioritize(&rls.comp);

                // Record our cycle_command_line.
                cycle_command_line = el->text();
                cycle_cursor_pos = token_end - buff;

                rls.complete_did_insert =
                    handle_completions(rls.comp, token_begin - buff, token_end - buff);

                // Show the search field if requested and if we printed a list of completions.
                if (c == rl::complete_and_search && !rls.complete_did_insert && !pager.empty()) {
                    pager.set_search_field_shown(true);
                    select_completion_in_direction(selection_motion_t::next);
                    mark_repaint_needed();
                }
            }
            break;
        }
        case rl::pager_toggle_search: {
            if (!pager.empty()) {
                // Toggle search, and begin navigating if we are now searching.
                bool sfs = pager.is_search_field_shown();
                pager.set_search_field_shown(!sfs);
                pager.set_fully_disclosed(true);
                if (pager.is_search_field_shown() && !is_navigating_pager_contents()) {
                    select_completion_in_direction(selection_motion_t::south);
                }
                mark_repaint_needed();
            }
            break;
        }
        case rl::kill_line: {
            editable_line_t *el = active_edit_line();
            const wchar_t *buff = el->text().c_str();
            const wchar_t *begin = &buff[el->position()];
            const wchar_t *end = begin;

            while (*end && *end != L'\n') end++;

            if (end == begin && *end) end++;

            size_t len = end - begin;
            if (len) {
                kill(el, begin - buff, len, KILL_APPEND, rls.last_cmd != rl::kill_line);
            }
            break;
        }
        case rl::backward_kill_line: {
            editable_line_t *el = active_edit_line();
            if (el->position() <= 0) {
                break;
            }
            const wchar_t *buff = el->text().c_str();
            const wchar_t *end = &buff[el->position()];
            const wchar_t *begin = end;

            begin--;  // make sure we delete at least one character (see issue #580)

            // Delete until we hit a newline, or the beginning of the string.
            while (begin > buff && *begin != L'\n') begin--;

            // If we landed on a newline, don't delete it.
            if (*begin == L'\n') begin++;
            assert(end >= begin);
            size_t len = std::max<size_t>(end - begin, 1);
            begin = end - len;
            kill(el, begin - buff, len, KILL_PREPEND, rls.last_cmd != rl::backward_kill_line);
            break;
        }
        case rl::kill_whole_line: {
            // We match the emacs behavior here: "kills the entire line including the following
            // newline".
            editable_line_t *el = active_edit_line();
            const wchar_t *buff = el->text().c_str();

            // Back up to the character just past the previous newline, or go to the beginning
            // of the command line. Note that if the position is on a newline, visually this
            // looks like the cursor is at the end of a line. Therefore that newline is NOT the
            // beginning of a line; this justifies the -1 check.
            size_t begin = el->position();
            while (begin > 0 && buff[begin - 1] != L'\n') {
                begin--;
            }

            // Push end forwards to just past the next newline, or just past the last char.
            size_t end = el->position();
            while (buff[end] != L'\0') {
                end++;
                if (buff[end - 1] == L'\n') {
                    break;
                }
            }
            assert(end >= begin);

            if (end > begin) {
                kill(el, begin, end - begin, KILL_APPEND, rls.last_cmd != rl::kill_whole_line);
            }
            break;
        }
        case rl::yank: {
            wcstring yank_str = kill_yank();
            insert_string(active_edit_line(), yank_str);
            rls.yank_len = yank_str.size();
            break;
        }
        case rl::yank_pop: {
            if (rls.yank_len) {
                editable_line_t *el = active_edit_line();
                wcstring yank_str = kill_yank_rotate();
                size_t new_yank_len = yank_str.size();
                el->replace_substring(el->position() - rls.yank_len, rls.yank_len,
                                      std::move(yank_str));
                update_buff_pos(el);
                rls.yank_len = new_yank_len;
                command_line_changed(el);
                suppress_autosuggestion = true;
                super_highlight_me_plenty();
                mark_repaint_needed();
            }
            break;
        }
        case rl::backward_delete_char: {
            delete_char();
            break;
        }
        case rl::delete_or_exit:
        case rl::delete_char: {
            // Remove the current character in the character buffer and on the screen using
            // syntax highlighting, etc.
            editable_line_t *el = active_edit_line();
            if (el->position() < el->size()) {
                delete_char(false /* backward */);
            } else if (c == rl::delete_or_exit && el->empty()) {
                reader_set_end_loop(true);
            }
            break;
        }
            // Evaluate. If the current command is unfinished, or if the charater is escaped
            // using a backslash, insert a newline.
        case rl::execute: {
            // If the user hits return while navigating the pager, it only clears the pager.
            if (is_navigating_pager_contents()) {
                clear_pager();
                break;
            }

            // Delete any autosuggestion.
            autosuggestion.clear();

            // The user may have hit return with pager contents, but while not navigating them.
            // Clear the pager in that event.
            clear_pager();

            // We only execute the command line.
            editable_line_t *el = &command_line;

            // Allow backslash-escaped newlines.
            bool continue_on_next_line = false;
            if (el->position() >= el->size()) {
                // We're at the end of the text and not in a comment (issue #1225).
                continue_on_next_line =
                    is_backslashed(el->text(), el->position()) && !text_ends_in_comment(el->text());
            } else {
                // Allow mid line split if the following character is whitespace (issue #613).
                if (is_backslashed(el->text(), el->position()) &&
                    iswspace(el->text().at(el->position()))) {
                    continue_on_next_line = true;
                    // Check if the end of the line is backslashed (issue #4467).
                } else if (is_backslashed(el->text(), el->size()) &&
                           !text_ends_in_comment(el->text())) {
                    // Move the cursor to the end of the line.
                    el->set_position(el->size());
                    continue_on_next_line = true;
                }
            }
            // If the conditions are met, insert a new line at the position of the cursor.
            if (continue_on_next_line) {
                insert_char(el, L'\n');
                break;
            }

            // See if this command is valid.
            int command_test_result = test_func(parser(), el->text());
            if (command_test_result == 0 || command_test_result == PARSER_TEST_INCOMPLETE) {
                // This command is valid, but an abbreviation may make it invalid. If so, we
                // will have to test again.
                bool abbreviation_expanded = expand_abbreviation_as_necessary(1);
                if (abbreviation_expanded) {
                    // It's our reponsibility to rehighlight and repaint. But everything we do
                    // below triggers a repaint.
                    command_test_result = test_func(parser(), el->text());

                    // If the command is OK, then we're going to execute it. We still want to do
                    // syntax highlighting, but a synchronous variant that performs no I/O, so
                    // as not to block the user.
                    bool skip_io = (command_test_result == 0);
                    super_highlight_me_plenty(0, skip_io);
                }
            }

            if (command_test_result == 0) {
                // Finished command, execute it. Don't add items that start with a leading
                // space.
                const editable_line_t *el = &command_line;
                if (history != nullptr && may_add_to_history(el->text())) {
                    history->add_pending_with_file_detection(el->text(), vars.get_pwd_slash());
                }
                rls.finished = true;
                update_buff_pos(&command_line, command_line.size());
                repaint();
            } else if (command_test_result == PARSER_TEST_INCOMPLETE) {
                // We are incomplete, continue editing.
                insert_char(el, L'\n');
            } else {
                // Result must be some combination including an error. The error message will
                // already be printed, all we need to do is repaint.
                wcstring_list_t argv(1, el->text());
                event_fire_generic(parser(), L"fish_posterror", &argv);
                s_reset(&screen, screen_reset_mode_t::abandon_line);
                mark_repaint_needed();
            }

            break;
        }

        case rl::history_prefix_search_backward:
        case rl::history_prefix_search_forward:
        case rl::history_search_backward:
        case rl::history_search_forward:
        case rl::history_token_search_backward:
        case rl::history_token_search_forward: {
            reader_history_search_t::mode_t mode =
                (c == rl::history_token_search_backward || c == rl::history_token_search_forward)
                    ? reader_history_search_t::token
                    : (c == rl::history_prefix_search_backward ||
                       c == rl::history_prefix_search_forward)
                          ? reader_history_search_t::prefix
                          : reader_history_search_t::line;

            if (history_search.is_at_end()) {
                const editable_line_t *el = &command_line;
                if (mode == reader_history_search_t::token) {
                    // Searching by token.
                    const wchar_t *begin, *end;
                    const wchar_t *buff = el->text().c_str();
                    parse_util_token_extent(buff, el->position(), &begin, &end, nullptr, nullptr);
                    if (begin) {
                        wcstring token(begin, end);
                        history_search.reset_to_mode(token, history,
                                                     reader_history_search_t::token);
                    } else {
                        // No current token, refuse to do a token search.
                        history_search.reset();
                    }
                } else {
                    // Searching by line.
                    history_search.reset_to_mode(el->text(), history, mode);

                    // Skip the autosuggestion in the history unless it was truncated.
                    const wcstring &suggest = autosuggestion;
                    if (!suggest.empty() && !screen.autosuggestion_is_truncated &&
                        mode != reader_history_search_t::prefix) {
                        history_search.add_skip(suggest);
                    }
                }
            }
            if (history_search.active()) {
                history_search_direction_t dir =
                    (c == rl::history_search_backward || c == rl::history_token_search_backward ||
                     c == rl::history_prefix_search_backward)
                        ? history_search_direction_t::backward
                        : history_search_direction_t::forward;
                history_search.move_in_direction(dir);
                update_command_line_from_history_search();
            }
            break;
        }
        case rl::backward_char: {
            editable_line_t *el = active_edit_line();
            if (is_navigating_pager_contents()) {
                select_completion_in_direction(selection_motion_t::west);
            } else if (el->position() > 0) {
                update_buff_pos(el, el->position() - 1);
                mark_repaint_needed();
            }
            break;
        }
        case rl::forward_char: {
            editable_line_t *el = active_edit_line();
            if (is_navigating_pager_contents()) {
                select_completion_in_direction(selection_motion_t::east);
            } else if (el->position() < el->size()) {
                update_buff_pos(el, el->position() + 1);
                mark_repaint_needed();
            } else {
                accept_autosuggestion(true);
            }
            break;
        }
        case rl::backward_kill_word:
        case rl::backward_kill_path_component:
        case rl::backward_kill_bigword: {
            move_word_style_t style =
                (c == rl::backward_kill_bigword
                     ? move_word_style_whitespace
                     : c == rl::backward_kill_path_component ? move_word_style_path_components
                                                             : move_word_style_punctuation);
            // Is this the same killring item as the last kill?
            bool newv = (rls.last_cmd != rl::backward_kill_word &&
                         rls.last_cmd != rl::backward_kill_path_component &&
                         rls.last_cmd != rl::backward_kill_bigword);
            move_word(active_edit_line(), MOVE_DIR_LEFT, true /* erase */, style, newv);
            break;
        }
        case rl::kill_word:
        case rl::kill_bigword: {
            // The "bigword" functions differ only in that they move to the next whitespace, not
            // punctuation.
            auto move_style =
                (c == rl::kill_word) ? move_word_style_punctuation : move_word_style_whitespace;
            move_word(active_edit_line(), MOVE_DIR_RIGHT, true /* erase */, move_style,
                      rls.last_cmd != c /* same kill item if same movement */);
            break;
        }
        case rl::backward_word:
        case rl::backward_bigword: {
            auto move_style =
                (c == rl::backward_word) ? move_word_style_punctuation : move_word_style_whitespace;
            move_word(active_edit_line(), MOVE_DIR_LEFT, false /* do not erase */, move_style,
                      false);
            break;
        }
        case rl::forward_word:
        case rl::forward_bigword: {
            auto move_style =
                (c == rl::forward_word) ? move_word_style_punctuation : move_word_style_whitespace;
            editable_line_t *el = active_edit_line();
            if (el->position() < el->size()) {
                move_word(el, MOVE_DIR_RIGHT, false /* do not erase */, move_style, false);
            } else {
                accept_autosuggestion(false, move_style);
            }
            break;
        }
        case rl::beginning_of_history:
        case rl::end_of_history: {
            bool up = (c == rl::beginning_of_history);
            if (is_navigating_pager_contents()) {
                select_completion_in_direction(up ? selection_motion_t::page_north
                                                  : selection_motion_t::page_south);
            } else {
                if (up) {
                    history_search.go_to_beginning();
                } else {
                    history_search.go_to_end();
                }
                update_command_line_from_history_search();
            }
            break;
        }
        case rl::up_line:
        case rl::down_line: {
            if (is_navigating_pager_contents()) {
                // We are already navigating pager contents.
                selection_motion_t direction;
                if (c == rl::down_line) {
                    // Down arrow is always south.
                    direction = selection_motion_t::south;
                } else if (selection_is_at_top()) {
                    // Up arrow, but we are in the first column and first row. End navigation.
                    direction = selection_motion_t::deselect;
                } else {
                    // Up arrow, go north.
                    direction = selection_motion_t::north;
                }

                // Now do the selection.
                select_completion_in_direction(direction);
            } else if (!pager.empty()) {
                // We pressed a direction with a non-empty pager, begin navigation.
                select_completion_in_direction(c == rl::down_line ? selection_motion_t::south
                                                                  : selection_motion_t::north);
            } else {
                // Not navigating the pager contents.
                editable_line_t *el = active_edit_line();
                int line_old = parse_util_get_line_from_offset(el->text(), el->position());
                int line_new;

                if (c == rl::up_line)
                    line_new = line_old - 1;
                else
                    line_new = line_old + 1;

                int line_count = parse_util_lineno(el->text().c_str(), el->size()) - 1;

                if (line_new >= 0 && line_new <= line_count) {
                    size_t base_pos_new;
                    size_t base_pos_old;

                    int indent_old;
                    int indent_new;
                    size_t line_offset_old;
                    size_t total_offset_new;

                    base_pos_new = parse_util_get_offset_from_line(el->text(), line_new);

                    base_pos_old = parse_util_get_offset_from_line(el->text(), line_old);

                    assert(base_pos_new != static_cast<size_t>(-1) &&
                           base_pos_old != static_cast<size_t>(-1));
                    indent_old = indents.at(base_pos_old);
                    indent_new = indents.at(base_pos_new);

                    line_offset_old =
                        el->position() - parse_util_get_offset_from_line(el->text(), line_old);
                    total_offset_new = parse_util_get_offset(
                        el->text(), line_new, line_offset_old - 4 * (indent_new - indent_old));
                    update_buff_pos(el, total_offset_new);
                    mark_repaint_needed();
                }
            }
            break;
        }
        case rl::suppress_autosuggestion: {
            suppress_autosuggestion = true;
            autosuggestion.clear();
            mark_repaint_needed();
            break;
        }
        case rl::accept_autosuggestion: {
            accept_autosuggestion(true);
            break;
        }
        case rl::transpose_chars: {
            editable_line_t *el = active_edit_line();
            if (el->size() < 2) {
                break;
            }

            // If the cursor is at the end, transpose the last two characters of the line.
            if (el->position() == el->size()) {
                update_buff_pos(el, el->position() - 1);
            }

            // Drag the character before the cursor forward over the character at the cursor,
            // moving the cursor forward as well.
            if (el->position() > 0) {
                wcstring local_cmd = el->text();
                std::swap(local_cmd.at(el->position()), local_cmd.at(el->position() - 1));
                set_command_line_and_position(el, std::move(local_cmd), el->position() + 1);
            }
            break;
        }
        case rl::transpose_words: {
            editable_line_t *el = active_edit_line();
            size_t len = el->size();
            const wchar_t *buff = el->text().c_str();
            const wchar_t *tok_begin, *tok_end, *prev_begin, *prev_end;

            // If we are not in a token, look for one ahead.
            size_t buff_pos = el->position();
            while (buff_pos != len && !iswalnum(buff[buff_pos])) buff_pos++;

            update_buff_pos(el, buff_pos);

            parse_util_token_extent(buff, el->position(), &tok_begin, &tok_end, &prev_begin,
                                    &prev_end);

            // In case we didn't find a token at or after the cursor...
            if (tok_begin == &buff[len]) {
                // ...retry beginning from the previous token.
                size_t pos = prev_end - &buff[0];
                parse_util_token_extent(buff, pos, &tok_begin, &tok_end, &prev_begin, &prev_end);
            }

            // Make sure we have two tokens.
            if (prev_begin < prev_end && tok_begin < tok_end && tok_begin > prev_begin) {
                const wcstring prev(prev_begin, prev_end - prev_begin);
                const wcstring sep(prev_end, tok_begin - prev_end);
                const wcstring tok(tok_begin, tok_end - tok_begin);
                const wcstring trail(tok_end, &buff[len] - tok_end);

                // Compose new command line with swapped tokens.
                wcstring new_buff(buff, prev_begin - buff);
                new_buff.append(tok);
                new_buff.append(sep);
                new_buff.append(prev);
                new_buff.append(trail);
                // Put cursor right after the second token.
                set_command_line_and_position(el, std::move(new_buff), tok_end - buff);
            }
            break;
        }
        case rl::togglecase_char: {
            editable_line_t *el = active_edit_line();
            size_t buff_pos = el->position();

            // Check that the cursor is on a character
            if (buff_pos < el->size()) {
                wchar_t chr = el->text().at(buff_pos);
                wcstring replacement;

                // Toggle the case of the current character
                bool make_uppercase = iswlower(chr);
                if (make_uppercase) {
                    chr = towupper(chr);
                } else {
                    chr = tolower(chr);
                }

                replacement.push_back(chr);
                el->replace_substring(buff_pos, (size_t)1, std::move(replacement));

                // Restore the buffer position since replace_substring moves
                // the buffer position ahead of the replaced text.
                update_buff_pos(el, buff_pos);

                command_line_changed(el);
                super_highlight_me_plenty();
                mark_repaint_needed();
            }
            break;
        }
        case rl::togglecase_selection: {
            editable_line_t *el = active_edit_line();

            // Check that we have an active selection and get the bounds.
            size_t start, len;
            if (reader_get_selection(&start, &len)) {
                size_t buff_pos = el->position();
                wcstring replacement;

                // Loop through the selected characters and toggle their case.
                for (size_t pos = start; pos < start + len && pos < el->size(); pos++) {
                    wchar_t chr = el->text().at(pos);

                    // Toggle the case of the current character.
                    bool make_uppercase = iswlower(chr);
                    if (make_uppercase) {
                        chr = towupper(chr);
                    } else {
                        chr = tolower(chr);
                    }

                    replacement.push_back(chr);
                }

                el->replace_substring(start, len, std::move(replacement));

                // Restore the buffer position since replace_substring moves
                // the buffer position ahead of the replaced text.
                update_buff_pos(el, buff_pos);

                command_line_changed(el);
                super_highlight_me_plenty();
                mark_repaint_needed();
            }
            break;
        }
        case rl::upcase_word:
        case rl::downcase_word:
        case rl::capitalize_word: {
            editable_line_t *el = active_edit_line();
            // For capitalize_word, whether we've capitalized a character so far.
            bool capitalized_first = false;

            // We apply the operation from the current location to the end of the word.
            size_t pos = el->position();
            size_t init_pos = pos;
            move_word(el, MOVE_DIR_RIGHT, false, move_word_style_punctuation, false);
            wcstring replacement;
            for (; pos < el->position(); pos++) {
                wchar_t chr = el->text().at(pos);

                // We always change the case; this decides whether we go uppercase (true) or
                // lowercase (false).
                bool make_uppercase;
                if (c == rl::capitalize_word)
                    make_uppercase = !capitalized_first && iswalnum(chr);
                else
                    make_uppercase = (c == rl::upcase_word);

                // Apply the operation and then record what we did.
                if (make_uppercase)
                    chr = towupper(chr);
                else
                    chr = towlower(chr);

                replacement.push_back(chr);
                capitalized_first = capitalized_first || make_uppercase;
            }
            el->replace_substring(init_pos, pos - init_pos, std::move(replacement));
            update_buff_pos(el);
            command_line_changed(el);
            super_highlight_me_plenty();
            mark_repaint_needed();
            break;
        }
        case rl::begin_selection:
        case rl::end_selection: {
            sel_start_pos = command_line.position();
            if (c == rl::begin_selection) {
                sel_stop_pos = command_line.position() + 1;
                sel_active = true;
                sel_begin_pos = command_line.position();
            } else {
                sel_stop_pos = command_line.position();
                sel_active = false;
            }

            break;
        }
        case rl::swap_selection_start_stop: {
            if (!sel_active) break;
            size_t tmp = sel_begin_pos;
            sel_begin_pos = command_line.position();
            sel_start_pos = command_line.position();
            editable_line_t *el = active_edit_line();
            update_buff_pos(el, tmp);
            break;
        }
        case rl::kill_selection: {
            bool newv = (rls.last_cmd != rl::kill_selection);
            size_t start, len;
            if (reader_get_selection(&start, &len)) {
                kill(&command_line, start, len, KILL_APPEND, newv);
            }
            break;
        }
        case rl::forward_jump:
        case rl::backward_jump:
        case rl::forward_jump_till:
        case rl::backward_jump_till: {
            auto direction = (c == rl::forward_jump || c == rl::forward_jump_till)
                                 ? jump_direction_t::forward
                                 : jump_direction_t::backward;
            auto precision = (c == rl::forward_jump || c == rl::backward_jump)
                                 ? jump_precision_t::to
                                 : jump_precision_t::till;
            editable_line_t *el = active_edit_line();
            wchar_t target = inputter.function_pop_arg();
            bool success = jump(direction, precision, el, target);

            inputter.function_set_status(success);
            mark_repaint_needed();
            break;
        }
        case rl::repeat_jump: {
            editable_line_t *el = active_edit_line();
            bool success = false;

            if (last_jump_target) {
                success = jump(last_jump_direction, last_jump_precision, el, last_jump_target);
            }

            inputter.function_set_status(success);
            mark_repaint_needed();
            break;
        }
        case rl::reverse_repeat_jump: {
            editable_line_t *el = active_edit_line();
            bool success = false;
            jump_direction_t original_dir, dir;
            original_dir = dir = last_jump_direction;

            if (last_jump_direction == jump_direction_t::forward) {
                dir = jump_direction_t::backward;
            } else {
                dir = jump_direction_t::forward;
            }

            if (last_jump_target) {
                success = jump(dir, last_jump_precision, el, last_jump_target);
            }

            last_jump_direction = original_dir;

            inputter.function_set_status(success);
            mark_repaint_needed();
            break;
        }

        case rl::expand_abbr: {
            if (expand_abbreviation_as_necessary(1)) {
                super_highlight_me_plenty();
                mark_repaint_needed();
                inputter.function_set_status(true);
            } else {
                inputter.function_set_status(false);
            }
            break;
        }
        case rl::undo:
        case rl::redo: {
            editable_line_t *el = active_edit_line();
            bool ok = (c == rl::undo) ? el->undo() : el->redo();
            if (ok) {
                if (el == &command_line) {
                    clear_pager();
                }
                update_buff_pos(el);
                command_line_changed(el);
                super_highlight_me_plenty();
                mark_repaint_needed();
            } else {
                flash();
            }
            break;
        }
            // Some commands should have been handled internally by inputter_t::readch().
        case rl::self_insert:
        case rl::self_insert_notfirst:
        case rl::func_and: {
            DIE("should have been handled by inputter_t::readch");
        }
    }
}

maybe_t<wcstring> reader_data_t::readline(int nchars_or_0) {
    using rl = readline_cmd_t;
    readline_loop_state_t rls{};
    struct termios old_modes;

    // Suppress fish_trace during executing key bindings.
    // This is simply to reduce noise.
    scoped_push<bool> in_title(&parser().libdata().suppress_fish_trace, true);

    // If nchars_or_0 is positive, then that's the maximum number of chars. Otherwise keep it at
    // SIZE_MAX.
    if (nchars_or_0 > 0) {
        rls.nchars = static_cast<size_t>(nchars_or_0);
    }

    // The command line before completion.
    cycle_command_line.clear();
    cycle_cursor_pos = 0;

    history_search.reset();

    s_reset(&screen, screen_reset_mode_t::abandon_line);
    event_fire_generic(parser(), L"fish_prompt");
    exec_prompt();

    super_highlight_me_plenty();
    repaint();

    // Get the current terminal modes. These will be restored when the function returns.
    if (tcgetattr(STDIN_FILENO, &old_modes) == -1 && errno == EIO) redirect_tty_output();

    // Set the new modes.
    if (tcsetattr(0, TCSANOW, &shell_modes) == -1) {
        int err = errno;
        if (err == EIO) {
            redirect_tty_output();
        }
        // This check is required to work around certain issues with fish's approach to
        // terminal control when launching interactive processes while in non-interactive
        // mode. See #4178 for one such example.
        if (err != ENOTTY || session_interactivity() != session_interactivity_t::not_interactive) {
            wperror(L"tcsetattr");
        }
    }

    while (!rls.finished && !shell_is_exiting()) {
        if (rls.nchars <= command_line.size()) {
            // We've already hit the specified character limit.
            rls.finished = true;
            break;
        }

        maybe_t<char_event_t> event_needing_handling{};
        while (true) {
            event_needing_handling = read_normal_chars(rls);
            if (event_needing_handling.has_value()) break;

            if (rls.nchars <= command_line.size()) {
                event_needing_handling.reset();
                break;
            }
        }

        if (!event_needing_handling || event_needing_handling->is_check_exit()) {
            rls.coalescing_repaints = false;
            repaint_if_needed();
            continue;
        } else if (event_needing_handling->is_eof()) {
            reader_force_exit();
            continue;
        }
        assert((event_needing_handling->is_char() || event_needing_handling->is_readline()) &&
               "Should have a char or readline");

        if (rls.last_cmd != rl::yank && rls.last_cmd != rl::yank_pop) {
            rls.yank_len = 0;
        }

        if (event_needing_handling->is_readline()) {
            readline_cmd_t readline_cmd = event_needing_handling->get_readline();
            // If we get something other than a repaint, then stop coalescing them.
            if (readline_cmd != rl::repaint) rls.coalescing_repaints = false;

            if (readline_cmd == rl::cancel && is_navigating_pager_contents()) {
                clear_transient_edit();
            }

            // Clear the pager if necessary.
            bool focused_on_search_field = (active_edit_line() == &pager.search_field_line);
            if (command_ends_paging(readline_cmd, focused_on_search_field)) {
                clear_pager();
            }

            handle_readline_command(readline_cmd, rls);

            if (history_search.active() && command_ends_history_search(readline_cmd)) {
                // "cancel" means to abort the whole thing, other ending commands mean to finish the
                // search.
                if (readline_cmd == rl::cancel) {
                    // Go back to the search string by simply undoing the history-search edit.
                    clear_transient_edit();
                }
                history_search.reset();
            }

            // Readline commands may be bound to \cc which also sets the cancel flag.
            // See #6937.
            parser().clear_cancel();

            rls.last_cmd = readline_cmd;
        } else {
            // Ordinary char.
            wchar_t c = event_needing_handling->get_char();
            if (event_needing_handling->input_style == char_input_style_t::notfirst &&
                active_edit_line()->position() == 0) {
                // This character is skipped.
            } else if (!fish_reserved_codepoint(c) && (c >= L' ' || c == L'\n' || c == L'\r') &&
                       c != 0x7F) {
                // Regular character.
                editable_line_t *el = active_edit_line();
                insert_char(active_edit_line(), c);

                // End paging upon inserting into the normal command line.
                if (el == &command_line) {
                    clear_pager();
                    command_line_has_transient_edit = false;
                }
            } else {
                // This can happen if the user presses a control char we don't recognize. No
                // reason to report this to the user unless they've enabled debugging output.
                FLOGF(reader, _(L"Unknown key binding 0x%X"), c);
            }
            rls.last_cmd = none();
        }

        repaint_if_needed();
    }

    // Emit a newline so that the output is on the line after the command.
    // But do not emit a newline if the cursor has wrapped onto a new line all its own - see #6826.
    if (!screen.cursor_is_wrapped_to_own_line()) {
        ignore_result(write(STDOUT_FILENO, "\n", 1));
    }

    // Ensure we have no pager contents when we exit.
    if (!pager.empty()) {
        // Clear to end of screen to erase the pager contents.
        // TODO: this may fail if eos doesn't exist, in which case we should emit newlines.
        screen_force_clear_to_end();
        pager.clear();
    }

    if (!reader_exit_forced()) {
        // The order of the two conditions below is important. Try to restore the mode
        // in all cases, but only complain if interactive.
        if (tcsetattr(0, TCSANOW, &old_modes) == -1 &&
            session_interactivity() != session_interactivity_t::not_interactive) {
            if (errno == EIO) redirect_tty_output();
            wperror(L"tcsetattr");  // return to previous mode
        }
        outputter_t::stdoutput().set_color(rgb_color_t::reset(), rgb_color_t::reset());
    }

    return rls.finished ? maybe_t<wcstring>{command_line.text()} : none();
}

bool reader_data_t::jump(jump_direction_t dir, jump_precision_t precision, editable_line_t *el,
                         wchar_t target) {
    bool success = false;

    last_jump_target = target;
    last_jump_direction = dir;
    last_jump_precision = precision;

    switch (dir) {
        case jump_direction_t::backward: {
            size_t tmp_pos = el->position();

            while (tmp_pos--) {
                if (el->at(tmp_pos) == target) {
                    if (precision == jump_precision_t::till) {
                        tmp_pos = std::min(el->size() - 1, tmp_pos + 1);
                    }
                    update_buff_pos(el, tmp_pos);
                    success = true;
                    break;
                }
            }
            break;
        }
        case jump_direction_t::forward: {
            for (size_t tmp_pos = el->position() + 1; tmp_pos < el->size(); tmp_pos++) {
                if (el->at(tmp_pos) == target) {
                    if (precision == jump_precision_t::till && tmp_pos) {
                        tmp_pos--;
                    }
                    update_buff_pos(el, tmp_pos);
                    success = true;
                    break;
                }
            }
            break;
        }
    }

    return success;
}

maybe_t<wcstring> reader_readline(int nchars) { return current_data()->readline(nchars); }

bool reader_is_in_search_mode() {
    reader_data_t *data = current_data_or_null();
    return data && data->history_search.active();
}

bool reader_has_pager_contents() {
    reader_data_t *data = current_data_or_null();
    return data && !data->current_page_rendering.screen_data.empty();
}

int reader_reading_interrupted() {
    int res = reader_test_and_clear_interrupted();
    reader_data_t *data = current_data_or_null();
    if (res && data && data->exit_on_interrupt) {
        reader_set_end_loop(true);
        parser_t::cancel_requested(res);
        // We handled the interrupt ourselves, our caller doesn't need to handle it.
        return 0;
    }
    return res;
}

void reader_repaint_needed() {
    if (reader_data_t *data = current_data_or_null()) {
        data->mark_repaint_needed();
    }
}

void reader_repaint_if_needed() {
    if (reader_data_t *data = current_data_or_null()) {
        data->repaint_if_needed();
    }
}

void reader_queue_ch(const char_event_t &ch) {
    if (reader_data_t *data = current_data_or_null()) {
        data->inputter.queue_ch(ch);
    }
}

void reader_react_to_color_change() {
    reader_data_t *data = current_data_or_null();
    if (!data) return;

    if (!data->repaint_needed || !data->screen_reset_needed) {
        data->repaint_needed = true;
        data->screen_reset_needed = true;
        data->inputter.queue_ch(readline_cmd_t::repaint);
    }
}

const wchar_t *reader_get_buffer() {
    ASSERT_IS_MAIN_THREAD();
    reader_data_t *data = current_data_or_null();
    return data ? data->command_line.text().c_str() : nullptr;
}

history_t *reader_get_history() {
    ASSERT_IS_MAIN_THREAD();
    reader_data_t *data = current_data_or_null();
    return data ? data->history : nullptr;
}

void reader_sanity_check() {
    if (reader_data_t *data = current_data_or_null()) {
        data->sanity_check();
    }
}

/// Sets the command line contents, clearing the pager.
void reader_set_buffer(const wcstring &b, size_t pos) {
    reader_data_t *data = current_data_or_null();
    if (!data) return;

    data->clear_pager();
    data->set_buffer_maintaining_pager(b, pos);
}

size_t reader_get_cursor_pos() {
    reader_data_t *data = current_data_or_null();
    if (!data) return static_cast<size_t>(-1);

    return data->command_line.position();
}

bool reader_get_selection(size_t *start, size_t *len) {
    bool result = false;
    reader_data_t *data = current_data_or_null();
    if (data != nullptr && data->sel_active) {
        *start = data->sel_start_pos;
        *len = std::min(data->sel_stop_pos, data->command_line.size()) - data->sel_start_pos;
        result = true;
    }
    return result;
}

/// Read non-interactively.  Read input from stdin without displaying the prompt, using syntax
/// highlighting. This is used for reading scripts and init files.
/// The file is not closed.
static int read_ni(parser_t &parser, int fd, const io_chain_t &io) {
    // Read all data into a std::string.
    std::string fd_contents;
    for (;;) {
        char buff[4096];
        size_t amt = read(fd, buff, sizeof buff);
        if (amt > 0) {
            fd_contents.append(buff, amt);
        } else if (amt == 0) {
            // EOF.
            break;
        } else {
            int err = errno;
            if (err == EINTR) {
                continue;
            } else if ((err == EAGAIN || err == EWOULDBLOCK) && make_fd_blocking(fd)) {
                // We succeeded in making the fd blocking, keep going.
                continue;
            } else {
                // Fatal error.
                FLOGF(error, _(L"Unable to read input file: %s"), strerror(err));
                // Reset buffer on error. We won't evaluate incomplete files.
                fd_contents.clear();
            }
        }
    }

    wcstring str = str2wcstring(fd_contents);

    // Eagerly deallocate to save memory.
    fd_contents.clear();
    fd_contents.shrink_to_fit();

    // Swallow a BOM (issue #1518).
    if (!str.empty() && str.at(0) == UTF8_BOM_WCHAR) {
        str.erase(0, 1);
    }

    parse_error_list_t errors;
    parsed_source_ref_t pstree;
    if (!parse_util_detect_errors(str, &errors, false /* do not accept incomplete */, &pstree)) {
        parser.eval(pstree, io);
        return 0;
    } else {
        wcstring sb;
        parser.get_backtrace(str, errors, sb);
        std::fwprintf(stderr, L"%ls", sb.c_str());
        return 1;
    }
}

int reader_read(parser_t &parser, int fd, const io_chain_t &io) {
    int res;

    // If reader_read is called recursively through the '.' builtin, we need to preserve
    // is_interactive. This, and signal handler setup is handled by
    // proc_push_interactive/proc_pop_interactive.
    bool interactive = false;
    // This block is a hack to work around https://sourceware.org/bugzilla/show_bug.cgi?id=20632.
    // See also, commit 396bf12. Without the need for this workaround we would just write:
    // int inter = ((fd == STDIN_FILENO) && isatty(STDIN_FILENO));
    if (fd == STDIN_FILENO) {
        struct termios t;
        int a_tty = isatty(STDIN_FILENO);
        if (a_tty) {
            interactive = true;
        } else if (tcgetattr(STDIN_FILENO, &t) == -1 && errno == EIO) {
            redirect_tty_output();
            interactive = true;
        }
    }

    scoped_push<bool> interactive_push{&parser.libdata().is_interactive, interactive};
    signal_set_handlers_once(interactive);

    res = interactive ? read_i(parser) : read_ni(parser, fd, io);

    // If the exit command was called in a script, only exit the script, not the program.
    reader_set_end_loop(false);

    return res;
}
