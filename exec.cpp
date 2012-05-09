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
#include <time.h>
#include <vector>
#include <deque>
#include <algorithm>
#include <memory>

#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif

#include "fallback.h"
#include "util.h"
#include "iothread.h"
#include "postfork.h"

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

#include "parse_util.h"

/**
   file descriptor redirection error message
*/
#define FD_ERROR   _( L"An error occurred while redirecting file descriptor %d" )

/**
   file descriptor redirection error message
*/
#define WRITE_ERROR   _( L"An error occurred while writing output" )

/**
   file redirection error message
*/
#define FILE_ERROR _( L"An error occurred while redirecting file '%s'" )

/**
   Base open mode to pass to calls to open
*/
#define OPEN_MASK 0666

/**
   List of all pipes used by internal pipes. These must be closed in
   many situations in order to make sure that stray fds aren't lying
   around.
   
   Note this is used after fork, so we must not do anything that may allocate memory. Hopefully methods like open_fds.at() don't.
*/
static std::vector<bool> open_fds;

// Called in a forked child
static void exec_write_and_exit( int fd, const char *buff, size_t count, int status )
{
	if( write_loop(fd, buff, count) == -1 ) 
	{
		debug( 0, WRITE_ERROR);
		wperror( L"write" );
		exit_without_destructors(status);
	}
	exit_without_destructors( status );
}


