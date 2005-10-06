/** \file signal.c

The library for various signal related issues

*/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>

#include "common.h"
#include "signal.h"
#include "event.h"
#include "reader.h"
#include "proc.h"

/**
   Struct describing an entry for the lookup table used to convert
   between signal names and signal ids, etc.
*/
struct lookup_entry
{
	/**
	   Signal id
	*/
	int signal;
	/**
	   Signal name
	*/
	const wchar_t *name;
	/**
	   Signal description
	*/
	const wchar_t *desc;	
};

/**
   Lookup table used to convert between signal names and signal ids,
   etc.
*/
static struct lookup_entry lookup[] =
{
	{
		SIGHUP, 
		L"SIGHUP",
		L"Terminal hung up"
	}
	,
	{
		SIGINT,
		L"SIGINT",
		L"Quit request from job control (^C)"
	}
	,
	{
		SIGQUIT,
		L"SIGQUIT",
		L"Quit request from job control with core dump (^\\)"
	}
	,
	{
		SIGILL,
		L"SIGILL",
		L"Illegal instruction"
	}
	,
	{
		SIGTRAP, 
		L"SIGTRAP",
		L"Trace or breakpoint trap"
	}
	,
	{
		SIGABRT,
		L"SIGABRT",
		L"Abort"
	}
	,
	{
		SIGBUS, 
		L"SIGBUS",
		L"Misaligned address error"
	}
	,
	{
		SIGFPE, 
		L"SIGFPE",
		L"Floating point exception"
	}
	,
	{
		SIGKILL, 
		L"SIGKILL",
		L"Forced quit"
	}
	,
	{
		SIGUSR1,
		L"SIGUSR1",
		L"User defined signal 1"
	}
	,
	{
		SIGUSR2, L"SIGUSR2",
		L"User defined signal 2"
	}
	,
	{
		SIGSEGV, 
		L"SIGSEGV",
		L"Address boundary error"
	}
	,
	{
		SIGPIPE,
		L"SIGPIPE",
		L"Broken pipe"
	}
	,
	{
		SIGALRM, 
		L"SIGALRM",
		L"Timer expired"
	}
	,
	{
		SIGTERM,
		L"SIGTERM",
		L"Polite quit request"
	}
	,
	{
		SIGCHLD,
		L"SIGCHLD",
		L"Child process status changed"
	}
	,
	{
		SIGCONT,
		L"SIGCONT",
		L"Continue previously stopped process"
	}
	,
	{
		SIGSTOP,
		L"SIGSTOP",
		L"Forced stop"
	}
	,
	{
		SIGTSTP,
		L"SIGTSTP",
		L"Stop request from job control (^Z)"
	}
	,
	{
		SIGTTIN, 
		L"SIGTTIN",
		L"Stop from terminal input"
	}
	,
	{
		SIGTTOU,
		L"SIGTTOU",
		L"Stop from terminal output"
	}
	,
	{
		SIGURG, 
		L"SIGURG",
		L"Urgent socket condition"
	}
	,
	{
		SIGXCPU,
		L"SIGXCPU",
		L"CPU time limit exceeded"
	}
	,
	{
		SIGXFSZ, 
		L"SIGXFSZ",
		L"File size limit exceeded"
	}
	,
	{
		SIGVTALRM,
		L"SIGVTALRM",
		L"Virtual timer expired"
	}
	,
	{
		SIGPROF,
		L"SIGPROF",
		L"Profiling timer expired"
	}
	,
	{
		SIGWINCH,
		L"SIGWINCH",
		L"Window size change"
	}
	,
	{
		SIGIO,
		L"SIGIO",
		L"Asynchronous IO event"
	}
	,
#ifdef SIGPWR
	{
		SIGPWR, 
		L"SIGPWR",
		L"Power failure"
	}
	,
#endif
	{
		SIGSYS, 
		L"SIGSYS",
		L"Bad system call"
	}
	,
	{
		0,
		0,
		0
	}
}
	;


/**
   Test if \c name is a string describing the signal named \c canonical. 
*/
static int match_signal_name( const wchar_t *canonical, 
							  const wchar_t *name )
{
	if( wcsncasecmp( name, L"sig", 3 )==0)
		name +=3;

	return wcscasecmp( canonical+3,name ) == 0;	
}


