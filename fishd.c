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


#ifndef UNIX_PATH_MAX 
#define UNIX_PATH_MAX 100
#endif

#define GREETING "#Fish universal variable daemon\n#Lines beginning with '#' are ignored\n#Syntax:\n#SET VARNAME:VALUE\n#or\n#ERASE VARNAME\n#Where VALUE is the escaped value of the variable\n#Backslash escapes and \\xxx hexadecimal style escapes are supported\n"
#define FILE ".fishd"


static connection_t *conn;
static int sock;


int get_socket()
{
	int s, len;
	struct sockaddr_un local;
	char *name;
	char *dir = getenv( "FISHD_SOCKET_DIR" );
	char *uname = getenv( "USER" );

	if( !dir )
		dir = "/tmp";
	if( uname==0 )
	{
		struct passwd *pw;
		pw = getpwuid( getuid() );
		uname = strdup( pw->pw_name );
	}
		
	name = malloc( strlen(dir)+ strlen(uname)+ strlen(SOCK_FILENAME) + 2 );
	strcpy( name, dir );
	strcat( name, "/" );
	strcat( name, SOCK_FILENAME );
	strcat( name, uname );

	if( strlen( name ) >= UNIX_PATH_MAX )
	{
		debug( 1, L"Filename too long: '%s'", name );
		exit(1);
	}
	
	debug( 1, L"Connect to socket at %s", name );
	
	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}
	
	local.sun_family = AF_UNIX;
	strcpy(local.sun_path, name );
	unlink(local.sun_path);
	len = strlen(local.sun_path) + sizeof(local.sun_family);

	free( name );
	
	if (bind(s, (struct sockaddr *)&local, len) == -1) {
		perror("bind");
		exit(1);
	}
/*		
	if( setsockopt( s, SOL_SOCKET, SO_PASSCRED, &on, sizeof( on ) ) )
	{
		perror( "setsockopt");
		exit(1);
	}
*/

	if( fcntl( s, F_SETFL, O_NONBLOCK ) != 0 )
	{
		wperror( L"fcntl" );
		close( s );		
		return -1;

	}
	
	return s;
}

void enqueue( const void *k,
			  const void *v,
			  void *q)
{
	const wchar_t *key = (const wchar_t *)k;
	const wchar_t *val = (const wchar_t *)v;
	queue_t *queue = (queue_t *)q;
	
	message_t *msg = create_message( SET, key, val );
	msg->count=1;
	
	q_put( queue, msg );
}

void enqueue_all( connection_t *c )
{
	hash_foreach2( &env_universal_var,
	               &enqueue, 
	               (void *)&c->unsent );
	try_send_all( c );
}

void broadcast( int type, const wchar_t *key, const wchar_t *val )
{
	connection_t *c;
	message_t *msg;

	if( !conn )
		return;
	
	msg = create_message( type, key, val );

	/*
	  Don't merge loops, or try_send_all can free the message prematurely
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

void daemonize()
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
	setpgrp();

	/*
	  Close stdin and stdout
	*/
	close( 0 );
	close( 1 );

}


void load()
{
	struct passwd *pw;
	char *name;
	char *dir = getenv( "HOME" );
	if( !dir )
	{
		pw = getpwuid( getuid() );
		dir = pw->pw_dir;
	}
		
	name = malloc( strlen(dir)+ strlen(FILE)+ 2 );
	strcpy( name, dir );
	strcat( name, "/" );
	strcat( name, FILE );
	
	debug( 1, L"Open file for loading: '%s'", name );
	
	connection_t load;
	load.fd = open( name, O_RDONLY);
	
	free( name );
	
	if( load.fd == -1 )
	{
		debug( 0, L"Could not open save file. No previous saves?" );
	}
	debug( 1, L"Load input file on fd %d", load.fd );
	sb_init( &load.input );
	memset (&load.wstate, '\0', sizeof (mbstate_t));
	read_message( &load );
	sb_destroy( &load.input );
	close( load.fd );
}

void save()
{
	struct passwd *pw;
	char *name;
	char *dir = getenv( "HOME" );
	if( !dir )
	{
		pw = getpwuid( getuid() );
		dir = pw->pw_dir;
	}
		
	name = malloc( strlen(dir)+ strlen(FILE)+ 2 );
	strcpy( name, dir );
	strcat( name, "/" );
	strcat( name, FILE );
	
	debug( 1, L"Open file for saving: '%s'", name );
	
	connection_t save;
	save.fd = open( name, O_CREAT | O_TRUNC | O_WRONLY);
	free( name );

	if( save.fd == -1 )
	{
		debug( 0, L"Could not open save file" );
		wperror( L"open" );
		exit(1);
	}
	debug( 1, L"File open on fd %d'", save.fd );
	q_init( &save.unsent );
	enqueue_all( &save );
	close( save.fd );
	q_destroy( &save.unsent );
}

static void init()
{
	program_name=L"fishd";
	sock = get_socket();
	if (listen(sock, 64) == -1) 
	{
		wperror(L"listen");
		exit(1);
	}	

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
				if( update_count >= 8 )
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
