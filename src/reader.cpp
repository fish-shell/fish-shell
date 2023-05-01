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

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include "history.rs.h"
#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <wctype.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <set>
#include <type_traits>

#include "abbrs.h"
#include "ast.h"
#include "color.h"
#include "common.h"
#include "complete.h"
#include "env.h"
#include "event.h"
#include "exec.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fd_readable_set.rs.h"
#include "fds.h"
#include "flog.h"
#include "function.h"
#include "global_safety.h"
#include "highlight.h"
#include "history.h"
#include "input.h"
#include "input_common.h"
#include "io.h"
#include "iothread.h"
#include "kill.h"
#include "operation_context.h"
#include "output.h"
#include "pager.h"
#include "parse_constants.h"
#include "parse_tree.h"
#include "parse_util.h"
#include "parser.h"
#include "proc.h"
#include "reader.h"
#include "screen.h"
#include "signals.h"
#include "termsize.h"
#include "tokenizer.h"
#include "wcstringutil.h"
#include "wildcard.h"
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

/// When tab-completing with a wildcard, we expand the wildcard up to this many results.
/// If expansion would exceed this many results, beep and do nothing.
static const size_t TAB_COMPLETE_WILDCARD_MAX_EXPANSION = 256;

/// A mode for calling the reader_kill function. In this mode, the new string is appended to the
/// current contents of the kill buffer.
#define KILL_APPEND 0

/// A mode for calling the reader_kill function. In this mode, the new string is prepended to the
/// current contents of the kill buffer.
#define KILL_PREPEND 1

enum class jump_direction_t { forward, backward };
enum class jump_precision_t { till, to };

/// A singleton snapshot of the reader state. This is updated when the reader changes. This is
/// factored out for thread-safety reasons: it may be fetched on a background thread.
static acquired_lock<commandline_state_t> commandline_state_snapshot() {
    // Deliberately leaked to avoid shutdown dtors.
    static owning_lock<commandline_state_t> *const s_state = new owning_lock<commandline_state_t>();
    return s_state->acquire();
}

commandline_state_t commandline_get_state() {
    auto s = commandline_state_snapshot();

    commandline_state_t state{};
    state.text = s->text;
    state.cursor_pos = s->cursor_pos;
    state.selection = s->selection;
    state.history = (*s->history)->clone();
    state.pager_mode = s->pager_mode;
    state.pager_fully_disclosed = s->pager_fully_disclosed;
    state.search_mode = s->search_mode;
    state.initialized = s->initialized;

    return state;
}

void commandline_set_buffer(wcstring text, size_t cursor_pos) {
    auto state = commandline_state_snapshot();
    state->cursor_pos = std::min(cursor_pos, text.size());
    state->text = std::move(text);
}

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
    return operation_context_t{nullptr, *env, std::move(cancel_checker), kExpansionLimitBackground};
}

/// We try to ensure that syntax highlighting completes appropriately before executing what the user
/// typed. But we do not want it to block forever - e.g. it may hang on determining if an arbitrary
/// argument is a path. This is how long we'll wait (in milliseconds) before giving up and
/// performing a no-io syntax highlighting. See #7418, #5912.
static constexpr long kHighlightTimeoutForExecutionMs = 250;

/// Get the debouncer for autosuggestions and background highlighting.
/// These are deliberately leaked to avoid shutdown dtor registration.
static debounce_t &debounce_autosuggestions() {
    const long kAutosuggestTimeoutMs = 500;
    static auto res = new_debounce_t(kAutosuggestTimeoutMs);
    return *res;
}

static debounce_t &debounce_highlighting() {
    const long kHighlightTimeoutMs = 500;
    static auto res = new_debounce_t(kHighlightTimeoutMs);
    return *res;
}

