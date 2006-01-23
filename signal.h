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

/**
   Set all signal handlers to SIG_DFL
*/
void signal_reset_handlers();

/**
   Set signal handlers to fish default handlers
*/
void signal_set_handlers();

/**
   Tell fish what to do on the specified signal.

   \param sig The signal to specify the action of
   \param do_handle If true fish will catch the specified signal and fire an event, otherwise the default action (SIG_DFL) will be set
*/
void signal_handle( int sig, int do_handle );

/**
  Block all signals
*/
void signal_block();

/**
   Unblock all signals
*/
void signal_unblock();
