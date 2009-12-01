/**
   \file env_universal_common.c

   The utility library for universal variables. Used both by the
   client library and by the daemon.

*/
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
#include <sys/stat.h>
#include <dirent.h>
#include <wctype.h>
#include <iconv.h>

#include <errno.h>
#include <locale.h>
#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "fallback.h"
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
   ERROR string for internal buffered reader
*/
#define ENV_UNIVERSAL_ERROR 0x100

/**
   EAGAIN string for internal buffered reader
*/
#define ENV_UNIVERSAL_AGAIN 0x101

/**
   EOF string for internal buffered reader
*/
#define ENV_UNIVERSAL_EOF   0x102

/**
   A variable entry. Stores the value of a variable and whether it
   should be exported. Obviously, it needs to be allocated large
   enough to fit the value string.
*/
typedef struct var_uni_entry
{
	int export; /**< Whether the variable should be exported */
	wchar_t val[1]; /**< The value of the variable */
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

/**
   List of names for the UTF-8 character set.
 */
static char *iconv_utf8_names[]=
  {
    "utf-8", "UTF-8",
    "utf8", "UTF8",
    0
  }
  ;

/**
    List of wide character names, undefined byte length.
 */
static char *iconv_wide_names_unknown[]=
  {
    "wchar_t", "WCHAR_T", 
    "wchar", "WCHAR", 
    0
  }
  ;

/**
   List of wide character names, 4 bytes long.
 */
static char *iconv_wide_names_4[]=
  {
    "wchar_t", "WCHAR_T", 
    "wchar", "WCHAR", 
    "ucs-4", "UCS-4", 
    "ucs4", "UCS4", 
    "utf-32", "UTF-32", 
    "utf32", "UTF32", 
    0
  }
  ;

/**
   List of wide character names, 2 bytes long.
 */
static char *iconv_wide_names_2[]=
  {
    "wchar_t", "WCHAR_T", 
    "wchar", "WCHAR", 
    "ucs-2", "UCS-2", 
    "ucs2", "UCS2", 
    "utf-16", "UTF-16", 
    "utf16", "UTF16", 
    0
  }
  ;

/**
   Convert utf-8 string to wide string
 */
static wchar_t *utf2wcs( const char *in )
{
	iconv_t cd=(iconv_t) -1;
	int i,j;

	wchar_t *out;

	/*
	  Try to convert to wchar_t. If that is not a valid character set,
	  try various names for ucs-4. We can't be sure that ucs-4 is
	  really the character set used by wchar_t, but it is the best
	  assumption we can make.
	*/
	char **to_name=0;

	switch (sizeof (wchar_t))
	{
	
		case 2:
			to_name = iconv_wide_names_2;
			break;

		case 4:
			to_name = iconv_wide_names_4;
			break;
			
		default:
			to_name = iconv_wide_names_unknown;
			break;
	}
	

	/*
	  The line protocol fish uses is always utf-8. 
	*/
	char **from_name = iconv_utf8_names;

	size_t in_len = strlen( in );
	size_t out_len =  sizeof( wchar_t )*(in_len+2);
	size_t nconv;
	char *nout;
	
	out = malloc( out_len );
	nout = (char *)out;

	if( !out )
		return 0;
	
	for( i=0; to_name[i]; i++ )
	{
		for( j=0; from_name[j]; j++ )
		{
			cd = iconv_open ( to_name[i], from_name[j] );

			if( cd != (iconv_t) -1)
			{
				goto start_conversion;
				
			}
		}
	}

  start_conversion:

	if (cd == (iconv_t) -1)
	{
		/* Something went wrong.  */
		debug( 0, L"Could not perform utf-8 conversion" );		
		if(errno != EINVAL)
			wperror( L"iconv_open" );
		
		/* Terminate the output string.  */
		free(out);
		return 0;		
	}
	
	nconv = iconv( cd, (char **)&in, &in_len, &nout, &out_len );
		
	if (nconv == (size_t) -1)
	{
		debug( 0, L"Error while converting from utf string" );
		return 0;
	}
	     
	*((wchar_t *) nout) = L'\0';

	/*
		Check for silly iconv behaviour inserting an bytemark in the output
		string.
	 */
	if (*out == L'\xfeff' || *out == L'\xffef' || *out == L'\xefbbbf')
	{
		wchar_t *out_old = out;
		out = wcsdup(out+1);
		if (! out )
		{
			debug(0, L"FNORD!!!!");
			free( out_old );
			return 0;
		}
		free( out_old );
	}
	
	
	if (iconv_close (cd) != 0)
		wperror (L"iconv_close");
	
	return out;	
}

/**
   Convert wide string to utf-8
 */
static char *wcs2utf( const wchar_t *in )
{
	iconv_t cd=(iconv_t) -1;
	int i,j;
	
	char *char_in = (char *)in;
	char *out;

	/*
	  Try to convert to wchar_t. If that is not a valid character set,
	  try various names for ucs-4. We can't be sure that ucs-4 is
	  really the character set used by wchar_t, but it is the best
	  assumption we can make.
	*/
	char **from_name=0;

	switch (sizeof (wchar_t))
	{
	
		case 2:
			from_name = iconv_wide_names_2;
			break;

		case 4:
			from_name = iconv_wide_names_4;
			break;
			
		default:
			from_name = iconv_wide_names_unknown;
			break;
	}

	char **to_name = iconv_utf8_names;

	size_t in_len = wcslen( in );
	size_t out_len =  sizeof( char )*( (MAX_UTF8_BYTES*in_len)+1);
	size_t nconv;
	char *nout;
	
	out = malloc( out_len );
	nout = (char *)out;
	in_len *= sizeof( wchar_t );

	if( !out )
		return 0;
	
	for( i=0; to_name[i]; i++ )
	{
		for( j=0; from_name[j]; j++ )
		{
			cd = iconv_open ( to_name[i], from_name[j] );
			
			if( cd != (iconv_t) -1)
			{
				goto start_conversion;
				
			}
		}
	}

  start_conversion:

	if (cd == (iconv_t) -1)
	{
		/* Something went wrong.  */
		debug( 0, L"Could not perform utf-8 conversion" );		
		if(errno != EINVAL)
			wperror( L"iconv_open" );
		
		/* Terminate the output string.  */
		free(out);
		return 0;		
	}
	
	nconv = iconv( cd, &char_in, &in_len, &nout, &out_len );
	

	if (nconv == (size_t) -1)
	{
		debug( 0, L"%d %d", in_len, out_len );
		debug( 0, L"Error while converting from to string" );
		return 0;
	}
	     
	*nout = '\0';
	
	if (iconv_close (cd) != 0)
		wperror (L"iconv_close");
	
	return out;	
}



void env_universal_common_init( void (*cb)(int type, const wchar_t *key, const wchar_t *val ) )
{
	callback = cb;
	hash_init( &env_universal_var, &hash_wcs_func, &hash_wcs_cmp );
}

/**
   Free both key and data
*/
static void erase( void *key,
				   void *data )
{
	free( (void *)key );
	free( (void *)data );
}


void env_universal_common_destroy()
{
	hash_foreach( &env_universal_var, &erase );
	hash_destroy( &env_universal_var );
}

/**
   Read one byte of date form the specified connection
 */
static int read_byte( connection_t *src )
{

	if( src->buffer_consumed >= src->buffer_used )
	{

		int res;

		res = read( src->fd, src->buffer, ENV_UNIVERSAL_BUFFER_SIZE );
		
//		debug(4, L"Read chunk '%.*s'", res, src->buffer );
		
		if( res < 0 )
		{

			if( errno == EAGAIN ||
				errno == EINTR )
			{
				return ENV_UNIVERSAL_AGAIN;
			}
		
			return ENV_UNIVERSAL_ERROR;

		}
		
		if( res == 0 )
		{
			return ENV_UNIVERSAL_EOF;
		}
		
		src->buffer_consumed = 0;
		src->buffer_used = res;
	}
	
	return src->buffer[src->buffer_consumed++];

}


void read_message( connection_t *src )
{
	while( 1 )
	{
		
		int ib = read_byte( src );
		char b;
		
		switch( ib )
		{
			case ENV_UNIVERSAL_AGAIN:
			{
				return;
			}

			case ENV_UNIVERSAL_ERROR:
			{
				debug( 2, L"Read error on fd %d, set killme flag", src->fd );
				if( debug_level > 2 )
					wperror( L"read" );
				src->killme = 1;
				return;
			}

			case ENV_UNIVERSAL_EOF:
			{
				src->killme = 1;
				debug( 3, L"Fd %d has reached eof, set killme flag", src->fd );
				if( src->input.used > 0 )
				{
					char c = 0;
					b_append( &src->input, &c, 1 );
					debug( 1, 
						   L"Universal variable connection closed while reading command. Partial command recieved: '%s'", 
						   (wchar_t *)src->input.buff  );
				}
				return;
			}
		}
		
		b = (char)ib;
		
		if( b == '\n' )
		{
			wchar_t *msg;
			
			b = 0;
			b_append( &src->input, &b, 1 );
			
			msg = utf2wcs( src->input.buff );
			
			/*
			  Before calling parse_message, we must empty reset
			  everything, since the callback function could
			  potentially call read_message.
			*/
			src->input.used=0;
			
			if( msg )
			{
				parse_message( msg, src );	
			}
			else
			{
				debug( 0, _(L"Could not convert message '%s' to wide character string"), src->input.buff );
			}
			
			free( msg );
			
		}
		else
		{
			b_append( &src->input, &b, 1 );
		}
	}
}

/**
   Remove variable with specified name
*/
void env_universal_common_remove( const wchar_t *name )
{
	void *k, *v;
	hash_remove( &env_universal_var, 
				 name,
				 &k,
				 &v );
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

void env_universal_common_set( const wchar_t *key, const wchar_t *val, int export )
{
	var_uni_entry_t *entry;
	wchar_t *name;

	CHECK( key, );
	CHECK( val, );
	
	entry = malloc( sizeof(var_uni_entry_t) + sizeof(wchar_t)*(wcslen(val)+1) );			
	name = wcsdup(key);
	
	if( !entry || !name )
		DIE_MEM();
	
	entry->export=export;
	
	wcscpy( entry->val, val );
	env_universal_common_remove( name );
	
	hash_put( &env_universal_var, name, entry );
			
	if( callback )
	{
		callback( export?SET_EXPORT:SET, name, val );
	}
}


/**
   Parse message msg
*/
static void parse_message( wchar_t *msg, 
						   connection_t *src )
{
//	debug( 3, L"parse_message( %ls );", msg );
	
	if( msg[0] == L'#' )
		return;
	
	if( match( msg, SET_STR ) || match( msg, SET_EXPORT_STR ))
	{
		wchar_t *name, *tmp;
		int export = match( msg, SET_EXPORT_STR );
		
		name = msg+(export?wcslen(SET_EXPORT_STR):wcslen(SET_STR));
		while( wcschr( L"\t ", *name ) )
			name++;
		
		tmp = wcschr( name, L':' );
		if( tmp )
		{
			wchar_t *key;
			wchar_t *val;
			
			key = malloc( sizeof( wchar_t)*(tmp-name+1));
			memcpy( key, name, sizeof( wchar_t)*(tmp-name));
			key[tmp-name]=0;
			
			val = tmp+1;
			val = unescape( val, 0 );
			
			env_universal_common_set( key, val, export );
			
			free( val );
			free( key );
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

		env_universal_common_remove( name );
		
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

	if( res != -1 )
	{
		debug( 4, L"Wrote message '%s'", msg->body );
	}
	else
	{
		debug( 4, L"Failed to write message '%s'", msg->body );
	}
	
	if( res == -1 )
	{
		switch( errno )
		{
			case EAGAIN:
				return 0;
				
			default:
				debug( 2,
					   L"Error while sending universal variable message to fd %d. Closing connection",
					   fd );
				if( debug_level > 2 )
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
/*	debug( 3,
		   L"Send all updates to connection on fd %d", 
		   c->fd );*/
	while( !q_empty( &c->unsent) )
	{
		switch( try_send( (message_t *)q_peek( &c->unsent), c->fd ) )
		{
			case 1:
				q_get( &c->unsent);
				break;
				
			case 0:
				debug( 4,
					   L"Socket full, send rest later" );	
				return;
								
			case -1:
				c->killme = 1;
				return;
		}
	}
}

/**
   Escape specified string
 */
static wchar_t *full_escape( const wchar_t *in )
{
	string_buffer_t out;
	sb_init( &out );
	for( ; *in; in++ )
	{
		if( *in < 32 )
		{
			sb_printf( &out, L"\\x%.2x", *in );
		}
		else if( *in < 128 )
		{
			sb_append_char( &out, *in );
		}
		else if( *in < 65536 )
		{
			sb_printf( &out, L"\\u%.4x", *in );
		}
		else
		{
			sb_printf( &out, L"\\U%.8x", *in );
		}
	}
	return (wchar_t *)out.buff;
}


message_t *create_message( int type,
						   const wchar_t *key_in, 
						   const wchar_t *val_in )
{
	message_t *msg=0;
	
	char *key=0;
	size_t sz;

//	debug( 4, L"Crete message of type %d", type );
	
	if( key_in )
	{
		if( wcsvarname( key_in ) )
		{
			debug( 0, L"Illegal variable name: '%ls'", key_in );
			return 0;
		}
		
		key = wcs2utf(key_in);
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
			
			wchar_t *esc = full_escape( val_in );
			if( !esc )
				break;
			
			char *val = wcs2utf(esc );
			free(esc);
						
			sz = strlen(type==SET?SET_MBS:SET_EXPORT_MBS) + strlen(key) + strlen(val) + 4;
			msg = malloc( sizeof( message_t ) + sz );
			
			if( !msg )
				DIE_MEM();
			
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
				DIE_MEM();
			
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
				DIE_MEM();
			strcpy( msg->body, BARRIER_MBS "\n" );
			break;
		}
		
		case BARRIER_REPLY:
		{
			msg = malloc( sizeof( message_t )  +
						  strlen( BARRIER_REPLY_MBS ) +2);
			if( !msg )
				DIE_MEM();
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

//	debug( 4, L"Message body is '%s'", msg->body );

	return msg;	
}

/**
   Function used with hash_foreach to insert keys of one table into
   another
*/
static void add_key_to_hash( void *key, 
							 void *data,
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
static void enqueue( void *k,
					 void *v,
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


void connection_init( connection_t *c, int fd )
{
	memset (c, 0, sizeof (connection_t));
	c->fd = fd;
	b_init( &c->input );
	q_init( &c->unsent );
	c->buffer_consumed = c->buffer_used = 0;	
}

void connection_destroy( connection_t *c)
{
	q_destroy( &c->unsent );
	b_destroy( &c->input );

	/*
	  A connection need not always be open - we only try to close it
	  if it is open.
	*/
	if( c->fd >= 0 )
	{
		if( close( c->fd ) )
		{
			wperror( L"close" );
		}
	}
}
