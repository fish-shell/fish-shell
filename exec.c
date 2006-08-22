/** \file exec.c
	Functions for executing a program.

	Some of the code in this file is based on code from the Glibc
	manual, though I the changes performed have been massive.
*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <wchar.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <sys/wait.h>
#include <assert.h>
#include <dirent.h>

#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "wutil.h"
#include "proc.h"
#include "exec.h"
#include "parser.h"
#include "builtin.h"
#include "function.h"
#include "env.h"
#include "wildcard.h"
#include "sanity.h"
#include "expand.h"
#include "signal.h"
#include "env_universal.h"

#include "halloc.h"
#include "halloc_util.h"
#include "parse_util.h"

/**
   Prototype for the getpgid library function. The prototype for this
   function seems to be missing in glibc, at least I've not found any
   combination of includes, macros and compiler switches that will
   include it.
*/
pid_t getpgid( pid_t pid );

/**
   file descriptor redirection error message
*/
#define FD_ERROR   _( L"An error occurred while redirecting file descriptor %d" )
/**
   file redirection error message
*/
#define FILE_ERROR _( L"An error occurred while redirecting file '%ls'" )
/**
   fork error message
*/
#define FORK_ERROR _( L"Could not create child process - exiting" )


/**
   List of all pipes used by internal pipes. These must be closed in
   many situations in order to make sure that stray fds aren't lying
   around.
*/
static array_list_t *open_fds=0;

static int set_child_group( job_t *j, process_t *p, int print_errors );

void exec_close( int fd )
{
	int i;
	
	while( close(fd) == -1 )
	{
		if( errno != EINTR )
		{
			debug( 1, FD_ERROR, fd );
			wperror( L"close" );
			break;
		}
	}
	
	if( open_fds )
	{
		for( i=0; i<al_get_count( open_fds ); i++ )
		{
			int n = (int)al_get_long( open_fds, i );
			if( n == fd )
			{
				al_set_long( open_fds,
							 i,
							 al_get_long( open_fds, al_get_count( open_fds ) -1 ) );
				al_truncate( open_fds, 
							 al_get_count( open_fds ) -1 );
				break;
			}
		}
	}

}

int exec_pipe( int fd[2])
{
	int res;
	
	while( ( res=pipe( fd ) ) )
	{
		if( errno != EINTR )
		{
			wperror(L"pipe");
			return res;
		}
	}	
	
	debug( 4, L"Created pipe using fds %d and %d", fd[0], fd[1]);
	
	if( open_fds == 0 )
	{
		open_fds = al_halloc( global_context );
	}
	
	al_push_long( open_fds, (long)fd[0] );
	al_push_long( open_fds, (long)fd[1] );		
	
	return res;
}

/**
   Check if the specified fd is used as a part of a pipeline in the
   specidied set of IO redirections.

   \param fd the fd to search for
   \param io the set of io redirections to search in
*/
static int use_fd_in_pipe( int fd, io_data_t *io )
{	
	if( !io )
		return 0;
	
	if( ( io->io_mode == IO_BUFFER ) || 
		( io->io_mode == IO_PIPE ) )
	{
		if( io->param1.pipe_fd[0] == fd ||
			io->param1.pipe_fd[1] == fd )
			return 1;
	}
		
	return use_fd_in_pipe( fd, io->next );
}


/**
   Close all fds in open_fds, except for those that are mentioned in
   the redirection list io. This should make sure that there are no
   stray opened file descriptors in the child.
   
   \param io the list of io redirections for this job. Pipes mentioned
   here should not be closed.
*/
static void close_unused_internal_pipes( io_data_t *io )
{
	int i=0;
	
	if( open_fds )
	{
		for( ;i<al_get_count( open_fds ); i++ )
		{
			int n = (long)al_get_long( open_fds, i );
			if( !use_fd_in_pipe( n, io) )
			{
				debug( 4, L"Close fd %d, used in other context", n );
				exec_close( n );
				i--;
			}
		}
	}
}

