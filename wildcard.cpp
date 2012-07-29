/** \file wildcard.c

Fish needs it's own globbing implementation to support
tab-expansion of globbed parameters. Also provides recursive
wildcards using **.

*/

#include "config.h"
#include <algorithm>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <wchar.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>


#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "complete.h"
#include "common.h"
#include "wildcard.h"
#include "complete.h"
#include "reader.h"
#include "expand.h"
#include "exec.h"
#include <map>

/**
   This flag is set in the flags parameter of wildcard_expand if the
   call is part of a recursiv wildcard search. It is used to make sure
   that the contents of subdirectories are only searched once.
*/
#define WILDCARD_RECURSIVE 64

/**
   The maximum length of a filename token. This is a fallback value,
   an attempt to find the true value using patchconf is always made. 
*/
#define MAX_FILE_LENGTH 1024

/**
   The command to run to get a description from a file suffix
*/
#define SUFFIX_CMD_STR L"mimedb 2>/dev/null -fd "

/**
   Description for generic executable
*/
#define COMPLETE_EXEC_DESC _( L"Executable" )
/**
   Description for link to executable
*/
#define COMPLETE_EXEC_LINK_DESC _( L"Executable link" )

/**
   Description for regular file
*/
#define COMPLETE_FILE_DESC _( L"File" )
/**
   Description for character device
*/
#define COMPLETE_CHAR_DESC _( L"Character device" )
/**
   Description for block device
*/
#define COMPLETE_BLOCK_DESC _( L"Block device" )
/**
   Description for fifo buffer
*/
#define COMPLETE_FIFO_DESC _( L"Fifo" )
/**
   Description for symlink
*/
#define COMPLETE_SYMLINK_DESC _( L"Symbolic link" )
/**
   Description for symlink
*/
#define COMPLETE_DIRECTORY_SYMLINK_DESC _( L"Symbolic link to directory" )
/**
   Description for Rotten symlink
*/
#define COMPLETE_ROTTEN_SYMLINK_DESC _( L"Rotten symbolic link" )
/**
   Description for symlink loop
*/
#define COMPLETE_LOOP_SYMLINK_DESC _( L"Symbolic link loop" )
/**
   Description for socket files
*/
#define COMPLETE_SOCKET_DESC _( L"Socket" )
/**
   Description for directories
*/
#define COMPLETE_DIRECTORY_DESC _( L"Directory" )

/** Hashtable containing all descriptions that describe an executable */
static std::map<wcstring, wcstring> suffix_map;


int wildcard_has( const wchar_t *str, int internal )
{
	if( !str )
	{
		debug( 2, L"Got null string on line %d of file %s", __LINE__, __FILE__ );
		return 0;		
	}

	if( internal )
	{
		for( ; *str; str++ )
		{
			if( ( *str == ANY_CHAR ) || (*str == ANY_STRING) || (*str == ANY_STRING_RECURSIVE) )
				return 1;
		}
	}
	else
	{
        wchar_t prev=0;
		for( ; *str; str++ )
		{
			if( ( (*str == L'*' ) || (*str == L'?' ) ) && (prev != L'\\') )
				return 1;
			prev = *str;
		}
	}
	
	return 0;
}

/**
   Check whether the string str matches the wildcard string wc.
  
   \param str String to be matched.
   \param wc The wildcard.
   \param is_first Whether files beginning with dots should not be matched against wildcards. 
*/
static int wildcard_match2( const wchar_t *str, 
							const wchar_t *wc, 
							int is_first )
{
	if( *str == 0 && *wc==0 )
		return 1;
	
	if( *wc == ANY_STRING || *wc == ANY_STRING_RECURSIVE)
	{		
		/* Ignore hidden file */
		if( is_first && *str == L'.' )
		{
			return 0;
		}
		
		/* Try all submatches */
		do
		{
			if( wildcard_match2( str, wc+1, 0 ) )
				return 1;
		}
		while( *(str++) != 0 );
		return 0;
	}
	else if( *str == 0 )
	{
		/*
		  End of string, but not end of wildcard, and the next wildcard
		  element is not a '*', so this is not a match.
		*/
		return 0;
	}

	if( *wc == ANY_CHAR )
	{
		if( is_first && *str == L'.' )
		{
			return 0;
		}
		
		return wildcard_match2( str+1, wc+1, 0 );
	}
	
	if( *wc == *str )
		return wildcard_match2( str+1, wc+1, 0 );

	return 0;
}

