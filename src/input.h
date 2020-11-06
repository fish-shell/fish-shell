// Functions for reading a character of input from stdin, using the inputrc information for key
// bindings.
#ifndef FISH_INPUT_H
#define FISH_INPUT_H

#include <stddef.h>

#include <vector>

#include "common.h"
#include "input_common.h"

#define FISH_BIND_MODE_VAR L"fish_bind_mode"
#define DEFAULT_BIND_MODE L"default"

class parser_t;

wcstring describe_char(wint_t c);

/// Set up arrays used by readch to detect escape sequences for special keys and perform related
/// initializations for our input subsystem.
void init_input();

struct input_mapping_t;
class inputter_t {
    input_event_queue_t event_queue_;
    std::vector<wchar_t> input_function_args_{};
    bool function_status_{false};

    // We need a parser to evaluate bindings.
    const std::shared_ptr<parser_t> parser_;

    void function_push_arg(wchar_t arg);
    void function_push_args(readline_cmd_t code);
    void mapping_execute(const input_mapping_t &m, bool allow_commands);
    void mapping_execute_matching_or_generic(bool allow_commands);
    bool mapping_is_match(const input_mapping_t &m);
    maybe_t<input_mapping_t> find_mapping();
    char_event_t read_characters_no_readline();

   public:
    inputter_t(parser_t &parser, int in = 0);

    /// Read a character from stdin. Try to convert some escape sequences into character constants,
    /// but do not permanently block the escape character.
    ///
    /// This is performed in the same way vim does it, i.e. if an escape character is read, wait for
    /// more input for a short time (a few milliseconds). If more input is avaialable, it is assumed
    /// to be an escape sequence for a special character (such as an arrow key), and readch attempts
    /// to parse it. If no more input follows after the escape key, it is assumed to be an actual
    /// escape key press, and is returned as such.
    ///
    /// The argument determines whether fish commands are allowed to be run as bindings. If false,
    /// when a character is encountered that would invoke a fish command, it is unread and
    /// char_event_type_t::check_exit is returned.
    char_event_t readch(bool allow_commands = true);

    /// Enqueue a char event to the queue of unread characters that input_readch will return before
    /// actually reading from fd 0.
    void queue_ch(const char_event_t &ch);

    /// Enqueue a char event to the front of the queue; this will be the next event returned.
    void push_front(const char_event_t &ch);

    /// Sets the return status of the most recently executed input function.
    void function_set_status(bool status) { function_status_ = status; }

    /// Pop an argument from the function argument stack.
    wchar_t function_pop_arg();
};

struct input_mapping_name_t {
    wcstring seq;
    wcstring mode;
};

/// The input mapping set is the set of mappings from character sequences to commands.
class input_mapping_set_t {
    friend acquired_lock<input_mapping_set_t> input_mappings();
    friend void init_input();

    using mapping_list_t = std::vector<input_mapping_t>;

    mapping_list_t mapping_list_;
    mapping_list_t preset_mapping_list_;
    std::shared_ptr<const mapping_list_t> all_mappings_cache_;

    input_mapping_set_t();

   public:
    ~input_mapping_set_t();

    /// Erase all bindings.
    void clear(const wchar_t *mode = nullptr, bool user = true);

    /// Erase binding for specified key sequence.
    bool erase(const wcstring &sequence, const wcstring &mode = DEFAULT_BIND_MODE,
               bool user = true);

    /// Gets the command bound to the specified key sequence in the specified mode. Returns true if
    /// it exists, false if not.
    bool get(const wcstring &sequence, const wcstring &mode, wcstring_list_t *out_cmds, bool user,
             wcstring *out_sets_mode) const;

    /// Returns all mapping names and modes.
    std::vector<input_mapping_name_t> get_names(bool user = true) const;

    /// Add a key mapping from the specified sequence to the specified command.
    ///
    /// \param sequence the sequence to bind
    /// \param command an input function that will be run whenever the key sequence occurs
    void add(wcstring sequence, const wchar_t *command, const wchar_t *mode = DEFAULT_BIND_MODE,
             const wchar_t *sets_mode = DEFAULT_BIND_MODE, bool user = true);

    void add(wcstring sequence, const wchar_t *const *commands, size_t commands_len,
             const wchar_t *mode = DEFAULT_BIND_MODE, const wchar_t *sets_mode = DEFAULT_BIND_MODE,
             bool user = true);

    /// \return a snapshot of the list of input mappings.
    std::shared_ptr<const mapping_list_t> all_mappings();
};

/// Access the singleton input mapping set.
acquired_lock<input_mapping_set_t> input_mappings();

/// Return the sequence for the terminfo variable of the specified name.
///
/// If no terminfo variable of the specified name could be found, return false and set errno to
/// ENOENT. If the terminfo variable does not have a value, return false and set errno to EILSEQ.
bool input_terminfo_get_sequence(const wcstring &name, wcstring *out_seq);

/// Return the name of the terminfo variable with the specified sequence, in out_name. Returns true
/// if found, false if not found.
bool input_terminfo_get_name(const wcstring &seq, wcstring *out_name);

/// Return a list of all known terminfo names.
wcstring_list_t input_terminfo_get_names(bool skip_null);

/// Returns the input function code for the given input function name.
maybe_t<readline_cmd_t> input_function_get_code(const wcstring &name);

/// Returns a list of all existing input function names.
wcstring_list_t input_function_get_names(void);

#endif