/**
   Make sure the fd used by this redirection is not used by i.e. a pipe. 
*/
void free_fd( io_data_t *io, int fd )
{
	if( !io )
		return;
	
	if( io->io_mode == IO_PIPE )
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
							exit(1);
						}
					}
					else
						break;
				}
			}
		}
	}
}

/**
   Set up a childs io redirections. Should only be called by
   setup_child_process(). Does the following: First it closes any open
   file descriptors not related to the child by calling
   close_unused_internal_pipes() and closing the universal variable
   server file descriptor. It then goes on to perform all the
   redirections described by \c io.

   \param io the list of IO redirections for the child
   \param exit_on_error whether to call exit() on errors

   \return 0 on sucess, -1 on failiure
*/
static int handle_child_io( io_data_t *io, int exit_on_error )
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
				if( close(io->fd) )
				{
					debug( 0, _(L"Failed to close file descriptor %d"), io->fd );
					wperror( L"close" );
				}
				break;
			case IO_FILE:
			{
				if( (tmp=wopen( io->param1.filename,
                                io->param2.flags, 0777 ) )==-1 )
				{
					debug( 1, 
						   FILE_ERROR,
						   io->param1.filename );
					
					wperror( L"open" );
					if( exit_on_error )
					{
						exit(1);
					}
					else
					{
						return -1;
					}					
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
						if( exit_on_error )
						{
							exit(1);
						}
						else
						{
							return -1;
						}
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
					if( exit_on_error )
					{
						exit(1);
					}
					else
					{
						return -1;
					}
				}
				break;
			}
			
			case IO_BUFFER:
			case IO_PIPE:
			{
				int fd_to_dup = io->fd;

				/*
				  This call will sometimes fail, but that is ok,
				  this is just a precausion.
				*/
				close(io->fd);
				
				if( dup2( io->param1.pipe_fd[fd_to_dup?1:0], io->fd ) == -1 )
				{
					debug( 1, PIPE_ERROR );
					wperror( L"dup2" );
					if( exit_on_error )
					{
						exit(1);
					}
					else
					{
						return -1;
					}					

				}

				if( fd_to_dup != 0 )
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

	if( env_universal_server.fd >= 0 )
		exec_close( env_universal_server.fd );

	return 0;
	
}

