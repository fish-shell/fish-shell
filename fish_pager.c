#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include <sys/time.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>

#include <locale.h>

#if HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif

#if HAVE_TERMIO_H
#include <termio.h>
#endif

#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif

#include <signal.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <errno.h>

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "common.h"
#include "complete.h"
#include "output.h"
#include "input_common.h"
#include "env_universal.h"
#include "halloc.h"
#include "halloc_util.h"
#include "print_help.h"

enum 
{
	LINE_UP = R_NULL+1,
	LINE_DOWN,
	PAGE_UP,
	PAGE_DOWN
}
	;


enum
{
	HIGHLIGHT_PAGER_PREFIX,
	HIGHLIGHT_PAGER_COMPLETION,
	HIGHLIGHT_PAGER_DESCRIPTION,
	HIGHLIGHT_PAGER_PROGRESS
}
	;

enum
{
	/*
	  Returnd by the pager if no more displaying is needed
	*/
	PAGER_DONE,
	/*
	  Returned by the pager if the completions would not fit in the specified number of columns
	*/
	PAGER_RETRY,
	/*
	  Returned by the pager if the terminal changes size
	*/
	PAGER_RESIZE
}
	;

/**
   The minimum width (in characters) the terminal may have for fish_pager to not refuse showing the completions
*/
#define PAGER_MIN_WIDTH 16

/**
   The maximum number of columns of completion to attempt to fit onto the screen
*/
#define PAGER_MAX_COLS 6

/**
   The string describing the single-character options accepted by fish_pager
*/
#define GETOPT_STRING "c:hr:qvp:"

/**
   Error to use when given an invalid file descriptor for reading completions or writing output
*/
#define ERR_NOT_FD _( L"%ls: Argument '%s' is not a valid file descriptor\n" )

/**
   This struct should be continually updated by signals as the term
   resizes, and as such always contain the correct current size.
*/
static struct winsize termsize;

/**
   The termios modes the terminal had when the program started. These
   should be restored on exit
*/
static struct termios saved_modes;

/**
   This flag is set to 1 of we have sent the enter_ca_mode terminfo
   sequence to save the previous terminal contents.
*/
static int is_ca_mode = 0;

/**
   This buffer_t is used to buffer the output of the pager to improve
   screen redraw performance bu cutting down the number of write()
   calls to only one.
*/
static buffer_t *pager_buffer;

/**
   The environment variables used to specify the color of different
   tokens.
*/
static wchar_t *hightlight_var[] = 
{
	L"fish_pager_color_prefix",
	L"fish_pager_color_completion",
	L"fish_pager_color_description",
	L"fish_pager_color_progress"
}
	;

/**
   This string_buffer_t contains the text that should be sent back to the calling program
*/
static string_buffer_t out_buff;
/**
   This is the file to which the output text should be sent. It is really a pipe.
*/
static FILE *out_file;

/**
   Data structure describing one or a group of related completions
*/
typedef struct 
{
	/**
	   The list of all completin strings this entry applies to
	*/
	array_list_t *comp;
	/**
	   The description
	*/
	wchar_t *desc;
	/**
	   On-screen width of the completion string
	*/
	int comp_width;	
	/**
	   On-screen width of the description information
	*/
	int desc_width;	
	/**
	   Preffered total width
	*/
	int pref_width;
	/**
	   Minimum acceptable width
	*/
	int min_width;
}
	comp_t;

/**
   This function translates from a highlight code to a specific color
   by check invironement variables
*/
static int get_color( int highlight )
{
	wchar_t *val;

	if( highlight < 0 )
		return FISH_COLOR_NORMAL;
	if( highlight >= (4) )
		return FISH_COLOR_NORMAL;
	
	val = wgetenv( hightlight_var[highlight]);

	if( !val )
	{
		val = env_universal_get( hightlight_var[highlight]);
	}
	
	if( !val )
	{
		return FISH_COLOR_NORMAL;
	}
	
	return output_color_code( val );	
}

