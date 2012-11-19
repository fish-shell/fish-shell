/** \file input_common.c

Implementation file for the low level input library

*/
#include "config.h"


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <wchar.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "wutil.h"
#include "input_common.h"
#include "env_universal.h"
#include "iothread.h"

/**
   Time in milliseconds to wait for another byte to be available for
   reading after \\x1b is read before assuming that escape key was
   pressed, and not an escape sequence.
*/
#define WAIT_ON_ESCAPE 10

/**
   Characters that have been read and returned by the sequence matching code
*/
static wint_t lookahead_arr[1024];

/**
   Number of entries in lookahead_arr
*/
static int lookahead_count = 0;

/** Callback function for handling interrupts on reading */
static int (*interrupt_handler)();

/** Callback function to be invoked before reading each byte */
static void (*poll_handler)();


void input_common_init(int (*ih)())
{
    interrupt_handler = ih;
}

void input_common_set_poll_callback(void (*handler)(void))
{
    poll_handler = handler;
}

void input_common_destroy()
{

}

/**
   Internal function used by input_common_readch to read one byte from fd 1. This function should only be called by
   input_common_readch().
*/
static wint_t readb()
{
    /* do_loop must be set on every path through the loop; leaving it uninitialized allows the static analyzer to assist in catching mistakes. */
    unsigned char arr[1];
    bool do_loop;

    do
    {
        /* Invoke any poll handler */
        if (poll_handler)
            poll_handler();

        fd_set fdset;
        int fd_max=0;
        int ioport = iothread_port();
        int res;

        FD_ZERO(&fdset);
        FD_SET(0, &fdset);
        if (env_universal_server.fd > 0)
        {
            FD_SET(env_universal_server.fd, &fdset);
            if (fd_max < env_universal_server.fd) fd_max = env_universal_server.fd;
        }
        if (ioport > 0)
        {
            FD_SET(ioport, &fdset);
            if (fd_max < ioport) fd_max = ioport;
        }

        res = select(fd_max + 1, &fdset, 0, 0, 0);
        if (res==-1)
        {
            switch (errno)
            {
                case EINTR:
                case EAGAIN:
                {
                    if (interrupt_handler)
                    {
                        int res = interrupt_handler();
                        if (res)
                        {
                            return res;
                        }
                        if (lookahead_count)
                        {
                            return lookahead_arr[--lookahead_count];
                        }

                    }


                    do_loop = true;
                    break;
                }
                default:
                {
                    /*
                      The terminal has been closed. Save and exit.
                    */
                    return R_EOF;
                }
            }
        }
        else
        {
            /* Assume we loop unless we see a character in stdin */
            do_loop = true;

            if (env_universal_server.fd > 0 && FD_ISSET(env_universal_server.fd, &fdset))
            {
                debug(3, L"Wake up on universal variable event");
                env_universal_read_all();
                if (lookahead_count)
                {
                    return lookahead_arr[--lookahead_count];
                }
            }

            if (ioport > 0 && FD_ISSET(ioport, &fdset))
            {
                iothread_service_completion();
                if (lookahead_count)
                {
                    return lookahead_arr[--lookahead_count];
                }
            }

            if (FD_ISSET(STDIN_FILENO, &fdset))
            {
                if (read_blocked(0, arr, 1) != 1)
                {
                    /* The teminal has been closed. Save and exit. */
                    return R_EOF;
                }

                /* We read from stdin, so don't loop */
                do_loop = false;
            }
        }
    }
    while (do_loop);

    return arr[0];
}

wchar_t input_common_readch(int timed)
{
    if (lookahead_count == 0)
    {
        if (timed)
        {
            int count;
            fd_set fds;
            struct timeval tm=
            {
                0,
                1000 * WAIT_ON_ESCAPE
            }
            ;

            FD_ZERO(&fds);
            FD_SET(0, &fds);
            count = select(1, &fds, 0, 0, &tm);

            switch (count)
            {
                case 0:
                    return WEOF;

                case -1:
                    return WEOF;
                    break;
                default:
                    break;

            }
        }

        wchar_t res;
        static mbstate_t state;

        while (1)
        {
            wint_t b = readb();
            char bb;

            size_t sz;

            if ((b >= R_NULL) && (b < R_NULL + 1000))
                return b;

            bb=b;

            sz = mbrtowc(&res, &bb, 1, &state);

            switch (sz)
            {
                case (size_t)(-1):
                    memset(&state, '\0', sizeof(state));
                    debug(2, L"Illegal input");
                    return R_NULL;
                case (size_t)(-2):
                    break;
                case 0:
                    return 0;
                default:

                    return res;
            }
        }
    }
    else
    {
        if (!timed)
        {
            while ((lookahead_count >= 0) && (lookahead_arr[lookahead_count-1] == WEOF))
                lookahead_count--;
            if (lookahead_count == 0)
                return input_common_readch(0);
        }

        return lookahead_arr[--lookahead_count];
    }
}


void input_common_unreadch(wint_t ch)
{
    lookahead_arr[lookahead_count++] = ch;
}