/**
   Initialize a new child process. This should be called right away
   after forking in the child process. If job control is suitable, the
   process is put in the jobs group, all signal handlers are reset,
   SIGCHLD is unblocked (the exec call blocks blocks SIGCHLD), and all
   IO redirections and other file descriptor actions are performed.

   \param j the job to set up the IO for
   \param p the child process to set up

   \return 0 on sucess, -1 on failiure
*/
static int setup_child_process( job_t *j, process_t *p )
{
	int res=0;
	
	if( p )
	{
		res = set_child_group( j, p, 1 );
	}
	
	if( !res )	
	{
		res = handle_child_io( j->io, (p!=0) );
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
   This function is executed by the child process created by a call to
   fork(). It should be called after \c setup_child_process. It calls
   execve to replace the fish process image with the command specified
   in \c p. It never returns.
*/
static void launch_process( process_t *p )
{
//	debug( 1, L"exec '%ls'", p->argv[0] );
		
	execve (wcs2str(p->actual_cmd), wcsv2strv( (const wchar_t **) p->argv), env_export_arr( 0 ) );
	debug( 0, 
		   _( L"Failed to execute process '%ls'" ),
		   p->actual_cmd );
	wperror( L"execve" );
	exit(1);
}


/**
   Check if the IO redirection chains contains redirections for the
   specified file descriptor
*/
static int has_fd( io_data_t *d, int fd )
{
	return io_get( d, fd ) != 0;
}


/**
   Free a transmogrified io chain. Only the chain itself and resources
   used by a transmogrified IO_FILE redirection are freed, since the
   original chain may still be needed.
*/
static void io_untransmogrify( io_data_t * in, io_data_t *out )
{
	if( !out )
		return;
	io_untransmogrify( in->next, out->next );
	switch( in->io_mode )
	{
		case IO_FILE:
			exec_close( out->param1.old_fd );
			break;
	}	
	free(out);
}


/**
   Make a copy of the specified io redirection chain, but change file
   redirection into fd redirection. This makes the redirection chain
   suitable for use as block-level io, since the file won't be
   repeatedly reopened for every command in the block.

   \return the transmogrified chain on sucess, or 0 on failiure
*/
static io_data_t *io_transmogrify( io_data_t * in )
{
	io_data_t *out;

	if( !in )
		return 0;
	
	out = malloc( sizeof( io_data_t ) );
	if( !out )
		DIE_MEM();
	
	out->fd = in->fd;
	out->io_mode = IO_FD;
	out->param2.close_old = 1;
	out->next=0;
		
	switch( in->io_mode )
	{
		/*
		  These redirections don't need transmogrification. They can be passed through.
		*/
		case IO_FD:
		case IO_CLOSE:
		case IO_BUFFER:
		case IO_PIPE:
		{
			memcpy( out, in, sizeof(io_data_t));
			break;
		}

		/*
		  Transmogrify file redirections
		*/
		case IO_FILE:
		{
			int fd;
			
			if( (fd=wopen( in->param1.filename, in->param2.flags, 0777 ) )==-1 )
			{
				debug( 1, 
					   FILE_ERROR,
					   in->param1.filename );
								
				wperror( L"open" );
				free( out );
				return 0;
			}	

			out->param1.old_fd = fd;
			break;
		}
	}
	
	if( in->next)
	{
		out->next = io_transmogrify( in->next );
		if( !out->next )
		{
			io_untransmogrify( in, out );
			return 0;
		}
	}
	
	return out;
}

/**
   Morph an io redirection chain into redirections suitable for
   passing to eval, call eval, and clean up morphed redirections.

   \param def the code to evaluate
   \param block_type the type of block to push on evaluation
   \param io the io redirections to be performed on this block
*/

static void internal_exec_helper( const wchar_t *def, 
								 int block_type,
								 io_data_t *io )
{
	io_data_t *io_internal = io_transmogrify( io );
	int is_block_old=is_block;
	is_block=1;
	
	/*
	  Did the transmogrification fail - if so, set error status and return
	*/
	if( io && !io_internal )
	{
		proc_set_last_status( 1 );
		return;
	}
	
	signal_unblock();
	
	eval( def, io_internal, block_type );		
	
	signal_block();
	
	io_untransmogrify( io, io_internal );
	job_reap( 0 );
	is_block=is_block_old;
}

/**
   This function should be called by both the parent process and the
   child right after fork() has been called. If job control is
   enabled, the child is put in the jobs group, and if the child is
   also in the foreground, it is also given control of the
   terminal. When called in the parent process, this function may
   fail, since the child might have already finished and called
   exit. The parent process may safely ignore the exit status of this
   call.

   Returns 0 on sucess, -1 on failiure. 
*/
static int set_child_group( job_t *j, process_t *p, int print_errors )
{
	int res = 0;
	
	if( j->job_control )
	{
		int new_pgid=0;
		
		if (!j->pgid)
		{
			new_pgid=1;			
			j->pgid = p->pid;
		}
		
		if( setpgid (p->pid, j->pgid) )
		{						
			if( getpgid( p->pid) != j->pgid && print_errors )
			{
				debug( 1, 
					   _( L"Could not send process %d from group %d to group %d" ),
					   p->pid, 
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

	if( j->terminal && j->fg )
	{
		if( tcsetpgrp (0, j->pgid) && print_errors )
		{
			debug( 1, _( L"Could not send job %d ('%ls') to foreground" ), 
				   j->job_id, 
				   j->command );
			wperror( L"tcsetpgrp" );
			res = -1;
		}
	}

	return res;
}



void exec( job_t *j )
{
	process_t *p;
	pid_t pid;
	int mypipe[2];
	sigset_t chldset; 
	int skip_fork;
	
	io_data_t pipe_read, pipe_write;
	io_data_t *tmp;

	io_data_t *io_buffer =0;

	/*
	  Set to 1 if something goes wrong while exec:ing the job, in which case the cleanup code will kick in.
	*/
	int exec_error=0;

	CHECK( j, );
		
	if( no_exec )
		return;
	
	

	sigemptyset( &chldset );
	sigaddset( &chldset, SIGCHLD );
	
	debug( 4, L"Exec job '%ls' with id %d", j->command, j->job_id );	
	
	if( block_io )
	{
		if( j->io )
			j->io = io_add( io_duplicate( j, block_io), j->io );
		else
			j->io=io_duplicate( j, block_io);				
	}

	io_data_t *input_redirect = io_get( j->io, 0 );
	
	if( input_redirect && 
		(input_redirect->io_mode == IO_BUFFER) && 
		input_redirect->param3.is_input )
	{
		/*
		  Input redirection - create a new gobetween process to take
		  care of buffering
		*/
		
		process_t *fake = halloc( j, sizeof(process_t) );
		fake->type = INTERNAL_BUFFER;
		fake->pipe_fd = 1;
		fake->next = j->first_process;
		j->first_process = fake;
	}

	if( j->first_process->type==INTERNAL_EXEC )
	{
		/*
		  Do a regular launch -  but without forking first...
		*/
		signal_block();

		/*
		  setup_child_process makes sure signals are properly set
		  up. It will also call signal_unblock
		*/
		if( !setup_child_process( j, 0 ) )
		{
			/*
			  launch_process never returns
			*/
			launch_process( j->first_process );
		}
		else
		{
			j->constructed=1;
			j->first_process->completed=1;
			return;
		}

	}	

	pipe_read.fd=0;
	pipe_write.fd=1;
	pipe_read.io_mode=IO_PIPE;
	pipe_read.param1.pipe_fd[0] = -1;
	pipe_read.param1.pipe_fd[1] = -1;
	pipe_write.io_mode=IO_PIPE;
	pipe_read.next=0;
	pipe_write.next=0;
	pipe_write.param1.pipe_fd[0]=pipe_write.param1.pipe_fd[1]=-1;	


	
	//fwprintf( stderr, L"Run command %ls\n", j->command );
	
	j->io = io_add( j->io, &pipe_write );
	
	signal_block();
	
	/*
	  This loop loops over every process_t in the job, starting it as
	  appropriate. This turns out to be rather complex, since a
	  process_t can be one of many rather different things.

	  The loop also has to handle pipelining between the jobs.
	*/
	
	for( p=j->first_process; p; p = p->next )
	{
		mypipe[1]=-1;
		skip_fork=0;
		
		pipe_write.fd = p->pipe_fd;

		/* 
		   This call is used so the global environment variable array is
		   regenerated, if needed, before the fork. That way, we avoid a
		   lot of duplicate work where EVERY child would need to generate
		   it
		*/
		if( p->type == EXTERNAL )
			env_export_arr( 1 );
		
		
		/*
		  Set up fd:s that will be used in the pipe 
		*/
		
		if( p == j->first_process->next )
		{
			j->io = io_add( j->io, &pipe_read );
		}
		
		if( p->next )
		{
//			debug( 1, L"%ls|%ls" , p->argv[0], p->next->argv[0]);
			
			if( exec_pipe( mypipe ) == -1 )
			{
				debug( 1, PIPE_ERROR );
				wperror (L"pipe");
				exec_error=1;
				break;
			}

			memcpy( pipe_write.param1.pipe_fd, mypipe, sizeof(int)*2);
		}
		else
		{
			/*
			  This is the last element of the pipeline.
			  Remove the io redirection for pipe output.
			*/
			j->io = io_remove( j->io, &pipe_write );
			
		}

		switch( p->type )
		{
			case INTERNAL_FUNCTION:
			{
				
				wchar_t * def = halloc_register( j, wcsdup( function_get_definition( p->argv[0] )));
				if( def == 0 )
				{
					debug( 0, _( L"Unknown function '%ls'" ), p->argv[0] );
					break;
				}
				
				parser_push_block( FUNCTION_CALL );
				
				current_block->param2.function_call_process = p;
				current_block->param1.function_name = halloc_register( current_block, wcsdup( p->argv[0] ) );
						
				parse_util_set_argv( p->argv+1 );
								
				parser_forbid_function( p->argv[0] );

				if( p->next )
				{
					io_buffer = io_buffer_create( 0 );					
					j->io = io_add( j->io, io_buffer );
				}
				
				internal_exec_helper( def, TOP, j->io );
				
				parser_allow_function();
				parser_pop_block();
				
				break;				
			}
			
			case INTERNAL_BLOCK:
			{
				if( p->next )
				{
					io_buffer = io_buffer_create( 0 );					
					j->io = io_add( j->io, io_buffer );
				}
								
				internal_exec_helper( p->argv[0], TOP, j->io );			
				break;
				
			}

			case INTERNAL_BUILTIN:
			{
				int builtin_stdin=0;
				int fg;
				int close_stdin=0;
				
				if( p == j->first_process )
				{
					io_data_t *in = io_get( j->io, 0 );
					
					if( in )
					{
						switch( in->io_mode )
						{
							
							case IO_FD:
							{
								builtin_stdin = in->param1.old_fd;
								break;
							}
							case IO_PIPE:
							{
								builtin_stdin = in->param1.pipe_fd[0];
								break;
							}
							
							case IO_FILE:
							{
								builtin_stdin=wopen( in->param1.filename,
                                              in->param2.flags, 0777 );
								if( builtin_stdin == -1 )
								{
									debug( 1, 
										   FILE_ERROR,
										   in->param1.filename );
									wperror( L"open" );
								}
								else
								{
									close_stdin = 1;
								}
								
								break;
							}

							default:
							{
								builtin_stdin=-1;
								debug( 1, 
									   _( L"Unknown input redirection type %d" ),
									   in->io_mode);
								break;
							}
						
						}
					}
				}
				else
				{
					builtin_stdin = pipe_read.param1.pipe_fd[0];
				}

				if( builtin_stdin == -1 )
				{
					exec_error=1;
					break;
				}
				else
				{
					builtin_push_io( builtin_stdin );
					
					/* 
					   Since this may be the foreground job, and since a
					   builtin may execute another foreground job, we need to
					   pretend to suspend this job while running the builtin.
					*/
					
					builtin_out_redirect = has_fd( j->io, 1 );
					builtin_err_redirect = has_fd( j->io, 2 );		
					fg = j->fg;
					j->fg = 0;
					
					signal_unblock();
					
					p->status = builtin_run( p->argv );
					
					signal_block();
					
					/*
					  Restore the fg flag, which is temporarily set to
					  false during builtin execution so as not to confuse
					  some job-handling builtins.
					*/
					j->fg = fg;
				}
				
				if( close_stdin )
				{
					exec_close( builtin_stdin );
				}				
				break;				
			}
		}
		
		if( exec_error )
			break;

		switch( p->type )
		{

			case INTERNAL_BLOCK:
			case INTERNAL_FUNCTION:
			{
				int status = proc_get_last_status();
						
				/*
				  Handle output from a block or function. This usually
				  means do nothing, but in the case of pipes, we have
				  to buffer such io, since otherwise the internal pipe
				  buffer might overflow.
				*/
				if( !io_buffer )
				{
					/*
					  No buffer, so we exit directly. This means we
					  have to manually set the exit status.
					*/
					if( p->next == 0 )
					{
						proc_set_last_status( j->negate?(status?0:1):status);
					}
					p->completed = 1;
					break;
				}

				j->io = io_remove( j->io, io_buffer );
				
				io_buffer_read( io_buffer );
				
				if( io_buffer->param2.out_buffer->used != 0 )
				{
					pid = fork();

					if( pid == 0 )
					{
						/*
						  This is the child process. Write out the contents of the pipeline.
						*/
						p->pid = getpid();
						setup_child_process( j, p );
						write( io_buffer->fd, 
							   io_buffer->param2.out_buffer->buff, 
							   io_buffer->param2.out_buffer->used );
						exit( status );
					}
					else if( pid < 0 )
					{
						/* The fork failed. */
						debug( 0, FORK_ERROR );
						wperror (L"fork");
						exit (1);
					}
					else
					{
						/* 
						   This is the parent process. Store away
						   information on the child, and possibly give
						   it control over the terminal.
						*/
						p->pid = pid;						
						set_child_group( j, p, 0 );
												
					}					
					
				}
				else
				{
					if( p->next == 0 )
					{
						proc_set_last_status( j->negate?(status?0:1):status);
					}
					p->completed = 1;
				}
				
				io_buffer_destroy( io_buffer );
				
				io_buffer=0;
				break;
				
			}


			case INTERNAL_BUFFER:
			{
		
				pid = fork();
				
				if( pid == 0 )
				{
					/*
					  This is the child process. Write out the contents of the pipeline.
					*/
					p->pid = getpid();
					setup_child_process( j, p );
					
					write( 1,
						   input_redirect->param2.out_buffer->buff, 
						   input_redirect->param2.out_buffer->used );
					exit( 0 );
				}
				else if( pid < 0 )
				{
					/* The fork failed. */
					debug( 0, FORK_ERROR );
					wperror (L"fork");
					exit (1);
				}
				else
				{
					/* 
					   This is the parent process. Store away
					   information on the child, and possibly give
					   it control over the terminal.
					*/
					p->pid = pid;						
					set_child_group( j, p, 0 );	
				}	

				break;				
			}
			
			case INTERNAL_BUILTIN:
			{
				int skip_fork=0;
				
				/*
				  If a builtin didn't produce any output, and it is not inside a pipeline, there is no need to fork 
				*/
				skip_fork =
					( !sb_out->used ) &&
					( !sb_err->used ) &&
					( !p->next );
				
				/*
				  If the output of a builtin is to be sent to an internal
				  buffer, there is no need to fork. This helps out the
				  performance quite a bit in complex completion code.
				*/

				io_data_t *io = io_get( j->io, 1 );		
				int buffer_stdout = io && io->io_mode == IO_BUFFER;
				
				if( ( !sb_err->used ) && 
					( !p->next ) &&
					( sb_out->used ) && 
					( buffer_stdout ) )
				{
					char *res = wcs2str( (wchar_t *)sb_out->buff );				
					b_append( io->param2.out_buffer, res, strlen( res ) );
					skip_fork = 1;
					free( res );				
				}

				if( skip_fork )
				{
					p->completed=1;
					if( p->next == 0 )
					{
						debug( 3, L"Set status of %ls to %d using short circut", j->command, p->status );
						
						proc_set_last_status( j->negate?(p->status?0:1):p->status );
					}
					break;
					
				}
				
				pid = fork();
				if( pid == 0 )
				{

					/*
					  This is the child process. Setup redirections,
					  print correct output to stdout and stderr, and
					  then exit.
					*/

					p->pid = getpid();
					setup_child_process( j, p );
					if( sb_out->used )
						fwprintf( stdout, L"%ls", sb_out->buff );
					if( sb_err->used )
						fwprintf( stderr, L"%ls", sb_err->buff );
					
					exit( p->status );
						
				}
				else if( pid < 0 )
				{
					/* The fork failed. */
					debug( 0, FORK_ERROR );
					wperror (L"fork");
					exit (1);
				}
				else
				{
					/* 
					   This is the parent process. Store away
					   information on the child, and possibly give
					   it control over the terminal.
					*/
					p->pid = pid;
						
					set_child_group( j, p, 0 );
										
				}					
				
				break;
			}
			
			case EXTERNAL:
			{
				pid = fork();
				if( pid == 0 )
				{
					/*
					  This is the child process. 
					*/
					p->pid = getpid();
					setup_child_process( j, p );
					launch_process( p );

					/*
					  launch_process _never_ returns...
					*/
				}
				else if( pid < 0 )
				{
					/* The fork failed. */
					debug( 0, FORK_ERROR );
					wperror( L"fork" );
					exit( 1 );
				}
				else
				{
					/* 
					   This is the parent process. Store away
					   information on the child, and possibly fice
					   it control over the terminal.
					*/
					p->pid = pid;

					set_child_group( j, p, 0 );
															
				}
				break;
			}
			
		}

		if( p->type == INTERNAL_BUILTIN )
			builtin_pop_io();
				
		/* 
		   Close the pipe the current process uses to read from the previous process_t 
		*/
		if( pipe_read.param1.pipe_fd[0] >= 0 )
			exec_close( pipe_read.param1.pipe_fd[0] );
		/* 
		   Set up the pipe the next process uses to read from the current process_t 
		*/
		if( p->next )
			pipe_read.param1.pipe_fd[0] = mypipe[0];
		
		/* 
		   If there is a next process in the pipeline, close the
		   output end of the current pipe (the surrent child
		   subprocess already has a copy of the pipe - this makes sure
		   we don't leak file descriptors either in the shell or in
		   the children).
		*/
		if( p->next )
		{
			exec_close(mypipe[1]);
		}		
	}

	signal_unblock();	

	debug( 3, L"Job is constructed" );

	j->io = io_remove( j->io, &pipe_read );

	for( tmp = block_io; tmp; tmp=tmp->next )
		j->io = io_remove( j->io, tmp );
	
	j->constructed = 1;

	if( !j->fg )
	{
		proc_last_bg_pid = j->pgid;
	}

	if( !exec_error )
	{
		job_continue (j, 0);
	}
	
}

int exec_subshell( const wchar_t *cmd, 
				   array_list_t *l )
{
	char *begin, *end;
	char z=0;
	int prev_subshell = is_subshell;
	int status, prev_status;
	io_data_t *io_buffer;

	if( !cmd )
	{
		debug( 1, 
			   _( L"Sent null command to subshell. This is a fish bug. If it can be reproduced, please send a bug report to %s." ), 
			   PACKAGE_BUGREPORT );		
		return 0;		
	}
	
	is_subshell=1;	
	io_buffer= io_buffer_create( 0 );
	
	prev_status = proc_get_last_status();
	
	eval( cmd, io_buffer, SUBST );
	
	io_buffer_read( io_buffer );
	
	status = proc_get_last_status();
	proc_set_last_status( prev_status );
	
	is_subshell = prev_subshell;
	
	b_append( io_buffer->param2.out_buffer, &z, 1 );
	
	begin=end=io_buffer->param2.out_buffer->buff;	

	if( l )
	{
		while( 1 )
		{
			switch( *end )
			{
				case 0:
				
					if( begin != end )
					{
						wchar_t *el = str2wcs( begin );
						if( el )
						{
							al_push( l, el );
						}
						else
						{
							debug( 2, L"Got null string on line %d of file %s", __LINE__, __FILE__ );
						}
					}				
					io_buffer_destroy( io_buffer );
					
					return status;
				
				case '\n':
				{
					wchar_t *el;
					*end=0;
					el = str2wcs( begin );
					if( el )
					{
						al_push( l, el );
					}
					else
					{
						debug( 2, L"Got null string on line %d of file %s", __LINE__, __FILE__ );
					}
					begin = end+1;
					break;
				}
			}
			end++;
		}
	}
	
	io_buffer_destroy( io_buffer );

	return status;	
}
