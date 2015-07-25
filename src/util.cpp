/** \file util.c
  Generic utilities library.

  Contains datastructures such as automatically growing array lists, priority queues, etc.
*/

#include "config.h"


#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <math.h>
#include <sys/time.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <wctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "wutil.h"

/**
   Minimum allocated size for data structures. Used to avoid excessive
   memory allocations for lists, hash tables, etc, which are nearly
   empty.
*/
#define MIN_SIZE 32

/**
   Maximum number of characters that can be inserted using a single
   call to sb_printf. This is needed since vswprintf doesn't tell us
   what went wrong. We don't know if we ran out of space or something
   else went wrong. We assume that any error is an out of memory-error
   and try again until we reach this size.  After this size has been
   reached, it is instead assumed that something was wrong with the
   format string.
*/
#define SB_MAX_SIZE (128*1024*1024)

int wcsfilecmp(const wchar_t *a, const wchar_t *b)
{
    CHECK(a, 0);
    CHECK(b, 0);

    if (*a==0)
    {
        if (*b==0)
            return 0;
        return -1;
    }
    if (*b==0)
    {
        return 1;
    }

    long secondary_diff=0;
    if (iswdigit(*a) && iswdigit(*b))
    {
        wchar_t *aend, *bend;
        long al;
        long bl;
        long diff;

        errno = 0;
        al = wcstol(a, &aend, 10);
        bl = wcstol(b, &bend, 10);

        if (errno)
        {
            /*
              Huuuuuuuuge numbers - fall back to regular string comparison
            */
            return wcscmp(a, b);
        }

        diff = al - bl;
        if (diff)
            return diff > 0 ? 2 : -2;

        secondary_diff = (aend-a) - (bend-b);

        a=aend-1;
        b=bend-1;
    }
    else
    {
        int diff = towlower(*a) - towlower(*b);
        if (diff != 0)
            return (diff>0)?2:-2;

        secondary_diff = *a-*b;
    }

    int res = wcsfilecmp(a+1, b+1);

    if (abs(res) < 2)
    {
        /*
          No primary difference in rest of string.
          Use secondary difference on this element if found.
        */
        if (secondary_diff)
        {
            return secondary_diff > 0 ? 1 :-1;
        }
    }

    return res;
}

long long get_time()
{
    struct timeval time_struct;
    gettimeofday(&time_struct, 0);
    return 1000000ll*time_struct.tv_sec+time_struct.tv_usec;
}

