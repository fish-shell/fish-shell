/** \file complete.h
	Prototypes for functions related to tab-completion.

	These functions are used for storing and retrieving tab-completion
	data, as well as for performing tab-completion.
*/

#ifndef FISH_COMPLETE_H

/**
   Header guard
*/
#define FISH_COMPLETE_H

#include <wchar.h>

#include "util.h"

/** 
	Use all completions 
*/
#define SHARED 0
/** 
	Do not use file completion 
*/
#define NO_FILES 1
/** 
	Require a parameter after completion 
*/
#define NO_COMMON 2
/** 
	Only use the argument list specifies with completion after
	option. This is the same as (NO_FILES & NO_COMMON)
*/
#define EXCLUSIVE 3

/** 
	Command is a path 
*/
#define PATH 1
/** 
	Command is not a path 
*/
#define COMMAND 0

/** 
	Separator between completion and description
*/
#define COMPLETE_SEP L'\004'

/** 
	Separator between completion and description
*/
#define COMPLETE_SEP_STR L"\004"

/**
   Separator between completion items in fish_pager. This is used for
   completion grouping, e.g. when putting completions with the same
   descriptions on the same line.
*/
#define COMPLETE_ITEM_SEP L'\uf500'

/**
   Character that separates the completion and description on
   programmable completions
*/
#define PROG_COMPLETE_SEP L'\t'

/**
   Do not insert space afterwards if this is the only completion. (The
   default is to try insert a space)
*/
#define COMPLETE_NO_SPACE 1

/**
   This compeltion is case insensitive. 

   Warning: The contents of the completion_t structure is actually
   different if this flag is set! Specifically, the completion string
   contains the _entire_ completion token, not only the current
*/
#define COMPLETE_NO_CASE 2

/**
   This compeltion is the whole argument, not just the remainder. This
   flag must never be set on completions returned from the complete()
   function. It is strictly for internal use in the completion code.
*/
#define COMPLETE_WHOLE_ARGUMENT 4

/**
   This completion may or may not want a space at the end - guess by
   checking the last character of the completion.
*/
#define COMPLETE_AUTO_SPACE 8

/**
   This completion should be inserted as-is, without escaping.
*/
#define COMPLETE_DONT_ESCAPE 16



typedef struct
{

	/**
	   The completion string
	*/
	const wchar_t *completion;

	/**
	   The description for this completion
	*/
	const wchar_t *description;

	/**
	   Flags determining the completion behaviour. 

	   Determines whether a space should be inserted after this
	   compeltion if it is the only possible completion using the
	   COMPLETE_NO_SPACE flag.

	   The COMPLETE_NO_CASE can be used to signal that this completion
	   is case insensitive.
	*/
	int flags;

}
	completion_t;


/**

  Add a completion. 

  All supplied values are copied, they should be freed by or otherwise
  disposed by the caller.

  Examples: 
  
  The command 'gcc -o' requires that a file follows it, so the
  NO_COMMON option is suitable. This can be done using the following
  line:
  
  complete -c gcc -s o -r

  The command 'grep -d' required that one of the strings 'read',
  'skip' or 'recurse' is used. As such, it is suitable to specify that
  a completion requires one of them. This can be done using the
  following line:

  complete -c grep -s d -x -a "read skip recurse"


  \param cmd Command to complete.
  \param cmd_type If cmd_type is PATH, cmd will be interpreted as the absolute
  path of the program (optionally containing wildcards), otherwise it
  will be interpreted as the command name.
  \param short_opt The single character name of an option. (-a is a short option, --all and  -funroll are long options)
  \param long_opt The multi character name of an option. (-a is a short option, --all and  -funroll are long options)
  \param long_mode Whether to use old style, single dash long options. 
  \param result_mode Whether to search further completions when this
  completion has been succesfully matched. If result_mode is SHARED,
  any other completions may also be used. If result_mode is NO_FILES,
  file completion should not be used, but other completions may be
  used. If result_mode is NO_COMMON, on option may follow it - only a
  parameter. If result_mode is EXCLUSIVE, no option may follow it, and
  file completion is not performed.
  \param comp A space separated list of completions which may contain subshells.
  \param desc A description of the completion.
  \param condition a command to be run to check it this completion should be used. If \c condition is empty, the completion is always used.
  \param flags A set of completion flags
*/
void complete_add( const wchar_t *cmd, 
		   int cmd_type, 
		   wchar_t short_opt,
		   const wchar_t *long_opt,
		   int long_mode, 
		   int result_mode, 
		   const wchar_t *condition,
		   const wchar_t *comp,
		   const wchar_t *desc,
		   int flags ); 
/**
  Sets whether the completion list for this command is complete. If
  true, any options not matching one of the provided options will be
  flagged as an error by syntax highlighting.
*/
void complete_set_authoritative( const wchar_t *cmd,
				 int cmd_type,
				 int authoritative );

/**
  Remove a previously defined completion
*/
void complete_remove( const wchar_t *cmd, 
		      int cmd_type, 
		      wchar_t short_opt,
		      const wchar_t *long_opt );

/**
  Find all completions of the command cmd, insert them into out. The
  caller must free the variables returned in out.  The results are
  returned in the array_list_t 'out', in the format of wide character
  strings, with each element consisting of a suggested completion and
  a description of what kind of object this completion represents,
  separated by a separator of type COMPLETE_SEP.

  Values returned by this function should be freed by the caller.
*/
void complete( const wchar_t *cmd, array_list_t *out );

/**
   Print a list of all current completions into the string_buffer_t. 

   \param out The string_buffer_t to write completions to
*/
void complete_print( string_buffer_t *out );

/**
   Tests if the specified option is defined for the specified command
*/
int complete_is_valid_option( const wchar_t *str, 
							  const wchar_t *opt, 
							  array_list_t *errors );

/**
   Tests if the specified argument is valid for the specified option
   and command
*/
int complete_is_valid_argument( const wchar_t *str, 
								const wchar_t *opt, 
								const wchar_t *arg );


/**
   Load command-specific completions for the specified command. This
   is done automatically whenever completing any given command, so
   there is no need to call this except in the case of completions
   with internal dependencies.

   \param cmd the command for which to load command-specific completions
   \param reload should the commands completions be reloaded, even if they where previously loaded. (This is set to true on actual completions, so that changed completion are updated in running shells)
*/
void complete_load( const wchar_t *cmd, int reload );

/**
   Create a new completion entry

   \param context The halloc context to use for allocating new memory
   \param comp The completion string
   \param desc The description of the completion
   \param flags completion flags
*/
void completion_allocate( array_list_t *context,
			  const wchar_t *comp,
			  const wchar_t *desc,
			  int flags );


#endif
