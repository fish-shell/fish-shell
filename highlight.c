/** \file highlight.c
	Functions for syntax highlighting
*/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <wchar.h>
#include <wctype.h>
#include <termios.h>
#include <signal.h>

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "highlight.h"
#include "tokenizer.h"
#include "proc.h"
#include "parser.h"
#include "parse_util.h"
#include "parser_keywords.h"
#include "builtin.h"
#include "function.h"
#include "env.h"
#include "expand.h"
#include "sanity.h"
#include "common.h"
#include "complete.h"
#include "output.h"
#include "halloc.h"
#include "halloc_util.h"
#include "wildcard.h"
#include "path.h"

/**
   Number of elements in the highlight_var array
*/
#define VAR_COUNT ( sizeof(highlight_var)/sizeof(wchar_t *) )

static void highlight_universal_internal( wchar_t * buff, 
					  int *color, 
					  int pos, 
					  array_list_t *error );

/**
   The environment variables used to specify the color of different tokens.
*/
static wchar_t *highlight_var[] = 
{
	L"fish_color_normal",
	L"fish_color_error",
	L"fish_color_command",
	L"fish_color_end",
	L"fish_color_param",
	L"fish_color_comment",
	L"fish_color_match",
	L"fish_color_search_match",
	L"fish_color_operator",
	L"fish_color_escape",
	L"fish_color_quote",
	L"fish_color_redirection",
	L"fish_color_valid_path"
}
	;

/**
   Tests if the specified string is the prefix of any valid path in the system. 

   \return zero it this is not a valid prefix, non-zero otherwise
*/
static int is_potential_path( const wchar_t *path )
{
	wchar_t *tilde, *unescaped;
	wchar_t *in, *out;
	int has_magic = 0;
	int res = 0;

	void *context = halloc( 0, 0 );

	tilde = expand_tilde( wcsdup(path) );
	if( tilde )
	{
		halloc_register( context, tilde );
	
		unescaped = unescape( tilde, 1 );
		if( unescaped )
		{
			
//	debug( 1, L"%ls -> %ls ->%ls", path, tilde, unescaped );
			
			halloc_register( context, unescaped );
			
			for( in = out = unescaped; *in; in++ )
			{
				switch( *in )
				{
					case PROCESS_EXPAND:
					case VARIABLE_EXPAND:
					case VARIABLE_EXPAND_SINGLE:
					case BRACKET_BEGIN:
					case BRACKET_END:
					case BRACKET_SEP:
					case ANY_CHAR:
					case ANY_STRING:
					case ANY_STRING_RECURSIVE:
					{
						has_magic = 1;
						break;		
					}
				
					case INTERNAL_SEPARATOR:
					{
						break;
					}
				
					default:
					{
						*(out++) = *in;
						break;
					}
				
				}
			
			}
			*out = 0;
		
			if( !has_magic && wcslen( unescaped ) )
			{
				int must_be_dir = 0;
				DIR *dir;
				must_be_dir = unescaped[wcslen(unescaped)-1] == L'/';
				if( must_be_dir )
				{
					dir = wopendir( unescaped );
					res = !!dir;
					if( dir )
					{
						closedir( dir );
					}
				}
				else
				{
					wchar_t *dir_name, *base;
					struct wdirent *ent;
				
					dir_name = wdirname( halloc_wcsdup(context, unescaped) );
					base = wbasename( halloc_wcsdup(context, unescaped) );			
			
					if( (wcscmp( dir_name, L"/" ) == 0 ) &&
						(wcscmp( base, L"/" ) == 0 ) )
					{
						res = 1;
					}
					else if( (dir = wopendir( dir_name )) )
					{
					
						while( (ent = wreaddir( dir )) )
						{
							if( wcsncmp( ent->d_name, base, wcslen(base) ) == 0 )
							{
								res = 1;
								break;
							}
						}
					
						closedir( dir );
					}
				}
			
			}
		}
	}
	
	halloc_free( context );
		
	return res;
	
}