/**
   This function calculates the minimum width for each completion
   entry in the specified array_list. This width depends on the
   terminal size, so this function should be called when the terminal
   changes size.
*/
static void recalc_width( array_list_t *l, const wchar_t *prefix )
{
	int i;
	for( i=0; i<al_get_count( l ); i++ )
	{
		comp_t *c = (comp_t *)al_get( l, i );
		
		c->min_width = mini( c->desc_width, maxi(0,termsize.ws_col/3 - 2)) +
			mini( c->desc_width, maxi(0,termsize.ws_col/5 - 4)) +4;
	}
	
}

/**
   Test if the specified character sequence has been entered on the
   keyboard
*/
static int try_sequence( char *seq )
{
	int j, k;
	wint_t c=0;
	
	for( j=0; 
	     seq[j] != '\0' && seq[j] == (c=input_common_readch( j>0 )); 
	     j++ )
		;

	if( seq[j] == '\0' )
	{		
		return 1;
	}
	else
	{
		input_common_unreadch(c);
		for(k=j-1; k>=0; k--)
			input_common_unreadch(seq[k]);
	}
	return 0;
}

/**
   Read a character from keyboard
*/
static wint_t readch()
{
	struct mapping
	{
		char *seq;
		wint_t bnd;
	}
	;
	
	struct mapping m[]=
		{
			{				
				"\x1b[A", LINE_UP
			}
			,
			{
				key_up, LINE_UP
			}
			,
			{				
				"\x1b[B", LINE_DOWN
			}
			,
			{
				key_down, LINE_DOWN
			}
			,
			{
				key_ppage, PAGE_UP
			}
			,
			{
				key_npage, PAGE_DOWN
			}
			,
			{
				" ", PAGE_DOWN
			}
			,
			{
				"\t", PAGE_DOWN
			}
			,
			{
				0, 0
			}
			
		}
	;
	int i;
	
	for( i=0; m[i].bnd; i++ )
	{
		if( !m[i].seq )
		{
			continue;
		}
		
		if( try_sequence(m[i].seq ) )
			return m[i].bnd;
	}
	return input_common_readch(0);
}

/**
   Write specified character to the output buffer \c pager_buffer
*/
static int pager_buffered_writer( char c)
{
	b_append( pager_buffer, &c, 1 );
	return 0;
}

/**
   Flush \c pager_buffer to stdout
*/
static void pager_flush()
{
	write_loop( 1, pager_buffer->buff, pager_buffer->used );
	pager_buffer->used = 0;
}

/**
   Print the specified string, but use at most the specified amount of
   space. If the whole string can't be fitted, ellipsize it.

   \param str the string to print
   \param max the maximum space that may be used for printing
   \param has_more if this flag is true, this is not the entire string, and the string should be ellisiszed even if the string fits but takes up the whole space.
*/
static int print_max( const wchar_t *str, int max, int has_more )
{
	int i;
	int written = 0;
	for( i=0; str[i]; i++ )
	{
		
		if( written + wcwidth(str[i]) > max )
			break;
		if( ( written + wcwidth(str[i]) == max) && (has_more || str[i+1]) )
		{
			writech( ellipsis_char );
			written += wcwidth(ellipsis_char );
			break;
		}
			
		writech( str[i] );
		written+= wcwidth( str[i] );
	}
	return written;
}

