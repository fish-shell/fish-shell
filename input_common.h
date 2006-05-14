/** \file input_common.h
	
Header file for the low level input library

*/
#ifndef INPUT_COMMON_H
#define INPUT_COMMON_H

#include <wchar.h>

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
void input_common_init( int (*ih)() );

/**
   Free memory used by the library
*/
void input_common_destroy();

/**
   Function used by input_readch to read bytes from stdin until enough
   bytes have been read to convert them to a wchar_t. Conversion is
   done using mbrtowc. If a character has previously been read and
   then 'unread' using \c input_common_unreadch, that character is
   returned. If timed is true, readch2 will wait at most
   WAIT_ON_ESCAPE milliseconds for a character to be available for
   reading before returning with the value WEOF.
*/
wchar_t input_common_readch( int timed );

/**
   Push a character or a readline function onto the stack of unread
   characters that input_readch will return before actually reading from fd
   0.
*/
void input_common_unreadch( wint_t ch );

#endif
