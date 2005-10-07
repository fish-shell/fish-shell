/** \file fishd.c

The universal variable server. fishd is automatically started by fish
if a fishd server isn't already running. fishd reads any saved
variables from ~/.fishd, and takes care of commonication between fish
instances. When no clients are running, fishd will automatically shut
down and save.

*/
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pwd.h>
#include <fcntl.h>

#include <errno.h>
#include <locale.h>
#include <signal.h>

#include "util.h"
#include "common.h"
#include "wutil.h"
#include "env_universal_common.h"

/**
   Maximum length of socket filename
*/
#ifndef UNIX_PATH_MAX 
#define UNIX_PATH_MAX 100
#endif

/**
   Small greeting to show that fishd is running
*/
#define GREETING "#Fish universal variable daemon\n"

/**
   The name of the save file. The hostname is appended to this.
*/
#define FILE ".fishd."

/**
   Maximum length of hostname. Longer hostnames are truncated
*/
#define HOSTNAME_LEN 32

/**
   The string to append to the socket name to name the lockfile
*/
#define LOCKPOSTFIX ".lock"

/**
   The timeout in seconds on the lockfile for critical section
*/
#define LOCKTIMEOUT 1

/**
   The list of connections to clients
*/
static connection_t *conn;

/**
   The socket to accept new clients on
*/
static int sock;

/**
   Constructs the fish socket filename
*/
static char *get_socket_filename()
{
	char *name;
	char *dir = getenv( "FISHD_SOCKET_DIR" );
	char *uname = getenv( "USER" );

	if( dir == NULL )
	{
		dir = "/tmp";
	}

	if( uname == NULL )
	{
		struct passwd *pw;
		pw = getpwuid( getuid() );
		uname = strdup( pw->pw_name );
	}

	name = malloc( strlen(dir)+ strlen(uname)+ strlen(SOCK_FILENAME) + 2 );
	if( name == NULL )
	{
		wperror( L"get_socket_filename" );
		exit( EXIT_FAILURE );
	}
	strcpy( name, dir );
	strcat( name, "/" );
	strcat( name, SOCK_FILENAME );
	strcat( name, uname );

	if( strlen( name ) >= UNIX_PATH_MAX )
	{
		debug( 1, L"Filename too long: '%s'", name );
		exit( EXIT_FAILURE );
	}
	return name;
}

/**
   Acquire the lock for the socket
   Returns the name of the lock file if successful or 
   NULL if unable to obtain lock.
   The returned string must be free()d after unlink()ing the file to release 
   the lock
*/
static char *acquire_socket_lock( const char *sock_name )
{
	int len = strlen( sock_name );
	char *lockfile = malloc( len + strlen( LOCKPOSTFIX ) + 1 );
	
	if( lockfile == NULL ) 
	{
		wperror( L"acquire_socket_lock" );
		exit( EXIT_FAILURE );
	}
	strcpy( lockfile, sock_name );
	strcpy( lockfile + len, LOCKPOSTFIX );
	if ( !acquire_lock_file( lockfile, LOCKTIMEOUT, 1 ) )
	{
		free( lockfile );
		lockfile = NULL;
	}
	return lockfile;
}

