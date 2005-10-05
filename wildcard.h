
/** \file wildcard.h 

    My own globbing implementation. Needed to implement this instead
    of using libs globbing to support tab-expantion of globbed
    paramaters.

*/

#ifndef FISH_WILDCARD_H
#define FISH_WILDCARD_H

#include <wchar.h>

#include "util.h"

/*
    These constants are outside the 31 bit character space of USC4,
	thogh they may clash with WEOF. I need to use characters outside of
	the regular character space to represent wildcards and such, 
	in order to do backslash removal before wildcard matching.
*/

/** Character representing any character except '/' */
#define ANY_CHAR 0xfffffffe

/** Character representing any character string not containing '/' (A slash) */
#define ANY_STRING 0xfffffffd

/** Character representing any character string */
#define ANY_STRING_RECURSIVE 0xfffffff6

/**
    Expand the wildcard by matching against the filesystem. 

	New strings are allocated using malloc and should be freed by the caller.

	wildcard_expand works by dividing the wildcard into segments at
	each directory boundary. Each segment is processed separatly. All
	except the last segment are handled by matching the wildcard
	segment against all subdirectories of matching directories, and
	recursively calling wildcard_expand for matches. On the last
	segment, matching is made to any file, and all matches are
	inserted to the list.

	If wildcard_expand encounters any errors (such as insufficient
	priviliges) during matching, no error messages will be printed and
	wildcard_expand will continue the matching process.

	\param wc The wildcard string
	\param base_dir The base directory of the filesystem to perform the match against
	\param status flags for the search. Can be any combination of ACCEPT_INCOMPLETE and EXECUTABLES_ONLY
	\param out The list in which to put the output

	\return 1 if matches where found, 0 otherwise.
   
*/
int wildcard_expand( const wchar_t *wc, 
					 const wchar_t *base_dir, 
					 int flags, 
					 array_list_t *out );
/**
   Test whether the given wildcard matches the string

   \param str The string to test
   \param wc The wildcard to test against
   \return true if the wildcard matched
*/
int wildcard_match( const wchar_t *str, 
					const wchar_t *wc );


/**
   Check if the specified string contains wildcards
*/
int wildcard_has( const wchar_t *str, int internal );

/**
   Test wildcard completion
*/
int wildcard_complete( const wchar_t *str,
						const wchar_t *wc,
						const wchar_t *desc,						
						const wchar_t *(*desc_func)(const wchar_t *),
						array_list_t *out );

#endif
