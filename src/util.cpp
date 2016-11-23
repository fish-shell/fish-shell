// Generic utilities library.
//
// Contains data structures such as automatically growing array lists, priority queues, etc.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <wchar.h>
#include <wctype.h>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "util.h"
#include "wutil.h"  // IWYU pragma: keep

int wcsfilecmp(const wchar_t *a, const wchar_t *b) {
    CHECK(a, 0);
    CHECK(b, 0);

    if (*a == 0) {
        if (*b == 0) return 0;
        return -1;
    }
    if (*b == 0) {
        return 1;
    }

    long secondary_diff = 0;
    if (iswdigit(*a) && iswdigit(*b)) {
        const wchar_t *aend, *bend;
        long al;
        long bl;
        long diff;

        al = fish_wcstol(a, &aend);
        int a1_errno = errno;
        bl = fish_wcstol(b, &bend);
        if (a1_errno || errno) {
            // Huge numbers - fall back to regular string comparison.
            return wcscmp(a, b);
        }

        diff = al - bl;
        if (diff) return diff > 0 ? 2 : -2;

        secondary_diff = (aend - a) - (bend - b);

        a = aend - 1;  //!OCLINT(parameter reassignment)
        b = bend - 1;  //!OCLINT(parameter reassignment)
    } else {
        int diff = towlower(*a) - towlower(*b);
        if (diff != 0) return diff > 0 ? 2 : -2;

        secondary_diff = *a - *b;
    }

    int res = wcsfilecmp(a + 1, b + 1);

    // If no primary difference in rest of string use secondary difference on this element if
    // found.
    if (abs(res) < 2 && secondary_diff) {
        return secondary_diff > 0 ? 1 : -1;
    }

    return res;
}

long long get_time() {
    struct timeval time_struct;
    gettimeofday(&time_struct, 0);
    return 1000000ll * time_struct.tv_sec + time_struct.tv_usec;
}
