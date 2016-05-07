// Header file for the low level input library.
#ifndef INPUT_COMMON_H
#define INPUT_COMMON_H

#include <stddef.h>

#include "common.h"

enum {
    R_MIN = INPUT_COMMON_BASE,
    // R_NULL is sometimes returned by the input when a character was requested but none could be
    // delivered, or when an exception happened.
    R_NULL = R_MIN,
    R_EOF,
    // Key codes for inputrc-style keyboard functions that are passed on to the caller of
    // input_read().
    //
    // NOTE: If you modify this sequence of symbols you must update the name_arr, code_arr and
    // desc_arr variables in input.cpp to match!
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
    R_AND,
    R_CANCEL,
    R_MAX = R_CANCEL,
    // This is a special psuedo-char that is not used other than to mark the end of the the special
    // characters so we can sanity check the enum range.
    R_SENTINAL
};

/// Init the library.
void input_common_init(int (*ih)());

/// Free memory used by the library.
void input_common_destroy();

/// Adjust the escape timeout.
void update_wait_on_escape_ms();

/// Set the escape timeout directly.
void set_wait_on_escape_ms(int ms);

/// Function used by input_readch to read bytes from stdin until enough bytes have been read to
/// convert them to a wchar_t. Conversion is done using mbrtowc. If a character has previously been
/// read and then 'unread' using \c input_common_unreadch, that character is returned. If timed is
/// true, readch2 will wait at most WAIT_ON_ESCAPE milliseconds for a character to be available for
/// reading before returning with the value WEOF.
wchar_t input_common_readch(int timed);

/// Enqueue a character or a readline function to the queue of unread characters that input_readch
/// will return before actually reading from fd 0.
void input_common_queue_ch(wint_t ch);

/// Add a character or a readline function to the front of the queue of unread characters.  This
/// will be the first character returned by input_readch (unless this function is called more than
/// once).
void input_common_next_ch(wint_t ch);

/// Adds a callback to be invoked at the next turn of the "event loop." The callback function will
/// be invoked and passed arg.
void input_common_add_callback(void (*callback)(void *), void *arg);

#endif
