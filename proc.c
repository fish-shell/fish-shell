/** \file proc.c

Utilities for keeping track of jobs, processes and subshells, as
well as signal handling functions for tracking children. These
functions do not themselves launch new processes, the exec library
will call proc to create representations of the running jobs as
needed.

Some of the code in this file is based on code from the Glibc manual.
	
*/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <wchar.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <sys/time.h>

#if HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif

#if HAVE_TERMIO_H
#include <termio.h>
#endif

#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif

#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "proc.h"
#include "common.h"
#include "reader.h"
#include "sanity.h"
#include "env.h"
#include "parser.h"
#include "signal.h"
#include "event.h"

#include "halloc.h"
#include "halloc_util.h"
#include "output.h"

/**
   Size of message buffer 
*/
#define MESS_SIZE 256

/**
   Size of buffer for reading buffered output
*/
#define BUFFER_SIZE 4096

/** 
	Status of last process to exit 
*/
static int last_status=0;

/**
   Signal flag 
*/
static sig_atomic_t got_signal=0;

job_t *first_job=0;
int is_interactive=-1;
int is_interactive_session=0;
int is_subshell=0;
int is_block=0;
int is_login=0;
int is_event=0;
int proc_had_barrier;
pid_t proc_last_bg_pid = 0;
int job_control_mode = JOB_CONTROL_INTERACTIVE;
int no_exec=0;


/**
   The event variable used to send all process event
*/
static event_t event;

/**
  Stringbuffer used to create arguments when firing events
*/
static string_buffer_t event_pid;

/**
  Stringbuffer used to create arguments when firing events
*/
static string_buffer_t event_status;

/**
   A stack containing the values of is_interactive. Used by proc_push_interactive and proc_pop_interactive.
*/
static array_list_t *interactive_stack;

void proc_init()
{
	interactive_stack = al_halloc( global_context );
	proc_push_interactive( 0 );
	al_init( &event.arguments );
	sb_init( &event_pid );
	sb_init( &event_status );
}


/**
   Remove job from list of jobs 
*/
static int job_remove( job_t *j )
{
	job_t *prev=0, *curr=first_job;
	while( (curr != 0) && (curr != j) )
	{
		prev = curr;
		curr = curr->next;
	}

	if( j != curr )
	{
		debug( 1, _( L"Job inconsistency" ) );
		sanity_lose();
		return 0;
	}
	
	if( prev == 0 )
		first_job = j->next;
	else
		prev->next = j->next;
	return 1;
}


/*
  Remove job from the job list and free all memory associated with
  it.
*/
void job_free( job_t * j )
{
	job_remove( j );
	halloc_free( j );
}

void proc_destroy()
{
	al_destroy( &event.arguments );
	sb_destroy( &event_pid );
	sb_destroy( &event_status );
	while( first_job )
	{
		debug( 2, L"freeing leaked job %ls", first_job->command );
		job_free( first_job );
	}	
}

void proc_set_last_status( int s )
{
	last_status = s;
}

int proc_get_last_status()
{
	return last_status;
}

job_t *job_create()
{
	int free_id=1;
	job_t *res;
	
	while( job_get( free_id ) != 0 )
		free_id++;
	res = halloc( 0, sizeof(job_t) );
	res->next = first_job;
	res->job_id = free_id;
	first_job = res;

	job_set_flag( res, 
				  JOB_CONTROL, 
				  (job_control_mode==JOB_CONTROL_ALL) || 
				  ((job_control_mode == JOB_CONTROL_INTERACTIVE) && (is_interactive)) );

//	if( res->job_id > 2 )
//		fwprintf( stderr, L"Create job %d\n", res->job_id );	
	return res;
}

 
job_t *job_get( int id )
{
	job_t *res = first_job;
	if( id <= 0 )
	{
		return res;
	}
	
	while( res != 0 )
	{
		if( res->job_id == id )
			return res;
		res = res->next;
	}
	return 0;
}

