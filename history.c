/** \file history.c
  History functions, part of the user interface.
*/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "history.h"
#include "common.h"
#include "reader.h"
#include "env.h"
#include "sanity.h"
#include "signal.h"
#include "halloc.h"
#include "halloc_util.h"
#include "path.h"

/*
  The history is implemented using a linked list. Searches are done
  using linear search. 
*/


/**
   A struct describing the state of the interactive history
   list. Multiple states can be created. Use \c history_set_mode() to
   change between history contexts.
*/
typedef struct
{
	/**
	   Number of entries
	*/
	int count;

	/**
	   Last history item
	*/
	ll_node_t *last;

	/**
	   The last item loaded from file
	*/
	ll_node_t *last_loaded;

	/**
	   Set to one if the file containing the saved history has been loaded
	*/
	int is_loaded;
}
	history_data_t;


static ll_node_t /** Last item in history */ *history_last=0, /** Current search position*/ *history_current =0;

/**
   We only try to save the part of the history that was written in
   this session. This pointer keeps track of where the cutoff is.

   The reason for wanting to do this is so that when two concurrent
   fish sessions have exited, the latter one to exit won't push the
   added history items of the prior session to the top of the history.
*/
static ll_node_t *last_loaded=0;

/**
  past_end is a kludge. It is set when the current history item is
  unset, i.e. a new entry is being typed.
*/
static int past_end =1;

/**
   Items in history
*/
static int history_count=0;

/**
   The name of the current history list. The name is used to switch
   between history lists for different commands as sell as for
   deciding the name of the file to save the history in.
*/
static wchar_t *mode_name;

/**
   Hash table for storing all the history lists.
*/
static hash_table_t history_table;

/**
   Flag, set to 1 once the history file has been loaded
*/
static int is_loaded=0;

static ll_node_t *unused = 0;

/**
   Create a new ll_node_t struct. The struct is allocated using
   halloc, and will be automatically free'd on shutdown, but not
   before.
*/
static ll_node_t *history_create_node()
{
	ll_node_t *res;
	
	if( !unused )
	{
		return halloc( global_context, sizeof( ll_node_t ) );
	}

	res = unused;
	unused = unused->next;
	return res;
	
}

/**
   Return a ll_node_t struct to the pool of unused such structs, so
   that it can later be reused by a call to history_create_node().
*/
static void history_free_node( ll_node_t *n )
{
	n->next = unused;
	unused = n;
}

/**
   Add backslashes to all newlines, so that the returning string is
   suitable for writing to the history file. The memory for the return
   value will be reused by subsequent calls to this function.
*/
static wchar_t *history_escape_newlines( wchar_t *in )
{
	static string_buffer_t *out = 0;
	if( !out )
	{
		out = sb_halloc( global_context );
		if( !out )
		{
			DIE_MEM();
		}
	}
	else
	{
		sb_clear( out );
	}
	for( ; *in; in++ )
	{
		if( *in == L'\\' )
		{
			sb_append_char( out, *in );
			if( *(in+1) )
			{
				in++;
				sb_append_char( out, *in );
			}
			else
			{
				/*
				  This is a weird special case. When we are trying to
				  save a string that ends with a backslash, we need to
				  handle it specially, otherwise this command would be
				  combined with the one following it. We hack around
				  this by adding an additional newline.
				*/
				sb_append_char( out, L'\n' );
			}
			
		}
		else if( *in == L'\n' )
		{
			sb_append_char( out, L'\\' );
			sb_append_char( out, *in );
		}
		else
		{
			sb_append_char( out, *in );
		}
		
	}
	return (wchar_t *)out->buff;
}

/**
   Remove backslashes from all newlines. This makes a string from the
   history file better formated for on screen display. The memory for
   the return value will be reused by subsequent calls to this
   function.
*/
static wchar_t *history_unescape_newlines( wchar_t *in )
{
	static string_buffer_t *out = 0;
	if( !out )
	{
		out = sb_halloc( global_context );
		if( !out )
		{
			DIE_MEM();
		}
	}
	else
	{
		sb_clear( out );
	}
	for( ; *in; in++ )
	{
		if( *in == L'\\' )
		{
			if( *(in+1)!= L'\n')
			{
				sb_append_char( out, *in );
			}
		}
		else
		{
			sb_append_char( out, *in );
		}
		
	}
	return (wchar_t *)out->buff;
}

