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

