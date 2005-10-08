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

#include "util.h"
#include "io.h"


/**
   Types of internal processes
*/
enum
{
	EXTERNAL,
	INTERNAL_BUILTIN,
	INTERNAL_FUNCTION,
	INTERNAL_BLOCK,
	INTERNAL_EXEC
}
	;


/** 
	A structure representing a single process. Contains variables for
	tracking process state and the process argument list. 
*/
typedef struct process{
	/** argv parameter for for execv */
	wchar_t **argv;
	/** actual command to pass to exec */
	wchar_t *actual_cmd;       
	/** process ID */
	pid_t pid;
	/** 
		Type of process. Can be one of \c EXTERNAL, \c
		INTERNAL_BUILTIN, \c INTERNAL_FUNCTION, \c INTERNAL_BLOCK
	*/
	int type;
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
} process_t;


/** Represents a pipeline of one or more processes.  */
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
		forked
	*/
	int constructed;
	/**
	   Whether the specified job is a part of a subshell or some other form of special job that should not be reported
	*/
	int skip_notification;
	
	/** List of IO redrections for the job */
	io_data_t *io;
	
	/** Should the exit status be negated */
	int negate;	
	/** Is this a conditional short circut thing? If so, is it an COND_OR or a COND_AND */
	struct job *next;           
} job_t;

/** Whether we are running a subshell command */
extern int is_subshell;
/** Whether we are running a block of commands */
extern int is_block;
/** Whether we are reading from the keyboard right now*/
extern int is_interactive;
/** Whether this shell is attached to the keyboard at all*/
extern int is_interactive_session;
/** Whether we are a login shell*/
extern int is_login;
/** Whether we are a event handler*/
extern int is_event;
/** Linked list of all jobs */
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

extern pid_t proc_last_bg_pid;

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
   Create a new job
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
*/
int job_do_notification();
/**
   Signal handler for SIGCHLD.  Mark any processes with relevant
   information.

*/
void job_handle_signal( int signal, siginfo_t *info, void *con );

/**
   Clean up before exiting
*/
void proc_destroy();


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

#endif
