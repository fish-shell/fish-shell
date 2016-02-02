/** \file input_common.h

Header file for the low level input library

*/
#ifndef INPUT_COMMON_H
#define INPUT_COMMON_H

#include <stddef.h>

/**
  Use unencoded private-use keycodes for internal characters
*/
#define INPUT_COMMON_RESERVED 0xe000

enum
{
    /**
       R_NULL is sometimes returned by the input when a character was
       requested but none could be delivered, or when an exception
       happened.
    */
    R_NULL = INPUT_COMMON_RESERVED,
    R_EOF
}
;

/**
   Init the library
*/
void input_common_init(int (*ih)());

/**
   Free memory used by the library
*/
void input_common_destroy();

// Adjust the escape timeout.
void update_wait_on_escape_ms();

/**
   Function used by input_readch to read bytes from stdin until enough
   bytes have been read to convert them to a wchar_t. Conversion is
   done using mbrtowc. If a character has previously been read and
   then 'unread' using \c input_common_unreadch, that character is
   returned. If timed is true, readch2 will wait at most
   WAIT_ON_ESCAPE milliseconds for a character to be available for
   reading before returning with the value WEOF.
*/
wchar_t input_common_readch(int timed);

/**
   Enqueue a character or a readline function to the queue of unread
   characters that input_readch will return before actually reading from fd
   0.
*/
void input_common_queue_ch(wint_t ch);

/**
   Add a character or a readline function to the front of the queue of unread
   characters.  This will be the first character returned by input_readch
   (unless this function is called more than once).
*/
void input_common_next_ch(wint_t ch);

/** Adds a callback to be invoked at the next turn of the "event loop." The callback function will be invoked and passed arg. */
void input_common_add_callback(void (*callback)(void *), void *arg);

#endif