/**
   Matches the string against the wildcard, and if the wildcard is a
   possible completion of the string, the remainder of the string is
   inserted into the out vector.
*/
static bool wildcard_complete_internal(const wcstring &orig, 
									   const wchar_t *str, 
									   const wchar_t *wc, 
									   bool is_first,
									   const wchar_t *desc,
									   wcstring (*desc_func)(const wcstring &),
									   std::vector<completion_t> &out,
									   int flags )
{
	if( !wc || ! str || orig.empty())
	{
		debug( 2, L"Got null string on line %d of file %s", __LINE__, __FILE__ );
		return 0;		
	}

	if( *wc == 0 &&
		( (str[0] != L'.') || (!is_first)) )
	{
		wcstring out_completion;
        wcstring out_desc = (desc ? desc : L"");

		if( flags & COMPLETE_NO_CASE )
		{
			out_completion = orig;
		}
		else
		{
			out_completion = str;
		}

        size_t complete_sep_loc = out_completion.find(PROG_COMPLETE_SEP);
        if (complete_sep_loc != wcstring::npos)
		{
			/* This completion has an embedded description, do not use the generic description */
            out_desc.assign(out_completion, complete_sep_loc + 1, out_completion.size() - complete_sep_loc - 1);
            out_completion.resize(complete_sep_loc);			
		}
		else
		{
			if( desc_func )
			{
				/*
				  A description generating function is specified, call
				  it. If it returns something, use that as the
				  description.
				*/
				wcstring func_desc = desc_func( orig );
				if (! func_desc.empty())
					out_desc = func_desc;
			}
			
		}
		
		if (! out_completion.empty())
		{
			append_completion( out, 
								 out_completion,
								 out_desc,
								 flags );
		}		
		return true;
	}
	
	
	if( *wc == ANY_STRING )
	{		
		bool res=false;
		
		/* Ignore hidden file */
		if( is_first && str[0] == L'.' )
			return false;
		
		/* Try all submatches */
		do
		{
			res = wildcard_complete_internal( orig, str, wc+1, 0, desc, desc_func, out, flags );
			if (res)
				break;
		}
		while (*str++ != 0);
		return res;
		
	}
	else if( *wc == ANY_CHAR )
	{
		return wildcard_complete_internal( orig, str+1, wc+1, 0, desc, desc_func, out, flags );
	}	
	else if( *wc == *str )
	{
		return wildcard_complete_internal( orig, str+1, wc+1, 0, desc, desc_func, out, flags );
	}
	else if( towlower(*wc) == towlower(*str) )
	{
		return wildcard_complete_internal( orig, str+1, wc+1, 0, desc, desc_func, out, flags | COMPLETE_NO_CASE );
	}
	return false;	
}

bool wildcard_complete(const wcstring &str,
					   const wchar_t *wc,
					   const wchar_t *desc,						
					   wcstring (*desc_func)(const wcstring &),
					   std::vector<completion_t> &out,
					   int flags )
{
	bool res;
	res =  wildcard_complete_internal( str, str.c_str(), wc, true, desc, desc_func, out, flags );	
	return res;
}


int wildcard_match( const wcstring &str, const wcstring &wc )
{
	return wildcard_match2( str.c_str(), wc.c_str(), 1 );	
}

/**
   Creates a path from the specified directory and filename. 
*/
static wcstring make_path(const wcstring &base_dir, const wcstring &name) {
    return base_dir + name;
}

/**
   Return a description of a file based on its suffix. This function
   does not perform any caching, it directly calls the mimedb command
   to do a lookup.
 */
static wcstring complete_get_desc_suffix_internal( const wcstring &suff )
{

    wcstring cmd = wcstring(SUFFIX_CMD_STR) + suff;
    
    wcstring_list_t lst;
    wcstring desc;
	
	if( exec_subshell( cmd, lst ) != -1 )
	{
		if( lst.size()>0 )
		{
            const wcstring & ln = lst.at(0);
			if( ln.size() > 0 && ln != L"unknown" )
			{
				desc = ln;
				/*
				  I have decided I prefer to have the description
				  begin in uppercase and the whole universe will just
				  have to accept it. Hah!
				*/
				desc[0]=towupper(desc[0]);
			}
		}
	}
	
	if( desc.empty() )
	{
		desc = COMPLETE_FILE_DESC;
	}
	
    suffix_map[suff] = desc.c_str();
	return desc;
}


