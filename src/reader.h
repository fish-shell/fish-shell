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

class history_t;
class env_vars_snapshot_t;
class io_chain_t;

/// Helper class for storing a command line.
class editable_line_t {
   public:
    /// The command line.
    wcstring text;
    /// The current position of the cursor in the command line.
    size_t position;

    const wcstring &get_text() const { return text; }

    // Gets the length of the text.
    size_t size() const { return text.size(); }

    bool empty() const { return text.empty(); }

    void clear() {
        text.clear();
        position = 0;
    }

    wchar_t at(size_t idx) { return text.at(idx); }

    editable_line_t() : text(), position(0) {}

    /// Inserts a substring of str given by start, len at the cursor position.
    void insert_string(const wcstring &str, size_t start = 0, size_t len = wcstring::npos);
};

/// Read commands from \c fd until encountering EOF.
int reader_read(int fd, const io_chain_t &io);

/// Tell the shell that it should exit after the currently running command finishes.
void reader_exit(int do_exit, int force);

/// Check that the reader is in a sane state.
void reader_sanity_check();

/// Initialize the reader.
void reader_init();

/// Restore the term mode at startup.
void restore_term_mode();

/// Returns the filename of the file currently read.
const wchar_t *reader_current_filename();

/// Push a new filename on the stack of read files.
///
/// \param fn The fileanme to push
void reader_push_current_filename(const wchar_t *fn);

/// Change the history file for the current command reading context.
void reader_change_history(const wcstring &name);

/// Pop the current filename from the stack of read files.
void reader_pop_current_filename();

/// Write the title to the titlebar. This function is called just before a new application starts
/// executing and just after it finishes.
///
/// \param cmd Command line string passed to \c fish_title if is defined.
/// \param reset_cursor_position If set, issue a \r so the line driver knows where we are
void reader_write_title(const wcstring &cmd, bool reset_cursor_position = true);

/// Call this function to tell the reader that a repaint is needed, and should be performed when
/// possible.
void reader_repaint_needed();

/// Call this function to tell the reader that some color has changed.
void reader_react_to_color_change();

/// Repaint immediately if needed.
void reader_repaint_if_needed();

/// Run the specified command with the correct terminal modes, and while taking care to perform job
/// notification, set the title, etc.
void reader_run_command(const wcstring &buff);

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
/// was set.
int reader_interrupted();

/// Clear the interrupted flag unconditionally without handling anything. The flag could have been
/// set e.g. when an interrupt arrived just as we were ending an earlier \c reader_readline
/// invocation but before the \c is_interactive_read flag was cleared.
void reader_reset_interrupted();

/// Return the value of the interrupted flag, which is set by the sigint handler, and clear it if it
/// was set. If the current reader is interruptible, call \c reader_exit().
int reader_reading_interrupted();

/// Returns true if the current reader generation count does not equal the generation count the
/// current thread was started with. Note 1: currently only valid for autocompletion threads! Other
/// threads don't set the threadlocal generation count when they start up.
bool reader_thread_job_is_stale();

/// Read one line of input. Before calling this function, reader_push() must have been called in
/// order to set up a valid reader environment. If nchars > 0, return after reading that many
/// characters even if a full line has not yet been read. Note: the returned value may be longer
/// than nchars if a single keypress resulted in multiple characters being inserted into the
/// commandline.
const wchar_t *reader_readline(int nchars);

/// Push a new reader environment.
void reader_push(const wcstring &name);

/// Return to previous reader environment.
void reader_pop();

/// Specify function to use for finding possible tab completions. The function must take these
/// arguments:
///
/// - The command to be completed as a null terminated array of wchar_t
/// - An array_list_t in which completions will be inserted.
typedef void (*complete_function_t)(const wcstring &, std::vector<completion_t> *,
                                    completion_request_flags_t);
void reader_set_complete_function(complete_function_t);

/// The type of a highlight function.
typedef void (*highlight_function_t)(const wcstring &, std::vector<highlight_spec_t> &, size_t,
                                     wcstring_list_t *, const env_vars_snapshot_t &vars);

/// Function type for testing if a string is valid for the reader to return.
using test_function_t = parser_test_error_bits_t (*)(const wcstring &);

/// Specify function for syntax highlighting. The function must take these arguments:
///
/// - The command to be highlighted as a null terminated array of wchar_t
/// - The color code of each character as an array of ints
/// - The cursor position
/// - An array_list_t used for storing error messages
void reader_set_highlight_function(highlight_function_t func);

/// Specify function for testing if the command buffer contains syntax errors that must be corrected
/// before returning.
void reader_set_test_function(test_function_t func);

/// Specify string of shell commands to be run in order to generate the prompt.
void reader_set_left_prompt(const wcstring &prompt);

/// Specify string of shell commands to be run in order to generate the right prompt.
void reader_set_right_prompt(const wcstring &prompt);

/// Sets whether autosuggesting is allowed.
void reader_set_allow_autosuggesting(bool flag);

/// Sets whether abbreviation expansion is performed.
void reader_set_expand_abbreviations(bool flag);

/// Sets whether the reader should exit on ^C.
void reader_set_exit_on_interrupt(bool flag);

void reader_set_silent_status(bool f);

/// Returns true if the shell is exiting, 0 otherwise.
bool shell_is_exiting();

/// The readers interrupt signal handler. Cancels all currently running blocks.
void reader_handle_sigint();

/// This function returns true if fish is exiting by force, i.e. because stdin died.
bool reader_exit_forced();

/// Test if the given shell command contains errors. Uses parser_test for testing. Suitable for
/// reader_set_test_function().
parser_test_error_bits_t reader_shell_test(const wcstring &);

/// Test whether the interactive reader is in search mode.
bool reader_is_in_search_mode();

/// Test whether the interactive reader has visible pager contents.
bool reader_has_pager_contents();

/// Given a command line and an autosuggestion, return the string that gets shown to the user.
/// Exposed for testing purposes only.
wcstring combine_command_and_autosuggestion(const wcstring &cmdline,
                                            const wcstring &autosuggestion);

/// Expand abbreviations at the given cursor position. Exposed for testing purposes only.
bool reader_expand_abbreviation_in_command(const wcstring &cmdline, size_t cursor_pos,
                                           wcstring *output);

/// Apply a completion string. Exposed for testing only.
wcstring completion_apply_to_command_line(const wcstring &val_str, complete_flags_t flags,
                                          const wcstring &command_line, size_t *inout_cursor_pos,
                                          bool append_only);

/// Print warning with list of backgrounded jobs
void reader_bg_job_warning();

/// Return the current interactive reads loop count. Useful for determining how many commands have
/// been executed between invocations of code.
uint32_t reader_run_count();

#endif
