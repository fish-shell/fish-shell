/** \file mimedb.c 

mimedb is a program for checking the mimetype, description and
default action associated with a file or mimetype.  It uses the
xdgmime library written by the fine folks at freedesktop.org. There does 
not seem to be any standard way for the user to change the preferred
application yet.

The first implementation of mimedb used xml_grep to parse the xml
file for the mime entry to determine the description. This was abandoned
because of the performance implications of parsing xml. The current
version only does a simple string search, which is much, much
faster but it might fall on it's head.

This code is Copyright 2005-2008 Axel Liljencrantz.
It is released under the GPL.

The xdgmime library is dual licensed under LGPL/artistic
license. Read the source code of the library for more information.
*/

#include "config.h"


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <errno.h>
#include <regex.h>
#include <locale.h>


#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#if HAVE_LIBINTL_H
#include <libintl.h>
#endif

#include "xdgmime.h"
#include "fallback.h"
#include "util.h"
#include "print_help.h"


/**
   Location of the applications .desktop file, relative to a base mime directory
*/
#define APPLICATIONS_DIR "applications/"

/**
   Location of the mime xml database, relative to a base mime directory
*/
#define MIME_DIR "mime/"
/**
   Filename suffix for XML files
*/
#define MIME_SUFFIX ".xml"

/**
   Start tag for langauge-specific comment
*/
#define START_TAG "<comment( +xml:lang *= *(\"%s\"|'%s'))? *>"

/**
   End tab for comment
*/
#define STOP_TAG "</comment *>"

/**
   File contains cached list of mime actions
*/
#define DESKTOP_DEFAULT "applications/defaults.list"

/**
   Size for temporary string buffer used to make a regex for language
   specific descriptions
*/
#define BUFF_SIZE 1024

/**
  Program name
*/
#define MIMEDB "mimedb"

/**
   Getopt short switches for mimedb
*/
#define GETOPT_STRING "tfimdalhv"

/**
   Error message if system call goes wrong.
*/
#define ERROR_SYSTEM "%s: Could not execute command \"%s\"\n" 

/**
   Exit code if system call goes wrong.
*/
#define STATUS_ERROR_SYSTEM 1


/**
   All types of input and output possible
*/
enum
{
	FILEDATA,
	FILENAME,
	MIMETYPE,
	DESCRIPTION,
	ACTION,
	LAUNCH
}
;

/**
   Regular expression variable used to find start tag of description
*/
static regex_t *start_re=0;
/**
   Regular expression variable used to find end tag of description
*/
static regex_t *stop_re=0;

/**
   Error flag. Non-zero if something bad happened.
*/
static int error = 0;

/**
   String of characters to send to system() to launch a file
*/
static char *launch_buff=0;

/**
   Length of the launch_buff buffer
*/
static int launch_len=0;
/**
   Current position in the launch_buff buffer
*/
static int launch_pos=0;

/**
   gettext alias
*/
#ifdef USE_GETTEXT
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

/**
   Call malloc, set error flag and print message on failure
*/
void *my_malloc( size_t s )
{
	void *res = malloc( s );
	if( !s )
	{
		error=1;
		fprintf( stderr, _("%s: Out of memory\n"), MIMEDB );
	}
	return res;
}

/**
   Duplicate string, set error flag and print message on failure
*/
char *my_strdup( char *s )
{
	char *res = strdup( s );
	if( !s )
	{
		error=1;
		fprintf( stderr, _("%s: Out of memory\n"), MIMEDB );
	}
	return res;
}


/**
  Search the file \c filename for the first line starting with \c
  match, which is returned in a newly allocated string.
*/
static char * search_ini( const char *filename, const char *match )
{
	FILE *f = fopen( filename, "r" );
	char buf[4096];
	int len=strlen(match);
	int done = 0;	
	
	if(!f )
	{
		perror( "fopen" );
		error=1;
		return 0;
	}
	while( !done )
	{
		if( !fgets( buf, 4096, f ) )
		{
			if( !feof( f ) )
			{
				perror( "fgets" );
				error=1;
			}			
			buf[0]=0;
			done = 1;
		}
		else if( strncmp( buf, match,len )==0)
		{
			done=1;
		}		
	}
	fclose( f );
	if( buf[0] )
	{
		char *res=strdup(buf);
		if( res )
		{
			if(res[strlen(res)-1]=='\n' )
				res[strlen(res)-1]='\0';
		}
		return res;
	}
	else
		return (char *)0;
}