int highlight_get_color( int highlight )
{
	int i;
	int idx=0;
	int result = 0;
	
	if( highlight < 0 )
		return FISH_COLOR_NORMAL;
	if( highlight >= (1<<VAR_COUNT) )
		return FISH_COLOR_NORMAL;

	for( i=0; i<(VAR_COUNT-1); i++ )
	{
		if( highlight & (1<<i ))
		{
			idx = i;
			break;
		}
	}
		
	wchar_t *val = env_get( highlight_var[idx]);

//	debug( 1, L"%d -> %d -> %ls", highlight, idx, val );	
	
	if( val == 0 )
		val = env_get( highlight_var[0]);
	
	if( val )
		result = output_color_code( val );
	
	if( highlight & HIGHLIGHT_VALID_PATH )
	{
		wchar_t *val2 = env_get( L"fish_color_valid_path" );
		int result2 = output_color_code( val2 );
		if( result == FISH_COLOR_NORMAL )
			result = result2;
		else 
		{
			if( result2 & FISH_COLOR_BOLD )
				result |= FISH_COLOR_BOLD;
			if( result2 & FISH_COLOR_UNDERLINE )
				result |= FISH_COLOR_UNDERLINE;
		}
		
	}
	return result;
	
}

/**
   Highligt operators (such as $, ~, %, as well as escaped characters.
*/
static void highlight_param( const wchar_t * buff,
							 int *color,
							 int pos,
							 array_list_t *error )
{
	
	
	int mode = 0; 
	int in_pos, len = wcslen( buff );
	int bracket_count=0;
	wchar_t c;
	int normal_status = *color;
	
	for( in_pos=0;
		 in_pos<len; 
		 in_pos++ )
	{
		c = buff[in_pos];
		switch( mode )
		{

			/*
			  Mode 0 means unquoted string
			*/
			case 0:
			{
				if( c == L'\\' )
				{
					int start_pos = in_pos;
					in_pos++;
					
					if( wcschr( L"~%", buff[in_pos] ) )
					{
						if( in_pos == 1 )
						{
							color[start_pos] = HIGHLIGHT_ESCAPE;
							color[in_pos+1] = normal_status;
						}
					}
					else if( buff[in_pos]==L',' )
					{
						if( bracket_count )
						{
							color[start_pos] = HIGHLIGHT_ESCAPE;
							color[in_pos+1] = normal_status;
						}
					}
					else if( wcschr( L"abefnrtv*?$(){}[]'\"<>^ \\#;|&", buff[in_pos] ) )
					{
						color[start_pos]=HIGHLIGHT_ESCAPE;
						color[in_pos+1]=normal_status;
					}
					else if( wcschr( L"c", buff[in_pos] ) )
						{
						color[start_pos]=HIGHLIGHT_ESCAPE;
						color[in_pos+2]=normal_status;
					}
					else if( wcschr( L"uUxX01234567", buff[in_pos] ) )
					{
						int i;
						long long res=0;
						int chars=2;
						int base=16;
						
						int byte = 0;
						wchar_t max_val = ASCII_MAX;
						
						switch( buff[in_pos] )
						{
							case L'u':
							{
								chars=4;
								max_val = UCS2_MAX;
								break;
							}
							
							case L'U':
							{
								chars=8;
								max_val = WCHAR_MAX;
								break;
							}
							
							case L'x':
							{
								break;
							}
							
							case L'X':
							{
								byte=1;
								max_val = BYTE_MAX;
								break;
							}
							
							default:
							{
								base=8;
								chars=3;
								in_pos--;
								break;
							}								
						}
						
						for( i=0; i<chars; i++ )
						{
							int d = convert_digit( buff[++in_pos],base);
							
							if( d < 0 )
							{
								in_pos--;
								break;
							}
							
							res=(res*base)|d;
						}

						if( (res <= max_val) )
						{
							color[start_pos] = HIGHLIGHT_ESCAPE;
							color[in_pos+1] = normal_status;								
						}
						else
						{	
							color[start_pos] = HIGHLIGHT_ERROR;
							color[in_pos+1] = normal_status;								
						}
					}

				}
				else 
				{
					switch( buff[in_pos]){
						case L'~':
						case L'%':
						{
							if( in_pos == 0 )
							{
								color[in_pos] = HIGHLIGHT_OPERATOR;
								color[in_pos+1] = normal_status;
							}
							break;
						}

						case L'$':
						{
							wchar_t n = buff[in_pos+1];							
							color[in_pos] = (n==L'$'||wcsvarchr(n))? HIGHLIGHT_OPERATOR:HIGHLIGHT_ERROR;
							color[in_pos+1] = normal_status;								
							break;
						}


						case L'*':
						case L'?':
						case L'(':
						case L')':
						{
							color[in_pos] = HIGHLIGHT_OPERATOR;
							color[in_pos+1] = normal_status;
							break;
						}
						
						case L'{':
						{
							color[in_pos] = HIGHLIGHT_OPERATOR;
							color[in_pos+1] = normal_status;
							bracket_count++;
							break;					
						}
						
						case L'}':
						{
							color[in_pos] = HIGHLIGHT_OPERATOR;
							color[in_pos+1] = normal_status;
							bracket_count--;
							break;						
						}

						case L',':
						{
							if( bracket_count )
							{
								color[in_pos] = HIGHLIGHT_OPERATOR;
								color[in_pos+1] = normal_status;
							}

							break;					
						}
						
						case L'\'':
						{
							color[in_pos] = HIGHLIGHT_QUOTE;
							mode = 1;
							break;					
						}
				
						case L'\"':
						{
							color[in_pos] = HIGHLIGHT_QUOTE;
							mode = 2;
							break;
						}

					}
				}		
				break;
			}

			/*
			  Mode 1 means single quoted string, i.e 'foo'
			*/
			case 1:
			{
				if( c == L'\\' )
				{
					int start_pos = in_pos;
					switch( buff[++in_pos] )
					{
						case '\\':
						case L'\'':
						{
							color[start_pos] = HIGHLIGHT_ESCAPE;
							color[in_pos+1] = HIGHLIGHT_QUOTE;
							break;
						}
						
						case 0:
						{
							return;
						}
						
					}
					
				}
				if( c == L'\'' )
				{
					mode = 0;
					color[in_pos+1] = normal_status;
				}
				
				break;
			}

			/*
			  Mode 2 means double quoted string, i.e. "foo"
			*/
			case 2:
			{
				switch( c )
				{
					case '"':
					{
						mode = 0;
						color[in_pos+1] = normal_status;
						break;
					}
				
					case '\\':
					{
						int start_pos = in_pos;
						switch( buff[++in_pos] )
						{
							case L'\0':
							{
								return;
							}
							
							case '\\':
							case L'$':
							case '"':
							{
								color[start_pos] = HIGHLIGHT_ESCAPE;
								color[in_pos+1] = HIGHLIGHT_QUOTE;
								break;
							}
						}
						break;
					}
					
					case '$':
					{
						wchar_t n = buff[in_pos+1];
						color[in_pos] = (n==L'$'||wcsvarchr(n))? HIGHLIGHT_OPERATOR:HIGHLIGHT_ERROR;
						color[in_pos+1] = HIGHLIGHT_QUOTE;								
						break;
					}
				
				}						
				break;
			}
		}
	}
}

