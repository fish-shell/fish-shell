/** \file proc.h 

    Prototypes for utilities for keeping track of jobs, processes and subshells, as
	well as signal handling functions for tracking children. These
	functions do not themselves launch new processes, the exec library
	will call proc to create representations of the running jobs as
	needed.
	
*/

#ifndef FISH_PROC_H
#define FISH_PROC_H

#include <wchar.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#include "util.h"
#include "io.h"

/**
   The status code use when a command was not found
*/
#define STATUS_UNKNOWN_COMMAND 127

/**
   The status code use when an unknown error occured during execution of a command
*/
#define STATUS_NOT_EXECUTABLE 126

/**
   The status code use when an unknown error occured during execution of a command
*/
#define STATUS_EXEC_FAIL 125

/**
   The status code use when a wildcard had no matches
*/
#define STATUS_UNMATCHED_WILDCARD 124

/**
   The status code used for normal exit in a  builtin
*/
#define STATUS_BUILTIN_OK 0

/**
   The status code used for erroneous argument combinations in a builtin
*/
#define STATUS_BUILTIN_ERROR 1

/**
   Types of processes
*/
enum
{
	/**
	   A regular external command
	*/
	EXTERNAL,
	/**
	   A builtin command
	*/
	INTERNAL_BUILTIN,
	/**
	   A shellscript function
	*/
	INTERNAL_FUNCTION,
	/**
	   A block of commands
	*/
	INTERNAL_BLOCK,
	/**
	   The exec builtin
	*/
	INTERNAL_EXEC,
	/**
	   A buffer
	*/
	INTERNAL_BUFFER,
      
}
	;

enum
{
	JOB_CONTROL_ALL, 
	JOB_CONTROL_INTERACTIVE,
	JOB_CONTROL_NONE,
}
	;

/** 
	A structure representing a single fish process. Contains variables
	for tracking process state and the process argument
	list. Actually, a fish process can be either a regular externa
	lrocess, an internal builtin which may or may not spawn a fake IO
	process during execution, a shellscript function or a block of
	commands to be evaluated by calling eval. Lastly, this process can
	be the result of an exec command. The role of this process_t is
	determined by the type field, which can be one of EXTERNAL,
	INTERNAL_BUILTIN, INTERNAL_FUNCTION, INTERNAL_BLOCK and
	INTERNAL_EXEC, INTERNAL_BUFFER

	The process_t contains information on how the process should be
	started, such as command name and arguments, as well as runtime
	information on the status of the actual physical process which
	represents it. Shellscript functions, builtins and blocks of code
	may all need to spawn an external process that handles the piping
	and redirecting of IO for them.

	If the process is of type EXTERNAL or INTERNAL_EXEC, argv is the
	argument array and actual_cmd is the absolute path of the command
	to execute.

	If the process is of type INTERNAL_BUILTIN, argv is the argument
	vector, and argv[0] is the name of the builtin command.

	If the process is of type INTERNAL_FUNCTION, argv is the argument
	vector, and argv[0] is the name of the shellscript function.

	If the process is of type INTERNAL_BLOCK, argv has exactly one
	element, which is the block of commands to execute.

*/
typedef struct process
{
	/** 
		Type of process. Can be one of \c EXTERNAL, \c
		INTERNAL_BUILTIN, \c INTERNAL_FUNCTION, \c INTERNAL_BLOCK,
		INTERNAL_EXEC, or INTERNAL_BUFFER
	*/
	int type;

	/** argv parameter for for execv, builtin_run, etc. */
	wchar_t **argv;

	/** actual command to pass to exec in case of EXTERNAL or INTERNAL_EXEC */
	wchar_t *actual_cmd;       

	/** process ID */
	pid_t pid;

	/** File descriptor that pipe output should bind to */
	int pipe_write_fd;

	/** File descriptor that the _next_ process pipe input should bind to */
	int pipe_read_fd;

	/** true if process has completed */
	volatile int completed;

	/** true if process has stopped */
	volatile int stopped;

	/** reported status value */
	volatile int status;

	/** Special flag to tell the evaluation function for count to print the help information */
	int count_help_magic;

	/** next process in pipeline */
	struct process *next;       	
#ifdef HAVE__PROC_SELF_STAT
	/** Last time of cpu time check */
	struct timeval last_time;
	/** Number of jiffies spent in process at last cpu time check */
	unsigned long last_jiffies;	
#endif
} 
	process_t;