/**
  Test if the specified file exists. If it does not, also try
  replacing dashes with slashes in \c in.
*/
static char *file_exists( const char *dir, const char *in )
{
	char *filename = my_malloc( strlen( dir ) + strlen(in) + 1 );
	char *replaceme;
	struct stat buf;

//	fprintf( stderr, "Check %s%s\n", dir, in );
	
	if( !filename )
	{
		return 0;
	}	
	strcpy( filename, dir );
	strcat( filename, in );	
	
	if( !stat( filename, &buf ) )
		return filename;

	free( filename );
	
	/*
	  DOH! File does not exist. But all is not lost. KDE sometimes uses
	  a slash in the name as a directory separator. We try to replace
	  a dash with a slash and try again. 
	*/
	replaceme = strchr( in, '-' ); 
	if( replaceme )
	{
		char *res;
		
		*replaceme = '/';
		res = file_exists( dir, in );
		*replaceme = '-';
		return res;
	}
	/*
	  OK, no more slashes left. We really are screwed. Nothing to to
	  but admit defeat and go home.
	*/
	return 0;
}


/**
   Try to find the specified file in any of the possible directories
   where mime files can be located.  This code is shamelessly stolen
   from xdg_run_command_on_dirs.
*/
static char *get_filename( char *f )
{
	char *result;
	const char *xdg_data_home;
	const char *xdg_data_dirs;
	const char *ptr;

	xdg_data_home = getenv ("XDG_DATA_HOME");
	if (xdg_data_home)
    {
		result = file_exists( xdg_data_home, f ); 
		if (result)
			return result;
    }
	else
    {
		const char *home;

		home = getenv ("HOME");
		if (home != NULL)
		{
			char *guessed_xdg_home;

			guessed_xdg_home = my_malloc (strlen (home) + strlen ("/.local/share/") + 1);
			if( !guessed_xdg_home )
				return 0;
			
			strcpy (guessed_xdg_home, home);
			strcat (guessed_xdg_home, "/.local/share/");
			result = file_exists( guessed_xdg_home, f ); 
			free (guessed_xdg_home);

			if (result)
				return result;
		}
    }

	xdg_data_dirs = getenv ("XDG_DATA_DIRS");
	if (xdg_data_dirs == NULL)
		xdg_data_dirs = "/usr/local/share/:/usr/share/";

	ptr = xdg_data_dirs;

	while (*ptr != '\000')
    {
		const char *end_ptr;
		char *dir;
		int len;

		end_ptr = ptr;
		while (*end_ptr != ':' && *end_ptr != '\000')
			end_ptr ++;

		if (end_ptr == ptr)
		{
			ptr++;
			continue;
		}

		if (*end_ptr == ':')
			len = end_ptr - ptr;
		else
			len = end_ptr - ptr + 1;
		dir = my_malloc (len + 1);
		if( !dir )
			return 0;
		
		strncpy (dir, ptr, len);
		dir[len] = '\0';
		result = file_exists( dir, f ); 
		
		free (dir);

		if (result)
			return result;

		ptr = end_ptr;
    }
	return 0;	
}

/**
   Remove excessive whitespace from string. Replaces arbitrary sequence
   of whitespace with a single space.  Also removes any leading and
   trailing whitespace
*/
static char *munge( char *in )
{
	char *out = my_malloc( strlen( in )+1 );
	char *p=out;
	int had_whitespace = 0;
	int printed = 0;
	if( !out )
	{
		return 0;
	}
	
	while( 1 )
	{
//		fprintf( stderr, "%c\n", *in );
		
		switch( *in )
		{
			case ' ':
			case '\n':
			case '\t':
			case '\r':
			{
				had_whitespace = 1;
				break;
			}
			case '\0':
				*p = '\0';
				return out;
			default:
			{
				if( printed && had_whitespace )
				{
					*(p++)=' ';
				}
				printed=1;
				had_whitespace=0;				
				*(p++)=*in;
				break;
			}
		}
		in++;
	}
	fprintf( stderr, _( "%s: Unknown error in munge()\n"), MIMEDB );
	error=1;
	return 0;
}

