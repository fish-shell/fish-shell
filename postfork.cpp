/** \file postfork.cpp

	Functions that we may safely call after fork().
*/

#include <fcntl.h>
#include "signal.h"
#include "postfork.h"
#include "iothread.h"
#include "exec.h"


/** The number of times to try to call fork() before giving up */
#define FORK_LAPS 5

/** The number of nanoseconds to sleep between attempts to call fork() */
#define FORK_SLEEP_TIME 1000000

/** Base open mode to pass to calls to open */
#define OPEN_MASK 0666

/** fork error message */
#define FORK_ERROR _( L"Could not create child process - exiting" )

/** file redirection clobbering error message */
#define NOCLOB_ERROR _( L"The file '%ls' already exists" )

/** file redirection error message */
#define FILE_ERROR _( L"An error occurred while redirecting file '%ls'" )

/** file descriptor redirection error message */
#define FD_ERROR   _( L"An error occurred while redirecting file descriptor %d" )


// PCA These calls to debug are rather sketchy because they may allocate memory. Fortunately they only occur if an error occurs.
int set_child_group( job_t *j, process_t *p, int print_errors )
{
	int res = 0;
	
	if( job_get_flag( j, JOB_CONTROL ) )
	{
		if (!j->pgid)
		{
			j->pgid = p->pid;
		}
		
		if( setpgid (p->pid, j->pgid) )
		{
			if( getpgid( p->pid) != j->pgid && print_errors )
			{
                // PCA FIXME This is sketchy to do in a forked child because it may allocate memory. This needs to call only safe functions.
				debug( 1, 
				       _( L"Could not send process %d, '%ls' in job %d, '%ls' from group %d to group %d" ),
				       p->pid,
				       p->argv0(),
				       j->job_id,
				       j->command_cstr(),
				       getpgid( p->pid),
				       j->pgid );

				wperror( L"setpgid" );
				res = -1;
			}
		}
	}
	else
	{
		j->pgid = getpid();
	}

	if( job_get_flag( j, JOB_TERMINAL ) && job_get_flag( j, JOB_FOREGROUND ) )
	{
		if( tcsetpgrp (0, j->pgid) && print_errors )
		{
			debug( 1, _( L"Could not send job %d ('%ls') to foreground" ), 
				   j->job_id, 
				   j->command_cstr() );
			wperror( L"tcsetpgrp" );
			res = -1;
		}
	}

	return res;
}

/** Make sure the fd used by this redirection is not used by i.e. a pipe.  */
static void free_fd( io_data_t *io, int fd )
{
	if( !io )
		return;
	
	if( ( io->io_mode == IO_PIPE ) || ( io->io_mode == IO_BUFFER ) )
	{
		int i;
		for( i=0; i<2; i++ )
		{
			if(io->param1.pipe_fd[i] == fd )
			{
				while(1)
				{
					if( (io->param1.pipe_fd[i] = dup(fd)) == -1)
					{
						 if( errno != EINTR )
						{
							debug( 1, 
								   FD_ERROR,
								   fd );							
							wperror( L"dup" );
							FATAL_EXIT();
						}
					}
					else
					{
						break;
					}
				}
			}
		}
	}
	free_fd( io->next, fd );
}


