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
#include <functional>
#include <memory>
#include <set>
#include <stack>

#include "ast.h"
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
#include "termsize.h"
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
static std::atomic<uint32_t> s_generation;

/// Helper to get the generation count
static inline uint32_t read_generation_count() {
    return s_generation.load(std::memory_order_relaxed);
}

/// \return an operation context for a background operation..
/// Crucially the operation context itself does not contain a parser.
/// It is the caller's responsibility to ensure the environment lives as long as the result.
static operation_context_t get_bg_context(const std::shared_ptr<environment_t> &env,
                                          uint32_t generation_count) {
    cancel_checker_t cancel_checker = [generation_count] {
        // Cancel if the generation count changed.
        return generation_count != read_generation_count();
    };
    return operation_context_t{nullptr, *env, std::move(cancel_checker)};
}

/// We try to ensure that syntax highlighting completes appropriately before executing what the user typed.
/// But we do not want it to block forever - e.g. it may hang on determining if an arbitrary argument
/// is a path. This is how long we'll wait (in milliseconds) before giving up and performing a
/// no-io syntax highlighting. See #7418, #5912.
static constexpr long kHighlightTimeoutForExecutionMs = 250;

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

void editable_line_t::insert_coalesce(const wcstring &str) {
    edit_t &edit = undo_history.edits.back();
    edit.replacement.append(str);
    apply_edit(&text_, edit_t(position(), 0, str));
    set_position(position() + str.size());
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
    std::vector<wcstring> matches_;

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
        if (matches_.empty()) return;
        match_index_ = matches_.size() - 1;
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
        history_search_flags_t flags = history_search_no_dedup;
        // Make the search case-insensitive unless we have an uppercase character.
        wcstring low = wcstolower(text);
        if (low == text) flags |= history_search_ignore_case;
        // We can skip dedup in history_search_t because we do it ourselves in skips_.
        search_ = history_search_t(
            *hist, text,
            by_prefix() ? history_search_type_t::prefix : history_search_type_t::contains,
            flags);
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

/// Data wrapping up the visual selection.
struct selection_data_t {
    /// The position of the cursor when selection was initiated.
    size_t begin{0};

    /// The start and stop position of the current selection.
    size_t start{0};
    size_t stop{0};

    bool operator==(const selection_data_t &rhs) const {
        return begin == rhs.begin && start == rhs.start && stop == rhs.stop;
    }

    bool operator!=(const selection_data_t &rhs) const { return !(*this == rhs); }
};

/// A value-type struct representing a layout from which we can call to s_write().
/// The intent is that everything we send to the screen is encapsulated in this struct.
struct layout_data_t {
    /// Text of the command line.
    wcstring text{};

    /// The colors. This has the same length as 'text'.
    std::vector<highlight_spec_t> colors{};

    /// Position of the cursor in the command line.
    size_t position{};

    /// Whether the cursor is focused on the pager or not.
    bool focused_on_pager{false};

    /// Visual selection of the command line, or none if none.
    maybe_t<selection_data_t> selection{};

    /// String containing the autosuggestion.
    wcstring autosuggestion{};

    /// String containing the history search. If non-empty, then highlight the found range within
    /// the text.
    wcstring history_search_text{};

    /// The result of evaluating the left, mode and right prompt commands.
    /// That is, this the text of the prompts, not the commands to produce them.
    wcstring left_prompt_buff{};
    wcstring mode_prompt_buff{};
    wcstring right_prompt_buff{};
};

/// A struct describing the state of the interactive reader. These states can be stacked, in case
/// reader_readline() calls are nested. This happens when the 'read' builtin is used.
class reader_data_t : public std::enable_shared_from_this<reader_data_t> {
   public:
    /// Configuration for the reader.
    const reader_config_t conf;
    /// The parser being used.
    std::shared_ptr<parser_t> parser_ref;
    /// String containing the whole current commandline.
    editable_line_t command_line;
    /// Whether the most recent modification to the command line was done by either history search
    /// or a pager selection change. When this is true and another transient change is made, the
    /// old transient change will be removed from the undo history.
    bool command_line_has_transient_edit = false;
    /// The most recent layout data sent to the screen.
    layout_data_t rendered_layout;
    /// String containing the autosuggestion.
    wcstring autosuggestion;
    /// Current pager.
    pager_t pager;
    /// The output of the pager.
    page_rendering_t current_page_rendering;
    /// When backspacing, we temporarily suppress autosuggestions.
    bool suppress_autosuggestion{false};

    /// The representation of the current screen contents.
    screen_t screen;

    /// The source of input events.
    inputter_t inputter;
    /// The history.
    history_t *history{nullptr};
    /// The history search.
    reader_history_search_t history_search{};

    /// The selection data. If this is not none, then we have an active selection.
    maybe_t<selection_data_t> selection{};

    wcstring left_prompt_buff;
    wcstring mode_prompt_buff;
    /// The output of the last evaluation of the right prompt command.
    wcstring right_prompt_buff;

    /// When navigating the pager, we modify the command line.
    /// This is the saved command line before modification.
    wcstring cycle_command_line;
    size_t cycle_cursor_pos{0};

    /// If set, a key binding or the 'exit' command has asked us to exit our read loop.
    bool exit_loop_requested{false};
    /// If this is true, exit reader even if there are running jobs. This happens if we press e.g.
    /// ^D twice.
    bool did_warn_for_bg_jobs{false};
    /// The current contents of the top item in the kill ring.
    wcstring kill_item;

    /// A flag which may be set to force re-execing all prompts and re-rendering.
    /// This may come about when a color like $fish_color... has changed.
    bool force_exec_prompt_and_repaint{false};

    /// The target character of the last jump command.
    wchar_t last_jump_target{0};
    jump_direction_t last_jump_direction{jump_direction_t::forward};
    jump_precision_t last_jump_precision{jump_precision_t::to};

    /// The text of the most recent asynchronous highlight and autosuggestion requests.
    /// If these differs from the text of the command line, then we must kick off a new request.
    wcstring in_flight_highlight_request;
    wcstring in_flight_autosuggest_request;

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

    /// \return the string used for history search, or an empty string if none.
    wcstring history_search_text_if_active() const;

    /// \return true if the command line has changed and repainting is needed. If \p colors is not
    /// null, then also return true if the colors have changed.
    using highlight_list_t = std::vector<highlight_spec_t>;
    bool is_repaint_needed(const highlight_list_t *mcolors = nullptr) const;

    /// Generate a new layout data from the current state of the world.
    /// If \p mcolors has a value, then apply it; otherwise extend existing colors.
    layout_data_t make_layout_data(maybe_t<highlight_list_t> mcolors = none()) const;

    /// Generate a new layout data from the current state of the world, and paint with it.
    /// If \p mcolors has a value, then apply it; otherwise extend existing colors.
    void layout_and_repaint(const wchar_t *reason, maybe_t<highlight_list_t> mcolors = none()) {
        this->rendered_layout = make_layout_data(std::move(mcolors));
        paint_layout(reason);
    }

    /// Paint the last rendered layout.
    /// \p reason is used in FLOG to explain why.
    void paint_layout(const wchar_t *reason);

    /// Return the variable set used for e.g. command duration.
    env_stack_t &vars() { return parser_ref->vars(); }
    const env_stack_t &vars() const { return parser_ref->vars(); }

    /// Access the parser.
    parser_t &parser() { return *parser_ref; }

    reader_data_t(std::shared_ptr<parser_t> parser, history_t *hist, reader_config_t &&conf)
        : conf(std::move(conf)),
          parser_ref(std::move(parser)),
          inputter(*parser_ref, conf.in),
          history(hist) {}

    void update_buff_pos(editable_line_t *el, maybe_t<size_t> new_pos = none_t());

    void kill(editable_line_t *el, size_t begin_idx, size_t length, int mode, int newv);
    /// Inserts a substring of str given by start, len at the cursor position.
    void insert_string(editable_line_t *el, const wcstring &str);
    /// Erase @length characters starting at @offset.
    void erase_substring(editable_line_t *el, size_t offset, size_t length);
    /// Replace the text of length @length at @offset by @replacement.
    void replace_substring(editable_line_t *el, size_t offset, size_t length,
                           wcstring &&replacement);
    void push_edit(editable_line_t *el, edit_t &&edit);

    /// Insert the character into the command line buffer and print it to the screen using syntax
    /// highlighting, etc.
    void insert_char(editable_line_t *el, wchar_t c) { insert_string(el, wcstring{c}); }

    /// Read a command to execute, respecting input bindings.
    /// \return the command, or none if we were asked to cancel (e.g. SIGHUP).
    maybe_t<wcstring> readline(int nchars);

    void move_word(editable_line_t *el, bool move_right, bool erase, enum move_word_style_t style,
                   bool newv);

    maybe_t<char_event_t> read_normal_chars(readline_loop_state_t &rls);
    void handle_readline_command(readline_cmd_t cmd, readline_loop_state_t &rls);

    void select_completion_in_direction(selection_motion_t dir);
    void flash();

    void completion_insert(const wcstring &val, size_t token_end, complete_flags_t flags);

    bool can_autosuggest() const;
    void autosuggest_completed(autosuggestion_result_t result);
    void update_autosuggestion();
    void accept_autosuggestion(bool full, bool single = false,
                               move_word_style_t style = move_word_style_punctuation);
    void super_highlight_me_plenty();

    /// Finish up any outstanding syntax highlighting, before execution.
    /// This plays some tricks to not block on I/O for too long.
    void finish_highlighting_before_exec();

    void highlight_complete(highlight_result_t result);
    void exec_mode_prompt();
    void exec_prompt();

    bool jump(jump_direction_t dir, jump_precision_t precision, editable_line_t *el,
              wchar_t target);

    bool handle_completions(const completion_list_t &comp, size_t token_begin, size_t token_end);

    void set_command_line_and_position(editable_line_t *el, wcstring &&new_str, size_t pos);
    void clear_transient_edit();
    void replace_current_token(wcstring &&new_token);
    void update_command_line_from_history_search();
    void set_buffer_maintaining_pager(const wcstring &b, size_t pos, bool transient = false);
    void delete_char(bool backward = true);

    /// Called to update the termsize, including $COLUMNS and $LINES, as necessary.
    void update_termsize() { (void)termsize_container_t::shared().updating(parser()); }

    // Import history from older location (config path) if our current history is empty.
    void import_history_if_necessary();
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

/// Restore terminal settings we care about, to prevent a broken shell.
static void term_fix_modes(struct termios *modes) {
    modes->c_iflag &= ~ICRNL;   // disable mapping CR (\cM) to NL (\cJ)
    modes->c_iflag &= ~INLCR;   // disable mapping NL (\cJ) to CR (\cM)
    modes->c_lflag &= ~ICANON;  // turn off canonical mode
    modes->c_lflag &= ~ECHO;    // turn off echo mode
    modes->c_lflag &= ~IEXTEN;  // turn off handling of discard and lnext characters
    modes->c_oflag |= OPOST;    // turn on "implementation-defined post processing" - this often
                                // changes how line breaks work.

    // Disable flow control in the shell. We don't want to be stopped.
    modes->c_iflag &= ~IXON;
    modes->c_iflag &= ~IXOFF;

    modes->c_cc[VMIN] = 1;
    modes->c_cc[VTIME] = 0;

    unsigned char disabling_char = '\0';
    // Prefer to use _POSIX_VDISABLE to disable control functions.
    // This permits separately binding nul (typically control-space).
    // POSIX calls out -1 as a special value which should be ignored.
#ifdef _POSIX_VDISABLE
    if (_POSIX_VDISABLE != -1) disabling_char = _POSIX_VDISABLE;
#endif

    // We ignore these anyway, so there is no need to sacrifice a character.
    modes->c_cc[VSUSP] = disabling_char;

    // (these two are already disabled because of IXON/IXOFF)
    modes->c_cc[VSTOP] = disabling_char;
    modes->c_cc[VSTART] = disabling_char;
}

static void term_fix_external_modes(struct termios *modes) {
    // Turning off OPOST breaks output (staircase effect), we don't allow it.
    // See #7133.
    modes->c_oflag |= OPOST;
    // These cause other ridiculous behaviors like input not being shown.
    modes->c_lflag |= ICANON;
    modes->c_lflag |= IEXTEN;
    modes->c_lflag |= ECHO;
    modes->c_iflag |= ICRNL;
    modes->c_iflag &= ~INLCR;
}
/// A description of where fish is in the process of exiting.
enum class exit_state_t {
    none,               /// fish is not exiting.
    running_handlers,   /// fish intends to exit, and is running handlers like 'fish_exit'.
    finished_handlers,  /// fish is finished running handlers and no more fish script may be run.
};
static relaxed_atomic_t<exit_state_t> s_exit_state{exit_state_t::none};

/// If set, SIGHUP has been received. This latches to true.
/// This is set from a signal handler.
static volatile sig_atomic_t s_sighup_received{false};

void reader_sighup() {
    // Beware, we may be in a signal handler.
    s_sighup_received = true;
}

static void redirect_tty_after_sighup() {
    // If we have received SIGHUP, redirect the tty to avoid a user script triggering SIGTTIN or
    // SIGTTOU.
    assert(s_sighup_received && "SIGHUP not received");
    static bool s_tty_redirected = false;
    if (!s_tty_redirected) {
        s_tty_redirected = true;
        redirect_tty_output();
    }
}

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
    // Copy the (potentially changed) terminal modes and use them from now on.
    struct termios modes;
    tcgetattr(STDIN_FILENO, &modes);
    std::memcpy(&tty_modes_for_external_cmds, &modes, sizeof tty_modes_for_external_cmds);
    term_fix_external_modes(&tty_modes_for_external_cmds);

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

    termsize_container_t::shared().invalidate_tty();
}

bool check_cancel_from_fish_signal() {
    switch (s_exit_state) {
        case exit_state_t::none:
            // Cancel if we got SIGHUP.
            return s_sighup_received;
        case exit_state_t::running_handlers:
            // We intend to exit but we want to allow these handlers to run.
            return false;
        case exit_state_t::finished_handlers:
            // Done running exit handlers, time to exit.
            return true;
    }
    DIE("Unreachable");
}

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
    if (el == &command_line && selection.has_value()) {
        if (selection->begin <= buff_pos) {
            selection->start = selection->begin;
            selection->stop = buff_pos + 1;
        } else {
            selection->start = buff_pos;
            selection->stop = selection->begin + 1;
        }
    }
}

