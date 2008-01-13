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
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "history.h"
#include "common.h"
#include "halloc.h"
#include "halloc_util.h"
#include "intern.h"
#include "path.h"
#include "signal.h"


/**
   Interval in seconds between automatic history save
*/
#define SAVE_INTERVAL (5*60)

/**
   Number of new history entries to add before automatic history save
*/
#define SAVE_COUNT 5

/**
   A struct representiong a history list
*/
typedef struct 
{
	/**
	   The name of this list. Used for picking a suitable filename and for switching modes.
	*/
	const wchar_t *name;
	
	/**
	   The items of the list. Each entry may be either a pointer to an
	   item_t struct or a pointer to an mmaped memory region where a
	   multibyte string representing the item is stored. Use the
	   item_get function to always get an item_t.
	*/
	array_list_t item;


	/**
	   A hash table containing all the items created by the current
	   session as keys. This can be used as a lookup when loading the
	   history list to ignore the on-file version of an entry from
	   this session.
	*/
	hash_table_t session_item;
	
	/**
	   The current history position
	*/
	int pos;
	
	/**
	   This flag is set nonzero if the file containing earlier saves has ben mmaped in
	*/
	int has_loaded;

	/**
	   The mmaped region for the history file
	*/
	char *mmap_start;

	/**
	   The size of the mmaped region
	*/
	size_t mmap_length;

	/**
	   A list of indices of all previous search maches. This is used to eliminate duplicate search results.
	*/
	array_list_t used;
	
	/**
	   Timestamp of last save
	*/
	time_t save_timestamp;

	/**
	   Number of entries that have been added since last save
	*/
	int new_count;

	/**
	   A halloc context. This context is free'd every time the history
	   is saved. Therefore it is very well suited for use as the
	   context for history item data.
	*/
	void *item_context;
}
	history_mode_t;

/**
   This struct represents a history item
*/
typedef struct 
{
	/**
	   The actual contents of the entry
	*/
	wchar_t *data;
	
	/**
	   Original creation time for the entry
	*/
	time_t timestamp;
}
	item_t;

/**
   Table of all history modes
*/
static hash_table_t *mode_table=0;

/**
   The surrent history mode
*/
static history_mode_t *current_mode=0;

/**
   Hash function for item_t struct
*/
static int hash_item_func( void *v )
{
	item_t *i = (item_t *)v;
	return i->timestamp ^ hash_wcs_func( i->data );
}

/**
   Comparison function for item_t struct
*/
static int hash_item_cmp( void *v1, void *v2 )
{
	item_t *i1 = (item_t *)v1;
	item_t *i2 = (item_t *)v2;
	return (i1->timestamp == i2->timestamp) && (wcscmp( i1->data, i2->data )==0);	
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

/**
   Check if the specified item is already loaded
 */
static int item_is_new( history_mode_t *m, void *d )
{	
	char *begin = (char *)d;

	if( !m->has_loaded || !m->mmap_start || (m->mmap_start == MAP_FAILED ) )
	{
		return 1;		
	}
	
	if( (begin < m->mmap_start) || (begin > (m->mmap_start+m->mmap_length) ) )
	{
		return 1;
	}
	return 0;
}

/**
   Returns an item_t for the specified adress. The adress must come from the item list of the specified mode.

   Later calls to this function may erase the output of a previous call to this function.
*/
static item_t *item_get( history_mode_t *m, void *d )
{	
	char *begin = (char *)d;

	if( item_is_new( m, d ) )
	{
		return (item_t *)d;
	}
	else
	{
		
		char *end = m->mmap_start + m->mmap_length;
		char *pos=begin;
		
		int was_backslash = 0;	
		static string_buffer_t *out = 0;
		static item_t narrow_item;
		int first_char = 1;	
		int timestamp_mode = 0;

		narrow_item.timestamp = 0;
							
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
			wchar_t c;
			mbstate_t state;
			size_t res;
			
			memset( &state, 0, sizeof(state) );
			
			res = mbrtowc( &c, pos, end-pos, &state );
			
			if( res == (size_t)-1 )
			{
				pos++;
				continue;
			}
			else if( res == (size_t)-2 )
			{
				break;
			}
			else if( res == (size_t)0 )
			{
				pos++;
				continue;
			}
			pos += res;
			
			if( c == L'\n' )
			{
				if( timestamp_mode )
				{
					wchar_t *time_string = (wchar_t *)out->buff;
					while( *time_string && !iswdigit(*time_string))
						time_string++;
					errno=0;
					
					if( *time_string )
					{
						time_t tm;
						wchar_t *end;
						
						errno = 0;
						tm = (time_t)wcstol( time_string, &end, 10 );
				
						if( tm && !errno && !*end )
						{
							narrow_item.timestamp = tm;
						}

					}					

					sb_clear( out );
					timestamp_mode = 0;
					continue;
				}
				if( !was_backslash )
					break;
			}
			
			if( first_char )
			{
				if( c == L'#' ) 
					timestamp_mode = 1;
			}
			
			first_char = 0;
			
			sb_append_char( out, c );
			
			was_backslash = ( (c == L'\\') && !was_backslash);
			
		}
		
		narrow_item.data = history_unescape_newlines((wchar_t *)out->buff);
		return &narrow_item;
	}

}

