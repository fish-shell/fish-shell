#include "config.h"


#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>

#if HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif

#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <signal.h>

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "wutil.h"
#include "env_universal_common.h"
#include "env_universal.h"

/**
   Maximum number of times to try to get a new fishd socket
*/

#define RECONNECT_COUNT 32


connection_t env_universal_server;

/**
   Set to 1 after initialization has been performed
*/
static int init = 0;

/**
   The number of attempts to start fishd
*/
static int get_socket_count = 0;

static wchar_t * path;
static wchar_t *user;
static void (*start_fishd)();
static void (*external_callback)( int type, const wchar_t *name, const wchar_t *val );

/**
   Flag set to 1 when a barrier reply is recieved
*/
static int barrier_reply = 0;

void env_universal_barrier();

static int is_dead()
{
	return env_universal_server.fd < 0;
}


/**
   Get a socket for reading from the server
*/
static int get_socket( int fork_ok )
{
	int s, len;
	struct sockaddr_un local;
	
	char *name;
	wchar_t *wdir;
	wchar_t *wuname;	
	char *dir =0, *uname=0;

	get_socket_count++;
	wdir = path;
	wuname = user;
	
	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) 
	{
		wperror(L"socket");
		return -1;
	}
	
	if( wdir )
		dir = wcs2str(wdir );
	else
		dir = strdup("/tmp");
	
	if( wuname )
		uname = wcs2str(wuname );
	else
	{
		struct passwd *pw;
		pw = getpwuid( getuid() );
		uname = strdup( pw->pw_name );
	}
	
	name = malloc( strlen(dir) +
				   strlen(uname) + 
				   strlen(SOCK_FILENAME) + 
				   2 );
	
	strcpy( name, dir );
	strcat( name, "/" );
	strcat( name, SOCK_FILENAME );
	strcat( name, uname );
	
	free( dir );
	free( uname );
	
	debug( 3, L"Connect to socket %s at fd %2", name, s );
	
	local.sun_family = AF_UNIX;
	strcpy(local.sun_path, name );
	free( name );
	len = sizeof(local);
	
	if( connect( s, (struct sockaddr *)&local, len) == -1 ) 
	{
		close( s );
		if( fork_ok && start_fishd )
		{
			debug( 2, L"Could not connect to socket %d, starting fishd", s );
			
			start_fishd();
									
			return get_socket( 0 );
		}
		
		debug( 1, L"Could not connect to universal variable server, already tried manual restart (or no command supplied). You will not be able to share variable values between fish sessions. Is fish properly installed?" );
		return -1;
	}
	
	if( (fcntl( s, F_SETFL, O_NONBLOCK ) != 0) || (fcntl( s, F_SETFD, FD_CLOEXEC ) != 0) ) 
	{
		wperror( L"fcntl" );
		close( s );		
		
		return -1;
	}

	debug( 3, L"Connected to fd %d", s );
	
	return s;
}

/**
   Callback function used whenever a new fishd message is recieved
*/
static void callback( int type, const wchar_t *name, const wchar_t *val )
{	
	if( type == BARRIER_REPLY )
	{
		barrier_reply = 1;
	}
	else
	{
		if( external_callback )
			external_callback( type, name, val );		
	}	
}

/**
   Make sure the connection is healthy. If not, close it, and try to
   establish a new connection.
*/
static void check_connection()
{
	if( !init )
		return;
	
	if( env_universal_server.killme )
	{
		debug( 3, L"Lost connection to universal variable server." );
		
		if( close( env_universal_server.fd ) )
		{
			wperror( L"close" );
		}
		
		env_universal_server.fd = -1;
		env_universal_server.killme=0;
		env_universal_server.input.used=0;	
		env_universal_read_all();
	}	
}

/**
   Remove all universal variables.
*/
static void env_universal_remove_all()
{
	array_list_t lst;
	int i;
	
	al_init( &lst );
	
	env_universal_common_get_names( &lst, 
									1,
									1 );

	for( i=0; i<al_get_count( &lst ); i++ )
	{
		wchar_t *key = (wchar_t *)al_get( &lst, i );
		env_universal_common_remove( key );
	}

	al_destroy( &lst );
	
}


/**
   Try to establish a new connection to fishd. If successfull, end
   with call to env_universal_barrier(), to make sure everything is in
   sync.
*/
static void reconnect()
{
	if( get_socket_count >= RECONNECT_COUNT )
		return;
	
	debug( 3, L"Get new fishd connection" );
	
	init = 0;
	env_universal_server.buffer_consumed = env_universal_server.buffer_used = 0;
	env_universal_server.fd = get_socket(1);
	init = 1;
	if( env_universal_server.fd >= 0 )
	{
		env_universal_remove_all();
		env_universal_barrier();
	}
}


