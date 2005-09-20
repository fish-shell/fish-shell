/** \file input_common.h
	
Header file for the low level input library

*/
#ifndef INPUT_COMMON_HH
#define INPUT_COMMON_HH

/** 
	Hopefully, the biggest value that a wchar_t can have. UCS4 is a
	31-bit character set, we use the upper half for special key
	sequences. On systems where wchar_t is not a 31 (or less) bit character set
	in a 32 (or more) bit type this will fail horribly. 
*/
#define WCHAR_END 0x80000000

enum
{
	R_NULL = WCHAR_END + 1
}
	;

void input_common_init( int (*ih)() );

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

void input_common_unreadch( wint_t ch );

#endif
