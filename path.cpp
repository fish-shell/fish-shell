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

wchar_t *path_get_path( const wchar_t *cmd )
{
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
				return wcsdup( cmd );
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
		env_var_t path = env_get_string(L"PATH");
		if( path.missing() )
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
		wchar_t *new_cmd = (wchar_t *)calloc(wcslen(cmd)+path.size()+2, sizeof(wchar_t) );
		
		/*
		  We tokenize a copy of the path, since strtok modifies
		  its arguments
		*/
		wchar_t *path_cpy = wcsdup( path.c_str() );
		const wchar_t *nxt_path = path.c_str();
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

bool path_get_path_string(const wcstring &cmd, wcstring &output)
{
    bool success = false;
    wchar_t *tmp = path_get_path(cmd.c_str());
    if (tmp) {
        output = tmp;
        free(tmp);
        success = true;
    }
    return success;
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

		const wchar_t *path = L".";
                
        wcstokenizer tokenizer(path, ARRAY_SEP_STR);
        wcstring next_path;
        while (tokenizer.next(next_path))
        {
            expand_tilde(next_path);
            if (next_path.size() == 0) continue;
            
            wcstring whole_path = next_path;
            append_path_component(whole_path, dir);
            
			struct stat buf;
			if( wstat( whole_path, &buf ) == 0 )
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
				if( lwstat( whole_path, &buf ) == 0 )
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


wchar_t *path_allocate_cdpath( const wchar_t *dir, const wchar_t *wd )
{
	wchar_t *res = NULL;
	int err = ENOENT;
	if( !dir )
		return 0;
        
    if (wd) {
        size_t len = wcslen(wd);
        assert(wd[len - 1] == L'/');
    }
    
    wcstring_list_t paths;
    
    if (dir[0] == L'/') {
        /* Absolute path */
        paths.push_back(dir);
    } else if (wcsncmp(dir, L"./", 2) == 0 || 
               wcsncmp(dir, L"../", 3) == 0 ||
               wcscmp(dir, L".") == 0 ||
               wcscmp(dir, L"..") == 0) {
        /* Path is relative to the working directory */
        wcstring path;
        if (wd)
            path.append(wd);
        path.append(dir);
        paths.push_back(path);
    } else {
        wchar_t *path_cpy;
        const wchar_t *nxt_path;
        wchar_t *state;
        wchar_t *whole_path;

        env_var_t path = env_get_string(L"CDPATH");
        if (path.missing())
            path = wd ? wd : L".";

        nxt_path = path.c_str();
        path_cpy = wcsdup( path.c_str() );

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
            paths.push_back(whole_path);
            free( whole_path );
        }
        free( path_cpy );
    }
    
    for (wcstring_list_t::const_iterator iter = paths.begin(); iter != paths.end(); iter++) {
		struct stat buf;
        const wcstring &dir = *iter;
		if( wstat( dir, &buf ) == 0 )
		{
			if( S_ISDIR(buf.st_mode) )
			{
				res = wcsdup(dir.c_str());
                break;
			}
			else
			{
				err = ENOTDIR;
			}
		}
    }
     
    if( !res )
	{
		errno = err;
	}

	return res;
}


bool path_can_get_cdpath(const wcstring &in, const wchar_t *wd) {
    wchar_t *tmp = path_allocate_cdpath(in.c_str(), wd);
    bool result = (tmp != NULL);
    free(tmp);
    return result;
}


bool path_get_config(wcstring &path)
{
	int done = 0;
	wcstring res;
	
	const env_var_t xdg_dir  = env_get_string( L"XDG_CONFIG_HOME" );
	if( ! xdg_dir.missing() )
	{
		res = xdg_dir + L"/fish";
		if( !create_directory( res ) )
		{
			done = 1;
		}
	}
	else
	{		
		const env_var_t home = env_get_string( L"HOME" );
		if( ! home.missing() )
		{
			res = home + L"/.config/fish";
			if( !create_directory( res ) )
			{
				done = 1;
			}
		}
	}
	
	if( done )
	{
        path = res;
        return true;
	}
	else
	{
		debug( 0, _(L"Unable to create a configuration directory for fish. Your personal settings will not be saved. Please set the $XDG_CONFIG_HOME variable to a directory where the current user has write access." ));
		return false;
	}	

}

static void replace_all(wcstring &str, const wchar_t *needle, const wchar_t *replacement)
{
    size_t needle_len = wcslen(needle);
    size_t offset = 0;
    while((offset = str.find(needle, offset)) != wcstring::npos)
    {
        str.replace(offset, needle_len, replacement);
        offset += needle_len;
    }
}

void path_make_canonical( wcstring &path )
{

    /* Remove double slashes */
    size_t size;
    do {
        size = path.size();
        replace_all(path, L"//", L"/");
    } while (path.size() != size);
    
    /* Remove trailing slashes */
    while (size--) {
        if (path.at(size) != L'/')
            break;
    }
    /* Now size is either -1 (if the entire string was slashes) or is the index of the last non-slash character. Either way this will set it to the correct size. */
    path.resize(size+1);
}

bool path_is_valid(const wcstring &path, const wcstring &working_directory)
{
    bool path_is_valid;
    /* Some special paths are always valid */
    if (path.empty()) {
        path_is_valid = false;
    } else if (path == L"." || path == L"./") {
        path_is_valid = true;
    } else if (path == L".." || path == L"../") {
        path_is_valid = (! working_directory.empty() && working_directory != L"/");
    } else if (path.at(0) != '/') {
        /* Prepend the working directory. Note that we know path is not empty here. */
        wcstring tmp = working_directory;
        tmp.append(path);
        path_is_valid = (0 == waccess(tmp.c_str(), F_OK));
    } else {
        /* Simple check */
        path_is_valid = (0 == waccess(path.c_str(), F_OK));
    }
    return path_is_valid;
}

bool paths_are_same_file(const wcstring &path1, const wcstring &path2) {
    if (path1 == path2)
        return true;
        
    struct stat s1, s2;
    if (wstat(path1, &s1) == 0 && wstat(path2, &s2) == 0) {
        return s1.st_ino == s2.st_ino && s1.st_dev == s2.st_dev;
    } else {
        return false;
    }
}

wcstring get_working_directory(void) {
    wcstring wd = L"./";
    wchar_t dir_path[4096];
    const wchar_t *cwd = wgetcwd( dir_path, 4096 );
    if (cwd) {
        wd = cwd;
        /* Make sure the working directory ends with a slash */
        if (! wd.empty() && wd.at(wd.size() - 1) != L'/')
            wd.push_back(L'/');
    }
    return wd;
}
