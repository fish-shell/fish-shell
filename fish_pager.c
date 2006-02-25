#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
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

#include "util.h"
#include "wutil.h"
#include "common.h"
#include "complete.h"
#include "output.h"
#include "input_common.h"
#include "env_universal.h"
#include "halloc.h"
#include "halloc_util.h"

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

/**
   This struct should be continually updated by signals as the term
   resizes, and as such always contain the correct current size.
*/

static struct winsize termsize;

static struct termios saved_modes;
static struct termios pager_modes;

static int is_ca_mode = 0;

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

static string_buffer_t out_buff;
static FILE *out_file;

typedef struct 
{
	array_list_t *comp;
	wchar_t *desc;
	int comp_width;	
	int desc_width;	
	int pref_width;
	int min_width;
}
	comp_t;

static int get_color( int highlight )
{
	if( highlight < 0 )
		return FISH_COLOR_NORMAL;
	if( highlight >= (4) )
		return FISH_COLOR_NORMAL;
	
	wchar_t *val = env_universal_get( hightlight_var[highlight]);
	
	if( val == 0 )
	{
		return FISH_COLOR_NORMAL;
	}
	
	return output_color_code( val );	
}

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
				"\e[A", LINE_UP
			}
			,
			{
				key_up, LINE_UP
			}
			,
			{				
				"\e[B", LINE_DOWN
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
	
	for( i=0; m[i].seq; i++ )
	{
		if( try_sequence(m[i].seq ) )
			return m[i].bnd;
	}
	return input_common_readch(0);
}


static int pager_buffered_writer( char c)
{
	b_append( pager_buffer, &c, 1 );
	return 0;
}

static void pager_flush()
{
	write( 1, pager_buffer->buff, pager_buffer->used );
	pager_buffer->used = 0;
}


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

   \return zero if the specified number of columns do not fit, no-zero otherwise
*/

static int completion_try_print( int cols,
								 wchar_t *prefix,
								 int is_quoted,
								 array_list_t *l )
{
	/*
	  The calculated preferred width of each column
	*/
	int pref_width[32];
	/*
	  The calculated minimum width of each column
	*/
	int min_width[32];
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
	int res=0;
	/*
	  Skip completions on tiny terminals
	*/
	
	if( termsize.ws_col < 16 )
		return 1;
	
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
			  Terminal almost wide enough, or squeezing makes the whole list fit on-screen
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

//	return cols==1;
	
	if( print )
	{
		res=1;
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

			is_ca_mode=1;
			writembs(enter_ca_mode);

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
				wchar_t msg[10];
				int percent = 100*pos/(rows-termsize.ws_row+1);
				set_color( FISH_COLOR_BLACK,
						   get_color(HIGHLIGHT_PAGER_PROGRESS) );
				swprintf( msg, 12,
						  L" %ls(%d%%) \r",
						  percent==100?L"":(percent >=10?L" ": L"  "),
						  percent );
				writestr(msg);
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
						res=2;
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
	int i, in, out;
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
			
//			debug( 1, L"WOOT WOOT %ls är släkt med %ls", item, al_get( l, prev_idx ) );
			
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
				wchar_t * str = escape( start, 1 );
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

static int interrupt_handler()
{
	return R_NULL;
}


static void init()
{
	struct sigaction act;
	program_name = L"fish_pager";
	wsetlocale( LC_ALL, L"" );

	/*
	  Make fd 1 output to screen, and use some other fd for writing
	  the resulting output back to the caller
	*/
	int out = dup( 1 );
	close(1);
	if( open( ttyname(0), O_WRONLY ) != 1 )
	{
		if( dup2( 2, 1 ) == -1 )
		{			
			debug( 0, L"Could not set up file descriptors for pager" );
			exit( 1 );
		}
	}
	out_file = fdopen( out, "w" );

	/**
	   Init the stringbuffer used to keep any output in
	*/
	sb_init( &out_buff );

	halloc_util_init();
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
		debug( 0, L"Could not set up terminal" );
		exit(1);
	}

}

void destroy()
{
	env_universal_destroy();
	input_common_destroy();
	halloc_util_destroy();
	del_curterm( cur_term );
	sb_destroy( &out_buff );
	fclose( out_file );
}

int main( int argc, char **argv )
{
	int i;
	int is_quoted=0;	
	array_list_t *comp;
	wchar_t *prefix;
		
	init();
	
	if( argc < 3 )
	{
		debug( 0, L"Insufficient arguments" );
	}
	else
	{
		comp = al_halloc( global_context );
		prefix = str2wcs( argv[2] );
		is_quoted = strcmp( "1", argv[1] )==0;
		is_quoted = 0;
		
		debug( 3, L"prefix is '%ls'", prefix );
		
		for( i=3; i<argc; i++ )
		{
			wchar_t *wcs = str2wcs( argv[i] );
			if( wcs )
			{
				al_push( comp, wcs );
			}
		}
		
		mangle_descriptions( comp );
		if( wcscmp( prefix, L"-" ) == 0 )
			join_completions( comp );
		mangle_completions( comp, prefix );
	
		for( i = 6; i>0; i-- )
		{
			switch( completion_try_print( i, prefix, is_quoted, comp ) )
			{
				case 0:
					break;
				case 1:
					i=0;
					break;
				case 2:
					i=7;
					break;
			}
		
		}
	
		free(prefix );

		fwprintf( out_file, L"%ls", (wchar_t *)out_buff.buff );
		if( is_ca_mode )
			writembs(exit_ca_mode);
	}
		
	destroy();
}

