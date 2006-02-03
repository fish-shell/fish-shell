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


int get_color( int highlight )
{
	if( highlight < 0 )
		return FISH_COLOR_NORMAL;
	if( highlight >= (4) )
		return FISH_COLOR_NORMAL;
	
	wchar_t *val = env_universal_get( hightlight_var[highlight]);
	
	if( val == 0 )
		return FISH_COLOR_NORMAL;
	
	if( val == 0 )
	{
		return FISH_COLOR_NORMAL;
	}
	
	return output_color_code( val );	
}

int try_sequence( char *seq )
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
	int prefix_width= my_wcswidth(prefix);

	for( i = row_start; i<row_stop; i++ )
	{
		for( j = 0; j < cols; j++ )
		{
			wchar_t *el, *el_end;

			if( al_get_count( l ) <= j*rows + i )
				continue;

			el = (wchar_t *)al_get( l, j*rows + i );
			el_end= wcschr( el, COMPLETE_SEP );

			set_color( get_color(HIGHLIGHT_PAGER_PREFIX),FISH_COLOR_NORMAL );

			writestr( prefix );

			set_color( get_color(HIGHLIGHT_PAGER_COMPLETION),FISH_COLOR_IGNORE );

			if( el_end == 0 )
			{
				/* We do not have a description for this completion */
				int written = 0;
				int max_written = width[j] - prefix_width - (j==cols-1?0:2);

				if( is_quoted )
				{
					for( i=0; i<max_written; i++ )
					{
						if( !el[i] )
							break;
						writech( el[i] );
						written+= wcwidth( el[i] );
					}
				}
				else
				{
					written = write_escaped_str( el, max_written );
				}

				set_color( get_color( HIGHLIGHT_PAGER_DESCRIPTION ),
						   FISH_COLOR_IGNORE );

				writespace( width[j]-
							written-
							prefix_width );
			}
			else
			{
				int whole_desc_width = my_wcswidth(el_end+1);
				int whole_comp_width;

				/*
				  Temporarily drop the description so that wcswidth et
				  al only calculate the width of the completion.
				*/
				*el_end = L'\0';

				/*
				  Calculate preferred completion width
				*/
				if( is_quoted )
				{
					whole_comp_width = my_wcswidth(el);
				}
				else
				{
					wchar_t *tmp = escape( el, 1 );
					whole_comp_width = my_wcswidth( tmp );
					free(tmp);
				}

				/*
				  Calculate how wide this entry 'wants' to be
				*/
				int pref_width = whole_desc_width + 4 + prefix_width + 2 -
					(j==cols-1?2:0) + whole_comp_width;

				int comp_width, desc_width;

				if( pref_width <= width[j] )
				{
					/*
					  The entry fits, we give it as much space as it wants
					*/
					comp_width = whole_comp_width;
					desc_width = whole_desc_width;
				}
				else
				{
					/*
					  The completion and description won't fit on the
					  allocated space. Give a maximum of 2/3 of the
					  space to the completion, and whatever is left to
					  the description.
					*/
					int sum = width[j] - prefix_width - 4 - 2 + (j==cols-1?2:0);

					comp_width = maxi( mini( whole_comp_width,
											 2*sum/3 ),
									   sum - whole_desc_width );
					desc_width = sum-comp_width;
				}

				/* First we must print the completion. */
				if( is_quoted )
				{
					writestr_ellipsis( el, comp_width);
				}
				else
				{
					write_escaped_str( el, comp_width );
				}

				/* Put the description back */
				*el_end = COMPLETE_SEP;

				/* And print it */
				set_color( get_color(HIGHLIGHT_PAGER_DESCRIPTION),
						   FISH_COLOR_IGNORE );
				writespace( maxi( 2,
								  width[j]
								  - comp_width
								  - desc_width
								  - 4
								  - prefix_width
								  + (j==cols-1?2:0) ) );
				/* Print description */
				writestr(L"(");
				writestr_ellipsis( el_end+1, desc_width);
				writestr(L")");

				if( j != cols-1)
					writestr( L"  " );

			}
		}
		writech( L'\n' );
	}
}