/**
   Return a regular expression that matches all strings specifying the current locale
*/
static char *get_lang_re()
{
	
	static char buff[BUFF_SIZE];
	const char *lang = setlocale( LC_MESSAGES, 0 );
	int close=0;
	char *out=buff;
    
	if( (1+strlen(lang)*4) >= BUFF_SIZE )
	{
		fprintf( stderr, _( "%s: Locale string too long\n"), MIMEDB );
		error = 1;
		return 0;
	}
	
	for( ; *lang; lang++ )
	{
		switch( *lang )
		{
			case '@':
			case '.':
			case '_':
				if( close )
				{
					*out++ = ')';
					*out++ = '?';
				}
				
				close=1;
				*out++ = '(';
				*out++ = *lang;
				break;
				
			default:
				*out++ = *lang;
		}
	}
	
	if( close )
	{
		*out++ = ')';
		*out++ = '?';
	}
	*out++=0;

	return buff;
}

/**
   Get description for a specified mimetype. 
*/
static char *get_description( const char *mimetype )
{
	char *fn_part;
	
	char *fn;
	int fd;
	struct stat st;
	char *contents;
	char *start=0, *stop=0, *best_start=0;

	if( !start_re )
	{
		char *lang;
		char buff[BUFF_SIZE];

		lang = get_lang_re();
		if( !lang )
			return 0;
		
		snprintf( buff, BUFF_SIZE, START_TAG, lang, lang );

		start_re = my_malloc( sizeof(regex_t));
		stop_re = my_malloc( sizeof(regex_t));
		
        int reg_status;
		if( ( reg_status = regcomp( start_re, buff, REG_EXTENDED ) ) )
        {
            char regerrbuf[BUFF_SIZE];
            regerror(reg_status, start_re, regerrbuf, BUFF_SIZE);
			fprintf( stderr, _( "%s: Could not compile regular expressions %s with error %s\n"), MIMEDB, buff, regerrbuf);
			error=1;
        
        }
        else if ( ( reg_status = regcomp( stop_re, STOP_TAG, REG_EXTENDED ) ) )
		{
            char regerrbuf[BUFF_SIZE];
            regerror(reg_status, stop_re, regerrbuf, BUFF_SIZE);
			fprintf( stderr, _( "%s: Could not compile regular expressions %s with error %s\n"), MIMEDB, buff, regerrbuf);
			error=1;
        
        }

        if( error )
        {
			free( start_re );
			free( stop_re );
			start_re = stop_re = 0;

			return 0;
        }
	}
	
	fn_part = my_malloc( strlen(MIME_DIR) + strlen( mimetype) + strlen(MIME_SUFFIX) + 1 );

	if( !fn_part )
	{
		return 0;
	}

	strcpy( fn_part, MIME_DIR );
	strcat( fn_part, mimetype );
	strcat( fn_part, MIME_SUFFIX );

	fn = get_filename(fn_part); //malloc( strlen(MIME_DIR) +strlen( MIME_SUFFIX)+ strlen( mimetype ) + 1 );
	free(fn_part );
	
	if( !fn )
	{
		return 0;
	}

	fd = open( fn, O_RDONLY );

//	fprintf( stderr, "%s\n", fn );
	
	if( fd == -1 )
	{
		perror( "open" );
		error=1;
		return 0;
	}
	
	if( stat( fn, &st) )
	{
		perror( "stat" );
		error=1;
		return 0;
	}

	contents = my_malloc( st.st_size + 1 );
	if( !contents )
	{
		return 0;
	}
	
	if( read( fd, contents, st.st_size ) != st.st_size )
	{
		perror( "read" );
		error=1;
		return 0;
	}

	/*
	  Don't need to check exit status of close on read-only file descriptors
	*/
	close( fd );
	free( fn );

	contents[st.st_size]=0;
	regmatch_t match[1];
	int w = -1;
	
	start=contents;
	
	/*
	  On multiple matches, use the longest match, should be a pretty
	  good heuristic for best match...
	*/
	while( !regexec(start_re, start, 1, match, 0) )
	{
		int new_w = match[0].rm_eo - match[0].rm_so;
		start += match[0].rm_eo;
		
		if( new_w > w )
		{
			/* 
			   New match is for a longer match then the previous
			   match, so we use the new match
			*/
			w=new_w;
			best_start = start;
		}
	}
	
	if( w != -1 )
	{
		start = best_start;
		if( !regexec(stop_re, start, 1, match, 0) )
		{
			/*
			  We've found the beginning and the end of a suitable description
			*/
			char *res;

			stop = start + match[0].rm_so;
			*stop = '\0';
			res = munge( start );
			free( contents );
			return res;
		}
	}
	free( contents );
	fprintf( stderr, _( "%s: No description for type %s\n"), MIMEDB, mimetype );
	error=1;
	return 0;

}


