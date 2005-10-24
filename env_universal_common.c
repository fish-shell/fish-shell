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
   Non-wide version of the set_export command
*/
#define SET_EXPORT_MBS "SET_EXPORT"

/**
   Non-wide version of the erase command
*/
#define ERASE_MBS "ERASE"

/**
   Non-wide version of the barrier command
*/
#define BARRIER_MBS "BARRIER"

/**
   Non-wide version of the barrier_reply command
*/
#define BARRIER_REPLY_MBS "BARRIER_REPLY"

/**
   Error message
*/
#define PARSE_ERR L"Unable to parse universal variable message: '%ls'"

/**
   A variable entry. Stores the value of a variable and whether it
   should be exported. Obviously, it needs to be allocated large
   enough to fit the value string.
*/
typedef struct var_uni_entry
{
	int export; /**< Whether the variable should be exported */
	wchar_t val[0]; /**< The value of the variable */
}
var_uni_entry_t;


static void parse_message( wchar_t *msg,
						   connection_t *src );

/**
   The table of all universal variables
*/
hash_table_t env_universal_var;

/**
   Callback function, should be called on all events
*/
void (*callback)( int type, 
				  const wchar_t *key, 
				  const wchar_t *val );


/**
   Variable used by env_get_names to communicate auxiliary information
   to add_key_to_hash
*/
static int get_names_show_exported;
/**
   Variable used by env_get_names to communicate auxiliary information
   to add_key_to_hash
*/
static int get_names_show_unexported;


void env_universal_common_init( void (*cb)(int type, const wchar_t *key, const wchar_t *val ) )
{
	debug( 2, L"Init env_universal_common" );
	callback = cb;
	hash_init( &env_universal_var, &hash_wcs_func, &hash_wcs_cmp );
}

/**
   Free both key and data
*/
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
				/*
				  Before calling parse_message, we must empty reset
				  everything, since the callback function could
				  potentially call read_message.
				*/
				
				wchar_t *msg = wcsdup( (wchar_t *)src->input.buff );
				sb_clear( &src->input );
			 	memset (&src->wstate, '\0', sizeof (mbstate_t));


				parse_message( msg, src );	
				free( msg );
				
			}
			else
			{
				sb_printf( &src->input, L"%lc", res );
			}
		}
	}
}

/**
   Remove variable with specified name
*/
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

/**
   Test if the message msg contains the command cmd
*/
static int match( const wchar_t *msg, const wchar_t *cmd )
{
	size_t len = wcslen( cmd );
	if( wcsncasecmp( msg, cmd, len ) != 0 )
		return 0;

	if( msg[len] && msg[len]!= L' ' && msg[len] != L'\t' )
		return 0;
	
	return 1;
}

/**
   Parse message msg
*/
static void parse_message( wchar_t *msg, 
						   connection_t *src )
{
	debug( 2, L"parse_message( %ls );", msg );
	
	if( msg[0] == L'#' )
		return;
	
	if( match( msg, SET_STR ) || match( msg, SET_EXPORT_STR ))
	{
		wchar_t *name, *val, *tmp;
		int export = match( msg, SET_EXPORT_STR );
		
		name = msg+(export?wcslen(SET_EXPORT_STR):wcslen(SET_STR));
		while( wcschr( L"\t ", *name ) )
			name++;
		
		tmp = wcschr( name, L':' );
		if( tmp )
		{
			wchar_t *key =malloc( sizeof( wchar_t)*(tmp-name+1));
			memcpy( key, name, sizeof( wchar_t)*(tmp-name));
			key[tmp-name]=0;
			
			val = tmp+1;
			

			val = unescape( val, 0 );
			
			var_uni_entry_t *entry = 
				malloc( sizeof(var_uni_entry_t) + sizeof(wchar_t)*(wcslen(val)+1) );			
			if( !entry )
				die_mem();
			entry->export=export;
			
			wcscpy( entry->val, val );
			remove_entry( key );
			
			hash_put( &env_universal_var, key, entry );
			
			if( callback )
			{
				callback( export?SET_EXPORT:SET, key, val );
			}
			free(val );
		}
		else
		{
			debug( 1, PARSE_ERR, msg );
		}			
	}
	else if( match( msg, ERASE_STR ) )
	{
		wchar_t *name, *tmp;
		
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

/**
   Attempt to send the specified message to the specified file descriptor

   \return 1 on sucess, 0 if the message could not be sent without blocking and -1 on error
*/
static int try_send( message_t *msg,
					 int fd )
{

	debug( 3,
		   L"before write of %d chars to fd %d", strlen(msg->body), fd );	

	int res = write( fd, msg->body, strlen(msg->body) );
		
	if( res == -1 )
	{
		switch( errno )
		{
			case EAGAIN:
				return 0;
				
			default:
				debug( 1,
					   L"Error while sending universal variable message to fd %d. Closing connection",
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
	debug( 3,
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
				debug( 1,
					   L"Socket full, send rest later" );	
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
		case SET_EXPORT:
		{
			if( !val_in )
			{
				val_in=L"";
			}
			
			wchar_t *esc = escape(val_in,1);
			if( !esc )
				break;
			
			char *val = wcs2str(esc );
			free(esc);
						
			sz = strlen(type==SET?SET_MBS:SET_EXPORT_MBS) + strlen(key) + strlen(val) + 4;
			msg = malloc( sizeof( message_t ) + sz );
			
			if( !msg )
				die_mem();
			
			strcpy( msg->body, (type==SET?SET_MBS:SET_EXPORT_MBS) );
			strcat( msg->body, " " );
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

/**
   Function used with hash_foreach to insert keys of one table into
   another
*/
static void add_key_to_hash( const void *key, 
							 const void *data,
							 void *aux )
{
	var_uni_entry_t *e = (var_uni_entry_t *)data;
	if( ( e->export && get_names_show_exported) || 
		( !e->export && get_names_show_unexported) )
		al_push( (array_list_t *)aux, key );
}

void env_universal_common_get_names( array_list_t *l,
									 int show_exported,
									 int show_unexported )
{
	get_names_show_exported = show_exported;
	get_names_show_unexported = show_unexported;
	
	hash_foreach2( &env_universal_var, 
				   add_key_to_hash,
				   l );
}

wchar_t *env_universal_common_get( const wchar_t *name )
{
	var_uni_entry_t *e = (var_uni_entry_t *)hash_get( &env_universal_var, name );	
	if( e )
		return e->val;
	return 0;	
}

int env_universal_common_get_export( const wchar_t *name )
{
	var_uni_entry_t *e = (var_uni_entry_t *)hash_get( &env_universal_var, name );
	if( e )
		return e->export;
	return 0;
}

/**
   Adds a variable creation message about the specified variable to
   the specified queue. The function signature is non-obvious since
   this function is used together with hash_foreach2, which requires
   the specified function signature.

   \param k the variable name
   \param v the variable value
   \param q the queue to add the message to
*/
static void enqueue( const void *k,
					 const void *v,
					 void *q)
{
	const wchar_t *key = (const wchar_t *)k;
	const var_uni_entry_t *val = (const var_uni_entry_t *)v;
	dyn_queue_t *queue = (dyn_queue_t *)q;
	
	message_t *msg = create_message( val->export?SET_EXPORT:SET, key, val->val );
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