/**
   Connects to the fish socket and starts listening for connections
*/
static int get_socket()
{
	int s, len, doexit = 0;
	int exitcode = EXIT_FAILURE;
	struct sockaddr_un local;
	char *sock_name = get_socket_filename();

	/*
	   Start critical section protected by lock
	*/
	char *lockfile = acquire_socket_lock( sock_name );
	if( lockfile == NULL )
	{
		debug( 0, L"Unable to obtain lock on socket, exiting" );
		exit( EXIT_FAILURE );
	}
	debug( 1, L"Acquired lockfile: %s", lockfile );
	
	local.sun_family = AF_UNIX;
	strcpy( local.sun_path, sock_name );
	len = strlen( local.sun_path ) + sizeof( local.sun_family );

	debug(1, L"Connect to socket at %s", sock_name);
	
	if( ( s = socket( AF_UNIX, SOCK_STREAM, 0 ) ) == -1 )
	{
		wperror( L"socket" );
		doexit = 1;
		goto unlock;
	}

	/*
	   First check whether the socket has been opened by another fishd;
	   if so, exit with success status
	*/
	if( connect( s, (struct sockaddr *)&local, len ) == 0 )
	{
		debug( 1, L"Socket already exists, exiting" );
		doexit = 1;
		exitcode = 0;
		goto unlock;
	}
	
	unlink( local.sun_path );
	if( bind( s, (struct sockaddr *)&local, len ) == -1 )
	{
		wperror( L"bind" );
		doexit = 1;
		goto unlock;
	}

	if( fcntl( s, F_SETFL, O_NONBLOCK ) != 0 )
	{
		wperror( L"fcntl" );
		close( s );
		doexit = 1;
	} else if( listen( s, 64 ) == -1 )
	{
		wperror( L"listen" );
		doexit = 1;
	}

unlock:
	(void)unlink( lockfile );
	debug( 1, L"Released lockfile: %s", lockfile );
	/*
	   End critical section protected by lock
	*/
	
	free( lockfile );
	
	free( sock_name );

	if( doexit )
	{
		exit( exitcode );
	}

	return s;
}

/**
   Event handler. Broadcasts updates to all clients.
*/
static void broadcast( int type, const wchar_t *key, const wchar_t *val )
{
	connection_t *c;
	message_t *msg;

	if( !conn )
		return;
	
	msg = create_message( type, key, val );
	
	/*
	  Don't merge these loops, or try_send_all can free the message
	  prematurely
	*/
	
	for( c = conn; c; c=c->next )
	{
		msg->count++;
		q_put( &c->unsent, msg );
	}	
	
	for( c = conn; c; c=c->next )
	{
		try_send_all( c );
	}	
}

/**
   Make program into a creature of the night.
*/
static void daemonize()
{
	/*
	  Fork, and let parent exit
	*/
	switch( fork() )
	{
		case -1:
			debug( 0, L"Could not put fishd in background. Quitting" );
			wperror( L"fork" );
			exit(1);
			
		case 0:
		{
			struct sigaction act;
			sigemptyset( & act.sa_mask );
			act.sa_flags=0;
			act.sa_handler=SIG_IGN;
			sigaction( SIGHUP, &act, 0);
			break;
		}
		
		default:
		{			
			debug( 0, L"Parent calling exit" );
			exit(0);
		}		
	}

	/*
	  Put ourself in out own processing group
	*/
	setsid();

	/*
	  Close stdin and stdout. We only use stderr, anyway.
	*/
	close( 0 );
	close( 1 );

}

/**
   Load or save all variables
*/
void load_or_save( int save)
{
	struct passwd *pw;
	char *name;
	char *dir = getenv( "HOME" );
	char hostname[HOSTNAME_LEN];
	connection_t c;
	
	if( !dir )
	{
		pw = getpwuid( getuid() );
		dir = pw->pw_dir;
	}
	
	gethostname( hostname, HOSTNAME_LEN );
	
	name = malloc( strlen(dir)+ strlen(FILE)+ strlen(hostname) + 2 );
	strcpy( name, dir );
	strcat( name, "/" );
	strcat( name, FILE );
	strcat( name, hostname );
	
	debug( 1, L"Open file for %s: '%s'", 
		   save?"saving":"loading", 
		   name );
	
	c.fd = open( name, save?(O_CREAT | O_TRUNC | O_WRONLY):O_RDONLY, 0600);
	free( name );
	
	if( c.fd == -1 )
	{
		debug( 1, L"Could not open load/save file. No previous saves?" );
		wperror( L"open" );
		return;		
	}
	debug( 1, L"File open on fd %d", c.fd );

	sb_init( &c.input );
	memset (&c.wstate, '\0', sizeof (mbstate_t));
	q_init( &c.unsent );

	if( save )
		enqueue_all( &c );
	else
		read_message( &c );

	q_destroy( &c.unsent );
	sb_destroy( &c.input );
	close( c.fd );
}