bool reader_data_t::is_repaint_needed(const std::vector<highlight_spec_t> *mcolors) const {
    // Note: this function is responsible for detecting all of the ways that the command line may
    // change, by comparing it to what is present in rendered_layout.
    // The pager is the problem child, it has its own update logic.
    auto check = [](bool val, const wchar_t *reason) {
        if (val) FLOG(reader_render, L"repaint needed because", reason, L"change");
        return val;
    };

    bool focused_on_pager = active_edit_line() == &pager.search_field_line;
    const layout_data_t &last = this->rendered_layout;
    return check(force_exec_prompt_and_repaint, L"forced") ||
           check(command_line.text() != last.text, L"text") ||
           check(mcolors && *mcolors != last.colors, L"highlight") ||
           check(selection != last.selection, L"selection") ||
           check(focused_on_pager != last.focused_on_pager, L"focus") ||
           check(command_line.position() != last.position, L"position") ||
           check(history_search_text_if_active() != last.history_search_text, L"history search") ||
           check(autosuggestion != last.autosuggestion, L"autosuggestion") ||
           check(left_prompt_buff != last.left_prompt_buff, L"left_prompt") ||
           check(mode_prompt_buff != last.mode_prompt_buff, L"mode_prompt") ||
           check(right_prompt_buff != last.right_prompt_buff, L"right_prompt") ||
           check(pager.rendering_needs_update(current_page_rendering), L"pager");
}

