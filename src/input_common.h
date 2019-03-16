// Header file for the low level input library.
#ifndef INPUT_COMMON_H
#define INPUT_COMMON_H

#include <stddef.h>

#include <functional>

#include "common.h"
#include "maybe.h"

enum {
    R_MIN = INPUT_COMMON_BASE,
    // R_NULL is sometimes returned by the input when a character was requested but none could be
    // delivered, or when an exception happened.
    R_NULL = R_MIN,

    R_BEGINNING_OF_LINE,
    R_END_OF_LINE,
    R_FORWARD_CHAR,
    R_BACKWARD_CHAR,
    R_FORWARD_WORD,
    R_BACKWARD_WORD,
    R_FORWARD_BIGWORD,
    R_BACKWARD_BIGWORD,
    R_HISTORY_SEARCH_BACKWARD,
    R_HISTORY_SEARCH_FORWARD,
    R_DELETE_CHAR,
    R_BACKWARD_DELETE_CHAR,
    R_KILL_LINE,
    R_YANK,
    R_YANK_POP,
    R_COMPLETE,
    R_COMPLETE_AND_SEARCH,
    R_PAGER_TOGGLE_SEARCH,
    R_BEGINNING_OF_HISTORY,
    R_END_OF_HISTORY,
    R_BACKWARD_KILL_LINE,
    R_KILL_WHOLE_LINE,
    R_KILL_WORD,
    R_KILL_BIGWORD,
    R_BACKWARD_KILL_WORD,
    R_BACKWARD_KILL_PATH_COMPONENT,
    R_BACKWARD_KILL_BIGWORD,
    R_HISTORY_TOKEN_SEARCH_BACKWARD,
    R_HISTORY_TOKEN_SEARCH_FORWARD,
    R_SELF_INSERT,
    R_TRANSPOSE_CHARS,
    R_TRANSPOSE_WORDS,
    R_UPCASE_WORD,
    R_DOWNCASE_WORD,
    R_CAPITALIZE_WORD,
    R_VI_ARG_DIGIT,
    R_VI_DELETE_TO,
    R_EXECUTE,
    R_BEGINNING_OF_BUFFER,
    R_END_OF_BUFFER,
    R_REPAINT,
    R_FORCE_REPAINT,
    R_UP_LINE,
    R_DOWN_LINE,
    R_SUPPRESS_AUTOSUGGESTION,
    R_ACCEPT_AUTOSUGGESTION,
    R_BEGIN_SELECTION,
    R_SWAP_SELECTION_START_STOP,
    R_END_SELECTION,
    R_KILL_SELECTION,
    R_FORWARD_JUMP,
    R_BACKWARD_JUMP,
    R_FORWARD_JUMP_TILL,
    R_BACKWARD_JUMP_TILL,
    R_AND,
    R_CANCEL,
    R_REPEAT_JUMP,
    R_REVERSE_REPEAT_JUMP,

    // The range of key codes for inputrc-style keyboard functions that are passed on to the caller
    // of input_read().
    R_BEGIN_INPUT_FUNCTIONS = R_BEGINNING_OF_LINE,
    R_END_INPUT_FUNCTIONS = R_REVERSE_REPEAT_JUMP + 1
};

/// Represents an event on the character input stream.
enum class char_event_type_t {
    /// A character was entered.
    charc,

    /// A timeout was hit.
    timeout,

    /// end-of-file was reached.
    eof
};

class char_event_t {
    /// Set if the type is charc.
    wchar_t c_;

   public:
    char_event_type_t type;

    bool is_timeout() const { return type == char_event_type_t::timeout; }

    bool is_char() const { return type == char_event_type_t::charc; }

    bool is_eof() const { return type == char_event_type_t::eof; }

    bool is_readline() const {
        return is_char() && c_ >= R_BEGIN_INPUT_FUNCTIONS && c_ < R_END_INPUT_FUNCTIONS;
    }

    wchar_t get_char() const {
        assert(type == char_event_type_t::charc && "Not a char type");
        return c_;
    }

    /* implicit */ char_event_t(wchar_t c) : c_(c), type(char_event_type_t::charc) {}

    /* implicit */ char_event_t(char_event_type_t type) : c_(0), type(type) {
        assert(type != char_event_type_t::charc &&
               "Cannot create a char event with this constructor");
    }
};

/// Init the library.
void input_common_init(int (*ih)());

/// Adjust the escape timeout.
class environment_t;
void update_wait_on_escape_ms(const environment_t &vars);

/// Function used by input_readch to read bytes from stdin until enough bytes have been read to
/// convert them to a wchar_t. Conversion is done using mbrtowc. If a character has previously been
/// read and then 'unread' using \c input_common_unreadch, that character is returned.
/// This function never returns a timeout.
char_event_t input_common_readch();

/// Like input_common_readch(), except it will wait at most WAIT_ON_ESCAPE milliseconds for a
/// character to be available for reading.
/// If \p dequeue_timeouts is set, remove any timeout from the queue; otherwise retain them.
char_event_t input_common_readch_timed(bool dequeue_timeouts = false);

/// Enqueue a character or a readline function to the queue of unread characters that input_readch
/// will return before actually reading from fd 0.
void input_common_queue_ch(char_event_t ch);

/// Add a character or a readline function to the front of the queue of unread characters.  This
/// will be the first character returned by input_readch (unless this function is called more than
/// once).
void input_common_next_ch(char_event_t ch);

/// Adds a callback to be invoked at the next turn of the "event loop." The callback function will
/// be invoked and passed arg.
void input_common_add_callback(std::function<void(void)>);

#endif
