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
#include <sys/ioctl.h>
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

#include <term.h>

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

/**
   Size of message buffer 
*/
#define MESS_SIZE 256

/**
   Size of buffer for reading buffered output
*/
#define BUFFER_SIZE 4096

/** Status of last process to exit */
static int last_status=0;

/** Signal flag */
static sig_atomic_t got_signal=0;

job_t *first_job=0;
int is_interactive=0;
int is_interactive_session=0;
int is_subshell=0;
int is_block=0;
int is_login=0;
int is_event=0;
int proc_had_barrier;
pid_t proc_last_bg_pid = 0;

/**
  List used to store arguments when firing events
*/
static array_list_t event_arg;
/**
  Stringbuffer used to create arguments when firing events
*/
static string_buffer_t event_pid;
/**
  Stringbuffer used to create arguments when firing events
*/
static string_buffer_t event_status;


void proc_init()
{
	al_init( &event_arg );
	sb_init( &event_pid );
	sb_init( &event_status );
}


/**
   Recursively free a process and those following it
*/
static void free_process( process_t *p )
{
	wchar_t **arg;
	
	if( p==0 )
		return;

	

	free_process( p->next );
	debug( 3, L"Free process %ls", p->actual_cmd );
	free( p->actual_cmd );
	if( p->argv != 0 )
	{
		debug( 3, L"Process has argument vector" );
		for( arg=p->argv; *arg; arg++ )
		{
			debug( 3, L"Free argument %ls", *arg );
			free( *arg );
		}		
		free(p->argv );
	}
	free( p );
}

/** Remove job from list of jobs */

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
		debug( 1, L"Job inconsistency" );
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
	io_data_t *io, *ionext;
	
//	fwprintf( stderr, L"Remove job %d (%ls)\n", j->job_id, j->command );	

	job_remove( j );
	
	/* Then free ourselves */
	free_process( j->first_process);
	
	if( j->command != 0 )
		free( j->command );
	
	for( io=j->io; io; io=ionext )
	{
		ionext = io->next;
//		fwprintf( stderr, L"Kill redirect %d of type %d\n", ionext, io->io_mode );
		if( io->io_mode == IO_FILE )
		{
			free( io->param1.filename );
		}
		free( io );		
	}
	
	free( j );
}

void proc_destroy()
{
	al_destroy( &event_arg );
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
	//	fwprintf( stderr, L"Set last status to %d\n", s );
}

int proc_get_last_status()
{
	return last_status;
}