layout_data_t reader_data_t::make_layout_data(maybe_t<highlight_list_t> mcolors) const {
    layout_data_t result{};
    bool focused_on_pager = active_edit_line() == &pager.search_field_line;
    result.text = command_line.text();

    if (mcolors.has_value()) {
        result.colors = mcolors.acquire();
    } else {
        result.colors = rendered_layout.colors;
    }

    result.position = focused_on_pager ? pager.cursor_position() : command_line.position();
    result.selection = selection;
    result.focused_on_pager = (active_edit_line() == &pager.search_field_line);
    result.history_search_text = history_search_text_if_active();
    result.autosuggestion = autosuggestion;
    result.left_prompt_buff = left_prompt_buff;
    result.mode_prompt_buff = mode_prompt_buff;
    result.right_prompt_buff = right_prompt_buff;

    // Ensure our color list has the same length as the command line, by extending it with the last
    // color. This typically reduces redraws; e.g. if the user continues types into an argument, we
    // guess it's still an argument, while the highlighting proceeds in the background.
    highlight_spec_t last_color = result.colors.empty() ? highlight_spec_t{} : result.colors.back();
    result.colors.resize(result.text.size(), last_color);
    return result;
}

void reader_data_t::paint_layout(const wchar_t *reason) {
    FLOGF(reader_render, L"Repainting from %ls", reason);
    const layout_data_t &data = this->rendered_layout;
    const editable_line_t *cmd_line = &command_line;

    wcstring full_line;
    if (conf.in_silent_mode) {
        full_line = wcstring(cmd_line->text().length(), get_obfuscation_read_char());
    } else {
        // Combine the command and autosuggestion into one string.
        full_line = combine_command_and_autosuggestion(cmd_line->text(), autosuggestion);
    }

    // Copy the colors and extend them with autosuggestion color.
    std::vector<highlight_spec_t> colors = data.colors;

    // Highlight any history search.
    if (!conf.in_silent_mode && !data.history_search_text.empty()) {
        const wcstring &needle = data.history_search_text;
        const wcstring &haystack = cmd_line->text();
        size_t match_pos = ifind(haystack,needle);
        if (match_pos != wcstring::npos) {
            for (size_t i = 0; i < needle.size(); i++) {
                colors.at(match_pos + i).background = highlight_role_t::search_match;
            }
        }
    }

    // Apply any selection.
    if (data.selection.has_value()) {
        highlight_spec_t selection_color = {highlight_role_t::normal, highlight_role_t::selection};
        for (size_t i = data.selection->start; i < std::min(selection->stop, colors.size()); i++) {
            colors.at(i) = selection_color;
        }
    }

    // Extend our colors with the autosuggestion.
    colors.resize(full_line.size(), highlight_role_t::autosuggestion);

    // Compute the indentation, then extend it with 0s for the autosuggestion. The autosuggestion
    // always conceptually has an indent of 0.
    std::vector<int> indents = parse_util_compute_indents(cmd_line->text());
    indents.resize(full_line.size(), 0);

    // Prepend the mode prompt to the left prompt.
    s_write(&screen, mode_prompt_buff + left_prompt_buff, right_prompt_buff, full_line,
            cmd_line->size(), colors, indents, data.position, pager, current_page_rendering,
            data.focused_on_pager);
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
    erase_substring(el, begin_idx, length);
}

// This is called from a signal handler!
void reader_handle_sigint() { interrupted = SIGINT; }

/// Make sure buffers are large enough to hold the current string length.
void reader_data_t::command_line_changed(const editable_line_t *el) {
    ASSERT_IS_MAIN_THREAD();
    if (el == &this->command_line) {
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
    using namespace ast;
    auto ast =
        ast_t::parse(subcmd, parse_flag_continue_after_error | parse_flag_accept_incomplete_tokens |
                                 parse_flag_leave_unterminated);

    // Look for plain statements where the cursor is at the end of the command.
    const ast::string_t *matching_cmd_node = nullptr;
    for (const node_t &n : ast) {
        const auto *stmt = n.try_as<decorated_statement_t>();
        if (!stmt) continue;

        // Skip if we have a decoration.
        if (stmt->opt_decoration) continue;

        // See if the command's source range range contains our cursor, including at the end.
        auto msource = stmt->command.try_source_range();
        if (!msource) continue;

        // Now see if its source range contains our cursor, including at the end.
        if (subcmd_cursor_pos >= msource->start &&
            subcmd_cursor_pos <= msource->start + msource->length) {
            // Success!
            matching_cmd_node = &stmt->command;
            break;
        }
    }

    // Now if we found a command node, expand it.
    maybe_t<edit_t> result{};
    if (matching_cmd_node) {
        assert(!matching_cmd_node->unsourced && "Should not be unsourced");
        const wcstring token = matching_cmd_node->source(subcmd);
        if (auto abbreviation = expand_abbreviation(token, vars)) {
            // There was an abbreviation! Replace the token in the full command. Maintain the
            // relative position of the cursor.
            source_range_t r = matching_cmd_node->source_range();
            result = edit_t(subcmd_offset + r.start, r.length, std::move(*abbreviation));
        }
    }
    return result;
}

/// Expand abbreviations at the current cursor position, minus the given cursor backtrack. This may
/// change the command line but does NOT repaint it. This is to allow the caller to coalesce
/// repaints.
bool reader_data_t::expand_abbreviation_as_necessary(size_t cursor_backtrack) {
    bool result = false;
    editable_line_t *el = active_edit_line();

    if (conf.expand_abbrev_ok && el == &command_line) {
        // Try expanding abbreviations.
        size_t cursor_pos = el->position() - std::min(el->position(), cursor_backtrack);

        if (auto edit = reader_expand_abbreviation_in_command(el->text(), cursor_pos, vars())) {
            push_edit(el, std::move(*edit));
            update_buff_pos(el);
            result = true;
        }
    }
    return result;
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

    // Update the termsize now.
    // This allows prompts to react to $COLUMNS.
    update_termsize();

    // If we have any prompts, they must be run non-interactively.
    if (!conf.left_prompt_cmd.empty() || !conf.right_prompt_cmd.empty()) {
        scoped_push<bool> noninteractive{&parser().libdata().is_interactive, false};

        exec_mode_prompt();

        if (!conf.left_prompt_cmd.empty()) {
            // Status is ignored.
            wcstring_list_t prompt_list;
            // Historic compatibility hack.
            // If the left prompt function is deleted, then use a default prompt instead of
            // producing an error.
            bool left_prompt_deleted = conf.left_prompt_cmd == LEFT_PROMPT_FUNCTION_NAME &&
                                       !function_exists(conf.left_prompt_cmd, parser());
            exec_subshell(left_prompt_deleted ? DEFAULT_PROMPT : conf.left_prompt_cmd, parser(),
                          prompt_list, false);
            left_prompt_buff = join_strings(prompt_list, L'\n');
        }

        if (!conf.right_prompt_cmd.empty()) {
            if (function_exists(conf.right_prompt_cmd, parser())) {
                // Status is ignored.
                wcstring_list_t prompt_list;
                exec_subshell(conf.right_prompt_cmd, parser(), prompt_list, false);
                // Right prompt does not support multiple lines, so just concatenate all of them.
                for (const auto &i : prompt_list) {
                    right_prompt_buff += i;
                }
            }
        }
    }

    // Write the screen title. Do not reset the cursor position: exec_prompt is called when there
    // may still be output on the line from the previous command (#2499) and we need our PROMPT_SP
    // hack to work.
    reader_write_title(L"", parser(), false);
}

void reader_init() {
    parser_t &parser = parser_t::principal_parser();
    auto &vars = parser.vars();

    // Ensure this var is present even before an interactive command is run so that if it is used
    // in a function like `fish_prompt` or `fish_right_prompt` it is defined at the time the first
    // prompt is written.
    vars.set_one(ENV_CMD_DURATION, ENV_UNEXPORT, L"0");

    // Save the initial terminal mode.
    tcgetattr(STDIN_FILENO, &terminal_mode_on_startup);

    // Set the mode used for program execution, initialized to the current mode.
    std::memcpy(&tty_modes_for_external_cmds, &terminal_mode_on_startup,
                sizeof tty_modes_for_external_cmds);
    // Disable flow control for external commands by default.
    tty_modes_for_external_cmds.c_iflag &= ~IXON;
    tty_modes_for_external_cmds.c_iflag &= ~IXOFF;
    term_fix_external_modes(&tty_modes_for_external_cmds);

    // Set the mode used for the terminal, initialized to the current mode.
    std::memcpy(&shell_modes, &terminal_mode_on_startup, sizeof shell_modes);

    term_fix_modes(&shell_modes);

    // We do this not because we actually need the window size but for its side-effect of correctly
    // setting the COLUMNS and LINES env vars.
    termsize_container_t::shared().updating(parser);
}

/// Restore the term mode if we own the terminal. It's important we do this before
/// restore_foreground_process_group, otherwise we won't think we own the terminal.
void restore_term_mode() {
    if (getpgrp() != tcgetpgrp(STDIN_FILENO)) return;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &terminal_mode_on_startup) == -1 && errno == EIO) {
        redirect_tty_output();
    }
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
        case rl::cancel_commandline:
        case rl::cancel: {
            // These commands always end paging.
            return true;
        }
        case rl::complete:
        case rl::complete_and_search:
        case rl::backward_char:
        case rl::forward_char:
        case rl::forward_single_char:
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
        case readline_cmd_t::beginning_of_history:
        case readline_cmd_t::end_of_history:
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
    erase_substring(el, pos, pos_end - pos);
    update_buff_pos(el);
    suppress_autosuggestion = true;
}

