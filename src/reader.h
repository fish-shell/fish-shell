// Prototypes for functions for reading data from stdin and passing to the parser. If stdin is a
// keyboard, it supplies a killring, history, syntax highlighting, tab-completion and various other
// features.
#ifndef FISH_READER_H
#define FISH_READER_H

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common.h"
#include "complete.h"
#include "env.h"
#include "highlight.h"
#include "io.h"
#include "maybe.h"
#include "parse_constants.h"
#include "parser.h"

/// An edit action that can be undone.
struct edit_t {
    /// When undoing the edit we use this to restore the previous cursor position.
    size_t cursor_position_before_edit = 0;

    /// The span of text that is replaced by this edit.
    size_t offset, length;

    /// The strings that are removed and added by this edit, respectively.
    wcstring old, replacement;

    /// edit_t is only for contiguous changes, so to restore a group of arbitrary changes to the
    /// command line we need to have a group id as forcibly coalescing changes is not enough.
    maybe_t<int> group_id;

    explicit edit_t(size_t offset, size_t length, wcstring replacement)
        : offset(offset), length(length), replacement(std::move(replacement)) {}

    explicit edit_t(source_range_t range, wcstring replacement)
        : edit_t(range.start, range.length, std::move(replacement)) {}

    /// Used for testing.
    bool operator==(const edit_t &other) const;
};

/// Modify a string and its syntax highlighting according to the given edit.
/// Currently exposed for testing only.
void apply_edit(wcstring *target, std::vector<highlight_spec_t> *colors, const edit_t &edit);

/// The history of all edits to some command line.
struct undo_history_t {
    /// The stack of edits that can be undone or redone atomically.
    std::vector<edit_t> edits;

    /// The position in the undo stack that corresponds to the current
    /// state of the input line.
    /// Invariants:
    ///     edits_applied - 1 is the index of the next edit to undo.
    ///     edits_applied     is the index of the next edit to redo.
    ///
    /// For example, if nothing was undone, edits_applied is edits.size().
    /// If every single edit was undone, edits_applied is 0.
    size_t edits_applied = 0;

    /// Whether we allow the next edit to be grouped together with the
    /// last one.
    bool may_coalesce = false;

    /// Whether to be more aggressive in coalescing edits. Ideally, it would be "force coalesce"
    /// with guaranteed atomicity but as `edit_t` is strictly for contiguous changes, that guarantee
    /// can't be made at this time.
    bool try_coalesce = false;

    /// Empty the history.
    void clear();
};

/// Helper class for storing a command line.
class editable_line_t {
   public:
    const wcstring &text() const { return text_; }

    const std::vector<highlight_spec_t> &colors() const { return colors_; }
    void set_colors(std::vector<highlight_spec_t> colors) {
        assert(colors.size() == size());
        colors_ = std::move(colors);
    }

    size_t position() const { return position_; }
    void set_position(size_t position) { position_ = position; }

    // Gets the length of the text.
    size_t size() const { return text().size(); }

    bool empty() const { return text().empty(); }

    wchar_t at(size_t idx) const { return text().at(idx); }

    void clear();

    /// Modify the commandline according to @edit. Most modifications to the
    /// text should pass through this function.
    void push_edit(edit_t edit, bool allow_coalesce);

    /// Undo the most recent edit that was not yet undone. Returns true on success.
    bool undo();

    /// Redo the most recent undo. Returns true on success.
    bool redo();

    /// Start a logical grouping of command line edits that should be undone/redone together.
    void begin_edit_group();
    /// End a logical grouping of command line edits that should be undone/redone together.
    void end_edit_group();

   private:
    /// Whether we want to append this string to the previous edit.
    bool want_to_coalesce_insertion_of(const wcstring &str) const;

    /// The command line.
    wcstring text_;
    /// Syntax highlighting.
    std::vector<highlight_spec_t> colors_;
    /// The current position of the cursor in the command line.
    size_t position_ = 0;

    /// The history of all edits.
    undo_history_t undo_history_;
    /// The nesting level for atomic edits, so that recursive invocations of start_edit_group()
    /// are not ended by one end_edit_group() call.
    int32_t edit_group_level_ = -1;
    /// Monotonically increasing edit group, ignored when edit_group_level_ is -1. Allowed to wrap.
    uint32_t edit_group_id_ = -1;
};

/// Read commands from \c fd until encountering EOF.
/// The fd is not closed.
int reader_read(parser_t &parser, int fd, const io_chain_t &io);
int reader_read_ffi(void *parser, int fd, const void *io);

/// Mark that we encountered SIGHUP and must (soon) exit. This is invoked from a signal handler.
extern "C" {
void reader_sighup();
}

/// Initialize the reader.
void reader_init();
void term_copy_modes();

/// Restore the term mode at startup.
void restore_term_mode();

/// Change the history file for the current command reading context.
void reader_change_history(const wcstring &name);

/// Strategy for determining how the selection behaves.
enum class cursor_selection_mode_t : uint8_t {
    /// The character at/after the cursor is excluded.
    /// This is most useful with a line cursor shape.
    exclusive,
    /// The character at/after the cursor is included.
    /// This is most useful with a block or underscore cursor shape.
    inclusive,
};

void reader_change_cursor_selection_mode(cursor_selection_mode_t selection_mode);

/// Enable or disable autosuggestions based on the associated variable.
void reader_set_autosuggestion_enabled(const env_stack_t &vars);