static int has_expand_reserved( wchar_t *str )
{
	while( *str )
	{
		if( *str >= EXPAND_RESERVED &&
		    *str <= EXPAND_RESERVED_END )
		{
			return 1;
		}
		str++;
	}
	return 0;
}


void highlight_shell( wchar_t * buff, 
					  int *color, 
					  int pos, 
					  array_list_t *error )
{
	tokenizer tok;
	int had_cmd=0;
	int i;
	int last_val;
	wchar_t *last_cmd=0;
	int len;

	void *context;
	wchar_t *cmd=0;
	int accept_switches = 1;		
	
	int use_function = 1;
	int use_command = 1;
	int use_builtin = 1;
	
	CHECK( buff, );
	CHECK( color, );
	
	len = wcslen(buff);
	
	if( !len )
		return;
	
	context = halloc( 0, 0 );
	
	for( i=0; buff[i] != 0; i++ )
		color[i] = -1;
	
	for( tok_init( &tok, buff, TOK_SHOW_COMMENTS );
		 tok_has_next( &tok );
		 tok_next( &tok ) )
	{	
		int last_type = tok_last_type( &tok );
		int prev_argc=0;

		switch( last_type )
		{
			case TOK_STRING:
			{
				if( had_cmd )
				{
					
					/*Parameter */
					wchar_t *param = tok_last( &tok );
					if( param[0] == L'-' )
					{
						if (wcscmp( param, L"--" ) == 0 )
						{
							accept_switches = 0;
							color[ tok_get_pos( &tok ) ] = HIGHLIGHT_PARAM;
						}
						else if( accept_switches )
						{
							if( complete_is_valid_option( last_cmd, param, error ) )
								color[ tok_get_pos( &tok ) ] = HIGHLIGHT_PARAM;
							else
								color[ tok_get_pos( &tok ) ] = HIGHLIGHT_ERROR;
						}
						else
						{
							color[ tok_get_pos( &tok ) ] = HIGHLIGHT_PARAM;
						}
					}
					else
					{
						color[ tok_get_pos( &tok ) ] = HIGHLIGHT_PARAM;
					}					

					if( cmd && (wcscmp( cmd, L"cd" ) == 0) )
					{
						wchar_t *dir = expand_one( context, 
									   wcsdup(tok_last( &tok )),
									   EXPAND_SKIP_CMDSUBST );
						if( dir )
						{
							int is_long_help = wcsncmp(dir,L"--help", wcslen(dir) );
							int is_short_help = wcsncmp(dir,L"-h", wcslen(dir) );
							
							if( !is_long_help && !is_short_help  && !path_get_cdpath( context, dir ) )
							{
								color[ tok_get_pos( &tok ) ] = HIGHLIGHT_ERROR;							
							}
						}
					}
					
					highlight_param( param,
									 &color[tok_get_pos( &tok )],
									 pos-tok_get_pos( &tok ), 
									 error );
					

				}
				else
				{ 
					prev_argc=0;
					
					/*
					  Command. First check that the command actually exists.
					*/
					cmd = expand_one( context, 
							  wcsdup(tok_last( &tok )),
							  EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES);
					
					if( (cmd == 0) || has_expand_reserved( cmd ) )
					{
						color[ tok_get_pos( &tok ) ] = HIGHLIGHT_ERROR;
					}
					else
					{
						int is_cmd = 0;
						int is_subcommand = 0;
						int mark = tok_get_pos( &tok );
						color[ tok_get_pos( &tok ) ] = HIGHLIGHT_COMMAND;

						if( parser_keywords_is_subcommand( cmd ) )
						{
							
							int sw;
							
							if( wcscmp( cmd, L"builtin" )==0)
							{
								use_function = 0;
								use_command  = 0;
								use_builtin  = 1;
							}
							else if( wcscmp( cmd, L"command" )==0)
							{
								use_command  = 1;
								use_function = 0;
								use_builtin  = 0;
							}

							tok_next( &tok );
							
							sw = parser_keywords_is_switch( tok_last( &tok ) );
							
							if( !parser_keywords_is_block( cmd ) &&
								sw == ARG_SWITCH )
							{
								/* 
								   The 'builtin' and 'command' builtins
								   are normally followed by another
								   command, but if they are invoked
								   with a switch, they aren't.

								*/
								use_command  = 1;
								use_function = 1;
								use_builtin  = 2;
							}
							else
							{
								if( sw == ARG_SKIP )
								{
									color[ tok_get_pos( &tok ) ] = HIGHLIGHT_PARAM;
									mark = tok_get_pos( &tok );
								}
								
								is_subcommand = 1;
							}
							tok_set_pos( &tok, mark );
						}
						
						if( !is_subcommand )
						{
							wchar_t *tmp;
							/*
							  OK, this is a command, it has been
							  successfully expanded and everything
							  looks ok. Lets check if the command
							  exists.
							*/

							/*
							  First check if it is a builtin or
							  function, since we don't have to stat
							  any files for that
							*/
							if( use_builtin )
								is_cmd |= builtin_exists( cmd );
							
							if( use_function )
								is_cmd |= function_exists( cmd );

							/*
							  Moving on to expensive tests
							*/

							/*
							  Check if this is a regular command
							*/
							if( use_command )
								is_cmd |= !!(tmp=path_get_path( context, cmd ));
							
							/* 
							   Could not find the command. Maybe it is
							   a path for a implicit cd command.
							*/
							if( use_builtin || (use_function && function_exists( L"cd") ) )
								is_cmd |= !!(tmp=path_get_cdpath( context, cmd ));
														
							if( is_cmd )
							{								
								color[ tok_get_pos( &tok ) ] = HIGHLIGHT_COMMAND;
							}
							else
							{
								if( error )
									al_push( error, wcsdupcat ( L"Unknown command \'", cmd, L"\'" ));
								color[ tok_get_pos( &tok ) ] = (HIGHLIGHT_ERROR);
							}
							had_cmd = 1;
						}

						if( had_cmd )
						{
							last_cmd = halloc_wcsdup( context, tok_last( &tok ) );						
						}
					}

				}
				break;
			}
		
			case TOK_REDIRECT_NOCLOB:
			case TOK_REDIRECT_OUT:
			case TOK_REDIRECT_IN:
			case TOK_REDIRECT_APPEND:
			case TOK_REDIRECT_FD:
			{
				if( !had_cmd )
				{
					color[ tok_get_pos( &tok ) ] = HIGHLIGHT_ERROR;
					if( error )
						al_push( error, wcsdup ( L"Redirection without a command" ) );
					break;
				}
				
				wchar_t *target=0;
				
				color[ tok_get_pos( &tok ) ] = HIGHLIGHT_REDIRECTION;
				tok_next( &tok );

				/*
				  Check that we are redirecting into a file
				*/

				switch( tok_last_type( &tok ) )
				{
					case TOK_STRING:
					{
						target = expand_one( context, wcsdup( tok_last( &tok ) ), EXPAND_SKIP_CMDSUBST);
						/*
						  Redirect filename may contain a cmdsubst. 
						  If so, it will be ignored/not flagged.
						*/
					}
					break;
					default:
					{
						color[ tok_get_pos( &tok ) ] = HIGHLIGHT_ERROR;
						if( error )
							al_push( error, wcsdup ( L"Invalid redirection" ) );
					}
					
				}

				if( target != 0 )
				{
					wchar_t *dir = halloc_wcsdup( context, target );
					wchar_t *dir_end = wcsrchr( dir, L'/' );
					struct stat buff;
					/* 
					   If file is in directory other than '.', check
					   that the directory exists.
					*/
					if( dir_end != 0 )
					{
						*dir_end = 0;
						if( wstat( dir, &buff ) == -1 )
						{
							color[ tok_get_pos( &tok ) ] = HIGHLIGHT_ERROR;
							if( error )
								al_push( error, wcsdupcat( L"Directory \'", dir, L"\' does not exist" ) );
							
						}
					}
					
					/*
					  If the file is read from or appended to, check
					  if it exists.
					*/
					if( last_type == TOK_REDIRECT_IN || 
					    last_type == TOK_REDIRECT_APPEND )
					{
						if( wstat( target, &buff ) == -1 )
						{
							color[ tok_get_pos( &tok ) ] = HIGHLIGHT_ERROR;
							if( error )
								al_push( error, wcsdupcat( L"File \'", target, L"\' does not exist" ) );
						}
					}
					if( last_type == TOK_REDIRECT_NOCLOB )
					{
						if( wstat( target, &buff ) != -1 )
						{
							color[ tok_get_pos( &tok ) ] = HIGHLIGHT_ERROR;
							if( error )
								al_push( error, wcsdupcat( L"File \'", target, L"\' exists" ) );
						}
					}
				}
				break;
			}
			
			case TOK_PIPE:
			case TOK_BACKGROUND:
			{
				if( had_cmd )
				{
					color[ tok_get_pos( &tok ) ] = HIGHLIGHT_END;
					had_cmd = 0;
					use_command  = 1;
					use_function = 1;
					use_builtin  = 1;
					accept_switches = 1;
				}
				else
				{
					color[ tok_get_pos( &tok ) ] = HIGHLIGHT_ERROR;					
					if( error )
						al_push( error, wcsdup ( L"No job to put in background" ) );
				}
				
				break;
			}
			
			case TOK_END:
			{
				color[ tok_get_pos( &tok ) ] = HIGHLIGHT_END;
				had_cmd = 0;
				use_command  = 1;
				use_function = 1;
				use_builtin  = 1;
				accept_switches = 1;
				break;
			}
			
			case TOK_COMMENT:
			{
				color[ tok_get_pos( &tok ) ] = HIGHLIGHT_COMMENT;
				break;
			}
			
			case TOK_ERROR:
			default:
			{
				/*
				  If the tokenizer reports an error, highlight it as such.
				*/
				if( error )
					al_push( error, wcsdup ( tok_last( &tok) ) );
				color[ tok_get_pos( &tok ) ] = HIGHLIGHT_ERROR;
				break;				
			}			
		}
	}

	tok_destroy( &tok );	
			 
	/*
	  Locate and syntax highlight cmdsubsts recursively
	*/

	wchar_t *buffcpy = halloc_wcsdup( context, buff );
	wchar_t *subpos=buffcpy;
	int done=0;
	
	while( 1 )
	{
		wchar_t *begin, *end;
	
		if( parse_util_locate_cmdsubst( subpos, 
										&begin, 
										&end,
										1) <= 0)
		{
			break;
		}
		
		if( !*end )
			done=1;
		else
			*end=0;
		
		highlight_shell( begin+1, color +(begin-buffcpy)+1, -1, error );
		color[end-buffcpy]=HIGHLIGHT_OPERATOR;
		
		if( done )
			break;
		
		subpos = end+1;		
		
	}

	/*
	  The highlighting code only changes the first element when the
	  color changes. This fills in the rest.
	*/
	last_val=0;
	for( i=0; buff[i] != 0; i++ )
	{
		if( color[i] >= 0 )
			last_val = color[i];
		else
			color[i] = last_val;
	}

	/*
	  Color potentially valid paths in a special path color if they
	  are the current token.
	*/

	if( pos >= 0 && pos <= len )
	{
		
		wchar_t *tok_begin, *tok_end;
		wchar_t *token;
		
		parse_util_token_extent( buff, pos, &tok_begin, &tok_end, 0, 0 );
		if( tok_begin && tok_end )
		{
			token = halloc_wcsndup( context, tok_begin, tok_end-tok_begin );
			
			if( is_potential_path( token ) )
			{
				for( i=tok_begin-buff; i < (tok_end-buff); i++ )
				{
					color[i] |= HIGHLIGHT_VALID_PATH;
				}
			}
		}
	}
	

	highlight_universal_internal( buff, color, pos, error );

	/*
	  Spaces should not be highlighted at all, since it makes cursor look funky in some terminals
	*/
	for( i=0; buff[i]; i++ )
	{
		if( iswspace(buff[i]) )
		{
			color[i]=0;
		}
	}

	halloc_free( context );
	
}

