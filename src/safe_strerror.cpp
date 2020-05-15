#include "config.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>

// strerror is not thread-safe and strerror_r is not async-safe (see
// man signal-safety) glibc strerror_r can hang in certain real-world
// scenarios. (cf github issues #472, #1830, #4183)

// To work around that, pre-generate a read-only list of messages at
// startup which can be safely returned.

// Since INT_MAX is pretty big, let's look at various OS maximum value for errno:
//
// Linux [1-133]
// https://github.com/torvalds/linux/blob/master/include/uapi/asm-generic/errno.h
//
// FreeBSD (userspace visible) [1-97]
// https://github.com/freebsd/freebsd/blob/master/sys/sys/errno.h
//
// OpenBSD (userspace visible) [1-95]
// https://github.com/openbsd/src/blob/master/sys/sys/errno.h
//
// Apple (userspace visible) [1-88]
// https://opensource.apple.com/source/xnu/xnu-201/bsd/sys/errno.h.auto.html
//
// Solaris [1-151]
// http://fxr.watson.org/fxr/source/common/sys/errno.h?v=OPENSOLARIS
//
static const int max_errno = 200;
static const char *errno_list[max_errno] = {};

void errno_list_init()
{
    // save errno
    int err = errno;
    char buf[512] = {};

    for (int i = 0; i < max_errno; i++) {
	const char *s = strerror(i);
	if (!s) {
	    snprintf(buf, sizeof(buf)-1, "Unknown error %d", i);
	    s = buf;
	}
	errno_list[i] = strdup(s);
	if (!errno_list[i]) {
	    // alloc error
	    fprintf(stderr, "fish: coud not alloc memory for safe_errno()\n");
	}
    }

    // restore errno
    errno = err;
}

void errno_list_free()
{
    // free(null) is a no-op
    for (int i = 0; i < max_errno; i++) {
	free((void *)errno_list[i]);
	errno_list[i] = nullptr;
    }
}

const char *safe_strerror(int err)
{
    const char *s = nullptr;
    if (0 <= err && err < max_errno)
	s = errno_list[err];
    return s ? s : "Unknown error";
}
