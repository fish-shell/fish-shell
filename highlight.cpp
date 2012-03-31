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
#include "wildcard.h"
#include "path.h"

/**
   Number of elements in the highlight_var array
*/
#define VAR_COUNT ( sizeof(highlight_var)/sizeof(wchar_t *) )

static void highlight_universal_internal( const wcstring &buff,
					  std::vector<int> &color, 
					  int pos );

/**
   The environment variables used to specify the color of different tokens.
*/
static const wchar_t * const highlight_var[] = 
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
	L"fish_color_valid_path",
    L"fish_color_autosuggestion"
};

/**
   Tests if the specified string is the prefix of any valid path in the system. 
   
   \require_dir Whether the valid path must be a directory
   \out_path If non-null, the path on output
   \return zero it this is not a valid prefix, non-zero otherwise
*/
// PCA DOES_IO
static bool is_potential_path( const wcstring &cpath, wcstring *out_path = NULL, bool require_dir = false )
{
    ASSERT_IS_BACKGROUND_THREAD();
    
	const wchar_t *unescaped, *in;
    wcstring cleaned_path;
	int has_magic = 0;
	bool res = false;
    
    wcstring path(cpath);
    expand_tilde(path);
    if (! unescape_string(path, 1))
        return false;
    
    unescaped = path.c_str();
    
    //	debug( 1, L"%ls -> %ls ->%ls", path, tilde, unescaped );
    
    for( in = unescaped; *in; in++ )
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
                cleaned_path += *in;
                break;
            }
				
        }
        
    }
    
    if( !has_magic && ! cleaned_path.empty() )
    {
        bool must_be_full_dir = cleaned_path[cleaned_path.length()-1] == L'/';
        DIR *dir;
        if( must_be_full_dir )
        {
            dir = wopendir( cleaned_path );
            if( dir )
            {
                res = true;
                if (out_path)
                    *out_path = cleaned_path;
                closedir( dir );
            }
        }
        else
        {
            wcstring dir_name = wdirname(cleaned_path);
            wcstring base_name = wbasename(cleaned_path);
            
            if( dir_name == L"/" && base_name == L"/" )
            {
                res = true;
                if (out_path)
                    *out_path = cleaned_path;
            }
            else if( (dir = wopendir( dir_name)) )
            {
                wcstring ent;
                bool is_dir;
                while (wreaddir_resolving(dir, dir_name, ent, &is_dir))
                {
                    if (string_prefixes_string(base_name, ent) && (! require_dir || is_dir))
                    {
                        res = true;
                        if (out_path) {
                            out_path->assign(dir_name);
                            out_path->push_back(L'/');
                            out_path->append(ent);
                            path_make_canonical(*out_path);
                            /* We actually do want a trailing / for directories, since it makes autosuggestion a bit nicer */
                            if (is_dir)
                                out_path->push_back(L'/');
                        }
                        break;
                    }
                }
                
                closedir( dir );
            }
        }
        
    }
	
	return res;
	
}

rgb_color_t highlight_get_color( int highlight, bool is_background )
{
	size_t i;
	int idx=0;
	rgb_color_t result;

	if( highlight < 0 )
		return rgb_color_t::normal();
	if( highlight > (1<<VAR_COUNT) )
		return rgb_color_t::normal();
	for( i=0; i<VAR_COUNT; i++ )
	{
		if( highlight & (1<<i ))
		{
			idx = i;
			break;
		}
	}
    
	env_var_t val_wstr = env_get_string( highlight_var[idx]); 

//	debug( 1, L"%d -> %d -> %ls", highlight, idx, val );	
	
	if (val_wstr.missing())
		val_wstr = env_get_string( highlight_var[0]);
	
	if( ! val_wstr.missing() )
		result = parse_color( val_wstr, is_background );
	
	if( highlight & HIGHLIGHT_VALID_PATH )
	{
		env_var_t val2_wstr =  env_get_string( L"fish_color_valid_path" );
		const wcstring val2 = val2_wstr.missing() ? L"" : val2_wstr.c_str(); 

		rgb_color_t result2 = parse_color( val2, is_background );
		if( result.is_normal() )
			result = result2;
		else 
		{
			if( result2.is_bold() )
				result.set_bold(true);
			if( result2.is_underline() )
				result.set_underline(true);
		}
	}
	return result;
}


