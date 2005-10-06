/** \file signal.h

The library for various signal related issues

*/

/**
   Get the integer signal value representing the specified signal, or
   -1 of no signal was found
*/
int wcs2sig( const wchar_t *str );

/**
   Get string representation of a signal
*/
const wchar_t *sig2wcs( int sig );

/**
   Returns a description of the specified signal.
*/
const wchar_t *sig_description( int sig );

void signal_set_handlers();