/**
   Print the specified item using at the specified amount of space
*/
static void completion_print_item( const wchar_t *prefix, comp_t *c, int width )
{
	int comp_width=0, desc_width=0;
	int i;
	int written=0;
	
	if( c->pref_width <= width )
	{
		/*
		  The entry fits, we give it as much space as it wants
		*/
		comp_width = c->comp_width;
		desc_width = c->desc_width;
	}
	else
	{
		/*
		  The completion and description won't fit on the
		  allocated space. Give a maximum of 2/3 of the
		  space to the completion, and whatever is left to
		  the description.
		*/
		int desc_all = c->desc_width?c->desc_width+4:0;
		
		comp_width = maxi( mini( c->comp_width,
					 2*(width-4)/3 ),
				   width - desc_all );
		if( c->desc_width )
			desc_width = width-comp_width-4;
		else
			c->desc_width=0;
		
	}
	
	for( i=0; i<al_get_count( c->comp ); i++ )
	{
		const wchar_t *comp = (const wchar_t *)al_get( c->comp, i );
		if( i != 0 )
			written += print_max( L"  ", comp_width - written, 2 );
		set_color( get_color(HIGHLIGHT_PAGER_PREFIX),FISH_COLOR_NORMAL );
		written += print_max( prefix, comp_width - written, comp[0]?1:0 );
		set_color( get_color(HIGHLIGHT_PAGER_COMPLETION),FISH_COLOR_IGNORE );
		written += print_max( comp, comp_width - written, i!=(al_get_count(c->comp)-1) );
	}


	if( desc_width )
	{
		while( written < (width-desc_width-2))
		{
			written++;
			writech( L' ');
		}
		written += print_max( L"(", 1, 0 );
		set_color( get_color( HIGHLIGHT_PAGER_DESCRIPTION ),
			   FISH_COLOR_IGNORE );
		written += print_max( c->desc, desc_width, 0 );
		written += print_max( L")", 1, 0 );
	}
	else
	{
		while( written < width )
		{
			written++;
			writech( L' ');
		}
	}
	
}

/**
   Print the specified part of the completion list, using the
   specified column offsets and quoting style.

   \param l The list of completions to print
   \param cols number of columns to print in
   \param width An array specifying the width of each column
   \param row_start The first row to print
   \param row_stop the row after the last row to print
   \param prefix The string to print before each completion
   \param is_quoted Whether to print the completions are in a quoted environment
*/

static void completion_print( int cols,
			      int *width,
			      int row_start,
			      int row_stop,
			      wchar_t *prefix,
			      int is_quoted,
			      array_list_t *l)
{

	int rows = (al_get_count( l )-1)/cols+1;
	int i, j;

	for( i = row_start; i<row_stop; i++ )
	{
		for( j = 0; j < cols; j++ )
		{
			comp_t *el;

			int is_last = (j==(cols-1));
			
			if( al_get_count( l ) <= j*rows + i )
				continue;

			el = (comp_t *)al_get( l, j*rows + i );
			
			completion_print_item( prefix, el, width[j] - (is_last?0:2) );
			
			if( !is_last)
				writestr( L"  " );
		}
		writech( L'\n' );
	}
}


/**
   Try to print the list of completions l with the prefix prefix using
   cols as the number of columns. Return 1 if the completion list was
   printed, 0 if the terminal is to narrow for the specified number of
   columns. Always succeeds if cols is 1.

   If all the elements do not fit on the screen at once, make the list
   scrollable using the up, down and space keys to move. The list will
   exit when any other key is pressed.

   \param cols the number of columns to try to fit onto the screen
   \param prefix the character string to prefix each completion with
   \param is_quoted whether the completions should be quoted
   \param l the list of completions

   \return one of PAGER_RETRY, PAGER_DONE and PAGER_RESIZE
*/

