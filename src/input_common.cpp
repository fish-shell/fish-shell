// Implementation file for the low level input library.
#include "config.h"

#include <errno.h>
#include <cstring>
#include <unistd.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <cwchar>

#include <deque>
#include <list>
#include <memory>
#include <type_traits>
#include <utility>

#include "common.h"
#include "env.h"
#include "env_universal_common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "input_common.h"
#include "iothread.h"
#include "wutil.h"

/// Time in milliseconds to wait for another byte to be available for reading
/// after \x1B is read before assuming that escape key was pressed, and not an
/// escape sequence.
#define WAIT_ON_ESCAPE_DEFAULT 30
static int wait_on_escape_ms = WAIT_ON_ESCAPE_DEFAULT;

/// Events which have been read and returned by the sequence matching code.
static std::deque<char_event_t> lookahead_list;

// Queue of pairs of (function pointer, argument) to be invoked. Expected to be mostly empty.
typedef std::list<std::function<void(void)>> callback_queue_t;
static callback_queue_t callback_queue;
static void input_flush_callbacks();

static bool has_lookahead() { return !lookahead_list.empty(); }

static char_event_t lookahead_pop() {
    auto result = lookahead_list.front();
    lookahead_list.pop_front();
    return result;
}

/// \return the next lookahead char, or none if none. Discards timeouts.
static maybe_t<char_event_t> lookahead_pop_evt() {
    while (has_lookahead()) {
        auto evt = lookahead_pop();
        if (! evt.is_timeout()) {
            return evt;
        }
    }
    return none();
}

static void lookahead_push_back(char_event_t c) { lookahead_list.push_back(c); }

static void lookahead_push_front(char_event_t c) { lookahead_list.push_front(c); }

/// Callback function for handling interrupts on reading.
static interrupt_func_t interrupt_handler;

void input_common_init(interrupt_func_t func) { interrupt_handler = func; }

/// Internal function used by input_common_readch to read one byte from fd 0. This function should
/// only be called by input_common_readch().
static char_event_t readb() {
    // do_loop must be set on every path through the loop; leaving it uninitialized allows the
    // static analyzer to assist in catching mistakes.
    unsigned char arr[1];
    bool do_loop;

    do {
        // Flush callbacks.
        input_flush_callbacks();

        fd_set fdset;
        int fd_max = 0;
        int ioport = iothread_port();
        int res;

        FD_ZERO(&fdset);
        FD_SET(0, &fdset);
        if (ioport > 0) {
            FD_SET(ioport, &fdset);
            fd_max = std::max(fd_max, ioport);
        }

        // Get our uvar notifier.
        universal_notifier_t &notifier = universal_notifier_t::default_notifier();

        // Get the notification fd (possibly none).
        int notifier_fd = notifier.notification_fd();
        if (notifier_fd > 0) {
            FD_SET(notifier_fd, &fdset);
            fd_max = std::max(fd_max, notifier_fd);
        }

        // Get its suggested delay (possibly none).
        struct timeval tv = {};
        const unsigned long usecs_delay = notifier.usec_delay_between_polls();
        if (usecs_delay > 0) {
            unsigned long usecs_per_sec = 1000000;
            tv.tv_sec = (int)(usecs_delay / usecs_per_sec);
            tv.tv_usec = (int)(usecs_delay % usecs_per_sec);
        }

        res = select(fd_max + 1, &fdset, 0, 0, usecs_delay > 0 ? &tv : NULL);
        if (res == -1) {
            if (errno == EINTR || errno == EAGAIN) {
                if (interrupt_handler) {
                    if (auto interrupt_evt = interrupt_handler()) {
                        return *interrupt_evt;
                    } else if (auto mc = lookahead_pop_evt()) {
                        return *mc;
                    }
                }

                do_loop = true;
            } else {
                // The terminal has been closed.
                return char_event_type_t::eof;
            }
        } else {
            // Assume we loop unless we see a character in stdin.
            do_loop = true;

            // Check to see if we want a universal variable barrier.
            bool barrier_from_poll = notifier.poll();
            bool barrier_from_readability = false;
            if (notifier_fd > 0 && FD_ISSET(notifier_fd, &fdset)) {
                barrier_from_readability = notifier.notification_fd_became_readable(notifier_fd);
            }
            if (barrier_from_poll || barrier_from_readability) {
                env_universal_barrier();
            }

            if (ioport > 0 && FD_ISSET(ioport, &fdset)) {
                iothread_service_completion();
                if (auto mc = lookahead_pop_evt()) {
                    return *mc;
                }
            }

            if (FD_ISSET(STDIN_FILENO, &fdset)) {
                if (read_blocked(0, arr, 1) != 1) {
                    // The teminal has been closed.
                    return char_event_type_t::eof;
                }

                // We read from stdin, so don't loop.
                do_loop = false;
            }
        }
    } while (do_loop);

    return arr[0];
}

