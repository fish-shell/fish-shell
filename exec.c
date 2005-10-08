/** \file exec.c
	Functions for executing a program.

	Some of the code in this file is based on code from the Glibc
	manual, though I the changes performed have been massive.
*/

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

#include "config.h"
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

/**
   Prototype for the getpgid library function. The prototype for this
   function seems to be missing in glibc, at least I've not found any
   combination of #includes, macros and compiler switches that will
   include it.
*/
pid_t getpgid( pid_t pid );

/**
   file descriptor redirection error message
*/
#define FD_ERROR   L"An error occurred while redirecting file descriptor %d"
/**
   file redirection error message
*/
#define FILE_ERROR L"An error occurred while redirecting file '%ls'"
/**
   fork error message
*/
#define FORK_ERROR L"Could not create child process - exiting"

/**
   List of all pipes used by internal pipes. These must be closed in
   many situations in order to make sure that stray fds aren't lying
   around.
*/
array_list_t *open_fds=0;

void exec_close( int fd )
{
	int i;
	
	while( close(fd) == -1 )
	{
		if( errno != EINTR )
		{
			debug( 1, FD_ERROR, fd );
			wperror( L"close" );
		}
	}

	if( open_fds )
	{
		for( i=0; i<al_get_count( open_fds ); i++ )
		{
			int n = (int)(long)al_get( open_fds, i );
			if( n == fd )
			{
				al_set( open_fds,
						i,
						al_get( open_fds,
								al_get_count( open_fds ) -1 ) );
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
	
	if( open_fds == 0 )
	{
		open_fds = malloc( sizeof( array_list_t ) );
		if(!open_fds )
			die_mem();
		al_init( open_fds );
	}
	
	if( res != -1 )
	{
		debug( 4, L"Created pipe using fds %d and %d", fd[0], fd[1]);
		
		al_push( open_fds, (void *)(long)fd[0] );
		al_push( open_fds, (void *)(long)fd[1] );		
	}
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
		if( io->pipe_fd[0] == fd ||
			io->pipe_fd[1] == fd )
			return 1;
	}
		
	return use_fd_in_pipe( fd, io->next );
}


/**
   Close all fds in open_fds, except for those that are mentioned in
   the redirection list io

   \param io the list of io redirections for this job. Pipes mentioned here should not be closed.
*/
static void close_unused_internal_pipes( io_data_t *io )
{
	int i=0;
	
	if( open_fds )
	{
		for( ;i<al_get_count( open_fds ); i++ )
		{
			int n = (int)(long)al_get( open_fds, i );
			if( !use_fd_in_pipe( n, io) )
			{
				debug( 4, L"Close fd %d, used in other context", n );
				exec_close( n );
				i--;
			}
		}
	}
}

void exec_init()
{
	
}

void exec_destroy()
{
	if( open_fds )
	{
		al_destroy( open_fds );
		free( open_fds );
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
			if(io->pipe_fd[i] == fd )
			{
				while(1)
				{
					if( (io->pipe_fd[i] = dup(fd)) == -1)
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


static void handle_child_io( io_data_t *io )
{

	close_unused_internal_pipes( io );
	
	if( env_universal_server.fd >= 0 )
		exec_close( env_universal_server.fd );

	for( ; io; io=io->next )
	{
		int tmp;

		if( io->io_mode == IO_FD && io->fd == io->old_fd )
		{
			continue;
		}

		if( io->fd > 2 )
		{
			/*
			  Make sure the fd used by this redirection is not used by i.e. a pipe. 
			*/
			free_fd( io, io->fd );
		}
				
		/*
		  We don't mind if this fails, it is just a speculative close
		  to make sure no unexpected untracked fd causes us to fail
		*/
		while( close(io->fd) == -1 )
		{
			if( errno != EINTR )
				break;
			
/*			debug( 1, 
				   FD_ERROR,
				   io->fd );
			wperror( L"close" );
			exit(1);*/
		}
		
		switch( io->io_mode )
		{
			case IO_CLOSE:
				break;
			case IO_FILE:
				if( (tmp=wopen( io->filename, io->flags, 0666 ))==-1 )
				{
					debug( 1, 
						   FILE_ERROR,
						   io->filename );
					
					wperror( L"open" );
					exit(1);
				}				
				else if( tmp != io->fd)
				{
					if(dup2( tmp, io->fd ) == -1 )
					{
						debug( 1, 
							   FD_ERROR,
							   io->fd );
						wperror( L"dup2" );
						exit(1);
					}
					exec_close( tmp );
				}				
				break;
			case IO_FD:
/*				debug( 3, L"Redirect fd %d in process %ls (%d) from fd %d",
  io->fd,
  p->actual_cmd,
  p->pid,
  io->old_fd );
*/			
				if( dup2( io->old_fd, io->fd ) == -1 )
				{
					debug( 1, 
						   FD_ERROR,
						   io->fd );
					wperror( L"dup2" );
					exit(1);					
				}
				break;
				
			case IO_BUFFER:
			case IO_PIPE:
			{
				
/*				debug( 3, L"Pipe fd %d in process %ls (%d) (Through fd %d)", 
  io->fd, 
  p->actual_cmd,
  p->pid,
  io->pipe_fd[io->fd] );
*/
				int fd_to_dup = io->fd;
				
				if( dup2( io->pipe_fd[fd_to_dup?1:0], io->fd ) == -1 )
				{
					debug( 1, PIPE_ERROR );
					wperror( L"dup2" );
					exit(1);
				}

				if( fd_to_dup != 0 )
				{
					exec_close( io->pipe_fd[0]);
					exec_close( io->pipe_fd[1]);
				}
				else
					exec_close( io->pipe_fd[0] );
				
/*				
  if( close( io[i].pipe_fd[ io->fd ] ) == -1 )
  {
  wperror( L"close" );
  exit(1);
  }
*/			
/*				if( close( io[i].pipe_fd[1] ) == -1 )
  {
  wperror( L"close" );
  exit(1);
  }
*/
		
/*		    fprintf( stderr, "fd %d points to fd %d\n", i, io[i].fd[i>0?1:0] );*/
				break;
			}
			
		}
	}
}


static void setup_child_process( job_t *j )
{
	if( is_interactive && !is_subshell && !is_block)
    {
		pid_t pid;
		/* 
		   Put the process into the process group and give the process group
		   the terminal, if appropriate.
		   This has to be done both by the shell and in the individual
		   child processes because of potential race conditions.  
		*/
		pid = getpid ();
		if (j->pgid == 0)
			j->pgid = pid;

		/* Wait till shell puts os in our own group */
		while( getpgrp() != j->pgid )
			sleep(0);
		
		/* Wait till shell gives us stdin */
		if ( j->fg )
		{
			while( tcgetpgrp( 0 ) != j->pgid )
				sleep(0);
		}
	}
	
	/* Set the handling for job control signals back to the default.  */
	signal_reset_handlers();
	
	sigset_t chldset; 
	sigemptyset( &chldset );
	sigaddset( &chldset, SIGCHLD );
	sigprocmask(SIG_UNBLOCK, &chldset, 0);
	
	handle_child_io( j->io );
}

								 
								 
/**
   This function is executed by the child process created by a call to
   fork().  This function begins by updating FDs as specified by the io
   parameter.  If the command requested for this process is not a
   builtin, execv is then called with the appropriate parameters. If
   the command is a builtin, the contents of out and err are printed.
*/
static void launch_process( process_t *p )
{
	
	/* Set the standard input/output channels of the new process.  */
		
	execve (wcs2str(p->actual_cmd), wcsv2strv( (const wchar_t **) p->argv), env_export_arr( 0 ) );
	debug( 0, 
		   L"Failed to execute process %ls",
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
   Make a copy of the specified io redirection chain, but change file redirection into fd redirection. 
*/

static io_data_t *io_transmogrify( io_data_t * in )
{
	io_data_t *out;

	if( !in )
		return 0;
	
	out = malloc( sizeof( io_data_t ) );
	if( !out )
		die_mem();
	
	out->fd = in->fd;
	out->io_mode = IO_FD;
	out->close_old = 1;
	
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
			
			if( (fd=wopen( in->filename, in->flags, 0666 ))==-1 )
			{
				debug( 1, 
					   FILE_ERROR,
					   in->filename );
								
				wperror( L"open" );
				exit(1);
			}	

			out->old_fd = fd;
			break;
		}
	}
	
	out->next = io_transmogrify( in->next );
	
	return out;
}

/**
   Free a transmogrified io chain.
*/
static void io_untransmogrify( io_data_t * in, io_data_t *out )
{
	if( !in )
		return;
	io_untransmogrify( in->next, out->next );
	switch( in->io_mode )
	{
		case IO_FILE:
			exec_close( out->old_fd );
			break;
	}	
	free(out);
}


/**
   Morph an io redirection chain into redirections suitable for
   passing to eval, call eval, and clean up morphed redirections.
   

   \param def the code to evaluate
   \param block_type the type of block to push on evaluation
   \param io the io redirections to be performed on this block
*/

static int internal_exec_helper( const wchar_t *def, 
								 int block_type,
								 io_data_t *io )
{
	int res=0;
	io_data_t *io_internal = io_transmogrify( io );
	int is_block_old=is_block;
	is_block=1;
	
	eval( def, io_internal, block_type );		
/*
	io_data_t *buff = io_get( io, 1 );
	if( buff && buff->io_mode == IO_BUFFER )
	fwprintf( stderr, L"block %ls produced %d bytes of output\n", 
	def,
	buff->out_buffer->used );
*/
	io_untransmogrify( io, io_internal );
	if( !is_event )
		job_do_notification();
	is_block=is_block_old;
	return res;
}

/*
static void io_print( io_data_t *io )
{
	if( !io )
	{
		fwprintf( stderr, L"\n" );
		return;
	}
	

	fwprintf( stderr, L"IO fd %d, type ",
			  io->fd );
	switch( io->io_mode )
	{
		case IO_PIPE:
			fwprintf(stderr, L"PIPE, data %d\n", io->pipe_fd[io->fd?1:0] );
			break;
		
		case IO_FD:
			fwprintf(stderr, L"FD, copy %d\n", io->old_fd );
			break;

		case IO_BUFFER:
			fwprintf( stderr, L"BUFFER\n" );
			break;
			
		default:
			fwprintf( stderr, L"OTHER\n" );
	}

	io_print( io->next );
	
}
*/

static int handle_new_child( job_t *j, process_t *p )
{
	
	if(is_interactive  && !is_subshell && !is_block)
	{
		int new_pgid=0;
		
		if (!j->pgid)
		{
			new_pgid=1;			
			j->pgid = p->pid;
		}
		
		if( setpgid (p->pid, j->pgid) )
		{						
			if( getpgid( p->pid) != j->pgid )
			{
				debug( 1, 
					   L"Could not send process %d from group %d to group %d",
					   p->pid, 
					   getpgid( p->pid),
					   j->pgid );
				wperror( L"setpgid" );
			}

		}

		if( j->fg )
		{
			while( 1)
			{
				if( tcsetpgrp (0, j->pgid) )
				{
					if( errno != EINTR )
					{
						debug( 1, L"Could not send job %d ('%ls')to foreground", 
							   j->job_id, 
							   j->command );
						wperror( L"tcsetpgrp" );
						return -1;
					}
				}
				else
					break;
			}
		}

		if( j->fg && new_pgid)
		{
			while( 1 )
			{
				if( tcsetpgrp (0, j->pgid) )
				{
					if( errno != EINTR )
					{
						debug( 1, L"Could not send job %d ('%ls')to foreground", 
							   j->job_id, 
							   j->command );
						wperror( L"tcsetpgrp" );
						return -1;
					}
				}
				else
					break;
			}
		}
		
	}
	else
	{
		j->pgid = getpid();
	}
	return 0;
	
}



void exec( job_t *j )
{
	process_t *p;
	pid_t pid;
	int mypipe[2];
	sigset_t chldset; 
	sigemptyset( &chldset );
	sigaddset( &chldset, SIGCHLD );
	int skip_fork;
	
	io_data_t pipe_read, pipe_write;
	io_data_t *tmp;

	io_data_t *io_buffer =0;
		
	debug( 4, L"Exec job %ls with id %d", j->command, j->job_id );	
	
	if( j->first_process->type==INTERNAL_EXEC )
	{
		/*
		  Do a regular launch -  but without forking first...
		*/
		setup_child_process( j );
		launch_process( j->first_process );
		/*
		  launch_process _never_ returns...
		*/
	}
	

	pipe_read.fd=0;
	pipe_write.fd=1;
	pipe_read.io_mode=IO_PIPE;
	pipe_read.pipe_fd[0] = -1;
	pipe_read.pipe_fd[1] = -1;
	pipe_write.io_mode=IO_PIPE;
	pipe_read.next=0;
	pipe_write.next=0;
	pipe_write.pipe_fd[0]=pipe_write.pipe_fd[1]=0;	

	//fwprintf( stderr, L"Run command %ls\n", j->command );
	
	if( block_io )
	{
		if( j->io )
			j->io = io_add( io_duplicate(block_io), j->io );
		else
			j->io=io_duplicate(block_io);				
	}
	
	j->io = io_add( j->io, &pipe_write );
	
	for (p = j->first_process; p; p = p->next)
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
		
		if (p->next)
		{
			if (exec_pipe( mypipe ) == -1)
			{
				debug( 1, PIPE_ERROR );
				wperror (L"pipe");
				exit (1);
			}

/*			fwprintf( stderr,
  L"Make pipe from %ls to %ls using fds %d and %d\n", 
  p->actual_cmd,
  p->next->actual_cmd,
  mypipe[0],
  mypipe[1] );
*/
			memcpy( pipe_write.pipe_fd, mypipe, sizeof(int)*2);
		}
		else
		{
			/*
			  This is the last element of the pipeline.
			*/

			/*
			  Remove the io redirection for pipe output.
			*/
			j->io = io_remove( j->io, &pipe_write );
			
		}

		switch( p->type )
		{
			case INTERNAL_FUNCTION:
			{
				wchar_t **arg;
				int i;
				string_buffer_t sb;
			
				const wchar_t * def = function_get_definition( p->argv[0] );
//			fwprintf( stderr, L"run function %ls\n", argv[0] );
				if( def == 0 )
				{
					debug( 0, L"Unknown function %ls", p->argv[0] );
					break;
				}
				parser_push_block( FUNCTION_CALL );
				
				if( builtin_count_args(p->argv)>1 )
				{
					sb_init( &sb );
				
					for( i=1,arg = p->argv+1; *arg; i++, arg++ )
					{
						if( i != 1 )
							sb_append( &sb, ARRAY_SEP_STR );
						sb_append( &sb, *arg );
					}

					env_set( L"argv", (wchar_t *)sb.buff, ENV_LOCAL );
					sb_destroy( &sb );
				}
				parser_forbid_function( p->argv[0] );

				if( p->next )
				{
//					fwprintf( stderr, L"Function %ls\n", def );
					io_buffer = io_buffer_create();					
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
//					fwprintf( stderr, L"Block %ls\n", p->argv[0] );
					io_buffer = io_buffer_create();					
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
								builtin_stdin = in->old_fd;
/*								fwprintf( stderr, 
  L"Input redirection for builtin '%ls' from fd %d\n", 
  p->argv[0],
  in->old_fd );
*/
								break;
							}
							case IO_PIPE:
							{
								builtin_stdin = in->pipe_fd[0];
								break;
							}
							
							case IO_FILE:
							{
								int new_fd;
/*								
  fwprintf( stderr, 
  L"Input redirection for builtin from file %ls\n",
  in->filename);
*/
								new_fd=wopen( in->filename, in->flags, 0666 );
								if( new_fd == -1 )
								{
									wperror( L"wopen" );
								}
								else
								{
									builtin_stdin = new_fd;
									
									close_stdin = 1;
								}
								
								break;
							}

							default:
							{
								debug( 1, 
									   L"Unknown input redirection type %d",
									   in->io_mode);
								break;
							}
						
						}
					}
				}
				else
				{
					builtin_stdin = pipe_read.pipe_fd[0];
				}

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
				p->status = builtin_run( p->argv );

				/*
				  Restore the fg flag, which is temporarily set to
				  false during builtin execution so as not to confuse
				  some job-handling builtins.
				*/
				j->fg = fg;


/*			if( is_interactive )
  fwprintf( stderr, L"Builtin '%ls' finished\n", j->command );
*/
				if( close_stdin )
				{
//					fwprintf( stderr, L"Close builtin_stdin\n" );					
					exec_close( builtin_stdin );
				}				
				break;				
			}
		}


		switch( p->type )
		{
			case INTERNAL_BLOCK:
			case INTERNAL_FUNCTION:
			{
				int status = proc_get_last_status();
				
				/*
				  Handle output from a block or function. This usually
				  means do nothing, but in the case of pipes, we have
				  to buffer such io, since otherwisethe internal pipe
				  buffer might overflow.
				*/
				if( !io_buffer)
				{
					p->completed = 1;
					break;
				}

				j->io = io_remove( j->io, io_buffer );
				
				io_buffer_read( io_buffer );
				
				if( io_buffer->out_buffer->used != 0 )
				{
					
					/*Temporary signal block for SIGCHLD */
					sigprocmask(SIG_BLOCK, &chldset, 0);
					pid = fork ();
					if (pid == 0)
					{
						/*
						  This is the child process. Write out the contents of the pipeline.
						*/
						p->pid = getpid();
						setup_child_process( j );
						write( io_buffer->fd, 
							   io_buffer->out_buffer->buff, 
							   io_buffer->out_buffer->used );
						exit( status );
					}
					else if (pid < 0)
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
						   information on the child, and possibly fice
						   it control over the terminal.
						*/
						p->pid = pid;						
						if( handle_new_child( j, p ) )
							return;										
					}					
					
				}
				
				io_buffer_destroy( io_buffer );
				
				io_buffer=0;
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
//				fwprintf( stderr, L"Skip fork of %ls\n", j->command );				
					char *res = wcs2str( (wchar_t *)sb_out->buff );				
					b_append( io->out_buffer, res, strlen( res ) );
					skip_fork = 1;
					free( res );				
				}

				if( skip_fork )
				{
					p->completed=1;
					if( p->next == 0 )
					{
						debug( 3, L"Set status of %ls to %d using short circut", j->command, p->status );
						
						proc_set_last_status( p->status );
						proc_set_last_status( j->negate?(p->status?0:1):p->status );
					}
					break;
					
				}

				/*Temporary signal block for SIGCHLD */
				sigprocmask(SIG_BLOCK, &chldset, 0);
				pid = fork ();
				if (pid == 0)
				{
					/*
					  This is the child process. 
					*/
					p->pid = getpid();
					setup_child_process( j );
					if( sb_out->used )
						fwprintf( stdout, L"%ls", sb_out->buff );
					if( sb_err->used )
						fwprintf( stderr, L"%ls", sb_err->buff );
					
					exit( p->status );
						
				}
				else if (pid < 0)
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
					   information on the child, and possibly fice
					   it control over the terminal.
					*/
					p->pid = pid;
						
					if( handle_new_child( j, p ) )
						return;										
				}					
				
				break;
			}
			
			case EXTERNAL:
			{
				/*Temporary signal block for SIGCHLD */
				sigprocmask(SIG_BLOCK, &chldset, 0);
		
//			fwprintf( stderr, 
//					  L"fork on %ls\n", j->command );
				pid = fork ();
				if (pid == 0)
				{
					/*
					  This is the child process. 
					*/
					p->pid = getpid();
					setup_child_process( j );
					launch_process( p );

					/*
					  launch_process _never_ returns...
					*/
				}
				else if (pid < 0)
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
					   information on the child, and possibly fice
					   it control over the terminal.
					*/
					p->pid = pid;

					if( handle_new_child( j, p ) )
						return;
										
				}
				break;
			}
			
		}


		if(p->type == INTERNAL_BUILTIN)
			builtin_pop_io();			
		
		sigprocmask(SIG_UNBLOCK, &chldset, 0);
		
		/* 
		   Close the pipe the current process uses to read from the previous process_t 
		*/
		if( pipe_read.pipe_fd[0] >= 0 )
			exec_close( pipe_read.pipe_fd[0] );
		/* 
		   Set up the pipe the next process uses to read from the current process_t 
		*/
		if( p->next )
			pipe_read.pipe_fd[0] = mypipe[0];
		
		/* 
		   If there is a next process, close the output end of the
		   pipe (the child subprocess already has a copy of the pipe).
		*/
		if( p->next )
		{
			exec_close(mypipe[1]);
		}		
	}

	debug( 3, L"Job is constructed" );

	j->io = io_remove( j->io, &pipe_read );

	for( tmp = block_io; tmp; tmp=tmp->next )
		j->io = io_remove( j->io, tmp );
	
	j->constructed = 1;

	if( !j->fg )
	{
		proc_last_bg_pid = j->pgid;
	}
	
	job_continue (j, 0);

	debug( 3, L"End of exec()" );
	
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
		debug( 1, L"Sent null command to subshell" );		
		return 0;		
	}

	is_subshell=1;	
	io_buffer= io_buffer_create();
	
	prev_status = proc_get_last_status();
	
	eval( cmd, io_buffer, SUBST );
	
	debug( 4, L"exec_read_io_buffer on cmdsub '%ls'", cmd );
	io_buffer_read( io_buffer );
	
	status = proc_get_last_status();
	proc_set_last_status( prev_status );
	
	is_subshell = prev_subshell;
	
	b_append( io_buffer->out_buffer, &z, 1 );
	
	begin=end=io_buffer->out_buffer->buff;	
	
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
							al_push( l, el );
					}				
					io_buffer_destroy( io_buffer );
				
					return status;
				
				case '\n':
				{
					wchar_t *el;
					*end=0;
					el = str2wcs( begin );				
					al_push( l, el );
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