static debounce_t &debounce_history_pager() {
    const long kHistoryPagerTimeoutMs = 500;
    static auto res = new_debounce_t(kHistoryPagerTimeoutMs);
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

void apply_edit(wcstring *target, std::vector<highlight_spec_t> *colors, const edit_t &edit) {
    size_t offset = edit.offset;
    target->replace(offset, edit.length, edit.replacement);

    // Now do the same to highlighting.
    auto it = colors->begin() + offset;
    colors->erase(it, it + edit.length);
    highlight_spec_t last_color = offset == 0 ? highlight_spec_t{} : colors->at(offset - 1);
    colors->insert(it, edit.replacement.size(), last_color);
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

bool editable_line_t::undo() {
    bool did_undo = false;
    maybe_t<int> last_group_id{-1};
    while (undo_history_.edits_applied != 0) {
        const edit_t &edit = undo_history_.edits.at(undo_history_.edits_applied - 1);
        if (did_undo && (!edit.group_id.has_value() || edit.group_id != last_group_id)) {
            // We've restored all the edits in this logical undo group
            break;
        }
        last_group_id = edit.group_id;
        undo_history_.edits_applied--;
        edit_t inverse = edit_t(edit.offset, edit.replacement.size(), L"");
        inverse.replacement = edit.old;
        size_t old_position = edit.cursor_position_before_edit;
        apply_edit(&text_, &colors_, inverse);
        set_position(old_position);
        did_undo = true;
    }

    end_edit_group();
    undo_history_.may_coalesce = false;
    return did_undo;
}

void editable_line_t::clear() {
    undo_history_.clear();
    if (empty()) return;
    apply_edit(&text_, &colors_, edit_t(0, text_.length(), L""));
    set_position(0);
}

void editable_line_t::push_edit(edit_t edit, bool allow_coalesce) {
    bool is_insertion = edit.length == 0;
    /// Coalescing insertion does not create a new undo entry but adds to the last insertion.
    if (allow_coalesce && is_insertion && want_to_coalesce_insertion_of(edit.replacement)) {
        assert(edit.offset == position());
        edit_t &last_edit = undo_history_.edits.back();
        last_edit.replacement.append(edit.replacement);
        apply_edit(&text_, &colors_, edit);
        set_position(position() + edit.replacement.size());

        assert(undo_history_.may_coalesce);
        return;
    }

    // Assign a new group id or propagate the old one if we're in a logical grouping of edits
    if (edit_group_level_ != -1) {
        edit.group_id = edit_group_id_;
    }

    bool edit_does_nothing = edit.length == 0 && edit.replacement.empty();
    if (edit_does_nothing) return;
    if (undo_history_.edits_applied != undo_history_.edits.size()) {
        // After undoing some edits, the user is making a new edit;
        // we are about to create a new edit branch.
        // Discard all edits that were undone because we only support
        // linear undo/redo, they will be unreachable.
        undo_history_.edits.erase(undo_history_.edits.begin() + undo_history_.edits_applied,
                                  undo_history_.edits.end());
    }
    edit.cursor_position_before_edit = position();
    edit.old = text_.substr(edit.offset, edit.length);
    apply_edit(&text_, &colors_, edit);
    set_position(cursor_position_after_edit(edit));
    assert(undo_history_.edits_applied == undo_history_.edits.size());
    undo_history_.may_coalesce =
        is_insertion && (undo_history_.try_coalesce || edit.replacement.size() == 1);
    undo_history_.edits_applied++;
    undo_history_.edits.emplace_back(std::move(edit));
}

bool editable_line_t::redo() {
    bool did_redo = false;

    maybe_t<int> last_group_id{-1};
    while (undo_history_.edits_applied < undo_history_.edits.size()) {
        const edit_t &edit = undo_history_.edits.at(undo_history_.edits_applied);
        if (did_redo && (!edit.group_id.has_value() || edit.group_id != last_group_id)) {
            // We've restored all the edits in this logical undo group
            break;
        }
        last_group_id = edit.group_id;
        undo_history_.edits_applied++;
        apply_edit(&text_, &colors_, edit);
        set_position(cursor_position_after_edit(edit));
        did_redo = true;
    }

    end_edit_group();
    return did_redo;
}

void editable_line_t::begin_edit_group() {
    if (++edit_group_level_ == 0) {
        // Indicate that the next change must trigger the creation of a new history item
        undo_history_.may_coalesce = false;
        // Indicate that future changes should be coalesced into the same edit if possible.
        undo_history_.try_coalesce = true;
        // Assign a logical edit group id to future edits in this group
        edit_group_id_ += 1;
    }
}

void editable_line_t::end_edit_group() {
    if (edit_group_level_ == -1) {
        // Clamp the minimum value to -1 to prevent unbalanced end_edit_group() calls from breaking
        // everything.
        return;
    }

    if (--edit_group_level_ == -1) {
        undo_history_.try_coalesce = false;
        undo_history_.may_coalesce = false;
    }
}

/// Whether we want to append this string to the previous edit.
bool editable_line_t::want_to_coalesce_insertion_of(const wcstring &str) const {
    // The previous edit must support coalescing.
    if (!undo_history_.may_coalesce) return false;
    // Only consolidate single character inserts.
    if (str.size() != 1) return false;
    // Make an undo group after every space.
    if (str.at(0) == L' ' && !undo_history_.try_coalesce) return false;
    assert(!undo_history_.edits.empty());
    const edit_t &last_edit = undo_history_.edits.back();
    // Don't add to the last edit if it deleted something.
    if (last_edit.length != 0) return false;
    // Must not have moved the cursor!
    if (cursor_position_after_edit(last_edit) != position()) return false;
    return true;
}

// Make the search case-insensitive unless we have an uppercase character.
static history_search_flags_t smartcase_flags(const wcstring &query) {
    return query == wcstolower(query) ? history_search_ignore_case : 0;
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

    struct match_t {
        /// The text of the match.
        wcstring text;
        /// The offset of the current search string in this match.
        size_t offset;
    };

   private:
    /// The type of search performed.
    mode_t mode_{inactive};

    /// Our history search itself.
    maybe_t<rust::Box<HistorySearch>> search_;

    /// The ordered list of matches. This may grow long.
    std::vector<match_t> matches_;

    /// A set of new items to skip, corresponding to matches_ and anything added in skip().
    std::set<wcstring> skips_;

    /// Index into our matches list.
    size_t match_index_{0};

    /// The offset of the current token in the command line. Only non-zero for a token search.
    size_t token_offset_{0};

    /// Adds the given match if we haven't seen it before.
    void add_if_new(match_t match) {
        if (add_skip(match.text)) {
            matches_.push_back(std::move(match));
        }
    }

    /// Attempt to append matches from the current history item.
    /// \return true if something was appended.
    bool append_matches_from_search() {
        auto find = [this](const wcstring &haystack, const wcstring &needle) {
            if ((*search_)->ignores_case()) {
                return ifind(haystack, needle);
            }
            return haystack.find(needle);
        };
        const size_t before = matches_.size();
        auto text = *(*search_)->current_string();
        const wcstring &needle = search_string();
        if (mode_ == line || mode_ == prefix) {
            size_t offset = find(text, needle);
            // FIXME: Previous versions asserted out if this wasn't true.
            // This could be hit with a needle of "ö" and haystack of "echo Ö"
            // I'm not sure why - this points to a bug in ifind (probably wrong locale?)
            // However, because the user experience of having it crash is horrible,
            // and the worst thing that can otherwise happen here is that a search is unsuccessful,
            // we just check it instead.
            if (offset != wcstring::npos) {
                add_if_new({std::move(text), offset});
            }
        } else if (mode_ == token) {
            auto tok = new_tokenizer(text.c_str(), TOK_ACCEPT_UNFINISHED);

            std::vector<match_t> local_tokens;
            while (auto token = tok->next()) {
                if (token->type_ != token_type_t::string) continue;
                wcstring text = *tok->text_of(*token);
                size_t offset = find(text, needle);
                if (offset != wcstring::npos) {
                    local_tokens.push_back({std::move(text), offset});
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
        while ((*search_)->go_to_next_match(history_search_direction_t::Backward)) {
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
        return dir == history_search_direction_t::Forward ? move_forwards() : move_backwards();
    }

    /// Go to the beginning (earliest) of the search.
    void go_to_beginning() {
        if (matches_.empty()) return;
        match_index_ = matches_.size() - 1;
    }

    /// Go to the end (most recent) of the search.
    void go_to_end() { match_index_ = 0; }

    /// \return the current search result.
    const match_t &current_result() const {
        assert(match_index_ < matches_.size() && "Invalid match index");
        return matches_.at(match_index_);
    }

    /// \return the string we are searching for.
    const wcstring search_string() const { return *(*search_)->original_term(); }

    /// \return the range of the original search string in the new command line.
    maybe_t<source_range_t> search_range_if_active() const {
        if (!active() || is_at_end()) {
            return {};
        }
        return {{static_cast<source_offset_t>(token_offset_ + current_result().offset),
                 static_cast<source_offset_t>(search_string().length())}};
    }

    /// \return whether we are at the end (most recent) of our search.
    bool is_at_end() const { return match_index_ == 0; }

    // Add an item to skip.
    // \return true if it was added, false if already present.
    bool add_skip(const wcstring &str) { return skips_.insert(str).second; }

    /// Reset, beginning a new line or token mode search.
    void reset_to_mode(const wcstring &text, const HistorySharedPtr &hist, mode_t mode,
                       size_t token_offset) {
        assert(mode != inactive && "mode cannot be inactive in this setter");
        skips_ = {text};
        matches_ = {{text, 0}};
        match_index_ = 0;
        mode_ = mode;
        token_offset_ = token_offset;
        history_search_flags_t flags = history_search_no_dedup | smartcase_flags(text);
        // We can skip dedup in history_search_t because we do it ourselves in skips_.
        search_ = rust_history_search_new(
            hist, text.c_str(),
            by_prefix() ? history_search_type_t::Prefix : history_search_type_t::Contains, flags,
            0);
    }

    /// Reset to inactive search.
    void reset() {
        matches_.clear();
        skips_.clear();
        match_index_ = 0;
        mode_ = inactive;
        token_offset_ = 0;
        search_ = maybe_t<rust::Box<HistorySearch>>{};
    }
};

/// The result of an autosuggestion computation.
struct autosuggestion_t {
    // The text to use, as an extension of the command line.
    wcstring text{};

    // The string which was searched for.
    wcstring search_string{};

    // The list of completions which may need loading.
    std::vector<wcstring> needs_load{};

    // Whether the autosuggestion should be case insensitive.
    // This is true for file-generated autosuggestions, but not for history.
    bool icase{false};

    // Clear our contents.
    void clear() {
        text.clear();
        search_string.clear();
    }

    // \return whether we have empty text.
    bool empty() const { return text.empty(); }

    autosuggestion_t() = default;
    autosuggestion_t(wcstring text, wcstring search_string, bool icase)
        : text(std::move(text)), search_string(std::move(search_string)), icase(icase) {}
};

struct highlight_result_t {
    std::vector<highlight_spec_t> colors;
    wcstring text;
};

struct history_pager_result_t {
    completion_list_t matched_commands;
    size_t final_index;
    bool have_more_results;
};

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

}  // namespace

/// Data wrapping up the visual selection.
namespace {
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

/// A value-type struct representing a layout that can be rendered.
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

    /// The matching range of the command line from a history search. If non-empty, then highlight
    /// the range within the text.
    maybe_t<source_range_t> history_search_range{};

    /// The result of evaluating the left, mode and right prompt commands.
    /// That is, this the text of the prompts, not the commands to produce them.
    wcstring left_prompt_buff{};
    wcstring mode_prompt_buff{};
    wcstring right_prompt_buff{};
};
}  // namespace

/// A struct describing the state of the interactive reader. These states can be stacked, in case
/// reader_readline() calls are nested. This happens when the 'read' builtin is used.
class reader_data_t : public std::enable_shared_from_this<reader_data_t> {
   public:
    /// Configuration for the reader.
    reader_config_t conf;
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
    /// The current autosuggestion.
    autosuggestion_t autosuggestion;
    /// Current pager.
    pager_t pager;
    /// The output of the pager.
    page_rendering_t current_page_rendering;
    /// When backspacing, we temporarily suppress autosuggestions.
    bool suppress_autosuggestion{false};

    /// HACK: A flag to reset the loop state from the outside.
    bool reset_loop_state{false};

    /// Whether this is the first prompt.
    bool first_prompt{true};

    /// The time when the last flash() completed
    std::chrono::time_point<std::chrono::steady_clock> last_flash;

    /// The representation of the current screen contents.
    screen_t screen;

    /// The source of input events.
    inputter_t inputter;
    /// The history.
    maybe_t<rust::Box<HistorySharedPtr>> history{};
    /// The history search.
    reader_history_search_t history_search{};
    /// Whether the in-pager history search is active.
    bool history_pager_active{false};
    /// The range in history covered by the history pager's current page.
    size_t history_pager_history_index_start{static_cast<size_t>(-1)};
    size_t history_pager_history_index_end{static_cast<size_t>(-1)};

    /// The cursor selection mode.
    cursor_selection_mode_t cursor_selection_mode{cursor_selection_mode_t::exclusive};

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

    bool is_navigating_pager_contents() const {
        return this->pager.is_navigating_contents() || history_pager_active;
    }

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
    void maybe_refilter_pager(const editable_line_t *el);
    void fill_history_pager(bool new_search, history_search_direction_t direction =
                                                 history_search_direction_t::Backward);

    /// Do what we need to do whenever our pager selection changes.
    void pager_selection_changed();

    /// Expand abbreviations at the current cursor position, minus cursor_backtrack.
    bool expand_abbreviation_at_cursor(size_t cursor_backtrack);

    /// \return true if the command line has changed and repainting is needed. If \p colors is not
    /// null, then also return true if the colors have changed.
    using highlight_list_t = std::vector<highlight_spec_t>;
    bool is_repaint_needed(const highlight_list_t *mcolors = nullptr) const;

    /// Generate a new layout data from the current state of the world.
    /// If \p mcolors has a value, then apply it; otherwise extend existing colors.
    layout_data_t make_layout_data() const;

    /// Generate a new layout data from the current state of the world, and paint with it.
    /// If \p mcolors has a value, then apply it; otherwise extend existing colors.
    void layout_and_repaint(const wchar_t *reason) {
        this->rendered_layout = make_layout_data();
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
    const parser_t &parser() const { return *parser_ref; }

    /// Convenience cover over exec_count().
    uint64_t exec_count() const { return parser().libdata().exec_count; }

    reader_data_t(std::shared_ptr<parser_t> parser, HistorySharedPtr &hist, reader_config_t &&conf)
        : conf(std::move(conf)),
          parser_ref(std::move(parser)),
          inputter(*parser_ref, conf.in),
          history(hist.clone()) {}

    void update_buff_pos(editable_line_t *el, maybe_t<size_t> new_pos = none_t());

    void kill(editable_line_t *el, size_t begin_idx, size_t length, int mode, int newv);
    /// Inserts a substring of str given by start, len at the cursor position.
    void insert_string(editable_line_t *el, const wcstring &str);
    /// Erase @length characters starting at @offset.
    void erase_substring(editable_line_t *el, size_t offset, size_t length);
    /// Replace the text of length @length at @offset by @replacement.
    void replace_substring(editable_line_t *el, size_t offset, size_t length, wcstring replacement);
    void push_edit(editable_line_t *el, edit_t edit);

    /// Insert the character into the command line buffer and print it to the screen using syntax
    /// highlighting, etc.
    void insert_char(editable_line_t *el, wchar_t c) { insert_string(el, wcstring{c}); }

    /// Read a command to execute, respecting input bindings.
    /// \return the command, or none if we were asked to cancel (e.g. SIGHUP).
    maybe_t<wcstring> readline(int nchars);

    /// Reflect our current data in the command line state snapshot.
    /// This is called before we run any fish script, so that the commandline builtin can see our
    /// state.
    void update_commandline_state() const;

    /// Apply any changes from the reader snapshot. This is called after running fish script,
    /// incorporating changes from the commandline builtin.
    void apply_commandline_state_changes();

    /// Compute completions and update the pager and/or commandline as needed.
    void compute_and_apply_completions(readline_cmd_t c, readline_loop_state_t &rls);

    /// Given that the user is tab-completing a token \p wc whose cursor is at \p pos in the token,
    /// try expanding it as a wildcard, populating \p result with the expanded string.
    expand_result_t::result_t try_expand_wildcard(wcstring wc, size_t pos, wcstring *result);

    void move_word(editable_line_t *el, bool move_right, bool erase, move_word_style_t style,
                   bool newv);

    void run_input_command_scripts(const std::vector<wcstring> &cmds);
    maybe_t<char_event_t> read_normal_chars(readline_loop_state_t &rls);
    void handle_readline_command(readline_cmd_t cmd, readline_loop_state_t &rls);

    // Handle readline_cmd_t::execute. This may mean inserting a newline if the command is
    // unfinished. It may also set 'finished' and 'cmd' inside the rls.
    // \return true on success, false if we got an error, in which case the caller should fire the
    // error event.
    bool handle_execute(readline_loop_state_t &rls);

    // Add the current command line contents to history.
    void add_to_history();

    // Expand abbreviations before execution.
    // Replace the command line with any abbreviations as needed.
    // \return the test result, which may be incomplete to insert a newline, or an error.
    parser_test_error_bits_t expand_for_execute();

    void clear_pager();
    void select_completion_in_direction(selection_motion_t dir,
                                        bool force_selection_change = false);
    void flash();

    maybe_t<source_range_t> get_selection() const;

    void completion_insert(const wcstring &val, size_t token_end, complete_flags_t flags);

    bool can_autosuggest() const;
    void autosuggest_completed(autosuggestion_t result);
    void update_autosuggestion();
    void accept_autosuggestion(
        bool full, bool single = false,
        move_word_style_t style = move_word_style_t::move_word_style_punctuation);
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
    void update_termsize() { termsize_update_ffi(reinterpret_cast<unsigned char *>(&parser())); }

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
    modes->c_oflag |= ONLCR;    // "translate newline to carriage return-newline" - without
                                // you see staircase output.

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
    modes->c_cc[VQUIT] = disabling_char;
}

static void term_fix_external_modes(struct termios *modes) {
    // Turning off OPOST or ONLCR breaks output (staircase effect), we don't allow it.
    // See #7133.
    modes->c_oflag |= OPOST;
    modes->c_oflag |= ONLCR;
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

extern "C" {
void reader_sighup() {
    // Beware, we may be in a signal handler.
    s_sighup_received = true;
}
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
static void term_donate(bool quiet = false) {
    while (tcsetattr(STDIN_FILENO, TCSANOW, &tty_modes_for_external_cmds) == -1) {
        if (errno == EIO) redirect_tty_output();
        if (errno != EINTR) {
            if (!quiet) {
                FLOGF(warning, _(L"Could not set terminal mode for new job"));
                wperror(L"tcsetattr");
            }
            break;
        }
    }
}

/// Copy the (potentially changed) terminal modes and use them from now on.
void term_copy_modes() {
    struct termios modes;
    tcgetattr(STDIN_FILENO, &modes);
    std::memcpy(&tty_modes_for_external_cmds, &modes, sizeof tty_modes_for_external_cmds);
    term_fix_external_modes(&tty_modes_for_external_cmds);

    // Copy flow control settings to shell modes.
    if (tty_modes_for_external_cmds.c_iflag & IXON) {
        shell_modes.c_iflag |= IXON;
    } else {
        shell_modes.c_iflag &= ~IXON;
    }
    if (tty_modes_for_external_cmds.c_iflag & IXOFF) {
        shell_modes.c_iflag |= IXOFF;
    } else {
        shell_modes.c_iflag &= ~IXOFF;
    }
}

/// Grab control of terminal.
static void term_steal() {
    term_copy_modes();
    while (tcsetattr(STDIN_FILENO, TCSANOW, &shell_modes) == -1) {
        if (errno == EIO) redirect_tty_output();
        if (errno != EINTR) {
            FLOGF(warning, _(L"Could not set terminal mode for shell"));
            perror("tcsetattr");
            break;
        }
    }

    termsize_invalidate_tty();
}

bool fish_is_unwinding_for_exit() {
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
    if (new_pos.has_value()) {
        el->set_position(*new_pos);
    }
    size_t buff_pos = el->position();
    if (el == &command_line && selection.has_value()) {
        if (selection->begin <= buff_pos) {
            selection->start = selection->begin;
            selection->stop =
                buff_pos + (cursor_selection_mode == cursor_selection_mode_t::inclusive ? 1 : 0);
        } else {
            selection->start = buff_pos;
            selection->stop = selection->begin +
                              (cursor_selection_mode == cursor_selection_mode_t::inclusive ? 1 : 0);
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
           check(history_search.search_range_if_active() != last.history_search_range,
                 L"history search") ||
           check(autosuggestion.text != last.autosuggestion, L"autosuggestion") ||
           check(left_prompt_buff != last.left_prompt_buff, L"left_prompt") ||
           check(mode_prompt_buff != last.mode_prompt_buff, L"mode_prompt") ||
           check(right_prompt_buff != last.right_prompt_buff, L"right_prompt") ||
           check(pager.rendering_needs_update(current_page_rendering), L"pager");
}

layout_data_t reader_data_t::make_layout_data() const {
    layout_data_t result{};
    bool focused_on_pager = active_edit_line() == &pager.search_field_line;
    result.text = command_line.text();
    result.colors = command_line.colors();
    assert(result.text.size() == result.colors.size());
    result.position = focused_on_pager ? pager.cursor_position() : command_line.position();
    result.selection = selection;
    result.focused_on_pager = (active_edit_line() == &pager.search_field_line);
    result.history_search_range = history_search.search_range_if_active();
    result.autosuggestion = autosuggestion.text;
    result.left_prompt_buff = left_prompt_buff;
    result.mode_prompt_buff = mode_prompt_buff;
    result.right_prompt_buff = right_prompt_buff;
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
        full_line = combine_command_and_autosuggestion(cmd_line->text(), autosuggestion.text);
    }

    // Copy the colors and extend them with autosuggestion color.
    std::vector<highlight_spec_t> colors = data.colors;

    // Highlight any history search.
    if (!conf.in_silent_mode && data.history_search_range) {
        // std::min gets confused about types here.
        size_t end = data.history_search_range->end();
        if (colors.size() < end) {
            end = colors.size();
        }

        for (size_t i = data.history_search_range->start; i < end; i++) {
            colors.at(i).background = highlight_role_t::search_match;
        }
    }

    // Apply any selection.
    if (data.selection.has_value()) {
        highlight_spec_t selection_color = {highlight_role_t::selection,
                                            highlight_role_t::selection};
        auto end = std::min(selection->stop, colors.size());
        for (size_t i = data.selection->start; i < end; i++) {
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
    screen.write(mode_prompt_buff + left_prompt_buff, right_prompt_buff, full_line,
                 cmd_line->size(), colors, indents, data.position, parser().vars(), pager,
                 current_page_rendering, data.focused_on_pager);
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

extern "C" {
// This is called from a signal handler!
void reader_handle_sigint() { interrupted = SIGINT; }
}

/// Make sure buffers are large enough to hold the current string length.
void reader_data_t::command_line_changed(const editable_line_t *el) {
    ASSERT_IS_MAIN_THREAD();
    if (el == &this->command_line) {
        // Update the gen count.
        s_generation.store(1 + read_generation_count(), std::memory_order_relaxed);
    } else if (el == &this->pager.search_field_line) {
        if (history_pager_active) {
            fill_history_pager(true, history_search_direction_t::Backward);
            return;
        }
        this->pager.refilter_completions();
        this->pager_selection_changed();
    }
    // Ensure that the commandline builtin sees our new state.
    update_commandline_state();
}

void reader_data_t::maybe_refilter_pager(const editable_line_t *el) {
    if (el == &this->pager.search_field_line) {
        command_line_changed(el);
    }
}

static history_pager_result_t history_pager_search(const HistorySharedPtr &history,
                                                   history_search_direction_t direction,
                                                   size_t history_index,
                                                   const wcstring &search_string) {
    // Limit the number of elements to half the screen like we do for completions
    // Note that this is imperfect because we could have a multi-column layout.
    //
    // We can still push fish further upward in case the first entry is multiline,
    // but that can't really be helped.
    // (subtract 2 for the search line and the prompt)
    size_t page_size = std::max(termsize_last().height / 2 - 2, (rust::isize)12);

    completion_list_t completions;
    rust::Box<HistorySearch> search =
        rust_history_search_new(history, search_string.c_str(), history_search_type_t::Contains,
                                smartcase_flags(search_string), history_index);
    bool next_match_found = search->go_to_next_match(direction);
    if (!next_match_found) {
        // If there were no matches, try again with subsequence search
        search = rust_history_search_new(history, search_string.c_str(),
                                         history_search_type_t::ContainsSubsequence,
                                         smartcase_flags(search_string), history_index);
        next_match_found = search->go_to_next_match(direction);
    }
    while (completions.size() < page_size && next_match_found) {
        const history_item_t &item = search->current_item();
        completions.push_back(completion_t{
            *item.str(), L"", string_fuzzy_match_t::exact_match(),
            COMPLETE_REPLACES_COMMANDLINE | COMPLETE_DONT_ESCAPE | COMPLETE_DONT_SORT});

        next_match_found = search->go_to_next_match(direction);
    }
    size_t last_index = search->current_index();
    if (direction == history_search_direction_t::Forward)
        std::reverse(completions.begin(), completions.end());
    return {completions, last_index, search->go_to_next_match(direction)};
}

void reader_data_t::fill_history_pager(bool new_search, history_search_direction_t direction) {
    assert(!new_search || direction == history_search_direction_t::Backward);
    size_t index;
    if (new_search) {
        index = 0;
    } else if (direction == history_search_direction_t::Forward) {
        index = history_pager_history_index_start;
    } else {
        assert(direction == history_search_direction_t::Backward);
        index = history_pager_history_index_end;
    }
    const wcstring &search_term = pager.search_field_line.text();
    auto shared_this = this->shared_from_this();
    std::function<history_pager_result_t()> func = [=]() {
        return history_pager_search(**shared_this->history, direction, index, search_term);
    };
    std::function<void(const history_pager_result_t &)> completion =
        [=](const history_pager_result_t &result) {
            if (search_term != shared_this->pager.search_field_line.text())
                return;  // Stale request.
            if (result.matched_commands.empty() && !new_search) {
                // No more matches, keep the existing ones and flash.
                shared_this->flash();
                return;
            }
            if (direction == history_search_direction_t::Forward) {
                shared_this->history_pager_history_index_start = result.final_index;
                shared_this->history_pager_history_index_end = index;
            } else {
                shared_this->history_pager_history_index_start = index;
                shared_this->history_pager_history_index_end = result.final_index;
            }
            shared_this->pager.extra_progress_text =
                result.have_more_results ? _(L"Search again for more results") : L"";
            shared_this->pager.set_completions(result.matched_commands);
            shared_this->select_completion_in_direction(selection_motion_t::next, true);
            shared_this->super_highlight_me_plenty();
            shared_this->layout_and_repaint(L"history-pager");
        };
    auto &debouncer = debounce_history_pager();
    debounce_perform_with_completion(debouncer, std::move(func), std::move(completion));
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

/// Expand an abbreviation replacer, which may mean running its function.
/// \return the replacement, or none to skip it. This may run fish script!
maybe_t<abbrs_replacement_t> expand_replacer(SourceRange range, const wcstring &token,
                                             const abbrs_replacer_t &repl, parser_t &parser) {
    if (!repl.is_function) {
        // Literal replacement cannot fail.
        FLOGF(abbrs, L"Expanded literal abbreviation <%ls> -> <%ls>", token.c_str(),
              (*repl.replacement).c_str());
        return abbrs_replacement_from(range, *repl.replacement, *repl.set_cursor_marker,
                                      repl.has_cursor_marker);
    }

    wcstring cmd = escape_string(*repl.replacement);
    cmd.push_back(L' ');
    cmd.append(escape_string(token));

    scoped_push<bool> not_interactive(&parser.libdata().is_interactive, false);

    std::vector<wcstring> outputs{};
    int ret = exec_subshell(cmd, parser, outputs, false /* not apply_exit_status */);
    if (ret != STATUS_CMD_OK) {
        return none();
    }
    wcstring result = join_strings(outputs, L'\n');
    FLOGF(abbrs, L"Expanded function abbreviation <%ls> -> <%ls>", token.c_str(), result.c_str());
    return abbrs_replacement_from(range, result, *repl.set_cursor_marker, repl.has_cursor_marker);
}

// Extract all the token ranges in \p str, along with whether they are an undecorated command.
// Tokens containing command substitutions are skipped; this ensures tokens are non-overlapping.
struct positioned_token_t {
    source_range_t range;
    bool is_cmd;
};
static std::vector<positioned_token_t> extract_tokens(const wcstring &str) {
    using namespace ast;

    parse_tree_flags_t ast_flags = parse_flag_continue_after_error |
                                   parse_flag_accept_incomplete_tokens |
                                   parse_flag_leave_unterminated;
    auto ast = ast_parse(str, ast_flags);

    // Helper to check if a node is the command portion of an undecorated statement.
    auto is_command = [&](const ast::node_t &node) {
        for (auto cursor = node.ptr(); cursor->has_value(); cursor = cursor->parent()) {
            if (const auto *stmt = cursor->try_as_decorated_statement()) {
                if (!stmt->has_opt_decoration() && node.pointer_eq(*stmt->command().ptr())) {
                    return true;
                }
            }
        }
        return false;
    };

    wcstring cmdsub_contents;
    std::vector<positioned_token_t> result;
    for (auto tv = new_ast_traversal(*ast->top());;) {
        auto node = tv->next();
        if (!node->has_value()) break;
        // We are only interested in leaf nodes with source.
        if (node->category() != category_t::leaf) continue;
        source_range_t r = node->source_range();
        if (r.length == 0) continue;

        // If we have command subs, then we don't include this token; instead we recurse.
        bool has_cmd_subs = false;
        size_t cmdsub_cursor = r.start, cmdsub_start = 0, cmdsub_end = 0;
        while (parse_util_locate_cmdsubst_range(str, &cmdsub_cursor, &cmdsub_contents,
                                                &cmdsub_start, &cmdsub_end,
                                                true /* accept incomplete */) > 0) {
            if (cmdsub_start >= r.end()) {
                break;
            }
            has_cmd_subs = true;
            for (positioned_token_t t : extract_tokens(cmdsub_contents)) {
                // cmdsub_start is the open paren; the contents start one after it.
                t.range.start += static_cast<source_offset_t>(cmdsub_start + 1);
                result.push_back(t);
            }
        }

        if (!has_cmd_subs) {
            // Common case of no command substitutions in this leaf node.
            result.push_back(positioned_token_t{r, is_command(*node)});
        }
    }
    return result;
}

/// Expand abbreviations at the given cursor position.
/// \return the replacement. This does NOT inspect the current reader data.
maybe_t<abbrs_replacement_t> reader_expand_abbreviation_at_cursor(const wcstring &cmdline,
                                                                  size_t cursor_pos,
                                                                  parser_t &parser) {
    // Find the token containing the cursor. Usually users edit from the end, so walk backwards.
    const auto tokens = extract_tokens(cmdline);
    auto iter = std::find_if(tokens.rbegin(), tokens.rend(), [&](const positioned_token_t &t) {
        return t.range.contains_inclusive(cursor_pos);
    });
    if (iter == tokens.rend()) {
        return none();
    }
    source_range_t range = iter->range;
    abbrs_position_t position =
        iter->is_cmd ? abbrs_position_t::command : abbrs_position_t::anywhere;

    wcstring token_str = cmdline.substr(range.start, range.length);
    auto replacers = abbrs_match(token_str, position);
    for (const auto &replacer : replacers) {
        if (auto replacement = expand_replacer(range, token_str, replacer, parser)) {
            return replacement;
        }
    }
    return none();
}

/// Expand abbreviations at the current cursor position, minus the given cursor backtrack. This may
/// change the command line but does NOT repaint it. This is to allow the caller to coalesce
/// repaints.
bool reader_data_t::expand_abbreviation_at_cursor(size_t cursor_backtrack) {
    bool result = false;
    editable_line_t *el = active_edit_line();

    if (conf.expand_abbrev_ok && el == &command_line) {
        // Try expanding abbreviations.
        this->update_commandline_state();
        size_t cursor_pos = el->position() - std::min(el->position(), cursor_backtrack);
        if (auto replacement =
                reader_expand_abbreviation_at_cursor(el->text(), cursor_pos, this->parser())) {
            push_edit(el, edit_t{replacement->range, *replacement->text});
            if (replacement->has_cursor) {
                update_buff_pos(el, replacement->cursor);
            } else {
                update_buff_pos(el, none());
            }
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
            fish_title_command.append(escape_string(cmd, ESCAPE_NO_QUOTED | ESCAPE_NO_TILDE));
        }
    }

    std::vector<wcstring> lst;
    (void)exec_subshell(fish_title_command, parser, lst, false /* ignore exit status */);
    if (!lst.empty()) {
        wcstring title_line = L"\x1B]0;";
        for (const auto &i : lst) {
            title_line += i;
        }
        title_line += L"\a";
        std::string narrow = wcs2string(title_line);
        ignore_result(write_loop(STDOUT_FILENO, narrow.data(), narrow.size()));
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
        std::vector<wcstring> mode_indicator_list;
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
            std::vector<wcstring> prompt_list;
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
                std::vector<wcstring> prompt_list;
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

    // Some prompt may have requested an exit (#8033).
    this->exit_loop_requested |= parser().libdata().exit_current_script;
    parser().libdata().exit_current_script = false;
}

void reader_init() {
    // Save the initial terminal mode.
    tcgetattr(STDIN_FILENO, &terminal_mode_on_startup);

    // Set the mode used for program execution, initialized to the current mode.
    std::memcpy(&tty_modes_for_external_cmds, &terminal_mode_on_startup,
                sizeof tty_modes_for_external_cmds);
    term_fix_external_modes(&tty_modes_for_external_cmds);

    // Set the mode used for the terminal, initialized to the current mode.
    std::memcpy(&shell_modes, &terminal_mode_on_startup, sizeof shell_modes);

    // Disable flow control by default.
    tty_modes_for_external_cmds.c_iflag &= ~IXON;
    tty_modes_for_external_cmds.c_iflag &= ~IXOFF;
    shell_modes.c_iflag &= ~IXON;
    shell_modes.c_iflag &= ~IXOFF;

    term_fix_modes(&shell_modes);

    // Set up our fixed terminal modes once,
    // so we don't get flow control just because we inherited it.
    if (is_interactive_session() && getpgrp() == tcgetpgrp(STDIN_FILENO)) {
        term_donate(/* quiet */ true);
    }
}

/// Restore the term mode if we own the terminal and are interactive (#8705).
/// It's important we do this before restore_foreground_process_group,
/// otherwise we won't think we own the terminal.
void restore_term_mode() {
    if (!is_interactive_session() || getpgrp() != tcgetpgrp(STDIN_FILENO)) return;

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
        case rl::history_pager:
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
        case rl::nextd_or_forward_word:
        case rl::prevd_or_backward_word:
        case rl::delete_char:
        case rl::backward_delete_char:
        case rl::kill_line:
        case rl::yank:
        case rl::yank_pop:
        case rl::backward_kill_line:
        case rl::kill_whole_line:
        case rl::kill_inner_line:
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
    if (!str.empty()) {
        el->push_edit(edit_t(el->position(), 0, str),
                      !history_search.active() /* allow_coalesce */);
    }

    if (el == &command_line) {
        command_line_has_transient_edit = false;
        suppress_autosuggestion = false;
    }
    maybe_refilter_pager(el);
}

void reader_data_t::push_edit(editable_line_t *el, edit_t edit) {
    el->push_edit(std::move(edit), false /* allow_coalesce */);
    maybe_refilter_pager(el);
}

void reader_data_t::erase_substring(editable_line_t *el, size_t offset, size_t length) {
    push_edit(el, edit_t(offset, length, L""));
}

void reader_data_t::replace_substring(editable_line_t *el, size_t offset, size_t length,
                                      wcstring replacement) {
    push_edit(el, edit_t(offset, length, std::move(replacement)));
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
    bool do_replace_commandline = bool(flags & COMPLETE_REPLACES_COMMANDLINE);
    bool do_escape = !bool(flags & COMPLETE_DONT_ESCAPE);
    bool no_tilde = bool(flags & COMPLETE_DONT_ESCAPE_TILDES);

    const size_t cursor_pos = *inout_cursor_pos;
    bool back_into_trailing_quote = false;
    bool have_space_after_token = command_line[cursor_pos] == L' ';

    if (do_replace_commandline) {
        assert(!do_escape && "unsupported completion flag");
        *inout_cursor_pos = val.size();
        return val;
    }

    if (do_replace) {
        size_t move_cursor;
        const wchar_t *begin, *end;

        const wchar_t *buff = command_line.c_str();
        parse_util_token_extent(buff, cursor_pos, &begin, &end, nullptr, nullptr);

        wcstring sb(buff, begin - buff);

        if (do_escape) {
            wcstring escaped =
                escape_string(val, ESCAPE_NO_QUOTED | (no_tilde ? ESCAPE_NO_TILDE : 0));
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
        quote = parse_util_get_quote_type(
            command_line.substr(cmdsub_offset, (cmdsub_end - cmdsub_begin)),
            cursor_pos - cmdsub_offset);

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

// Returns a function that can be invoked (potentially
// on a background thread) to determine the autosuggestion
static std::function<autosuggestion_t(void)> get_autosuggestion_performer(
    parser_t &parser, const wcstring &search_string, size_t cursor_pos,
    const HistorySharedPtr &history) {
    const uint32_t generation_count = read_generation_count();
    auto vars = parser.vars().snapshot();
    const wcstring working_directory = vars->get_pwd_slash();
    // TODO: suspicious use of 'history' here
    // This is safe because histories are immortal, but perhaps
    // this should use shared_ptr
    const HistorySharedPtr *history_ptr = &history;
    return [=]() -> autosuggestion_t {
        ASSERT_IS_BACKGROUND_THREAD();
        autosuggestion_t nothing = {};
        operation_context_t ctx = get_bg_context(vars, generation_count);
        if (ctx.check_cancel()) {
            return nothing;
        }

        // Let's make sure we aren't using the empty string.
        if (search_string.empty()) {
            return nothing;
        }

        // Search history for a matching item.
        rust::Box<history_search_t> searcher =
            rust_history_search_new(*history_ptr, search_string.c_str(),
                                    history_search_type_t::Prefix, history_search_flags_t{}, 0);
        while (!ctx.check_cancel() &&
               searcher->go_to_next_match(history_search_direction_t::Backward)) {
            const history_item_t &item = searcher->current_item();

            // Skip items with newlines because they make terrible autosuggestions.
            if (item.str()->find(L'\n') != wcstring::npos) continue;

            if (autosuggest_validate_from_history(item, working_directory, ctx)) {
                // The command autosuggestion was handled specially, so we're done.
                // History items are case-sensitive, see #3978.
                return autosuggestion_t{*searcher->current_string(), search_string,
                                        false /* icase */};
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
        completion_request_options_t complete_flags = completion_request_options_t::autosuggest();
        std::vector<wcstring> needs_load;
        completion_list_t completions = complete(search_string, complete_flags, ctx, &needs_load);

        autosuggestion_t result{};
        result.search_string = search_string;
        result.needs_load = std::move(needs_load);
        result.icase = true;  // normal completions are case-insensitive.
        if (!completions.empty()) {
            completions_sort_and_prioritize(&completions, complete_flags);
            const completion_t &comp = completions.at(0);
            size_t cursor = cursor_pos;
            result.text = completion_apply_to_command_line(
                comp.completion, comp.flags, search_string, &cursor, true /* append only */);
        }
        return result;
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
void reader_data_t::autosuggest_completed(autosuggestion_t result) {
    ASSERT_IS_MAIN_THREAD();
    if (result.search_string == in_flight_autosuggest_request) {
        in_flight_autosuggest_request.clear();
    }
    if (result.search_string != command_line.text()) {
        // This autosuggestion is stale.
        return;
    }
    // Maybe load completions for commands discovered by this autosuggestion.
    bool loaded_new = false;
    for (const wcstring &to_load : result.needs_load) {
        if (complete_load(to_load, this->parser())) {
            FLOGF(complete, "Autosuggest found new completions for %ls, restarting",
                  to_load.c_str());
            loaded_new = true;
        }
    }
    if (loaded_new) {
        // We loaded new completions for this command.
        // Re-do our autosuggestion.
        this->update_autosuggestion();
    } else if (!result.empty() && can_autosuggest() &&
               string_prefixes_string_case_insensitive(result.search_string, result.text)) {
        // Autosuggestion is active and the search term has not changed, so we're good to go.
        autosuggestion = std::move(result);
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
    if (autosuggestion.text.size() > el.text().size() &&
        (autosuggestion.icase
             ? string_prefixes_string_case_insensitive(el.text(), autosuggestion.text)
             : string_prefixes_string(el.text(), autosuggestion.text))) {
        return;
    }

    // Do nothing if we've already kicked off this autosuggest request.
    if (el.text() == in_flight_autosuggest_request) return;
    in_flight_autosuggest_request = el.text();

    // Clear the autosuggestion and kick it off in the background.
    FLOG(reader_render, L"Autosuggesting");
    autosuggestion.clear();
    std::function<autosuggestion_t()> performer =
        get_autosuggestion_performer(parser(), el.text(), el.position(), **history);
    auto shared_this = this->shared_from_this();
    std::function<void(autosuggestion_t)> completion = [shared_this](autosuggestion_t result) {
        shared_this->autosuggest_completed(std::move(result));
    };
    debounce_perform_with_completion(debounce_autosuggestions(), std::move(performer),
                                     std::move(completion));
}

// Accept any autosuggestion by replacing the command line with it. If full is true, take the whole
// thing; if it's false, then respect the passed in style.
void reader_data_t::accept_autosuggestion(bool full, bool single, move_word_style_t style) {
    if (!autosuggestion.empty()) {
        // Accepting an autosuggestion clears the pager.
        clear_pager();

        // Accept the autosuggestion.
        if (full) {
            // Just take the whole thing.
            replace_substring(&command_line, 0, command_line.size(), autosuggestion.text);
        } else if (single) {
            replace_substring(&command_line, command_line.size(), 0,
                              autosuggestion.text.substr(command_line.size(), 1));
        } else {
            // Accept characters according to the specified style.
            auto state = new_move_word_state_machine(style);
            size_t want;
            for (want = command_line.size(); want < autosuggestion.text.size(); want++) {
                wchar_t wc = autosuggestion.text.at(want);
                if (!state->consume_char(wc)) break;
            }
            size_t have = command_line.size();
            replace_substring(&command_line, command_line.size(), 0,
                              autosuggestion.text.substr(have, want - have));
        }
    }
}

// Ensure we have no pager contents.
void reader_data_t::clear_pager() {
    pager.clear();
    history_pager_active = false;
    command_line_has_transient_edit = false;
}

void reader_data_t::select_completion_in_direction(selection_motion_t dir,
                                                   bool force_selection_change) {
    bool selection_changed = pager.select_next_completion_in_direction(dir, current_page_rendering);
    if (force_selection_change || selection_changed) {
        pager_selection_changed();
    }
}

/// Flash the screen. This function changes the color of the current line momentarily.
void reader_data_t::flash() {
    // Multiple flashes may be enqueued by keypress repeat events and can pile up to cause a
    // significant delay in processing future input while all the flash() calls complete, as we
    // effectively sleep for 100ms each go. See #8610.
    auto now = std::chrono::steady_clock::now();
    if ((now - last_flash) < std::chrono::milliseconds{50}) {
        last_flash = now;
        return;
    }

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

    pollint.tv_sec = 0;
    pollint.tv_nsec = 100 * 1000000;
    nanosleep(&pollint, nullptr);

    // Re-render with our saved data.
    data.colors = std::move(saved_colors);
    this->rendered_layout = std::move(data);
    paint_layout(L"unflash");

    // Save the time we stopped flashing as the time of the most recent flash. We can't just
    // increment the old `now` value because the sleep is non-deterministic.
    last_flash = std::chrono::steady_clock::now();
}

maybe_t<source_range_t> reader_data_t::get_selection() const {
    if (!this->selection.has_value()) return none();
    size_t start = this->selection->start;
    size_t len =
        std::min(this->selection->stop, this->command_line.size()) - this->selection->start;
    return source_range_t{static_cast<uint32_t>(start), static_cast<uint32_t>(len)};
}

/// Characters that may not be part of a token that is to be replaced by a case insensitive
/// completion.
const wchar_t *REPLACE_UNCLEAN = L"$*?({})";

/// Check if the specified string can be replaced by a case insensitive completion with the
/// specified flags.
///
/// Advanced tokens like those containing {}-style expansion can not at the moment be replaced,
/// other than if the new token is already an exact replacement, e.g. if the COMPLETE_DONT_ESCAPE
/// flag is set.
static bool reader_can_replace(const wcstring &in, complete_flags_t flags) {
    if (flags & COMPLETE_DONT_ESCAPE) {
        return true;
    }

    // Test characters that have a special meaning in any character position.
    if (in.find_first_of(REPLACE_UNCLEAN) != wcstring::npos) return false;

    return true;
}

/// Determine the best (lowest) match rank for a set of completions.
static uint32_t get_best_rank(const completion_list_t &comp) {
    uint32_t best_rank = UINT32_MAX;
    for (const auto &c : comp) {
        best_rank = std::min(best_rank, c.rank());
    }
    return best_rank;
}

/// Handle the list of completions. This means the following:
///
/// - If the list is empty, flash the terminal.
/// - If the list contains one element, write the whole element, and if the element does not end on
/// a '/', '@', ':', '.', ',', '-' or a '=', also write a trailing space.
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

    const wcstring tok(el->text(), token_begin, token_end - token_begin);

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

    auto best_rank = get_best_rank(comp);

    // Determine whether we are going to replace the token or not. If any commands of the best
    // rank do not require replacement, then ignore all those that want to use replacement.
    bool will_replace_token = true;
    for (const completion_t &el : comp) {
        if (el.rank() <= best_rank && !(el.flags & COMPLETE_REPLACES_TOKEN)) {
            will_replace_token = false;
            break;
        }
    }

    // Decide which completions survived. There may be a lot of them; it would be nice if we could
    // figure out how to avoid copying them here.
    completion_list_t surviving_completions;
    bool all_matches_exact_or_prefix = true;
    for (const completion_t &el : comp) {
        // Ignore completions with a less suitable match rank than the best.
        if (el.rank() > best_rank) continue;

        // Only use completions that match replace_token.
        bool completion_replace_token = static_cast<bool>(el.flags & COMPLETE_REPLACES_TOKEN);
        if (completion_replace_token != will_replace_token) continue;

        // Don't use completions that want to replace, if we cannot replace them.
        if (completion_replace_token && !reader_can_replace(tok, el.flags)) continue;

        // This completion survived.
        surviving_completions.push_back(el);
        all_matches_exact_or_prefix = all_matches_exact_or_prefix && el.match.is_exact_or_prefix();
    }

    if (surviving_completions.size() == 1) {
        // After sorting and stuff only one completion is left, use it.
        //
        // TODO: This happens when smartcase kicks in, e.g.
        // the token is "cma" and the options are "cmake/" and "CMakeLists.txt"
        // it would be nice if we could figure
        // out how to use it more.
        const completion_t &c = surviving_completions.at(0);

        // If this is a replacement completion, check that we know how to replace it, e.g. that
        // the token doesn't contain evil operators like {}.
        if (!(c.flags & COMPLETE_REPLACES_TOKEN) || reader_can_replace(tok, c.flags)) {
            completion_insert(c.completion, token_end, c.flags);
        }
        return true;
    }

    bool use_prefix = false;
    wcstring common_prefix;
    if (all_matches_exact_or_prefix) {
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
                    if (common_prefix.at(idx) != el.completion.at(idx)) break;
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
    if (will_replace_token || !all_matches_exact_or_prefix) {
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
            if (!is_interactive_session()) {
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
                    _(L"I appear to be an orphaned process, so I am quitting politely. My pid is "
                      L"%d.");
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
    pid_t shell_pid = getpid();

    // Set up key bindings.
    init_input();

    // Ensure interactive signal handling is enabled.
    signal_set_handlers_once(true);

    // Wait until we own the terminal.
    acquire_tty_or_exit(shell_pgid);

    // If fish has no valid pgroup (possible with firejail, see #5295) or is interactive,
    // ensure it owns the terminal. Also see #5909, #7060.
    if (shell_pgid == 0 || (is_interactive_session() && shell_pgid != shell_pid)) {
        shell_pgid = shell_pid;
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

    termsize_invalidate_tty();

    // Provide value for `status current-command`
    parser.libdata().status_vars.command = L"fish";
    // Also provide a value for the deprecated fish 2.0 $_ variable
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
                                                   : history_search.current_result().text;
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
                              move_word_style_t style, bool newv) {
    // Return if we are already at the edge.
    const size_t boundary = move_right ? el->size() : 0;
    if (el->position() == boundary) return;

    // When moving left, a value of 1 means the character at index 0.
    auto state = new_move_word_state_machine(style);
    const wchar_t *const command_line = el->text().c_str();
    const size_t start_buff_pos = el->position();

    size_t buff_pos = el->position();
    while (buff_pos != boundary) {
        size_t idx = (move_right ? buff_pos : buff_pos - 1);
        wchar_t c = command_line[idx];
        if (!state->consume_char(c)) break;
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
    size_t command_line_len = b.size();
    if (transient) {
        if (command_line_has_transient_edit) {
            command_line.undo();
        }
        command_line_has_transient_edit = true;
    }
    replace_substring(&command_line, 0, command_line.size(), b);
    command_line_changed(&command_line);

    // Don't set a position past the command line length.
    if (pos > command_line_len) pos = command_line_len;  //!OCLINT(parameter reassignment)
    update_buff_pos(&command_line, pos);

    // Clear history search.
    history_search.reset();
}

/// Run the specified command with the correct terminal modes, and while taking care to perform job
/// notification, set the title, etc.
static eval_res_t reader_run_command(parser_t &parser, const wcstring &cmd) {
    wcstring ft = *tok_command(cmd);

    // Provide values for `status current-command` and `status current-commandline`
    if (!ft.empty()) {
        parser.libdata().status_vars.command = ft;
        parser.libdata().status_vars.commandline = cmd;
        // Also provide a value for the deprecated fish 2.0 $_ variable
        parser.vars().set_one(L"_", ENV_GLOBAL, ft);
    }

    outputter_t &outp = outputter_t::stdoutput();
    reader_write_title(cmd, parser);
    outp.set_color(rgb_color_t::normal(), rgb_color_t::normal());
    term_donate();

    timepoint_t time_before = timef();
    auto eval_res = parser.eval(cmd, io_chain_t{});
    job_reap(parser, true);

    // Update the execution duration iff a command is requested for execution
    // issue - #4926
    if (!ft.empty()) {
        timepoint_t time_after = timef();
        double duration = time_after - time_before;
        long duration_ms = std::round(duration * 1000);
        parser.vars().set_one(ENV_CMD_DURATION, ENV_UNEXPORT, to_string(duration_ms));
    }

    term_steal();

    // Provide value for `status current-command`
    parser.libdata().status_vars.command = program_name;
    // Also provide a value for the deprecated fish 2.0 $_ variable
    parser.vars().set_one(L"_", ENV_GLOBAL, program_name);
    // Provide value for `status current-commandline`
    parser.libdata().status_vars.commandline = L"";

    if (have_proc_stat()) {
        proc_update_jiffies(parser);
    }

    return eval_res;
}

static parser_test_error_bits_t reader_shell_test(const parser_t &parser, const wcstring &bstr) {
    auto errors = new_parse_error_list();
    parser_test_error_bits_t res =
        parse_util_detect_errors(bstr, &*errors, true /* do accept incomplete */);

    if (res & PARSER_TEST_ERROR) {
        wcstring error_desc;
        parser.get_backtrace(bstr, *errors, error_desc);

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

void reader_data_t::highlight_complete(highlight_result_t result) {
    ASSERT_IS_MAIN_THREAD();
    in_flight_highlight_request.clear();
    if (result.text == command_line.text()) {
        assert(result.colors.size() == command_line.size());
        if (this->is_repaint_needed(&result.colors)) {
            command_line.set_colors(std::move(result.colors));
            this->layout_and_repaint(L"highlight");
        }
    }
}

// Given text and  whether IO is allowed, return a function that performs highlighting. The function
// may be invoked on a background thread.
static std::function<highlight_result_t(void)> get_highlight_performer(parser_t &parser,
                                                                       const editable_line_t &el,
                                                                       bool io_ok) {
    auto vars = parser.vars().snapshot();
    uint32_t generation_count = read_generation_count();
    return [=]() -> highlight_result_t {
        if (el.text().empty()) return {};
        operation_context_t ctx = get_bg_context(vars, generation_count);
        std::vector<highlight_spec_t> colors(el.text().size(), highlight_spec_t{});
        highlight_shell(el.text(), colors, ctx, io_ok, el.position());
        return highlight_result_t{std::move(colors), el.text()};
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
    std::function<highlight_result_t()> highlight_performer =
        get_highlight_performer(parser(), *el, true /* io_ok */);
    auto shared_this = this->shared_from_this();
    std::function<void(highlight_result_t)> completion = [shared_this](highlight_result_t result) {
        shared_this->highlight_complete(std::move(result));
    };
    debounce_perform_with_completion(debounce_highlighting(), std::move(highlight_performer),
                                     std::move(completion));
}

void reader_data_t::finish_highlighting_before_exec() {
    // Early-out if highlighting is not OK.
    if (!conf.highlight_ok) return;

    // Decide if our current highlighting is OK. If not we will do a quick highlight without I/O.
    bool current_highlight_ok = false;
    if (in_flight_highlight_request.empty()) {
        // There is no in-flight highlight request. Two possibilities:
        // 1: The user hit return after highlighting finished, so current highlighting is correct.
        // 2: The user hit return before highlighting started, so current highlighting is stale.
        // We can distinguish these based on what we last rendered.
        current_highlight_ok = (this->rendered_layout.text == command_line.text());
    } else if (in_flight_highlight_request == command_line.text()) {
        // The user hit return while our in-flight highlight request was still processing the text.
        // Wait for its completion to run, but not forever.
        namespace sc = std::chrono;
        auto now = sc::steady_clock::now();
        auto deadline = now + sc::milliseconds(kHighlightTimeoutForExecutionMs);
        while (now < deadline) {
            long timeout_usec = sc::duration_cast<sc::microseconds>(deadline - now).count();
            iothread_service_main_with_timeout(timeout_usec);

            // Note iothread_service_main_with_timeout will reentrantly modify us,
            // by invoking a completion.
            if (in_flight_highlight_request.empty()) break;
            now = sc::steady_clock::now();
        }

        // If our in_flight_highlight_request is now empty, it means it completed and we highlighted
        // successfully.
        current_highlight_ok = in_flight_highlight_request.empty();
    }

    if (!current_highlight_ok) {
        // We need to do a quick highlight without I/O.
        auto highlight_no_io =
            get_highlight_performer(parser(), command_line, false /* io not ok */);
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
        (*data->history)->save();
        data->history = history_with_name(name.c_str());
        commandline_state_snapshot()->history = (*data->history)->clone();
    }
}

void reader_change_cursor_selection_mode(cursor_selection_mode_t selection_mode) {
    // We don't need to _change_ if we're not initialized yet.
    reader_data_t *data = current_data_or_null();
    if (data) {
        data->cursor_selection_mode = selection_mode;
    }
}

static bool check_autosuggestion_enabled(const env_stack_t &vars) {
    if (auto val = vars.get(L"fish_autosuggestion_enabled")) {
        return val->as_string() != L"0";
    }
    return true;
}

void reader_set_autosuggestion_enabled(const env_stack_t &vars) {
    // We don't need to _change_ if we're not initialized yet.
    reader_data_t *data = current_data_or_null();
    if (data) {
        bool enable = check_autosuggestion_enabled(vars);
        if (data->conf.autosuggest_ok != enable) {
            data->conf.autosuggest_ok = enable;
            data->force_exec_prompt_and_repaint = true;
            data->inputter.queue_char(readline_cmd_t::repaint);
        }
    }
}

/// Add a new reader to the reader stack.
/// \return a shared pointer to it.
static std::shared_ptr<reader_data_t> reader_push_ret(parser_t &parser,
                                                      const wcstring &history_name,
                                                      reader_config_t &&conf) {
    rust::Box<HistorySharedPtr> hist = history_with_name(history_name.c_str());
    hist->resolve_pending();  // see #6892
    auto data = std::make_shared<reader_data_t>(parser.shared(), *hist, std::move(conf));
    reader_data_stack.push_back(data);
    data->command_line_changed(&data->command_line);
    if (reader_data_stack.size() == 1) {
        reader_interactive_init(parser);
    }
    data->update_commandline_state();
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
        *commandline_state_snapshot() = commandline_state_t{};
    } else {
        new_reader->screen.reset_abandoning_line(termsize_last().width);
        new_reader->update_commandline_state();
    }
}

void reader_data_t::import_history_if_necessary() {
    // Import history from older location (config path) if our current history is empty.
    if (history && (*history)->is_empty()) {
        (*history)->populate_from_config_path();
    }

    // Import history from bash, etc. if our current history is still empty and is the default
    // history.
    if (history && (*history)->is_empty() && (*history)->is_default()) {
        // Try opening a bash file. We make an effort to respect $HISTFILE; this isn't very complete
        // (AFAIK it doesn't have to be exported), and to really get this right we ought to ask bash
        // itself. But this is better than nothing.
        const auto var = vars().get(L"HISTFILE");
        wcstring path = (var ? var->as_string() : L"~/.bash_history");
        expand_tilde(path, vars());
        (*history)->populate_from_bash(path.c_str());
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
bool check_exit_loop_maybe_warning(reader_data_t *data) {
    // sighup always forces exit.
    if (s_sighup_received) return true;

    // Check if an exit is requested.
    if (data && data->exit_loop_requested) {
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

void reader_data_t::update_commandline_state() const {
    auto snapshot = commandline_state_snapshot();
    snapshot->text = this->command_line.text();
    snapshot->cursor_pos = this->command_line.position();
    snapshot->history = (*this->history)->clone();
    snapshot->selection = this->get_selection();
    snapshot->pager_mode = !this->pager.empty();
    snapshot->pager_fully_disclosed = this->current_page_rendering.remaining_to_disclose == 0;
    snapshot->search_mode = this->history_search.active();
    snapshot->initialized = true;
}

void reader_data_t::apply_commandline_state_changes() {
    // Only the text and cursor position may be changed.
    commandline_state_t state = commandline_get_state();
    if (state.text != this->command_line.text() ||
        state.cursor_pos != this->command_line.position()) {
        // The commandline builtin changed our contents.
        this->clear_pager();
        this->set_buffer_maintaining_pager(state.text, state.cursor_pos);
        this->reset_loop_state = true;
    }
}

expand_result_t::result_t reader_data_t::try_expand_wildcard(wcstring wc, size_t position,
                                                             wcstring *result) {
    // Hacky from #8593: only expand if there are wildcards in the "current path component."
    // Find the "current path component" by looking for an unescaped slash before and after
    // our position.
    // This is quite naive; for example it mishandles brackets.
    auto is_path_sep = [&](size_t where) {
        return wc.at(where) == L'/' && count_preceding_backslashes(wc, where) % 2 == 0;
    };
    size_t comp_start = position;
    while (comp_start > 0 && !is_path_sep(comp_start - 1)) {
        comp_start--;
    }
    size_t comp_end = position;
    while (comp_end < wc.size() && !is_path_sep(comp_end)) {
        comp_end++;
    }
    if (!wildcard_has(wc.c_str() + comp_start, comp_end - comp_start)) {
        return expand_result_t::wildcard_no_match;
    }

    result->clear();

    // Have a low limit on the number of matches, otherwise we will overwhelm the command line.
    operation_context_t ctx{nullptr, vars(), parser().cancel_checker(),
                            TAB_COMPLETE_WILDCARD_MAX_EXPANSION};
    // We do wildcards only.
    expand_flags_t flags{expand_flag::skip_cmdsubst, expand_flag::skip_variables,
                         expand_flag::preserve_home_tildes};
    completion_list_t expanded;
    expand_result_t ret = expand_string(std::move(wc), &expanded, flags, ctx);
    if (ret != expand_result_t::ok) return ret.result;

    // Insert all matches (escaped) and a trailing space.
    wcstring joined;
    for (const auto &match : expanded) {
        if (match.flags & COMPLETE_DONT_ESCAPE) {
            joined.append(match.completion);
        } else {
            complete_flags_t tildeflag =
                (match.flags & COMPLETE_DONT_ESCAPE_TILDES) ? ESCAPE_NO_TILDE : 0;
            joined.append(escape_string(match.completion, ESCAPE_NO_QUOTED | tildeflag));
        }
        joined.push_back(L' ');
    }

    *result = std::move(joined);
    return expand_result_t::ok;
}

void reader_data_t::compute_and_apply_completions(readline_cmd_t c, readline_loop_state_t &rls) {
    assert((c == readline_cmd_t::complete || c == readline_cmd_t::complete_and_search) &&
           "Invalid command");
    editable_line_t *el = &command_line;

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
    size_t position_in_cmdsub = el->position() - (cmdsub_begin - buff);

    // Figure out the extent of the token within the command substitution. Note we
    // pass cmdsub_begin here, not buff.
    const wchar_t *token_begin, *token_end;
    parse_util_token_extent(cmdsub_begin, position_in_cmdsub, &token_begin, &token_end, nullptr,
                            nullptr);
    size_t position_in_token = position_in_cmdsub - (token_begin - cmdsub_begin);

    // Hack: the token may extend past the end of the command substitution, e.g. in
    // (echo foo) the last token is 'foo)'. Don't let that happen.
    if (token_end > cmdsub_end) token_end = cmdsub_end;

    // Check if we have a wildcard within this string; if so we first attempt to expand the
    // wildcard; if that succeeds we don't then apply user completions (#8593).
    wcstring wc_expanded;
    switch (
        try_expand_wildcard(wcstring(token_begin, token_end), position_in_token, &wc_expanded)) {
        case expand_result_t::error:
            // This may come about if we exceeded the max number of matches.
            // Return "success" to suppress normal completions.
            flash();
            return;
        case expand_result_t::wildcard_no_match:
            break;
        case expand_result_t::cancel:
            // e.g. the user hit control-C. Suppress normal completions.
            return;
        case expand_result_t::ok:
            rls.comp.clear();
            rls.complete_did_insert = false;
            size_t tok_off = static_cast<size_t>(token_begin - buff);
            size_t tok_len = static_cast<size_t>(token_end - token_begin);
            push_edit(el, edit_t{tok_off, tok_len, std::move(wc_expanded)});
            return;
    }

    // Construct a copy of the string from the beginning of the command substitution
    // up to the end of the token we're completing.
    const wcstring buffcpy = wcstring(cmdsub_begin, token_end);

    // Ensure that `commandline` inside the completions gets the current state.
    update_commandline_state();

    rls.comp = complete(buffcpy, completion_request_options_t::normal(), parser_ref->context());

    // User-supplied completions may have changed the commandline - prevent buffer
    // overflow.
    if (token_begin > buff + el->text().size()) token_begin = buff + el->text().size();
    if (token_end > buff + el->text().size()) token_end = buff + el->text().size();

    // Munge our completions.
    completions_sort_and_prioritize(&rls.comp);

    // Record our cycle_command_line.
    cycle_command_line = el->text();
    cycle_cursor_pos = token_end - buff;

    rls.complete_did_insert = handle_completions(rls.comp, token_begin - buff, token_end - buff);

    // Show the search field if requested and if we printed a list of completions.
    if (c == readline_cmd_t::complete_and_search && !rls.complete_did_insert && !pager.empty()) {
        pager.set_search_field_shown(true);
        select_completion_in_direction(selection_motion_t::next);
    }
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
    parser.assert_can_execute();
    reader_config_t conf;
    conf.complete_ok = true;
    conf.highlight_ok = true;
    conf.syntax_check_ok = true;
    conf.autosuggest_ok = check_autosuggestion_enabled(parser.vars());
    conf.expand_abbrev_ok = true;
    conf.event = L"fish_prompt";

    if (parser.is_breakpoint() && function_exists(DEBUG_PROMPT_FUNCTION_NAME, parser)) {
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

        if (maybe_t<wcstring> mcmd = data->readline(0)) {
            const wcstring command = mcmd.acquire();
            if (command.empty()) {
                continue;
            }

            data->update_buff_pos(&data->command_line, 0);
            data->command_line.clear();
            data->command_line_changed(&data->command_line);
            event_fire_generic(parser, L"fish_preexec", {command});
            auto eval_res = reader_run_command(parser, command);
            signal_clear_cancel();
            if (!eval_res.no_status) {
                ++status_count;
            }

            // If the command requested an exit, then process it now and clear it.
            data->exit_loop_requested |= parser.libdata().exit_current_script;
            parser.libdata().exit_current_script = false;

            event_fire_generic(parser, L"fish_postexec", {command});
            // Allow any pending history items to be returned in the history array.
            if (data->history) {
                (*data->history)->resolve_pending();
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

            // Apply any command line update from this command or fish_postexec, etc.
            // See #8807.
            data->apply_commandline_state_changes();
        }
    }
    reader_pop();

    // If we got SIGHUP, ensure the tty is redirected.
    if (s_sighup_received) {
        // If we are the top-level reader, then we translate SIGHUP into exit_forced.
        redirect_tty_after_sighup();
    }

    // If we are the last reader, then kill remaining jobs before exiting.
    if (reader_data_stack.empty()) {
        // Send the exit event and then commit to not executing any more fish script.
        s_exit_state = exit_state_t::running_handlers;
        event_fire_generic(parser, L"fish_exit");
        s_exit_state = exit_state_t::finished_handlers;
        hup_jobs(parser.jobs());
    }

    return 0;
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
    auto tok = new_tokenizer(text.c_str(), TOK_ACCEPT_UNFINISHED | TOK_SHOW_COMMENTS);
    bool is_comment = false;
    while (auto token = tok->next()) {
        is_comment = token->type_ == token_type_t::comment;
    }
    return is_comment;
}

/// \return true if an event is a normal character that should be inserted into the buffer.
static bool event_is_normal_char(const char_event_t &evt) {
    if (!evt.is_char()) return false;
    auto c = evt.get_char();
    return !fish_reserved_codepoint(c) && c > 31 && c != 127;
}

/// Run a sequence of commands from an input binding.
void reader_data_t::run_input_command_scripts(const std::vector<wcstring> &cmds) {
    auto last_statuses = parser().get_last_statuses();
    for (const wcstring &cmd : cmds) {
        update_commandline_state();
        parser().eval(cmd, io_chain_t{});
        apply_commandline_state_changes();
    }
    parser().set_last_statuses(std::move(last_statuses));

    // Restore tty to shell modes.
    // Some input commands will take over the tty - see #2114 for an example where vim is invoked
    // from a key binding. However we do NOT want to invoke term_donate(), because that will enable
    // ECHO mode, causing a race between new input and restoring the mode (#7770). So we leave the
    // tty alone, run the commands in shell mode, and then restore shell modes.
    int res;
    do {
        res = tcsetattr(STDIN_FILENO, TCSANOW, &shell_modes);
    } while (res < 0 && errno == EINTR);
    if (res < 0) {
        wperror(L"tcsetattr");
    }
    termsize_invalidate_tty();
}

/// Read normal characters, inserting them into the command line.
/// \return the next unhandled event.
maybe_t<char_event_t> reader_data_t::read_normal_chars(readline_loop_state_t &rls) {
    maybe_t<char_event_t> event_needing_handling{};
    wcstring accumulated_chars;
    size_t limit = std::min(rls.nchars - command_line.size(), READAHEAD_MAX);

    using command_handler_t = inputter_t::command_handler_t;
    command_handler_t normal_handler = [this](const std::vector<wcstring> &cmds) {
        this->run_input_command_scripts(cmds);
    };
    command_handler_t empty_handler = {};

    // We repaint our prompt if fstat reports the tty as having changed.
    // But don't react to tty changes that we initiated, because of commands or
    // on-variable events (e.g. for fish_bind_mode). See #3481.
    uint64_t last_exec_count = exec_count();
    while (accumulated_chars.size() < limit) {
        bool allow_commands = (accumulated_chars.empty());
        auto evt = inputter.read_char(allow_commands ? normal_handler : empty_handler);
        if (!event_is_normal_char(evt) || !poll_fd_readable(conf.in)) {
            event_needing_handling = std::move(evt);
            break;
        } else if (evt.input_style == char_input_style_t::notfirst && accumulated_chars.empty() &&
                   active_edit_line()->position() == 0) {
            // The cursor is at the beginning and nothing is accumulated, so skip this character.
            continue;
        } else {
            accumulated_chars.push_back(evt.get_char());
        }

        if (last_exec_count != exec_count()) {
            last_exec_count = exec_count();
            screen.save_status();
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

    if (last_exec_count != exec_count()) {
        last_exec_count = exec_count();
        screen.save_status();
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
            if (!command_line.empty()) {
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
                screen.reset_abandoning_line(termsize_last().width - command_line.size());

                // Post fish_cancel.
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
                (rls.last_cmd == rl::complete || rls.last_cmd == rl::complete_and_search)) {
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
                screen.reset_line(true /* redraw prompt */);
                if (this->is_repaint_needed()) this->layout_and_repaint(L"mode");
                parser().libdata().is_repaint = false;
                break;
            }
            // Else we repaint as normal.
            __fallthrough__
        }
        case rl::force_repaint:
        case rl::repaint: {
            parser().libdata().is_repaint = true;
            exec_prompt();
            screen.reset_line(true /* redraw prompt */);
            this->layout_and_repaint(L"readline");
            force_exec_prompt_and_repaint = false;
            parser().libdata().is_repaint = false;
            break;
        }
        case rl::complete:
        case rl::complete_and_search: {
            if (!conf.complete_ok) break;
            if (is_navigating_pager_contents() ||
                (!rls.comp.empty() && !rls.complete_did_insert && rls.last_cmd == rl::complete)) {
                // The user typed complete more than once in a row. If we are not yet fully
                // disclosed, then become so; otherwise cycle through our available completions.
                if (current_page_rendering.remaining_to_disclose > 0) {
                    pager.set_fully_disclosed();
                } else {
                    select_completion_in_direction(c == rl::complete ? selection_motion_t::next
                                                                     : selection_motion_t::prev);
                }
            } else {
                // Either the user hit tab only once, or we had no visible completion list.
                compute_and_apply_completions(c, rls);
            }
            break;
        }
        case rl::pager_toggle_search: {
            if (history_pager_active) {
                fill_history_pager(false, history_search_direction_t::Forward);
                break;
            }
            if (!pager.empty()) {
                // Toggle search, and begin navigating if we are now searching.
                bool sfs = pager.is_search_field_shown();
                pager.set_search_field_shown(!sfs);
                pager.set_fully_disclosed();
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
            if (el->position() == 0) {
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
        case rl::kill_whole_line:  // We match the emacs behavior here: "kills the entire line
                                   // including the following newline".
        case rl::kill_inner_line:  // Do not kill the following newline
        {
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
            for (;; end++) {
                if (buff[end] == L'\0') {
                    if (c == rl::kill_whole_line && begin > 0) {
                        // We are on the last line. Delete the newline in the beginning to clear
                        // this line.
                        begin--;
                    }
                    break;
                }
                if (buff[end] == L'\n') {
                    if (c == rl::kill_whole_line) {
                        end++;
                    }
                    break;
                }
            }

            assert(end >= begin);

            if (end > begin) {
                kill(el, begin, end - begin, KILL_APPEND, rls.last_cmd != c);
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
        case rl::exit: {
            // This is by definition a successful exit, override the status
            parser().set_last_statuses(statuses_t::just(STATUS_CMD_OK));
            exit_loop_requested = true;
            check_exit_loop_maybe_warning(this);
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
        case rl::execute: {
            if (!this->handle_execute(rls)) {
                event_fire_generic(parser(), L"fish_posterror", {command_line.text()});
                screen.reset_abandoning_line(termsize_last().width);
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
                        history_search.reset_to_mode(token, **history,
                                                     reader_history_search_t::token, begin - buff);
                    } else {
                        // No current token, refuse to do a token search.
                        history_search.reset();
                    }
                } else {
                    // Searching by line.
                    history_search.reset_to_mode(el->text(), **history, mode, 0);

                    // Skip the autosuggestion in the history unless it was truncated.
                    const wcstring &suggest = autosuggestion.text;
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
                        ? history_search_direction_t::Backward
                        : history_search_direction_t::Forward;
                bool found = history_search.move_in_direction(dir);

                // Signal that we've found nothing
                if (!found) flash();

                if (!found && !was_active_before) {
                    history_search.reset();
                    break;
                }
                if (found ||
                    (dir == history_search_direction_t::Forward && history_search.is_at_end())) {
                    update_command_line_from_history_search();
                }
            }
            break;
        }
        case rl::history_pager: {
            if (history_pager_active) {
                fill_history_pager(false, history_search_direction_t::Backward);
                break;
            }

            // Record our cycle_command_line.
            cycle_command_line = command_line.text();
            cycle_cursor_pos = command_line.position();

            this->history_pager_active = true;
            this->history_pager_history_index_start = 0;
            this->history_pager_history_index_end = 0;
            // Update the pager data.
            pager.set_search_field_shown(true);
            pager.set_prefix(MB_CUR_MAX > 1 ? L"► " : L"> ", false /* highlight */);
            // Update the search field, which triggers the actual history search.
            insert_string(&pager.search_field_line, command_line.text());
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
                (c == rl::backward_kill_bigword ? move_word_style_t::move_word_style_whitespace
                 : c == rl::backward_kill_path_component
                     ? move_word_style_t::move_word_style_path_components
                     : move_word_style_t::move_word_style_punctuation);
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
            auto move_style = (c == rl::kill_word) ? move_word_style_t::move_word_style_punctuation
                                                   : move_word_style_t::move_word_style_whitespace;
            move_word(active_edit_line(), MOVE_DIR_RIGHT, true /* erase */, move_style,
                      rls.last_cmd != c /* same kill item if same movement */);
            break;
        }
        case rl::backward_word:
        case rl::backward_bigword:
        case rl::prevd_or_backward_word: {
            if (c == rl::prevd_or_backward_word && command_line.empty()) {
                auto last_statuses = parser().get_last_statuses();
                (void)parser().eval(L"prevd", io_chain_t{});
                parser().set_last_statuses(std::move(last_statuses));
                force_exec_prompt_and_repaint = true;
                inputter.queue_char(readline_cmd_t::repaint);
                break;
            }

            auto move_style = (c != rl::backward_bigword)
                                  ? move_word_style_t::move_word_style_punctuation
                                  : move_word_style_t::move_word_style_whitespace;
            move_word(active_edit_line(), MOVE_DIR_LEFT, false /* do not erase */, move_style,
                      false);
            break;
        }
        case rl::forward_word:
        case rl::forward_bigword:
        case rl::nextd_or_forward_word: {
            if (c == rl::nextd_or_forward_word && command_line.empty()) {
                auto last_statuses = parser().get_last_statuses();
                (void)parser().eval(L"nextd", io_chain_t{});
                parser().set_last_statuses(std::move(last_statuses));
                force_exec_prompt_and_repaint = true;
                inputter.queue_char(readline_cmd_t::repaint);
                break;
            }

            auto move_style = (c != rl::forward_bigword)
                                  ? move_word_style_t::move_word_style_punctuation
                                  : move_word_style_t::move_word_style_whitespace;
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

                int line_count = parse_util_lineno(el->text(), el->size()) - 1;

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
                    chr = std::tolower(chr);
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
            if (auto selection = this->get_selection()) {
                size_t start = selection->start;
                size_t len = selection->length;

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
                        chr = std::tolower(chr);
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
            move_word(el, MOVE_DIR_RIGHT, false, move_word_style_t::move_word_style_punctuation,
                      false);
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
            selection->stop =
                pos + (cursor_selection_mode == cursor_selection_mode_t::inclusive ? 1 : 0);
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
            if (auto selection = this->get_selection()) {
                kill(&command_line, selection->start, selection->length, KILL_APPEND, newv);
            }
            break;
        }
        case rl::insert_line_over: {
            editable_line_t *el = active_edit_line();
            while (el->position() > 0 && el->text().at(el->position() - 1) != L'\n') {
                update_buff_pos(el, el->position() - 1);
            }
            insert_char(el, L'\n');
            update_buff_pos(el, el->position() - 1);
            break;
        }
        case rl::insert_line_under: {
            editable_line_t *el = active_edit_line();
            if (el->position() < el->size()) {
                const wchar_t *buff = el->text().c_str();
                while (buff[el->position()] && buff[el->position()] != L'\n') {
                    update_buff_pos(el, el->position() + 1);
                }
            }
            insert_char(el, L'\n');
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
            if (expand_abbreviation_at_cursor(1)) {
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
                maybe_refilter_pager(el);
            } else {
                flash();
            }
            break;
        }
        case rl::begin_undo_group: {
            editable_line_t *el = active_edit_line();
            el->begin_edit_group();
            break;
        }
        case rl::end_undo_group: {
            editable_line_t *el = active_edit_line();
            el->end_edit_group();
            break;
        }
        case rl::disable_mouse_tracking: {
            outputter_t &outp = outputter_t::stdoutput();
            outp.writestr(L"\x1B[?1000l");
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

void reader_data_t::add_to_history() {
    if (!history || conf.in_silent_mode) {
        return;
    }

    // Historical behavior is to trim trailing spaces, unless escape (#7661).
    wcstring text = command_line.text();
    while (!text.empty() && text.back() == L' ' &&
           count_preceding_backslashes(text, text.size() - 1) % 2 == 0) {
        text.pop_back();
    }

    // Remove ephemeral items - even if the text is empty.
    (*history)->remove_ephemeral_items();

    if (!text.empty()) {
        // Mark this item as ephemeral if there is a leading space (#615).
        history_persistence_mode_t mode;
        if (text.front() == L' ') {
            // Leading spaces are ephemeral (#615).
            mode = history_persistence_mode_t::Ephemeral;
        } else if (in_private_mode(this->vars())) {
            // Private mode means in-memory only.
            mode = history_persistence_mode_t::Memory;
        } else {
            mode = history_persistence_mode_t::Disk;
        }
        (*history)->add_pending_with_file_detection(text.c_str(), *this->vars().snapshot(), mode);
    }
}

parser_test_error_bits_t reader_data_t::expand_for_execute() {
    // Expand abbreviations at the cursor.
    // The first expansion is "user visible" and enters into history.
    editable_line_t *el = &command_line;
    parser_test_error_bits_t test_res = 0;

    // Syntax check before expanding abbreviations. We could consider relaxing this: a string may be
    // syntactically invalid but become valid after expanding abbreviations.
    if (conf.syntax_check_ok) {
        test_res = reader_shell_test(parser(), el->text());
        if (test_res & PARSER_TEST_ERROR) return test_res;
    }

    // Exec abbreviations at the cursor.
    // Note we want to expand abbreviations even if incomplete.
    if (expand_abbreviation_at_cursor(0)) {
        // Trigger syntax highlighting as we are likely about to execute this command.
        this->super_highlight_me_plenty();
        if (conf.syntax_check_ok) {
            test_res = reader_shell_test(parser(), el->text());
        }
    }
    return test_res;
}

bool reader_data_t::handle_execute(readline_loop_state_t &rls) {
    // Evaluate. If the current command is unfinished, or if the charater is escaped
    // using a backslash, insert a newline.
    // If the user hits return while navigating the pager, it only clears the pager.
    if (is_navigating_pager_contents()) {
        clear_pager();
        return true;
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
        if (is_backslashed(el->text(), el->position()) && iswspace(el->text().at(el->position()))) {
            continue_on_next_line = true;
            // Check if the end of the line is backslashed (issue #4467).
        } else if (is_backslashed(el->text(), el->size()) && !text_ends_in_comment(el->text())) {
            // Move the cursor to the end of the line.
            el->set_position(el->size());
            continue_on_next_line = true;
        }
    }
    // If the conditions are met, insert a new line at the position of the cursor.
    if (continue_on_next_line) {
        insert_char(el, L'\n');
        return true;
    }

    // Expand the command line in preparation for execution.
    // to_exec is the command to execute; the command line itself has the command for history.
    parser_test_error_bits_t test_res = this->expand_for_execute();
    if (test_res & PARSER_TEST_ERROR) {
        return false;
    } else if (test_res & PARSER_TEST_INCOMPLETE) {
        insert_char(el, L'\n');
        return true;
    }
    assert(test_res == 0);

    this->add_to_history();
    rls.finished = true;
    update_buff_pos(&command_line, command_line.size());
    return true;
}

maybe_t<wcstring> reader_data_t::readline(int nchars_or_0) {
    using rl = readline_cmd_t;
    readline_loop_state_t rls{};

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

    // It may happen that a command we ran when job control was disabled nevertheless stole the tty
    // from us. In that case when we read from our fd, it will trigger SIGTTIN. So just
    // unconditionally reclaim the tty. See #9181.
    (void)tcsetpgrp(conf.in, getpgrp());

    // Get the current terminal modes. These will be restored when the function returns.
    struct termios old_modes {};
    if (tcgetattr(conf.in, &old_modes) == -1 && errno == EIO) redirect_tty_output();

    // Set the new modes.
    if (tcsetattr(conf.in, TCSANOW, &shell_modes) == -1) {
        int err = errno;
        if (err == EIO) redirect_tty_output();

        // This check is required to work around certain issues with fish's approach to
        // terminal control when launching interactive processes while in non-interactive
        // mode. See #4178 for one such example.
        if (err != ENOTTY || is_interactive_session()) {
            wperror(L"tcsetattr");
        }
    }

    // HACK: Don't abandon line for the first prompt, because
    // if we're started with the terminal it might not have settled,
    // so the width is quite likely to be in flight.
    //
    // This means that `printf %s foo; fish` will overwrite the `foo`,
    // but that's a smaller problem than having the omitted newline char
    // appear constantly.
    //
    // I can't see a good way around this.
    if (!first_prompt) {
        screen.reset_abandoning_line(termsize_last().width);
    }
    first_prompt = false;

    if (!conf.event.empty()) {
        event_fire_generic(parser(), conf.event);
    }
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

    while (!rls.finished && !check_exit_loop_maybe_warning(this)) {
        if (reset_loop_state) {
            reset_loop_state = false;
            rls.last_cmd = none();
            rls.complete_did_insert = false;
        }
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

        // If we ran `exit` anywhere, exit.
        exit_loop_requested |= parser().libdata().exit_current_script;
        parser().libdata().exit_current_script = false;
        if (exit_loop_requested) continue;

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
            if (!history_search.active() &&
                command_ends_paging(readline_cmd, focused_on_search_field)) {
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
                command_line_has_transient_edit = false;
            }

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
    if (this->is_repaint_needed() || conf.in != STDIN_FILENO)
        this->layout_and_repaint(L"prepare to execute");

    // Finish syntax highlighting (but do not wait forever).
    if (rls.finished) {
        finish_highlighting_before_exec();
    }

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
        clear_pager();
    }

    if (s_exit_state != exit_state_t::finished_handlers) {
        // The order of the two conditions below is important. Try to restore the mode
        // in all cases, but only complain if interactive.
        if (tcsetattr(conf.in, TCSANOW, &old_modes) == -1 && is_interactive_session()) {
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

maybe_t<wcstring> reader_readline(int nchars) {
    auto *data = current_data();
    // Apply any outstanding commandline changes (#8633).
    data->apply_commandline_state_changes();
    return data->readline(nchars);
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
        data->inputter.queue_char(readline_cmd_t::repaint);
    }
}

void reader_handle_command(readline_cmd_t cmd) {
    if (reader_data_t *data = current_data_or_null()) {
        readline_loop_state_t rls{};
        data->handle_readline_command(cmd, rls);
    }
}

void reader_queue_ch(const char_event_t &ch) {
    if (reader_data_t *data = current_data_or_null()) {
        data->inputter.queue_char(ch);
    }
}

/// Read non-interactively.  Read input from stdin without displaying the prompt, using syntax
/// highlighting. This is used for reading scripts and init files.
/// The file is not closed.
static int read_ni(parser_t &parser, int fd, const io_chain_t &io) {
    struct stat buf {};
    if (fstat(fd, &buf) == -1) {
        int err = errno;
        FLOGF(error, _(L"Unable to read input file: %s"), strerror(err));
        return 1;
    }

    /* FreeBSD allows read() on directories. Error explicitly in that case. */
    // XXX: This can be triggered spuriously, so we'll not do that for stdin.
    // This can be seen e.g. with node's "spawn" api.
    if (fd != STDIN_FILENO && buf.st_mode & S_IFDIR) {
        FLOGF(error, _(L"Unable to read input file: %s"), strerror(EISDIR));
        return 1;
    }

    // Read all data into a std::string.
    std::string fd_contents;
    fd_contents.reserve(buf.st_size);
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
                return 1;
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
    auto errors = new_parse_error_list();
    auto ast = ast_parse(str, parse_flag_none, &*errors);
    bool errored = ast->errored();
    if (!errored) {
        errored = parse_util_detect_errors(*ast, str, &*errors);
    }
    if (!errored) {
        // Construct a parsed source ref.
        // Be careful to transfer ownership, this could be a very large string.
        auto ps = new_parsed_source_ref(str, *ast);
        parser.eval_parsed_source(*ps, io);
        return 0;
    } else {
        wcstring sb;
        parser.get_backtrace(str, *errors, sb);
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