static wchar_t *history_file_name( void *context )
{
	wchar_t *path = path_get_config( context );
	wchar_t *res;
	
	if( !path )
		return 0;
	
	res = wcsdupcat2( path, L"/", mode_name, L"_history", (void *)0 );
	halloc_register_function( context, &free, res );
	return res;
}

/**
   Read a complete histor entry from the specified FILE.
*/
static wchar_t *history_load_entry( FILE *in )
{
	int was_backslash = 0;	
	static string_buffer_t *out = 0;
	int first_char = 1;	
	int ignore = 0;
	
	if( !out )
	{
		out = sb_halloc( global_context );
		if( !out )
		{
			DIE_MEM();
		}
	}
	else
	{
		sb_clear( out );
	}
	
	while( 1 )
	{
		wint_t c;
		
		c = getwc( in );
	
	
		if( errno == EILSEQ )
		{
			getc( in );
			continue;
		}

		if( c == WEOF )
			break;


		if( c == L'\n' )
		{
			if( ignore )
			{
				ignore = 0;
				continue;
			}
			if( !was_backslash )
				break;
		}
		
		if( first_char )
		{
			if( c == L'#' ) 
				ignore = 1;
		}
		
		first_char = 0;
		
		if( !ignore )
			sb_append_char( out, c );
	
		was_backslash = ( (c == L'\\') && !was_backslash);
				
	}

	return (wchar_t *)out->buff;
}


/**
   Load history from file
*/
static int history_load()
{
	wchar_t *fn;
	wchar_t *buff=0;
	FILE *in_stream;
	hash_table_t used;
	int res = 0;
	void *context;
	
	if( !mode_name )
	{
		return -1;
	}	

	context = halloc( 0, 0 );

	is_loaded = 1;
		
	signal_block();
	hash_init2( &used, 
				&hash_wcs_func,
				&hash_wcs_cmp,
				4096 );

	fn = history_file_name( context );
	if( fn )
	{
		
	in_stream = wfopen( fn, "r" );
	
	if( in_stream != 0 )
	{
		while( !feof( in_stream ) )
		{
			buff = history_unescape_newlines(history_load_entry( in_stream ));
			/*
			  We do not call history_add here, since that would make
			  history_load() take quadratic time, and may be
			  unacceptably on slow systems with a long history file.

			  Use a hashtable to check for duplicates instead.
			*/
			if( wcslen(buff) && !hash_get( &used,
										   buff ) )
			{
				history_count++;		
				
				history_current = history_create_node();
				
				history_current->data=halloc_wcsdup( global_context, buff );

				hash_put( &used,
						  history_current->data, 
						  L"" );
				
				history_current->next=0;
				history_current->prev = history_last;
				if( history_last != 0 )
				{
					history_last->next = history_current;
				}
				history_last = history_current;
			}
		}
		
		fclose( in_stream );
	}
	else
	{
		if( errno != ENOENT )
		{
			debug( 1, _(L"The following non-fatal error occurred while reading command history from \'%ls\':"), mode_name );
			wperror( L"fopen" );
			res = -1;
			
		}
	}
	

	last_loaded = history_last;
	}
	
	hash_destroy( &used );
	signal_unblock();
	halloc_free( context );
	return res;
	
}

void history_init()
{
	hash_init( &history_table,
			   &hash_wcs_func,
			   &hash_wcs_cmp );
}

/**
   Make sure the current history list is in the history_table.
*/
static void history_to_hash()
{
	history_data_t *d;
	
	if( !mode_name )
		return;
	

	d = (history_data_t *)hash_get( &history_table, 
								  mode_name );
	
	if( !d )
		d = malloc( sizeof(history_data_t));
	d->last=history_last;
	d->last_loaded=last_loaded;
	d->count=history_count;
	d->is_loaded = is_loaded;
	
	hash_put( &history_table, 
			  mode_name,
			  d );	
	
}


