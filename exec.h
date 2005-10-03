/** \file exec.h
	Prototypes for functions for executing a program
*/

/**
   Initialize the exec library
*/
void exec_init();

/*
  Destroy dynamically allocated data and other resources used by the
  exec library
*/
void exec_destroy();

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
int exec_subshell( const wchar_t *cmd, 
				   array_list_t *l );

/**
   Free all resources used by a IO_BUFFER type io redirection.
*/
void exec_free_io_buffer( io_data_t *io_buffer );

/**
   Create a IO_BUFFER type io redirection.
*/
io_data_t *exec_make_io_buffer();

/**
   Close writing end of IO_BUFFER type io redirection, and fully read the reading end.
*/
void exec_read_io_buffer( io_data_t *d );