/**
   Calculates how long the specified string would be when printed on the command line.

   \param str The string to be printed.
   \param is_quoted Whether the string would be printed quoted or unquoted
   \param pref_width the preferred width for this item
   \param min_width the minimum width for this item
*/
static void printed_length( wchar_t *str,
							int is_quoted,
							int *pref_width,
							int *min_width )
{
	if( is_quoted )
	{
		wchar_t *sep = wcschr(str,COMPLETE_SEP);
		if( sep )
		{
			*sep=0;
			int cw = my_wcswidth( str );
			int dw = my_wcswidth(sep+1);

			if( termsize.ws_col > 80 )
				dw = mini( dw, termsize.ws_col/3 );


			*pref_width = cw+dw+4;

			if( dw > termsize.ws_col/3 )
			{
				dw = termsize.ws_col/3;
			}

			*min_width=cw+dw+4;

			*sep= COMPLETE_SEP;
			return;
		}
		else
		{
			*pref_width=*min_width= my_wcswidth( str );
			return;
		}

	}
	else
	{
		int comp_len=0, desc_len=0;
		int has_description = 0;
		while( *str != 0 )
		{
			if( ( *str >= ENCODE_DIRECT_BASE) &&
				( *str < ENCODE_DIRECT_BASE+256) )
			{
				if( has_description )
					desc_len+=4;
				else
					comp_len+=4;

			}
			else
			{
				
				switch( *str )
				{
					case L'\n':
					case L'\b':
					case L'\r':
					case L'\e':
					case L'\t':
					case L'\\':
					case L'&':
					case L'$':
					case L' ':
					case L'#':
					case L'^':
					case L'<':
					case L'>':
					case L'(':
					case L')':
					case L'[':
					case L']':
					case L'{':
					case L'}':
					case L'?':
					case L'*':
					case L'|':
					case L';':
					case L':':
					case L'\'':
					case L'"':
					case L'%':
					case L'~':
					
						if( has_description )
							desc_len++;
						else
							comp_len+=2;
						break;

					case COMPLETE_SEP:
						has_description = 1;
						break;

					default:
						if( has_description )
							desc_len+= wcwidth(*str);
						else
							comp_len+= wcwidth(*str);
						break;
				}
			}
			
			str++;
		}
		if( has_description )
		{
			/*
			  Mangle long descriptions to make formating look nicer
			*/
			debug( 3, L"Desc, width = %d %d\n", comp_len, desc_len );
//			if( termsize.ws_col > 80 )
//				desc_len = mini( desc_len, termsize.ws_col/3 );

			*pref_width = comp_len+ desc_len+4;;

			comp_len = mini( comp_len, maxi(0,termsize.ws_col/3 - 2));
			desc_len = mini( desc_len, maxi(0,termsize.ws_col/5 - 4));

			*min_width = comp_len+ desc_len+4;
			return;
		}
		else
		{
			debug( 3, L"No desc, width = %d\n", comp_len );
			
			*pref_width=*min_width= comp_len;
			return;
		}

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
	int prefix_width = my_wcswidth( prefix );
	
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
			wchar_t *el;
			if( al_get_count( l ) <= j*rows + i )
				continue;

			el = (wchar_t *)al_get( l, j*rows + i );
			printed_length( el, is_quoted, &pref, &min );

			pref += prefix_width;
			min += prefix_width;
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
   Substitute any series of tabs, newlines, etc. with a single space character in completion description
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

	output_init();
	env_universal_init( 0, 0, 0, 0);
	input_common_init( &interrupt_handler );
	
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
	output_destroy();
	del_curterm( cur_term );
	sb_destroy( &out_buff );
	fclose( out_file );

}

int main( int argc, char **argv )
{
	int i;
	int is_quoted=0;	
	array_list_t comp;
	wchar_t *prefix;

		
	init();
	if( argc < 3 )
	{
		debug( 0, L"Insufficient arguments" );
	}
	else
	{
		prefix = str2wcs( argv[2] );
		is_quoted = strcmp( "1", argv[1] )==0;
	
		debug( 3, L"prefix is '%ls'", prefix );
	
		al_init( &comp );
	
		for( i=3; i<argc; i++ )
		{
			wchar_t *wcs = str2wcs( argv[i] );
			if( wcs )
			{
				al_push( &comp, wcs );
			}
		}
	
		mangle_descriptions( &comp );

		for( i = 6; i>0; i-- )
		{
			switch( completion_try_print( i, prefix, is_quoted, &comp ) )
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
	
		al_foreach( &comp, (void(*)(const void *))&free );
		al_destroy( &comp );	
		free(prefix );

		fwprintf( out_file, L"%ls", (wchar_t *)out_buff.buff );
		if( is_ca_mode )
			writembs(exit_ca_mode);
	}
		
	destroy();
}