/**
   Write the specified item to the specified file.
*/
static int item_write( FILE *f, history_mode_t *m, void *v )
{
	item_t *i = item_get( m, v );
	return fwprintf( f, L"# %d\n%ls\n", i->timestamp, history_escape_newlines( i->data ) );
}

/**
   Release all memory used by the specified history mode.
*/
static void history_destroy_mode( history_mode_t *m )
{
	halloc_free( m->item_context );
	
	if( m->mmap_start && (m->mmap_start != MAP_FAILED ))
	{
		munmap( m->mmap_start, m->mmap_length);
	}	
	
}

/**
   Free all memory used by specified mistory mode
 */
static void history_destroy_mode_wrapper( void *n, history_mode_t *m )
{
	halloc_free( m );
}

/**
   Create a new empty mode with the specified name. 
   The mode is a halloc context, and the entire mode can be destroyed using halloc_free().
*/
static history_mode_t *history_create_mode( const wchar_t *name )
{	
	history_mode_t *new_mode = halloc( 0, sizeof( history_mode_t ));

	new_mode->name = intern(name);

	al_init( &new_mode->item );
	al_init( &new_mode->used );
	halloc_register_function( new_mode, (void (*)(void *))&al_destroy, &new_mode->item );
	halloc_register_function( new_mode, (void (*)(void *))&al_destroy, &new_mode->used );
	
	hash_init( &new_mode->session_item, &hash_item_func, &hash_item_cmp );
	halloc_register_function( new_mode, (void (*)(void *))&hash_destroy, &new_mode->session_item );
	
	new_mode->save_timestamp=time(0);
	new_mode->item_context = halloc( 0,0 );

	halloc_register_function( new_mode, (void (*)(void *))&history_destroy_mode, new_mode );

	return new_mode;	
}

/**
   This function tests if the search string is a match for the given string
*/
static int history_test( const wchar_t *needle, const wchar_t *haystack )
{
	return !!wcsstr( haystack, needle );
}

/**
   Returns the name of the save file for a mode.

   \param context a halloc context used to allocate memory
   \param name the name of the hstory mode
   \param suffix an optional file suffix
*/
static wchar_t *history_filename( void *context, const wchar_t *name, const wchar_t *suffix )
{
	wchar_t *path;
	wchar_t *res;	

	if( !current_mode )
		return 0;
	
	path = path_get_config( context );

	if( !path )
		return 0;
	
	res = wcsdupcat( path, L"/", name, L"_history", suffix?suffix:(void *)0);
	halloc_register_function( context, &free, res );
	return res;
}