job_t *job_get_from_pid( int pid )
{
	job_t *res = first_job;
	
	while( res != 0 )
	{
		if( res->pgid == pid )
			return res;
		res = res->next;
	}
	return 0;
}


/* 
   Return true if all processes in the job have stopped or completed.  

   \param j the job to test
*/
int job_is_stopped( const job_t *j )
{
	process_t *p;

	for (p = j->first_process; p; p = p->next)
	{
		if (!p->completed && !p->stopped)
		{
			return 0;
		}
	}
	return 1;
}


/* 
   Return true if the last processes in the job has completed.  

   \param j the job to test
*/
int job_is_completed( const job_t *j )
{
	process_t *p;
	
	for (p = j->first_process; p->next; p = p->next)
		;
	
	return p->completed;
	
}

void job_set_flag( job_t *j, int flag, int set )
{
	if( set )
		j->flags |= flag;
	else
		j->flags = j->flags & (0xffffffff ^ flag);
}

int job_get_flag( job_t *j, int flag )
{
	return j->flags&flag?1:0;
}

int job_signal( job_t *j, int signal )
{	
	pid_t my_pid = getpid();
	int res = 0;
	
	if( j->pgid != my_pid )
	{
		res = killpg( j->pgid, SIGHUP );
	}
	else
	{
		process_t *p;

		for( p = j->first_process; p; p=p->next )
		{
			if( ! p->completed )
			{
				if( p->pid )
				{
					if( kill( p->pid, SIGHUP ) )
					{
						res = -1;
						break;
					}
				}
			}
		}

	}

	return res;

}


/**
   Store the status of the process pid that was returned by waitpid.
   Return 0 if all went well, nonzero otherwise.  
*/
static void mark_process_status( job_t *j,
								 process_t *p,
								 int status )
{
//	debug( 0, L"Process %ls %ls", p->argv[0], WIFSTOPPED (status)?L"stopped":(WIFEXITED( status )?L"exited":(WIFSIGNALED( status )?L"signaled to exit":L"BLARGH")) );
	p->status = status;
	
	if (WIFSTOPPED (status))
	{
		p->stopped = 1;
	}
	else if (WIFSIGNALED(status) || WIFEXITED(status)) 
	{
		p->completed = 1;
	}
	else
	{
		ssize_t ignore;
		
		/* This should never be reached */
		p->completed = 1;

		char mess[MESS_SIZE];
		snprintf( mess,
				  MESS_SIZE,
				  "Process %d exited abnormally\n",
				  (int) p->pid );
		/*
		  If write fails, do nothing. We're in a signal handlers error
		  handler. If things aren't working properly, it's safer to
		  give up.
		 */
		ignore = write( 2, mess, strlen(mess) );
	}
}

/**
   Handle status update for child \c pid. This function is called by
   the signal handler, so it mustn't use malloc or any such hitech
   nonsense.

   \param pid the pid of the process whose status changes
   \param status the status as returned by wait
*/
static void handle_child_status( pid_t pid, int status )
{
	int found_proc = 0;
	job_t *j=0;
	process_t *p=0;
//	char mess[MESS_SIZE];	
	found_proc = 0;		
	/*
	  snprintf( mess, 
	  MESS_SIZE,
	  "Process %d\n",
	  (int) pid );
	  write( 2, mess, strlen(mess ));
	*/

	for( j=first_job; j && !found_proc; j=j->next )
	{
		process_t *prev=0;
		for( p=j->first_process; p; p=p->next )
		{
			if( pid == p->pid )
			{
/*				snprintf( mess,
  MESS_SIZE,
  "Process %d is %ls from job %ls\n",
  (int) pid, p->actual_cmd, j->command );
  write( 2, mess, strlen(mess ));
*/		
				
				mark_process_status ( j, p, status);
				if( p->completed && prev != 0  )
				{
					if( !prev->completed && prev->pid)
					{
						/*					snprintf( mess,
						  MESS_SIZE,
						  "Kill previously uncompleted process %ls (%d)\n",
						  prev->actual_cmd, 
						  prev->pid );
						  write( 2, mess, strlen(mess ));
						*/
						kill(prev->pid,SIGPIPE);
					}
				}
				found_proc = 1;
				break;
			}
			prev = p;
		}
	}


	if( WIFSIGNALED( status ) && 
		( WTERMSIG(status)==SIGINT  || 
		  WTERMSIG(status)==SIGQUIT ) )
	{
		if( !is_interactive_session )
		{			
			struct sigaction act;
			sigemptyset( & act.sa_mask );
			act.sa_flags=0;	
			act.sa_handler=SIG_DFL;
			sigaction( SIGINT, &act, 0 );
			sigaction( SIGQUIT, &act, 0 );
			kill( getpid(), WTERMSIG(status) );
		}
		else
		{
			block_t *c = current_block;
			if( p && found_proc )
			{
				while( c )
				{
					c->skip=1;
					c=c->outer;
				}		
			}			
		}
	}
		
	if( !found_proc )
	{
		/* 
		   A child we lost track of?
		   
		   There have been bugs in both subshell handling and in
		   builtin handling that have caused this previously...
		*/
/*		snprintf( mess, 
  MESS_SIZE,
  "Process %d not found by %d\n",
  (int) pid, (int)getpid() );

  write( 2, mess, strlen(mess ));
*/
	}	
	return;
}


