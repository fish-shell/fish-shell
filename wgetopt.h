/** \file wgetopt.h
	The getopt librar for wide character strings. 

	This is simply the gnu getopt library, but converted for use with wchar_t instead of char. This is not usually useful since the argv array is always defined to be of type char**, but in fish, all internal commands use wide characters and hence this library is usefull.
*/


/* Declarations for getopt.
   Copyright (C) 1989, 90, 91, 92, 93, 94 Free Software Foundation, Inc.

This file is part of the GNU C Library.  Its master source is NOT part of
the C library, however.  The master source lives in /gd/gnu/lib.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

#ifndef FISH_WGETOPT_H
#define FISH_WGETOPT_H

#include <wchar.h>

#ifdef	__cplusplus
extern "C" {
#endif

/** For communication from `getopt' to the caller.
   When `getopt' finds an option that takes an argument,
   the argument value is returned here.
   Also, when `ordering' is RETURN_IN_ORDER,
   each non-option ARGV-element is returned here.  */

extern wchar_t *woptarg;

/** Index in ARGV of the next element to be scanned.
   This is used for communication to and from the caller
   and for communication between successive calls to `getopt'.

   On entry to `getopt', zero means this is the first call; initialize.

   When `getopt' returns EOF, this is the index of the first of the
   non-option elements that the caller should itself scan.

   Otherwise, `optind' communicates from one call to the next
   how much of ARGV has been scanned so far.  */

extern int woptind;

/** Callers store zero here to inhibit the error message `getopt' prints
   for unrecognized options.  */

extern int wopterr;

/** Set to an option character which was unrecognized.  */

extern int woptopt;

/** Describe the long-named options requested by the application.
   The LONG_OPTIONS argument to getopt_long or getopt_long_only is a vector
   of `struct option' terminated by an element containing a name which is
   zero.

   The field `has_arg' is:
   no_argument		(or 0) if the option does not take an argument,
   required_argument	(or 1) if the option requires an argument,
   optional_argument 	(or 2) if the option takes an optional argument.

   If the field `flag' is not NULL, it points to a variable that is set
   to the value given in the field `val' when the option is found, but
   left unchanged if the option is not found.

   To have a long-named option do something other than set an `int' to
   a compiled-in constant, such as set a value from `optarg', set the
   option's `flag' field to zero and its `val' field to a nonzero
   value (the equivalent single-letter option character, if there is
   one).  For long options that have a zero `flag' field, `getopt'
   returns the contents of the `val' field.  */

struct woption
{
#if defined (__STDC__) && __STDC__
  const wchar_t *name;
#else
  wchar_t *name;
#endif
  /* has_arg can't be an enum because some compilers complain about
     type mismatches in all the code that assumes it is an int.  */
  int has_arg;
  int *flag;
  int val;
};

/* Names for the values of the `has_arg' field of `struct option'.  */

#define	no_argument		0
#define required_argument	1
#define optional_argument	2

#if defined (__STDC__) && __STDC__
#ifdef __GNU_LIBRARY__
/* Get options from argument list */
extern int wgetopt (int argc, wchar_t *const *argv, const wchar_t *shortopts);
#else /* not __GNU_LIBRARY__ */
/* Get options from argument list */
extern int wgetopt ();
#endif /* __GNU_LIBRARY__ */
/* Get options from argument list */
extern int wgetopt_long (int argc, wchar_t *const *argv, const wchar_t *shortopts,
		        const struct woption *longopts, int *longind);
/* Get options from argument list */
extern int wgetopt_long_only (int argc, wchar_t *const *argv,
			     const wchar_t *shortopts,
		             const struct woption *longopts, int *longind);

/** Internal only.  Users should not call this directly.  */
extern int _wgetopt_internal (int argc, wchar_t *const *argv,
			     const wchar_t *shortopts,
		             const struct woption *longopts, int *longind,
			     int long_only);
#else /* not __STDC__ */
/* Get options from argument list */
extern int wgetopt ();
/* Get options from argument list */
extern int wgetopt_long ();
/* Get options from argument list */
extern int wgetopt_long_only ();

/* Get options from argument list */
extern int _wgetopt_internal ();
#endif /* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif /* FISH_WGETOPT_H */