/**
   Go through the mmaped region and insert pointers to suitable loacations into the item list
*/
static void history_populate_from_mmap( history_mode_t *m )
{	
	char *begin = m->mmap_start;
	char *end = begin + m->mmap_length;
	char *pos;
	
	array_list_t old_item;
	array_list_t session_item_list;
	int ignore_newline = 0;
	int do_push = 1;
	
	al_init( &old_item );
	al_init( &session_item_list );
	al_push_all( &old_item, &m->item );
	al_truncate( &m->item, 0 );
	
	for( pos = begin; pos <end; pos++ )
	{
		
		if( do_push )
		{
			item_t *i;
			item_t *i_orig;

			ignore_newline = *pos == '#';

			i = item_get( m, pos );
			
			if( (i_orig=hash_get( &current_mode->session_item, i ) ) )
			{
				/*
				  This item comes from this session. Insert the
				  original item at the end of the item list.
				*/
				al_push( &session_item_list, i_orig );				
			}
			else
			{
				/*
				  Old item. Insert pointer directly to the item list
				*/
				al_push( &m->item, pos );
			}
			
			do_push = 0;
		}
		
		switch( *pos )
		{
			case '\\':
			{
				pos++;
				break;
			}
			
			case '\n':
			{
				if( ignore_newline )
				{
					ignore_newline = 0;
				}
				else
				{
					do_push = 1;
				}				
				break;
			}
		}
	}	
	
	al_push_all(  &m->item, &session_item_list );
	m->pos += al_get_count( &m->item );
	al_push_all(  &m->item, &old_item );


	al_destroy( &session_item_list );
	al_destroy( &old_item );
}

/**
   Load contents of the backing file to memory
*/
static void history_load( history_mode_t *m )
{
	int fd;
	int ok=0;
	
	void *context;
	wchar_t *filename;

	if( !m )
		return;	
	
	m->has_loaded=1;

	signal_block();

	context = halloc( 0, 0 );
	filename = history_filename( context, m->name, 0 );
	
	if( filename )
	{
		if( ( fd = wopen( filename, O_RDONLY ) ) > 0 )
		{
			off_t len = lseek( fd, 0, SEEK_END );
			if( len != (off_t)-1)
			{
				m->mmap_length = (size_t)len;
				if( lseek( fd, 0, SEEK_SET ) == 0 )
				{
					if( (m->mmap_start = mmap( 0, m->mmap_length, PROT_READ, MAP_PRIVATE, fd, 0 )) != MAP_FAILED )
					{
						ok = 1;
						history_populate_from_mmap( m );
					}
				}
			}
			close( fd );
		}
	}
	
	halloc_free( context );
	signal_unblock();
}