/**
   Set up a childs io redirections. Should only be called by
   setup_child_process(). Does the following: First it closes any open
   file descriptors not related to the child by calling
   close_unused_internal_pipes() and closing the universal variable
   server file descriptor. It then goes on to perform all the
   redirections described by \c io.

   \param io the list of IO redirections for the child

   \return 0 on sucess, -1 on failiure
*/
static int handle_child_io( io_data_t *io )
{

	close_unused_internal_pipes( io );

	for( ; io; io=io->next )
	{
		int tmp;

		if( io->io_mode == IO_FD && io->fd == io->param1.old_fd )
		{
			continue;
		}

		if( io->fd > 2 )
		{
			/*
			  Make sure the fd used by this redirection is not used by e.g. a pipe. 
			*/
			free_fd( io, io->fd );
		}
				
		switch( io->io_mode )
		{
			case IO_CLOSE:
			{
				if( close(io->fd) )
				{
					debug( 0, _(L"Failed to close file descriptor %d"), io->fd );
					wperror( L"close" );
				}
				break;
			}

			case IO_FILE:
			{
				if( (tmp=wopen( io->filename,
						io->param2.flags, OPEN_MASK ) )==-1 )
				{
					if( ( io->param2.flags & O_EXCL ) &&
					    ( errno ==EEXIST ) )
					{
						debug( 1, 
						       NOCLOB_ERROR,
						       io->filename.c_str() );
					}
					else
					{
						debug( 1, 
						       FILE_ERROR,
						       io->filename.c_str() );
										
						wperror( L"open" );
					}
					
					return -1;
				}
				else if( tmp != io->fd)
				{
					/*
					  This call will sometimes fail, but that is ok,
					  this is just a precausion.
					*/
					close(io->fd);
							
					if(dup2( tmp, io->fd ) == -1 )
					{
						debug( 1, 
							   FD_ERROR,
							   io->fd );
						wperror( L"dup2" );
						return -1;
					}
					exec_close( tmp );
				}				
				break;
			}
			
			case IO_FD:
			{
				/*
				  This call will sometimes fail, but that is ok,
				  this is just a precausion.
				*/
				close(io->fd);

				if( dup2( io->param1.old_fd, io->fd ) == -1 )
				{
					debug( 1, 
						   FD_ERROR,
						   io->fd );
					wperror( L"dup2" );
					return -1;
				}
				break;
			}
			
			case IO_BUFFER:
			case IO_PIPE:
			{
				int write_pipe;
				
				write_pipe = !io->is_input;
/*
				debug( 0,
					   L"%ls %ls on fd %d (%d %d)", 
					   write_pipe?L"write":L"read", 
					   (io->io_mode == IO_BUFFER)?L"buffer":L"pipe",
					   io->fd,
					   io->param1.pipe_fd[0],
					   io->param1.pipe_fd[1]);
*/
				if( dup2( io->param1.pipe_fd[write_pipe], io->fd ) != io->fd )
				{
					debug( 1, PIPE_ERROR );
					wperror( L"dup2" );
					return -1;
				}

				if( write_pipe ) 
				{
					exec_close( io->param1.pipe_fd[0]);
					exec_close( io->param1.pipe_fd[1]);
				}
				else
				{
					exec_close( io->param1.pipe_fd[0] );
				}
				break;
			}
			
		}
	}

	return 0;
	
}


int setup_child_process( job_t *j, process_t *p )
{
	int res=0;
	
	if( p )
	{
		res = set_child_group( j, p, 1 );
	}
	
	if( !res )	
	{
		res = handle_child_io( j->io );
		if( p != 0 && res )
		{
			exit_without_destructors( 1 );
		}
	}
	
	/* Set the handling for job control signals back to the default.  */
	if( !res )
	{
		signal_reset_handlers();
	}
	
	/* Remove all signal blocks */
	signal_unblock();	
	
	return res;
	
}

/**
   This function is a wrapper around fork. If the fork calls fails
   with EAGAIN, it is retried FORK_LAPS times, with a very slight
   delay between each lap. If fork fails even then, the process will
   exit with an error message.
*/
pid_t execute_fork(bool wait_for_threads_to_die)
{
    ASSERT_IS_MAIN_THREAD();
    
    if (wait_for_threads_to_die) {
        /* Make sure we have no outstanding threads before we fork. This is a pretty sketchy thing to do here, both because exec.cpp shouldn't have to know about iothreads, and because the completion handlers may do unexpected things. */
        iothread_drain_all();
    }
    
	pid_t pid;
	struct timespec pollint;
	int i;
	
	for( i=0; i<FORK_LAPS; i++ )
	{
		pid = fork();
		if( pid >= 0)
		{
			return pid;
		}
		
		if( errno != EAGAIN )
		{
			break;
		}

		pollint.tv_sec = 0;
		pollint.tv_nsec = FORK_SLEEP_TIME;

		/*
		  Don't sleep on the final lap - sleeping might change the
		  value of errno, which will break the error reporting below.
		*/
		if( i != FORK_LAPS-1 )
		{
			nanosleep( &pollint, NULL );
		}
	}
	
	debug( 0, FORK_ERROR );
	wperror (L"fork");
	FATAL_EXIT();
}