/**
   Highligt operators (such as $, ~, %, as well as escaped characters.
*/
static void highlight_param( const wcstring &buffstr, std::vector<int> &colors, int pos, wcstring_list_t *error )
{	
    const wchar_t * const buff = buffstr.c_str();
	enum {e_unquoted, e_single_quoted, e_double_quoted} mode = e_unquoted;
	size_t in_pos, len = buffstr.size();
	int bracket_count=0;
	int normal_status = colors.at(0);
	
	for (in_pos=0; in_pos<len; in_pos++)
	{
		wchar_t c = buffstr.at(in_pos);
		switch( mode )
		{
                /*
                 Mode 0 means unquoted string
                 */
			case e_unquoted:
			{
				if( c == L'\\' )
				{
					size_t start_pos = in_pos;
					in_pos++;
					
					if( wcschr( L"~%", buff[in_pos] ) )
					{
						if( in_pos == 1 )
						{
							colors.at(start_pos) = HIGHLIGHT_ESCAPE;
							colors.at(in_pos+1) = normal_status;
						}
					}
					else if( buff[in_pos]==L',' )
					{
						if( bracket_count )
						{
							colors.at(start_pos) = HIGHLIGHT_ESCAPE;
							colors.at(in_pos+1) = normal_status;
						}
					}
					else if( wcschr( L"abefnrtv*?$(){}[]'\"<>^ \\#;|&", buff[in_pos] ) )
					{
						colors.at(start_pos)=HIGHLIGHT_ESCAPE;
						colors.at(in_pos+1)=normal_status;
					}
					else if( wcschr( L"c", buff[in_pos] ) )
                    {
						colors.at(start_pos)=HIGHLIGHT_ESCAPE;
                        if (in_pos+2 < colors.size())
                            colors.at(in_pos+2)=normal_status;
					}
					else if( wcschr( L"uUxX01234567", buff[in_pos] ) )
					{
						int i;
						long long res=0;
						int chars=2;
						int base=16;
						
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
							colors.at(start_pos) = HIGHLIGHT_ESCAPE;
							colors.at(in_pos+1) = normal_status;								
						}
						else
						{	
							colors.at(start_pos) = HIGHLIGHT_ERROR;
							colors.at(in_pos+1) = normal_status;								
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
								colors.at(in_pos) = HIGHLIGHT_OPERATOR;
								colors.at(in_pos+1) = normal_status;
							}
							break;
						}
                            
						case L'$':
						{
							wchar_t n = buff[in_pos+1];							
							colors.at(in_pos) = (n==L'$'||wcsvarchr(n))? HIGHLIGHT_OPERATOR:HIGHLIGHT_ERROR;
							colors.at(in_pos+1) = normal_status;								
							break;
						}
                            
                            
						case L'*':
						case L'?':
						case L'(':
						case L')':
						{
							colors.at(in_pos) = HIGHLIGHT_OPERATOR;
							colors.at(in_pos+1) = normal_status;
							break;
						}
                            
						case L'{':
						{
							colors.at(in_pos) = HIGHLIGHT_OPERATOR;
							colors.at(in_pos+1) = normal_status;
							bracket_count++;
							break;					
						}
                            
						case L'}':
						{
							colors.at(in_pos) = HIGHLIGHT_OPERATOR;
							colors.at(in_pos+1) = normal_status;
							bracket_count--;
							break;						
						}
                            
						case L',':
						{
							if( bracket_count )
							{
								colors.at(in_pos) = HIGHLIGHT_OPERATOR;
								colors.at(in_pos+1) = normal_status;
							}
                            
							break;					
						}
                            
						case L'\'':
						{
							colors.at(in_pos) = HIGHLIGHT_QUOTE;
							mode = e_single_quoted;
							break;					
						}
                            
						case L'\"':
						{
							colors.at(in_pos) = HIGHLIGHT_QUOTE;
							mode = e_double_quoted;
							break;
						}
                            
					}
				}		
				break;
			}
                
                /*
                 Mode 1 means single quoted string, i.e 'foo'
                 */
			case e_single_quoted:
			{
				if( c == L'\\' )
				{
					int start_pos = in_pos;
					switch( buff[++in_pos] )
					{
						case '\\':
						case L'\'':
						{
							colors.at(start_pos) = HIGHLIGHT_ESCAPE;
							colors.at(in_pos+1) = HIGHLIGHT_QUOTE;
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
					mode = e_unquoted;
					colors.at(in_pos+1) = normal_status;
				}
				
				break;
			}
                
                /*
                 Mode 2 means double quoted string, i.e. "foo"
                 */
			case e_double_quoted:
			{
				switch( c )
				{
					case '"':
					{
						mode = e_unquoted;
						colors.at(in_pos+1) = normal_status;
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
								colors.at(start_pos) = HIGHLIGHT_ESCAPE;
								colors.at(in_pos+1) = HIGHLIGHT_QUOTE;
								break;
							}
						}
						break;
					}
                        
					case '$':
					{
						wchar_t n = buff[in_pos+1];
						colors.at(in_pos) = (n==L'$'||wcsvarchr(n))? HIGHLIGHT_OPERATOR:HIGHLIGHT_ERROR;
						colors.at(in_pos+1) = HIGHLIGHT_QUOTE;								
						break;
					}
                        
				}						
				break;
			}
		}
	}
}

