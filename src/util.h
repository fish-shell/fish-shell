/** \file util.h
  Generic utilities library.
*/

#ifndef FISH_UTIL_H
#define FISH_UTIL_H

/**
   Returns the larger of two ints
*/
template<typename T>
inline T maxi(T a, T b)
{
    return a>b?a:b;
}

/**
   Returns the smaller of two ints
 */
template<typename T>
inline T mini(T a, T b)
{
    return a<b?a:b;
}

/**
   Compares two wide character strings with an (arguably) intuitive
   ordering.

   This function tries to order strings in a way which is intuitive to
   humans with regards to sorting strings containing numbers.

   Most sorting functions would sort the strings 'file1.txt'
   'file5.txt' and 'file12.txt' as:

   file1.txt
   file12.txt
   file5.txt

   This function regards any sequence of digits as a single entity
   when performing comparisons, so the output is instead:

   file1.txt
   file5.txt
   file12.txt

   Which most people would find more intuitive.

   This won't return the optimum results for numbers in bases higher
   than ten, such as hexadecimal, but at least a stable sort order
   will result.

   This function performs a two-tiered sort, where difference in case
   and in number of leading zeroes in numbers only have effect if no
   other differences between strings are found. This way, a 'file1'
   and 'File1' will not be considered identical, and hence their
   internal sort order is not arbitrary, but the names 'file1',
   'File2' and 'file3' will still be sorted in the order given above.
*/
int wcsfilecmp(const wchar_t *a, const wchar_t *b);

/**
   Get the current time in microseconds since Jan 1, 1970
*/
long long get_time();

#endif