// Update the wait_on_escape_ms value in response to the fish_escape_delay_ms user variable being
// set.
void update_wait_on_escape_ms(const environment_t &vars) {
    auto escape_time_ms = vars.get(L"fish_escape_delay_ms");
    if (escape_time_ms.missing_or_empty()) {
        wait_on_escape_ms = WAIT_ON_ESCAPE_DEFAULT;
        return;
    }

    long tmp = fish_wcstol(escape_time_ms->as_string().c_str());
    if (errno || tmp < 10 || tmp >= 5000) {
        std::fwprintf(stderr,
                 L"ignoring fish_escape_delay_ms: value '%ls' "
                 L"is not an integer or is < 10 or >= 5000 ms\n",
                 escape_time_ms->as_string().c_str());
    } else {
        wait_on_escape_ms = (int)tmp;
    }
}

char_event_t input_common_readch() {
    if (auto mc = lookahead_pop_evt()) {
        return *mc;
    }
    wchar_t res;
    mbstate_t state = {};
    while (1) {
        auto evt = readb();
        if (!evt.is_char() || evt.is_readline()) {
            return evt;
        }

        wint_t b = evt.get_char();
        if (b >= R_NULL && b < R_END_INPUT_FUNCTIONS) return b;

        if (MB_CUR_MAX == 1) {
            return b;  // single-byte locale, all values are legal
        }

        char bb = b;
        size_t sz = std::mbrtowc(&res, &bb, 1, &state);

        switch (sz) {
            case (size_t)(-1): {
                std::memset(&state, '\0', sizeof(state));
                debug(2, L"Illegal input");
                return char_event_type_t::check_exit;
            }
            case (size_t)(-2): {
                break;
            }
            case 0: {
                return 0;
            }
            default: { return res; }
        }
    }
}

char_event_t input_common_readch_timed(bool dequeue_timeouts) {
    char_event_t result{char_event_type_t::timeout};
    if (has_lookahead()) {
        result = lookahead_pop();
    } else {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tm = {wait_on_escape_ms / 1000, 1000 * (wait_on_escape_ms % 1000)};
        if (select(1, &fds, 0, 0, &tm) > 0) {
            result = input_common_readch();
        }
    }
    // If we got a timeout, either through dequeuing or creating, ensure it stays on the queue.
    if (result.is_timeout()) {
        if (!dequeue_timeouts) lookahead_push_front(char_event_type_t::timeout);
        return char_event_type_t::timeout;
    }
    return result.get_char();
}

void input_common_queue_ch(char_event_t ch) { lookahead_push_back(ch); }

void input_common_next_ch(char_event_t ch) { lookahead_push_front(ch); }

void input_common_add_callback(std::function<void(void)> callback) {
    ASSERT_IS_MAIN_THREAD();
    callback_queue.push_back(std::move(callback));
}

static void input_flush_callbacks() {
    // We move the queue into a local variable, so that events queued up during a callback don't get
    // fired until next round.
    ASSERT_IS_MAIN_THREAD();
    callback_queue_t local_queue;
    std::swap(local_queue, callback_queue);
    for (auto &f : local_queue) {
        f();
    }
}