static int has_expand_reserved( const wchar_t *str )
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

/* Attempts to suggest a completion for a command we handle specially, like 'cd'. Returns true if we recognized the command (even if we couldn't think of a suggestion for it) */
bool autosuggest_suggest_special(const wcstring &str, const wcstring &working_directory, wcstring &outString) {
    if (str.empty())
        return false;
    
    wcstring cmd;
    bool had_cmd = false, recognized_cmd = false;
    
    wcstring suggestion;
    
    tokenizer tok;
	for( tok_init( &tok, str.c_str(), TOK_SQUASH_ERRORS );
		tok_has_next( &tok );
		tok_next( &tok ) )
	{	
        int last_type = tok_last_type( &tok );
		
		switch( last_type )
		{
			case TOK_STRING:
			{
				if( had_cmd )
				{
                    recognized_cmd = (cmd == L"cd");
					if( recognized_cmd )
					{
                        wcstring dir = tok_last( &tok );
                        wcstring suggested_path;
                        if (is_potential_path(dir, &suggested_path, true /* require directory */)) {
                            /* suggested_path needs to actually have dir as a prefix (perhaps with different case). Handle stuff like ./ */
                            bool wants_dot_slash = string_prefixes_string(L"./", dir);
                            bool has_dot_slash = string_prefixes_string(L"./", suggested_path);
                            
                            if (wants_dot_slash && ! has_dot_slash) {
                                suggested_path.insert(0, L"./");
                            } else if (! wants_dot_slash && has_dot_slash) {
                                suggested_path.erase(0, 2);
                            }
                            
                            suggestion = str;
                            suggestion.erase(tok_get_pos(&tok));
                            suggestion.append(suggested_path);
                        }
					}
				}
				else
				{ 	
					/*
					 Command. First check that the command actually exists.
					 */
                    cmd = tok_last( &tok );
                    bool expanded = expand_one(cmd, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES);
					if (! expanded || has_expand_reserved(cmd.c_str()))
					{

					}
					else
					{
						int is_subcommand = 0;
						int mark = tok_get_pos( &tok );
						
						if( parser_keywords_is_subcommand( cmd ) )
						{
							
							int sw;
							
							if( cmd == L"builtin")
							{
							}
							else if( cmd == L"command")
							{
							}
							
							tok_next( &tok );
							
							sw = parser_keywords_is_switch( tok_last( &tok ) );
							
							if( !parser_keywords_is_block( cmd ) &&
							   sw == ARG_SWITCH )
							{

							}
							else
							{
								if( sw == ARG_SKIP )
								{
									mark = tok_get_pos( &tok );
								}
								
								is_subcommand = 1;
							}
							tok_set_pos( &tok, mark );
						}
						
						if( !is_subcommand )
						{	
							had_cmd = true;
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
					break;
				}
				tok_next( &tok );				
                break;
			}
				
			case TOK_PIPE:
			case TOK_BACKGROUND:
			{
				had_cmd = false;
				break;
			}
				
			case TOK_END:
			{
				had_cmd = false;
				break;
			}
				
			case TOK_COMMENT:
			{
				break;
			}
				
			case TOK_ERROR:
			default:
			{
				break;				
			}			
		}
    }
    tok_destroy( &tok );
    
    if (recognized_cmd) {
        outString.swap(suggestion);
    }
    
    return recognized_cmd;
}

bool autosuggest_handle_special(const wcstring &str, const wcstring &working_directory, bool *outSuggestionOK) {
    ASSERT_IS_BACKGROUND_THREAD();
    assert(outSuggestionOK != NULL);
    
    if (str.empty())
        return false;
    
    wcstring cmd;
    bool had_cmd = false;
    
    bool handled = false;
    bool suggestionOK = true;
    
    tokenizer tok;
	for( tok_init( &tok, str.c_str(), TOK_SQUASH_ERRORS );
		tok_has_next( &tok );
		tok_next( &tok ) )
	{	
        int last_type = tok_last_type( &tok );
		
		switch( last_type )
		{
			case TOK_STRING:
			{
				if( had_cmd )
				{
					if( cmd == L"cd" )
					{
                        wcstring dir = tok_last( &tok );
                        if (expand_one(dir, EXPAND_SKIP_CMDSUBST))
						{
                            /* We can specially handle the cd command */
                            handled = true;
							bool is_help = string_prefixes_string(dir, L"--help") || string_prefixes_string(dir, L"-h");
                            if (is_help) {
                                suggestionOK = false;
                            } else {
                                wchar_t *path = path_allocate_cdpath(dir.c_str(), working_directory.c_str());
                                if (path == NULL) {
                                    suggestionOK = false;
                                } else if (paths_are_same_file(working_directory, path)) {
                                    /* Don't suggest the working directory as the path! */
                                    suggestionOK = false;
                                } else {
                                    suggestionOK = true;
                                }
                                free(path);
                            }
						}
					}
				}
				else
				{ 	
					/*
					 Command. First check that the command actually exists.
					 */
                    cmd = tok_last( &tok );
                    bool expanded = expand_one(cmd, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES);
					if (! expanded || has_expand_reserved(cmd.c_str()))
					{

					}
					else
					{
						int is_subcommand = 0;
						int mark = tok_get_pos( &tok );
						
						if( parser_keywords_is_subcommand( cmd ) )
						{
							
							int sw;
							
							if( cmd == L"builtin")
							{
							}
							else if( cmd == L"command")
							{
							}
							
							tok_next( &tok );
							
							sw = parser_keywords_is_switch( tok_last( &tok ) );
							
							if( !parser_keywords_is_block( cmd ) &&
							   sw == ARG_SWITCH )
							{

							}
							else
							{
								if( sw == ARG_SKIP )
								{
									mark = tok_get_pos( &tok );
								}
								
								is_subcommand = 1;
							}
							tok_set_pos( &tok, mark );
						}
						
						if( !is_subcommand )
						{	
							had_cmd = true;
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
					break;
				}
				tok_next( &tok );				
                break;
			}
				
			case TOK_PIPE:
			case TOK_BACKGROUND:
			{
				had_cmd = false;
				break;
			}
				
			case TOK_END:
			{
				had_cmd = false;
				break;
			}
				
			case TOK_COMMENT:
			{
				break;
			}
				
			case TOK_ERROR:
			default:
			{
				break;				
			}			
		}
    }
    tok_destroy( &tok );
    
    *outSuggestionOK = suggestionOK;
    return handled;
}

// This function does I/O
static void tokenize( const wchar_t * const buff, std::vector<int> &color, const int pos, wcstring_list_t *error, const env_vars &vars) {
    ASSERT_IS_BACKGROUND_THREAD();
    
	wcstring cmd;    
	int had_cmd=0;
	wcstring last_cmd;
	int len;

	int accept_switches = 1;
	
	int use_function = 1;
	int use_command = 1;
	int use_builtin = 1;
	
	CHECK( buff, );

	len = wcslen(buff);

	if( !len )
		return;

    std::fill(color.begin(), color.end(), -1); 

    tokenizer tok;
	for( tok_init( &tok, buff, TOK_SHOW_COMMENTS | TOK_SQUASH_ERRORS );
		tok_has_next( &tok );
		tok_next( &tok ) )
	{	
		int last_type = tok_last_type( &tok );
		
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
							color.at(tok_get_pos( &tok )) = HIGHLIGHT_PARAM;
						}
						else if( accept_switches )
						{
							if( complete_is_valid_option( last_cmd.c_str(), param, error, false /* no autoload */ ) )
								color.at(tok_get_pos( &tok )) = HIGHLIGHT_PARAM;
							else
								color.at(tok_get_pos( &tok )) = HIGHLIGHT_ERROR;
						}
						else
						{
							color.at(tok_get_pos( &tok )) = HIGHLIGHT_PARAM;
						}
					}
					else
					{
						color.at(tok_get_pos( &tok )) = HIGHLIGHT_PARAM;
					}					
					
					if( cmd == L"cd" )
					{
                        wcstring dir = tok_last( &tok );
                        if (expand_one(dir, EXPAND_SKIP_CMDSUBST))
						{
							int is_help = string_prefixes_string(dir, L"--help") || string_prefixes_string(dir, L"-h");
							if( !is_help && ! path_can_get_cdpath(dir))
							{
                                color.at(tok_get_pos( &tok )) = HIGHLIGHT_ERROR;							
							}
						}
					}
					
                    /* Highlight the parameter. highlight_param wants to write one more color than we have characters (hysterical raisins) so allocate one more in the vector. But don't copy it back. */
                    const wcstring param_str = param;
                    int tok_pos = tok_get_pos(&tok);
                    
                    std::vector<int>::const_iterator where = color.begin() + tok_pos;
                    std::vector<int> subcolors(where, where + param_str.size());
                    subcolors.push_back(-1);                
                    highlight_param(param_str, subcolors, pos-tok_pos, error);
                                        
                    /* Copy the subcolors back into our colors array */
                    std::copy(subcolors.begin(), subcolors.begin() + param_str.size(), color.begin() + tok_pos);
				}
				else
				{ 
					/*
					 Command. First check that the command actually exists.
					 */
                    cmd = tok_last( &tok );
                    bool expanded = expand_one(cmd, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES);
					if (! expanded || has_expand_reserved(cmd.c_str()))
					{
						color.at(tok_get_pos( &tok )) = HIGHLIGHT_ERROR;
					}
					else
					{
						int is_cmd = 0;
						int is_subcommand = 0;
						int mark = tok_get_pos( &tok );
						color.at(tok_get_pos( &tok )) = HIGHLIGHT_COMMAND;
						
						if( parser_keywords_is_subcommand( cmd ) )
						{
							
							int sw;
							
							if( cmd == L"builtin")
							{
								use_function = 0;
								use_command  = 0;
								use_builtin  = 1;
							}
							else if( cmd == L"command")
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
									color.at(tok_get_pos( &tok )) = HIGHLIGHT_PARAM;
									mark = tok_get_pos( &tok );
								}
								
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
							
							/*
							 First check if it is a builtin or
							 function, since we don't have to stat
							 any files for that
							 */
							if( use_builtin )
								is_cmd |= builtin_exists( cmd );
							
							if( use_function )
								is_cmd |= function_exists_no_autoload( cmd, vars );
							
							/*
							 Moving on to expensive tests
							 */
							
							/*
							 Check if this is a regular command
							 */
							if( use_command )
                            {
                                wcstring tmp;
								is_cmd |= !!path_get_path_string( cmd, tmp, vars );
                            }
							
							if( is_cmd )
							{								
								color.at(tok_get_pos( &tok )) = HIGHLIGHT_COMMAND;
							}
							else
							{
								if( error ) {
                                    error->push_back(format_string(L"Unknown command \'%ls\'", cmd.c_str()));
                                }
								color.at(tok_get_pos( &tok )) = (HIGHLIGHT_ERROR);
							}
							had_cmd = 1;
						}
						
						if( had_cmd )
						{
							last_cmd = tok_last( &tok );
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
					color.at(tok_get_pos( &tok )) = HIGHLIGHT_ERROR;
					if( error )
                        error->push_back(L"Redirection without a command");
					break;
				}
				
                wcstring target_str;
				const wchar_t *target=NULL;
				
				color.at(tok_get_pos( &tok )) = HIGHLIGHT_REDIRECTION;
				tok_next( &tok );
				
				/*
				 Check that we are redirecting into a file
				 */
				
				switch( tok_last_type( &tok ) )
				{
					case TOK_STRING:
					{
                        target_str = tok_last( &tok );
                        if (expand_one(target_str, EXPAND_SKIP_CMDSUBST)) {
                            target = target_str.c_str();
                        }
						/*
						 Redirect filename may contain a cmdsubst. 
						 If so, it will be ignored/not flagged.
						 */
					}
						break;
					default:
					{
                        size_t pos = tok_get_pos(&tok);
                        if (pos < color.size()) {
                            color.at(pos) = HIGHLIGHT_ERROR;
                        }
						if( error )
                            error->push_back(L"Invalid redirection");
					}
						
				}
				
				if( target != 0 )
				{
                    wcstring dir = target;
                    size_t slash_idx = dir.find_last_of(L'/');
					struct stat buff;
					/* 
					 If file is in directory other than '.', check
					 that the directory exists.
					 */
					if( slash_idx != wcstring::npos )
					{
						dir.resize(slash_idx);
						if( wstat( dir, &buff ) == -1 )
						{
							color.at(tok_get_pos( &tok )) = HIGHLIGHT_ERROR;
							if( error )
                                error->push_back(format_string(L"Directory \'%ls\' does not exist", dir.c_str()));
							
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
							color.at(tok_get_pos( &tok )) = HIGHLIGHT_ERROR;
							if( error )
                                error->push_back(format_string(L"File \'%ls\' does not exist", target));
						}
					}
					if( last_type == TOK_REDIRECT_NOCLOB )
					{
						if( wstat( target, &buff ) != -1 )
						{
							color.at(tok_get_pos( &tok )) = HIGHLIGHT_ERROR;
							if( error )
                                error->push_back(format_string(L"File \'%ls\' exists", target));
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
					color.at(tok_get_pos( &tok )) = HIGHLIGHT_END;
					had_cmd = 0;
					use_command  = 1;
					use_function = 1;
					use_builtin  = 1;
					accept_switches = 1;
				}
				else
				{
					color.at(tok_get_pos( &tok )) = HIGHLIGHT_ERROR;					
					if( error )
                        error->push_back(L"No job to put in background" );
				}
				
				break;
			}
				
			case TOK_END:
			{
				color.at(tok_get_pos( &tok )) = HIGHLIGHT_END;
				had_cmd = 0;
				use_command  = 1;
				use_function = 1;
				use_builtin  = 1;
				accept_switches = 1;
				break;
			}
				
			case TOK_COMMENT:
			{
				color.at(tok_get_pos( &tok )) = HIGHLIGHT_COMMENT;
				break;
			}
				
			case TOK_ERROR:
			default:
			{
				/*
				 If the tokenizer reports an error, highlight it as such.
				 */
				if( error )
                    error->push_back(tok_last( &tok));
				color.at(tok_get_pos( &tok )) = HIGHLIGHT_ERROR;
				break;				
			}			
		}
	}
    tok_destroy( &tok );
}


// PCA DOES_IO (calls is_potential_path, path_get_path, maybe others)
void highlight_shell( const wcstring &buff, std::vector<int> &color, int pos, wcstring_list_t *error, const env_vars &vars )
{
    ASSERT_IS_BACKGROUND_THREAD();
    
    const size_t length = buff.size();
    assert(buff.size() == color.size());

    
	if( length == 0 )
		return;
	
    std::fill(color.begin(), color.end(), -1);

    /* Tokenize the string */
    tokenize(buff.c_str(), color, pos, error, vars);

	/*
	  Locate and syntax highlight cmdsubsts recursively
	*/

	wchar_t * const subbuff = wcsdup(buff.c_str());
    wchar_t * subpos = subbuff;
	int done=0;
	
	while( 1 )
	{
		wchar_t *begin, *end;
    
		if( parse_util_locate_cmdsubst(subpos, &begin, &end, 1) <= 0)
		{
			break;
		}
		
		if( !*end )
			done=1;
		else
			*end=0;
		
        //our subcolors start at color + (begin-subbuff)+1
        size_t start = begin - subbuff + 1, len = wcslen(begin + 1);
        std::vector<int> subcolors;
        subcolors.resize(len, -1);
        
		highlight_shell( begin+1, subcolors, -1, error, vars );
        
        // insert subcolors
        std::copy(subcolors.begin(), subcolors.end(), color.begin() + start);
        
        // highlight the end of the subcommand
        assert(end >= subbuff);
        if ((size_t)(end - subbuff) < length) {
            color.at(end-subbuff)=HIGHLIGHT_OPERATOR;
        }
		
		if( done )
			break;
		
		subpos = end+1;
	}
    free(subbuff);

	/*
	  The highlighting code only changes the first element when the
	  color changes. This fills in the rest.
	*/
	int last_val=0;
	for( size_t i=0; i < buff.size(); i++ )
	{
		if( color.at(i) >= 0 )
			last_val = color.at(i);
		else
			color.at(i) = last_val;
	}

	/*
	  Color potentially valid paths in a special path color if they
	  are the current token.
      For reasons that I don't yet understand, it's required that pos be allowed to be length (e.g. when backspacing).
	*/
	if( pos >= 0 && (size_t)pos <= length )
	{
		
        const wchar_t *cbuff = buff.c_str();
		const wchar_t *tok_begin, *tok_end;
		parse_util_token_extent( cbuff, pos, &tok_begin, &tok_end, 0, 0 );
		if( tok_begin && tok_end )
		{
            const wcstring token(tok_begin, tok_end-tok_begin);
			if( is_potential_path( token ) )
			{
				for( ptrdiff_t i=tok_begin-cbuff; i < (tok_end-cbuff); i++ )
				{
                    // Don't color HIGHLIGHT_ERROR because it looks dorky. For example, trying to cd into a non-directory would show an underline and also red.
                    if (! (color.at(i) & HIGHLIGHT_ERROR)) {
                        color.at(i) |= HIGHLIGHT_VALID_PATH;
                    }
				}
			}
		}
	}
	

	highlight_universal_internal( buff, color, pos );

	/*
	  Spaces should not be highlighted at all, since it makes cursor look funky in some terminals
	*/
	for( size_t i=0; i < buff.size(); i++ )
	{
		if( iswspace(buff.at(i)) )
		{
			color.at(i)=0;
		}
	}
}



/**
   Perform quote and parenthesis highlighting on the specified string.
*/
static void highlight_universal_internal( const wcstring &buffstr,
										 std::vector<int> &color, 
										 int pos )
{	
    assert(buffstr.size() == color.size());
	if( (pos >= 0) && ((size_t)pos < buffstr.size()) )
	{
		
		/*
		  Highlight matching quotes
		*/
		if( (buffstr.at(pos) == L'\'') || (buffstr.at(pos) == L'\"') )
		{
			std::vector<long> lst;
		
			int level=0;
			wchar_t prev_q=0;
		
            const wchar_t * const buff = buffstr.c_str();
			const wchar_t *str = buff;

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
                            lst.push_back((long)(str-buff));
							prev_q = *str;
						}
						else
						{
							if( prev_q == *str )
							{
								long pos1, pos2;
							
								level--;
                                pos1 = lst.back();
								pos2 = str-buff;
								if( pos1==pos || pos2==pos )
								{
									color.at(pos1)|=HIGHLIGHT_MATCH<<16;
									color.at(pos2)|=HIGHLIGHT_MATCH<<16;
									match_found = 1;
									
								}
								prev_q = *str==L'\"'?L'\'':L'\"';
							}
							else
							{
								level++;
                                lst.push_back((long)(str-buff));
								prev_q = *str;
							}
						}
					
						break;
				}
				if( (*str == L'\0'))
					break;

				str++;
			}
		
			if( !match_found )
				color.at(pos) = HIGHLIGHT_ERROR<<16;
		}

		/*
		  Highlight matching parenthesis
		*/
        const wchar_t c = buffstr.at(pos);
		if( wcschr( L"()[]{}", c ) )
		{
			int step = wcschr(L"({[", c)?1:-1;
			wchar_t dec_char = *(wcschr( L"()[]{}", c ) + step);
			wchar_t inc_char = c;
			int level = 0;
			int match_found=0;			
            for (long i=pos; i >= 0 && (size_t)i < buffstr.size(); i+=step) {
                const wchar_t test_char = buffstr.at(i); 
                if( test_char == inc_char )
					level++;
				if( test_char == dec_char )
					level--;
				if( level == 0 )
				{
					long pos2 = i;
					color.at(pos)|=HIGHLIGHT_MATCH<<16;
					color.at(pos2)|=HIGHLIGHT_MATCH<<16;
					match_found=1;
					break;
				}
			}
			
			if( !match_found )
				color[pos] = HIGHLIGHT_ERROR<<16;
		}
	}
}

void highlight_universal( const wcstring &buff, std::vector<int> &color, int pos, wcstring_list_t *error, const env_vars &vars )
{
    assert(buff.size() == color.size());
    std::fill(color.begin(), color.end(), 0);	
	highlight_universal_internal( buff, color, pos );
}