/** 
	Constant for the flag variable in the job struct

	true if user was told about stopped job 
*/
#define JOB_NOTIFIED 1
/** 
	Constant for the flag variable in the job struct

	Whether this job is in the foreground 
*/
#define JOB_FOREGROUND 2
/** 
	Constant for the flag variable in the job struct

	Whether the specified job is completely constructed,
	i.e. completely parsed, and every process in the job has been
	forked, etc.
*/
#define JOB_CONSTRUCTED 4
/**
	Constant for the flag variable in the job struct

   Whether the specified job is a part of a subshell, event handler or some other form of special job that should not be reported
*/
#define JOB_SKIP_NOTIFICATION 8
/** 
	Constant for the flag variable in the job struct

	Should the exit status be negated? This flag can only be set by the not builtin. 
*/
#define JOB_NEGATE 16
/** 
	Constant for the flag variable in the job struct

	This flag is set to one on wildcard expansion errors. It means that the current command should not be executed 
*/
#define JOB_WILDCARD_ERROR 32

/** 
	Constant for the flag variable in the job struct

	Skip executing this job. This flag is set by the short-circut builtins, i.e. and and or 
*/
#define JOB_SKIP 64

/** 
	Constant for the flag variable in the job struct

	Whether the job is under job control 
*/
#define JOB_CONTROL 128
/** 
	Constant for the flag variable in the job struct

	Whether the job wants to own the terminal when in the foreground 
*/
#define JOB_TERMINAL 256

/** 
    A struct represeting a job. A job is basically a pipeline of one
    or more processes and a couple of flags.
 */
typedef struct job
{
	/** 
	    The original command which led to the creation of this
	    job. It is used for displaying messages about job status
	    on the terminal.
	*/
	wchar_t *command;              
	
	/** 
	    A linked list of all the processes in this job.
	*/
	process_t *first_process;  
	
	/** 
	    process group ID for the process group that this job is
	    running in. 
	*/
	pid_t pgid;   
	
	/** 
	    The saved terminal modes of this job. This needs to be
	    saved so that we can restore the terminal to the same
	    state after temporarily taking control over the terminal
	    when a job stops. 
	*/
	struct termios tmodes;
    
	/**
	   The job id of the job. This is a small integer that is a
	   unique identifier of the job within this shell, and is
	   used e.g. in process expansion.
	*/
	int job_id;
	
	/**
	   List of all IO redirections for this job 
	*/
	io_data_t *io;
			
	/** 
	    A pointer to the next job in the job queue 
	*/
	struct job *next;           

	/**
	   Bitset containing information about the job. A combination of the JOB_* constants.
	*/
	int flags;
	
} 
	job_t;

/** 
	Whether we are running a subshell command 
*/
extern int is_subshell;

/** 
	Whether we are running a block of commands 
*/
extern int is_block;

/** 
	Whether we are reading from the keyboard right now
*/
extern int is_interactive;

/** 
	Whether this shell is attached to the keyboard at all
*/
extern int is_interactive_session;

/** 
	Whether we are a login shell
*/
extern int is_login;

/** 
	Whether we are running an event handler
*/
extern int is_event;

/** 
	Linked list of all living jobs 
*/
extern job_t *first_job;   