/**
   Use the mimedb command to look up a description for a given suffix
*/
static wcstring complete_get_desc_suffix( const wchar_t *suff_orig )
{

	size_t len;
	wchar_t *suff;
	wchar_t *pos;
	wchar_t *tmp;

	len = wcslen(suff_orig );
	
	if( len == 0 )
		return COMPLETE_FILE_DESC;

	suff = wcsdup(suff_orig);

	/*
	  Drop characters that are commonly used as backup suffixes from the suffix
	*/
	for( pos=suff; *pos; pos++ )
	{
		if( wcschr( L"?;#~@&", *pos ) )
		{
			*pos=0;
			break;
		}
	}
    
	tmp = escape( suff, 1 );
	free(suff);
	suff = tmp;
    
    std::map<wcstring, wcstring>::iterator iter = suffix_map.find(suff);
    wcstring desc;
    if (iter != suffix_map.end()) {
        desc = iter->second;
    } else {
        desc = complete_get_desc_suffix_internal( suff );
    }

	free( suff );

	return desc;
}


/**
   Obtain a description string for the file specified by the filename.

   The returned value is a string constant and should not be free'd.

   \param filename The file for which to find a description string
   \param lstat_res The result of calling lstat on the file
   \param lbuf The struct buf output of calling lstat on the file
   \param stat_res The result of calling stat on the file
   \param buf The struct buf output of calling stat on the file
   \param err The errno value after a failed stat call on the file. 
*/

static wcstring file_get_desc( const wcstring &filename, 
									 int lstat_res,
									 struct stat lbuf, 
									 int stat_res, 
									 struct stat buf, 
									 int err )
{
	const wchar_t *suffix;

	if( !lstat_res )
	{
		if( S_ISLNK(lbuf.st_mode))
		{
			if( !stat_res )
			{
				if( S_ISDIR(buf.st_mode) )
				{
					return COMPLETE_DIRECTORY_SYMLINK_DESC;
				}
				else
				{

					if( ( buf.st_mode & S_IXUSR ) ||
						( buf.st_mode & S_IXGRP ) ||
						( buf.st_mode & S_IXOTH ) )
					{
		
						if( waccess( filename, X_OK ) == 0 )
						{
							/*
							  Weird group permissions and other such
							  issues make it non-trivial to find out
							  if we can actually execute a file using
							  the result from stat. It is much safer
							  to use the access function, since it
							  tells us exactly what we want to know.
							*/
							return COMPLETE_EXEC_LINK_DESC;
						}
					}
				}
				
				return COMPLETE_SYMLINK_DESC;

			}
			else
			{
				switch( err )
				{
					case ENOENT:
					{
						return COMPLETE_ROTTEN_SYMLINK_DESC;
					}
					
					case ELOOP:
					{
						return COMPLETE_LOOP_SYMLINK_DESC;
					}
				}
				/*
				  On unknown errors we do nothing. The file will be
				  given the default 'File' description or one based on the suffix.
				*/
			}

		}
		else if( S_ISCHR(buf.st_mode) )
		{
			return COMPLETE_CHAR_DESC;
		}
		else if( S_ISBLK(buf.st_mode) )
		{
			return COMPLETE_BLOCK_DESC;
		}
		else if( S_ISFIFO(buf.st_mode) )
		{
			return COMPLETE_FIFO_DESC;
		}
		else if( S_ISSOCK(buf.st_mode))
		{
			return COMPLETE_SOCKET_DESC;
		}
		else if( S_ISDIR(buf.st_mode) )
		{
			return COMPLETE_DIRECTORY_DESC;
		}
		else 
		{
			if( ( buf.st_mode & S_IXUSR ) ||
				( buf.st_mode & S_IXGRP ) ||
				( buf.st_mode & S_IXOTH ) )
			{
		
				if( waccess( filename, X_OK ) == 0 )
				{
					/*
					  Weird group permissions and other such issues
					  make it non-trivial to find out if we can
					  actually execute a file using the result from
					  stat. It is much safer to use the access
					  function, since it tells us exactly what we want
					  to know.
					*/
					return COMPLETE_EXEC_DESC;
				}
			}
		}
	}
	
	suffix = wcsrchr( filename.c_str(), L'.' );
	if( suffix != 0 && !wcsrchr( suffix, L'/' ) )
	{
		return complete_get_desc_suffix( suffix );
	}
	
	return COMPLETE_FILE_DESC ;
}