/// Insert the characters of the string into the command line buffer and print them to the screen
/// using syntax highlighting, etc.
/// Returns true if the string changed.
void reader_data_t::insert_string(editable_line_t *el, const wcstring &str) {
    if (str.empty()) return;

    command_line_has_transient_edit = false;
    if (!history_search.active() && want_to_coalesce_insertion_of(*el, str)) {
        el->insert_coalesce(str);
        assert(el->undo_history.may_coalesce);
    } else {
        el->push_edit(edit_t(el->position(), 0, str));
        el->undo_history.may_coalesce = (str.size() == 1);
    }

    if (el == &command_line) suppress_autosuggestion = false;
    // The pager needs to be refiltered.
    if (el == &this->pager.search_field_line) {
        command_line_changed(el);
    }
}

void reader_data_t::push_edit(editable_line_t *el, edit_t &&edit) {
    el->push_edit(std::move(edit));
    el->undo_history.may_coalesce = false;
    // The pager needs to be refiltered.
    if (el == &this->pager.search_field_line) {
        command_line_changed(el);
    }
}

void reader_data_t::erase_substring(editable_line_t *el, size_t offset, size_t length) {
    push_edit(el, edit_t(offset, length, L""));
}

void reader_data_t::replace_substring(editable_line_t *el, size_t offset, size_t length,
                                      wcstring &&replacement) {
    push_edit(el, edit_t(offset, length, replacement));
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
void reader_data_t::completion_insert(const wcstring &val, size_t token_end,
                                      complete_flags_t flags) {
    editable_line_t *el = active_edit_line();

    // Move the cursor to the end of the token.
    if (el->position() != token_end) update_buff_pos(el, token_end);

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
    const uint32_t generation_count = read_generation_count();
    auto vars = parser.vars().snapshot();
    const wcstring working_directory = vars->get_pwd_slash();
    // TODO: suspicious use of 'history' here
    // This is safe because histories are immortal, but perhaps
    // this should use shared_ptr
    return [=]() -> autosuggestion_result_t {
        ASSERT_IS_BACKGROUND_THREAD();
        autosuggestion_result_t nothing = {};
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
    return conf.autosuggest_ok && !suppress_autosuggestion && history_search.is_at_end() &&
           el == &command_line && el->text().find_first_not_of(whitespace) != wcstring::npos;
}

// Called after an autosuggestion has been computed on a background thread.
void reader_data_t::autosuggest_completed(autosuggestion_result_t result) {
    ASSERT_IS_MAIN_THREAD();
    if (result.search_string == in_flight_autosuggest_request)
        in_flight_autosuggest_request.clear();
    if (!result.suggestion.empty() && can_autosuggest() &&
        result.search_string == command_line.text() &&
        string_prefixes_string_case_insensitive(result.search_string, result.suggestion)) {
        // Autosuggestion is active and the search term has not changed, so we're good to go.
        autosuggestion = std::move(result.suggestion);
        if (this->is_repaint_needed()) {
            this->layout_and_repaint(L"autosuggest");
        }
    }
}

void reader_data_t::update_autosuggestion() {
    // If we can't autosuggest, just clear it.
    if (!can_autosuggest()) {
        in_flight_autosuggest_request.clear();
        autosuggestion.clear();
        return;
    }

    // Check to see if our autosuggestion still applies; if so, don't recompute it.
    // Since the autosuggestion computation is asynchronous, this avoids "flashing" as you type into
    // the autosuggestion.
    // This is also the main mechanism by which readline commands that don't change the command line
    // text avoid recomputing the autosuggestion.
    const editable_line_t &el = command_line;
    if (!autosuggestion.empty() &&
        string_prefixes_string_case_insensitive(el.text(), autosuggestion)) {
        return;
    }

    // Do nothing if we've already kicked off this autosuggest request.
    if (el.text() == in_flight_autosuggest_request) return;
    in_flight_autosuggest_request = el.text();

    // Clear the autosuggestion and kick it off in the background.
    FLOG(reader_render, L"Autosuggesting");
    autosuggestion.clear();
    auto performer = get_autosuggestion_performer(parser(), el.text(), el.position(), history);
    auto shared_this = this->shared_from_this();
    debounce_autosuggestions().perform(performer, [shared_this](autosuggestion_result_t result) {
        shared_this->autosuggest_completed(std::move(result));
    });
}

// Accept any autosuggestion by replacing the command line with it. If full is true, take the whole
// thing; if it's false, then respect the passed in style.
void reader_data_t::accept_autosuggestion(bool full, bool single, move_word_style_t style) {
    if (!autosuggestion.empty()) {
        // Accepting an autosuggestion clears the pager.
        pager.clear();

        // Accept the autosuggestion.
        if (full) {
            // Just take the whole thing.
            replace_substring(&command_line, 0, command_line.size(), std::move(autosuggestion));
        } else if (single) {
            replace_substring(&command_line, command_line.size(), 0,
                              autosuggestion.substr(command_line.size(), 1));
        } else {
            // Accept characters according to the specified style.
            move_word_state_machine_t state(style);
            size_t want;
            for (want = command_line.size(); want < autosuggestion.size(); want++) {
                wchar_t wc = autosuggestion.at(want);
                if (!state.consume_char(wc)) break;
            }
            size_t have = command_line.size();
            replace_substring(&command_line, command_line.size(), 0,
                              autosuggestion.substr(have, want - have));
        }
    }
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
    layout_data_t data = make_layout_data();

    // Save off the colors and set the background.
    highlight_list_t saved_colors = data.colors;
    for (size_t i = 0; i < el->position(); i++) {
        data.colors.at(i) = highlight_spec_t::make_background(highlight_role_t::search_match);
    }
    this->rendered_layout = data;  // need to copy the data since we will use it again.
    paint_layout(L"flash");

    layout_data_t old_data = std::move(rendered_layout);

    ignore_result(write(STDOUT_FILENO, "\a", 1));
    // The write above changed the timestamp of stdout; ensure we don't therefore reset our screen.
    // See #3693.
    s_save_status(&screen);

    pollint.tv_sec = 0;
    pollint.tv_nsec = 100 * 1000000;
    nanosleep(&pollint, nullptr);

    // Re-render with our saved data.
    data.colors = std::move(saved_colors);
    this->rendered_layout = std::move(data);
    paint_layout(L"unflash");
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
    } else if (size == 1) {
        // Exactly one suitable completion found - insert it.
        const completion_t &c = comp.at(0);

        // If this is a replacement completion, check that we know how to replace it, e.g. that
        // the token doesn't contain evil operators like {}.
        if (!(c.flags & COMPLETE_REPLACES_TOKEN) || reader_can_replace(tok, c.flags)) {
            completion_insert(c.completion, token_end, c.flags);
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
            completion_insert(common_prefix, token_end, flags);
            cycle_command_line = command_line.text();
            cycle_cursor_pos = command_line.position();
        }
    }

    if (use_prefix) {
        for (completion_t &c : surviving_completions) {
            c.flags &= ~COMPLETE_REPLACES_TOKEN;
            c.completion.erase(0, common_prefix.size());
        }
    }

    // Print the completion list.
    wcstring prefix;
    if (will_replace_token || match_type_requires_full_replacement(best_match_type)) {
        if (use_prefix) prefix = std::move(common_prefix);
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
    pid_t owner = tcgetpgrp(STDIN_FILENO);
    if (owner == shell_pgid) {
        return;
    }

    // In some strange cases the tty may be come preassigned to fish's pid, but not its pgroup.
    // In that case we simply attempt to claim our own pgroup.
    // See #7388.
    if (owner == getpid()) {
        (void)setpgid(owner, owner);
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
        owner = tcgetpgrp(STDIN_FILENO);
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
                    _(L"I appear to be an orphaned process, so I am quitting politely. My pid is %d.");
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
        if (tcsetattr(STDIN_FILENO, TCSANOW, &shell_modes) == -1) {
            if (errno == EIO) {
                redirect_tty_output();
            }
            FLOGF(warning, _(L"Failed to set startup terminal mode!"));
            wperror(L"tcsetattr");
        }
    }

    termsize_container_t::shared().invalidate_tty();

    // For compatibility with fish 2.0's $_, now replaced with `status current-command`
    parser.vars().set_one(L"_", ENV_GLOBAL, L"fish");
}

/// Destroy data for interactive use.
static void reader_interactive_destroy() {
    outputter_t::stdoutput().set_color(rgb_color_t::reset(), rgb_color_t::reset());
}

/// Set the specified string as the current buffer.
void reader_data_t::set_command_line_and_position(editable_line_t *el, wcstring &&new_str,
                                                  size_t pos) {
    push_edit(el, edit_t(0, el->size(), std::move(new_str)));
    el->set_position(pos);
    update_buff_pos(el, pos);
}

/// Undo the transient edit und update commandline accordingly.
void reader_data_t::clear_transient_edit() {
    if (!command_line_has_transient_edit) {
        return;
    }
    command_line.undo();
    update_buff_pos(&command_line);
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
    replace_substring(el, offset, length, std::move(new_token));
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
    } else {
        assert(history_search.by_line() || history_search.by_prefix());
        replace_substring(&command_line, 0, command_line.size(), std::move(new_text));
    }
    command_line_has_transient_edit = true;
    assert(el == &command_line);
    update_buff_pos(el);
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
    replace_substring(&command_line, 0, command_line.size(), wcstring(b));
    command_line_changed(&command_line);

    // Don't set a position past the command line length.
    if (pos > command_line_len) pos = command_line_len;  //!OCLINT(parameter reassignment)
    update_buff_pos(&command_line, pos);

    // Clear history search and pager contents.
    history_search.reset();
}

static void set_env_cmd_duration(struct timeval *after, struct timeval *before, env_stack_t &vars) {
    time_t secs = after->tv_sec - before->tv_sec;
    suseconds_t usecs = after->tv_usec - before->tv_usec;

    if (after->tv_usec < before->tv_usec) {
        usecs += 1000000;
        secs -= 1;
    }

    vars.set_one(ENV_CMD_DURATION, ENV_UNEXPORT, std::to_wstring((secs * 1000) + (usecs / 1000)));
}

/// Run the specified command with the correct terminal modes, and while taking care to perform job
/// notification, set the title, etc.
static eval_res_t reader_run_command(parser_t &parser, const wcstring &cmd) {
    struct timeval time_before, time_after;

    wcstring ft = tok_command(cmd);

    // For compatibility with fish 2.0's $_, now replaced with `status current-command`
    if (!ft.empty()) parser.vars().set_one(L"_", ENV_GLOBAL, ft);

    outputter_t &outp = outputter_t::stdoutput();
    reader_write_title(cmd, parser);
    term_donate(outp);

    gettimeofday(&time_before, nullptr);

    auto eval_res = parser.eval(cmd, io_chain_t{});
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

    return eval_res;
}

static parser_test_error_bits_t reader_shell_test(const parser_t &parser, const wcstring &b) {
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
        reader_schedule_prompt_repaint();
    }
    return res;
}