void job_handle_signal ( int signal, siginfo_t *info, void *con )
{
	
	int status;
	pid_t pid;
	int errno_old = errno;

	got_signal = 1;

//	write( 2, "got signal\n", 11 );

	while(1)
	{
		switch(pid=waitpid( -1,&status,WUNTRACED|WNOHANG ))
		{
			case 0:
			case -1:
			{
				errno=errno_old;
				return;
			}	
			default:

				handle_child_status( pid, status );
				break;
		}
	}	
	kill( 0, SIGIO );
	errno=errno_old;
}

/** 
	Format information about job status for the user to look at.  

	\param j the job to test
	\param status a string description of the job exit type
*/
static void format_job_info( const job_t *j, const wchar_t *status )
{
	fwprintf (stdout, L"\r" );
	fwprintf (stdout, _( L"Job %d, \'%ls\' has %ls" ), j->job_id, j->command, status);
	fflush( stdout );
	tputs(clr_eol,1,&writeb);
	fwprintf (stdout, L"\n" );
}

void proc_fire_event( const wchar_t *msg, int type, pid_t pid, int status )
{
	
	event.type=type;
	event.param1.pid = pid;
	
	al_push( &event.arguments, msg );			

	sb_printf( &event_pid, L"%d", pid );
	al_push( &event.arguments, event_pid.buff );
	
	sb_printf( &event_status, L"%d", status );			
	al_push( &event.arguments, event_status.buff );

	event_fire( &event );

	al_truncate( &event.arguments, 0 );
	sb_clear( &event_pid );	
	sb_clear( &event_status );	
}				

