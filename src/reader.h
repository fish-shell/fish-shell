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
#include "wutil.h"

#if INCLUDE_RUST_HEADERS
#include "reader.rs.h"
#endif

#include "editable_line.h"

int reader_read_ffi(const void *parser, int fd, const void *io_chain);
/// Read commands from \c fd until encountering EOF.
/// The fd is not closed.
int reader_read(const parser_t &parser, int fd, const io_chain_t &io);

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

#if INCLUDE_RUST_HEADERS
void reader_change_cursor_selection_mode(cursor_selection_mode_t selection_mode);
#else
void reader_change_cursor_selection_mode(uint8_t selection_mode);
#endif

struct EnvDyn;
/// Enable or disable autosuggestions based on the associated variable.
void reader_set_autosuggestion_enabled(const env_stack_t &vars);
void reader_set_autosuggestion_enabled_ffi(bool enabled);

/// Write the title to the titlebar. This function is called just before a new application starts
/// executing and just after it finishes.
///
/// \param cmd Command line string passed to \c fish_title if is defined.
/// \param parser The parser to use for autoloading fish_title.
/// \param reset_cursor_position If set, issue a \r so the line driver knows where we are
void reader_write_title(const wcstring &cmd, const parser_t &parser,
                        bool reset_cursor_position = true);

void reader_write_title_ffi(const wcstring &cmd, const void *parser, bool reset_cursor_position);

/// Tell the reader that it needs to re-exec the prompt and repaint.
/// This may be called in response to e.g. a color variable change.
void reader_schedule_prompt_repaint();

/// Enqueue an event to the back of the reader's input queue.
struct CharEvent;
using char_event_t = CharEvent;
void reader_queue_ch(rust::Box<char_event_t> ch);

/// Return the value of the interrupted flag, which is set by the sigint handler, and clear it if it
/// was set. If the current reader is interruptible, call \c reader_exit().
int reader_reading_interrupted();

/// Read one line of input. Before calling this function, reader_push() must have been called in
/// order to set up a valid reader environment. If nchars > 0, return after reading that many
/// characters even if a full line has not yet been read. Note: the returned value may be longer
/// than nchars if a single keypress resulted in multiple characters being inserted into the
/// commandline.
maybe_t<wcstring> reader_readline(int nchars);

bool reader_readline_ffi(wcstring &line, int nchars);

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
void reader_push(const parser_t &parser, const wcstring &history_name, reader_config_t &&conf);

void reader_push_ffi(const void *parser, const wcstring &history_name, const void *conf);

/// Return to previous reader environment.
void reader_pop();

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
                                                                  const parser_t &parser);

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

HistorySharedPtr *commandline_get_state_history_ffi();
bool commandline_get_state_initialized_ffi();
wcstring commandline_get_state_text_ffi();

/// Set the command line text and position. This may be called on a background thread; the reader
/// will pick it up when it is done executing.
void commandline_set_buffer(wcstring text, size_t cursor_pos = -1);
void commandline_set_buffer_ffi(const wcstring &text, size_t cursor_pos);

/// Return the current interactive reads loop count. Useful for determining how many commands have
/// been executed between invocations of code.
uint64_t reader_run_count();

/// Returns the current "generation" of interactive status. Useful for determining whether the
/// previous command produced a status.
uint64_t reader_status_count();

// For FFI
uint32_t read_generation_count();

#endif