/**
   Perform quote and parenthesis highlighting on the specified string.
*/
static void highlight_universal_internal( wchar_t * buff, 
										  int *color, 
										  int pos, 
										  array_list_t *error )
{
	
	if( (pos >= 0) && (pos < wcslen(buff)) )
	{
		
		/*
		  Highlight matching quotes
		*/
		if( (buff[pos] == L'\'') || (buff[pos] == L'\"') )
		{

			array_list_t l;
			al_init( &l );
		
			int level=0;
			wchar_t prev_q=0;
		
			wchar_t *str=buff;

			int match_found=0;
		
			while(*str)
			{
				switch( *str )
				{
					case L'\\':
						str++;
						break;
					case L'\"':
					case L'\'':
						if( level == 0 )
						{
							level++;
							al_push_long( &l, (long)(str-buff) );
							prev_q = *str;
						}
						else
						{
							if( prev_q == *str )
							{
								long pos1, pos2;
							
								level--;
								pos1 = al_pop_long( &l );
								pos2 = str-buff;
								if( pos1==pos || pos2==pos )
								{
									color[pos1]|=HIGHLIGHT_MATCH<<16;
									color[pos2]|=HIGHLIGHT_MATCH<<16;
									match_found = 1;
									
								}
								prev_q = *str==L'\"'?L'\'':L'\"';
							}
							else
							{
								level++;
								al_push_long( &l, (long)(str-buff) );
								prev_q = *str;
							}
						}
					
						break;
				}
				if( (*str == L'\0'))
					break;

				str++;
			}

			al_destroy( &l );
		
			if( !match_found )
				color[pos] = HIGHLIGHT_ERROR<<16;
		}

		/*
		  Highlight matching parenthesis
		*/
		if( wcschr( L"()[]{}", buff[pos] ) )
		{
			int step = wcschr(L"({[", buff[pos])?1:-1;
			wchar_t dec_char = *(wcschr( L"()[]{}", buff[pos] ) + step);
			wchar_t inc_char = buff[pos];
			int level = 0;
			wchar_t *str = &buff[pos];
			int match_found=0;			

			while( (str >= buff) && *str)
			{
				if( *str == inc_char )
					level++;
				if( *str == dec_char )
					level--;
				if( level == 0 )
				{
					int pos2 = str-buff;
					color[pos]|=HIGHLIGHT_MATCH<<16;
					color[pos2]|=HIGHLIGHT_MATCH<<16;
					match_found=1;
					break;
				}
				str+= step;
			}
			
			if( !match_found )
				color[pos] = HIGHLIGHT_ERROR<<16;
		}
	}
}

void highlight_universal( wchar_t * buff, 
						  int *color, 
						  int pos, 
						  array_list_t *error )
{
	int i;
	
	for( i=0; buff[i]; i++ )
		color[i] = 0;
	
	highlight_universal_internal( buff, color, pos, error );
}