/**
   Whether a universal variable barrier roundtrip has already been
   made for the currently executing command. Such a roundtrip only
   needs to be done once on a given command, unless a universal
   variable value is changed. Once this has been done, this variable
   is set to 1, so that no more roundtrips need to be done.

   Both setting it to one when it should be zero and the opposite may
   cause concurrency bugs.
*/
extern int proc_had_barrier;

/**
   Pid of last process to be started in the background
*/
extern pid_t proc_last_bg_pid;

/**
   The current job control mode.

   Must be one of JOB_CONTROL_ALL, JOB_CONTROL_INTERACTIVE and JOB_CONTROL_NONE
*/
extern int job_control_mode;

/**
   If this flag is set, fish will never fork or run execve. It is used
   to put fish into a syntax verifier mode where fish tries to validate
   the syntax of a file but doesn't actually do anything.
  */
extern int no_exec;

/**
   Add the specified flag to the bitset of flags for the specified job
 */
void job_set_flag( job_t *j, int flag, int set );

/**
   Returns one if the specified flag is set in the specified job, 0 otherwise.
 */
int job_get_flag( job_t *j, int flag );

/**
   Sets the status of the last process to exit
*/
void proc_set_last_status( int s );

/**
   Returns the status of the last process to exit
*/
int proc_get_last_status();

/**
   Remove the specified job
*/
void job_free( job_t* j );

/**
   Create a new job. Job struct is allocated using halloc, so anything
   that should be freed with the job can uset it as a halloc context
   when allocating.
*/
job_t *job_create();

/**
  Return the job with the specified job id.
  If id is 0 or less, return the last job used.
*/
job_t *job_get(int id);

/**
  Return the job with the specified pid.
*/
job_t *job_get_from_pid(int pid);

/**
   Tests if the job is stopped 
 */
int job_is_stopped( const job_t *j );

/**
   Tests if the job has completed, i.e. if the last process of the pipeline has ended.
 */
int job_is_completed( const job_t *j );

/**
  Reassume a (possibly) stopped job. Put job j in the foreground.  If
  cont is nonzero, restore the saved terminal modes and send the
  process group a SIGCONT signal to wake it up before we block.

  \param j The job
  \param cont Whether the function should wait for the job to complete before returning
*/
void job_continue( job_t *j, int cont );

/**
   Notify the user about stopped or terminated jobs. Delete terminated
   jobs from the job list.

   \param interactive whether interactive jobs should be reaped as well
*/
int job_reap( int interactive );

/**
   Signal handler for SIGCHLD. Mark any processes with relevant
   information.
*/
void job_handle_signal( int signal, siginfo_t *info, void *con );

/**
   Send the specified signal to all processes in the specified job.
*/
int job_signal( job_t *j, int signal );

#ifdef HAVE__PROC_SELF_STAT
/**
   Use the procfs filesystem to look up how many jiffies of cpu time
   was used by this process. This function is only available on
   systems with the procfs file entry 'stat', i.e. Linux.
*/
unsigned long proc_get_jiffies( process_t *p );

/**
   Update process time usage for all processes by calling the
   proc_get_jiffies function for every process of every job.
*/
void proc_update_jiffies();

#endif

/**
   Perform a set of simple sanity checks on the job list. This
   includes making sure that only one job is in the foreground, that
   every process is in a valid state, etc.
*/
void proc_sanity_check();

/**
   Send a process/job exit event notification. This function is a
   conveniance wrapper around event_fire().
*/
void proc_fire_event( const wchar_t *msg, int type, pid_t pid, int status );

/**
  Initializations
*/
void proc_init();

/**
   Clean up before exiting
*/
void proc_destroy();

/**
   Set new value for is_interactive flag, saving previous value. If
   needed, update signal handlers.
*/
void proc_push_interactive( int value );

/**
   Set is_interactive flag to the previous value. If needed, update
   signal handlers.
*/
void proc_pop_interactive();

/**
   Format an exit status code as returned by e.g. wait into a fish exit code number as accepted by proc_set_last_status.
 */
int proc_format_status(int status) ;


#endif
