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
#include <string.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#include <algorithm>
#include <atomic>
#include <csignal>
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
#include "function.h"
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
#include "util.h"
#include "wutil.h"  // IWYU pragma: keep

// Name of the variable that tells how long it took, in milliseconds, for the previous
// interactive command to complete.
#define ENV_cmd_duration L"cmd_duration"

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
#define READAHEAD_MAX 256

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
static std::atomic<unsigned int> s_generation_count;

/// This pthreads generation count is set when an autosuggestion background thread starts up, so it
/// can easily check if the work it is doing is no longer useful.
static pthread_key_t generation_count_key;

/// Helper to get the generation count
static unsigned int read_generation_count() {
    return s_generation_count.load(std::memory_order_relaxed);
}

static void set_command_line_and_position(editable_line_t *el, const wcstring &new_str, size_t pos);

void editable_line_t::insert_string(const wcstring &str, size_t start, size_t len) {
    // Clamp the range to something valid.
    size_t string_length = str.size();
    start = mini(start, string_length);      //!OCLINT(parameter reassignment)
    len = mini(len, string_length - start);  //!OCLINT(parameter reassignment)
    this->text.insert(this->position, str, start, len);
    this->position += len;
}

namespace {

/// Encapsulation of the reader's history search functionality.
class reader_history_search_t {
   public:
    enum mode_t {
        inactive,  // no search
        line,      // searching by line
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
        if (mode_ == line) {
            add_if_new(std::move(text));
        } else if (mode_ == token) {
            const wcstring &needle = search_string();
            tokenizer_t tok(text.c_str(), TOK_ACCEPT_UNFINISHED);
            tok_t token;

            std::vector<wcstring> local_tokens;
            while (tok.next(&token)) {
                if (token.type != TOK_STRING) continue;
                wcstring text = tok.text_of(token);
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
        search_ =
            history_search_t(*hist, text, HISTORY_SEARCH_TYPE_CONTAINS, history_search_no_dedup);
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

}  // namespace

/// A struct describing the state of the interactive reader. These states can be stacked, in case
/// reader_readline() calls are nested. This happens when the 'read' builtin is used.
class reader_data_t {
   public:
    /// String containing the whole current commandline.
    editable_line_t command_line;
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
    /// Function for tab completion.
    complete_function_t complete_func{nullptr};
    /// Function for syntax highlighting.
    highlight_function_t highlight_func{nullptr};
    /// Function for testing if the string can be returned.
    test_function_t test_func{nullptr};
    /// When this is true, the reader will exit.
    bool end_loop{false};
    /// If this is true, exit reader even if there are running jobs. This happens if we press e.g.
    /// ^D twice.
    bool prev_end_loop{false};
    /// The current contents of the top item in the kill ring.
    wcstring kill_item;
    /// Pointer to previous reader_data.
    reader_data_t *next{nullptr};
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
    editable_line_t *active_edit_line() {
        if (this->is_navigating_pager_contents() && this->pager.is_search_field_shown()) {
            return &this->pager.search_field_line;
        }
        return &this->command_line;
    }

    /// Do what we need to do whenever our command line changes.
    void command_line_changed(const editable_line_t *el);

    /// Do what we need to do whenever our pager selection.
    void pager_selection_changed();

    /// Expand abbreviations at the current cursor position, minus backtrack_amt.
    bool expand_abbreviation_as_necessary(size_t cursor_backtrack) const;

    /// Constructor
    reader_data_t(history_t *hist) : history(hist) {}
};

/// Sets the command line contents, without clearing the pager.
static void reader_set_buffer_maintaining_pager(const wcstring &b, size_t pos);

/// Clears the pager.
static void clear_pager();

/// The stack of current interactive reading contexts.
static std::vector<std::unique_ptr<reader_data_t>> reader_data_stack;

/// Access the top level reader data.
static reader_data_t *current_data_or_null() {
    return reader_data_stack.empty() ? nullptr : reader_data_stack.back().get();
}
static reader_data_t *current_data() {
    assert(!reader_data_stack.empty() && "no current reader");
    return reader_data_stack.back().get();
}

/// This flag is set to true when fish is interactively reading from stdin. It changes how a ^C is
/// handled by the fish interrupt handler.
static volatile sig_atomic_t is_interactive_read;

/// Flag for ending non-interactive shell.
static int end_loop = 0;

/// The stack containing names of files that are being parsed.
static std::stack<const wchar_t *, std::vector<const wchar_t *>> current_filename;

/// This variable is set to true by the signal handler when ^C is pressed.
static volatile sig_atomic_t interrupted = 0;

// Prototypes for a bunch of functions defined later on.
static bool is_backslashed(const wcstring &str, size_t pos);
static wchar_t unescaped_quote(const wcstring &str, size_t pos);
static bool jump(jump_direction_t dir, jump_precision_t precision, editable_line_t *el,
                 wchar_t target);

/// Mode on startup, which we restore on exit.
static struct termios terminal_mode_on_startup;

/// Mode we use to execute programs.
static struct termios tty_modes_for_external_cmds;

static void reader_super_highlight_me_plenty(int highlight_pos_adjust = 0, bool no_io = false);

/// Variable to keep track of forced exits - see \c reader_exit_forced();
static bool exit_forced;

/// Give up control of terminal.
static void term_donate() {
    set_color(rgb_color_t::normal(), rgb_color_t::normal());

    while (1) {
        if (tcsetattr(STDIN_FILENO, TCSANOW, &tty_modes_for_external_cmds) == -1) {
            if (errno == EIO) redirect_tty_output();
            if (errno != EINTR) {
                debug(1, _(L"Could not set terminal mode for new job"));
                wperror(L"tcsetattr");
                break;
            }
        } else
            break;
    }
}

/// Update the cursor position.
static void update_buff_pos(editable_line_t *el, size_t buff_pos) {
    el->position = buff_pos;
    reader_data_t *data = current_data();
    if (el == &data->command_line && data->sel_active) {
        if (data->sel_begin_pos <= buff_pos) {
            data->sel_start_pos = data->sel_begin_pos;
            data->sel_stop_pos = buff_pos;
        } else {
            data->sel_start_pos = buff_pos;
            data->sel_stop_pos = data->sel_begin_pos;
        }
    }
}

/// Grab control of terminal.
static void term_steal() {
    while (1) {
        if (tcsetattr(STDIN_FILENO, TCSANOW, &shell_modes) == -1) {
            if (errno == EIO) redirect_tty_output();
            if (errno != EINTR) {
                debug(1, _(L"Could not set terminal mode for shell"));
                perror("tcsetattr");
                break;
            }
        } else
            break;
    }

    invalidate_termsize();
}

bool reader_exit_forced() { return exit_forced; }

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
        const wchar_t *begin = NULL, *cmd = cmdline.c_str();
        parse_util_token_extent(cmd, cmdline.size() - 1, &begin, NULL, NULL, NULL);
        bool last_token_contains_uppercase = false;
        if (begin) {
            const wchar_t *end = begin + wcslen(begin);
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

/// Repaint the entire commandline. This means reset and clear the commandline, write the prompt,
/// perform syntax highlighting, write the commandline and move the cursor.
static void reader_repaint() {
    reader_data_t *data = current_data();
    editable_line_t *cmd_line = &data->command_line;
    // Update the indentation.
    data->indents = parse_util_compute_indents(cmd_line->text);

    wcstring full_line;
    if (data->silent) {
        full_line = wcstring(cmd_line->text.length(), obfuscation_read_char);
    } else {
        // Combine the command and autosuggestion into one string.
        full_line = combine_command_and_autosuggestion(cmd_line->text, data->autosuggestion);
    }

    size_t len = full_line.size();
    if (len < 1) len = 1;

    std::vector<highlight_spec_t> colors = data->colors;
    colors.resize(len, highlight_spec_autosuggestion);

    if (data->sel_active) {
        highlight_spec_t selection_color = highlight_make_background(highlight_spec_selection);
        for (size_t i = data->sel_start_pos; i < std::min(len, data->sel_stop_pos); i++) {
            colors[i] = selection_color;
        }
    }

    std::vector<int> indents = data->indents;
    indents.resize(len);

    // Re-render our completions page if necessary. Limit the term size of the pager to the true
    // term size, minus the number of lines consumed by our string. (Note this doesn't yet consider
    // wrapping).
    int full_line_count = 1 + std::count(full_line.begin(), full_line.end(), '\n');
    data->pager.set_term_size(maxi(1, common_get_width()),
                              maxi(1, common_get_height() - full_line_count));
    data->pager.update_rendering(&data->current_page_rendering);

    bool focused_on_pager = data->active_edit_line() == &data->pager.search_field_line;
    size_t cursor_position = focused_on_pager ? data->pager.cursor_position() : cmd_line->position;

    s_write(&data->screen, data->left_prompt_buff, data->right_prompt_buff, full_line,
            cmd_line->size(), &colors[0], &indents[0], cursor_position,
            data->current_page_rendering, focused_on_pager);

    data->repaint_needed = false;
}

/// Internal helper function for handling killing parts of text.
static void reader_kill(editable_line_t *el, size_t begin_idx, size_t length, int mode, int newv) {
    reader_data_t *data = current_data();
    const wchar_t *begin = el->text.c_str() + begin_idx;
    if (newv) {
        data->kill_item = wcstring(begin, length);
        kill_add(data->kill_item);
    } else {
        wcstring old = data->kill_item;
        if (mode == KILL_APPEND) {
            data->kill_item.append(begin, length);
        } else {
            data->kill_item = wcstring(begin, length);
            data->kill_item.append(old);
        }

        kill_replace(old, data->kill_item);
    }

    if (el->position > begin_idx) {
        // Move the buff position back by the number of characters we deleted, but don't go past
        // buff_pos.
        size_t backtrack = mini(el->position - begin_idx, length);
        update_buff_pos(el, el->position - backtrack);
    }

    el->text.erase(begin_idx, length);
    data->command_line_changed(el);

    reader_super_highlight_me_plenty();
    reader_repaint();
}

// This is called from a signal handler!
void reader_handle_sigint() {
    if (!is_interactive_read) {
        parser_t::skip_all_blocks();
    }

    interrupted = 1;
}

const wchar_t *reader_current_filename() {
    ASSERT_IS_MAIN_THREAD();
    return current_filename.empty() ? NULL : current_filename.top();
}

void reader_push_current_filename(const wchar_t *fn) {
    ASSERT_IS_MAIN_THREAD();
    current_filename.push(intern(fn));
}

void reader_pop_current_filename() {
    ASSERT_IS_MAIN_THREAD();
    current_filename.pop();
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
        s_generation_count++;
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

    if (completion == NULL) {
        new_cmd_line = this->cycle_command_line;
    } else {
        new_cmd_line =
            completion_apply_to_command_line(completion->completion, completion->flags,
                                             this->cycle_command_line, &cursor_pos, false);
    }
    reader_set_buffer_maintaining_pager(new_cmd_line, cursor_pos);

    // Trigger repaint (see issue #765).
    reader_repaint_needed();
}

/// Expand abbreviations at the given cursor position. Does NOT inspect 'data'.
bool reader_expand_abbreviation_in_command(const wcstring &cmdline, size_t cursor_pos,
                                           wcstring *output) {
    // See if we are at "command position". Get the surrounding command substitution, and get the
    // extent of the first token.
    const wchar_t *const buff = cmdline.c_str();
    const wchar_t *cmdsub_begin = NULL, *cmdsub_end = NULL;
    parse_util_cmdsubst_extent(buff, cursor_pos, &cmdsub_begin, &cmdsub_end);
    assert(cmdsub_begin != NULL && cmdsub_begin >= buff);
    assert(cmdsub_end != NULL && cmdsub_end >= cmdsub_begin);

    // Determine the offset of this command substitution.
    const size_t subcmd_offset = cmdsub_begin - buff;

    const wcstring subcmd = wcstring(cmdsub_begin, cmdsub_end - cmdsub_begin);
    const size_t subcmd_cursor_pos = cursor_pos - subcmd_offset;

    // Parse this subcmd.
    parse_node_tree_t parse_tree;
    parse_tree_from_string(subcmd,
                           parse_flag_continue_after_error | parse_flag_accept_incomplete_tokens,
                           &parse_tree, NULL);

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
    bool result = false;
    if (matching_cmd_node) {
        const wcstring token = matching_cmd_node.get_source(subcmd);
        wcstring abbreviation;
        if (expand_abbreviation(token, &abbreviation)) {
            // There was an abbreviation! Replace the token in the full command. Maintain the
            // relative position of the cursor.
            if (output != NULL) {
                output->assign(cmdline);
                source_range_t r = *matching_cmd_node.source_range();
                output->replace(subcmd_offset + r.start, r.length, abbreviation);
            }
            result = true;
        }
    }
    return result;
}

/// Expand abbreviations at the current cursor position, minus the given  cursor backtrack. This may
/// change the command line but does NOT repaint it. This is to allow the caller to coalesce
/// repaints.
bool reader_data_t::expand_abbreviation_as_necessary(size_t cursor_backtrack) const {
    reader_data_t *data = current_data();
    bool result = false;
    editable_line_t *el = data->active_edit_line();
    if (this->expand_abbreviations && el == &data->command_line) {
        // Try expanding abbreviations.
        wcstring new_cmdline;
        size_t cursor_pos = el->position - mini(el->position, cursor_backtrack);
        if (reader_expand_abbreviation_in_command(el->text, cursor_pos, &new_cmdline)) {
            // We expanded an abbreviation! The cursor moves by the difference in the command line
            // lengths.
            size_t new_buff_pos = el->position + new_cmdline.size() - el->text.size();

            el->text = std::move(new_cmdline);
            update_buff_pos(el, new_buff_pos);
            data->command_line_changed(el);
            result = true;
        }
    }
    return result;
}

void reader_reset_interrupted() { interrupted = 0; }

int reader_interrupted() {
    int res = interrupted;
    if (res) {
        interrupted = 0;
    }
    return res;
}

int reader_reading_interrupted() {
    int res = reader_interrupted();
    reader_data_t *data = current_data_or_null();
    if (res && data && data->exit_on_interrupt) {
        reader_exit(1, 0);
        parser_t::skip_all_blocks();
        // We handled the interrupt ourselves, our caller doesn't need to handle it.
        return 0;
    }
    return res;
}

bool reader_thread_job_is_stale() {
    ASSERT_IS_BACKGROUND_THREAD();
    void *current_count = (void *)(uintptr_t)read_generation_count();
    return current_count != pthread_getspecific(generation_count_key);
}

void reader_write_title(const wcstring &cmd, bool reset_cursor_position) {
    if (!term_supports_setting_title()) return;

    wcstring fish_title_command = DEFAULT_TITLE;
    if (function_exists(L"fish_title")) {
        fish_title_command = L"fish_title";
        if (!cmd.empty()) {
            fish_title_command.append(L" ");
            fish_title_command.append(
                escape_string(cmd, ESCAPE_ALL | ESCAPE_NO_QUOTED | ESCAPE_NO_TILDE));
        }
    }

    wcstring_list_t lst;
    proc_push_interactive(0);
    if (exec_subshell(fish_title_command, lst, false /* ignore exit status */) != -1 &&
        !lst.empty()) {
        fputws(L"\x1B]0;", stdout);
        for (size_t i = 0; i < lst.size(); i++) {
            fputws(lst.at(i).c_str(), stdout);
        }
        ignore_result(write(STDOUT_FILENO, "\a", 1));
    }

    proc_pop_interactive();
    set_color(rgb_color_t::reset(), rgb_color_t::reset());
    if (reset_cursor_position && !lst.empty()) {
        // Put the cursor back at the beginning of the line (issue #2453).
        ignore_result(write(STDOUT_FILENO, "\r", 1));
    }
}

/// Reexecute the prompt command. The output is inserted into data->prompt_buff.
static void exec_prompt() {
    // Clear existing prompts.
    reader_data_t *data = current_data();
    data->left_prompt_buff.clear();
    data->right_prompt_buff.clear();

    // Do not allow the exit status of the prompts to leak through.
    const bool apply_exit_status = false;

    // HACK: Query winsize again because it might have changed.
    // This allows prompts to react to $COLUMNS.
    (void)get_current_winsize();

    // If we have any prompts, they must be run non-interactively.
    if (data->left_prompt.size() || data->right_prompt.size()) {
        proc_push_interactive(0);

        // Prepend any mode indicator to the left prompt (issue #1988).
        if (function_exists(MODE_PROMPT_FUNCTION_NAME)) {
            wcstring_list_t mode_indicator_list;
            exec_subshell(MODE_PROMPT_FUNCTION_NAME, mode_indicator_list, apply_exit_status);
            // We do not support multiple lines in the mode indicator, so just concatenate all of
            // them.
            for (size_t i = 0; i < mode_indicator_list.size(); i++) {
                data->left_prompt_buff += mode_indicator_list.at(i);
            }
        }

        if (!data->left_prompt.empty()) {
            wcstring_list_t prompt_list;
            // Ignore return status.
            exec_subshell(data->left_prompt, prompt_list, apply_exit_status);
            for (size_t i = 0; i < prompt_list.size(); i++) {
                if (i > 0) data->left_prompt_buff += L'\n';
                data->left_prompt_buff += prompt_list.at(i);
            }
        }

        if (!data->right_prompt.empty()) {
            wcstring_list_t prompt_list;
            // Status is ignored.
            exec_subshell(data->right_prompt, prompt_list, apply_exit_status);
            for (size_t i = 0; i < prompt_list.size(); i++) {
                // Right prompt does not support multiple lines, so just concatenate all of them.
                data->right_prompt_buff += prompt_list.at(i);
            }
        }

        proc_pop_interactive();
    }

    // Write the screen title. Do not reset the cursor position: exec_prompt is called when there
    // may still be output on the line from the previous command (#2499) and we need our PROMPT_SP
    // hack to work.
    reader_write_title(L"", false);
}

void reader_init() {
    DIE_ON_FAILURE(pthread_key_create(&generation_count_key, NULL));

    // Ensure this var is present even before an interactive command is run so that if it is used
    // in a function like `fish_prompt` or `fish_right_prompt` it is defined at the time the first
    // prompt is written.
    env_set_one(ENV_cmd_duration, ENV_UNEXPORT, L"0");

    // Save the initial terminal mode.
    tcgetattr(STDIN_FILENO, &terminal_mode_on_startup);

    // Set the mode used for program execution, initialized to the current mode.
    memcpy(&tty_modes_for_external_cmds, &terminal_mode_on_startup,
           sizeof tty_modes_for_external_cmds);
    tty_modes_for_external_cmds.c_iflag &= ~IXON;   // disable flow control
    tty_modes_for_external_cmds.c_iflag &= ~IXOFF;  // disable flow control

    // Set the mode used for the terminal, initialized to the current mode.
    memcpy(&shell_modes, &terminal_mode_on_startup, sizeof shell_modes);

    shell_modes.c_iflag &= ~ICRNL;  // disable mapping CR (\cM) to NL (\cJ)
    shell_modes.c_iflag &= ~INLCR;  // disable mapping NL (\cJ) to CR (\cM)
    shell_modes.c_iflag &= ~IXON;   // disable flow control
    shell_modes.c_iflag &= ~IXOFF;  // disable flow control

    shell_modes.c_lflag &= ~ICANON;  // turn off canonical mode
    shell_modes.c_lflag &= ~ECHO;    // turn off echo mode
    shell_modes.c_lflag &= ~IEXTEN;  // turn off handling of discard and lnext characters

    shell_modes.c_cc[VMIN] = 1;
    shell_modes.c_cc[VTIME] = 0;

    // We don't use term_steal because this can fail if fd 0 isn't associated with a tty and this
    // function is run regardless of whether stdin is tied to a tty. This is harmless in that case.
    // We do it unconditionally because disabling ICRNL mode (see above) needs to be done at the
    // earliest possible moment. Doing it here means it will be done within approximately 1 ms of
    // the start of the shell rather than 250 ms (or more) when reader_interactive_init is
    // eventually called.
    //
    // TODO: Remove this condition when issue #2315 and #1041 are addressed.
    if (is_interactive_session) {
        tcsetattr(STDIN_FILENO, TCSANOW, &shell_modes);
    }

    // We do this not because we actually need the window size but for its side-effect of correctly
    // setting the COLUMNS and LINES env vars.
    get_current_winsize();
}

/// Restore the term mode if we own the terminal. It's important we do this before
/// restore_foreground_process_group, otherwise we won't think we own the terminal.
void restore_term_mode() {
    if (getpid() != tcgetpgrp(STDIN_FILENO)) return;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &terminal_mode_on_startup) == -1 && errno == EIO) {
        redirect_tty_output();
    }
}

void reader_exit(int do_exit, int forced) {
    if (reader_data_t *data = current_data_or_null()) {
        data->end_loop = do_exit;
    }
    end_loop = do_exit;
    if (forced) exit_forced = true;
}

void reader_repaint_needed() {
    if (reader_data_t *data = current_data_or_null()) {
        data->repaint_needed = true;
    }
}

void reader_repaint_if_needed() {
    reader_data_t *data = current_data_or_null();
    if (!data) return;

    bool needs_reset = data->screen_reset_needed;
    bool needs_repaint = needs_reset || data->repaint_needed;

    if (needs_reset) {
        exec_prompt();
        s_reset(&data->screen, screen_reset_current_line_and_prompt);
        data->screen_reset_needed = false;
    }

    if (needs_repaint) {
        reader_repaint();  // reader_repaint clears repaint_needed
    }
}

void reader_react_to_color_change() {
    reader_data_t *data = current_data_or_null();
    if (!data) return;

    if (!data->repaint_needed || !data->screen_reset_needed) {
        data->repaint_needed = true;
        data->screen_reset_needed = true;
        input_common_add_callback(reader_repaint_if_needed);
    }
}

/// Indicates if the given command char ends paging.
static bool command_ends_paging(wchar_t c, bool focused_on_search_field) {
    switch (c) {
        case R_HISTORY_SEARCH_BACKWARD:
        case R_HISTORY_SEARCH_FORWARD:
        case R_HISTORY_TOKEN_SEARCH_BACKWARD:
        case R_HISTORY_TOKEN_SEARCH_FORWARD:
        case R_ACCEPT_AUTOSUGGESTION:
        case R_CANCEL: {
            // These commands always end paging.
            return true;
        }
        case R_COMPLETE:
        case R_COMPLETE_AND_SEARCH:
        case R_BACKWARD_CHAR:
        case R_FORWARD_CHAR:
        case R_UP_LINE:
        case R_DOWN_LINE:
        case R_NULL:
        case R_REPAINT:
        case R_SUPPRESS_AUTOSUGGESTION:
        case R_BEGINNING_OF_HISTORY:
        case R_END_OF_HISTORY: {
            // These commands never end paging.
            return false;
        }
        case R_EXECUTE: {
            // R_EXECUTE does end paging, but only executes if it was not paging. So it's handled
            // specially.
            return false;
        }
        case R_BEGINNING_OF_LINE:
        case R_END_OF_LINE:
        case R_FORWARD_WORD:
        case R_BACKWARD_WORD:
        case R_FORWARD_BIGWORD:
        case R_BACKWARD_BIGWORD:
        case R_DELETE_CHAR:
        case R_BACKWARD_DELETE_CHAR:
        case R_KILL_LINE:
        case R_YANK:
        case R_YANK_POP:
        case R_BACKWARD_KILL_LINE:
        case R_KILL_WHOLE_LINE:
        case R_KILL_WORD:
        case R_KILL_BIGWORD:
        case R_BACKWARD_KILL_WORD:
        case R_BACKWARD_KILL_PATH_COMPONENT:
        case R_BACKWARD_KILL_BIGWORD:
        case R_SELF_INSERT:
        case R_TRANSPOSE_CHARS:
        case R_TRANSPOSE_WORDS:
        case R_UPCASE_WORD:
        case R_DOWNCASE_WORD:
        case R_CAPITALIZE_WORD:
        case R_VI_ARG_DIGIT:
        case R_VI_DELETE_TO:
        case R_BEGINNING_OF_BUFFER:
        case R_END_OF_BUFFER: {
            // These commands operate on the search field if that's where the focus is.
            return !focused_on_search_field;
        }
        default: { return false; }
    }
}

/// Remove the previous character in the character buffer and on the screen using syntax
/// highlighting, etc.
static void remove_backward() {
    reader_data_t *data = current_data();
    editable_line_t *el = data->active_edit_line();

    if (el->position <= 0) return;

    // Fake composed character sequences by continuing to delete until we delete a character of
    // width at least 1.
    int width;
    do {
        update_buff_pos(el, el->position - 1);
        width = fish_wcwidth(el->text.at(el->position));
        el->text.erase(el->position, 1);
    } while (width == 0 && el->position > 0);
    data->command_line_changed(el);
    data->suppress_autosuggestion = true;

    reader_super_highlight_me_plenty();

    reader_repaint_needed();
}

/// Insert the characters of the string into the command line buffer and print them to the screen
/// using syntax highlighting, etc. Optionally also expand abbreviations, after space characters.
/// Returns true if the string changed.
static bool insert_string(editable_line_t *el, const wcstring &str,
                          bool allow_expand_abbreviations = false) {
    reader_data_t *data = current_data();
    size_t len = str.size();
    if (len == 0) return false;

    // Start inserting. If we are expanding abbreviations, we have to do this after every space (see
    // #1434), so look for spaces. We try to do this efficiently (rather than the simpler character
    // at a time) to avoid expensive work in command_line_changed().
    size_t cursor = 0;
    while (cursor < len) {
        // Determine the position of the next expansion-triggering char (possibly none), and the end
        // of the range we wish to insert.
        const wchar_t *expansion_triggering_chars = L" ;|&^><";
        size_t char_triggering_expansion_pos =
            allow_expand_abbreviations ? str.find_first_of(expansion_triggering_chars, cursor)
                                       : wcstring::npos;
        bool has_expansion_triggering_char = (char_triggering_expansion_pos != wcstring::npos);
        size_t range_end =
            (has_expansion_triggering_char ? char_triggering_expansion_pos + 1 : len);

        // Insert from the cursor up to but not including the range end.
        assert(range_end > cursor);
        el->insert_string(str, cursor, range_end - cursor);

        update_buff_pos(el, el->position);
        data->command_line_changed(el);

        // If we got an expansion trigger, then the last character we inserted was it (i.e. was a
        // space). Expand abbreviations.
        if (has_expansion_triggering_char && allow_expand_abbreviations) {
            assert(range_end > 0);
            assert(wcschr(expansion_triggering_chars, str.at(range_end - 1)));
            data->expand_abbreviation_as_necessary(1);
        }
        cursor = range_end;
    }

    if (el == &data->command_line) {
        data->suppress_autosuggestion = false;

        // Syntax highlight. Note we must have that buff_pos > 0 because we just added something
        // nonzero to its length.
        assert(el->position > 0);
        reader_super_highlight_me_plenty(-1);
    }

    reader_repaint();

    return true;
}

/// Insert the character into the command line buffer and print it to the screen using syntax
/// highlighting, etc.
static bool insert_char(editable_line_t *el, wchar_t c, bool allow_expand_abbreviations = false) {
    return insert_string(el, wcstring(1, c), allow_expand_abbreviations);
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

    if (do_replace) {
        size_t move_cursor;
        const wchar_t *begin, *end;

        const wchar_t *buff = command_line.c_str();
        parse_util_token_extent(buff, cursor_pos, &begin, 0, 0, 0);
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
            sb.append(L" ");
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
        parse_util_get_parameter_info(command_line, cursor_pos, &quote, NULL, NULL);

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
        result.insert(new_cursor_pos++, L" ");
    }
    *inout_cursor_pos = new_cursor_pos;
    return result;
}

/// Insert the string at the current cursor position. The function checks if the string is quoted or
/// not and correctly escapes the string.
///
/// \param val the string to insert
/// \param flags A union of all flags describing the completion to insert. See the completion_t
/// struct for more information on possible values.
static void completion_insert(const wchar_t *val, complete_flags_t flags) {
    reader_data_t *data = current_data();
    editable_line_t *el = data->active_edit_line();
    size_t cursor = el->position;
    wcstring new_command_line = completion_apply_to_command_line(val, flags, el->text, &cursor,
                                                                 false /* not append only */);
    reader_set_buffer_maintaining_pager(new_command_line, cursor);
}

struct autosuggestion_result_t {
    wcstring suggestion;
    wcstring search_string;
};

// Returns a function that can be invoked (potentially
// on a background thread) to determine the autosuggestion
static std::function<autosuggestion_result_t(void)> get_autosuggestion_performer(
    const wcstring &search_string, size_t cursor_pos, history_t *history) {
    const unsigned int generation_count = read_generation_count();
    const wcstring working_directory(env_get_pwd_slash());
    env_vars_snapshot_t vars(env_vars_snapshot_t::highlighting_keys);
    // TODO: suspicious use of 'history' here
    // This is safe because histories are immortal, but perhaps
    // this should use shared_ptr
    return [=]() -> autosuggestion_result_t {
        ASSERT_IS_BACKGROUND_THREAD();

        const autosuggestion_result_t nothing = {};
        // If the main thread has moved on, skip all the work.
        if (generation_count != read_generation_count()) {
            return nothing;
        }

        DIE_ON_FAILURE(
            pthread_setspecific(generation_count_key, (void *)(uintptr_t)generation_count));

        // Let's make sure we aren't using the empty string.
        if (search_string.empty()) {
            return nothing;
        }

        history_search_t searcher(*history, search_string, HISTORY_SEARCH_TYPE_PREFIX);
        while (!reader_thread_job_is_stale() && searcher.go_backwards()) {
            history_item_t item = searcher.current_item();

            // Skip items with newlines because they make terrible autosuggestions.
            if (item.str().find('\n') != wcstring::npos) continue;

            if (autosuggest_validate_from_history(item, working_directory, vars)) {
                // The command autosuggestion was handled specially, so we're done.
                return {searcher.current_string(), search_string};
            }
        }

        // Maybe cancel here.
        if (reader_thread_job_is_stale()) return nothing;

        // Here we do something a little funny. If the line ends with a space, and the cursor is not
        // at the end, don't use completion autosuggestions. It ends up being pretty weird seeing
        // stuff get spammed on the right while you go back to edit a line
        const wchar_t last_char = search_string.at(search_string.size() - 1);
        const bool cursor_at_end = (cursor_pos == search_string.size());
        if (!cursor_at_end && iswspace(last_char)) return nothing;

        // On the other hand, if the line ends with a quote, don't go dumping stuff after the quote.
        if (wcschr(L"'\"", last_char) && cursor_at_end) return nothing;

        // Try normal completions.
        completion_request_flags_t complete_flags = COMPLETION_REQUEST_AUTOSUGGESTION;
        std::vector<completion_t> completions;
        complete(search_string, &completions, complete_flags);
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

static bool can_autosuggest() {
    // We autosuggest if suppress_autosuggestion is not set, if we're not doing a history search,
    // and our command line contains a non-whitespace character.
    reader_data_t *data = current_data();
    const editable_line_t *el = data->active_edit_line();
    const wchar_t *whitespace = L" \t\r\n\v";
    return data->allow_autosuggestion && !data->suppress_autosuggestion &&
           data->history_search.is_at_end() && el == &data->command_line &&
           el->text.find_first_not_of(whitespace) != wcstring::npos;
}

// Called after an autosuggestion has been computed on a background thread
static void autosuggest_completed(autosuggestion_result_t result) {
    reader_data_t *data = current_data();
    if (!result.suggestion.empty() && can_autosuggest() &&
        result.search_string == data->command_line.text &&
        string_prefixes_string_case_insensitive(result.search_string, result.suggestion)) {
        // Autosuggestion is active and the search term has not changed, so we're good to go.
        data->autosuggestion = std::move(result.suggestion);
        sanity_check();
        reader_repaint();
    }
}

static void update_autosuggestion() {
    // Updates autosuggestion. We look for an autosuggestion if the command line is non-empty and if
    // we're not doing a history search.
    reader_data_t *data = current_data();
    data->autosuggestion.clear();
    if (can_autosuggest()) {
        const editable_line_t *el = data->active_edit_line();
        auto performer = get_autosuggestion_performer(el->text, el->position, data->history);
        iothread_perform(performer, &autosuggest_completed);
    }
}

// Accept any autosuggestion by replacing the command line with it. If full is true, take the whole
// thing; if it's false, then take only the first "word".
static void accept_autosuggestion(bool full) {
    reader_data_t *data = current_data();
    if (!data->autosuggestion.empty()) {
        // Accepting an autosuggestion clears the pager.
        clear_pager();

        // Accept the autosuggestion.
        if (full) {
            // Just take the whole thing.
            data->command_line.text = data->autosuggestion;
        } else {
            // Accept characters up to a word separator.
            move_word_state_machine_t state(move_word_style_punctuation);
            for (size_t idx = data->command_line.size(); idx < data->autosuggestion.size(); idx++) {
                wchar_t wc = data->autosuggestion.at(idx);
                if (!state.consume_char(wc)) break;
                data->command_line.text.push_back(wc);
            }
        }
        update_buff_pos(&data->command_line, data->command_line.size());
        data->command_line_changed(&data->command_line);
        reader_super_highlight_me_plenty();
        reader_repaint();
    }
}

// Ensure we have no pager contents.
static void clear_pager() {
    if (reader_data_t *data = current_data_or_null()) {
        data->pager.clear();
        data->current_page_rendering = page_rendering_t();
        reader_repaint_needed();
    }
}

static void select_completion_in_direction(enum selection_direction_t dir) {
    reader_data_t *data = current_data();
    bool selection_changed =
        data->pager.select_next_completion_in_direction(dir, data->current_page_rendering);
    if (selection_changed) {
        data->pager_selection_changed();
    }
}

/// Flash the screen. This function changes the color of the current line momentarily and sends a
/// BEL to maybe flash the screen or emite a sound, depending on how it is configured.
static void reader_flash() {
    struct timespec pollint;
    reader_data_t *data = current_data();
    editable_line_t *el = &data->command_line;
    for (size_t i = 0; i < el->position; i++) {
        data->colors.at(i) = highlight_spec_search_match << 16;
    }

    reader_repaint();
    ignore_result(write(STDOUT_FILENO, "\a", 1));
    // The write above changed the timestamp of stdout; ensure we don't therefore reset our screen.
    // See #3693.
    s_save_status(&data->screen);

    pollint.tv_sec = 0;
    pollint.tv_nsec = 100 * 1000000;
    nanosleep(&pollint, NULL);

    reader_super_highlight_me_plenty();

    reader_repaint();
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
        if (wcschr(REPLACE_UNCLEAN, *str)) return false;
        str++;
    }

    return true;
}

/// Determine the best match type for a set of completions.
static fuzzy_match_type_t get_best_match_type(const std::vector<completion_t> &comp) {
    fuzzy_match_type_t best_type = fuzzy_match_none;
    for (size_t i = 0; i < comp.size(); i++) {
        best_type = std::min(best_type, comp.at(i).match.type);
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
/// a '/', '@', ':', or a '=', also write a trailing space.
/// - If the list contains multiple elements with a common prefix, write the prefix.
/// - If the list contains multiple elements without a common prefix, call run_pager to display a
/// list of completions. Depending on terminal size and the length of the list, run_pager may either
/// show less than a screenfull and exit or use an interactive pager to allow the user to scroll
/// through the completions.
///
/// \param comp the list of completion strings
/// \param cont_after_prefix_insertion If we have a shared prefix, whether to print the list of
/// completions after inserting it.
///
/// Return true if we inserted text into the command line, false if we did not.
static bool handle_completions(const std::vector<completion_t> &comp,
                               bool cont_after_prefix_insertion) {
    bool done = false;
    bool success = false;
    reader_data_t *data = current_data();
    const editable_line_t *el = &data->command_line;
    const wchar_t *begin, *end, *buff = el->text.c_str();

    parse_util_token_extent(buff, el->position, &begin, 0, 0, 0);
    end = buff + el->position;

    const wcstring tok(begin, end - begin);

    // Check trivial cases.
    size_t size = comp.size();
    if (size == 0) {
        // No suitable completions found, flash screen and return.
        reader_flash();
        done = true;
        success = false;
    } else if (size == 1) {
        // Exactly one suitable completion found - insert it.
        const completion_t &c = comp.at(0);

        // If this is a replacement completion, check that we know how to replace it, e.g. that
        // the token doesn't contain evil operators like {}.
        if (!(c.flags & COMPLETE_REPLACES_TOKEN) || reader_can_replace(tok, c.flags)) {
            completion_insert(c.completion.c_str(), c.flags);
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
    std::vector<completion_t> surviving_completions;
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
    if (match_type_shares_prefix(best_match_type)) {
        // Try to find a common prefix to insert among the surviving completions.
        wcstring common_prefix;
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
                size_t idx, max = mini(common_prefix.size(), el.completion.size());
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
            completion_insert(common_prefix.c_str(), flags);
            success = true;
        }
    }

    if (!cont_after_prefix_insertion && use_prefix) {
        return success;
    }

    // We didn't get a common prefix, or we want to print the list anyways.
    size_t len, prefix_start = 0;
    wcstring prefix;
    parse_util_get_parameter_info(el->text, el->position, NULL, &prefix_start, NULL);

    assert(el->position >= prefix_start);
    len = el->position - prefix_start;

    if (will_replace_token || match_type_requires_full_replacement(best_match_type)) {
        prefix.clear();  // no prefix
    } else if (len <= PREFIX_MAX_LEN) {
        prefix.append(el->text, prefix_start, len);
    } else {
        // Append just the end of the string.
        prefix = wcstring(&ellipsis_char, 1);
        prefix.append(el->text, prefix_start + len - PREFIX_MAX_LEN, PREFIX_MAX_LEN);
    }

    wchar_t quote;
    parse_util_get_parameter_info(el->text, el->position, &quote, NULL, NULL);
    // Update the pager data.
    data->pager.set_prefix(prefix);
    data->pager.set_completions(surviving_completions);
    // Invalidate our rendering.
    data->current_page_rendering = page_rendering_t();
    // Modify the command line to reflect the new pager.
    data->pager_selection_changed();
    reader_repaint_needed();
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
        char *tty = ctermid(NULL);
#endif
        if (!tty) {
            wperror(L"ctermid");
            exit_without_destructors(1);
        }

        // Open the tty. Presumably this is stdin, but maybe not?
        int tty_fd = open(tty, O_RDONLY | O_NONBLOCK);
        if (tty_fd < 0) {
            wperror(L"open");
            exit_without_destructors(1);
        }

        char tmp;
        if (read(tty_fd, &tmp, 1) < 0 && errno == EIO) {
            we_think_we_are_orphaned = true;
        }

        close(tty_fd);
    }

    // Just give up if we've done it a lot times.
    if (loop_count > 4096) {
        we_think_we_are_orphaned = true;
    }

    return we_think_we_are_orphaned;
}

/// Initialize data for interactive use.
static void reader_interactive_init() {
    // See if we are running interactively.
    pid_t shell_pgid;

    if (!input_initialized) init_input();
    kill_init();
    shell_pgid = getpgrp();

    // This should enable job control on fish, even if our parent process did not enable it for us.

    // Check if we are in control of the terminal, so that we don't do semi-expensive things like
    // reset signal handlers unless we really have to, which we often don't.
    if (tcgetpgrp(STDIN_FILENO) != shell_pgid) {
        // Bummer, we are not in control of the terminal. Stop until parent has given us control of
        // it.
        //
        // In theory, reseting signal handlers could cause us to miss signal deliveries. In
        // practice, this code should only be run during startup, when we're not waiting for any
        // signals.
        signal_reset_handlers();

        // Ok, signal handlers are taken out of the picture. Stop ourself in a loop until we are in
        // control of the terminal. However, the call to signal(SIGTTIN) may silently not do
        // anything if we are orphaned.
        //
        // As far as I can tell there's no really good way to detect that we are orphaned. One way
        // is to just detect if the group leader exited, via kill(shell_pgid, 0). Another
        // possibility is that read() from the tty fails with EIO - this is more reliable but it's
        // harder, because it may succeed or block. So we loop for a while, trying those strategies.
        // Eventually we just give up and assume we're orphaend.
        for (unsigned long loop_count = 0;; loop_count++) {
            pid_t owner = tcgetpgrp(STDIN_FILENO);
            shell_pgid = getpgrp();
            if (owner == -1 && errno == ENOTTY) {
                // No TTY, cannot be interactive?
                redirect_tty_output();
                debug(1, _(L"No TTY for interactive shell (tcgetpgrp failed)"));
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
                    debug(1, fmt, (int)getpid());
                    exit_without_destructors(1);
                }

                // Try stopping us.
                int ret = killpg(shell_pgid, SIGTTIN);
                if (ret < 0) {
                    wperror(L"killpg");
                    exit_without_destructors(1);
                }
            }
        }

        signal_set_handlers();
    }

    invalidate_termsize();

    // For compatibility with fish 2.0's $_, now replaced with `status current-command`
    env_set_one(L"_", ENV_GLOBAL, L"fish");
}

/// Destroy data for interactive use.
static void reader_interactive_destroy() {
    kill_destroy();
    set_color(rgb_color_t::reset(), rgb_color_t::reset());
    input_destroy();
}

void reader_sanity_check() {
    // Note: 'data' is non-null if we are interactive, except in the testing environment.
    reader_data_t *data = current_data_or_null();
    if (shell_is_interactive() && data != NULL) {
        if (data->command_line.position > data->command_line.size()) sanity_lose();
        if (data->colors.size() != data->command_line.size()) sanity_lose();
        if (data->indents.size() != data->command_line.size()) sanity_lose();
    }
}

/// Set the specified string as the current buffer.
static void set_command_line_and_position(editable_line_t *el, const wcstring &new_str,
                                          size_t pos) {
    reader_data_t *data = current_data();
    el->text = new_str;
    update_buff_pos(el, pos);
    data->command_line_changed(el);
    reader_super_highlight_me_plenty();
    reader_repaint_needed();
}

static void reader_replace_current_token(const wcstring &new_token) {
    const wchar_t *begin, *end;
    size_t new_pos;

    // Find current token.
    reader_data_t *data = current_data();
    editable_line_t *el = data->active_edit_line();
    const wchar_t *buff = el->text.c_str();
    parse_util_token_extent(buff, el->position, &begin, &end, 0, 0);

    if (!begin || !end) return;

    // Make new string.
    wcstring new_buff(buff, begin - buff);
    new_buff.append(new_token);
    new_buff.append(end);
    new_pos = (begin - buff) + new_token.size();

    set_command_line_and_position(el, new_buff, new_pos);
}

/// Apply the history search to the command line.
static void update_command_line_from_history_search() {
    reader_data_t *data = current_data();
    wcstring new_text = data->history_search.is_at_end() ? data->history_search.search_string()
                                                         : data->history_search.current_result();
    if (data->history_search.by_token()) {
        reader_replace_current_token(new_text);
    } else if (data->history_search.by_line()) {
        set_command_line_and_position(&data->command_line, new_text, new_text.size());
    }
}

enum move_word_dir_t { MOVE_DIR_LEFT, MOVE_DIR_RIGHT };

/// Move buffer position one word or erase one word. This function updates both the internal buffer
/// and the screen. It is used by M-left, M-right and ^W to do block movement or block erase.
///
/// \param move_right true if moving right
/// \param erase Whether to erase the characters along the way or only move past them.
/// \param newv if the new kill item should be appended to the previous kill item or not.
static void move_word(editable_line_t *el, bool move_right, bool erase,
                      enum move_word_style_t style, bool newv) {
    // Return if we are already at the edge.
    const size_t boundary = move_right ? el->size() : 0;
    if (el->position == boundary) return;

    // When moving left, a value of 1 means the character at index 0.
    move_word_state_machine_t state(style);
    const wchar_t *const command_line = el->text.c_str();
    const size_t start_buff_pos = el->position;

    size_t buff_pos = el->position;
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
        reader_data_t *data = current_data();
        if (el == &data->command_line) {
            data->suppress_autosuggestion = true;
        }

        if (move_right) {
            reader_kill(el, start_buff_pos, buff_pos - start_buff_pos, KILL_APPEND, newv);
        } else {
            reader_kill(el, buff_pos, start_buff_pos - buff_pos, KILL_PREPEND, newv);
        }
    } else {
        update_buff_pos(el, buff_pos);
        reader_repaint();
    }
}

const wchar_t *reader_get_buffer() {
    ASSERT_IS_MAIN_THREAD();
    reader_data_t *data = current_data_or_null();
    return data ? data->command_line.text.c_str() : NULL;
}

history_t *reader_get_history() {
    ASSERT_IS_MAIN_THREAD();
    reader_data_t *data = current_data_or_null();
    return data ? data->history : NULL;
}

/// Sets the command line contents, without clearing the pager.
static void reader_set_buffer_maintaining_pager(const wcstring &b, size_t pos) {
    reader_data_t *data = current_data();
    // Callers like to pass us pointers into ourselves, so be careful! I don't know if we can use
    // operator= with a pointer to our interior, so use an intermediate.
    size_t command_line_len = b.size();
    data->command_line.text = b;
    data->command_line_changed(&data->command_line);

    // Don't set a position past the command line length.
    if (pos > command_line_len) pos = command_line_len;  //!OCLINT(parameter reassignment)

    update_buff_pos(&data->command_line, pos);

    // Clear history search and pager contents.
    data->history_search.reset();
    reader_super_highlight_me_plenty();
    reader_repaint_needed();
}

/// Sets the command line contents, clearing the pager.
void reader_set_buffer(const wcstring &b, size_t pos) {
    reader_data_t *data = current_data_or_null();
    if (!data) return;

    clear_pager();
    reader_set_buffer_maintaining_pager(b, pos);
}

size_t reader_get_cursor_pos() {
    reader_data_t *data = current_data_or_null();
    if (!data) return (size_t)-1;

    return data->command_line.position;
}

bool reader_get_selection(size_t *start, size_t *len) {
    bool result = false;
    reader_data_t *data = current_data_or_null();
    if (data != NULL && data->sel_active) {
        *start = data->sel_start_pos;
        *len = std::min(data->sel_stop_pos - data->sel_start_pos, data->command_line.size());
        result = true;
    }
    return result;
}

void set_env_cmd_duration(struct timeval *after, struct timeval *before) {
    time_t secs = after->tv_sec - before->tv_sec;
    suseconds_t usecs = after->tv_usec - before->tv_usec;
    wchar_t buf[16];

    if (after->tv_usec < before->tv_usec) {
        usecs += 1000000;
        secs -= 1;
    }

    swprintf(buf, 16, L"%d", (secs * 1000) + (usecs / 1000));
    env_set_one(ENV_cmd_duration, ENV_UNEXPORT, buf);
}

void reader_run_command(parser_t &parser, const wcstring &cmd) {
    struct timeval time_before, time_after;

    wcstring ft = tok_first(cmd);

    // For compatibility with fish 2.0's $_, now replaced with `status current-command`
    if (!ft.empty()) env_set_one(L"_", ENV_GLOBAL, ft);

    reader_write_title(cmd);

    term_donate();

    gettimeofday(&time_before, NULL);

    parser.eval(cmd, io_chain_t(), TOP);
    job_reap(1);

    gettimeofday(&time_after, NULL);

    // update the execution duration iff a command is requested for execution
    // issue - #4926
    if (!ft.empty()) set_env_cmd_duration(&time_after, &time_before);

    term_steal();

    // For compatibility with fish 2.0's $_, now replaced with `status current-command`
    env_set_one(L"_", ENV_GLOBAL, program_name);

#ifdef HAVE__PROC_SELF_STAT
    proc_update_jiffies();
#endif
}

parser_test_error_bits_t reader_shell_test(const wcstring &b) {
    wcstring bstr = b;

    // Append a newline, to act as a statement terminator.
    bstr.push_back(L'\n');

    parse_error_list_t errors;
    parser_test_error_bits_t res =
        parse_util_detect_errors(bstr, &errors, true /* do accept incomplete */);

    if (res & PARSER_TEST_ERROR) {
        wcstring error_desc;
        parser_t::principal_parser().get_backtrace(bstr, errors, error_desc);

        // Ensure we end with a newline. Also add an initial newline, because it's likely the user
        // just hit enter and so there's junk on the current line.
        if (!string_suffixes_string(L"\n", error_desc)) {
            error_desc.push_back(L'\n');
        }
        fwprintf(stderr, L"\n%ls", error_desc.c_str());
    }
    return res;
}

/// Test if the given string contains error. Since this is the error detection for general purpose,
/// there are no invalid strings, so this function always returns false.
static parser_test_error_bits_t default_test(const wcstring &b) {
    UNUSED(b);
    return 0;
}

void reader_change_history(const wcstring &name) {
    // We don't need to _change_ if we're not initialized yet.
    reader_data_t *data = current_data_or_null();
    if (data && data->history) {
        data->history->save();
        data->history = &history_t::history_with_name(name);
    }
}

void reader_push(const wcstring &name) {
    history_t *hist = &history_t::history_with_name(name);
    reader_data_stack.push_back(make_unique<reader_data_t>(hist));
    reader_data_t *data = current_data();
    data->command_line_changed(&data->command_line);
    if (reader_data_stack.size() == 1) {
        reader_interactive_init();
    }

    exec_prompt();
    reader_set_highlight_function(&highlight_universal);
    reader_set_test_function(&default_test);
    reader_set_left_prompt(L"");
}

void reader_pop() {
    assert(!reader_data_stack.empty() && "empty stack in reader_data_stack");
    reader_data_stack.pop_back();
    reader_data_t *new_reader = current_data_or_null();
    if (new_reader == nullptr) {
        reader_interactive_destroy();
    } else {
        end_loop = 0;
        s_reset(&new_reader->screen, screen_reset_abandon_line);
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

void reader_set_complete_function(complete_function_t f) { current_data()->complete_func = f; }

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

    // Import history from bash, etc. if our current history is still empty.
    if (data->history && data->history->is_empty()) {
        // Try opening a bash file. We make an effort to respect $HISTFILE; this isn't very complete
        // (AFAIK it doesn't have to be exported), and to really get this right we ought to ask bash
        // itself. But this is better than nothing.
        const auto var = env_get(L"HISTFILE");
        wcstring path = (var ? var->as_string() : L"~/.bash_history");
        expand_tilde(path);
        FILE *f = wfopen(path, "r");
        if (f) {
            data->history->populate_from_bash(f);
            fclose(f);
        }
    }
}

/// Called to set the highlight flag for search results.
static void highlight_search() {
    reader_data_t *data = current_data();
    if (data->history_search.is_at_end()) {
        return;
    }
    const wcstring &needle = data->history_search.search_string();
    const editable_line_t *el = &data->command_line;
    size_t match_pos = el->text.find(needle);
    if (match_pos != wcstring::npos) {
        size_t end = match_pos + needle.size();
        for (size_t i = match_pos; i < end; i++) {
            data->colors.at(i) |= (highlight_spec_search_match << 16);
        }
    }
}

struct highlight_result_t {
    std::vector<highlight_spec_t> colors;
    wcstring text;
};

static void highlight_complete(highlight_result_t result) {
    ASSERT_IS_MAIN_THREAD();
    reader_data_t *data = current_data();
    if (result.text == data->command_line.text) {
        // The data hasn't changed, so swap in our colors. The colors may not have changed, so do
        // nothing if they have not.
        assert(result.colors.size() == data->command_line.size());
        if (data->colors != result.colors) {
            data->colors = std::move(result.colors);
            sanity_check();
            highlight_search();
            reader_repaint();
        }
    }
}

// Given text, bracket matching position, and whether IO is allowed,
// return a function that performs highlighting. The function may be invoked on a background thread.
static std::function<highlight_result_t(void)> get_highlight_performer(const wcstring &text,
                                                                       long match_highlight_pos,
                                                                       bool no_io) {
    env_vars_snapshot_t vars(env_vars_snapshot_t::highlighting_keys);
    unsigned int generation_count = read_generation_count();
    highlight_function_t highlight_func =
        no_io ? highlight_shell_no_io : current_data()->highlight_func;
    return [=]() -> highlight_result_t {
        if (generation_count != read_generation_count()) {
            // The gen count has changed, so don't do anything.
            return {};
        }
        if (text.empty()) {
            return {};
        }
        DIE_ON_FAILURE(
            pthread_setspecific(generation_count_key, (void *)(uintptr_t)generation_count));
        std::vector<highlight_spec_t> colors(text.size(), 0);
        highlight_func(text, colors, match_highlight_pos, NULL /* error */, vars);
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
static void reader_super_highlight_me_plenty(int match_highlight_pos_adjust, bool no_io) {
    reader_data_t *data = current_data();
    const editable_line_t *el = &data->command_line;
    assert(el != NULL);
    long match_highlight_pos = (long)el->position + match_highlight_pos_adjust;
    assert(match_highlight_pos >= 0);

    reader_sanity_check();

    auto highlight_performer = get_highlight_performer(el->text, match_highlight_pos, no_io);
    if (no_io) {
        // Highlighting without IO, we just do it.
        highlight_complete(highlight_performer());
    } else {
        // Highlighting including I/O proceeds in the background.
        iothread_perform(highlight_performer, &highlight_complete);
    }
    highlight_search();

    // Here's a hack. Check to see if our autosuggestion still applies; if so, don't recompute it.
    // Since the autosuggestion computation is asynchronous, this avoids "flashing" as you type into
    // the autosuggestion.
    const wcstring &cmd = el->text, &suggest = data->autosuggestion;
    if (can_autosuggest() && !suggest.empty() &&
        string_prefixes_string_case_insensitive(cmd, suggest)) {
        ;  // the autosuggestion is still reasonable, so do nothing
    } else {
        update_autosuggestion();
    }
}

bool shell_is_exiting() {
    if (shell_is_interactive()) {
        reader_data_t *data = current_data_or_null();
        return job_list_is_empty() && data != NULL && data->end_loop;
    }
    return end_loop;
}

void reader_bg_job_warning() {
    fputws(_(L"There are still jobs active:\n"), stdout);
    fputws(_(L"\n   PID  Command\n"), stdout);

    job_iterator_t jobs;
    while (job_t *j = jobs.next()) {
        if (!job_is_completed(j)) {
            fwprintf(stdout, L"%6d  %ls\n", j->processes[0]->pid, j->command_wcstr());
        }
    }
    fputws(L"\n", stdout);
    fputws(_(L"A second attempt to exit will terminate them.\n"), stdout);
    fputws(_(L"Use 'disown PID' to remove jobs from the list without terminating them.\n"), stdout);
}

void kill_background_jobs() {
    job_iterator_t jobs;
    while (job_t *j = jobs.next()) {
        if (!job_is_completed(j)) {
            if (job_is_stopped(j)) job_signal(j, SIGCONT);
            job_signal(j, SIGHUP);
        }
    }
}

/// This function is called when the main loop notices that end_loop has been set while in
/// interactive mode. It checks if it is ok to exit.
static void handle_end_loop() {
    if (!reader_exit_forced()) {
        const parser_t &parser = parser_t::principal_parser();
        for (size_t i = 0; i < parser.block_count(); i++) {
            if (parser.block_at_index(i)->type() == BREAKPOINT) {
                // We're executing within a breakpoint so we do not want to terminate the shell and
                // kill background jobs.
                return;
            }
        }

        bool bg_jobs = false;
        job_iterator_t jobs;
        while (const job_t *j = jobs.next()) {
            if (!job_is_completed(j)) {
                bg_jobs = true;
                break;
            }
        }

        reader_data_t *data = current_data();
        if (!data->prev_end_loop && bg_jobs) {
            reader_bg_job_warning();
            reader_exit(0, 0);
            data->prev_end_loop = 1;
            return;
        }
    }

    // Kill remaining jobs before exiting.
    kill_background_jobs();
}

static bool selection_is_at_top() {
    reader_data_t *data = current_data();
    const pager_t *pager = &data->pager;
    size_t row = pager->get_selected_row(data->current_page_rendering);
    if (row != 0 && row != PAGER_SELECTION_NONE) return false;

    size_t col = pager->get_selected_column(data->current_page_rendering);
    if (col != 0 && col != PAGER_SELECTION_NONE) return false;

    return true;
}

static uint32_t run_count = 0;

/// Returns the current interactive loop count
uint32_t reader_run_count() { return run_count; }

/// Read interactively. Read input from stdin while providing editing facilities.
static int read_i() {
    reader_push(history_session_id());
    reader_set_complete_function(&complete);
    reader_set_highlight_function(&highlight_shell);
    reader_set_test_function(&reader_shell_test);
    reader_set_allow_autosuggesting(true);
    reader_set_expand_abbreviations(true);
    reader_import_history_if_necessary();

    parser_t &parser = parser_t::principal_parser();
    reader_data_t *data = current_data();
    data->prev_end_loop = 0;

    while ((!data->end_loop) && (!sanity_check())) {
        event_fire_generic(L"fish_prompt");
        run_count++;

        if (is_breakpoint && function_exists(DEBUG_PROMPT_FUNCTION_NAME)) {
            reader_set_left_prompt(DEBUG_PROMPT_FUNCTION_NAME);
        } else if (function_exists(LEFT_PROMPT_FUNCTION_NAME)) {
            reader_set_left_prompt(LEFT_PROMPT_FUNCTION_NAME);
        } else {
            reader_set_left_prompt(DEFAULT_PROMPT);
        }

        if (function_exists(RIGHT_PROMPT_FUNCTION_NAME)) {
            reader_set_right_prompt(RIGHT_PROMPT_FUNCTION_NAME);
        } else {
            reader_set_right_prompt(L"");
        }

        // Put buff in temporary string and clear buff, so that we can handle a call to
        // reader_set_buffer during evaluation.
        const wchar_t *tmp = reader_readline(0);

        if (data->end_loop) {
            handle_end_loop();
        } else if (tmp) {
            const wcstring command = tmp;
            update_buff_pos(&data->command_line, 0);
            data->command_line.text.clear();
            data->command_line_changed(&data->command_line);
            wcstring_list_t argv(1, command);
            event_fire_generic(L"fish_preexec", &argv);
            reader_run_command(parser, command);
            event_fire_generic(L"fish_postexec", &argv);
            // Allow any pending history items to be returned in the history array.
            if (data->history) {
                data->history->resolve_pending();
            }
            if (data->end_loop) {
                handle_end_loop();
            } else {
                data->prev_end_loop = 0;
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
    return select(fd + 1, &fds, 0, 0, &can_read_timeout) == 1;
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
    tok_t token;
    while (tok.next(&token)) {
        ;  // pass
    }
    return token.type == TOK_COMMENT;
}

const wchar_t *reader_readline(int nchars) {
    wint_t c;
    int last_char = 0;
    size_t yank_len = 0;
    const wchar_t *yank_str;
    bool comp_empty = true;
    std::vector<completion_t> comp;
    int finished = 0;
    struct termios old_modes;

    // Coalesce redundant repaints. When we get a repaint, we set this to true, and skip repaints
    // until we get something else.
    bool coalescing_repaints = false;

    // The command line before completion.
    reader_data_t *data = current_data();
    data->cycle_command_line.clear();
    data->cycle_cursor_pos = 0;

    data->history_search.reset();

    exec_prompt();

    reader_super_highlight_me_plenty();
    s_reset(&data->screen, screen_reset_abandon_line);
    reader_repaint();

    // Get the current terminal modes. These will be restored when the function returns.
    if (tcgetattr(STDIN_FILENO, &old_modes) == -1 && errno == EIO) redirect_tty_output();
    // Set the new modes.
    if (tcsetattr(0, TCSANOW, &shell_modes) == -1) {
        if (errno == EIO) redirect_tty_output();
        wperror(L"tcsetattr");
    }

    while (!finished && !data->end_loop) {
        if (0 < nchars && (size_t)nchars <= data->command_line.size()) {
            // We've already hit the specified character limit.
            finished = 1;
            break;
        }

        // Sometimes strange input sequences seem to generate a zero byte. I believe these simply
        // mean a character was pressed but it should be ignored. (Example: Trying to add a tilde
        // (~) to digit).
        while (1) {
            int was_interactive_read = is_interactive_read;
            is_interactive_read = 1;
            c = input_readch();
            is_interactive_read = was_interactive_read;
            // fwprintf(stderr, L"C: %lx\n", (long)c);

            if (((!fish_reserved_codepoint(c))) && (c > 31) && (c != 127) && can_read(0)) {
                wchar_t arr[READAHEAD_MAX + 1];
                size_t i;
                size_t limit = 0 < nchars ? std::min((size_t)nchars - data->command_line.size(),
                                                     (size_t)READAHEAD_MAX)
                                          : READAHEAD_MAX;

                memset(arr, 0, sizeof(arr));
                arr[0] = c;

                for (i = 1; i < limit; ++i) {
                    if (!can_read(0)) {
                        c = 0;
                        break;
                    }
                    // Only allow commands on the first key; otherwise, we might have data we
                    // need to insert on the commandline that the commmand might need to be able
                    // to see.
                    c = input_readch(false);
                    if (!fish_reserved_codepoint(c) && c > 31 && c != 127) {
                        arr[i] = c;
                        c = 0;
                    } else
                        break;
                }

                editable_line_t *el = data->active_edit_line();
                insert_string(el, arr, true);

                // End paging upon inserting into the normal command line.
                if (el == &data->command_line) {
                    clear_pager();
                }
                last_char = c;
            }

            if (c != 0) break;

            if (0 < nchars && (size_t)nchars <= data->command_line.size()) {
                c = R_NULL;
                break;
            }
        }

        // If we get something other than a repaint, then stop coalescing them.
        if (c != R_REPAINT) coalescing_repaints = false;

        if (last_char != R_YANK && last_char != R_YANK_POP) yank_len = 0;

        // Restore the text.
        if (c == R_CANCEL && data->is_navigating_pager_contents()) {
            set_command_line_and_position(&data->command_line, data->cycle_command_line,
                                          data->cycle_cursor_pos);
        }

        // Clear the pager if necessary.
        bool focused_on_search_field = (data->active_edit_line() == &data->pager.search_field_line);
        if (command_ends_paging(c, focused_on_search_field)) {
            clear_pager();
        }
        // fwprintf(stderr, L"\n\nchar: %ls\n\n", describe_char(c).c_str());

        switch (c) {
            // Go to beginning of line.
            case R_BEGINNING_OF_LINE: {
                editable_line_t *el = data->active_edit_line();
                while (el->position > 0 && el->text.at(el->position - 1) != L'\n') {
                    update_buff_pos(el, el->position - 1);
                }

                reader_repaint_needed();
                break;
            }
            case R_END_OF_LINE: {
                editable_line_t *el = data->active_edit_line();
                if (el->position < el->size()) {
                    const wchar_t *buff = el->text.c_str();
                    while (buff[el->position] && buff[el->position] != L'\n') {
                        update_buff_pos(el, el->position + 1);
                    }
                } else {
                    accept_autosuggestion(true);
                }

                reader_repaint_needed();
                break;
            }
            case R_BEGINNING_OF_BUFFER: {
                update_buff_pos(&data->command_line, 0);
                reader_repaint_needed();
                break;
            }
            case R_END_OF_BUFFER: {
                update_buff_pos(&data->command_line, data->command_line.size());
                reader_repaint_needed();
                break;
            }
            case R_NULL: {
                break;
            }
            case R_CANCEL: {
                // The only thing we can cancel right now is paging, which we handled up above.
                break;
            }
            case R_FORCE_REPAINT:
            case R_REPAINT: {
                if (!coalescing_repaints) {
                    coalescing_repaints = true;
                    exec_prompt();
                    s_reset(&data->screen, screen_reset_current_line_and_prompt);
                    data->screen_reset_needed = false;
                    reader_repaint();
                }
                break;
            }
            case R_EOF: {
                exit_forced = true;
                data->end_loop = 1;
                break;
            }
            case R_COMPLETE:
            case R_COMPLETE_AND_SEARCH: {
                if (!data->complete_func) break;

                // Use the command line only; it doesn't make sense to complete in any other line.
                editable_line_t *el = &data->command_line;
                if (data->is_navigating_pager_contents() ||
                    (!comp_empty && last_char == R_COMPLETE)) {
                    // The user typed R_COMPLETE more than once in a row. If we are not yet fully
                    // disclosed, then become so; otherwise cycle through our available completions.
                    if (data->current_page_rendering.remaining_to_disclose > 0) {
                        data->pager.set_fully_disclosed(true);
                        reader_repaint_needed();
                    } else {
                        select_completion_in_direction(c == R_COMPLETE ? direction_next
                                                                       : direction_prev);
                    }
                } else {
                    // Either the user hit tab only once, or we had no visible completion list.
                    // Remove a trailing backslash. This may trigger an extra repaint, but this is
                    // rare.
                    if (is_backslashed(el->text, el->position)) {
                        remove_backward();
                    }

                    // Get the string; we have to do this after removing any trailing backslash.
                    const wchar_t *const buff = el->text.c_str();

                    // Clear the completion list.
                    comp.clear();

                    // Figure out the extent of the command substitution surrounding the cursor.
                    // This is because we only look at the current command substitution to form
                    // completions - stuff happening outside of it is not interesting.
                    const wchar_t *cmdsub_begin, *cmdsub_end;
                    parse_util_cmdsubst_extent(buff, el->position, &cmdsub_begin, &cmdsub_end);

                    // Figure out the extent of the token within the command substitution. Note we
                    // pass cmdsub_begin here, not buff.
                    const wchar_t *token_begin, *token_end;
                    parse_util_token_extent(cmdsub_begin, el->position - (cmdsub_begin - buff),
                                            &token_begin, &token_end, 0, 0);

                    // Hack: the token may extend past the end of the command substitution, e.g. in
                    // (echo foo) the last token is 'foo)'. Don't let that happen.
                    if (token_end > cmdsub_end) token_end = cmdsub_end;

                    // Figure out how many steps to get from the current position to the end of the
                    // current token.
                    size_t end_of_token_offset = token_end - buff;

                    // Move the cursor to the end.
                    if (el->position != end_of_token_offset) {
                        update_buff_pos(el, end_of_token_offset);
                        reader_repaint();
                    }

                    // Construct a copy of the string from the beginning of the command substitution
                    // up to the end of the token we're completing.
                    const wcstring buffcpy = wcstring(cmdsub_begin, token_end);

                    // fwprintf(stderr, L"Complete (%ls)\n", buffcpy.c_str());
                    complete_flags_t complete_flags = COMPLETION_REQUEST_DEFAULT |
                                                      COMPLETION_REQUEST_DESCRIPTIONS |
                                                      COMPLETION_REQUEST_FUZZY_MATCH;
                    data->complete_func(buffcpy, &comp, complete_flags);

                    // Munge our completions.
                    completions_sort_and_prioritize(&comp);

                    // Record our cycle_command_line.
                    data->cycle_command_line = el->text;
                    data->cycle_cursor_pos = el->position;

                    bool cont_after_prefix_insertion = (c == R_COMPLETE_AND_SEARCH);
                    comp_empty = handle_completions(comp, cont_after_prefix_insertion);

                    // Show the search field if requested and if we printed a list of completions.
                    if (c == R_COMPLETE_AND_SEARCH && !comp_empty && !data->pager.empty()) {
                        data->pager.set_search_field_shown(true);
                        select_completion_in_direction(direction_next);
                        reader_repaint_needed();
                    }
                }
                break;
            }
            case R_PAGER_TOGGLE_SEARCH: {
                if (data->is_navigating_pager_contents()) {
                    bool sfs = data->pager.is_search_field_shown();
                    data->pager.set_search_field_shown(!sfs);
                    data->pager.set_fully_disclosed(true);
                    reader_repaint_needed();
                }
                break;
            }
            case R_KILL_LINE: {
                editable_line_t *el = data->active_edit_line();
                const wchar_t *buff = el->text.c_str();
                const wchar_t *begin = &buff[el->position];
                const wchar_t *end = begin;

                while (*end && *end != L'\n') end++;

                if (end == begin && *end) end++;

                size_t len = end - begin;
                if (len) {
                    reader_kill(el, begin - buff, len, KILL_APPEND, last_char != R_KILL_LINE);
                }
                break;
            }
            case R_BACKWARD_KILL_LINE: {
                editable_line_t *el = data->active_edit_line();
                if (el->position <= 0) {
                    break;
                }
                const wchar_t *buff = el->text.c_str();
                const wchar_t *end = &buff[el->position];
                const wchar_t *begin = end;

                begin--;  // make sure we delete at least one character (see issue #580)

                // Delete until we hit a newline, or the beginning of the string.
                while (begin > buff && *begin != L'\n') begin--;

                // If we landed on a newline, don't delete it.
                if (*begin == L'\n') begin++;
                assert(end >= begin);
                size_t len = maxi<size_t>(end - begin, 1);
                begin = end - len;
                reader_kill(el, begin - buff, len, KILL_PREPEND, last_char != R_BACKWARD_KILL_LINE);
                break;
            }
            case R_KILL_WHOLE_LINE: {
                // We match the emacs behavior here: "kills the entire line including the following
                // newline".
                editable_line_t *el = data->active_edit_line();
                const wchar_t *buff = el->text.c_str();

                // Back up to the character just past the previous newline, or go to the beginning
                // of the command line. Note that if the position is on a newline, visually this
                // looks like the cursor is at the end of a line. Therefore that newline is NOT the
                // beginning of a line; this justifies the -1 check.
                size_t begin = el->position;
                while (begin > 0 && buff[begin - 1] != L'\n') {
                    begin--;
                }

                // Push end forwards to just past the next newline, or just past the last char.
                size_t end = el->position;
                while (buff[end] != L'\0') {
                    end++;
                    if (buff[end - 1] == L'\n') {
                        break;
                    }
                }
                assert(end >= begin);

                if (end > begin) {
                    reader_kill(el, begin, end - begin, KILL_APPEND,
                                last_char != R_KILL_WHOLE_LINE);
                }
                break;
            }
            case R_YANK: {
                yank_str = kill_yank();
                insert_string(data->active_edit_line(), yank_str);
                yank_len = wcslen(yank_str);
                break;
            }
            case R_YANK_POP: {
                if (yank_len) {
                    for (size_t i = 0; i < yank_len; i++) remove_backward();

                    yank_str = kill_yank_rotate();
                    insert_string(data->active_edit_line(), yank_str);
                    yank_len = wcslen(yank_str);
                }
                break;
            }
            // Escape was pressed.
            case L'\x1B': {
                if (data->history_search.active()) {
                    data->history_search.go_to_end();
                    update_command_line_from_history_search();
                    data->history_search.reset();
                }
                assert(!data->history_search.active());
                break;
            }
            case R_BACKWARD_DELETE_CHAR: {
                remove_backward();
                break;
            }
            case R_DELETE_CHAR: {
                // Remove the current character in the character buffer and on the screen using
                // syntax highlighting, etc.
                editable_line_t *el = data->active_edit_line();
                if (el->position < el->size()) {
                    update_buff_pos(el, el->position + 1);
                    remove_backward();
                }
                break;
            }
            // Evaluate. If the current command is unfinished, or if the charater is escaped using a
            // backslash, insert a newline.
            case R_EXECUTE: {
                // If the user hits return while navigating the pager, it only clears the pager.
                if (data->is_navigating_pager_contents()) {
                    clear_pager();
                    break;
                }

                // Delete any autosuggestion.
                data->autosuggestion.clear();

                // The user may have hit return with pager contents, but while not navigating them.
                // Clear the pager in that event.
                clear_pager();

                // We only execute the command line.
                editable_line_t *el = &data->command_line;

                // Allow backslash-escaped newlines.
                bool continue_on_next_line = false;
                if (el->position >= el->size()) {
                    // We're at the end of the text and not in a comment (issue #1225).
                    continue_on_next_line =
                        is_backslashed(el->text, el->position) && !text_ends_in_comment(el->text);
                } else {
                    // Allow mid line split if the following character is whitespace (issue #613).
                    if (is_backslashed(el->text, el->position) &&
                        iswspace(el->text.at(el->position))) {
                        continue_on_next_line = true;
                        // Check if the end of the line is backslashed (issue #4467).
                    } else if (is_backslashed(el->text, el->size()) &&
                               !text_ends_in_comment(el->text)) {
                        // Move the cursor to the end of the line.
                        el->position = el->size();
                        continue_on_next_line = true;
                    }
                }
                // If the conditions are met, insert a new line at the position of the cursor.
                if (continue_on_next_line) {
                    insert_char(el, '\n');
                    break;
                }

                // See if this command is valid.
                int command_test_result = data->test_func(el->text);
                if (command_test_result == 0 || command_test_result == PARSER_TEST_INCOMPLETE) {
                    // This command is valid, but an abbreviation may make it invalid. If so, we
                    // will have to test again.
                    bool abbreviation_expanded = data->expand_abbreviation_as_necessary(1);
                    if (abbreviation_expanded) {
                        // It's our reponsibility to rehighlight and repaint. But everything we do
                        // below triggers a repaint.
                        command_test_result = data->test_func(el->text.c_str());

                        // If the command is OK, then we're going to execute it. We still want to do
                        // syntax highlighting, but a synchronous variant that performs no I/O, so
                        // as not to block the user.
                        bool skip_io = (command_test_result == 0);
                        reader_super_highlight_me_plenty(0, skip_io);
                    }
                }

                if (command_test_result == 0) {
                    // Finished command, execute it. Don't add items that start with a leading
                    // space.
                    const editable_line_t *el = &data->command_line;
                    if (data->history != NULL && !el->empty() && el->text.at(0) != L' ') {
                        data->history->add_pending_with_file_detection(el->text);
                    }
                    finished = 1;
                    update_buff_pos(&data->command_line, data->command_line.size());
                    reader_repaint();
                } else if (command_test_result == PARSER_TEST_INCOMPLETE) {
                    // We are incomplete, continue editing.
                    insert_char(el, '\n');
                } else {
                    // Result must be some combination including an error. The error message will
                    // already be printed, all we need to do is repaint.
                    s_reset(&data->screen, screen_reset_abandon_line);
                    reader_repaint_needed();
                }

                break;
            }

            case R_HISTORY_SEARCH_BACKWARD:
            case R_HISTORY_TOKEN_SEARCH_BACKWARD:
            case R_HISTORY_SEARCH_FORWARD:
            case R_HISTORY_TOKEN_SEARCH_FORWARD: {
                if (data->history_search.is_at_end()) {
                    const editable_line_t *el = &data->command_line;
                    bool by_token = (c == R_HISTORY_TOKEN_SEARCH_BACKWARD) ||
                                    (c == R_HISTORY_TOKEN_SEARCH_FORWARD);
                    if (by_token) {
                        // Searching by token.
                        const wchar_t *begin, *end;
                        const wchar_t *buff = el->text.c_str();
                        parse_util_token_extent(buff, el->position, &begin, &end, 0, 0);
                        if (begin) {
                            wcstring token(begin, end);
                            data->history_search.reset_to_mode(token, data->history,
                                                               reader_history_search_t::token);
                        } else {
                            // No current token, refuse to do a token search.
                            data->history_search.reset();
                        }
                    } else {
                        // Searching by line.
                        data->history_search.reset_to_mode(el->text, data->history,
                                                           reader_history_search_t::line);

                        // Skip the autosuggestion in the history unless it was truncated.
                        const wcstring &suggest = data->autosuggestion;
                        if (!suggest.empty() && !data->screen.autosuggestion_is_truncated) {
                            data->history_search.add_skip(suggest);
                        }
                    }
                }
                if (data->history_search.active()) {
                    history_search_direction_t dir =
                        (c == R_HISTORY_SEARCH_BACKWARD || c == R_HISTORY_TOKEN_SEARCH_BACKWARD)
                            ? history_search_direction_t::backward
                            : history_search_direction_t::forward;
                    data->history_search.move_in_direction(dir);
                    update_command_line_from_history_search();
                }
                break;
            }
            case R_BACKWARD_CHAR: {
                editable_line_t *el = data->active_edit_line();
                if (data->is_navigating_pager_contents()) {
                    select_completion_in_direction(direction_west);
                } else if (el->position > 0) {
                    update_buff_pos(el, el->position - 1);
                    reader_repaint_needed();
                }
                break;
            }
            case R_FORWARD_CHAR: {
                editable_line_t *el = data->active_edit_line();
                if (data->is_navigating_pager_contents()) {
                    select_completion_in_direction(direction_east);
                } else if (el->position < el->size()) {
                    update_buff_pos(el, el->position + 1);
                    reader_repaint_needed();
                } else {
                    accept_autosuggestion(true);
                }
                break;
            }
            case R_BACKWARD_KILL_WORD:
            case R_BACKWARD_KILL_PATH_COMPONENT:
            case R_BACKWARD_KILL_BIGWORD: {
                move_word_style_t style =
                    (c == R_BACKWARD_KILL_BIGWORD
                         ? move_word_style_whitespace
                         : c == R_BACKWARD_KILL_PATH_COMPONENT ? move_word_style_path_components
                                                               : move_word_style_punctuation);
                bool newv = (last_char != R_BACKWARD_KILL_WORD &&
                             last_char != R_BACKWARD_KILL_PATH_COMPONENT &&
                             last_char != R_BACKWARD_KILL_BIGWORD);
                move_word(data->active_edit_line(), MOVE_DIR_LEFT, true /* erase */, style, newv);
                break;
            }
            case R_KILL_WORD: {
                move_word(data->active_edit_line(), MOVE_DIR_RIGHT, true /* erase */,
                          move_word_style_punctuation, last_char != R_KILL_WORD);
                break;
            }
            case R_KILL_BIGWORD: {
                move_word(data->active_edit_line(), MOVE_DIR_RIGHT, true /* erase */,
                          move_word_style_whitespace, last_char != R_KILL_BIGWORD);
                break;
            }
            case R_BACKWARD_WORD: {
                move_word(data->active_edit_line(), MOVE_DIR_LEFT, false /* do not erase */,
                          move_word_style_punctuation, false);
                break;
            }
            case R_BACKWARD_BIGWORD: {
                move_word(data->active_edit_line(), MOVE_DIR_LEFT, false /* do not erase */,
                          move_word_style_whitespace, false);
                break;
            }
            case R_FORWARD_WORD: {
                editable_line_t *el = data->active_edit_line();
                if (el->position < el->size()) {
                    move_word(el, MOVE_DIR_RIGHT, false /* do not erase */,
                              move_word_style_punctuation, false);
                } else {
                    accept_autosuggestion(false /* accept only one word */);
                }
                break;
            }
            case R_FORWARD_BIGWORD: {
                editable_line_t *el = data->active_edit_line();
                if (el->position < el->size()) {
                    move_word(el, MOVE_DIR_RIGHT, false /* do not erase */,
                              move_word_style_whitespace, false);
                } else {
                    accept_autosuggestion(false /* accept only one word */);
                }
                break;
            }
            case R_BEGINNING_OF_HISTORY:
            case R_END_OF_HISTORY: {
                bool up = (c == R_BEGINNING_OF_HISTORY);
                if (data->is_navigating_pager_contents()) {
                    select_completion_in_direction(up ? direction_page_north
                                                      : direction_page_south);
                } else {
                    if (up) {
                        data->history_search.go_to_beginning();
                    } else {
                        data->history_search.go_to_end();
                    }
                    update_command_line_from_history_search();
                }
                break;
            }
            case R_UP_LINE:
            case R_DOWN_LINE: {
                if (data->is_navigating_pager_contents()) {
                    // We are already navigating pager contents.
                    selection_direction_t direction;
                    if (c == R_DOWN_LINE) {
                        // Down arrow is always south.
                        direction = direction_south;
                    } else if (selection_is_at_top()) {
                        // Up arrow, but we are in the first column and first row. End navigation.
                        direction = direction_deselect;
                    } else {
                        // Up arrow, go north.
                        direction = direction_north;
                    }

                    // Now do the selection.
                    select_completion_in_direction(direction);
                } else if (!data->pager.empty()) {
                    // We pressed a direction with a non-empty pager, begin navigation.
                    select_completion_in_direction(c == R_DOWN_LINE ? direction_south
                                                                    : direction_north);
                } else {
                    // Not navigating the pager contents.
                    editable_line_t *el = data->active_edit_line();
                    int line_old = parse_util_get_line_from_offset(el->text, el->position);
                    int line_new;

                    if (c == R_UP_LINE)
                        line_new = line_old - 1;
                    else
                        line_new = line_old + 1;

                    int line_count = parse_util_lineno(el->text.c_str(), el->size()) - 1;

                    if (line_new >= 0 && line_new <= line_count) {
                        size_t base_pos_new;
                        size_t base_pos_old;

                        int indent_old;
                        int indent_new;
                        size_t line_offset_old;
                        size_t total_offset_new;

                        base_pos_new = parse_util_get_offset_from_line(el->text, line_new);

                        base_pos_old = parse_util_get_offset_from_line(el->text, line_old);

                        assert(base_pos_new != (size_t)(-1) && base_pos_old != (size_t)(-1));
                        indent_old = data->indents.at(base_pos_old);
                        indent_new = data->indents.at(base_pos_new);

                        line_offset_old =
                            el->position - parse_util_get_offset_from_line(el->text, line_old);
                        total_offset_new = parse_util_get_offset(
                            el->text, line_new, line_offset_old - 4 * (indent_new - indent_old));
                        update_buff_pos(el, total_offset_new);
                        reader_repaint_needed();
                    }
                }
                break;
            }
            case R_SUPPRESS_AUTOSUGGESTION: {
                data->suppress_autosuggestion = true;
                data->autosuggestion.clear();
                reader_repaint_needed();
                break;
            }
            case R_ACCEPT_AUTOSUGGESTION: {
                accept_autosuggestion(true);
                break;
            }
            case R_TRANSPOSE_CHARS: {
                editable_line_t *el = data->active_edit_line();
                if (el->size() < 2) {
                    break;
                }

                // If the cursor is at the end, transpose the last two characters of the line.
                if (el->position == el->size()) {
                    update_buff_pos(el, el->position - 1);
                }

                // Drag the character before the cursor forward over the character at the cursor,
                // moving the cursor forward as well.
                if (el->position > 0) {
                    wcstring local_cmd = el->text;
                    std::swap(local_cmd.at(el->position), local_cmd.at(el->position - 1));
                    set_command_line_and_position(el, local_cmd, el->position + 1);
                }
                break;
            }
            case R_TRANSPOSE_WORDS: {
                editable_line_t *el = data->active_edit_line();
                size_t len = el->size();
                const wchar_t *buff = el->text.c_str();
                const wchar_t *tok_begin, *tok_end, *prev_begin, *prev_end;

                // If we are not in a token, look for one ahead.
                size_t buff_pos = el->position;
                while (buff_pos != len && !iswalnum(buff[buff_pos])) buff_pos++;

                update_buff_pos(el, buff_pos);

                parse_util_token_extent(buff, el->position, &tok_begin, &tok_end, &prev_begin,
                                        &prev_end);

                // In case we didn't find a token at or after the cursor...
                if (tok_begin == &buff[len]) {
                    // ...retry beginning from the previous token.
                    size_t pos = prev_end - &buff[0];
                    parse_util_token_extent(buff, pos, &tok_begin, &tok_end, &prev_begin,
                                            &prev_end);
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
                    set_command_line_and_position(el, new_buff, tok_end - buff);
                }
                break;
            }
            case R_UPCASE_WORD:
            case R_DOWNCASE_WORD:
            case R_CAPITALIZE_WORD: {
                editable_line_t *el = data->active_edit_line();
                // For capitalize_word, whether we've capitalized a character so far.
                bool capitalized_first = false;

                // We apply the operation from the current location to the end of the word.
                size_t pos = el->position;
                move_word(el, MOVE_DIR_RIGHT, false, move_word_style_punctuation, false);
                for (; pos < el->position; pos++) {
                    wchar_t chr = el->text.at(pos);

                    // We always change the case; this decides whether we go uppercase (true) or
                    // lowercase (false).
                    bool make_uppercase;
                    if (c == R_CAPITALIZE_WORD)
                        make_uppercase = !capitalized_first && iswalnum(chr);
                    else
                        make_uppercase = (c == R_UPCASE_WORD);

                    // Apply the operation and then record what we did.
                    if (make_uppercase)
                        chr = towupper(chr);
                    else
                        chr = towlower(chr);

                    data->command_line.text.at(pos) = chr;
                    capitalized_first = capitalized_first || make_uppercase;
                }
                data->command_line_changed(el);
                reader_super_highlight_me_plenty();
                reader_repaint_needed();
                break;
            }
            case R_BEGIN_SELECTION: {
                data->sel_active = true;
                data->sel_begin_pos = data->command_line.position;
                data->sel_start_pos = data->command_line.position;
                data->sel_stop_pos = data->command_line.position;
                break;
            }
            case R_SWAP_SELECTION_START_STOP: {
                if (!data->sel_active) break;
                size_t tmp = data->sel_begin_pos;
                data->sel_begin_pos = data->command_line.position;
                data->sel_start_pos = data->command_line.position;
                editable_line_t *el = data->active_edit_line();
                update_buff_pos(el, tmp);
                break;
            }
            case R_END_SELECTION: {
                data->sel_active = false;
                data->sel_start_pos = data->command_line.position;
                data->sel_stop_pos = data->command_line.position;
                break;
            }
            case R_KILL_SELECTION: {
                bool newv = (last_char != R_KILL_SELECTION);
                size_t start, len;
                if (reader_get_selection(&start, &len)) {
                    reader_kill(&data->command_line, start, len, KILL_APPEND, newv);
                }
                break;
            }
            case R_FORWARD_JUMP: {
                editable_line_t *el = data->active_edit_line();
                wchar_t target = input_function_pop_arg();
                bool success = jump(jump_direction_t::forward, jump_precision_t::to, el, target);

                input_function_set_status(success);
                reader_repaint_needed();
                break;
            }
            case R_BACKWARD_JUMP: {
                editable_line_t *el = data->active_edit_line();
                wchar_t target = input_function_pop_arg();
                bool success = jump(jump_direction_t::backward, jump_precision_t::to, el, target);

                input_function_set_status(success);
                reader_repaint_needed();
                break;
            }
            case R_FORWARD_JUMP_TILL: {
                editable_line_t *el = data->active_edit_line();
                wchar_t target = input_function_pop_arg();
                bool success = jump(jump_direction_t::forward, jump_precision_t::till, el, target);

                input_function_set_status(success);
                reader_repaint_needed();
                break;
            }
            case R_BACKWARD_JUMP_TILL: {
                editable_line_t *el = data->active_edit_line();
                wchar_t target = input_function_pop_arg();
                bool success = jump(jump_direction_t::backward, jump_precision_t::till, el, target);

                input_function_set_status(success);
                reader_repaint_needed();
                break;
            }
            case R_REPEAT_JUMP: {
                editable_line_t *el = data->active_edit_line();
                bool success = false;

                if (data->last_jump_target) {
                    success = jump(data->last_jump_direction, data->last_jump_precision, el,
                                   data->last_jump_target);
                }

                input_function_set_status(success);
                reader_repaint_needed();
                break;
            }
            case R_REVERSE_REPEAT_JUMP: {
                editable_line_t *el = data->active_edit_line();
                bool success = false;
                jump_direction_t original_dir, dir;
                original_dir = dir = data->last_jump_direction;

                if (data->last_jump_direction == jump_direction_t::forward) {
                    dir = jump_direction_t::backward;
                } else {
                    dir = jump_direction_t::forward;
                }

                if (data->last_jump_target) {
                    success = jump(dir, data->last_jump_precision, el, data->last_jump_target);
                }

                data->last_jump_direction = original_dir;

                input_function_set_status(success);
                reader_repaint_needed();
                break;
            }
            default: {
                // Other, if a normal character, we add it to the command.
                if (!fish_reserved_codepoint(c) && (c >= L' ' || c == L'\n' || c == L'\r') &&
                    c != 0x7F) {
                    // Regular character.
                    editable_line_t *el = data->active_edit_line();
                    bool allow_expand_abbreviations = (el == &data->command_line);
                    insert_char(data->active_edit_line(), c, allow_expand_abbreviations);

                    // End paging upon inserting into the normal command line.
                    if (el == &data->command_line) {
                        clear_pager();
                    }
                } else {
                    // This can happen if the user presses a control char we don't recognize. No
                    // reason to report this to the user unless they've enabled debugging output.
                    debug(2, _(L"Unknown key binding 0x%X"), c);
                }
                break;
            }
        }

        if ((c != R_HISTORY_SEARCH_BACKWARD) && (c != R_HISTORY_SEARCH_FORWARD) &&
            (c != R_HISTORY_TOKEN_SEARCH_BACKWARD) && (c != R_HISTORY_TOKEN_SEARCH_FORWARD) &&
            (c != R_NULL) && (c != R_REPAINT) && (c != R_FORCE_REPAINT)) {
            data->history_search.reset();
        }

        last_char = c;

        reader_repaint_if_needed();
    }

    ignore_result(write(STDOUT_FILENO, "\n", 1));

    // Ensure we have no pager contents when we exit.
    if (!data->pager.empty()) {
        // Clear to end of screen to erase the pager contents.
        // TODO: this may fail if eos doesn't exist, in which case we should emit newlines.
        screen_force_clear_to_end();
        data->pager.clear();
    }

    if (!reader_exit_forced()) {
        if (tcsetattr(0, TCSANOW, &old_modes) == -1) {
            if (errno == EIO) redirect_tty_output();
            wperror(L"tcsetattr");  // return to previous mode
        }
        set_color(rgb_color_t::reset(), rgb_color_t::reset());
    }

    return finished ? data->command_line.text.c_str() : NULL;
}

bool jump(jump_direction_t dir, jump_precision_t precision, editable_line_t *el, wchar_t target) {
    reader_data_t *data = current_data_or_null();
    bool success = false;

    data->last_jump_target = target;
    data->last_jump_direction = dir;
    data->last_jump_precision = precision;

    switch (dir) {
        case jump_direction_t::backward: {
            size_t tmp_pos = el->position;

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
            for (size_t tmp_pos = el->position + 1; tmp_pos < el->size(); tmp_pos++) {
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

bool reader_is_in_search_mode() {
    reader_data_t *data = current_data_or_null();
    return data && data->history_search.active();
}

bool reader_has_pager_contents() {
    reader_data_t *data = current_data_or_null();
    return data && !data->current_page_rendering.screen_data.empty();
}

/// Read non-interactively.  Read input from stdin without displaying the prompt, using syntax
/// highlighting. This is used for reading scripts and init files.
static int read_ni(int fd, const io_chain_t &io) {
    parser_t &parser = parser_t::principal_parser();
    FILE *in_stream;
    wchar_t *buff = 0;
    std::vector<char> acc;

    int des = (fd == STDIN_FILENO ? dup(STDIN_FILENO) : fd);
    int res = 0;

    if (des == -1) {
        wperror(L"dup");
        return 1;
    }

    in_stream = fdopen(des, "r");
    if (in_stream != 0) {
        while (!feof(in_stream)) {
            char buff[4096];
            size_t c = fread(buff, 1, 4096, in_stream);

            if (ferror(in_stream)) {
                if (errno == EINTR) {
                    // We got a signal, just keep going. Be sure that we call insert() below because
                    // we may get data as well as EINTR.
                    clearerr(in_stream);
                } else if ((errno == EAGAIN || errno == EWOULDBLOCK) &&
                           make_fd_blocking(des) == 0) {
                    // We succeeded in making the fd blocking, keep going.
                    clearerr(in_stream);
                } else {
                    // Fatal error.
                    debug(1, _(L"Error while reading from file descriptor"));
                    // Reset buffer on error. We won't evaluate incomplete files.
                    acc.clear();
                    break;
                }
            }

            acc.insert(acc.end(), buff, buff + c);
        }

        wcstring str = acc.empty() ? wcstring() : str2wcstring(&acc.at(0), acc.size());
        acc.clear();

        if (fclose(in_stream)) {
            debug(1, _(L"Error while closing input stream"));
            wperror(L"fclose");
            res = 1;
        }

        // Swallow a BOM (issue #1518).
        if (!str.empty() && str.at(0) == UTF8_BOM_WCHAR) {
            str.erase(0, 1);
        }

        parse_error_list_t errors;
        parsed_source_ref_t pstree;
        if (!parse_util_detect_errors(str, &errors, false /* do not accept incomplete */,
                                      &pstree)) {
            parser.eval(pstree, io, TOP);
        } else {
            wcstring sb;
            parser.get_backtrace(str, errors, sb);
            fwprintf(stderr, L"%ls", sb.c_str());
            res = 1;
        }
    } else {
        debug(1, _(L"Error while opening input stream"));
        wperror(L"fdopen");
        free(buff);
        res = 1;
    }
    return res;
}

int reader_read(int fd, const io_chain_t &io) {
    int res;

    // If reader_read is called recursively through the '.' builtin, we need to preserve
    // is_interactive. This, and signal handler setup is handled by
    // proc_push_interactive/proc_pop_interactive.
    int inter = 0;
    // This block is a hack to work around https://sourceware.org/bugzilla/show_bug.cgi?id=20632.
    // See also, commit 396bf12. Without the need for this workaround we would just write:
    // int inter = ((fd == STDIN_FILENO) && isatty(STDIN_FILENO));
    if (fd == STDIN_FILENO) {
        struct termios t;
        int a_tty = isatty(STDIN_FILENO);
        if (a_tty) {
            inter = 1;
        } else if (tcgetattr(STDIN_FILENO, &t) == -1 && errno == EIO) {
            redirect_tty_output();
            inter = 1;
        }
    }
    proc_push_interactive(inter);

    res = shell_is_interactive() ? read_i() : read_ni(fd, io);

    // If the exit command was called in a script, only exit the script, not the program.
    reader_data_t *data = current_data_or_null();
    if (data) data->end_loop = 0;
    end_loop = 0;

    proc_pop_interactive();
    return res;
}