wcstring reader_data_t::history_search_text_if_active() const {
    if (!history_search.active() || history_search.is_at_end()) {
        return wcstring{};
    }
    return history_search.search_string();
}

void reader_data_t::highlight_complete(highlight_result_t result) {
    ASSERT_IS_MAIN_THREAD();
    in_flight_highlight_request.clear();
    if (result.text == command_line.text()) {
        assert(result.colors.size() == command_line.size());
        if (this->is_repaint_needed(&result.colors)) {
            this->layout_and_repaint(L"highlight", std::move(result.colors));
        }
    }
}

// Given text and  whether IO is allowed, return a function that performs highlighting. The function
// may be invoked on a background thread.
static std::function<highlight_result_t(void)> get_highlight_performer(parser_t &parser,
                                                                       const wcstring &text,
                                                                       bool io_ok) {
    auto vars = parser.vars().snapshot();
    uint32_t generation_count = read_generation_count();
    return [=]() -> highlight_result_t {
        if (text.empty()) return {};
        operation_context_t ctx = get_bg_context(vars, generation_count);
        std::vector<highlight_spec_t> colors(text.size(), highlight_spec_t{});
        highlight_shell(text, colors, ctx, io_ok);
        return highlight_result_t{std::move(colors), text};
    };
}

/// Highlight the command line in a super, plentiful way.
void reader_data_t::super_highlight_me_plenty() {
    if (!conf.highlight_ok) return;

    // Do nothing if this text is already in flight.
    const editable_line_t *el = &command_line;
    if (el->text() == in_flight_highlight_request) return;
    in_flight_highlight_request = el->text();

    FLOG(reader_render, L"Highlighting");
    auto highlight_performer = get_highlight_performer(parser(), el->text(), true /* io_ok */);
    auto shared_this = this->shared_from_this();
    debounce_highlighting().perform(highlight_performer, [shared_this](highlight_result_t result) {
        shared_this->highlight_complete(std::move(result));
    });
}

