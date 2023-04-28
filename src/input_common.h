// Header file for the low level input library.
#ifndef INPUT_COMMON_H
#define INPUT_COMMON_H

#include <unistd.h>

#include <cstdint>
#include <deque>
#include <string>
#include <utility>

#include "common.h"
#include "env.h"
#include "maybe.h"

enum class readline_cmd_t {
    beginning_of_line,
    end_of_line,
    forward_char,
    backward_char,
    forward_single_char,
    forward_word,
    backward_word,
    forward_bigword,
    backward_bigword,
    nextd_or_forward_word,
    prevd_or_backward_word,
    history_search_backward,
    history_search_forward,
    history_prefix_search_backward,
    history_prefix_search_forward,
    history_pager,
    delete_char,
    backward_delete_char,
    kill_line,
    yank,
    yank_pop,
    complete,
    complete_and_search,
    pager_toggle_search,
    beginning_of_history,
    end_of_history,
    backward_kill_line,
    kill_whole_line,
    kill_inner_line,
    kill_word,
    kill_bigword,
    backward_kill_word,
    backward_kill_path_component,
    backward_kill_bigword,
    history_token_search_backward,
    history_token_search_forward,
    self_insert,
    self_insert_notfirst,
    transpose_chars,
    transpose_words,
    upcase_word,
    downcase_word,
    capitalize_word,
    togglecase_char,
    togglecase_selection,
    execute,
    beginning_of_buffer,
    end_of_buffer,
    repaint_mode,
    repaint,
    force_repaint,
    up_line,
    down_line,
    suppress_autosuggestion,
    accept_autosuggestion,
    begin_selection,
    swap_selection_start_stop,
    end_selection,
    kill_selection,
    insert_line_under,
    insert_line_over,
    forward_jump,
    backward_jump,
    forward_jump_till,
    backward_jump_till,
    func_and,
    func_or,
    expand_abbr,
    delete_or_exit,
    exit,
    cancel_commandline,
    cancel,
    undo,
    redo,
    begin_undo_group,
    end_undo_group,
    repeat_jump,
    disable_mouse_tracking,
    // NOTE: This one has to be last.
    reverse_repeat_jump
};

// The range of key codes for inputrc-style keyboard functions.
enum { R_END_INPUT_FUNCTIONS = static_cast<int>(readline_cmd_t::reverse_repeat_jump) + 1 };

/// Represents an event on the character input stream.
enum class char_event_type_t : uint8_t {
    /// A character was entered.
    charc,

    /// A readline event.
    readline,

    /// end-of-file was reached.
    eof,

    /// An event was handled internally, or an interrupt was received. Check to see if the reader
    /// loop should exit.
    check_exit,
};

/// Hackish: the input style, which describes how char events (only) are applied to the command
/// line. Note this is set only after applying bindings; it is not set from readb().
enum class char_input_style_t : uint8_t {
    // Insert characters normally.
    normal,

    // Insert characters only if the cursor is not at the beginning. Otherwise, discard them.
    notfirst,
};

class char_event_t {
    union {
        /// Set if the type is charc.
        wchar_t c;

        /// Set if the type is readline.
        readline_cmd_t rl;
    } v_{};

   public:
    /// The type of event.
    char_event_type_t type;

    /// The style to use when inserting characters into the command line.
    char_input_style_t input_style{char_input_style_t::normal};

    /// The sequence of characters in the input mapping which generated this event.
    /// Note that the generic self-insert case does not have any characters, so this would be empty.
    wcstring seq{};

    bool is_char() const { return type == char_event_type_t::charc; }

    bool is_eof() const { return type == char_event_type_t::eof; }

    bool is_check_exit() const { return type == char_event_type_t::check_exit; }

    bool is_readline() const { return type == char_event_type_t::readline; }

    wchar_t get_char() const {
        assert(type == char_event_type_t::charc && "Not a char type");
        return v_.c;
    }

    maybe_t<wchar_t> maybe_char() const {
        if (type == char_event_type_t::charc) {
            return v_.c;
        } else {
            return none();
        }
    }

    readline_cmd_t get_readline() const {
        assert(type == char_event_type_t::readline && "Not a readline type");
        return v_.rl;
    }

    /* implicit */ char_event_t(wchar_t c) : type(char_event_type_t::charc) { v_.c = c; }

    /* implicit */ char_event_t(readline_cmd_t rl, wcstring seq = {})
        : type(char_event_type_t::readline), seq(std::move(seq)) {
        v_.rl = rl;
    }

    /* implicit */ char_event_t(char_event_type_t type) : type(type) {
        assert(type != char_event_type_t::charc && type != char_event_type_t::readline &&
               "Cannot create a char event with this constructor");
    }
};

/// Adjust the escape timeout.
void update_wait_on_escape_ms(const environment_t &vars);

/// A class which knows how to produce a stream of input events.
/// This is a base class; you may subclass it for its override points.
class input_event_queue_t {
   public:
    /// Construct from a file descriptor \p in, and an interrupt handler \p handler.
    explicit input_event_queue_t(int in = STDIN_FILENO);

    /// Function used by input_readch to read bytes from stdin until enough bytes have been read to
    /// convert them to a wchar_t. Conversion is done using mbrtowc. If a character has previously
    /// been read and then 'unread' using \c input_common_unreadch, that character is returned.
    char_event_t readch();

    /// Like readch(), except it will wait at most WAIT_ON_ESCAPE milliseconds for a
    /// character to be available for reading.
    /// \return none on timeout, the event on success.
    maybe_t<char_event_t> readch_timed();

    /// Enqueue a character or a readline function to the queue of unread characters that
    /// readch will return before actually reading from fd 0.
    void push_back(const char_event_t &ch);

    /// Add a character or a readline function to the front of the queue of unread characters.  This
    /// will be the next character returned by readch.
    void push_front(const char_event_t &ch);

    /// Find the first sequence of non-char events, and promote them to the front.
    void promote_interruptions_to_front();

    /// Add multiple characters or readline events to the front of the queue of unread characters.
    /// The order of the provided events is not changed, i.e. they are not inserted in reverse
    /// order.
    template <typename Iterator>
    void insert_front(const Iterator begin, const Iterator end) {
        queue_.insert(queue_.begin(), begin, end);
    }

    /// Override point for when we are about to (potentially) block in select(). The default does
    /// nothing.
    virtual void prepare_to_select();

    /// Override point for when when select() is interrupted by a signal. The default does nothing.
    virtual void select_interrupted();

    /// Override point for when when select() is interrupted by the universal variable notifier.
    /// The default does nothing.
    virtual void uvar_change_notified();

    virtual ~input_event_queue_t();

   private:
    /// \return if we have any lookahead.
    bool has_lookahead() const { return !queue_.empty(); }

    /// \return the next event in the queue, or none if the queue is empty.
    maybe_t<char_event_t> try_pop();

    int in_{0};
    std::deque<char_event_t> queue_;
};

#endif