void history_set_mode( wchar_t *name )
{
	history_data_t *curr;
	
	if( mode_name )
	{		
		/*
		  Move the current history to the hashtable
		*/
		history_to_hash();		
	}
	
	/*
	  See if the new history already exists
	*/
	curr = (history_data_t *)hash_get( &history_table,
									 name );
	if( curr )
	{
		/*
		  Yes. Restore it.
		*/
		mode_name = (wchar_t *)hash_get_key( &history_table,
											 name );
		history_current = history_last = curr->last;
		last_loaded = curr->last_loaded;
		history_count = curr->count;
		is_loaded = curr->is_loaded;
	}
	else
	{
		/*
		  Nope. Create a new history list.
		*/
		history_count=0;
		history_last = history_current = last_loaded=0;
		mode_name = wcsdup( name );
		is_loaded = 0;
	}
	
	past_end=1;
	
}

/**
   Print history node 
*/
static void history_save_node( ll_node_t *n, FILE *out )
{
	wchar_t *escaped;
	if( n==0 )
		return;
	history_save_node( n->prev, out );

	escaped = history_escape_newlines( (wchar_t *)(n->data) );
	fwprintf(out, L"#\n%ls\n", escaped );
}

/**
   Merge the process history with the history on file, write to
   disc. The merging operation is done so that two concurrently
   running shells wont erase each others history.
*/
static void history_save()
{
	wchar_t *fn;
	FILE *out_stream;
	/* First we save this sessions history in local variables */
	ll_node_t *real_pos = history_last, *real_first = last_loaded;

	if( !is_loaded )
		return;
	
	if( !real_first )
	{
		real_first = history_current;
		while( real_first->prev )
			real_first = real_first->prev;
	}

	if( real_pos == real_first )
		return;
	
	/* Then we load the history from file into the global pointers */
	history_last=history_current=last_loaded=0;
	history_count=0;
	past_end=1;

	if( !history_load() )
	{
		
		if( real_pos != 0 )
		{
			/* 
			   Rewind the session history to the first item which was
			   added in this session
			*/
			while( (real_pos->prev != 0) && (real_pos->prev != real_first) )
			{
				real_pos = real_pos->prev;
			}
			
			/* Free old history entries */
			ll_node_t *kill_node_t = real_pos->prev;
			while( kill_node_t != 0 )
			{
				ll_node_t *tmp = kill_node_t;
				kill_node_t = kill_node_t->prev;
				history_free_node( tmp );		
			}	
			
			/* 
			   Add all the history entries from this session to the global
			   history, free the old version
			*/
			while( real_pos != 0 )
			{
				ll_node_t *next = real_pos->next;
				history_add( (wchar_t *)real_pos->data );
				
				history_free_node( real_pos );
				real_pos = next;
			}
		}
		
		/* Save the global history */
		{
			void *context = halloc( 0, 0 );
			fn = history_file_name( context );
			if( fn )
			{
				out_stream = wfopen( fn, "w" );
				
				if( out_stream )
				{
					history_save_node( history_last, out_stream );
					if( fclose( out_stream ) )
					{
						debug( 1, L"The following non-fatal error occurred while saving command history to \'%ls\':", fn );
						wperror( L"fopen" );
					}			
				}
				else
				{
					debug( 1, L"The following non-fatal error occurred while saving command history to \'%ls\':", fn );
					wperror( L"fopen" );
				}
			}
			
			halloc_free( context );
		}
	}
	
	
}

/**
   Save the specified mode to file
*/

static void history_destroy_mode( void *name, void *link )
{
	mode_name = (wchar_t *)name;
	history_data_t *d = (history_data_t *)link;
	history_last = history_current = d->last;
	last_loaded = d->last_loaded;
	history_count = d->count;
	past_end=1;
	
//	fwprintf( stderr, L"Destroy history mode \'%ls\'\n", mode_name );
	
	if( history_last )
	{
		history_save();	
		history_current = history_last;
		while( history_current != 0 )
		{
			ll_node_t *tmp = history_current;
			history_current = history_current->prev;
			history_free_node( tmp );		
		}	
	}
	free( d );
	free( mode_name );
}

void history_destroy()
{
	
	/**
	   Make sure current mode is in table
	*/
	history_to_hash();		
		
	/**
	   Save all modes in table
	*/
	hash_foreach( &history_table, 
				  &history_destroy_mode );

	hash_destroy( &history_table );
}



/**
   Internal search function
*/
static ll_node_t *history_find( ll_node_t *n, const wchar_t *s )
{
	if( !is_loaded )
	{
		if( history_load() )
		{
			return 0;
		}
	}
	

	if( n == 0 )
		return 0;
	
	if( wcscmp( s, (wchar_t *)n->data ) == 0 )
	{
		return n;		
	}
	else
		return history_find( n->prev, s );
}