job_t *job_create()
{
	int free_id=0;
	job_t *res;
	
	while( job_get( free_id ) != 0 )
		free_id++;
	res = calloc( 1, sizeof(job_t) );
	res->next = first_job;
	res->job_id = free_id;
	first_job = res;
//	if( res->job_id > 2 )
//		fwprintf( stderr, L"Create job %d\n", res->job_id );	
	return res;
}

 
job_t *job_get( int id )
{
	job_t *res = first_job;
	if( id == -1 )
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
   Return true if all processes in the job have completed.  
*/
int job_is_completed( const job_t *j )
{
	process_t *p;
	
	for (p = j->first_process; p; p = p->next)
	{
		if (!p->completed)
		{
//			fwprintf( stderr, L"Process %ls not finished\n", p->argv[0] );
			return 0;
		}
	}
	return 1;
}

/**
   Return true if all processes in the job have completed.  
*/
static int job_last_is_completed( const job_t *j )
{
	process_t *p;
	
	for (p = j->first_process; p->next; p = p->next)
		;
	return p->completed;
}


/**
   Store the status of the process pid that was returned by waitpid.
   Return 0 if all went well, nonzero otherwise.  
*/
static void mark_process_status( job_t *j,
								 process_t *p,
								 int status )
{
	p->status = status;
	if (WIFSTOPPED (status))
	{
		p->stopped = 1;
	}
	else
	{
		p->completed = 1;
		
		if (( !WIFEXITED( status ) ) &&
			(! WIFSIGNALED( status )) )
		{
			/* This should never be reached */
			char mess[MESS_SIZE];
			snprintf( mess,
					 MESS_SIZE,
					 "Process %d exited abnormally\n",
					 (int) p->pid );
			
			write( 2, mess, strlen(mess) );
		}
	}
}

/**
   Handle status update for child \c pid. This function is called by
   the signal handler, so it mustn't use malloc or any such nonsense.
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
*/
static void format_job_info( const job_t *j, const wchar_t *status )
{
	fwprintf (stdout, L"\rJob %d, \'%ls\' has %ls", j->job_id, j->command, status);
	fflush( stdout );
	tputs(clr_eol,1,&writeb);
	fwprintf (stdout, L"\n" );
}

void proc_fire_event( const wchar_t *msg, int type, pid_t pid, int status )
{
	static event_t ev;
	
	ev.type=type;
	ev.param1.pid = pid;
	
	al_push( &event_arg, msg );			

	sb_printf( &event_pid, L"%d", pid );
	al_push( &event_arg, event_pid.buff );
	
	sb_printf( &event_status, L"%d", status );			
	al_push( &event_arg, event_status.buff );

	event_fire( &ev, &event_arg );
	al_truncate( &event_arg, 0 );
	sb_clear( &event_pid );	
	sb_clear( &event_status );	
}				

int job_reap( int interactive )
{
	job_t *j, *jnext;	
	int found=0;
	
	static int locked = 0;
	
	locked++;	
	if( locked>1 )
		return 0;
		
	for( j=first_job; j; j=jnext)
    {		
		process_t *p;
		jnext = j->next;
		
		if( (!j->skip_notification) && (!interactive) )
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
						j->notified = 1;
					if( !j->skip_notification )
					{
						if( proc_is_job )
							fwprintf( stdout,
									  L"fish: Job %d, \'%ls\' terminated by signal %ls (%ls)",
									  j->job_id, 
									  j->command,
									  sig2wcs(WTERMSIG(p->status)),
									  sig_description( WTERMSIG(p->status) ) );
						else
							fwprintf( stdout,
									  L"fish: Process %d, \'%ls\' from job %d, \'%ls\' terminated by signal %ls (%ls)",
									  p->pid,
									  p->argv[0],
									  j->job_id,
									  j->command,
									  sig2wcs(WTERMSIG(p->status)),
									  sig_description( WTERMSIG(p->status) ) );
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
			if( !j->fg && !j->notified )
			{
				if( !j->skip_notification )
				{
					format_job_info( j, L"ended" );
					found=1;
				}
			}
			proc_fire_event( L"JOB_EXIT", EVENT_EXIT, -j->pgid, 0 );			
			proc_fire_event( L"JOB_EXIT", EVENT_JOB_ID, j->job_id, 0 );			

			job_free(j);
		}		
		else if( job_is_stopped( j ) && !j->notified ) 
		{
			/* 
			   Notify the user about newly stopped jobs. 
			*/
			if( !j->skip_notification )
			{
				format_job_info( j, L"stopped" );
				found=1;
			}			
			j->notified = 1;
		}
	}

	if( found )
		fflush( stdout );

	locked = 0;
	
	return found;	
}


#ifdef HAVE__PROC_SELF_STAT
/**
   Get the CPU time for the specified process
*/
unsigned long proc_get_jiffies( process_t *p )
{
	wchar_t fn[256];
	//char stat_line[1024];

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
	
	swprintf( fn, 512, L"/proc/%d/stat", p->pid );
	
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
		
		tv.tv_sec=5;
		tv.tv_usec=0;
		
		retval =select( maxfd+1, &fds, 0, 0, &tv );
		return retval > 0;
	}

	return -1;
}