void reader_data_t::finish_highlighting_before_exec() {
    if (!conf.highlight_ok) return;
    if (in_flight_highlight_request.empty()) return;

    // We have an in-flight highlight request scheduled.
    // Wait for its completion to run, but not forever.
    namespace sc = std::chrono;
    auto now = sc::steady_clock::now();
    auto deadline = now + sc::milliseconds(kHighlightTimeoutForExecutionMs);
    while (now < deadline) {
        long timeout_usec = sc::duration_cast<sc::microseconds>(deadline - now).count();
        iothread_service_completion_with_timeout(timeout_usec);

        // Note iothread_service_completion_with_timeout will reentrantly modify us,
        // by invoking a completion.
        if (in_flight_highlight_request.empty()) break;
        now = sc::steady_clock::now();
    }

    if (!in_flight_highlight_request.empty()) {
        // We did not complete before the deadline.
        // Give up and highlight without I/O.
        const editable_line_t *el = &command_line;
        auto highlight_no_io = get_highlight_performer(parser(), el->text(), false /* io not ok */);
        this->highlight_complete(highlight_no_io());
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

/// Add a new reader to the reader stack.
/// \return a shared pointer to it.
static std::shared_ptr<reader_data_t> reader_push_ret(parser_t &parser,
                                                      const wcstring &history_name,
                                                      reader_config_t &&conf) {
    history_t *hist = &history_t::history_with_name(history_name);
    auto data = std::make_shared<reader_data_t>(parser.shared(), hist, std::move(conf));
    reader_data_stack.push_back(data);
    data->command_line_changed(&data->command_line);
    if (reader_data_stack.size() == 1) {
        reader_interactive_init(parser);
    }
    data->exec_prompt();
    return data;
}

/// Public variant which discards the return value.
void reader_push(parser_t &parser, const wcstring &history_name, reader_config_t &&conf) {
    (void)reader_push_ret(parser, history_name, std::move(conf));
}

void reader_pop() {
    assert(!reader_data_stack.empty() && "empty stack in reader_data_stack");
    reader_data_stack.pop_back();
    reader_data_t *new_reader = current_data_or_null();
    if (new_reader == nullptr) {
        reader_interactive_destroy();
    } else {
        s_reset_abandoning_line(&new_reader->screen, termsize_last().width);
    }
}

void reader_data_t::import_history_if_necessary() {
    // Import history from older location (config path) if our current history is empty.
    if (history && history->is_empty()) {
        history->populate_from_config_path();
    }

    // Import history from bash, etc. if our current history is still empty and is the default
    // history.
    if (history && history->is_empty() && history->is_default()) {
        // Try opening a bash file. We make an effort to respect $HISTFILE; this isn't very complete
        // (AFAIK it doesn't have to be exported), and to really get this right we ought to ask bash
        // itself. But this is better than nothing.
        const auto var = vars().get(L"HISTFILE");
        wcstring path = (var ? var->as_string() : L"~/.bash_history");
        expand_tilde(path, vars());
        int fd = wopen_cloexec(path, O_RDONLY);
        if (fd >= 0) {
            FILE *f = fdopen(fd, "r");
            history->populate_from_bash(f);
            fclose(f);
        }
    }
}

/// Check if we have background jobs that we have not warned about.
/// If so, print a warning and return true. Otherwise return false.
static bool try_warn_on_background_jobs(reader_data_t *data) {
    ASSERT_IS_MAIN_THREAD();
    // Have we already warned?
    if (data->did_warn_for_bg_jobs) return false;
    // Are we the top-level reader?
    if (reader_data_stack.size() > 1) return false;
    // Do we have background jobs?
    auto bg_jobs = jobs_requiring_warning_on_exit(data->parser());
    if (bg_jobs.empty()) return false;
    // Print the warning!
    print_exit_warning_for_jobs(bg_jobs);
    data->did_warn_for_bg_jobs = true;
    return true;
}

/// Check if we should exit the reader loop.
/// \return true if we should exit.
static bool check_exit_loop_maybe_warning(reader_data_t *data) {
    // sighup always forces exit.
    if (s_sighup_received) return true;

    // Check if an exit is requested.
    if (data->exit_loop_requested) {
        if (try_warn_on_background_jobs(data)) {
            data->exit_loop_requested = false;
            return false;
        }
        return true;
    }
    return false;
}

static bool selection_is_at_top(const reader_data_t *data) {
    const pager_t *pager = &data->pager;
    size_t row = pager->get_selected_row(data->current_page_rendering);
    if (row != 0 && row != PAGER_SELECTION_NONE) return false;

    size_t col = pager->get_selected_column(data->current_page_rendering);
    return !(col != 0 && col != PAGER_SELECTION_NONE);
}

static relaxed_atomic_t<uint64_t> run_count{0};

/// Returns the current interactive loop count
uint64_t reader_run_count() { return run_count; }

static relaxed_atomic_t<uint64_t> status_count{0};

/// Returns the current "generation" of interactive status.
/// This is not incremented if the command being run produces no status,
/// (e.g. background job, or variable assignment).
uint64_t reader_status_count() { return status_count; }

/// Read interactively. Read input from stdin while providing editing facilities.
static int read_i(parser_t &parser) {
    ASSERT_IS_MAIN_THREAD();
    reader_config_t conf;
    conf.complete_ok = true;
    conf.highlight_ok = true;
    conf.syntax_check_ok = true;
    conf.autosuggest_ok = true;
    conf.expand_abbrev_ok = true;

    if (parser.libdata().is_breakpoint && function_exists(DEBUG_PROMPT_FUNCTION_NAME, parser)) {
        conf.left_prompt_cmd = DEBUG_PROMPT_FUNCTION_NAME;
        conf.right_prompt_cmd = wcstring{};
    } else {
        conf.left_prompt_cmd = LEFT_PROMPT_FUNCTION_NAME;
        conf.right_prompt_cmd = RIGHT_PROMPT_FUNCTION_NAME;
    }

    std::shared_ptr<reader_data_t> data =
        reader_push_ret(parser, history_session_id(parser.vars()), std::move(conf));
    data->import_history_if_necessary();

    while (!check_exit_loop_maybe_warning(data.get())) {
        ++run_count;

        maybe_t<wcstring> tmp = data->readline(0);
        if (tmp && !tmp->empty()) {
            const wcstring command = tmp.acquire();
            data->update_buff_pos(&data->command_line, 0);
            data->command_line.clear();
            data->command_line_changed(&data->command_line);
            wcstring_list_t argv{command};
            event_fire_generic(parser, L"fish_preexec", &argv);
            auto eval_res = reader_run_command(parser, command);
            signal_clear_cancel();
            if (!eval_res.no_status) {
                ++status_count;
            }

            // If the command requested an exit, then process it now and clear it.
            data->exit_loop_requested |= parser.libdata().exit_current_script;
            parser.libdata().exit_current_script = false;

            event_fire_generic(parser, L"fish_postexec", &argv);
            // Allow any pending history items to be returned in the history array.
            if (data->history) {
                data->history->resolve_pending();
            }

            bool already_warned = data->did_warn_for_bg_jobs;
            if (check_exit_loop_maybe_warning(data.get())) {
                break;
            }
            if (already_warned) {
                // We had previously warned the user and they ran another command.
                // Reset the warning.
                data->did_warn_for_bg_jobs = false;
            }
        }
    }
    reader_pop();

    // If we got SIGHUP, ensure the tty is redirected.
    if (s_sighup_received) {
        // If we are the top-level reader, then we translate SIGHUP into exit_forced.
        redirect_tty_after_sighup();
    }

    // If we are the last reader, then kill remaining jobs before exiting.
    if (reader_data_stack.size() == 0) {
        // Send the exit event and then commit to not executing any more fish script.
        s_exit_state = exit_state_t::running_handlers;
        event_fire_generic(parser, L"fish_exit");
        s_exit_state = exit_state_t::finished_handlers;
        hup_jobs(parser.jobs());
    }

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
        if (!event_is_normal_char(evt) || !can_read(conf.in)) {
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
            pager.clear();
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
            break;
        }
        case rl::beginning_of_buffer: {
            update_buff_pos(&command_line, 0);
            break;
        }
        case rl::end_of_buffer: {
            update_buff_pos(&command_line, command_line.size());
            break;
        }
        case rl::cancel_commandline: {
            if (command_line.size()) {
                outputter_t &outp = outputter_t::stdoutput();
                // Move cursor to the end of the line.
                update_buff_pos(&command_line, command_line.size());
                autosuggestion.clear();
                // Repaint also changes the actual cursor position
                if (this->is_repaint_needed()) this->layout_and_repaint(L"cancel");

                auto fish_color_cancel = vars.get(L"fish_color_cancel");
                if (fish_color_cancel) {
                    outp.set_color(parse_color(*fish_color_cancel, false),
                                   parse_color(*fish_color_cancel, true));
                }
                outp.writestr(L"^C");
                outp.set_color(rgb_color_t::reset(), rgb_color_t::reset());

                // We print a newline last so the prompt_sp hack doesn't get us.
                outp.push_back('\n');

                set_command_line_and_position(&command_line, L"", 0);
                s_reset_abandoning_line(&screen, termsize_last().width - command_line.size());

                // Post fish_cancel, allowing it to fire.
                signal_clear_cancel();
                event_fire_generic(parser(), L"fish_cancel");
            }
            break;
        }
        case rl::cancel: {
            // If we last inserted a completion, undo it.
            // This doesn't apply if the completion was selected via the pager
            // (in which case the last command is "execute" or similar,
            // but never complete{,_and_search})
            //
            // Also paging is already cancelled above.
            if (rls.complete_did_insert &&
                (rls.last_cmd == rl::complete
                 || rls.last_cmd == rl::complete_and_search)) {
                editable_line_t *el = active_edit_line();
                el->undo();
                update_buff_pos(el);
            }
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

            // Don't go into an infinite loop of repainting.
            // This can happen e.g. if a variable triggers a repaint,
            // and the variable is set inside the prompt (#7324).
            // builtin commandline will refuse to enqueue these.
            parser().libdata().is_repaint = true;
            exec_mode_prompt();
            if (!mode_prompt_buff.empty()) {
                s_reset_line(&screen, true /* redraw prompt */);
                if (this->is_repaint_needed()) this->layout_and_repaint(L"mode");
                parser().libdata().is_repaint = false;
                break;
            }
            // Else we repaint as normal.
            /* fallthrough */
        }
        case rl::force_repaint:
        case rl::repaint: {
            parser().libdata().is_repaint = true;
            exec_prompt();
            s_reset_line(&screen, true /* redraw prompt */);
            this->layout_and_repaint(L"readline");
            force_exec_prompt_and_repaint = false;
            parser().libdata().is_repaint = false;
            break;
        }
        case rl::complete:
        case rl::complete_and_search: {
            if (!conf.complete_ok) break;

            // Use the command line only; it doesn't make sense to complete in any other line.
            editable_line_t *el = &command_line;
            if (is_navigating_pager_contents() ||
                (!rls.comp.empty() && !rls.complete_did_insert && rls.last_cmd == rl::complete)) {
                // The user typed complete more than once in a row. If we are not yet fully
                // disclosed, then become so; otherwise cycle through our available completions.
                if (current_page_rendering.remaining_to_disclose > 0) {
                    pager.set_fully_disclosed(true);
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
                replace_substring(el, el->position() - rls.yank_len, rls.yank_len,
                                  std::move(yank_str));
                update_buff_pos(el);
                rls.yank_len = new_yank_len;
                suppress_autosuggestion = true;
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
                // This is by definition a successful exit, override the status
                parser().set_last_statuses(statuses_t::just(STATUS_CMD_OK));
                exit_loop_requested = true;
                check_exit_loop_maybe_warning(this);
            }
            break;
        }
            // Evaluate. If the current command is unfinished, or if the charater is escaped
            // using a backslash, insert a newline.
        case rl::execute: {
            // If the user hits return while navigating the pager, it only clears the pager.
            if (is_navigating_pager_contents()) {
                pager.clear();
                break;
            }

            // Delete any autosuggestion.
            autosuggestion.clear();

            // The user may have hit return with pager contents, but while not navigating them.
            // Clear the pager in that event.
            pager.clear();

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
            parser_test_error_bits_t command_test_result = 0;
            if (conf.syntax_check_ok) {
                command_test_result = reader_shell_test(parser(), el->text());
            }
            if (command_test_result == 0 || command_test_result == PARSER_TEST_INCOMPLETE) {
                // This command is valid, but an abbreviation may make it invalid. If so, we
                // will have to test again.
                if (expand_abbreviation_as_necessary(1)) {
                    // Trigger syntax highlighting as we are likely about to execute this command.
                    this->super_highlight_me_plenty();
                    if (conf.syntax_check_ok) {
                        command_test_result = reader_shell_test(parser(), el->text());
                    }
                }
            }

            if (command_test_result == 0) {
                // Finished command, execute it. Don't add items that start with a leading
                // space, or if in silent mode (#7230).
                const editable_line_t *el = &command_line;
                if (history != nullptr && !conf.in_silent_mode && may_add_to_history(el->text())) {
                    history->add_pending_with_file_detection(el->text(), vars.get_pwd_slash());
                }
                rls.finished = true;
                update_buff_pos(&command_line, command_line.size());
            } else if (command_test_result == PARSER_TEST_INCOMPLETE) {
                // We are incomplete, continue editing.
                insert_char(el, L'\n');
            } else {
                // Result must be some combination including an error. The error message will
                // already be printed, all we need to do is repaint.
                wcstring_list_t argv(1, el->text());
                event_fire_generic(parser(), L"fish_posterror", &argv);
                s_reset_abandoning_line(&screen, termsize_last().width);
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

            bool was_active_before = history_search.active();

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
                bool found = history_search.move_in_direction(dir);

                // Signal that we've found nothing
                if (!found) flash();

                if (!found && !was_active_before) {
                    history_search.reset();
                    break;
                }
                if (found ||
                    (dir == history_search_direction_t::forward && history_search.is_at_end())) {
                    update_command_line_from_history_search();
                }
            }
            break;
        }
        case rl::backward_char: {
            editable_line_t *el = active_edit_line();
            if (is_navigating_pager_contents()) {
                select_completion_in_direction(selection_motion_t::west);
            } else if (el->position() > 0) {
                update_buff_pos(el, el->position() - 1);
            }
            break;
        }
        case rl::forward_char: {
            editable_line_t *el = active_edit_line();
            if (is_navigating_pager_contents()) {
                select_completion_in_direction(selection_motion_t::east);
            } else if (el->position() < el->size()) {
                update_buff_pos(el, el->position() + 1);
            } else {
                accept_autosuggestion(true);
            }
            break;
        }
        case rl::forward_single_char: {
            editable_line_t *el = active_edit_line();
            if (is_navigating_pager_contents()) {
                select_completion_in_direction(selection_motion_t::east);
            } else if (el->position() < el->size()) {
                update_buff_pos(el, el->position() + 1);
            } else {
                accept_autosuggestion(false, true);
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
                accept_autosuggestion(false, false, move_style);
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
                if (history_search.active()) {
                    update_command_line_from_history_search();
                }
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
                } else if (selection_is_at_top(this)) {
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
                    auto indents = parse_util_compute_indents(el->text());
                    size_t base_pos_new = parse_util_get_offset_from_line(el->text(), line_new);
                    size_t base_pos_old = parse_util_get_offset_from_line(el->text(), line_old);

                    assert(base_pos_new != static_cast<size_t>(-1) &&
                           base_pos_old != static_cast<size_t>(-1));
                    int indent_old = indents.at(std::min(indents.size() - 1, base_pos_old));
                    int indent_new = indents.at(std::min(indents.size() - 1, base_pos_new));

                    size_t line_offset_old = el->position() - base_pos_old;
                    size_t total_offset_new = parse_util_get_offset(
                        el->text(), line_new, line_offset_old - 4 * (indent_new - indent_old));
                    update_buff_pos(el, total_offset_new);
                }
            }
            break;
        }
        case rl::suppress_autosuggestion: {
            suppress_autosuggestion = true;
            bool success = !autosuggestion.empty();
            autosuggestion.clear();
            // Return true if we had a suggestion to clear.
            inputter.function_set_status(success);
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
                replace_substring(el, buff_pos, (size_t)1, std::move(replacement));

                // Restore the buffer position since replace_substring moves
                // the buffer position ahead of the replaced text.
                update_buff_pos(el, buff_pos);
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

                replace_substring(el, start, len, std::move(replacement));

                // Restore the buffer position since replace_substring moves
                // the buffer position ahead of the replaced text.
                update_buff_pos(el, buff_pos);
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
            replace_substring(el, init_pos, pos - init_pos, std::move(replacement));
            update_buff_pos(el);
            break;
        }

        case rl::begin_selection: {
            if (!selection) selection = selection_data_t{};
            size_t pos = command_line.position();
            selection->begin = pos;
            selection->start = pos;
            selection->stop = pos + 1;
            break;
        }

        case rl::end_selection: {
            selection.reset();
            break;
        }

        case rl::swap_selection_start_stop: {
            if (!selection) break;
            size_t tmp = selection->begin;
            selection->begin = command_line.position();
            selection->start = command_line.position();
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
            break;
        }
        case rl::repeat_jump: {
            editable_line_t *el = active_edit_line();
            bool success = false;

            if (last_jump_target) {
                success = jump(last_jump_direction, last_jump_precision, el, last_jump_target);
            }

            inputter.function_set_status(success);
            break;
        }
        case rl::reverse_repeat_jump: {
            editable_line_t *el = active_edit_line();
            bool success = false;
            jump_direction_t original_dir, dir;
            original_dir = last_jump_direction;

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
            break;
        }

        case rl::expand_abbr: {
            if (expand_abbreviation_as_necessary(1)) {
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
                    pager.clear();
                }
                update_buff_pos(el);
            } else {
                flash();
            }
            break;
        }
            // Some commands should have been handled internally by inputter_t::readch().
        case rl::self_insert:
        case rl::self_insert_notfirst:
        case rl::func_or:
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

    s_reset_abandoning_line(&screen, termsize_last().width);
    event_fire_generic(parser(), L"fish_prompt");
    exec_prompt();

    /// A helper that kicks off syntax highlighting, autosuggestion computing, and repaints.
    auto color_suggest_repaint_now = [this] {
        if (conf.in == STDIN_FILENO) {
            this->update_autosuggestion();
            this->super_highlight_me_plenty();
        }
        if (this->is_repaint_needed()) this->layout_and_repaint(L"toplevel");
        this->force_exec_prompt_and_repaint = false;
    };

    // Start out as initially dirty.
    force_exec_prompt_and_repaint = true;

    // Get the current terminal modes. These will be restored when the function returns.
    if (tcgetattr(conf.in, &old_modes) == -1 && errno == EIO) redirect_tty_output();

    // Set the new modes.
    if (tcsetattr(conf.in, TCSANOW, &shell_modes) == -1) {
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

    while (!rls.finished && !check_exit_loop_maybe_warning(this)) {
        // Perhaps update the termsize. This is cheap if it has not changed.
        update_termsize();

        // Repaint as needed.
        color_suggest_repaint_now();

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
            continue;
        } else if (event_needing_handling->is_eof()) {
            reader_sighup();
            continue;
        }
        assert((event_needing_handling->is_char() || event_needing_handling->is_readline()) &&
               "Should have a char or readline");

        if (rls.last_cmd != rl::yank && rls.last_cmd != rl::yank_pop) {
            rls.yank_len = 0;
        }

        if (event_needing_handling->is_readline()) {
            readline_cmd_t readline_cmd = event_needing_handling->get_readline();
            if (readline_cmd == rl::cancel && is_navigating_pager_contents()) {
                clear_transient_edit();
            }

            // Clear the pager if necessary.
            bool focused_on_search_field = (active_edit_line() == &pager.search_field_line);
            if (command_ends_paging(readline_cmd, focused_on_search_field)) {
                pager.clear();
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
            signal_clear_cancel();

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
                    pager.clear();
                    command_line_has_transient_edit = false;
                }
            } else {
                // This can happen if the user presses a control char we don't recognize. No
                // reason to report this to the user unless they've enabled debugging output.
                FLOGF(reader, _(L"Unknown key binding 0x%X"), c);
            }
            rls.last_cmd = none();
        }
    }

    // Redraw the command line. This is what ensures the autosuggestion is hidden, etc. after the
    // user presses enter.
    if (this->is_repaint_needed() || conf.in != STDIN_FILENO) this->layout_and_repaint(L"prepare to execute");

    // Finish any outstanding syntax highlighting (but do not wait forever).
    finish_highlighting_before_exec();

    // Emit a newline so that the output is on the line after the command.
    // But do not emit a newline if the cursor has wrapped onto a new line all its own - see #6826.
    if (!screen.cursor_is_wrapped_to_own_line()) {
        ignore_result(write(STDOUT_FILENO, "\n", 1));
    }

    // HACK: If stdin isn't the same terminal as stdout, we just moved the cursor.
    // For now, just reset it to the beginning of the line.
    if (conf.in != STDIN_FILENO) {
        ignore_result(write(STDOUT_FILENO, "\r", 1));
    }

    // Ensure we have no pager contents when we exit.
    if (!pager.empty()) {
        // Clear to end of screen to erase the pager contents.
        // TODO: this may fail if eos doesn't exist, in which case we should emit newlines.
        screen_force_clear_to_end();
        pager.clear();
    }

    if (s_exit_state != exit_state_t::finished_handlers) {
        // The order of the two conditions below is important. Try to restore the mode
        // in all cases, but only complain if interactive.
        if (tcsetattr(conf.in, TCSANOW, &old_modes) == -1 &&
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
    if (res && data && data->conf.exit_on_interrupt) {
        data->exit_loop_requested = true;
        // We handled the interrupt ourselves, our caller doesn't need to handle it.
        return 0;
    }
    return res;
}

void reader_schedule_prompt_repaint() {
    ASSERT_IS_MAIN_THREAD();
    reader_data_t *data = current_data_or_null();
    if (data && !data->force_exec_prompt_and_repaint) {
        data->force_exec_prompt_and_repaint = true;
        data->inputter.queue_ch(readline_cmd_t::repaint);
    }
}

void reader_queue_ch(const char_event_t &ch) {
    if (reader_data_t *data = current_data_or_null()) {
        data->inputter.queue_ch(ch);
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

/// Sets the command line contents, clearing the pager.
void reader_set_buffer(const wcstring &b, size_t pos) {
    reader_data_t *data = current_data_or_null();
    if (!data) return;

    data->pager.clear();
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
    if (data != nullptr && data->selection.has_value()) {
        *start = data->selection->start;
        *len = std::min(data->selection->stop, data->command_line.size()) - data->selection->start;
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
        ssize_t amt = read(fd, buff, sizeof buff);
        if (amt > 0) {
            fd_contents.append(buff, amt);
        } else if (amt == 0) {
            // EOF.
            break;
        } else {
            assert(amt == -1);
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

    // Parse into an ast and detect errors.
    parse_error_list_t errors;
    auto ast = ast::ast_t::parse(str, parse_flag_none, &errors);
    bool errored = ast.errored();
    if (!errored) {
        errored = parse_util_detect_errors(ast, str, &errors);
    }
    if (!errored) {
        // Construct a parsed source ref.
        // Be careful to transfer ownership, this could be a very large string.
        parsed_source_ref_t ps = std::make_shared<parsed_source_t>(std::move(str), std::move(ast));
        parser.eval(ps, io);
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
    parser.libdata().exit_current_script = false;

    return res;
}