void exec_close( int fd )
{
    /* This may be called in a child of fork(), so don't allocate memory */
	if( fd < 0 )
	{
		debug( 0, L"Called close on invalid file descriptor " );
		return;
	}
	
	while( close(fd) == -1 )
	{
		if( errno != EINTR )
		{
			debug( 1, FD_ERROR, fd );
			wperror( L"close" );
			break;
		}
	}
	
    /* Maybe remove this from our set of open fds */
    if (fd < (int)open_fds.size()) {
        open_fds[fd] = false;
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
	
    int max_fd = std::max(fd[0], fd[1]);
    if ((int)open_fds.size() <= max_fd) {
        open_fds.resize(max_fd + 1, false);
    }
    open_fds.at(fd[0]) = true;
    open_fds.at(fd[1]) = true;
	
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
void close_unused_internal_pipes( io_data_t *io )
{
    /* A call to exec_close will modify open_fds, so be careful how we walk */
    for (size_t i=0; i < open_fds.size(); i++) {
        if (open_fds[i]) {
            int fd = (int)i;
            if( !use_fd_in_pipe( fd, io) )
            {
                debug( 4, L"Close fd %d, used in other context", fd );
                exec_close( fd );
                i--;
            }
        }
    }
}

/**
   Returns the interpreter for the specified script. Returns NULL if file
   is not a script with a shebang.
 */
static char *get_interpreter( const char *command, char *interpreter, size_t buff_size )
{
    // OK to not use CLO_EXEC here because this is only called after fork
	int fd = open( command, O_RDONLY );
	if( fd >= 0 )
	{
        size_t idx = 0;
		while( idx + 1 < buff_size )
		{
            char ch;
            ssize_t amt = read(fd, &ch, sizeof ch);
			if( amt <= 0 )
				break;
			if( ch == '\n' )
				break;
            interpreter[idx++] = ch;
		}
        interpreter[idx++] = '\0';
        close(fd);
	}
	if (strncmp(interpreter, "#! /", 4) == 0) {
        return interpreter + 3;
    } else if (strncmp(interpreter, "#!/", 3) == 0) {
        return interpreter + 2;
    } else {
        return NULL;
    }
}

/**
   This function is executed by the child process created by a call to
   fork(). It should be called after \c setup_child_process. It calls
   execve to replace the fish process image with the command specified
   in \c p. It never returns.
*/
/* Called in a forked child! Do not allocate memory, etc. */
static void safe_launch_process( process_t *p, const char *actual_cmd, char **argv, char **envv )
{
	int err;
	
//	debug( 1, L"exec '%ls'", p->argv[0] );

	execve ( wcs2str(p->actual_cmd), 
		 argv,
		 envv );
	
	err = errno;
	
	/* 
	   Something went wrong with execve, check for a ":", and run
	   /bin/sh if encountered. This is a weird predecessor to the shebang
	   that is still sometimes used since it is supported on Windows.
	*/
    /* OK to not use CLO_EXEC here because this is called after fork and the file is immediately closed */
	int fd = open(actual_cmd, O_RDONLY);
	if (fd >= 0)
	{
		char begin[1] = {0};
		ssize_t amt_read = read(fd, begin, 1);
		close(fd);
		
		if( (amt_read==1) && (begin[0] == ':') )
		{
            // Relaunch it with /bin/sh. Don't allocate memory, so if you have more args than this, update your silly script! Maybe this should be changed to be based on ARG_MAX somehow.
            char sh_command[] = "/bin/sh";
            char *argv2[128];
            argv2[0] = sh_command;
            for (size_t i=1; i < sizeof argv2 / sizeof *argv2; i++) {
                argv2[i] = argv[i-1];
                if (argv2[i] == NULL)
                    break;
            }
            
			execve(sh_command, argv2, envv);
		}
	}
	
	errno = err;
	debug_safe( 0, "Failed to execute process '%s'. Reason:", actual_cmd );
	
	switch( errno )
	{
		
		case E2BIG:
		{
			char sz1[128], sz2[128];
			
			long arg_max = -1;

			size_t sz = 0;
			char **p;
			for(p=argv; *p; p++)
			{
				sz += strlen(*p)+1;
			}
			
			for(p=envv; *p; p++)
			{
				sz += strlen(*p)+1;
			}
			
            format_size_safe(sz1, sz);
			arg_max = sysconf( _SC_ARG_MAX );
			
			if( arg_max > 0 )
			{
                format_size_safe(sz2, sz);
                debug_safe(0, "The total size of the argument and environment lists %s exceeds the operating system limit of %s.", sz1, sz2);
			}
			else
			{
				debug_safe( 0, "The total size of the argument and environment lists (%s) exceeds the operating system limit.", sz1);
			}
			
			debug_safe(0, "Try running the command again with fewer arguments.");			
			exit_without_destructors(STATUS_EXEC_FAIL);
			break;
		}

		case ENOEXEC:
		{
            /* Hope strerror doesn't allocate... */
            const char *err = strerror(errno);
            debug_safe(0, "exec: %s", err);
			
			debug_safe(0, "The file '%ls' is marked as an executable but could not be run by the operating system.", actual_cmd);
			exit_without_destructors(STATUS_EXEC_FAIL);
		}

		case ENOENT:
		{
            char interpreter_buff[128] = {}, *interpreter;
            interpreter = get_interpreter(actual_cmd, interpreter_buff, sizeof interpreter_buff);
			if( interpreter && 0 != access( interpreter, X_OK ) )
			{
				debug_safe(0, "The file '%s' specified the interpreter '%s', which is not an executable command.", actual_cmd, interpreter );
			}
			else
			{
				debug_safe(0, "The file '%s' or a script or ELF interpreter does not exist, or a shared library needed for file or interpreter cannot be found.", actual_cmd);
			}
			exit_without_destructors(STATUS_EXEC_FAIL);
		}

		case ENOMEM:
		{
			debug_safe(0, "Out of memory");
			exit_without_destructors(STATUS_EXEC_FAIL);
		}

		default:
		{
            /* Hope strerror doesn't allocate... */
            const char *err = strerror(errno);
            debug_safe(0, "exec: %s", err);
			
			//		debug(0, L"The file '%ls' is marked as an executable but could not be run by the operating system.", p->actual_cmd);
			exit_without_destructors(STATUS_EXEC_FAIL);
		}
	}
}

/**
   This function is similar to launch_process, except it is not called after a fork (i.e. it only calls exec) and therefore it can allocate memory.
*/
static void launch_process_nofork( process_t *p )
{
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_NOT_FORKED_CHILD();
    
	char **argv = wcsv2strv(p->get_argv());
	char **envv = env_export_arr( 0 );
    char *actual_cmd = wcs2str(p->actual_cmd);
	
    /* Bounce to launch_process. This never returns. */
    safe_launch_process(p, actual_cmd, argv, envv);
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
    assert(in != NULL);
	io_untransmogrify( in->next, out->next );
	switch( in->io_mode )
	{
		case IO_FILE:
			exec_close( out->param1.old_fd );
			break;
	}	
	delete out;
}


/**
   Make a copy of the specified io redirection chain, but change file
   redirection into fd redirection. This makes the redirection chain
   suitable for use as block-level io, since the file won't be
   repeatedly reopened for every command in the block, which would
   reset the cursor position.

   \return the transmogrified chain on sucess, or 0 on failiure
*/
static io_data_t *io_transmogrify( io_data_t * in )
{
    ASSERT_IS_MAIN_THREAD();
	if( !in )
		return 0;
	
	std::auto_ptr<io_data_t> out(new io_data_t);
	
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
            out.reset(new io_data_t(*in));
			break;
		}

		/*
		  Transmogrify file redirections
		*/
		case IO_FILE:
		{
			int fd;
			
			if( (fd=open( in->filename_cstr, in->param2.flags, OPEN_MASK ) )==-1 )
			{
				debug( 1, 
					   FILE_ERROR,
					   in->filename_cstr );
								
				wperror( L"open" );
				return NULL;
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
			io_untransmogrify( in, out.release() );
			return NULL;
		}
	}
	
	return out.release();
}

/**
   Morph an io redirection chain into redirections suitable for
   passing to eval, call eval, and clean up morphed redirections.

   \param def the code to evaluate
   \param block_type the type of block to push on evaluation
   \param io the io redirections to be performed on this block
*/

static void internal_exec_helper( parser_t &parser,
                                  const wchar_t *def, 
								  enum block_type_t block_type,
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
		proc_set_last_status( STATUS_EXEC_FAIL );
		return;
	}
	
	signal_unblock();
	
	parser.eval( def, io_internal, block_type );		
	
	signal_block();
	
	io_untransmogrify( io, io_internal );
	job_reap( 0 );
	is_block=is_block_old;
}