static int completion_try_print( int cols,
				 wchar_t *prefix,
				 int is_quoted,
				 array_list_t *l )
{
	/*
	  The calculated preferred width of each column
	*/
	int pref_width[PAGER_MAX_COLS];
	/*
	  The calculated minimum width of each column
	*/
	int min_width[PAGER_MAX_COLS];
	/*
	  If the list can be printed with this width, width will contain the width of each column
	*/
	int *width=pref_width;
	/*
	  Set to one if the list should be printed at this width
	*/
	int print=0;
	
	int i, j;
	
	int rows = (al_get_count( l )-1)/cols+1;
	
	int pref_tot_width=0;
	int min_tot_width = 0;
	int res=PAGER_RETRY;
	/*
	  Skip completions on tiny terminals
	*/
	
	if( termsize.ws_col < PAGER_MIN_WIDTH )
		return PAGER_DONE;
	
	memset( pref_width, 0, sizeof(pref_width) );
	memset( min_width, 0, sizeof(min_width) );
	
	/* Calculate how wide the list would be */
	for( j = 0; j < cols; j++ )
	{
		for( i = 0; i<rows; i++ )
		{
			int pref,min;
			comp_t *c;
			if( al_get_count( l ) <= j*rows + i )
				continue;

			c = (comp_t *)al_get( l, j*rows + i );
			pref = c->pref_width;
			min = c->min_width;
			
			if( j != cols-1 )
			{
				pref += 2;
				min += 2;
			}
			min_width[j] = maxi( min_width[j],
					     min );
			pref_width[j] = maxi( pref_width[j],
					      pref );
		}
		min_tot_width += min_width[j];
		pref_tot_width += pref_width[j];
	}
	/*
	  Force fit if one column
	*/
	if( cols == 1)
	{
		if( pref_tot_width > termsize.ws_col )
		{
			pref_width[0] = termsize.ws_col;
		}
		width = pref_width;
		print=1;
	}
	else if( pref_tot_width <= termsize.ws_col )
	{
		/* Terminal is wide enough. Print the list! */
		width = pref_width;
		print=1;
	}
	else
	{
		int next_rows = (al_get_count( l )-1)/(cols-1)+1;
/*		fwprintf( stderr,
  L"cols %d, min_tot %d, term %d, rows=%d, nextrows %d, termrows %d, diff %d\n",
  cols,
  min_tot_width, termsize.ws_col,
  rows, next_rows, termsize.ws_row,
  pref_tot_width-termsize.ws_col );
*/
		if( min_tot_width < termsize.ws_col &&
		    ( ( (rows < termsize.ws_row) && (next_rows >= termsize.ws_row ) ) ||
		      ( pref_tot_width-termsize.ws_col< 4 && cols < 3 ) ) )
		{
			/*
			  Terminal almost wide enough, or squeezing makes the
			  whole list fit on-screen.

			  This part of the code is really important. People hate
			  having to scroll through the completion list. In cases
			  where there are a huge number of completions, it can't
			  be helped, but it is not uncommon for the completions to
			  _almost_ fit on one screen. In those cases, it is almost
			  always desirable to 'squeeze' the completions into a
			  single page. 

			  If we are using N columns and can get everything to
			  fit using squeezing, but everything would also fit
			  using N-1 columns, don't try.
			*/

			int tot_width = min_tot_width;
			width = min_width;

			while( tot_width < termsize.ws_col )
			{
				for( i=0; (i<cols) && ( tot_width < termsize.ws_col ); i++ )
				{
					if( width[i] < pref_width[i] )
					{
						width[i]++;
						tot_width++;
					}
				}
			}
			print=1;
		}
	}

	if( print )
	{
		res=PAGER_DONE;
		if( rows < termsize.ws_row )
		{
			/* List fits on screen. Print it and leave */
			if( is_ca_mode )
			{
				is_ca_mode = 0;
				writembs(exit_ca_mode);
			}
			
			completion_print( cols, width, 0, rows, prefix, is_quoted, l);
			pager_flush();
		}
		else
		{
			int npos, pos = 0;
			int do_loop = 1;

			/*
			  Enter ca_mode, which means that the terminal
			  content will be restored to the current
			  state on exit.
			*/
			if( enter_ca_mode && exit_ca_mode )
			{
				is_ca_mode=1;
				writembs(enter_ca_mode);
			}
			

			completion_print( cols,
					  width,
					  0,
					  termsize.ws_row-1,
					  prefix,
					  is_quoted,
					  l);
			/*
			  List does not fit on screen. Print one screenfull and
			  leave a scrollable interface
			*/
			while(do_loop)
			{
				string_buffer_t msg;
				sb_init( &msg );
				
				set_color( FISH_COLOR_BLACK,
					   get_color(HIGHLIGHT_PAGER_PROGRESS) );
				sb_printf( &msg,
					   _(L" %d to %d of %d"),
					   pos,
					   pos+termsize.ws_row-1, 
					   rows );
				
				sb_printf( &msg,
					   L"   \r" );
								
				writestr((wchar_t *)msg.buff);
				sb_destroy( &msg );
				set_color( FISH_COLOR_NORMAL, FISH_COLOR_NORMAL );
				pager_flush();
				int c = readch();

				switch( c )
				{
					case LINE_UP:
					{
						if( pos > 0 )
						{
							pos--;
							writembs(tparm( cursor_address, 0, 0));
							writembs(scroll_reverse);
							completion_print( cols,
									  width,
									  pos,
									  pos+1,
									  prefix,
									  is_quoted,
									  l );
							writembs( tparm( cursor_address,
									 termsize.ws_row-1, 0) );
							writembs(clr_eol );

						}

						break;
					}

					case LINE_DOWN:
					{
						if( pos <= (rows - termsize.ws_row ) )
						{
							pos++;
							completion_print( cols,
									  width,
									  pos+termsize.ws_row-2,
									  pos+termsize.ws_row-1,
									  prefix,
									  is_quoted,
									  l );
						}
						break;
					}

					case PAGE_DOWN:
					{

						npos = mini( rows - termsize.ws_row+1,
							     pos + termsize.ws_row-1 );
						if( npos != pos )
						{
							pos = npos;
							completion_print( cols,
									  width,
									  pos,
									  pos+termsize.ws_row-1,
									  prefix,
									  is_quoted,
									  l );
						}
						else
						{
							if( flash_screen )
								writembs( flash_screen );
						}

						break;
					}

					case PAGE_UP:
					{
						npos = maxi( 0,
							     pos - termsize.ws_row+1 );

						if( npos != pos )
						{
							pos = npos;
							completion_print( cols,
									  width,
									  pos,
									  pos+termsize.ws_row-1,
									  prefix,
									  is_quoted,
									  l );
						}
						else
						{
							if( flash_screen )
								writembs( flash_screen );
						}
						break;
					}

					case R_NULL:
					{
						do_loop=0;
						res=PAGER_RESIZE;
						break;
						
					}
					
					default:
					{
						sb_append_char( &out_buff, c );
						do_loop = 0;
						break;
					}					
				}
			}
			writembs(clr_eol);
		}
	}
	return res;
}