int job_reap( int interactive )
{
	job_t *j, *jnext;	
	int found=0;
	
	static int locked = 0;
	
	locked++;	
	
	/*
	  job_read may fire an event handler, we do not want to call
	  ourselves recursively (to avoid infinite recursion).
	*/
	if( locked>1 )
		return 0;
		
	for( j=first_job; j; j=jnext)
    {		
		process_t *p;
		jnext = j->next;
		
		/*
		  If we are reaping only jobs who do not need status messages
		  sent to the console, do not consider reaping jobs that need
		  status messages
		*/
		if( (!job_get_flag( j, JOB_SKIP_NOTIFICATION ) ) && (!interactive) && (!job_get_flag( j, JOB_FOREGROUND )))
		{
			continue;
		}
	
		for( p=j->first_process; p; p=p->next )
		{
			int s;
			if( !p->completed )
				continue;
			
			if( !p->pid )
				continue;			
			
			s = p->status;
			
			proc_fire_event( L"PROCESS_EXIT", EVENT_EXIT, p->pid, ( WIFSIGNALED(s)?-1:WEXITSTATUS( s )) );			
			
			if( WIFSIGNALED(s) )
			{
				/* 
				   Ignore signal SIGPIPE.We issue it ourselves to the pipe
				   writer when the pipe reader dies.
				*/
				if( WTERMSIG(s) != SIGPIPE )
				{	
					int proc_is_job = ((p==j->first_process) && (p->next == 0));
					if( proc_is_job )
						job_set_flag( j, JOB_NOTIFIED, 1 );
					if( !job_get_flag( j, JOB_SKIP_NOTIFICATION ) )
					{
						if( proc_is_job )
							fwprintf( stdout,
									  _( L"%ls: Job %d, \'%ls\' terminated by signal %ls (%ls)" ),
									  program_name,
									  j->job_id, 
									  j->command,
									  sig2wcs(WTERMSIG(p->status)),
									  signal_get_desc( WTERMSIG(p->status) ) );
						else
							fwprintf( stdout,
									  _( L"%ls: Process %d, \'%ls\' from job %d, \'%ls\' terminated by signal %ls (%ls)" ),
									  program_name,
									  p->pid,
									  p->argv[0],
									  j->job_id,
									  j->command,
									  sig2wcs(WTERMSIG(p->status)),
									  signal_get_desc( WTERMSIG(p->status) ) );
						tputs(clr_eol,1,&writeb);
						fwprintf (stdout, L"\n" );
						found=1;						
					}
					
					/* 
					   Clear status so it is not reported more than once
					*/
					p->status = 0;
				}
			}						
		}
		
		/* 
		   If all processes have completed, tell the user the job has
		   completed and delete it from the active job list.  
		*/
		if( job_is_completed( j ) ) 
		{
			if( !job_get_flag( j, JOB_FOREGROUND) && !job_get_flag( j, JOB_NOTIFIED ) && !job_get_flag( j, JOB_SKIP_NOTIFICATION ) )
			{
				format_job_info( j, _( L"ended" ) );
				found=1;
			}
			proc_fire_event( L"JOB_EXIT", EVENT_EXIT, -j->pgid, 0 );			
			proc_fire_event( L"JOB_EXIT", EVENT_JOB_ID, j->job_id, 0 );			

			job_free(j);
		}		
		else if( job_is_stopped( j ) && !job_get_flag( j, JOB_NOTIFIED ) ) 
		{
			/* 
			   Notify the user about newly stopped jobs. 
			*/
			if( !job_get_flag( j, JOB_SKIP_NOTIFICATION ) )
			{
				format_job_info( j, _( L"stopped" ) );
				found=1;
			}			
			job_set_flag( j, JOB_NOTIFIED, 1 );
		}
	}

	if( found )
		fflush( stdout );

	locked = 0;
	
	return found;	
}


#ifdef HAVE__PROC_SELF_STAT

/**
   Maximum length of a /proc/[PID]/stat filename
*/
#define FN_SIZE 256

/**
   Get the CPU time for the specified process
*/
unsigned long proc_get_jiffies( process_t *p )
{
	wchar_t fn[FN_SIZE];

	char state;
	int pid, ppid, pgrp, 
		session, tty_nr, tpgid,
		exit_signal, processor;	
	
	long int cutime, cstime, priority, 
		nice, placeholder, itrealvalue, 
		rss;
	unsigned long int flags, minflt, cminflt, 
		majflt, cmajflt, utime, 
		stime, starttime, vsize, 
		rlim, startcode, endcode,
		startstack, kstkesp, kstkeip,
		signal, blocked, sigignore,
		sigcatch, wchan, nswap, cnswap;
	char comm[1024];
	
	if( p->pid <= 0 )
		return 0;
	
	swprintf( fn, FN_SIZE, L"/proc/%d/stat", p->pid );
	
	FILE *f = wfopen( fn, "r" );
	if( !f )
		return 0;
	
	int count = fscanf( f, 
						"%d %s %c " 
						"%d %d %d "
						"%d %d %lu "
						
						"%lu %lu %lu "
						"%lu %lu %lu "
						"%ld %ld %ld "

						"%ld %ld %ld "
						"%lu %lu %ld "
						"%lu %lu %lu "

						"%lu %lu %lu "
						"%lu %lu %lu "
						"%lu %lu %lu "

						"%lu %d %d ",
	
						&pid, comm, &state, 
						&ppid, &pgrp, &session, 
						&tty_nr, &tpgid, &flags,

						&minflt, &cminflt, &majflt,
						&cmajflt, &utime, &stime,
						&cutime, &cstime, &priority,
			
						&nice, &placeholder, &itrealvalue,
						&starttime, &vsize, &rss,
						&rlim, &startcode, &endcode,

						&startstack, &kstkesp, &kstkeip,
						&signal, &blocked, &sigignore,
						&sigcatch, &wchan, &nswap,

						&cnswap, &exit_signal, &processor
		);

	if( count < 17 )
	{
		return 0;
	}

	/*
	  Don't need to check exit status of fclose on read-only streams
	*/
	fclose( f );
	return utime+stime+cutime+cstime;
	
}