/** Perform output from builtins. Called from a forked child, so don't do anything that may allocate memory, etc.. */
static void do_builtin_io( const char *out, const char *err )
{
    size_t len;
	if (out && (len = strlen(out)))
	{

		if (write_loop(STDOUT_FILENO, out, len) == -1)
		{
			debug( 0, L"Error while writing to stdout" );
			wperror( L"write_loop" );
			show_stackframe();
		}
	}
	
	if (err && (len = strlen(err)))
	{
		if (write_loop(STDERR_FILENO, err, len) == -1)
		{
			/*
			  Can't really show any error message here, since stderr is
			  dead.
			*/
		}
	}
}


void exec( parser_t &parser, job_t *j )
{
	process_t *p;
	pid_t pid;
	int mypipe[2];
	sigset_t chldset; 
	
	io_data_t pipe_read, pipe_write;
	io_data_t *tmp;

	io_data_t *io_buffer =0;

	/*
	  Set to 1 if something goes wrong while exec:ing the job, in
	  which case the cleanup code will kick in.
	*/
	int exec_error=0;

	bool needs_keepalive = false;
	process_t keepalive;
	

	CHECK( j, );
	CHECK_BLOCK();
	
	if( no_exec )
		return;
	
	sigemptyset( &chldset );
	sigaddset( &chldset, SIGCHLD );
	
	debug( 4, L"Exec job '%ls' with id %d", j->command_wcstr(), j->job_id );	
	
	if( parser.block_io )
	{
		if( j->io )
		{
			j->io = io_add( io_duplicate(parser.block_io), j->io );
		}
		else
		{
			j->io=io_duplicate(parser.block_io);				
		}
	}

	
	io_data_t *input_redirect;

	for( input_redirect = j->io; input_redirect; input_redirect = input_redirect->next )
	{
		if( (input_redirect->io_mode == IO_BUFFER) && 
			input_redirect->is_input )
		{
			/*
			  Input redirection - create a new gobetween process to take
			  care of buffering
			*/
			process_t *fake = new process_t();
            fake->type  = INTERNAL_BUFFER;
			fake->pipe_write_fd = 1;
			j->first_process->pipe_read_fd = input_redirect->fd;
			fake->next = j->first_process;
			j->first_process = fake;
			break;
		}
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
			  launch_process _never_ returns
			*/
			launch_process_nofork( j->first_process );
		}
		else
		{
			job_set_flag( j, JOB_CONSTRUCTED, 1 );
			j->first_process->completed=1;
			return;
		}

	}	

	pipe_read.fd=0;
	pipe_write.fd=1;
	pipe_read.io_mode=IO_PIPE;
	pipe_read.param1.pipe_fd[0] = -1;
	pipe_read.param1.pipe_fd[1] = -1;
	pipe_read.is_input = 1;

	pipe_write.io_mode=IO_PIPE;
	pipe_write.is_input = 0;
	pipe_read.next=0;
	pipe_write.next=0;
	pipe_write.param1.pipe_fd[0]=pipe_write.param1.pipe_fd[1]=-1;
	
	j->io = io_add( j->io, &pipe_write );
	
	signal_block();

	/*
	  See if we need to create a group keepalive process. This is
	  a process that we create to make sure that the process group
	  doesn't die accidentally, and is often needed when a
	  builtin/block/function is inside a pipeline, since that
	  usually means we have to wait for one program to exit before
	  continuing in the pipeline, causing the group leader to
	  exit.
	*/
	
	if( job_get_flag( j, JOB_CONTROL ) )
	{
		for( p=j->first_process; p; p = p->next )
		{
			if( p->type != EXTERNAL )
			{
				if( p->next )
				{
					needs_keepalive = true;
					break;
				}
				if( p != j->first_process )
				{
					needs_keepalive = true;
					break;
				}
				
			}
			
		}
	}
		
	if( needs_keepalive )
	{
        /* Call fork. No need to wait for threads since our use is confined and simple. */
        if (g_log_forks) {
            printf("fork #%d: Executing keepalive fork for '%ls'\n", g_fork_count, j->command_wcstr());
        }
		keepalive.pid = execute_fork(false);
		if( keepalive.pid == 0 )
		{
            /* Child */
			keepalive.pid = getpid();
			set_child_group( j, &keepalive, 1 );
			pause();			
			exit_without_destructors(0);
		}
		else
		{
            /* Parent */
			set_child_group( j, &keepalive, 0 );			
		}
	}
	
	/*
	  This loop loops over every process_t in the job, starting it as
	  appropriate. This turns out to be rather complex, since a
	  process_t can be one of many rather different things.

	  The loop also has to handle pipelining between the jobs.
	*/

	for( p=j->first_process; p; p = p->next )
	{
		const bool p_wants_pipe = (p->next != NULL);
		mypipe[1]=-1;
		
		pipe_write.fd = p->pipe_write_fd;
		pipe_read.fd = p->pipe_read_fd;
//		debug( 0, L"Pipe created from fd %d to fd %d", pipe_write.fd, pipe_read.fd );
		

		/* 
		   This call is used so the global environment variable array
		   is regenerated, if needed, before the fork. That way, we
		   avoid a lot of duplicate work where EVERY child would need
		   to generate it, since that result would not get written
		   back to the parent. This call could be safely removed, but
		   it would result in slightly lower performance - at least on
		   uniprocessor systems.
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
		
		if( p_wants_pipe )
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
				wchar_t * def=0;
				int shadows;
				

				/*
				  Calls to function_get_definition might need to
				  source a file as a part of autoloading, hence there
				  must be no blocks.
				*/

				signal_unblock();
				const wchar_t * orig_def = function_get_definition( p->argv0() );
                
                // function_get_named_arguments may trigger autoload, which deallocates the orig_def.
                // We should make function_get_definition return a wcstring (but how to handle NULL...)
                if (orig_def)
                    def = wcsdup(orig_def);
                
				wcstring_list_t named_arguments = function_get_named_arguments( p->argv0() );
				shadows = function_get_shadows( p->argv0() );

				signal_block();
				
				if( def == NULL )
				{
					debug( 0, _( L"Unknown function '%ls'" ), p->argv0() );
					break;
				}
				parser.push_block( shadows?FUNCTION_CALL:FUNCTION_CALL_NO_SHADOW );
				
				parser.current_block->state2<process_t *>() = p;
				parser.current_block->state1<wcstring>() = p->argv0();
						

				/*
				  set_argv might trigger an event
				  handler, hence we need to unblock
				  signals.
				*/
				signal_unblock();
				parse_util_set_argv( p->get_argv()+1, named_arguments );
				signal_block();
								
				parser.forbid_function( p->argv0() );

				if( p->next )
				{
					io_buffer = io_buffer_create( 0 );					
					j->io = io_add( j->io, io_buffer );
				}
				
				internal_exec_helper( parser, def, TOP, j->io );
				
				parser.allow_function();
				parser.pop_block();
                free(def);
				
				break;				
			}
			
			case INTERNAL_BLOCK:
			{
				if( p->next )
				{
					io_buffer = io_buffer_create( 0 );					
					j->io = io_add( j->io, io_buffer );
				}
                
				internal_exec_helper( parser, p->argv0(), TOP, j->io );			
				break;
				
			}

			case INTERNAL_BUILTIN:
			{
				int builtin_stdin=0;
				int fg;
				int close_stdin=0;

				/*
				  If this is the first process, check the io
				  redirections and see where we should be reading
				  from.
				*/
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
                                /* Do not set CLO_EXEC because child needs access */
								builtin_stdin=open( in->filename_cstr,
                                              in->param2.flags, OPEN_MASK );
								if( builtin_stdin == -1 )
								{
									debug( 1, 
										   FILE_ERROR,
										   in->filename_cstr );
									wperror( L"open" );
								}
								else
								{
									close_stdin = 1;
								}
								
								break;
							}
	
							case IO_CLOSE:
							{
								/*
								  FIXME:

								  When
								  requesting
								  that
								  stdin
								  be
								  closed,
								  we
								  really
								  don't
								  do
								  anything. How
								  should
								  this
								  be
								  handled?
								 */
								builtin_stdin = -1;
								
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
					int old_out = builtin_out_redirect;
					int old_err = builtin_err_redirect;

					/* 
					   Since this may be the foreground job, and since
					   a builtin may execute another foreground job,
					   we need to pretend to suspend this job while
					   running the builtin, in order to avoid a
					   situation where two jobs are running at once.

					   The reason this is done here, and not by the
					   relevant builtins, is that this way, the
					   builtin does not need to know what job it is
					   part of. It could probably figure that out by
					   walking the job list, but it seems more robust
					   to make exec handle things.
					*/
					
					builtin_push_io( parser, builtin_stdin );
					
					builtin_out_redirect = has_fd( j->io, 1 );
					builtin_err_redirect = has_fd( j->io, 2 );		

					fg = job_get_flag( j, JOB_FOREGROUND );
					job_set_flag( j, JOB_FOREGROUND, 0 );
					
					signal_unblock();
					
					p->status = builtin_run( parser, p->get_argv(), j->io );
					
					builtin_out_redirect=old_out;
					builtin_err_redirect=old_err;
					
					signal_block();
					
					/*
					  Restore the fg flag, which is temporarily set to
					  false during builtin execution so as not to confuse
					  some job-handling builtins.
					*/
					job_set_flag( j, JOB_FOREGROUND, fg );
				}
				
				/*
				  If stdin has been redirected, close the redirection
				  stream.
				*/
				if( close_stdin )
				{
					exec_close( builtin_stdin );
				}				
				break;				
			}
		}
		
		if( exec_error )
		{
			break;
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
						proc_set_last_status( job_get_flag( j, JOB_NEGATE )?(!status):status);
					}
					p->completed = 1;
					break;
				}

				j->io = io_remove( j->io, io_buffer );
				
				io_buffer_read( io_buffer );
				
                const char *buffer = io_buffer->out_buffer_ptr();
                size_t count = io_buffer->out_buffer_size();
                
				if( io_buffer->out_buffer_size() > 0 )
				{
                    /* We don't have to drain threads here because our child process is simple */
                    if (g_log_forks) {
                        printf("Executing fork for internal block or function for '%ls'\n", p->argv0());
                    }
					pid = execute_fork(false);
					if( pid == 0 )
					{
						
						/*
						  This is the child process. Write out the contents of the pipeline.
						*/
						p->pid = getpid();
						setup_child_process( j, p );

						exec_write_and_exit(io_buffer->fd, buffer, count, status);
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
						proc_set_last_status( job_get_flag( j, JOB_NEGATE )?(!status):status);
					}
					p->completed = 1;
				}
				
				io_buffer_destroy( io_buffer );
				
				io_buffer=0;
				break;
				
			}


			case INTERNAL_BUFFER:
			{
		
				const char *buffer = input_redirect->out_buffer_ptr();
				size_t count = input_redirect->out_buffer_size();
        
                /* We don't have to drain threads here because our child process is simple */
                if (g_log_forks) {
                    printf("fork #%d: Executing fork for internal buffer for '%ls'\n", g_fork_count, p->argv0() ? p->argv0() : L"(null)");
                }
				pid = execute_fork(false);
				if( pid == 0 )
				{
					/*
					  This is the child process. Write out the
					  contents of the pipeline.
					*/
					p->pid = getpid();
					setup_child_process( j, p );
					
					exec_write_and_exit( 1, buffer, count, 0);
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
				int skip_fork;
				
				/*
				  Handle output from builtin commands. In the general
				  case, this means forking of a worker process, that
				  will write out the contents of the stdout and stderr
				  buffers to the correct file descriptor. Since
				  forking is expensive, fish tries to avoid it wehn
				  possible.
				*/

				/*
				  If a builtin didn't produce any output, and it is
				  not inside a pipeline, there is no need to fork
				*/
				skip_fork =
                    get_stdout_buffer().empty() &&
                    get_stderr_buffer().empty() &&
					!p->next;
	
				/*
				  If the output of a builtin is to be sent to an internal
				  buffer, there is no need to fork. This helps out the
				  performance quite a bit in complex completion code.
				*/

				io_data_t *io = io_get( j->io, 1 );
				bool buffer_stdout = io && io->io_mode == IO_BUFFER;
				
				if( ( get_stderr_buffer().empty() ) && 
					( !p->next ) &&
					( ! get_stdout_buffer().empty() ) && 
					( buffer_stdout ) )
				{
					std::string res = wcs2string( get_stdout_buffer() );
					io->out_buffer_append( res.c_str(), res.size() );
					skip_fork = 1;
				}
                
                if (! skip_fork && ! j->io) {
                    /* PCA for some reason, fish forks a lot, even for basic builtins like echo just to write out their buffers. I'm certain a lot of this is unnecessary, but I am not sure exactly when. If j->io is NULL, then it means there's no pipes or anything, so we can certainly just write out our data. Beyond that, we may be able to do the same if io_get returns 0 for STDOUT_FILENO and STDERR_FILENO. */
                    if (g_log_forks) {
                        printf("fork #-: Skipping fork for internal builtin for '%ls' (io is %p, job_io is %p)\n", p->argv0(), io, j->io);
                    }
                    const wcstring &out = get_stdout_buffer(), &err = get_stderr_buffer();
                    char *outbuff = wcs2str(out.c_str()), *errbuff = wcs2str(err.c_str());
                    do_builtin_io(outbuff, errbuff);
                    free(outbuff);
                    free(errbuff);
                    skip_fork = 1;
                }

				for( io_data_t *tmp_io = j->io; tmp_io != NULL; tmp_io=tmp_io->next )
				{
					if( tmp_io->io_mode == IO_FILE && strcmp(tmp_io->filename_cstr, "/dev/null") != 0)
					{
						skip_fork = 0;
                        break;
					}
				}
                
				
				if( skip_fork )
				{
					p->completed=1;
					if( p->next == 0 )
					{
						debug( 3, L"Set status of %ls to %d using short circut", j->command_wcstr(), p->status );
						
						int status = p->status;
						proc_set_last_status( job_get_flag( j, JOB_NEGATE )?(!status):status );
					}
					break;
				}


				/* Ok, unfortunatly, we have to do a real fork. Bummer. We work hard to make sure we don't have to wait for all our threads to exit, by arranging things so that we don't have to allocate memory or do anything except system calls in the child. */
                
                /* Get the strings we'll write before we fork (since they call malloc) */
                const wcstring &out = get_stdout_buffer(), &err = get_stderr_buffer();
                char *outbuff = wcs2str(out.c_str()), *errbuff = wcs2str(err.c_str());
                
                fflush(stdout);
                fflush(stderr);
                if (g_log_forks) {
                    printf("fork #%d: Executing fork for internal builtin for '%ls' (io is %p, job_io is %p)\n", g_fork_count, p->argv0(), io, j->io);
                    io_print(io);
                }
				pid = execute_fork(false);
				if( pid == 0 )
				{
					/*
					  This is the child process. Setup redirections,
					  print correct output to stdout and stderr, and
					  then exit.
					*/
					p->pid = getpid();
					setup_child_process( j, p );
					do_builtin_io(outbuff, errbuff);
					exit_without_destructors( p->status );
						
				}
				else
				{
                    /* Free the strings in the parent */
                    free(outbuff);
                    free(errbuff);
                    
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
                /* Get argv and envv before we fork */
                null_terminated_array_t<char> argv_array = convert_wide_array_to_narrow(p->get_argv_array());
                
                null_terminated_array_t<char> envv_array;
                env_export_arr(false, envv_array);
                
                char **envv = envv_array.get();
                char **argv = argv_array.get();
                
                std::string actual_cmd_str = wcs2string(p->actual_cmd);
                const char *actual_cmd = actual_cmd_str.c_str();
                
                const wchar_t *reader_current_filename();
                if (g_log_forks) {
                    const wchar_t *file = reader_current_filename();
                    const wchar_t *func = parser_t::principal_parser().is_function();
                    printf("fork #%d: forking for '%s' in '%ls:%ls'\n", g_fork_count, actual_cmd, file ? file : L"", func ? func : L"?");
                }
				pid = execute_fork(false);
				if( pid == 0 )
				{
					/*
					  This is the child process. 
					*/
					p->pid = getpid();
					setup_child_process( j, p );
					safe_launch_process( p, actual_cmd, argv, envv );
					
					/*
					  safe_launch_process _never_ returns...
					*/
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
			builtin_pop_io(parser);
				
		/* 
		   Close the pipe the current process uses to read from the
		   previous process_t
		*/
		if( pipe_read.param1.pipe_fd[0] >= 0 )
			exec_close( pipe_read.param1.pipe_fd[0] );
		/* 
		   Set up the pipe the next process uses to read from the
		   current process_t
		*/
		if( p_wants_pipe )
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

	/*
	  The keepalive process is no longer needed, so we terminate it
	  with extreme prejudice
	*/
	if( needs_keepalive )
	{
		kill( keepalive.pid, SIGKILL );
	}
	
	signal_unblock();	

	debug( 3, L"Job is constructed" );

	j->io = io_remove( j->io, &pipe_read );

	for( tmp = parser.block_io; tmp; tmp=tmp->next )
		j->io = io_remove( j->io, tmp );
	
	job_set_flag( j, JOB_CONSTRUCTED, 1 );

	if( !job_get_flag( j, JOB_FOREGROUND ) )
	{
		proc_last_bg_pid = j->pgid;
	}

	if( !exec_error )
	{
		job_continue (j, 0);
	}
	
}


static int exec_subshell_internal( const wcstring &cmd, wcstring_list_t *lst )
{
    ASSERT_IS_MAIN_THREAD();
	char *begin, *end;
	char z=0;
	int prev_subshell = is_subshell;
	int status, prev_status;
	io_data_t *io_buffer;
	char sep=0;
	
	const env_var_t ifs = env_get_string(L"IFS");

	if( ! ifs.missing_or_empty() )
	{
		if( ifs.at(0) < 128 )
		{
			sep = '\n';//ifs[0];
		}
		else
		{
			sep = 0;
			debug( 0, L"Warning - invalid command substitution separator '%lc'. Please change the firsta character of IFS", ifs[0] );
		}
		
	}
	
	is_subshell=1;	
	io_buffer= io_buffer_create( 0 );
	
	prev_status = proc_get_last_status();
	
    parser_t &parser = parser_t::principal_parser();
	if( parser.eval( cmd, io_buffer, SUBST ) )
	{
		status = -1;
	}
	else
	{
		status = proc_get_last_status();
	}
	
	io_buffer_read( io_buffer );
		
	proc_set_last_status( prev_status );
	
	is_subshell = prev_subshell;
	
	io_buffer->out_buffer_append( &z, 1 );
	
	begin=end=io_buffer->out_buffer_ptr();	

	if( lst )
	{
		while( 1 )
		{
			if( *end == 0 )
			{
				if( begin != end )
				{
					wchar_t *el = str2wcs( begin );
					if( el )
					{
						lst->push_back(el);

						free(el);
					}
					else
					{
						debug( 2, L"Got null string on line %d of file %s", __LINE__, __FILE__ );
					}
				}				
				io_buffer_destroy( io_buffer );
				
				return status;
			}
			else if( *end == sep )
			{
				wchar_t *el;
				*end=0;
				el = str2wcs( begin );
				if( el )
				{
                    lst->push_back(el);

					free(el);
				}
				else
				{
					debug( 2, L"Got null string on line %d of file %s", __LINE__, __FILE__ );
				}
				begin = end+1;
			}
			end++;
		}
	}
	
	io_buffer_destroy( io_buffer );

	return status;	
}

int exec_subshell( const wcstring &cmd, std::vector<wcstring> &outputs )
{
    ASSERT_IS_MAIN_THREAD();
    return exec_subshell_internal(cmd, &outputs);
}

__warn_unused int exec_subshell( const wcstring &cmd )
{
    ASSERT_IS_MAIN_THREAD();
    return exec_subshell_internal(cmd, NULL);
}
