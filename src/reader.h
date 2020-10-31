// Prototypes for functions for reading data from stdin and passing to the parser. If stdin is a
// keyboard, it supplies a killring, history, syntax highlighting, tab-completion and various other
// features.
#ifndef FISH_READER_H
#define FISH_READER_H

#include <stddef.h>

#include <string>
#include <vector>

#include "common.h"
#include "complete.h"
#include "highlight.h"
#include "parse_constants.h"

class environment_t;
class history_t;
class io_chain_t;
class operation_context_t;
class parser_t;

/// An edit action that can be undone.
struct edit_t {
    /// When undoing the edit we use this to restore the previous cursor position.
    size_t cursor_position_before_edit = 0;

    /// The span of text that is replaced by this edit.
    size_t offset, length;

    /// The strings that are removed and added by this edit, respectively.
    wcstring old, replacement;

    explicit edit_t(size_t offset, size_t length, wcstring replacement)
        : offset(offset), length(length), replacement(std::move(replacement)) {}

    /// Used for testing.
    bool operator==(const edit_t &other) const;
};

/// Modify a string according to the given edit.
/// Currently exposed for testing only.
void apply_edit(wcstring *target, const edit_t &edit);

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

    /// Empty the history.
    void clear();
};

/// Helper class for storing a command line.
class editable_line_t {
    /// The command line.
    wcstring text_;
    /// The current position of the cursor in the command line.
    size_t position_ = 0;

   public:
    undo_history_t undo_history;

    const wcstring &text() const { return text_; }
    /// Set the text directly without maintaining undo invariants. Use with caution.
    void set_text_bypassing_undo_history(wcstring &&text) { text_ = text; }

    size_t position() const { return position_; }
    void set_position(size_t position) { position_ = position; }

    // Gets the length of the text.
    size_t size() const { return text().size(); }

    bool empty() const { return text().empty(); }

    wchar_t at(size_t idx) const { return text().at(idx); }

    void clear() {
        undo_history.clear();
        if (empty()) return;
        set_text_bypassing_undo_history(L"");
        set_position(0);
    }

    /// Modify the commandline according to @edit. Most modifications to the
    /// text should pass through this function.
    void push_edit(edit_t &&edit);

    /// Modify the commandline by inserting a string at the cursor.
    /// Does not create a new undo point, but adds to the last edit which
    /// must be an insertion, too.
    void insert_coalesce(const wcstring &str);

    /// Undo the most recent edit that was not yet undone. Returns true on success.
    bool undo();

    /// Redo the most recent undo. Returns true on success.
    bool redo();
};

/// Read commands from \c fd until encountering EOF.
/// The fd is not closed.
int reader_read(parser_t &parser, int fd, const io_chain_t &io);

/// Mark that we encountered SIGHUP and must (soon) exit. This is invoked from a signal handler.
void reader_sighup();

/// Initialize the reader.
void reader_init();

/// Restore the term mode at startup.
void restore_term_mode();

/// Change the history file for the current command reading context.
void reader_change_history(const wcstring &name);

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

/// Get the string of character currently entered into the command buffer, or 0 if interactive mode
/// is uninitialized.
const wchar_t *reader_get_buffer();

/// Returns the current reader's history.
history_t *reader_get_history();

/// Set the string of characters in the command buffer, as well as the cursor position.
///
/// \param b the new buffer value
/// \param p the cursor position. If \c p is larger than the length of the command line, the cursor
/// is placed on the last character.
void reader_set_buffer(const wcstring &b, size_t p = -1);

/// Get the current cursor position in the command line. If interactive mode is uninitialized,
/// return (size_t)-1.
size_t reader_get_cursor_pos();

/// Get the current selection range in the command line. Returns false if there is no active
/// selection, true otherwise.
bool reader_get_selection(size_t *start, size_t *len);

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

/// Push a new reader environment controlled by \p conf.
/// If \p history_name is not empty, then use the history with that name.
void reader_push(parser_t &parser, const wcstring &history_name, reader_config_t &&conf);

/// Return to previous reader environment.
void reader_pop();

/// The readers interrupt signal handler. Cancels all currently running blocks.
void reader_handle_sigint();

/// \return whether we should cancel fish script due to fish itself receiving a signal.
/// TODO: this doesn't belong in reader.
bool check_cancel_from_fish_signal();

/// Test whether the interactive reader is in search mode.
bool reader_is_in_search_mode();

/// Test whether the interactive reader has visible pager contents.
bool reader_has_pager_contents();

/// Given a command line and an autosuggestion, return the string that gets shown to the user.
/// Exposed for testing purposes only.
wcstring combine_command_and_autosuggestion(const wcstring &cmdline,
                                            const wcstring &autosuggestion);

/// Expand abbreviations at the given cursor position. Exposed for testing purposes only.
/// \return none if no abbreviations were expanded, otherwise the new command line.
maybe_t<edit_t> reader_expand_abbreviation_in_command(const wcstring &cmdline, size_t cursor_pos,
                                                      const environment_t &vars);

/// Apply a completion string. Exposed for testing only.
wcstring completion_apply_to_command_line(const wcstring &val_str, complete_flags_t flags,
                                          const wcstring &command_line, size_t *inout_cursor_pos,
                                          bool append_only);

/// Return the current interactive reads loop count. Useful for determining how many commands have
/// been executed between invocations of code.
uint64_t reader_run_count();

/// Returns the current "generation" of interactive status. Useful for determining whether the
/// previous command produced a status.
uint64_t reader_status_count();

#endif