/**
   Get default action for a specified mimetype. 
*/
static char *get_action( const char *mimetype )
{
	char *res=0;
	
	char *launcher;
	char *end;
	char *mime_filename;
	
	char *launcher_str;
	char *launcher_filename, *launcher_command_str, *launcher_command;
	char *launcher_full;
	
	mime_filename = get_filename( DESKTOP_DEFAULT );
	if( !mime_filename )
		return 0;
	
	launcher_str = search_ini( mime_filename, mimetype );

	free( mime_filename );
	
	if( !launcher_str )
	{
		/*
		  This type does not have a launcher. Try the supertype!
		*/
//		fprintf( stderr, "mimedb: %s does not have launcher, try supertype\n", mimetype );	
		const char ** parents = xdg_mime_get_mime_parents(mimetype);
		
		const char **p;
		if( parents )
		{
			for( p=parents; *p; p++ )
			{
				char *a = get_action(*p);
				if( a != 0 )
					return a;
			}
		}
		/*
		  Just in case subclassing doesn't work, (It doesn't on Fedora
		  Core 3) we also test some common subclassings.
		*/
		
		if( strncmp( mimetype, "text/plain", 10) != 0 && strncmp( mimetype, "text/", 5 ) == 0 )
			return get_action( "text/plain" );

		return 0;
	}
	
//	fprintf( stderr, "WOOT %s\n", launcher_str );	
	launcher = strchr( launcher_str, '=' );
		
	if( !launcher )
	{
		fprintf( stderr, _("%s: Could not parse launcher string '%s'\n"), MIMEDB, launcher_str );	
		error=1;
		return 0;
	}
	
	/* Skip the = */
	launcher++;
						
	/* Only use first launcher */
	end = strchr( launcher, ';' );
	if( end )
		*end = '\0';

	launcher_full = my_malloc( strlen( launcher) + strlen( APPLICATIONS_DIR)+1 );
	if( !launcher_full )
	{
		free( launcher_str );
		return 0;
	}
	
	strcpy( launcher_full, APPLICATIONS_DIR );
	strcat( launcher_full, launcher );
	free( launcher_str );
	
	launcher_filename = get_filename( launcher_full );
	
	free( launcher_full );
		
	launcher_command_str = search_ini( launcher_filename, "Exec=" );
	
	if( !launcher_command_str )
	{
		fprintf( stderr, 
				 _( "%s: Default launcher '%s' does not specify how to start\n"), MIMEDB, 
				 launcher_filename );
		free( launcher_filename );
		return 0;	
	}

	free( launcher_filename );
	
	launcher_command = strchr( launcher_command_str, '=' );
	launcher_command++;
	
	res = my_strdup( launcher_command );
	
	free( launcher_command_str );	

	return res;
}


/**
   Helper function for launch. Write the specified byte to the string we will execute
*/
static void writer( char c )
{
	if( launch_len == -1 )
		return;
	
	if( launch_len <= launch_pos )
	{
		int new_len = launch_len?2*launch_len:256;
		char *new_buff = realloc( launch_buff, new_len );
		if( !new_buff )
		{
			free( launch_buff );
			launch_len = -1;
			error=1;
			return;
		}
		launch_buff = new_buff;
		launch_len = new_len;
		
	}
	launch_buff[launch_pos++]=c;
}

/**
   Write out the specified byte in hex
*/
static void writer_hex( int num )
{
	int a, b;
	a = num /16;
	b = num %16;

	writer( a>9?('A'+a-10):('0'+a));
	writer( b>9?('A'+b-10):('0'+b));
}

