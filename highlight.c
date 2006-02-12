/** \file highlight.c
	Functions for syntax highlighting
*/
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <wchar.h>
#include <wctype.h>
#include <termios.h>
#include <signal.h>

#include "config.h"
#include "util.h"
#include "wutil.h"
#include "highlight.h"
#include "tokenizer.h"
#include "proc.h"
#include "parser.h"
#include "parse_util.h"
#include "builtin.h"
#include "function.h"
#include "env.h"
#include "expand.h"
#include "sanity.h"
#include "common.h"
#include "complete.h"
#include "output.h"

static void highlight_universal_internal( wchar_t * buff, 
										  int *color, 
										  int pos, 
										  array_list_t *error );


/**
   The environment variables used to specify the color of different tokens.
*/
static wchar_t *hightlight_var[] = 
{
	L"fish_color_normal",
	L"fish_color_command",
	L"fish_color_subshell",
	L"fish_color_redirection",
	L"fish_color_end",
	L"fish_color_error",
	L"fish_color_param",
	L"fish_color_comment",
	L"fish_color_match",
	L"fish_color_search_match",
	L"fish_color_pager_prefix",
	L"fish_color_pager_completion",
	L"fish_color_pager_description",
	L"fish_color_pager_progress"
}
	;


int highlight_get_color( int highlight )
{
	if( highlight < 0 )
		return FISH_COLOR_NORMAL;
	if( highlight >= (12) )
		return FISH_COLOR_NORMAL;
	
	wchar_t *val = env_get( hightlight_var[highlight]);
	if( val == 0 )
		val = env_get( hightlight_var[HIGHLIGHT_NORMAL]);
	
	if( val == 0 )
	{
		return FISH_COLOR_NORMAL;
	}
	
	return output_color_code( val );
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
	int len = wcslen(buff);
	
	if( !len )
		return;
	
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
						if( complete_is_valid_option( last_cmd, param, error ))
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
					prev_argc=0;
					
					/*
					  Command. First check that the command actually exists.
					*/
					wchar_t *cmd = expand_one( 0, 
											   wcsdup(tok_last( &tok )),
											   EXPAND_SKIP_SUBSHELL | EXPAND_SKIP_VARIABLES);
					
					if( cmd == 0 )
					{
						color[ tok_get_pos( &tok ) ] = HIGHLIGHT_ERROR;
					}
					else
					{
						wchar_t *tmp;
						int is_cmd = 0;
						int is_subcommand = 0;
						int mark = tok_get_pos( &tok );
						color[ tok_get_pos( &tok ) ] = HIGHLIGHT_COMMAND;
						
						if( parser_is_subcommand( cmd ) )
						{
							tok_next( &tok );
							if(( wcscmp( L"-h", tok_last( &tok ) ) == 0 ) ||
							   ( wcscmp( L"--help", tok_last( &tok ) ) == 0 ) )							
							{
								/* 
								   The builtin and command builtins
								   are normally followed by another
								   command, but if they are invoked
								   with the -h option, their help text
								   is displayed instead
								*/
							}
							else
							{
								is_subcommand = 1;
							}
							tok_set_pos( &tok, mark );
						}

						if( !is_subcommand )
						{
							/*
							  OK, this is a command, it has been
							  successfully expanded and everything
							  looks ok. Lets check if the command
							  exists.
							*/
							is_cmd |= builtin_exists( cmd );
							is_cmd |= function_exists( cmd );
							is_cmd |= (tmp=get_filename( cmd )) != 0;
							
							/* 
							   Could not find the command. Maybe it is a path for a implicit cd command.
							   Lets check!
							*/
							if( !is_cmd )
							{
								wchar_t *pp = parser_cdpath_get( cmd );
								if( pp )
								{
									free( pp );
									is_cmd = 1;
								}		
							}
							
							free(tmp);
							if( is_cmd )
							{								
								color[ tok_get_pos( &tok ) ] = HIGHLIGHT_COMMAND;
							}
							else
							{
								if( error )
									al_push( error, wcsdupcat2 ( L"Unknown command \'", cmd, L"\'", 0 ));
								color[ tok_get_pos( &tok ) ] = (HIGHLIGHT_ERROR);
							}
							had_cmd = 1;
						}
						free(cmd);

						if( had_cmd )
						{
							if( last_cmd )
								free( last_cmd );
							last_cmd = wcsdup( tok_last( &tok ) );						
						}					

					}

				}
				break;
			}
		
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
						target = expand_one( 0, wcsdup( tok_last( &tok ) ), EXPAND_SKIP_SUBSHELL);
						/*
						  Redirect filename may contain a subshell. 
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
					wchar_t *dir = wcsdup( target );
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
								al_push( error, wcsdupcat2( L"Directory \'", dir, L"\' does not exist", 0 ) );
							
						}						
					}
					free( dir );
					
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
								al_push( error, wcsdupcat2( L"File \'", target, L"\' does not exist", 0 ) );
						}
					}
					free( target );					
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

	if( last_cmd )
		free( last_cmd );
	
	tok_destroy( &tok );	
			 
	/*
	  Locate and syntax highlight subshells recursively
	*/

	wchar_t *buffcpy = wcsdup( buff );
	wchar_t *subpos=buffcpy;
	int done=0;
	
	while( 1 )
	{
		wchar_t *begin, *end;
	
		if( parse_util_locate_cmdsubst( subpos, 
										(const wchar_t **)&begin, 
										(const wchar_t **)&end,
										1) <= 0)
		{
			break;
		}
		
		if( !*end )
			done=1;
		else
			*end=0;
		
		highlight_shell( begin+1, color +(begin-buffcpy)+1, -1, error );
		color[end-buffcpy]=HIGHLIGHT_PARAM;
		
		if( done )
			break;
		
		subpos = end+1;		
		
	}
	free( buffcpy);
	

	last_val=0;
	for( i=0; buff[i] != 0; i++ )
	{
		if( color[i] >= 0 )
			last_val = color[i];
		else
			color[i] = last_val;
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
							al_push( &l, (void *)(str-buff) );
							prev_q = *str;
						}
						else
						{
							if( prev_q == *str )
							{
								int pos1, pos2;
							
								level--;
								pos1 = (int)al_pop( &l );
								pos2 = str-buff;
								if( pos1==pos || pos2==pos )
								{
									color[pos1]|=HIGHLIGHT_MATCH<<8;
									color[pos2]|=HIGHLIGHT_MATCH<<8;
									match_found = 1;
									
								}
								prev_q = *str==L'\"'?L'\'':L'\"';
							}
							else
							{
								level++;
								al_push( &l, (void *)(str-buff) );
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
				color[pos] = HIGHLIGHT_ERROR<<8;
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
					color[pos]|=HIGHLIGHT_MATCH<<8;
					color[pos2]|=HIGHLIGHT_MATCH<<8;
					match_found=1;
					break;
				}
				str+= step;
			}
			
			if( !match_found )
				color[pos] = HIGHLIGHT_ERROR<<8;
		}
	}
}

void highlight_universal( wchar_t * buff, 
						  int *color, 
						  int pos, 
						  array_list_t *error )
{
	int i;
	
	for( i=0; buff[i] != 0; i++ )
		color[i] = 0;
	
	highlight_universal_internal( buff, color, pos, error );
}