/**
   Update the CPU time for all jobs
*/
void proc_update_jiffies()
{
	job_t *j;
	process_t *p;
	
	for( j=first_job; j; j=j->next )
	{
		for( p=j->first_process; p; p=p->next )
		{
			gettimeofday( &p->last_time, 0 );
			p->last_jiffies = proc_get_jiffies( p );
		}	
	}
}


#endif

/**
   Check if there are buffers associated with the job, and select on
   them for a while if available. 
   
   \param j the job to test

   \return 1 if buffers were avaialble, zero otherwise
*/
static int select_try( job_t *j )
{
	fd_set fds;
	int maxfd=-1;
	io_data_t *d;

	FD_ZERO(&fds);
	
	for( d = j->io; d; d=d->next )
	{
		if( d->io_mode == IO_BUFFER )
		{
			int fd = d->param1.pipe_fd[0];
//			fwprintf( stderr, L"fd %d on job %ls\n", fd, j->command );
			FD_SET( fd, &fds );
			maxfd=maxi( maxfd, d->param1.pipe_fd[0] );
			debug( 3, L"select_try on %d\n", fd );
		}
	}
	
	if( maxfd >= 0 )
	{
		int retval;
		struct timeval tv;
		
		tv.tv_sec=0;
		tv.tv_usec=10000;
		
		retval =select( maxfd+1, &fds, 0, 0, &tv );
		return retval > 0;
	}

	return -1;
}

/**
   Read from descriptors until they are empty. 

   \param j the job to test
*/
static void read_try( job_t *j )
{
	io_data_t *d, *buff=0;

	/*
	  Find the last buffer, which is the one we want to read from
	*/
	for( d = j->io; d; d=d->next )
	{
		
		if( d->io_mode == IO_BUFFER )
		{
			buff=d;
		}
	}
	
	if( buff )
	{
		debug( 3, L"proc::read_try('%ls')\n", j->command );
		while(1)
		{
			char b[BUFFER_SIZE];
			int l;
			
			l=read_blocked( buff->param1.pipe_fd[0],
					b, BUFFER_SIZE );
			if( l==0 )
			{
				break;
			}
			else if( l<0 )
			{
				if( errno != EAGAIN )
				{
					debug( 1, 
						   _( L"An error occured while reading output from code block" ) );
					wperror( L"read_try" );			
				}				
				break;
			}
			else
			{
				b_append( buff->param2.out_buffer, b, l );
			}			
		}
	}
}


/**
   Give ownership of the terminal to the specified job. 

   \param j The job to give the terminal to.

   \param cont If this variable is set, we are giving back control to
   a job that has previously been stopped. In that case, we need to
   set the terminal attributes to those saved in the job.
 */
