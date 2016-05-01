// Functions for reading a character of input from stdin, using the inputrc information for key
// bindings.
#ifndef FISH_INPUT_H
#define FISH_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <vector>

#include "common.h"
#include "env.h"

#define DEFAULT_BIND_MODE L"default"
#define FISH_BIND_MODE_VAR L"fish_bind_mode"

wcstring describe_char(wint_t c);

/// Initialize the terminal by calling setupterm, and set up arrays used by readch to detect escape
/// sequences for special keys.
///
/// Before calling input_init, terminfo is not initialized and MUST not be used.
int input_init();

/// free up memory used by terminal functions.
void input_destroy();

/// Read a character from fd 0. Try to convert some escape sequences into character constants, but
/// do not permanently block the escape character.
///
/// This is performed in the same way vim does it, i.e. if an escape character is read, wait for
/// more input for a short time (a few milliseconds). If more input is avaialable, it is assumed to
/// be an escape sequence for a special character (such as an arrow key), and readch attempts to
/// parse it. If no more input follows after the escape key, it is assumed to be an actual escape
/// key press, and is returned as such.
///
/// The argument determines whether fish commands are allowed to be run as bindings. If false, when
/// a character is encountered that would invoke a fish command, it is unread and R_NULL is
/// returned.
wint_t input_readch(bool allow_commands = true);

/// Enqueue a character or a readline function to the queue of unread characters that input_readch
/// will return before actually reading from fd 0.
void input_queue_ch(wint_t ch);

/// Add a key mapping from the specified sequence to the specified command.
///
/// \param sequence the sequence to bind
/// \param command an input function that will be run whenever the key sequence occurs
void input_mapping_add(const wchar_t *sequence, const wchar_t *command,
                       const wchar_t *mode = DEFAULT_BIND_MODE,
                       const wchar_t *new_mode = DEFAULT_BIND_MODE);

void input_mapping_add(const wchar_t *sequence, const wchar_t *const *commands, size_t commands_len,
                       const wchar_t *mode = DEFAULT_BIND_MODE,
                       const wchar_t *new_mode = DEFAULT_BIND_MODE);

struct input_mapping_name_t {
    wcstring seq;
    wcstring mode;
};

/// Returns all mapping names and modes.
std::vector<input_mapping_name_t> input_mapping_get_names();

/// Erase binding for specified key sequence.
bool input_mapping_erase(const wcstring &sequence, const wcstring &mode = DEFAULT_BIND_MODE);

/// Gets the command bound to the specified key sequence in the specified mode. Returns true if it
/// exists, false if not.
bool input_mapping_get(const wcstring &sequence, const wcstring &mode, wcstring_list_t *out_cmds,
                       wcstring *out_new_mode);

/// Return the current bind mode.
wcstring input_get_bind_mode();

/// Set the current bind mode.
void input_set_bind_mode(const wcstring &bind_mode);

wchar_t input_function_pop_arg();

/// Sets the return status of the most recently executed input function.
void input_function_set_status(bool status);

/// Return the sequence for the terminfo variable of the specified name.
///
/// If no terminfo variable of the specified name could be found, return false and set errno to
/// ENOENT. If the terminfo variable does not have a value, return false and set errno to EILSEQ.
bool input_terminfo_get_sequence(const wchar_t *name, wcstring *out_seq);

/// Return the name of the terminfo variable with the specified sequence, in out_name. Returns true
/// if found, false if not found.
bool input_terminfo_get_name(const wcstring &seq, wcstring *out_name);

/// Return a list of all known terminfo names.
wcstring_list_t input_terminfo_get_names(bool skip_null);

/// Returns the input function code for the given input function name.
#define INPUT_CODE_NONE (wchar_t(-1))
wchar_t input_function_get_code(const wcstring &name);

/// Returns a list of all existing input function names.
wcstring_list_t input_function_get_names(void);

/// Updates our idea of whether we support term256 and term24bit.
void update_fish_color_support();

#endif