/**
   Add the specified filename if it matches the specified wildcard. 

   If the filename matches, first get the description of the specified
   filename. If this is a regular file, append the filesize to the
   description.

   \param list the list to add he completion to
   \param fullname the full filename of the file
   \param completion the completion part of the file name
   \param wc the wildcard to match against
   \param is_cmd whether we are performing command completion
*/
static void wildcard_completion_allocate( std::vector<completion_t> &list, 
					  const wcstring &fullname, 
					  const wcstring &completion,
					  const wchar_t *wc,
                      expand_flags_t expand_flags)
{
	struct stat buf, lbuf;
    wcstring sb;
	wcstring munged_completion;
    
	int flags = 0;
	int stat_res, lstat_res;
	int stat_errno=0;
	
	long long sz; 

	/*
	  If the file is a symlink, we need to stat both the file itself
	  _and_ the destination file. But we try to avoid this with
	  non-symlinks by first doing an lstat, and if the file is not a
	  link we copy the results over to the regular stat buffer.
	*/
	if( ( lstat_res = lwstat( fullname, &lbuf ) ) )
	{
        /* lstat failed! */
		sz=-1;
		stat_res = lstat_res;
	}
	else
	{
		if (S_ISLNK(lbuf.st_mode))
		{
			
			if( ( stat_res = wstat( fullname, &buf ) ) )
			{
				sz=-1;
			}
			else
			{
				sz = (long long)buf.st_size;
			}
			
			/*
			  In order to differentiate between e.g. rotten symlinks
			  and symlink loops, we also need to know the error status of wstat.
			*/
			stat_errno = errno;
		}
		else
		{
			stat_res = lstat_res;
			memcpy( &buf, &lbuf, sizeof( struct stat ) );
			sz = (long long)buf.st_size;
		}
	}
	
	wcstring desc;
    if (! (expand_flags & EXPAND_NO_DESCRIPTIONS))
        desc = file_get_desc( fullname.c_str(), lstat_res, lbuf, stat_res, buf, stat_errno );
    
	if( sz >= 0 && S_ISDIR(buf.st_mode) )
	{
		flags = flags | COMPLETE_NO_SPACE;
        munged_completion = completion;
        munged_completion.push_back(L'/');
        sb.append(desc);
	}
	else
	{
        sb.append(desc);
        sb.append(L", ");
        sb.append(format_size(sz));
	}
    
    const wcstring &completion_to_use = munged_completion.empty() ? completion : munged_completion;
	wildcard_complete(completion_to_use, wc, sb.c_str(), NULL, list, flags);
}

/**
  Test if the file specified by the given filename matches the
  expansion flags specified. flags can be a combination of
  EXECUTABLES_ONLY and DIRECTORIES_ONLY.
*/
static int test_flags( const wchar_t *filename,
					   int flags )
{
	if( flags & DIRECTORIES_ONLY )
	{
		struct stat buf;
		if( wstat( filename, &buf ) == -1 )
		{
			return 0;
		}

		if( !S_ISDIR( buf.st_mode ) )
		{
			return 0;
		}
	}
	
	
	if( flags & EXECUTABLES_ONLY )
	{
		if ( waccess( filename, X_OK ) != 0)
			return 0;
	}
	
	return 1;
}

/**
   The real implementation of wildcard expansion is in this
   function. Other functions are just wrappers around this one.

   This function traverses the relevant directory tree looking for
   matches, and recurses when needed to handle wildcrards spanning
   multiple components and recursive wildcards. 
 */