void env_universal_init( wchar_t * p, 
						 wchar_t *u, 
						 void (*sf)(),
						 void (*cb)( int type, const wchar_t *name, const wchar_t *val ))
{
	path=p;
	user=u;
	start_fishd=sf;	
	external_callback = cb;

	connection_init( &env_universal_server, -1 );
	
	env_universal_server.fd = get_socket(1);
	env_universal_common_init( &callback );
	env_universal_read_all();	
	init = 1;	
	if( env_universal_server.fd >= 0 )
	{
		env_universal_barrier();
	}
}

void env_universal_destroy()
{
	/*
	  Go into blocking mode and send all data before exiting
	*/
	if( env_universal_server.fd >= 0 )
	{
		if( fcntl( env_universal_server.fd, F_SETFL, 0 ) != 0 )
		{
			wperror( L"fcntl" );
		}
		try_send_all( &env_universal_server );		
	}

	connection_destroy( &env_universal_server );
	env_universal_server.fd =-1;
	env_universal_common_destroy();
	init = 0;
}


/**
   Read all available messages from the server.
*/
int env_universal_read_all()
{
	if( !init)
		return 0;

	if( env_universal_server.fd == -1 )
	{
		reconnect();		
		if( env_universal_server.fd == -1 )
			return 0;		
	}
	
	if( env_universal_server.fd != -1 )
	{
		read_message( &env_universal_server );
		check_connection();		
		return 1;
	}
	else
	{
		debug( 2, L"No connection to universal variable server" );
		return 0;
	}		
}

wchar_t *env_universal_get( const wchar_t *name )
{
	if( !init)
		return 0;

	CHECK( name, 0 );
	
	return env_universal_common_get( name );
}

int env_universal_get_export( const wchar_t *name )
{
	if( !init)
		return 0;

	CHECK( name, 0 );
	
	return env_universal_common_get_export( name );
}

void env_universal_barrier()
{
	message_t *msg;
	fd_set fds;

	if( !init || is_dead() )
		return;

	barrier_reply = 0;

	/*
	  Create barrier request
	*/
	msg= create_message( BARRIER, 0, 0);
	msg->count=1;
	q_put( &env_universal_server.unsent, msg );

	/*
	  Wait until barrier request has been sent
	*/
	debug( 3, L"Create barrier" );
	while( 1 )
	{
		try_send_all( &env_universal_server );	
		check_connection();		
		
		if( q_empty( &env_universal_server.unsent ) )
			break;
		
		if( env_universal_server.fd == -1 )
		{
			reconnect();
			debug( 2, L"barrier interrupted, exiting" );
			return;			
		}
		
		FD_ZERO( &fds );
		FD_SET( env_universal_server.fd, &fds );
		select( env_universal_server.fd+1, 0, &fds, 0, 0 );
	}

	/*
	  Wait for barrier reply
	*/
	debug( 3, L"Sent barrier request" );
	while( !barrier_reply )
	{
		if( env_universal_server.fd == -1 )
		{
			reconnect();
			debug( 2, L"barrier interrupted, exiting (2)" );
			return;			
		}		
		FD_ZERO( &fds );
        FD_SET( env_universal_server.fd, &fds );		
        select( env_universal_server.fd+1, &fds, 0, 0, 0 );
		env_universal_read_all();
	}
	debug( 3, L"End barrier" );
}


void env_universal_set( const wchar_t *name, const wchar_t *value, int export )
{
	message_t *msg;
	
	if( !init )
		return;

	CHECK( name, );
		
	debug( 3, L"env_universal_set( \"%ls\", \"%ls\" )", name, value );

	if( is_dead() )
	{
		env_universal_common_set( name, value, export );
	}
	else
	{
		msg = create_message( export?SET_EXPORT:SET, 
							  name, 
							  value);

		if( !msg )
		{
			debug( 1, L"Could not create universal variable message" );
			return;
		}
		
		msg->count=1;
		q_put( &env_universal_server.unsent, msg );
		env_universal_barrier();
	}
}

int env_universal_remove( const wchar_t *name )
{
	int res;
	
	message_t *msg;
	if( !init )
		return 1;
		
	CHECK( name, 1 );

	res = !env_universal_common_get( name );
	debug( 3,
		   L"env_universal_remove( \"%ls\" )",
		   name );
		
	if( is_dead() )
	{
		env_universal_common_remove( name );
	}
	else
	{
		msg= create_message( ERASE, name, 0);
		msg->count=1;
		q_put( &env_universal_server.unsent, msg );
		env_universal_barrier();
	}
	
	return res;
}

void env_universal_get_names( array_list_t *l,
                              int show_exported,
                              int show_unexported )
{
	if( !init )
		return;

	CHECK( l, );
	
	env_universal_common_get_names( l, 
									show_exported,
									show_unexported );	
}
