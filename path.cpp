#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "env.h"
#include "wutil.h"
#include "halloc.h"
#include "halloc_util.h"
#include "path.h"
#include "expand.h"

/**
   Unexpected error in path_get_path()
*/
#define MISSING_COMMAND_ERR_MSG _( L"Error while searching for command '%ls'" )

bool path_get_path_string(const wcstring &cmd_str, wcstring &output, const env_vars &vars)
{
    const wchar_t * const cmd = cmd_str.c_str();
    int err = ENOENT;
    debug( 3, L"path_get_path_string( '%ls' )", cmd );
    
	if(wcschr( cmd, L'/' ) != 0 )
	{
		if( waccess( cmd, X_OK )==0 )
		{
			struct stat buff;
			if(wstat( cmd, &buff ))
			{
				return false;
			}
			
			if (S_ISREG(buff.st_mode))
            {
                output = cmd_str;
                return true;
            }
            else
			{
				errno = EACCES;
				return false;
			}
		}
		else
		{
			//struct stat buff;
			//wstat( cmd, &buff );
			return false;
		}
		
	}
	else
	{
		const wchar_t *path = vars.get(L"PATH");
		if( path == 0 )
		{
			if( contains( PREFIX L"/bin", L"/bin", L"/usr/bin" ) )
			{
				path = L"/bin" ARRAY_SEP_STR L"/usr/bin";
			}
			else
			{
				path = L"/bin" ARRAY_SEP_STR L"/usr/bin" ARRAY_SEP_STR PREFIX L"/bin";
			}
		}
		
        wcstokenizer tokenizer(path, ARRAY_SEP_STR);
        wcstring new_cmd;
        while (tokenizer.next(new_cmd))
        {
            size_t path_len = new_cmd.size();
            if (path_len == 0) continue;
            
            append_path_component(new_cmd, cmd_str);
			if( waccess( new_cmd.c_str(), X_OK )==0 )
			{
				struct stat buff;
				if( wstat( new_cmd.c_str(), &buff )==-1 )
				{
					if( errno != EACCES )
					{
						wperror( L"stat" );
					}
					continue;
				}
				if( S_ISREG(buff.st_mode) )
				{
					output = new_cmd;
                    return true;
				}
				err = EACCES;
				
			}
			else
			{
				switch( errno )
				{
					case ENOENT:
					case ENAMETOOLONG:
					case EACCES:
					case ENOTDIR:
						break;
					default:
					{
						debug( 1,
                              MISSING_COMMAND_ERR_MSG,
                              new_cmd.c_str() );
						wperror( L"access" );
					}
				}
			}
		}
	}
    
	errno = err;
	return false;

}

wchar_t *path_get_path( void *context, const wchar_t *cmd )
{
	wchar_t *path;

	int err = ENOENT;
	
	CHECK( cmd, 0 );

	debug( 3, L"path_get_path( '%ls' )", cmd );

	if(wcschr( cmd, L'/' ) != 0 )
	{
		if( waccess( cmd, X_OK )==0 )
		{
			struct stat buff;
			if(wstat( cmd, &buff ))
			{
				return 0;
			}
			
			if( S_ISREG(buff.st_mode) )
				return halloc_wcsdup( context, cmd );
			else
			{
				errno = EACCES;
				return 0;
			}
		}
		else
		{
			struct stat buff;
			wstat( cmd, &buff );
			return 0;
		}
		
	}
	else
	{
		path = env_get(L"PATH");
		if( path == 0 )
		{
			if( contains( PREFIX L"/bin", L"/bin", L"/usr/bin" ) )
			{
				path = L"/bin" ARRAY_SEP_STR L"/usr/bin";
			}
			else
			{
				path = L"/bin" ARRAY_SEP_STR L"/usr/bin" ARRAY_SEP_STR PREFIX L"/bin";
			}
		}
		
		/*
		  Allocate string long enough to hold the whole command
		*/
		wchar_t *new_cmd = (wchar_t *)halloc( context, sizeof(wchar_t)*(wcslen(cmd)+wcslen(path)+2) );
		
		/*
		  We tokenize a copy of the path, since strtok modifies
		  its arguments
		*/
		wchar_t *path_cpy = wcsdup( path );
		wchar_t *nxt_path = path;
		wchar_t *state;
			
		if( (new_cmd==0) || (path_cpy==0) )
		{
			DIE_MEM();
		}

		for( nxt_path = wcstok( path_cpy, ARRAY_SEP_STR, &state );
			 nxt_path != 0;
			 nxt_path = wcstok( 0, ARRAY_SEP_STR, &state) )
		{
			int path_len = wcslen( nxt_path );
			wcscpy( new_cmd, nxt_path );
			if( new_cmd[path_len-1] != L'/' )
			{
				new_cmd[path_len++]=L'/';
			}
			wcscpy( &new_cmd[path_len], cmd );
			if( waccess( new_cmd, X_OK )==0 )
			{
				struct stat buff;
				if( wstat( new_cmd, &buff )==-1 )
				{
					if( errno != EACCES )
					{
						wperror( L"stat" );
					}
					continue;
				}
				if( S_ISREG(buff.st_mode) )
				{
					free( path_cpy );
					return new_cmd;
				}
				err = EACCES;
				
			}
			else
			{
				switch( errno )
				{
					case ENOENT:
					case ENAMETOOLONG:
					case EACCES:
					case ENOTDIR:
						break;
					default:
					{
						debug( 1,
							   MISSING_COMMAND_ERR_MSG,
							   new_cmd );
						wperror( L"access" );
					}
				}
			}
		}
		free( path_cpy );

	}

	errno = err;
	return 0;
}


