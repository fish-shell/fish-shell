/** \file builtin.h
	Prototypes for functions for executing builtin functions.
*/

#ifndef FISH_BUILTIN_H
#define FISH_BUILTIN_H

#include <wchar.h>

#include "util.h"
#include "io.h"

enum
{
	COMMAND_NOT_BUILTIN,
	BUILTIN_REGULAR,
	BUILTIN_FUNCTION
}
;

/**
   Error message on missing argument
*/
#define BUILTIN_ERR_MISSING _( L"%ls: Expected argument\n" )

/**
   Error message on invalid combination of options
*/
#define BUILTIN_ERR_COMBO _( L"%ls: Invalid combination of options\n" )

/**
   Error message on invalid combination of options
*/
#define BUILTIN_ERR_COMBO2 _( L"%ls: Invalid combination of options,\n%ls\n" )

/**
   Error message on multiple scope levels for variables
*/
#define BUILTIN_ERR_GLOCAL _( L"%ls: Variable scope can only be one of universal, global and local\n" )

/**
   Error message for specifying both export and unexport to set/read
*/
#define BUILTIN_ERR_EXPUNEXP _( L"%ls: Variable can't be both exported and unexported\n" )

/**
   Error message for unknown switch
*/
#define BUILTIN_ERR_UNKNOWN	_( L"%ls: Unknown option '%ls'\n" )

/**
   Error message for invalid character in variable name
*/
#define BUILTIN_ERR_VARCHAR _( L"%ls: Invalid character '%lc' in variable name. Only alphanumerical characters and underscores are valid in a variable name.\n" )

/**
   Error message for invalid (empty) variable name
*/
#define BUILTIN_ERR_VARNAME_ZERO _( L"%ls: Variable name can not be the empty string\n" )

/**
   Error message when second argument to for isn't 'in'
*/
#define BUILTIN_FOR_ERR_IN _( L"%ls: Second argument must be 'in'\n" )

/**
   Error message for insufficient number of arguments 
*/
#define BUILTIN_FOR_ERR_COUNT _( L"%ls: Expected at least two arguments, got %d\n")

#define BUILTIN_FOR_ERR_NAME _( L"%ls: '%ls' is not a valid variable name\n" )

/**
   Error message when too many arguments are supplied to a builtin
*/
#define BUILTIN_ERR_TOO_MANY_ARGUMENTS _( L"%ls: Too many arguments\n" )

/**
   Error message when block types mismatch in the end builtin, e.g. 'begin; end for'
*/
#define BUILTIN_END_BLOCK_MISMATCH _( L"%ls: Block mismatch: '%ls' vs. '%ls'\n" )

/**
   Error message for unknown block type in the end builtin, e.g. 'begin; end beggin'
*/
#define BUILTIN_END_BLOCK_UNKNOWN _( L"%ls: Unknown block type '%ls'\n" )

#define BUILTIN_ERR_NOT_NUMBER _( L"%ls: Argument '%ls' is not a number\n" )
/**
   Stringbuffer used to represent standard output
*/
extern string_buffer_t *sb_out;

/**
   Stringbuffer used to represent standard error
*/
extern string_buffer_t *sb_err;

/**
   Kludge. Tells builtins if output is to screen
*/
extern int builtin_out_redirect;

/**
   Kludge. Tells builtins if error is to screen
*/
extern int builtin_err_redirect;


/**
   Initialize builtin data. 
*/
void builtin_init();

/**
   Destroy builtin data.
*/
void builtin_destroy();

/**
  Is there a builtin command with the given name?
*/
int builtin_exists( wchar_t *cmd );

/**
  Execute a builtin command 

  \param argv Array containing the command and parameters 
  of the builtin.  The list is terminated by a
  null pointer. This syntax resembles the syntax 
  for exec.
  \param io the io redirections to perform on this builtin.

  \return the exit status of the builtin command
*/
int builtin_run( wchar_t **argv, io_data_t *io );

/**
  Insert all builtin names into l. These are not copies of the strings and should not be freed after use.
*/
void builtin_get_names( array_list_t *list );

/**
   Pushes a new set of input/output to the stack. The new stdin is supplied, a new set of output string_buffer_ts is created.
*/
void builtin_push_io( int stdin_fd );

/**
   Pops a set of input/output from the stack. The output string_buffer_ts are destroued, but the input file is not closed.
*/
void builtin_pop_io();


/**
   Return a one-line description of the specified builtin
*/
const wchar_t *builtin_get_desc( const wchar_t *b );


/**
   Slightly kludgy function used with 'complete -C' in order to make
   the commandline builtin operate on the string to complete instead
   of operating on whatever is to be completed.
*/
const wchar_t *builtin_complete_get_temporary_buffer();


/**
   Run the __fish_print_help function to obtain the help information
   for the specified command. The resulting string will be valid until
   the next time this function is called, and must never be free'd manually.
*/

wchar_t *builtin_help_get( const wchar_t *cmd );

#endif
