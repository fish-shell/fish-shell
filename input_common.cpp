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
#include <stack>
#include <list>
#include <queue>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "wutil.h"
#include "input_common.h"
#include "env_universal_common.h"
#include "iothread.h"

/**
   Time in milliseconds to wait for another byte to be available for
   reading after \\x1b is read before assuming that escape key was
   pressed, and not an escape sequence.
*/
#define WAIT_ON_ESCAPE 10

/** Characters that have been read and returned by the sequence matching code */
static std::deque<wint_t> lookahead_list;

/* Queue of pairs of (function pointer, argument) to be invoked. Expected to be mostly empty. */
typedef std::pair<void (*)(void *), void *> callback_info_t;
typedef std::queue<callback_info_t, std::list<callback_info_t> > callback_queue_t;
static callback_queue_t callback_queue;
static void input_flush_callbacks(void);

static bool has_lookahead(void)
{
    return ! lookahead_list.empty();
}

static wint_t lookahead_pop(void)
{
    wint_t result = lookahead_list.front();
    lookahead_list.pop_front();
    return result;
}

static void lookahead_push_back(wint_t c)
{
    lookahead_list.push_back(c);
}

static void lookahead_push_front(wint_t c)
{
    lookahead_list.push_front(c);
}

static wint_t lookahead_front(void)
{
    return lookahead_list.front();
}

/** Callback function for handling interrupts on reading */
static int (*interrupt_handler)();

void input_common_init(int (*ih)())
{
    interrupt_handler = ih;
}

void input_common_destroy()
{

}

/**
   Internal function used by input_common_readch to read one byte from fd 0. This function should only be called by
   input_common_readch().
*/
static wint_t readb()
{
    /* do_loop must be set on every path through the loop; leaving it uninitialized allows the static analyzer to assist in catching mistakes. */
    unsigned char arr[1];
    bool do_loop;

    do
    {
        /* Flush callbacks */
        input_flush_callbacks();

        fd_set fdset;
        int fd_max = 0;
        int ioport = iothread_port();
        int res;

        FD_ZERO(&fdset);
        FD_SET(0, &fdset);
        if (ioport > 0)
        {
            FD_SET(ioport, &fdset);
            fd_max = maxi(fd_max, ioport);
        }
        
        /* Get our uvar notifier */
        universal_notifier_t &notifier = universal_notifier_t::default_notifier();
        
        /* Get the notification fd (possibly none) */
        int notifier_fd = notifier.notification_fd();
        if (notifier_fd > 0)
        {
            FD_SET(notifier_fd, &fdset);
            fd_max = maxi(fd_max, notifier_fd);
        }
        
        /* Get its suggested delay (possibly none) */
        struct timeval tv = {};
        const unsigned long usecs_delay = notifier.usec_delay_between_polls();
        if (usecs_delay > 0)
        {
            unsigned long usecs_per_sec = 1000000;
            tv.tv_sec = (int)(usecs_delay / usecs_per_sec);
            tv.tv_usec = (int)(usecs_delay % usecs_per_sec);
        }
        
        res = select(fd_max + 1, &fdset, 0, 0, usecs_delay > 0 ? &tv : NULL);
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
                        if (has_lookahead())
                        {
                            return lookahead_pop();
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

            /* Check to see if we want a universal variable barrier */
            bool barrier_from_poll = notifier.poll();
            bool barrier_from_readability = false;
            if (notifier_fd > 0 && FD_ISSET(notifier_fd, &fdset))
            {
                barrier_from_readability = notifier.notification_fd_became_readable(notifier_fd);
            }
            if (barrier_from_poll || barrier_from_readability)
            {
                env_universal_barrier();
            }

            if (ioport > 0 && FD_ISSET(ioport, &fdset))
            {
                iothread_service_completion();
                if (has_lookahead())
                {
                    return lookahead_pop();
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
    if (! has_lookahead())
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
        mbstate_t state = {};

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
            while (has_lookahead() && lookahead_front() == WEOF)
                lookahead_pop();
            if (! has_lookahead())
                return input_common_readch(0);
        }

        return lookahead_pop();
    }
}


void input_common_queue_ch(wint_t ch)
{
    lookahead_push_back(ch);
}

void input_common_next_ch(wint_t ch)
{
    lookahead_push_front(ch);
}

void input_common_add_callback(void (*callback)(void *), void *arg)
{
    ASSERT_IS_MAIN_THREAD();
    callback_queue.push(callback_info_t(callback, arg));
}

static void input_flush_callbacks(void)
{
    /* Nothing to do if nothing to do */
    if (callback_queue.empty())
        return;

    /* We move the queue into a local variable, so that events queued up during a callback don't get fired until next round. */
    callback_queue_t local_queue;
    std::swap(local_queue, callback_queue);
    while (! local_queue.empty())
    {
        const callback_info_t &callback = local_queue.front();
        callback.first(callback.second); //cute
        local_queue.pop();
    }
}