/**
   Substitute any series of whitespace with a single space character
   inside completion descriptions. Remove all whitespace from
   beginning/end of completion descriptions.
*/
static void mangle_descriptions( array_list_t *l )
{
	int i, skip;
	for( i=0; i<al_get_count( l ); i++ )
	{
		wchar_t *next = (wchar_t *)al_get(l, i);
		wchar_t *in, *out;
		skip=1;
		
		while( *next != COMPLETE_SEP && *next )
			next++;
		
		if( !*next )
			continue;
		
		in=out=(next+1);
		
		while( *in != 0 )
		{
			if( *in == L' ' || *in==L'\t' || *in<32 )
			{
				if( !skip )
					*out++=L' ';
				skip=1;					
			}
			else
			{
				*out++ = *in;					
				skip=0;
			}
			in++;
		}
		*out=0;		
	}
}

/**
   Merge multiple completions with the same description to the same line
*/
static void join_completions( array_list_t *l )
{
	long i;
	int in, out;
	hash_table_t desc_table;
	
	hash_init( &desc_table, &hash_wcs_func, &hash_wcs_cmp );

	for( i=0; i<al_get_count(l); i++ )
	{
		wchar_t *item = (wchar_t *)al_get( l, i );
		wchar_t *desc = wcschr( item, COMPLETE_SEP );
		long prev_idx;
		
		if( !desc )
			continue;
		desc++;
		prev_idx = ((long)hash_get( &desc_table, desc) )-1;
		if( prev_idx == -1 )
		{
			hash_put( &desc_table, halloc_wcsdup( global_context, desc), (void *)(i+1));
		}
		else
		{
			string_buffer_t foo;
			wchar_t *old = (wchar_t *)al_get( l, prev_idx );
			wchar_t *old_end = wcschr( old, COMPLETE_SEP );
			
			if( old_end )
			{
				*old_end = 0;
				
				sb_init( &foo );
				sb_append( &foo, old );
				sb_append_char( &foo, COMPLETE_ITEM_SEP );
				sb_append( &foo, item );

				free( (void *)al_get( l, prev_idx ) );
				al_set( l, prev_idx, foo.buff );

				free( (void *)al_get( l, i ) );
				al_set( l, i, 0 );
			}
			
		}
		
	}	
	hash_destroy( &desc_table );

	out=0;
	for( in=0; in < al_get_count(l); in++ )
	{
		const void * d =  al_get( l, in );
		
		if( d )
		{
			al_set( l, out++, d );
		}
	}
	al_truncate( l, out );	
}