static int wildcard_expand_internal( const wchar_t *wc, 
									 const wchar_t *base_dir,
									 expand_flags_t flags,
									 std::vector<completion_t> &out )
{
	
	/* Points to the end of the current wildcard segment */
	const wchar_t *wc_end;

	/* Variables for traversing a directory */
	DIR *dir;
	
	/* The result returned */
	int res = 0;
	
	/* Length of the directory to search in */
	size_t base_len;

	/* Variables for testing for presense of recursive wildcards */
	const wchar_t *wc_recursive;
	int is_recursive;

	/* Sligtly mangled version of base_dir */
	const wchar_t *dir_string;
	
	//	debug( 3, L"WILDCARD_EXPAND %ls in %ls", wc, base_dir );

	if( reader_interrupted() )
	{
		return -1;
	}
	
	if( !wc || !base_dir )
	{
		debug( 2, L"Got null string on line %d of file %s", __LINE__, __FILE__ );
		return 0;		
	}

	if( flags & ACCEPT_INCOMPLETE )
	{	
		/* 
		   Avoid excessive number of returned matches for wc ending with a * 
		*/
		size_t len = wcslen(wc);
		if( len && (wc[len-1]==ANY_STRING) )
		{
			wchar_t * foo = wcsdup( wc );
			foo[len-1]=0;
			int res = wildcard_expand_internal( foo, base_dir, flags, out );
			free( foo );
			return res;			
		}
	}

	/*
	  Initialize various variables
	*/

	dir_string = base_dir[0]==L'\0'?L".":base_dir;
	
	if( !(dir = wopendir( dir_string )))
	{
		return 0;
	}

	wc_end = wcschr(wc,L'/');
	base_len = wcslen( base_dir );

	/*
	  Test for recursive match string in current segment
	*/	
	wc_recursive = wcschr( wc, ANY_STRING_RECURSIVE );
	is_recursive = ( wc_recursive && (!wc_end || wc_recursive < wc_end));

	/*
	  Is this segment of the wildcard the last?
	*/
	if( !wc_end )
	{
		/*
		  Wildcard segment is the last segment,

		  Insert all matching files/directories
		*/
		if( wc[0]=='\0' )
		{
			/*
			  The last wildcard segment is empty. Insert everything if
			  completing, the directory itself otherwise.
			*/
			if( flags & ACCEPT_INCOMPLETE )
			{
                wcstring next;
				while(wreaddir(dir, next))
				{
					if( next[0] != L'.' )
					{
						wcstring long_name = make_path( base_dir, next );
						
						if( test_flags( long_name.c_str(), flags ) )
						{
							wildcard_completion_allocate( out,
														  long_name,
														  next,
														  L"",
                                                          flags);
						}
					}					
				}
			}
			else
			{								
				res = 1;
				completion_t data_to_push(base_dir);
				if (std::find( out.begin(), out.end(), data_to_push ) == out.end()) {
					 out.push_back(data_to_push);
				}
			}							
		}
		else
		{
			/*
			  This is the last wildcard segment, and it is not empty. Match files/directories.
			*/
            wcstring next;
			while (wreaddir(dir, next))
			{
                const wchar_t * const name = next.c_str();
				if( flags & ACCEPT_INCOMPLETE )
				{
					
					const wcstring long_name = make_path( base_dir, next );

					/*
					  Test for matches before stating file, so as to minimize the number of calls to the much slower stat function 
					*/
					std::vector<completion_t> test;
					if( wildcard_complete( name,
										   wc,
										   L"",
										   0,
										   test,
										   0 ) )
					{
						if( test_flags( long_name.c_str(), flags ) )
						{
							wildcard_completion_allocate( out,
														  long_name,
                                                          name,
														  wc,
                                                          flags);
							
						}
					}					
				}
				else
				{
					if( wildcard_match2( name, wc, 1 ) )
					{
                        const wcstring long_name = make_path(base_dir, next);
						int skip = 0;
						
						if( is_recursive )
						{
							/*
							  In recursive mode, we are only
							  interested in adding files -directories
							  will be added in the next pass.
							*/
							struct stat buf;
							if( !wstat( long_name, &buf ) )
							{
								skip = S_ISDIR(buf.st_mode);
							}							
						}
						if (! skip)
						{
							out.push_back(completion_t(long_name) );
						}
						res = 1;
					}
				}
			}
		}
	}

	if( wc_end || is_recursive )
	{
		/*
		  Wilcard segment is not the last segment.  Recursively call
		  wildcard_expand for all matching subdirectories.
		*/
		
		/*
		  wc_str is the part of the wildcarded string from the
		  beginning to the first slash
		*/
		wchar_t *wc_str;

		/*
		  new_dir is a scratch area containing the full path to a
		  file/directory we are iterating over
		*/
		wchar_t *new_dir;

		/*
		  The maximum length of a file element
		*/
		long ln=MAX_FILE_LENGTH;
		char * narrow_dir_string = wcs2str( dir_string );

		/*
		  In recursive mode, we look through the direcotry twice. If
		  so, this rewind is needed.
		*/
		rewinddir( dir );

		if( narrow_dir_string )
		{
			/* 
			   Find out how long the filename can be in a worst case
			   scenario
			*/
			ln = pathconf( narrow_dir_string, _PC_NAME_MAX ); 

			/*
			  If not specified, use som large number as fallback
			*/
			if( ln < 0 )
				ln = MAX_FILE_LENGTH;		
			free( narrow_dir_string );
		}
		new_dir= (wchar_t *)malloc( sizeof(wchar_t)*(base_len+ln+2)  );

		wc_str = wc_end?wcsndup(wc, wc_end-wc):wcsdup(wc);

		if( (!new_dir) || (!wc_str) )
		{
			DIE_MEM();
		}

		wcscpy( new_dir, base_dir );
		
        wcstring next;
		while (wreaddir(dir, next))
		{
			const wchar_t *name = next.c_str();
			
			/*
			  Test if the file/directory name matches the whole
			  wildcard element, i.e. regular matching.
			*/
			int whole_match = wildcard_match2( name, wc_str, 1 );
			int partial_match = 0;
			
			/* 
			   If we are doing recursive matching, also check if this
			   directory matches the part up to the recusrive
			   wildcard, if so, then we can search all subdirectories
			   for matches.
			*/
			if( is_recursive )
			{
				const wchar_t *end = wcschr( wc, ANY_STRING_RECURSIVE );
				wchar_t *wc_sub = wcsndup( wc, end-wc+1);
				partial_match = wildcard_match2( name, wc_sub, 1 );
				free( wc_sub );
			}			

			if( whole_match || partial_match )
			{
				struct stat buf;			
				char *dir_str;
				int stat_res;
				int new_res;

				wcscpy(&new_dir[base_len], name );
				dir_str = wcs2str( new_dir );
				
				if( dir_str )
				{
					stat_res = stat( dir_str, &buf );
					free( dir_str );
					
					if( !stat_res )
					{
						if( S_ISDIR(buf.st_mode) )
						{
							size_t new_len = wcslen( new_dir );
							new_dir[new_len] = L'/';
							new_dir[new_len+1] = L'\0';
							
							/*
							  Regular matching
							*/
							if( whole_match )
							{
								const wchar_t *new_wc = L"";
								if( wc_end )
								{
									new_wc=wc_end+1;
									/*
									  Accept multiple '/' as a single direcotry separator
									*/
									while(*new_wc==L'/')
									{
										new_wc++;
									}
								}
								
								new_res = wildcard_expand_internal( new_wc,
																	new_dir, 
																	flags, 
																	out );

								if( new_res == -1 )
								{
									res = -1;
									break;
								}								
								res |= new_res;
								
							}
							
							/*
							  Recursive matching
							*/
							if( partial_match )
							{
								
								new_res = wildcard_expand_internal( wcschr( wc, ANY_STRING_RECURSIVE ), 
																	new_dir,
																	flags | WILDCARD_RECURSIVE, 
																	out );

								if( new_res == -1 )
								{
									res = -1;
									break;
								}								
								res |= new_res;
								
							}
						}								
					}
				}
			}
		}
		
		free( wc_str );
		free( new_dir );
	}
	closedir( dir );

	return res;
}


int wildcard_expand( const wchar_t *wc, 
					 const wchar_t *base_dir,
					 expand_flags_t flags,
					 std::vector<completion_t> &out )
{
	size_t c = out.size();
	int res = wildcard_expand_internal( wc, base_dir, flags, out );
			
	if( flags & ACCEPT_INCOMPLETE )
	{
        wcstring wc_base;
		const wchar_t *wc_base_ptr = wcsrchr( wc, L'/' );
		if( wc_base_ptr )
		{
            wc_base = wcstring(wc, (wc_base_ptr-wc)+1);
		}

		for( size_t i=c; i<out.size(); i++ )
		{
			completion_t &c = out.at( i );
			
			if( c.flags & COMPLETE_NO_CASE )
			{
				c.completion = format_string(L"%ls%ls%ls", base_dir, wc_base.c_str(), c.completion.c_str());
			}
		}		
	}
	return res;
}

int wildcard_expand_string(const wcstring &wc, const wcstring &base_dir, expand_flags_t flags, std::vector<completion_t> &outputs )
{
    std::vector<completion_t> lst;
    
    int res = wildcard_expand(wc.c_str(), base_dir.c_str(), flags, lst);    
    outputs.insert(outputs.end(), lst.begin(), lst.end());
    return res;
}