static int terminal_give_to_job( job_t *j, int cont )
{
	
	if( tcsetpgrp (0, j->pgid) )
	{
		debug( 1, 
			   _( L"Could not send job %d ('%ls') to foreground" ), 
			   j->job_id, 
			   j->command );
		wperror( L"tcsetpgrp" );
		return 0;
	}
	
	if( cont )
	{  
		if( tcsetattr (0, TCSADRAIN, &j->tmodes))
		{
			debug( 1,
				   _( L"Could not send job %d ('%ls') to foreground" ),
				   j->job_id,
				   j->command );
			wperror( L"tcsetattr" );
			return 0;
		}
	}
	return 1;
}

/**
   Returns contol of the terminal to the shell, and saves the terminal
   attribute state to the job, so that we can restore the terminal
   ownership to the job at a later time .  
*/
static int terminal_return_from_job( job_t *j)
{
		
	if( tcsetpgrp (0, getpid()) )
	{
		debug( 1, _( L"Could not return shell to foreground" ) );
		wperror( L"tcsetpgrp" );
		return 0;
	}
	
	/* 
	   Save jobs terminal modes.  
	*/
	if( tcgetattr (0, &j->tmodes) )
	{
		debug( 1, _( L"Could not return shell to foreground" ) );
		wperror( L"tcgetattr" );
		return 0;
	}
	
	/* 
	   Restore the shell's terminal modes.  
	*/
	if( tcsetattr (0, TCSADRAIN, &shell_modes))
	{
		debug( 1, _( L"Could not return shell to foreground" ) );
		wperror( L"tcsetattr" );
		return 0;
	}

	return 1;
}

void job_continue (job_t *j, int cont)
{
	/*
	  Put job first in the job list
	*/
	job_remove( j );
	j->next = first_job;
	first_job = j;
	job_set_flag( j, JOB_NOTIFIED, 0 );

	CHECK_BLOCK();
	
	debug( 4,
		   L"Continue job %d, gid %d (%ls), %ls, %ls",
		   j->job_id, 
		   j->pgid,
		   j->command, 
		   job_is_completed( j )?L"COMPLETED":L"UNCOMPLETED", 
		   is_interactive?L"INTERACTIVE":L"NON-INTERACTIVE" );
	
	if( !job_is_completed( j ) )
	{
		if( job_get_flag( j, JOB_TERMINAL ) && job_get_flag( j, JOB_FOREGROUND ) )
		{							
			/* Put the job into the foreground.  */
			int ok;
			
			signal_block();
			
			ok = terminal_give_to_job( j, cont );
			
			signal_unblock();		

			if( !ok )
				return;
			
		}
		
		/* 
		   Send the job a continue signal, if necessary.  
		*/
		if( cont )
		{
			process_t *p;

			for( p=j->first_process; p; p=p->next )
				p->stopped=0;

			if( job_get_flag( j, JOB_CONTROL ) )
			{
				if( killpg( j->pgid, SIGCONT ) )
				{
					wperror( L"killpg (SIGCONT)" );
					return;
				}
			}
			else
			{
				for( p=j->first_process; p; p=p->next )
				{
					if (kill ( p->pid, SIGCONT) < 0)
					{
						wperror (L"kill (SIGCONT)");
						return;
					}		
				}
			}
		}
	
		if( job_get_flag( j, JOB_FOREGROUND ) )
		{
			int quit = 0;
		
			/* 
			   Wait for job to report. Looks a bit ugly because it has to
			   handle the possibility that a signal is dispatched while
			   running job_is_stopped().
			*/
			while( !quit )
			{
				do
				{
					got_signal = 0;
					quit = job_is_stopped( j ) || job_is_completed( j );
				}

				while( got_signal && !quit )
					;

				if( !quit )
				{
					
//					debug( 1, L"select_try()" );	
					switch( select_try(j) )
					{
						case 1:			
						{
							read_try( j );
							break;
						}
					
						case -1:
						{
							/*
							  If there is no funky IO magic, we can use
							  waitpid instead of handling child deaths
							  through signals. This gives a rather large
							  speed boost (A factor 3 startup time
							  improvement on my 300 MHz machine) on
							  short-lived jobs.
							*/
							int status;						
							pid_t pid = waitpid(-1, &status, WUNTRACED );
							if( pid > 0 )
							{
								handle_child_status( pid, status );
							}
							else
							{
								/*
								  This probably means we got a
								  signal. A signal might mean that the
								  terminal emulator sent us a hup
								  signal to tell is to close. If so,
								  we should exit.
								*/
								if( reader_exit_forced() )
								{
									quit = 1;
								}
								
							}
							break;
						}
								
					}
				}					
			}
		}	
	}
	
	if( job_get_flag( j, JOB_FOREGROUND ) )
	{
		
		if( job_is_completed( j ))
		{
			process_t *p = j->first_process;
			while( p->next )
				p = p->next;

			if( WIFEXITED( p->status ) || WIFSIGNALED(p->status))
			{
				/* 
				   Mark process status only if we are in the foreground
				   and the last process in a pipe, and it is not a short circuted builtin
				*/
				if( p->pid )
				{
					int status = proc_format_status(p->status);
					
					proc_set_last_status( job_get_flag( j, JOB_NEGATE )?!status:status);
				}
			}			
		}
		/* 
		   Put the shell back in the foreground.  
		*/
		if( job_get_flag( j, JOB_TERMINAL ) && job_get_flag( j, JOB_FOREGROUND ) )
		{
			int ok;
			
			signal_block();

			ok = terminal_return_from_job( j );
			
			signal_unblock();
			
			if( !ok )
				return;
			
		}
	}
	
}