void history_add( const wchar_t *str )
{
	ll_node_t *old_node;

	if( !is_loaded )
	{
		if( history_load() )
		{
			return;
		}
	}
	

	if( wcslen( str ) == 0 )
		return;
	
	past_end=1;

	old_node = history_find( history_last, str );

	if( old_node == 0 )
	{
		history_count++;		
		history_current = history_create_node();
		history_current->data=halloc_wcsdup( global_context, str );
	}
	else
	{
		if( old_node == last_loaded )
		{
			last_loaded = last_loaded->prev;
		}
	
		history_current = old_node;
		if( old_node == history_last )
		{
			return;
		}

		if( old_node->prev != 0 )
			old_node->prev->next = old_node->next;
		if( old_node->next != 0 )
			old_node->next->prev = old_node->prev;
	}
	history_current->next=0;
	history_current->prev = history_last;
	if( history_last != 0 )
	{
		history_last->next = history_current;
	}
	history_last = history_current;
}

/**
   This function tests if the search string is a match for the given string
*/
static int history_test( const wchar_t *needle, const wchar_t *haystack )
{

/*
	return wcsncmp( haystack, needle, wcslen(needle) )==0;
*/
	return !!wcsstr( haystack, needle );
}

const wchar_t *history_prev_match( const wchar_t *str )
{
	if( !is_loaded )
	{
		if( history_load() )
		{
			return 0;
		}
	}
	

	if( history_current == 0 )
		return str;
	
	if( history_current->prev == 0 )
	{
		return str;
	}
	if( past_end )
		past_end = 0;
	else
		history_current = history_current->prev;

	if( history_test( str, history_current->data) )
		return (wchar_t *)history_current->data;
	else
		return history_prev_match( str );
}


const wchar_t *history_next_match( const wchar_t *str)
{
	if( !is_loaded )
	{
		if( history_load() )
		{
			return 0;
		}
	}
	

	if( history_current == 0 )
		return str;

	if( history_current->next == 0 )
	{
		past_end = 1;
		return str;
	}
	history_current = history_current->next;
	
	if( history_test( str, history_current->data ) )
		return (wchar_t *)history_current->data;
	else
		return history_next_match( str );	
}

void history_reset()
{
	history_current = history_last;
}

/**
   Move to first history item
*/
void history_first()
{
	if( is_loaded )
		while( history_current->prev )
			history_current = history_current->prev;
	
}

wchar_t *history_get( int idx )
{
	ll_node_t *n;
	int i;
	
	if( !is_loaded )
	{
		if( history_load() )
		{
			return 0;
		}
	}
	
	n = history_last;
	
	if( idx<0)
	{
		debug( 1, L"Tried to access negative history index %d", idx );
		return 0;
	}
	
	for( i=0; i<idx; i++ )
	{
		if( !n )
			break;
		n=n->prev;
	}
	return n?n->data:0;
}


void history_sanity_check()
{	
	return;
	
	if( history_current != 0 )
	{
		int i;
		int history_ok = 1;
		int found_current = 0;
		
		validate_pointer( history_last, L"History root", 1);
		
		ll_node_t *tmp = history_last;
		for( i=0; i<history_count; i++ )
		{
			found_current |= tmp == history_current;

			if( tmp == 0 )
			{
				history_ok = 0;
				debug( 1, L"History items missing" );
				break;
			}

			if( (tmp->prev != 0) && (tmp->prev->next != tmp ) )
			{
				history_ok = 0;
				debug( 1, L"History items order is inconsistent" );
				break;
			}
			
			validate_pointer( tmp->data, L"History data", 1);
			if( tmp->data == 0 )
			{
				history_ok = 0;
				debug( 1, L"Empty history item" );
				break;
			}
			validate_pointer( tmp->prev, L"History node", 1);
			tmp = tmp->prev;
		}
		if( tmp != 0 )
		{
			history_ok = 0;
			debug( 1, L"History list too long" );
		}

		if( (i!= history_count )|| (!found_current))
		{
			debug( 1, L"No history item selected" );
			history_ok=0;
		}
		
		if( !history_ok )
		{
			sanity_lose();			
		}
	}	
}