/**
   Save the specified mode to file
*/
static void history_save_mode( void *n, history_mode_t *m )
{
	FILE *out;
	history_mode_t *on_disk;
	int i;
	int has_new=0;
	wchar_t *tmp_name;

	int ok = 1;

	/*
	  First check if there are any new entries to save. If not, then
	  we can just return
	*/
	for( i=0; i<al_get_count(&m->item); i++ )
	{
		void *ptr = al_get( &m->item, i );
		has_new = item_is_new( m, ptr );
		if( has_new )
		{
			break;
		}
	}
	
	if( !has_new )
	{
		return;
	}
	
	signal_block();

	/*
	  Set up on_disk variable to describe the current contents of the
	  history file
	*/
	on_disk = history_create_mode( m->name );
	history_load( on_disk );
	
	tmp_name = history_filename( on_disk, m->name, L".tmp" );

	if( tmp_name )
	{
		tmp_name = wcsdup(tmp_name );
		
		if( (out=wfopen( tmp_name, "w" ) ) )
		{
			hash_table_t mine;
			
			hash_init( &mine, &hash_item_func, &hash_item_cmp );
			
			for( i=0; i<al_get_count(&m->item); i++ )
			{
				void *ptr = al_get( &m->item, i );
				int is_new = item_is_new( m, ptr );
				if( is_new )
				{
					hash_put( &mine, item_get( m, ptr ), L"" );
				}
			}
			
			/*
			  Re-save the old history
			*/
			for( i=0; ok && (i<al_get_count(&on_disk->item)); i++ )
			{
				void *ptr = al_get( &on_disk->item, i );
				item_t *i = item_get( on_disk, ptr );
				if( !hash_get( &mine, i ) )
				{
					if( item_write( out, on_disk, ptr ) == -1 )
					{
						ok = 0;
						break;
					}
				}
				
			}
			
			hash_destroy( &mine );
		
			/*
			  Add our own items last
			*/		
			for( i=0; ok && (i<al_get_count(&m->item)); i++ )
			{
				void *ptr = al_get( &m->item, i );
				int is_new = item_is_new( m, ptr );
				if( is_new )
				{
					if( item_write( out, m, ptr ) == -1 )
					{						
						ok = 0;
					}
				}
			}
			
			if( fclose( out ) || !ok )
			{
				/*
				  This message does not have high enough priority to
				  be shown by default.
				*/
				debug( 2, L"Error when writing history file" );
			}
			else
			{
				wrename( tmp_name, history_filename( on_disk, m->name, 0 ) );
			}
		}
		free( tmp_name );
	}	
	
	halloc_free( on_disk);

	if( ok )
	{

		/*
		  Reset the history. The item_t entries created in this session
		  are not lost or dropped, they are stored in the session_item
		  hash table. On reload, they will be automatically inserted at
		  the end of the history list.
		*/
			
		if( m->mmap_start && (m->mmap_start != MAP_FAILED ) )
		{
			munmap( m->mmap_start, m->mmap_length );
		}
		
		al_truncate( &m->item, 0 );
		al_truncate( &m->used, 0 );
		m->pos = 0;
		m->has_loaded = 0;
		m->mmap_start=0;
		m->mmap_length=0;
	
		m->save_timestamp=time(0);
		m->new_count = 0;
	}
	
	signal_unblock();
}


void history_add( const wchar_t *str )
{
	item_t *i;
	
	if( !current_mode )
		return;
	
	i = halloc( current_mode->item_context, sizeof(item_t));
	i->data = (wchar_t *)halloc_wcsdup( current_mode->item_context, str );
	i->timestamp = time(0);
	
	al_push( &current_mode->item, i );
	hash_put( &current_mode->session_item, i, i );
	
	al_truncate( &current_mode->used, 0 );
	current_mode->pos = al_get_count( &current_mode->item );

	current_mode->new_count++;
	
	if( (time(0) > current_mode->save_timestamp+SAVE_INTERVAL) || (current_mode->new_count >= SAVE_COUNT) )
	{
		history_save_mode( 0, current_mode );
	}
	
}

/**
   Check if the specified string has already been used a s a search match
*/
static int history_is_used( const wchar_t  *str )
{
	int i;
	int res =0;
	item_t *it = 0;

	for( i=0; i<al_get_count( &current_mode->used); i++ )
	{
		long idx = al_get_long( &current_mode->used, i );
		it = item_get( current_mode, al_get( &current_mode->item, (int)idx ) );
		if( wcscmp( it->data, str ) == 0 )
		{
			res = 1;
			break;			
		}
		
	}
	return res;
}