/**
  Return current directory in newly allocated string
*/
static char *my_getcwd ()
{
	size_t size = 100;	
	while (1)
	{
		char *buffer = (char *) malloc (size);
		if (getcwd (buffer, size) == buffer)
			return buffer;
		free (buffer);
		if (errno != ERANGE)
			return 0;
		size *= 2;
	}
}

/**
   Return absolute filename of specified file
 */
static char *get_fullfile( char *file )
{
	char *fullfile;
	
	if( file[0] == '/' )
	{
		fullfile = file;
	}
	else
	{
		char *cwd = my_getcwd();
		if( !cwd )
		{
			error = 1;
			perror( "getcwd" );
			return 0;
		}
		
		int l = strlen(cwd);
		
		fullfile = my_malloc( l + strlen(file)+2 );
		if( !fullfile )
		{
			free(cwd);
			return 0;
		}
		strcpy( fullfile, cwd );
		if( cwd[l-1] != '/' )
			strcat(fullfile, "/" );
		strcat( fullfile, file );
		
		free(cwd);
	}
	return fullfile;
}


/**
   Write specified file as an URL
*/
static void write_url( char *file )
{
	char *fullfile = get_fullfile( file );
	char *str = fullfile;
	
	if( str == 0 )
	{
		launch_len = -1;
		return;
	}
	
	writer( 'f');
	writer( 'i');
	writer( 'l');
	writer( 'e');
	writer( ':');
	writer( '/');
	writer( '/');
	while( *str )
	{
		if( ((*str >= 'a') && (*str <='z')) ||
			((*str >= 'A') && (*str <='Z')) ||
			((*str >= '0') && (*str <='9')) ||
			(strchr( "-_.~/",*str) != 0) )
		{
			writer(*str);			
		}
		else if(strchr( "()?&=",*str) != 0) 
		{
			writer('\\');
			writer(*str);
		}
		else
		{
			writer( '%' );
			writer_hex( (unsigned char)*str );
		}
		str++;
	}
	if( fullfile != file )
		free( fullfile );
	
}

/**
   Write specified file
*/
static void write_file( char *file, int print_path )
{
	char *fullfile;
	char *str;
	if( print_path )
	{
		fullfile = get_fullfile( file );
		str = fullfile;
	}
	else
	{
		fullfile = my_strdup( file );
		if( !fullfile )
		{
			return;
		}
		str = basename( fullfile );
	}
	
	if( !str )
	{
		error = 1;
		return;
	}

	while( *str )
	{
		switch(*str )
		{
			case ')':
			case '(':
			case '-':
			case '#':
			case '$':
			case '}':
			case '{':
			case ']':
			case '[':
			case '*':
			case '?':
			case ' ':
			case '|':
			case '<':
			case '>':
			case '^':
			case '&':
			case '\\':
			case '`':
			case '\'':
			case '\"':
				writer('\\');
				writer(*str);
				break;

			case '\n':
				writer('\\');
				writer('n');
				break;

			case '\r':
				writer('\\');
				writer('r');
				break;

			case '\t':
				writer('\\');
				writer('t');
				break;

			case '\b':
				writer('\\');
				writer('b');
				break;

			case '\v':
				writer('\\');
				writer('v');
				break;

			default:
				writer(*str);
				break;
		}
		str++;
	}

	if( fullfile != file )
		free( fullfile );
}