/**
   Replace completion strings with a comp_t structure
*/
static void mangle_completions( array_list_t *l, const wchar_t *prefix )
{
	int i;
	
	for( i=0; i<al_get_count( l ); i++ )
	{
		wchar_t *next = (wchar_t *)al_get( l, i );
		wchar_t *start, *end;
		comp_t *comp = halloc( global_context, sizeof( comp_t ) );
		comp->comp = al_halloc( global_context );
		
		for( start=end=next; 1; end++ )
		{
			wchar_t c = *end;
			
			if( (c == COMPLETE_ITEM_SEP) || (c==COMPLETE_SEP) || !c)
			{
				*end = 0;
				wchar_t * str = escape( start, ESCAPE_ALL | ESCAPE_NO_QUOTED );
				
				comp->comp_width += my_wcswidth( str );
				halloc_register( global_context, str );
				al_push( comp->comp, str );
				start = end+1;
			}

			if( c == COMPLETE_SEP )
			{
				comp->desc = halloc_wcsdup( global_context, start );
				break;
			}	
			
			if( !c )
				break;
			
		}

		comp->comp_width  += my_wcswidth(prefix)*al_get_count(comp->comp) + 2*(al_get_count(comp->comp)-1);
		comp->desc_width = comp->desc?my_wcswidth( comp->desc ):0;
		
		comp->pref_width = comp->comp_width + comp->desc_width + (comp->desc_width?4:0);
		
		free( next );
		al_set( l, i, comp );
	}
	
	recalc_width( l, prefix );
}



/**
   Respond to a winch signal by checking the terminal size
*/
static void handle_winch( int sig )
{
	if (ioctl(1,TIOCGWINSZ,&termsize)!=0)
	{
		return;
	}
}

/**
   The callback function that the keyboard reading function calls when
   an interrupt occurs. This makes sure that R_NULL is returned at
   once when an interrupt has occured.
*/
static int interrupt_handler()
{
	return R_NULL;
}

/**
   Initialize various subsystems. This also closes stdin and replaces
   it with a copy of stderr, so the reading of completion strings must
   be done before init is called.
*/
static void init( int mangle_descriptors, int out )
{
	struct sigaction act;

	static struct termios pager_modes;
	char *term;
	
	if( mangle_descriptors )
	{
		
		/*
		  Make fd 1 output to screen, and use some other fd for writing
		  the resulting output back to the caller
		*/
		int in;
		out = dup( 1 );
		close(1);
		close(0);

		if( (in = open( ttyname(2), O_RDWR )) != -1 )
		{
			if( dup2( 2, 1 ) == -1 )
			{			
				debug( 0, _(L"Could not set up output file descriptors for pager") );
				exit( 1 );
			}
			
			if( dup2( in, 0 ) == -1 )
			{			
				debug( 0, _(L"Could not set up input file descriptors for pager") );
				exit( 1 );
			}
		}
		else
		{
			debug( 0, _(L"Could not open tty for pager") );
			exit( 1 );
		}
	}
	
	if( !(out_file = fdopen( out, "w" )) )
	{
		debug( 0, _(L"Could not initialize result pipe" ) );
		exit( 1 );
	}
	

	/**
	   Init the stringbuffer used to keep any output in
	*/
	sb_init( &out_buff );

	env_universal_init( 0, 0, 0, 0);
	input_common_init( &interrupt_handler );
	output_set_writer( &pager_buffered_writer );
	pager_buffer = halloc( global_context, sizeof( buffer_t ) );
	halloc_register_function( global_context, (void (*)(void *))&b_destroy, pager_buffer );

	sigemptyset( & act.sa_mask );
	act.sa_flags=0;
	act.sa_handler=SIG_DFL;
	act.sa_flags = 0;
	act.sa_handler= &handle_winch;
	if( sigaction( SIGWINCH, &act, 0 ) )
	{
		wperror( L"sigaction" );
		exit(1);
	}
	
	handle_winch( 0 );                /* Set handler for window change events */

	tcgetattr(0,&pager_modes);        /* get the current terminal modes */
	memcpy( &saved_modes,
		&pager_modes,
		sizeof(saved_modes));     /* save a copy so we can reset the terminal later */

	pager_modes.c_lflag &= ~ICANON;   /* turn off canonical mode */
	pager_modes.c_lflag &= ~ECHO;     /* turn off echo mode */
	pager_modes.c_cc[VMIN]=1;
	pager_modes.c_cc[VTIME]=0;

	/*
	  
	*/
	if( tcsetattr(0,TCSANOW,&pager_modes))      /* set the new modes */
	{
		wperror(L"tcsetattr");
		exit(1);
	}
        

	if( setupterm( 0, STDOUT_FILENO, 0) == ERR )
	{
		debug( 0, _(L"Could not set up terminal") );
		exit(1);
	}

	term = getenv("TERM");
	if( term )
	{
		wchar_t *wterm = str2wcs(term);
		output_set_term( wterm );
		free( wterm );
	}
	
}