int wcs2sig( const wchar_t *str )
{
	int i;
	for( i=0; lookup[i].desc ; i++ )
	{
		if( match_signal_name( lookup[i].name, str) )
		{
			return lookup[i].signal;
		}
	}
	return -1;	
}


const wchar_t *sig2wcs( int sig )
{
	int i;
	for( i=0; lookup[i].desc ; i++ )
	{
		if( lookup[i].signal == sig )
		{
			return lookup[i].name;
		}
	}
	return L"Unknown";
}

const wchar_t *sig_description( int sig )
{
	int i;
	for( i=0; lookup[i].desc ; i++ )
	{
		if( lookup[i].signal == sig )
		{
			return lookup[i].desc;
		}
	}
	return L"Unknown";
}

static void default_handler(int signal, siginfo_t *info, void *context)
{
	event_t e;
	e.type=EVENT_SIGNAL;
	e.signal = signal;
	e.function_name=0;

	event_fire( &e, 0 );
}

/**
   Respond to a winch signal by checking the terminal size
*/
static void handle_winch( int sig, siginfo_t *info, void *context )
{
	reader_handle_winch( sig );	
	default_handler( sig, 0, 0 );	
}

/**
   Interactive mode ^C handler. Respond to int signal by setting
   interrupted-flag and stopping all loops and conditionals.
*/
static void handle_int( int sig, siginfo_t *info, void *context )
{
	reader_handle_int( sig );

	default_handler( sig, info, context);	
}

/**
   Sets appropriate signal handlers.
*/
void signal_set_handlers()
{
	struct sigaction act;
	sigemptyset( & act.sa_mask );
	act.sa_flags=SA_SIGINFO;
	act.sa_sigaction = &default_handler;

	/*
	  First reset everything to a use default_handler, a function
	  whose sole action is to fire of an event
	*/
	sigaction( SIGINT, &act, 0);
	sigaction( SIGQUIT, &act, 0);
	sigaction( SIGTSTP, &act, 0);
	sigaction( SIGTTIN, &act, 0);
	sigaction( SIGTTOU, &act, 0);
	sigaction( SIGCHLD, &act, 0);
	sigaction( SIGALRM, &act, 0);

	/*
	  Ignore sigpipe, it is generated if fishd dies, but we can
	  recover.
	*/
	sigaction( SIGPIPE, &act, 0);
	
	if( is_interactive )
	{

		/*
		   Interactive mode. Ignore interactive signals.  We are a
		   shell, we know whats best for the user. ;-)
		*/

		act.sa_handler=SIG_IGN;

		sigaction( SIGINT, &act, 0);
		sigaction( SIGQUIT, &act, 0);
		sigaction( SIGTSTP, &act, 0);
		sigaction( SIGTTIN, &act, 0);
		sigaction( SIGTTOU, &act, 0);

		act.sa_handler = &handle_int;
		act.sa_flags = SA_SIGINFO;
		if( sigaction( SIGINT, &act, 0) )
		{
			wperror( L"sigaction" );
			exit(1);
		}

		act.sa_sigaction = &job_handle_signal;
		act.sa_flags = SA_SIGINFO;
		if( sigaction( SIGCHLD, &act, 0) )
		{
			wperror( L"sigaction" );
			exit(1);
		}

		act.sa_flags = SA_SIGINFO;
		act.sa_handler= &handle_winch;
		if( sigaction( SIGWINCH, &act, 0 ) )
		{
			wperror( L"sigaction" );
			exit(1);
		}

	}
	else
	{
		/*
		  Non-interactive. Ignore interrupt, check exit status of
		  processes to determine result instead.
		*/
		act.sa_handler=SIG_IGN;

		sigaction( SIGINT, &act, 0);
		sigaction( SIGQUIT, &act, 0);

		act.sa_handler=SIG_DFL;

		act.sa_sigaction = &job_handle_signal;
		act.sa_flags = SA_SIGINFO;
		if( sigaction( SIGCHLD, &act, 0) )
		{
			wperror( L"sigaction" );
			exit(1);
		}
	}
}

