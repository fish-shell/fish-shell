/**
   \file env_universal_common.c

   The utility library for universal variables. Used both by the
   client library and by the daemon.

*/
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pwd.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <wctype.h>

#include <errno.h>
#include <locale.h>
#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>

#include "util.h"
#include "common.h"
#include "wutil.h"
#include "env_universal_common.h"

/**
   Non-wide version of the set command
*/
#define SET_MBS "SET"

/**
   Non-wide version of the erase command
*/
#define ERASE_MBS "ERASE"

#define BARRIER_MBS "BARRIER"
#define BARRIER_REPLY_MBS "BARRIER_REPLY"

/**
   Error message
*/
#define PARSE_ERR L"Unable to parse universal variable message: '%ls'"

static void parse_message( wchar_t *msg,
						   connection_t *src );

/**
   The table of all universal variables
*/
hash_table_t env_universal_var;

void (*callback)( int type, 
				  const wchar_t *key, 
				  const wchar_t *val );



void env_universal_common_init( void (*cb)(int type, const wchar_t *key, const wchar_t *val ) )
{
	debug( 2, L"Init env_universal_common" );
	callback = cb;
	hash_init( &env_universal_var, &hash_wcs_func, &hash_wcs_cmp );
}

static void erase( const void *key,
				   const void *data )
{
	free( (void *)key );
	free( (void *)data );
}


void env_universal_common_destroy()
{
	hash_foreach( &env_universal_var, &erase );
	hash_destroy( &env_universal_var );
}


void read_message( connection_t *src )
{
	while( 1 )
	{
		char b;		
		int read_res = read( src->fd, &b, 1 );
		wchar_t res=0;
		
		if( read_res < 0 )
		{
			if( errno != EAGAIN && 
				errno != EINTR )
			{
				debug( 2, L"Read error on fd %d, set killme flag", src->fd );
				wperror( L"read" );
				src->killme = 1;
			}
			return;
		}
		if( read_res == 0 )
		{
			src->killme = 1;
			debug( 3, L"Fd %d has reached eof, set killme flag", src->fd );
			if( src->input.used > 0 )
			{
				debug( 1, 
				       L"Universal variable connection closed while reading command. Partial command recieved: '%ls'", 
				       (wchar_t *)src->input.buff  );
			}
			return;
		}
		
		int sz = mbrtowc( &res, &b, 1, &src->wstate );
		
		if( sz == -1 )
		{			
			debug( 1, L"Error while reading universal variable after '%ls'", (wchar_t *)src->input.buff  );
			wperror( L"mbrtowc" );
		}
		else if( sz > 0 )
		{
			if( res == L'\n' )
			{
				parse_message( (wchar_t *)src->input.buff, src );	
				sb_clear( &src->input );
				memset (&src->wstate, '\0', sizeof (mbstate_t));
			}
			else
			{
				sb_printf( &src->input, L"%lc", res );
			}
		}
	}
}

static void remove_entry( wchar_t *name )
{
	void *k, *v;
	hash_remove( &env_universal_var, 
				 name,
				 (const void **)&k,
				 (const void **)&v );
	free( k );
	free( v );
}

static int match( const wchar_t *msg, const wchar_t *cmd )
{
	size_t len = wcslen( cmd );
	if( wcsncasecmp( msg, cmd, len ) != 0 )
		return 0;
	if( msg[len] && msg[len]!= L' ' && msg[len] != L'\t' )
		return 0;
	
	return 1;
}