static void load()
{
	load_or_save(0);
}


static void save()
{
	load_or_save(1);
}

/**
   Do all sorts of boring initialization.
*/
static void init()
{
	program_name=L"fishd";

	sock = get_socket();
	daemonize();	
	fish_setlocale( LC_ALL, L"" );	
	env_universal_common_init( &broadcast );
	
	load();	
}


int main( int argc, char ** argv )
{
	int child_socket, t;
	struct sockaddr_un remote;
	int max_fd;
	int update_count=0;
	
	fd_set read_fd, write_fd;
	
	init();
	
	while(1) 
	{
		connection_t *c;
		int res;

		t=sizeof( remote );		
		
		FD_ZERO( &read_fd );
		FD_ZERO( &write_fd );
		FD_SET( sock, &read_fd );
		max_fd = sock+1;
		for( c=conn; c; c=c->next )
		{
			FD_SET( c->fd, &read_fd );
			max_fd = maxi( max_fd, c->fd+1);
			
			if( ! q_empty( &c->unsent ) )
			{
				FD_SET( c->fd, &write_fd );
			}
		}
		
		res=select( max_fd, &read_fd, &write_fd, 0, 0 );
		
		if( res==-1 )
		{
			wperror( L"select" );
			exit(1);
		}
		
		if( FD_ISSET( sock, &read_fd ) )
		{
			if( (child_socket = 
				 accept( sock, 
						 (struct sockaddr *)&remote, 
						 &t) ) == -1) {
				wperror( L"accept" );
				exit(1);
			}
			else
			{
				debug( 1, L"Connected with new child on fd %d", child_socket );

				if( fcntl( child_socket, F_SETFL, O_NONBLOCK ) != 0 )
				{
					wperror( L"fcntl" );
					close( child_socket );		
				}
				else
				{
					connection_t *new = malloc( sizeof(connection_t));
					new->fd = child_socket;
					new->next = conn;
					q_init( &new->unsent );
					new->killme=0;				
					sb_init( &new->input );
					memset (&new->wstate, '\0', sizeof (mbstate_t));					
					send( new->fd, GREETING, strlen(GREETING), MSG_DONTWAIT );
					enqueue_all( new );				
					conn=new;
				}
			}
		}
		
		for( c=conn; c; c=c->next )
		{
			if( FD_ISSET( c->fd, &write_fd ) )
			{
				try_send_all( c );
			}
		}
		
		for( c=conn; c; c=c->next )
		{
			if( FD_ISSET( c->fd, &read_fd ) )
			{
				read_message( c );

				/*
				  Occasionally we save during normal use, so that we
				  won't lose everything on a system crash
				*/
				update_count++;
				if( update_count >= 64 )
				{
					save();
					update_count = 0;
				}
			}
		}
		
		connection_t *prev=0;
		c=conn;
		
		while( c )
		{
			if( c->killme )
			{
				debug( 1, L"Close connection %d", c->fd );

				close(c->fd );
				sb_destroy( &c->input );

				while( !q_empty( &c->unsent ) )
				{
					message_t *msg = (message_t *)q_get( &c->unsent );
					msg->count--;
					if( !msg->count )
						free( msg );
				}
				
				q_destroy( &c->unsent );				
				if( prev )
				{
					prev->next=c->next;
				}
				else
				{
					conn=c->next;
				}
				
				free(c);
				
				c=(prev?prev->next:conn);
				
			}
			else
			{
				prev=c;
				c=c->next;
			}
		}
		if( !conn )
		{
			debug( 0, L"No more clients. Quitting" );
			save();			
			env_universal_common_destroy();
			exit(0);
			c=c->next;
		}		
	}
}