/**
   Read from descriptors until they are empty. 
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
						   L"An error occured while reading output from code block" );
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

void job_continue (job_t *j, int cont)
{
	/*
	  Put job first in the job list
	*/
	job_remove( j );
	j->next = first_job;
	first_job = j;
	j->notified = 0;
	
	debug( 3,
		   L"Continue on job %d (%ls), %ls, %ls",
		   j->job_id, 
		   j->command, 
		   job_is_completed( j )?L"COMPLETED":L"UNCOMPLETED", 
		   is_interactive?L"INTERACTIVE":L"NON-INTERACTIVE" );

	if( !job_is_completed( j ) )
	{
		if( !is_subshell && is_interactive && !is_block)
		{
							
			/* Put the job into the foreground.  */
			if(  j->fg )
			{
				signal_block();
				if( tcsetpgrp (0, j->pgid) )
				{
					debug( 1, 
						   L"Could not send job %d ('%ls') to foreground", 
						   j->job_id, 
						   j->command );
					wperror( L"tcsetpgrp" );
					return;
				}
				
				if( cont )
				{  
					if( tcsetattr (0, TCSADRAIN, &j->tmodes))
					{
						debug( 1,
							   L"Could not send job %d ('%ls') to foreground",
							   j->job_id,
							   j->command );
						wperror( L"tcsetattr" );
						return;
					}										
				}
				signal_unblock();
			}
		}

		/* 
		   Send the job a continue signal, if necessary.  
		*/
		if( cont )
		{
			process_t *p;
			for( p=j->first_process; p; p=p->next )
				p->stopped=0;
			for( p=j->first_process; p; p=p->next )
			{
				if (kill ( p->pid, SIGCONT) < 0)
				{
					wperror (L"kill (SIGCONT)");
					return;
				}		
			}
		}
	
		if( j->fg )
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
					quit = job_is_stopped( j ) || job_last_is_completed( j );
				}
				while( got_signal && !quit );
				if( !quit )
				{
					
					debug( 3, L"select_try()" );	
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
								handle_child_status( pid, status );
							break;
						}
								
					}
				}					
			}
		}
	
	}
			
	if( j->fg )
	{
		
		if( job_is_completed( j ))
		{
			process_t *p = j->first_process;
			while( p->next )
				p = p->next;

			if( WIFEXITED( p->status ) )
			{
				/* 
				   Mark process status only if we are in the foreground
				   and the last process in a pipe, and it is not a short circuted builtin
				*/
				if( p->pid )
				{
					debug( 3, L"Set status of %ls to %d", j->command, WEXITSTATUS(p->status) );
					proc_set_last_status( j->negate?(WEXITSTATUS(p->status)?0:1):WEXITSTATUS(p->status) );
				}
			}			
		}
		/* 
		   Put the shell back in the foreground.  
		*/
		if( !is_subshell && is_interactive && !is_block)
		{
			signal_block();
			if( tcsetpgrp (0, getpid()) )
			{
				debug( 1, L"Could not return shell to foreground" );
				wperror( L"tcsetpgrp" );
				return;
			}
			
			/* 
			   Save jobs terminal modes.  
			*/
			if( tcgetattr (0, &j->tmodes) )
			{
				debug( 1, L"Could not return shell to foreground" );
				wperror( L"tcgetattr" );
				return;
			}
			
			/* 
			   Restore the shell's terminal modes.  
			*/
			if( tcsetattr (0, TCSADRAIN, &shell_modes))
			{
				debug( 1, L"Could not return shell to foreground" );
				wperror( L"tcsetattr" );
				return;
			}
			signal_unblock();
		}
	}
}

void proc_sanity_check()
{
	job_t *j;
	job_t *fg_job=0;
	
	for( j = first_job; j ; j=j->next )
	{
		process_t *p;

		if( !j->constructed )
			continue;
		
		
		validate_pointer( j->command, 
						  L"Job command", 
						  0 );
		validate_pointer( j->first_process,
						  L"Process list pointer",
						  0 );
		validate_pointer( j->next, 
						  L"Job list pointer",
						  1 );
		validate_pointer( j->command, 
						  L"Job command",
						  0 );
		/*
		  More than one foreground job?
		*/
		if( j->fg && !(job_is_stopped(j) || job_last_is_completed(j) ) )
		{
			if( fg_job != 0 )
			{
				debug( 0, 
					   L"More than one job in foreground:\n"
					   L"job 1: %ls\njob 2: %ls",
					   fg_job->command,
					   j->command );
				sanity_lose();
			}
			fg_job = j;
		}
		
   		p = j->first_process;
		while( p )
		{			
			validate_pointer( p->argv, L"Process argument list", 0 );
			validate_pointer( p->argv[0], L"Process name", 0 );
			validate_pointer( p->next, L"Process list pointer", 1 );
			validate_pointer( p->actual_cmd, L"Process command", 1 );
			
			if ( (p->stopped & (~0x00000001)) != 0 )
			{
				debug( 0,
					   L"Job %ls, process %ls "
					   L"has inconsistent state \'stopped\'=%d",
					   j->command, 
					   p->argv[0],
					   p->stopped );
				sanity_lose();
			}
			
			if ( (p->completed & (~0x00000001)) != 0 )
			{
				debug( 0,
					   L"Job %ls, process %ls "
					   L"has inconsistent state \'completed\'=%d",
					   j->command, 
					   p->argv[0],
					   p->completed );
				sanity_lose();
			}
			
			p=p->next;
		}
		
	}	
}