/// Write the title to the titlebar. This function is called just before a new application starts
/// executing and just after it finishes.
///
/// \param cmd Command line string passed to \c fish_title if is defined.
/// \param parser The parser to use for autoloading fish_title.
/// \param reset_cursor_position If set, issue a \r so the line driver knows where we are
void reader_write_title(const wcstring &cmd, parser_t &parser, bool reset_cursor_position = true);

/// Tell the reader that it needs to re-exec the prompt and repaint.
/// This may be called in response to e.g. a color variable change.
void reader_schedule_prompt_repaint();

/// Enqueue an event to the back of the reader's input queue.
class char_event_t;
void reader_queue_ch(const char_event_t &ch);

/// Return the value of the interrupted flag, which is set by the sigint handler, and clear it if it
/// was set. In practice this will return 0 or SIGINT.
int reader_test_and_clear_interrupted();

/// Clear the interrupted flag unconditionally without handling anything. The flag could have been
/// set e.g. when an interrupt arrived just as we were ending an earlier \c reader_readline
/// invocation but before the \c is_interactive_read flag was cleared.
void reader_reset_interrupted();

/// Return the value of the interrupted flag, which is set by the sigint handler, and clear it if it
/// was set. If the current reader is interruptible, call \c reader_exit().
int reader_reading_interrupted();

/// Read one line of input. Before calling this function, reader_push() must have been called in
/// order to set up a valid reader environment. If nchars > 0, return after reading that many
/// characters even if a full line has not yet been read. Note: the returned value may be longer
/// than nchars if a single keypress resulted in multiple characters being inserted into the
/// commandline.
maybe_t<wcstring> reader_readline(int nchars);

/// Configuration that we provide to a reader.
struct reader_config_t {
    /// Left prompt command, typically fish_prompt.
    wcstring left_prompt_cmd{};

    /// Right prompt command, typically fish_right_prompt.
    wcstring right_prompt_cmd{};

    /// Name of the event to trigger once we're set up.
    wcstring event{};

    /// Whether tab completion is OK.
    bool complete_ok{false};

    /// Whether to perform syntax highlighting.
    bool highlight_ok{false};

    /// Whether to perform syntax checking before returning.
    bool syntax_check_ok{false};

    /// Whether to allow autosuggestions.
    bool autosuggest_ok{false};

    /// Whether to expand abbreviations.
    bool expand_abbrev_ok{false};

    /// Whether to exit on interrupt (^C).
    bool exit_on_interrupt{false};

    /// If set, do not show what is typed.
    bool in_silent_mode{false};

    /// The fd for stdin, default to actual stdin.
    int in{0};
};

class reader_data_t;
bool check_exit_loop_maybe_warning(reader_data_t *data);

/// Push a new reader environment controlled by \p conf, using the given history name.
/// If \p history_name is empty, then save history in-memory only; do not write it to disk.
void reader_push(parser_t &parser, const wcstring &history_name, reader_config_t &&conf);

/// Return to previous reader environment.
void reader_pop();

/// The readers interrupt signal handler. Cancels all currently running blocks.
extern "C" {
void reader_handle_sigint();
}

/// \return whether fish is currently unwinding the stack in preparation to exit.
bool fish_is_unwinding_for_exit();

/// Given a command line and an autosuggestion, return the string that gets shown to the user.
/// Exposed for testing purposes only.
wcstring combine_command_and_autosuggestion(const wcstring &cmdline,
                                            const wcstring &autosuggestion);

/// Expand at most one abbreviation at the given cursor position, updating the position if the
/// abbreviation wants to move the cursor. Use the parser to run any abbreviations which want
/// function calls. \return none if no abbreviations were expanded, otherwise the resulting
/// replacement.
struct abbrs_replacement_t;
maybe_t<abbrs_replacement_t> reader_expand_abbreviation_at_cursor(const wcstring &cmdline,
                                                                  size_t cursor_pos,
                                                                  parser_t &parser);

/// Apply a completion string. Exposed for testing only.
wcstring completion_apply_to_command_line(const wcstring &val_str, complete_flags_t flags,
                                          const wcstring &command_line, size_t *inout_cursor_pos,
                                          bool append_only);

/// Snapshotted state from the reader.
struct commandline_state_t {
    wcstring text;                        // command line text, or empty if not interactive
    size_t cursor_pos{0};                 // position of the cursor, may be as large as text.size()
    maybe_t<source_range_t> selection{};  // visual selection, or none if none
    maybe_t<rust::Box<HistorySharedPtr>>
        history{};                      // current reader history, or null if not interactive
    bool pager_mode{false};             // pager is visible
    bool pager_fully_disclosed{false};  // pager already shows everything if possible
    bool search_mode{false};            // pager is visible and search is active
    bool initialized{false};            // if false, the reader has not yet been entered
};

/// Get the command line state. This may be fetched on a background thread.
commandline_state_t commandline_get_state();

/// Set the command line text and position. This may be called on a background thread; the reader
/// will pick it up when it is done executing.
void commandline_set_buffer(wcstring text, size_t cursor_pos = -1);

/// Return the current interactive reads loop count. Useful for determining how many commands have
/// been executed between invocations of code.
uint64_t reader_run_count();

/// Returns the current "generation" of interactive status. Useful for determining whether the
/// previous command produced a status.
uint64_t reader_status_count();

#endif
