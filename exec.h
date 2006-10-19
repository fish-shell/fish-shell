/** \file exec.h
	Prototypes for functions for executing a program
*/

#ifndef FISH_EXEC_H
/**
   Header guard
*/
#define FISH_EXEC_H

#include <wchar.h>

#include "proc.h"
#include "util.h"

/**
   pipe redirection error message
*/
#define PIPE_ERROR _(L"An error occurred while setting up pipe")

/**
  Execute the processes specified by j. 

   I've put a fair bit of work into making builtins behave like other
   programs as far as pipes are concerned. Unlike i.e. bash, builtins
   can pipe to other builtins with arbitrary amounts of data, and so
   on. To do this, after a builtin is run in the real process, it
   forks and a dummy process is created, responsible for writing the
   output of the builtin. This is surprisingly cheap on my computer,
   probably because of the marvels of copy on write forking.

   This rule is short circuted in the case where a builtin does not
   output to a pipe and does in fact not output anything. The speed
   improvement from this optimization is not noticable on a normal
   computer/OS in regular use, but the promiscous amounts of forking
   that resulted was responsible for a huge slowdown when using
   Valgrind as well as when doing complex command-specific
   completions.


*/
void exec( job_t *j );

/**
  Evaluate the expression cmd in a subshell, add the outputs into the
  list l. On return, the status flag as returned bu \c
  proc_gfet_last_status will not be changed.

  \param cmd the command to execute
  \param l The list to insert output into.If \c l is zero, the output will be discarded.

  \return the status of the last job to exit, or -1 if en error was encountered.
*/
__warn_unused int exec_subshell( const wchar_t *cmd, 
								 array_list_t *l );


/**
   Loops over close until thesyscall was run without beeing
   interrupted. Thenremoves the fd from the open_fds list.
*/
void exec_close( int fd );

/**
   Call pipe(), and add resulting fds to open_fds, the list of opend
   file descriptors for pipes.
*/
int exec_pipe( int fd[2]);

#endif