/**
   Free memory used by various subsystems.
*/
static void destroy()
{
	env_universal_destroy();
	input_common_destroy();
	wutil_destroy();
	if( del_curterm( cur_term ) == ERR )
	{
		debug( 0, _(L"Error while closing terminfo") );		
	}
	
	sb_destroy( &out_buff );
	fclose( out_file );
}

/**
   Read lines of input from the specified file, unescape them and
   insert them into the specified list.
*/
static void read_array( FILE* file, array_list_t *comp )
{
	buffer_t buffer;
	int c;
	char cc;
	wchar_t *wcs, *unescaped;

	b_init( &buffer );

	while( !feof( file ) )
	{
		buffer.used=0;

		while( 1 )
		{
			c = getc( file );
			if( c == EOF ) 
			{
				break;
			}

			if( c == '\n' )
			{
				break;
			}

			cc=c;
			
			b_append( &buffer, &cc, 1 );
		}

		if( buffer.used )
		{
			cc=0;
			b_append( &buffer, &cc, 1 );
			
			wcs = str2wcs( buffer.buff );
			if( wcs ) 
			{
				unescaped = unescape( wcs, 0 );
				if( unescaped )
				{
					al_push( comp, unescaped );
				}				
				free( wcs );
			}
		}
	}

	b_destroy( &buffer );

}

static int get_fd( const char *str )
{
	char *end;	
	long fd;
	
	errno = 0;
	fd = strtol( str, &end, 10 );
	if( fd < 0 || *end || errno )
	{
		debug( 0, ERR_NOT_FD, program_name, optarg );
		exit( 1 );
	}
	return (int)fd;
}