/**
   Use the specified launch filter to launch all the files in the specified list. 

   \param filter the action to take
   \param files the list of files for which to perform the action
   \param fileno an internal value. Should always be set to zero.
*/
static void launch( char *filter, array_list_t *files, int fileno )
{
	char *filter_org=filter;
	int count=0;
	int launch_again=0;
	
	if( al_get_count( files ) <= fileno )
		return;
	
	
	launch_pos=0;

	for( ;*filter && !error; filter++)
	{
		if(*filter == '%')
		{
			filter++;
			switch( *filter )
			{
				case 'u':
				{
					launch_again = 1;
					write_url( (char *)al_get( files, fileno ) );
					break;
				}				
				case 'U':
				{
					int i;
					for( i=0; i<al_get_count( files ); i++ )
					{
						if( i != 0 )
							writer( ' ' );
						write_url( (char *)al_get( files, i ) );
						if( error )
							break;
					}
					
					break;
				}
				
				case 'f':
				case 'n':
				{
					launch_again = 1;
					write_file( (char *)al_get( files, fileno ), *filter == 'f' );
					break;
				}
				
				case 'F':
				case 'N':
				{
					int i;
					for( i=0; i<al_get_count( files ); i++ )
					{
						if( i != 0 )
							writer( ' ' );
						write_file( (char *)al_get( files, i ), *filter == 'F' );
						if( error )
							break;
					}
					break;
				}
				

				case 'd':
				{
					char *cpy = get_fullfile( (char *)al_get( files, fileno ) );
					char *dir;

					launch_again=1;					
					/*
					  We wish to modify this string, make sure it is only a copy
					*/
					if( cpy == al_get( files, fileno ) )
						cpy = my_strdup( cpy );

					if( cpy == 0 )
					{
						break;
					}
					dir=dirname( cpy );
					write_file( dir, 1 );
					free( cpy );
					
					break;
				}
			
				case 'D':
				{
					int i;
					for( i=0; i<al_get_count( files ); i++ )
					{
						char *cpy = get_fullfile( (char *)al_get( files, i ) );					
						char *dir;

						/*
						  We wish to modify this string, make sure it is only a copy
						*/
						if( cpy == al_get( files, i ) )
							cpy = my_strdup( cpy );

						if( cpy == 0 )
						{
							break;
						}
						dir=dirname( cpy );
						
						if( i != 0 )
							writer( ' ' );

						write_file( dir, 1 );
						free( cpy );

					}
					break;					
				}
				
				default:
					fprintf( stderr, _("%s: Unsupported switch '%c' in launch string '%s'\n"), MIMEDB, *filter, filter_org );
					launch_len=0;
					break;
					
			}
		}
		else
		{
			writer( *filter );
			count++;
		}
	}

	if( error )
		return;
	
	switch( launch_len )
	{
		case -1:
		{
			launch_len = 0;
			fprintf( stderr, _( "%s: Out of memory\n"), MIMEDB );
			return;
		}
		case 0:
		{
			return;
		}
		default:
		{
			
			writer( ' ' );
			writer( '&' );
			writer( '\0' );
	
			if( system( launch_buff ) == -1 )
			{
				fprintf( stderr, _( ERROR_SYSTEM ), MIMEDB, launch_buff );
				exit(STATUS_ERROR_SYSTEM);
			}
			
			break;
		}
	}
	if( launch_again )
	{
		launch( filter_org, files, fileno+1 );
	}
	
}

/**
   Clean up one entry from the hash table of launch files
*/
static void clear_entry( void *key, void *val )
{
	/*
	  The key is a mime value, either from the libraries internal hash
	  table of mime types or from the command line. Either way, it
	  should not be freed.

	  The value is an array_list_t of filenames. The filenames com from
	  the argument list and should not be freed. The arraylist,
	  however, should be destroyed and freed.
	*/
	array_list_t *l = (array_list_t *)val;
	al_destroy( l );
	free( l );
}

/**
   Do locale specific init
*/
static void locale_init()
{
	setlocale( LC_ALL, "" );
	bindtextdomain( PACKAGE_NAME, LOCALEDIR );
	textdomain( PACKAGE_NAME );
}


