/** \file builtin.h
  Prototypes for functions for executing builtin functions.
*/

#ifndef FISH_BUILTIN_H
#define FISH_BUILTIN_H

#include <stddef.h>                     // for size_t
#include <vector>                       // for vector

#include "io.h"
#include "common.h"

class completion_t;
class parser_t;

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
#define BUILTIN_ERR_UNKNOWN  _( L"%ls: Unknown option '%ls'\n" )

/**
   Error message for invalid character in variable name
*/
#define BUILTIN_ERR_VARCHAR _( L"%ls: Invalid character '%lc' in variable name. Only alphanumerical characters and underscores are valid in a variable name.\n" )

/**
   Error message for invalid (empty) variable name
*/
#define BUILTIN_ERR_VARNAME_ZERO _( L"%ls: Variable name can not be the empty string\n" )

/**
   Error message when too many arguments are supplied to a builtin
*/
#define BUILTIN_ERR_TOO_MANY_ARGUMENTS _( L"%ls: Too many arguments\n" )

#define BUILTIN_ERR_NOT_NUMBER _( L"%ls: Argument '%ls' is not a number\n" )

/** Get the string used to represent stdout and stderr */
const wcstring &get_stdout_buffer();
const wcstring &get_stderr_buffer();

/** Output an error */
void builtin_show_error(const wcstring &err);

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
int builtin_exists(const wcstring &cmd);

/**
  Execute a builtin command

  \param parser The parser being used
  \param argv Array containing the command and parameters
  of the builtin.  The list is terminated by a
  null pointer. This syntax resembles the syntax
  for exec.
  \param io the io redirections to perform on this builtin.

  \return the exit status of the builtin command
*/
int builtin_run(parser_t &parser, const wchar_t * const *argv, const io_chain_t &io);

/** Returns a list of all builtin names */
wcstring_list_t builtin_get_names();

/** Insert all builtin names into list. */
void builtin_get_names(std::vector<completion_t> *list);

/**
   Pushes a new set of input/output to the stack. The new stdin is supplied, a new set of output strings is created.
*/
void builtin_push_io(parser_t &parser, int stdin_fd);

/**
   Pops a set of input/output from the stack. The output strings are destroued, but the input file is not closed.
*/
void builtin_pop_io(parser_t &parser);


/**
   Return a one-line description of the specified builtin.
*/
wcstring builtin_get_desc(const wcstring &b);



/** Support for setting and removing transient command lines.
    This is used by 'complete -C' in order to make
    the commandline builtin operate on the string to complete instead
    of operating on whatever is to be completed. It's also used by
    completion wrappers, to allow a command to appear as the command
    being wrapped for the purposes of completion.
    
    Instantiating an instance of builtin_commandline_scoped_transient_t
    pushes the command as the new transient commandline. The destructor removes it.
    It will assert if construction/destruction does not happen in a stack-like (LIFO) order.
*/
class builtin_commandline_scoped_transient_t
{
    size_t token;
    public:
    builtin_commandline_scoped_transient_t(const wcstring &cmd);
    ~builtin_commandline_scoped_transient_t();
};


/**
   Run the __fish_print_help function to obtain the help information
   for the specified command.
*/
wcstring builtin_help_get(parser_t &parser, const wchar_t *cmd);

/** Defines a function, like builtin_function. Returns 0 on success. args should NOT contain 'function' as the first argument. */
int define_function(parser_t &parser, const wcstring_list_t &args, const wcstring &contents, int definition_line_offset, wcstring *out_err);


#endif