const wchar_t *history_prev_match( const wchar_t *needle )
{
	if( current_mode )
	{
		if( current_mode->pos > 0 )
		{
			for( current_mode->pos--; current_mode->pos>=0; current_mode->pos-- )
			{
				item_t *i = item_get( current_mode, al_get( &current_mode->item, current_mode->pos ) );
				wchar_t *haystack = (wchar_t *)i->data;
				
				if( history_test( needle, haystack ) )
				{
					int is_used;
					
					/*
					  This is ugly.  Whenever we call item_get(),
					  there is a chance that the return value of any
					  previous call to item_get will become
					  invalid. The history_is_used function uses the
					  item_get() function. Therefore, we must create
					  a copy of the haystack string, and if the string
					  is unused, we must call item_get anew.
					*/

					haystack = wcsdup(haystack );
					
					is_used = history_is_used( haystack );
					
					free( haystack );
					
					if( !is_used )
					{
						i = item_get( current_mode, al_get( &current_mode->item, current_mode->pos ) );
						al_push_long( &current_mode->used, current_mode->pos );
						return i->data;
					}
				}
			}
		}
		
		if( !current_mode->has_loaded )
		{
			/*
			  We found no match in the list, try loading the history
			  file and continue the search
			*/
			history_load( current_mode );
			return history_prev_match( needle );
		}
		else
		{
			/*
			  We found no match in the list, and the file is already
			  loaded. Set poition before first element and return
			  original search string.
			*/
			current_mode->pos=-1;
			if( al_peek_long( &current_mode->used ) != -1 )
				al_push_long( &current_mode->used, -1 );
		}
		
	}
	
	return needle;
}


wchar_t *history_get( int idx )
{
	int len;

	if( !current_mode )
		return 0;
	
	len = al_get_count( &current_mode->item );

	if( (idx >= len ) && !current_mode->has_loaded )
	{
		history_load( current_mode );
		len = al_get_count( &current_mode->item );
	}
	
	if( idx < 0 )
		return 0;
	
	if( idx >= len )
		return 0;
	
	return item_get( current_mode, al_get( &current_mode->item, len - 1 - idx ) )->data;
}

void history_first()
{
	if( current_mode )
	{
		if( !current_mode->has_loaded )
		{
			history_load( current_mode );
		}
		
		current_mode->pos = 0;
	}	
}

void history_reset()
{
	if( current_mode )
	{
		current_mode->pos = al_get_count( &current_mode->item );
		/*
		  Clear list of search matches
		*/
		al_truncate( &current_mode->used, 0 );
	}	
}

const wchar_t *history_next_match( const wchar_t *needle)
{
	if( current_mode )
	{
		/*
		  The index of previous search matches are saved in the 'used'
		  list. We just need to pop the top item and set the new
		  position. Easy!
		*/
		if( al_get_count( &current_mode->used ) )
		{
			al_pop( &current_mode->used );
			if( al_get_count( &current_mode->used ) )
			{
				current_mode->pos = (int) al_peek_long( &current_mode->used );
				item_t *i = item_get( current_mode, al_get( &current_mode->item, current_mode->pos ) );
				return i->data;
			}
		}
		
		/*
		  The used-list is empty. Set position to 'past end of list'
		  and return the search string.
		*/
		current_mode->pos = al_get_count( &current_mode->item );

	}
	return needle;	
}


void history_set_mode( const wchar_t *name )
{
	if( !mode_table )
	{
		mode_table = malloc( sizeof(hash_table_t ));
		hash_init( mode_table, &hash_wcs_func, &hash_wcs_cmp );		
	}
	
	current_mode = (history_mode_t *)hash_get( mode_table, name );
	
	if( !current_mode )
	{
		current_mode = history_create_mode( name );
		hash_put( mode_table, name, current_mode );		
	}	
}

void history_init()
{
}


void history_destroy()
{
	if( mode_table )
	{
		hash_foreach( mode_table, (void (*)(void *, void *))&history_save_mode );
		hash_foreach( mode_table, (void (*)(void *, void *))&history_destroy_mode_wrapper );
		hash_destroy( mode_table );
		free( mode_table );
		mode_table=0;
	}
}


void history_sanity_check()
{
	/*
	  No sanity checking implemented yet...
	*/
}