int main( int argc, char **argv )
{
	int i;
	int is_quoted=0;	
	array_list_t *comp;
	wchar_t *prefix = 0;

	int mangle_descriptors = 0;
	int result_fd = -1;
		
	/*
	  This initialization is made early, so that the other init code
	  can use global_context for memory managment
	*/
	halloc_util_init();
	program_name = L"fish_pager";


	wsetlocale( LC_ALL, L"" );
	comp = al_halloc( global_context );

	/*
	  The call signature for fish_pager is a mess. Because we want
	  to be able to upgrade fish without breaking running
	  instances, we need to support all previous
	  modes. Unfortunatly, the two previous ones are a mess. The
	  third one is designed to be extensible, so hopefully it will
	  be the last.
	*/

	if( argc > 1 && argv[1][0] == '-' )
	{
		/*
		  Third mode
		*/
			
		int completion_fd = -1;
		FILE *completion_file;
			
		while( 1 )
		{
			static struct option
				long_options[] =
				{
					{
						"result-fd", required_argument, 0, 'r' 
					}
					,
					{
						"completion-fd", required_argument, 0, 'c' 
					}
					,
					{
						"prefix", required_argument, 0, 'p' 
					}
					,
					{
						"is-quoted", no_argument, 0, 'q' 
					}
					,
					{
						"help", no_argument, 0, 'h' 
					}
					,
					{
						"version", no_argument, 0, 'v' 
					}
					,
					{ 
						0, 0, 0, 0 
					}
				}
			;
		
			int opt_index = 0;
		
			int opt = getopt_long( argc,
					       argv, 
					       GETOPT_STRING,
					       long_options, 
					       &opt_index );
		
			if( opt == -1 )
				break;
		
			switch( opt )
			{
				case 0:
				{
					break;
				}
			
				case 'r':
				{
					result_fd = get_fd( optarg );
					break;
				}
			
				case 'c':
				{
					completion_fd = get_fd( optarg );
					break;
				}

				case 'p':
				{
					prefix = str2wcs(optarg);
					break;
				}

				case 'h':
				{
					print_help( argv[0], 1 );
					exit(0);						
				}

				case 'v':
				{
					debug( 0, L"%ls, version %s\n", program_name, PACKAGE_VERSION );
					exit( 0 );				
				}
					
				case 'q':
				{
					is_quoted = 1;
				}
			
			}
		}

		if( completion_fd == -1 || result_fd == -1 )
		{
			debug( 0, _(L"Unspecified file descriptors") );
			exit( 1 );
		}
			

		if( (completion_file = fdopen( completion_fd, "r" ) ) )
		{
			read_array( completion_file, comp );
			fclose( completion_file );
		}
		else
		{
			debug( 0, _(L"Could not read completions") );
			wperror( L"fdopen" );
			exit( 1 );
		}

		if( !prefix )
		{
			prefix = wcsdup( L"" );
		}
			
			
	}
	else
	{
		/*
		  Second or first mode. These suck, but we need to support
		  them for backwards compatibility. At least for some
		  time.

		  Third mode was implemented in January 2007, and previous
		  modes should be considered deprecated from that point
		  forward. A reasonable time frame for removal of the code
		  below has yet to be determined.
		*/
			
		if( argc < 3 )
		{
			print_help( argv[0], 1 );
			exit( 0 );
		}
		else
		{
			mangle_descriptors = 1;
			
			prefix = str2wcs( argv[2] );
			is_quoted = strcmp( "1", argv[1] )==0;
			
			if( argc > 3 )
			{
				/*
				  First mode
				*/
				for( i=3; i<argc; i++ )
				{
					wchar_t *wcs = str2wcs( argv[i] );
					if( wcs )
					{
						al_push( comp, wcs );
					}
				}
			}
			else
			{
				/*
				  Second mode
				*/
				read_array( stdin, comp );
			}
		}
			
	}
		
//		debug( 3, L"prefix is '%ls'", prefix );
		
	init( mangle_descriptors, result_fd );

	mangle_descriptions( comp );

	if( wcscmp( prefix, L"-" ) == 0 )
		join_completions( comp );

	mangle_completions( comp, prefix );

	/**
	   Try to print the completions. Start by trying to print the
	   list in PAGER_MAX_COLS columns, if the completions won't
	   fit, reduce the number of columns by one. Printing a single
	   column never fails.
	*/
	for( i = PAGER_MAX_COLS; i>0; i-- )
	{
		switch( completion_try_print( i, prefix, is_quoted, comp ) )
		{

			case PAGER_RETRY:
				break;

			case PAGER_DONE:
				i=0;
				break;

			case PAGER_RESIZE:
				/*
				  This means we got a resize event, so we start
				  over from the beginning. Since it the screen got
				  bigger, we might be able to fit all completions
				  on-screen.
				*/
				i=PAGER_MAX_COLS+1;
				break;

		}		
	}
	
	free(prefix );

	fwprintf( out_file, L"%ls", (wchar_t *)out_buff.buff );
	if( is_ca_mode )
	{
		writembs(exit_ca_mode);
		pager_flush();
	}
	destroy();

	halloc_util_destroy();
	
}