int proc_format_status(int status) 
{
	if( WIFSIGNALED( status ) ) 
	{
		return 128+WTERMSIG(status);
	} 
	else if( WIFEXITED( status ) ) 
	{
		return WEXITSTATUS(status);
	}
	return status;
	
}


void proc_sanity_check()
{
	job_t *j;
	job_t *fg_job=0;
	
	for( j = first_job; j ; j=j->next )
	{
		process_t *p;

		if( !job_get_flag( j, JOB_CONSTRUCTED ) )
			continue;
		
		
		validate_pointer( j->command, 
						  _( L"Job command" ), 
						  0 );
		validate_pointer( j->first_process,
						  _( L"Process list pointer" ),
						  0 );
		validate_pointer( j->next, 
						  _( L"Job list pointer" ),
						  1 );

		/*
		  More than one foreground job?
		*/
		if( job_get_flag( j, JOB_FOREGROUND ) && !(job_is_stopped(j) || job_is_completed(j) ) )
		{
			if( fg_job != 0 )
			{
				debug( 0, 
					   _( L"More than one job in foreground: job 1: '%ls' job 2: '%ls'"),
					   fg_job->command,
					   j->command );
				sanity_lose();
			}
			fg_job = j;
		}
		
   		p = j->first_process;
		while( p )
		{			
			validate_pointer( p->argv, _( L"Process argument list" ), 0 );
			validate_pointer( p->argv[0], _( L"Process name" ), 0 );
			validate_pointer( p->next, _( L"Process list pointer" ), 1 );
			validate_pointer( p->actual_cmd, _( L"Process command" ), 1 );
			
			if ( (p->stopped & (~0x00000001)) != 0 )
			{
				debug( 0,
					   _( L"Job '%ls', process '%ls' has inconsistent state \'stopped\'=%d" ),
					   j->command, 
					   p->argv[0],
					   p->stopped );
				sanity_lose();
			}
			
			if ( (p->completed & (~0x00000001)) != 0 )
			{
				debug( 0,
					   _( L"Job '%ls', process '%ls' has inconsistent state \'completed\'=%d" ),
					   j->command, 
					   p->argv[0],
					   p->completed );
				sanity_lose();
			}
			
			p=p->next;
		}
		
	}	
}

void proc_push_interactive( int value )
{
	int old = is_interactive;
	al_push_long( interactive_stack, (long)is_interactive );
	is_interactive = value;
	if( old != value )
		signal_set_handlers();
}

void proc_pop_interactive()
{
	int old = is_interactive;
	is_interactive= (int)al_pop_long(interactive_stack);
	if( is_interactive != old )
		signal_set_handlers();
}
