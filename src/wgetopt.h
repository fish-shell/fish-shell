/** \file wgetopt.h
  A version of the getopt library for use with wide character strings.

  This is simply the gnu getopt library, but converted for use with
  wchar_t instead of char. This is not usually useful since the argv
  array is always defined to be of type char**, but in fish, all
  internal commands use wide characters and hence this library is
  useful.

  If you want to use this version of getopt in your program,
  download the fish sourcecode, available at <a
  href='http://fishshell.com'>the fish homepage</a>. Extract
  the sourcode, copy wgetopt.c and wgetopt.h into your program
  directory, include wgetopt.h in your program, and use all the
  regular getopt functions, prefixing every function, global
  variable and structure with a 'w', and use only wide character
  strings. There are no other functional changes in this version of
  getopt besides using wide character strings.

  For examples of how to use wgetopt, see the fish builtin
  functions, many of which are defined in builtin.c.

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

#include <stddef.h>

class wgetopter_t
{
private:
    void exchange(wchar_t **argv);
    const wchar_t * _wgetopt_initialize(const wchar_t *optstring);
    int _wgetopt_internal(int argc, wchar_t **argv, const wchar_t *optstring, const struct woption *longopts, int *longind, int long_only);
    
public:
    /* For communication from `getopt' to the caller.
     When `getopt' finds an option that takes an argument,
     the argument value is returned here.
     Also, when `ordering' is RETURN_IN_ORDER,
     each non-option ARGV-element is returned here.  */
    
    wchar_t *woptarg;
    
    /* Index in ARGV of the next element to be scanned.
     This is used for communication to and from the caller
     and for communication between successive calls to `getopt'.
     
     On entry to `getopt', zero means this is the first call; initialize.
     
     When `getopt' returns EOF, this is the index of the first of the
     non-option elements that the caller should itself scan.
     
     Otherwise, `woptind' communicates from one call to the next
     how much of ARGV has been scanned so far.  */
    
    /* XXX 1003.2 says this must be 1 before any call.  */
    int woptind;
    
    
    /* The next char to be scanned in the option-element
     in which the last option character we returned was found.
     This allows us to pick up the scan where we left off.
     
     If this is zero, or a null string, it means resume the scan
     by advancing to the next ARGV-element.  */
    
    wchar_t *nextchar;
    
    /* Callers store zero here to inhibit the error message
     for unrecognized options.  */
    
    int wopterr;
    
    /* Set to an option character which was unrecognized.
     This must be initialized on some systems to avoid linking in the
     system's own getopt implementation.  */
    
    int woptopt;
    
    /* Describe how to deal with options that follow non-option ARGV-elements.
     
     If the caller did not specify anything,
     the default is PERMUTE.
     
     REQUIRE_ORDER means don't recognize them as options;
     stop option processing when the first non-option is seen.
     This is what Unix does.
     This mode of operation is selected by using `+' as the first
     character of the list of option characters.
     
     PERMUTE is the default.  We permute the contents of ARGV as we scan,
     so that eventually all the non-options are at the end.  This allows options
     to be given in any order, even with programs that were not written to
     expect this.
     
     RETURN_IN_ORDER is an option available to programs that were written
     to expect options and other ARGV-elements in any order and that care about
     the ordering of the two.  We describe each non-option ARGV-element
     as if it were the argument of an option with character code 1.
     Using `-' as the first character of the list of option characters
     selects this mode of operation.
     
     The special argument `--' forces an end of option-scanning regardless
     of the value of `ordering'.  In the case of RETURN_IN_ORDER, only
     `--' can cause `getopt' to return EOF with `woptind' != ARGC.  */
    
    enum
    {
        REQUIRE_ORDER, PERMUTE, RETURN_IN_ORDER
    } ordering;
        
    /* Handle permutation of arguments.  */
    
    /* Describe the part of ARGV that contains non-options that have
     been skipped.  `first_nonopt' is the index in ARGV of the first of them;
     `last_nonopt' is the index after the last of them.  */
    
    int first_nonopt;
    int last_nonopt;
    
    
    wgetopter_t() : woptarg(NULL), woptind(0), nextchar(0), wopterr(0), woptopt('?'), ordering(), first_nonopt(0), last_nonopt(0)
    {
    }
    
    int wgetopt_long(int argc, wchar_t **argv, const wchar_t *options, const struct woption *long_options, int *opt_index);
    int wgetopt_long_only(int argc, wchar_t **argv, const wchar_t *options, const struct woption *long_options, int *opt_index);
};

/** Describe the long-named options requested by the application.
   The LONG_OPTIONS argument to getopt_long or getopt_long_only is a vector
   of `struct option' terminated by an element containing a name which is
   zero.

   The field `has_arg' is:
   no_argument    (or 0) if the option does not take an argument,
   required_argument  (or 1) if the option requires an argument,
   optional_argument   (or 2) if the option takes an optional argument.

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
    /**
       long name for switch
    */
    const wchar_t *name;
    /**
      Must be one of no_argument, required_argument and
      optional_argument.

      has_arg can't be an enum because some compilers complain about
      type mismatches in all the code that assumes it is an int.
    */
    int has_arg;

    /**
      If non-null, the flag whose value should be set if this switch is encountered
    */
    int *flag;

    /**
      If \c flag is non-null, this is the value that flag will be set
      to. Otherwise, this is the return-value of the function call.
    */
    int val;
};

/* Names for the values of the `has_arg' field of `struct option'.  */

/**
   Specifies that a switch does not accept an argument
*/
#define  no_argument    0
/**
   Specifies that a switch requires an argument
*/
#define required_argument  1
/**
   Specifies that a switch accepts an optional argument
*/
#define optional_argument  2

#endif /* FISH_WGETOPT_H */