/**
   Main function. Parses options and calls helper function for any heavy lifting.
*/
int main (int argc, char *argv[])
{	
	int input_type=FILEDATA;
	int output_type=MIMETYPE;
	
	const char *mimetype;
	char *output=0;
	
	int i;

	hash_table_t launch_hash;

	locale_init();
	
	/*
	  Parse options
	*/
	while( 1 )
	{
		static struct option
			long_options[] =
			{
				{
					"input-file-data", no_argument, 0, 't' 
				}
				,
				{
					"input-filename", no_argument, 0, 'f' 
				}
				,
				{
					"input-mime", no_argument, 0, 'i' 
				}
				,
				{
					"output-mime", no_argument, 0, 'm' 
				}
				,
				{
					"output-description", no_argument, 0, 'd' 
				}
				,
				{
					"output-action", no_argument, 0, 'a' 
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
					"launch", no_argument, 0, 'l'
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
				break;				

			case 't':		
				input_type=FILEDATA;
				break;

			case 'f':		
				input_type=FILENAME;
				break;

			case 'i':		
				input_type=MIMETYPE;
				break;

			case 'm':		
				output_type=MIMETYPE;
				break;

			case 'd':		
				output_type=DESCRIPTION;
				break;

			case 'a':		
				output_type=ACTION;
				break;

			case 'l':		
				output_type=LAUNCH;
				break;

			case 'h':
				print_help( argv[0], 1 );
				exit(0);				
								
			case 'v':
				printf( _("%s, version %s\n"), MIMEDB, PACKAGE_VERSION );
				exit( 0 );				
				
			case '?':
				return 1;
				
		}		
	}

	if( ( output_type == LAUNCH )&&(input_type==MIMETYPE))
	{
		fprintf( stderr, _("%s: Can not launch a mimetype\n"), MIMEDB );
		print_help( argv[0], 2 );
		exit(1);
	}

	if( output_type == LAUNCH )
		hash_init( &launch_hash, &hash_str_func, &hash_str_cmp );
	
	
	/* 
	   Loop over all non option arguments and do the specified lookup
	*/
	
	//fprintf( stderr, "Input %d, output %d\n", input_type, output_type );	

	for (i = optind; (i < argc)&&(!error); i++)
    {
		/* Convert from filename to mimetype, if needed */
		if( input_type == FILENAME )
		{
			mimetype = xdg_mime_get_mime_type_from_file_name(argv[i]);
		}
		else if( input_type == FILEDATA )
		{
			mimetype = xdg_mime_get_mime_type_for_file(argv[i]);
		}
		else
			mimetype = xdg_mime_is_valid_mime_type(argv[i])?argv[i]:0;

		mimetype = xdg_mime_unalias_mime_type (mimetype);
		if( !mimetype )
		{
			fprintf( stderr, _( "%s: Could not parse mimetype from argument '%s'\n"), MIMEDB, argv[i] );
			error=1;
			return 1;
		}
		
		/*
		  Convert from mimetype to whatever, if needed
		*/
		switch( output_type )
		{
			case MIMETYPE:
			{
				output = (char *)mimetype;
				break;
				
			}
			case DESCRIPTION:
			{
				output = get_description( mimetype );				
				if( !output )
					output = strdup( _("Unknown") );
				
				break;
			}
			case ACTION:
			{
				output = get_action( mimetype );
				break;
			}			
			case LAUNCH:
			{
				/*
				  There may be more files using the same launcher, we
				  add them all up in little array_list_ts and launched
				  them together after all the arguments have been
				  parsed.
				*/
				array_list_t *l= (array_list_t *)hash_get( &launch_hash, mimetype );
				output = 0;
				
				if( !l )
				{
					l = my_malloc( sizeof( array_list_t ) );
					if( l == 0 )
					{
						break;
					}
					al_init( l );
					hash_put( &launch_hash, mimetype, l );
				}
				al_push( l, argv[i] );				
			}
		}
		
		/*
		  Print the glorious result
		*/
		if( output )
		{
			printf( "%s\n", output );
			if( output != mimetype )
				free( output );
		}
		output = 0;
    }

	/*
	  Perform the actual launching
	*/
	if( output_type == LAUNCH && !error )
	{
		int i;
		array_list_t mimes;
		al_init( &mimes );
		hash_get_keys( &launch_hash, &mimes );
		for( i=0; i<al_get_count( &mimes ); i++ )
		{
			char *mimetype = (char *)al_get( &mimes, i );
			array_list_t *files = (array_list_t *)hash_get( &launch_hash, mimetype );
			if( !files )
			{
				fprintf( stderr, _( "%s: Unknown error\n"), MIMEDB );
				error=1;
				break;				
			}
			
			char *launcher = get_action( mimetype );

			if( launcher )
			{
				launch( launcher, files, 0 );
				free( launcher );
			}
		}
		hash_foreach( &launch_hash, &clear_entry );
		hash_destroy( &launch_hash );
		al_destroy( &mimes );
	}
	
	if( launch_buff )
		free( launch_buff );

	if( start_re )
	{
		regfree( start_re );
		regfree( stop_re );
		free( start_re );
		free( stop_re );
	}	

	xdg_mime_shutdown();
	
	return error;	
}
