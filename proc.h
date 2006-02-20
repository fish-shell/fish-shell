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
	INTERNAL_EXEC.

	The process_t contains information on how the process should be
	started, such as command name and arguments, as well as runtime
	information on the status of the actual physical process which
	represents it. Shellscript functions, builtins and blocks of code
	may all need to spawn an external process that handles the piping
	and redirecting of IO for them.

	If the process is of type EXTERNAL or INTERNAL_EXEC, argv is the
	argument array and actual_cmd is the absolute path of the command
	to execute.

	If the process is of type ITERNAL_BUILTIN, argv is the argument
	vector, and argv[0] is the name of the builtin command.

	If the process is of type ITERNAL_FUNCTION, argv is the argument
	vector, and argv[0] is the name of the shellscript function.

	If the process is of type ITERNAL_BLOCK, argv has exactly one
	element, which is the block of commands to execute.

*/
typedef struct process
{
	/** 
		Type of process. Can be one of \c EXTERNAL, \c
		INTERNAL_BUILTIN, \c INTERNAL_FUNCTION, \c INTERNAL_BLOCK or
		INTERNAL_EXEC
	*/
	int type;
	/** argv parameter for for execv, builtin_run, etc. */
	wchar_t **argv;
	/** actual command to pass to exec in case of EXTERNAL or INTERNAL_EXEC */
	wchar_t *actual_cmd;       
	/** process ID */
	pid_t pid;
	/** File descriptor that pipe output should bind to */
	int pipe_fd;
	/** true if process has completed */
	volatile int completed;
	/** true if process has stopped */
	volatile int stopped;
	/** reported status value */
	volatile int status;
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


/** A pipeline of one or more processes. */
typedef struct job
{
	/** command line, used for messages */
	wchar_t *command;              
	/** list of processes in this job */
	process_t *first_process;  
	/** process group ID */
	pid_t pgid;   
	/** true if user was told about stopped job */
	int notified;     
	/** saved terminal modes */
	struct termios tmodes;    
	/** The job id of the job*/
	int job_id;
	/** Whether this job is in the foreground */
	int fg;
	/** 
		Whether the specified job is completely constructed,
		i.e. completely parsed, and every process in the job has been
		forked, etc.
	*/
	int constructed;
	/**
	   Whether the specified job is a part of a subshell, event handler or some other form of special job that should not be reported
	*/
	int skip_notification;
	
	/** List of IO redrections for the job */
	io_data_t *io;
	
	/** Should the exit status be negated? This flag can only be set by the not builtin. */
	int negate;	

	/** This flag is set to one on wildcard expansion errors. It means that the current command should not be executed */
	int wildcard_error;
	
	/** Skip executing this job. This flag is set by the short-circut builtins, i.e. and and or */
	int skip;

	/** Whether the job is under job control */
	int job_control;
			
	/** Whether the job wants to own the terminal when in the foreground */
	int terminal;
			
	/** Pointer to the next job */
	struct job *next;           
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
	Whether we are a event handler
*/
extern int is_event;

/** 
	Linked list of all jobs 
*/
extern job_t *first_job;   

/**
   Whether a universal variable barrier roundtrip has already been
   made for this command. Such a roundtrip only needs to be done once
   on a given command, unless a unversal variable value is
   changed. Once this has been done, this variable is set to 1, so
   that no more roundtrips need to be done.

   Both setting it to one when it should be zero and the opposite may
   cause concurrency bugs.
*/
extern int proc_had_barrier;

/**
   Pid of last process to be started in the background
*/
extern pid_t proc_last_bg_pid;

/**
   Can be one of JOB_CONTROL_ALL, JOB_CONTROL_INTERACTIVE and JOB_CONTROL_NONE
*/
extern int job_control_mode;

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
  If id is -1, return the last job used.
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
   Tests if the job has completed
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
   Notify user of nog events.  Notify the user about stopped or
   terminated jobs.  Delete terminated jobs from the active job list.

   \param interactive whether interactive jobs should be reaped as well
*/
int job_reap( int interactive );

/**
   Signal handler for SIGCHLD.  Mark any processes with relevant
   information.
*/
void job_handle_signal( int signal, siginfo_t *info, void *con );

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
   Send of an process/job exit event notification. This function is a conveniance wrapper around event_fire().
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

#endif