bool path_get_cdpath_string(const wcstring &dir_str, wcstring &result, const env_vars &vars)
{
	wchar_t *res = 0;
	int err = ENOENT;
    bool success = false;
    
    const wchar_t *const dir = dir_str.c_str();
	if( dir[0] == L'/'|| (wcsncmp( dir, L"./", 2 )==0) )
	{
		struct stat buf;
		if( wstat( dir, &buf ) == 0 )
		{
			if( S_ISDIR(buf.st_mode) )
			{
				result = dir_str;
                success = true;
			}
			else
			{
				err = ENOTDIR;
			}
            
		}
	}
	else
	{
		const wchar_t *path = vars.get(L"CDPATH");
		if( !path || !wcslen(path) )
		{
			path = L".";
		}
                
        wcstokenizer tokenizer(path, ARRAY_SEP_STR);
        wcstring next_path;
        while (tokenizer.next(next_path))
        {
            expand_tilde(next_path);
            if (next_path.size() == 0) continue;
            
            wcstring whole_path = next_path;
            append_path_component(whole_path, dir);
            
			struct stat buf;
			if( wstat( whole_path.c_str(), &buf ) == 0 )
			{
				if( S_ISDIR(buf.st_mode) )
				{
                    result = whole_path;
                    success = true;
					break;
				}
				else
				{
					err = ENOTDIR;
				}
			}
			else
			{
				if( lwstat( whole_path.c_str(), &buf ) == 0 )
				{
					err = EROTTEN;
				}
			}
        }
    }
		
    
	if( !success )
	{
		errno = err;
	}
    
	return res;
}

wchar_t *path_get_cdpath( void *context, const wchar_t *dir )
{
	wchar_t *res = 0;
	int err = ENOENT;
	if( !dir )
		return 0;


	if( dir[0] == L'/'|| (wcsncmp( dir, L"./", 2 )==0) )
	{
		struct stat buf;
		if( wstat( dir, &buf ) == 0 )
		{
			if( S_ISDIR(buf.st_mode) )
			{
				res = halloc_wcsdup( context, dir );
			}
			else
			{
				err = ENOTDIR;
			}

		}
	}
	else
	{
		wchar_t *path;
		wchar_t *path_cpy;
		wchar_t *nxt_path;
		wchar_t *state;
		wchar_t *whole_path;

		path = env_get(L"CDPATH");

		if( !path || !wcslen(path) )
		{
			path = L".";
		}

		nxt_path = path;
		path_cpy = wcsdup( path );

		if( !path_cpy )
		{
			DIE_MEM();
		}

		for( nxt_path = wcstok( path_cpy, ARRAY_SEP_STR, &state );
			 nxt_path != 0;
			 nxt_path = wcstok( 0, ARRAY_SEP_STR, &state) )
		{
			wchar_t *expanded_path = expand_tilde_compat( wcsdup(nxt_path) );

//			debug( 2, L"woot %ls\n", expanded_path );

			int path_len = wcslen( expanded_path );
			if( path_len == 0 )
			{
				free(expanded_path );
				continue;
			}

			whole_path =
				wcsdupcat( expanded_path,
							( expanded_path[path_len-1] != L'/' )?L"/":L"",
							dir );

			free(expanded_path );

			struct stat buf;
			if( wstat( whole_path, &buf ) == 0 )
			{
				if( S_ISDIR(buf.st_mode) )
				{
					res = whole_path;
					halloc_register( context, whole_path );					
					break;
				}
				else
				{
					err = ENOTDIR;
				}
			}
			else
			{
				if( lwstat( whole_path, &buf ) == 0 )
				{
					err = EROTTEN;
				}
			}
			
			free( whole_path );
		}
		free( path_cpy );
	}

	if( !res )
	{
		errno = err;
	}

	return res;
}


wchar_t *path_get_config( void *context)
{
	wchar_t *xdg_dir, *home;
	int done = 0;
	wchar_t *res = 0;
	
	xdg_dir = env_get( L"XDG_CONFIG_HOME" );
	if( xdg_dir )
	{
		res = wcsdupcat( xdg_dir, L"/fish" );
		if( !create_directory( res ) )
		{
			done = 1;
		}
		else
		{
			free( res );
		}
		
	}
	else
	{		
		home = env_get( L"HOME" );
		if( home )
		{
			res = wcsdupcat( home, L"/.config/fish" );
			if( !create_directory( res ) )
			{
				done = 1;
			}
			else
			{
				free( res );
			}
		}
	}
	
	if( done )
	{
		halloc_register_function( context, &free, res );
		return res;
	}
	else
	{
		debug( 0, _(L"Unable to create a configuration directory for fish. Your personal settings will not be saved. Please set the $XDG_CONFIG_HOME variable to a directory where the current user has write access." ));
		return 0;
	}
	
}

wchar_t *path_make_canonical( void *context, const wchar_t *path )
{
	wchar_t *res = halloc_wcsdup( context, path );
	wchar_t *in, *out;
	
	in = out = res;
	
	while( *in )
	{
		if( *in == L'/' )
		{
			while( *(in+1) == L'/' )
			{
				in++;
			}
		}
		*out = *in;
	
		out++;
		in++;
	}

	while( 1 )
	{
		if( out == res )
			break;
		if( *(out-1) != L'/' )
			break;
		out--;
	}
	*out = 0;
		
	return res;
}