static void parse_message( wchar_t *msg, 
						   connection_t *src )
{
	debug( 2, L"parse_message( %ls );", msg );

	if( msg[0] == L'#' )
		return;
		
	if( match( msg, SET_STR ) )
	{
		wchar_t *name, *val, *tmp;
			
		name = msg+wcslen(SET_STR);
		while( wcschr( L"\t ", *name ) )
			name++;
		
		tmp = wcschr( name, L':' );
		if( tmp )
		{
			wchar_t *key =malloc( sizeof( wchar_t)*(tmp-name+1));
			memcpy( key, name, sizeof( wchar_t)*(tmp-name));
			key[tmp-name]=0;
			
			val = tmp+1;
			
			val = unescape( wcsdup(val), 0 );
			
			remove_entry( key );
			
			hash_put( &env_universal_var, key, val );
			
			if( callback )
			{
				callback( SET, key, val );
			}
		}
		else
		{
			debug( 1, PARSE_ERR, msg );
		}			
	}
	else if( match( msg, ERASE_STR ) )
	{
		wchar_t *name, *val, *tmp;
		
		name = msg+wcslen(ERASE_STR);
		while( wcschr( L"\t ", *name ) )
			name++;
		
		tmp = name;
		while( iswalnum( *tmp ) || *tmp == L'_')
			tmp++;
		
		*tmp = 0;
		
		if( !wcslen( name ) )
		{
			debug( 1, PARSE_ERR, msg );
		}

		remove_entry( name );
		
		if( callback )
		{
			callback( ERASE, name, 0 );
		}
	}
	else if( match( msg, BARRIER_STR) )
	{
		message_t *msg = create_message( BARRIER_REPLY, 0, 0 );
		msg->count = 1;
		q_put( &src->unsent, msg );
		try_send_all( src );
	}
	else if( match( msg, BARRIER_REPLY_STR ) )
	{
		if( callback )
		{
			callback( BARRIER_REPLY, 0, 0 );
		}
	}
	else
	{
		debug( 1, PARSE_ERR, msg );
	}		
}

int try_send( message_t *msg,
			  int fd )
{

	int res = write( fd, msg->body, strlen(msg->body) );
	
	if( res == -1 )
	{
		switch( errno )
		{
			case EAGAIN:
				return 0;
				
			default:
				debug( 1,
					   L"Error while sending message to fd %d. Closing connection",
					   fd );
				wperror( L"write" );
				
				return -1;
		}		
	}
	msg->count--;
	
	if( !msg->count )
	{
		free( msg );
	}
	return 1;
}

void try_send_all( connection_t *c )
{
	debug( 2,
		   L"Send all updates to connection on fd %d", 
		   c->fd );
	while( !q_empty( &c->unsent) )
	{
		
		switch( try_send( (message_t *)q_peek( &c->unsent), c->fd ) )
		{
			case 1:
				q_get( &c->unsent);
				break;
				
			case 0:
				return;
								
			case -1:
				c->killme = 1;
				return;
		}
	}
}

message_t *create_message( int type,
						   const wchar_t *key_in, 
						   const wchar_t *val_in )
{
	message_t *msg=0;
	
	char *key=0;
	size_t sz;
	
	if( key_in )
	{
		key = wcs2str(key_in);
		if( !key )
		{
			debug( 0,
				   L"Could not convert %ls to narrow character string",
				   key_in );
			return 0;
		}
	}
	
	
	switch( type )
	{
		case SET:
		{
			if( !val_in )
			{
				val_in=L"";
			}
			
			wchar_t *esc = escape(wcsdup(val_in),1);
			if( !esc )
				break;
			
			char *val = wcs2str(esc );
			free(esc);
			
			
			sz = strlen(SET_MBS) + strlen(key) + strlen(val) + 4;
			msg = malloc( sizeof( message_t ) + sz );
	
			if( !msg )
				die_mem();
				
			strcpy( msg->body, SET_MBS " " );
			strcat( msg->body, key );
			strcat( msg->body, ":" );
			strcat( msg->body, val );
			strcat( msg->body, "\n" );

			free( val );
			
			break;
		}

		case ERASE:
		{
            sz = strlen(ERASE_MBS) + strlen(key) + 3;
            msg = malloc( sizeof( message_t ) + sz );

			if( !msg )
				die_mem();
			
            strcpy( msg->body, ERASE_MBS " " );
            strcat( msg->body, key );
            strcat( msg->body, "\n" );
            break;
		}

		case BARRIER:
		{
			msg = malloc( sizeof( message_t )  + 
						  strlen( BARRIER_MBS ) +2);
			if( !msg )
				die_mem();
			strcpy( msg->body, BARRIER_MBS "\n" );
			break;
		}
		
		case BARRIER_REPLY:
		{
			msg = malloc( sizeof( message_t )  +
						  strlen( BARRIER_REPLY_MBS ) +2);
			if( !msg )
				die_mem();
			strcpy( msg->body, BARRIER_REPLY_MBS "\n" );
			break;
		}
		
		default:
		{
			debug( 0, L"create_message: Unknown message type" );
		}
	}

	free( key );

	if( msg )
		msg->count=0;
	return msg;	
}
