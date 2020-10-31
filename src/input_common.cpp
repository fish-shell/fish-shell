// Implementation file for the low level input library.
#include "config.h"

#include <errno.h>
#include <unistd.h>

#include <cstring>
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
#include "flog.h"
#include "global_safety.h"
#include "input_common.h"
#include "iothread.h"
#include "wutil.h"

/// Time in milliseconds to wait for another byte to be available for reading
/// after \x1B is read before assuming that escape key was pressed, and not an
/// escape sequence.
#define WAIT_ON_ESCAPE_DEFAULT 30
static int wait_on_escape_ms = WAIT_ON_ESCAPE_DEFAULT;

/// Callback function for handling interrupts on reading.
static interrupt_func_t interrupt_handler;

void input_common_init(interrupt_func_t func) { interrupt_handler = func; }

/// Internal function used by input_common_readch to read one byte from fd 0. This function should
/// only be called by input_common_readch().
char_event_t input_event_queue_t::readb() {
    for (;;) {
        fd_set fdset;
        int fd_max = in_;
        int ioport = iothread_port();
        int res;

        FD_ZERO(&fdset);
        FD_SET(in_, &fdset);
        if (ioport > 0) {
            FD_SET(ioport, &fdset);
            fd_max = std::max(fd_max, ioport);
        }

        // Get our uvar notifier.
        universal_notifier_t& notifier = universal_notifier_t::default_notifier();

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
            tv.tv_sec = static_cast<int>(usecs_delay / usecs_per_sec);
            tv.tv_usec = static_cast<int>(usecs_delay % usecs_per_sec);
        }

        res = select(fd_max + 1, &fdset, nullptr, nullptr, usecs_delay > 0 ? &tv : nullptr);
        if (res == -1) {
            if (errno == EINTR || errno == EAGAIN) {
                if (interrupt_handler) {
                    if (auto interrupt_evt = interrupt_handler()) {
                        return *interrupt_evt;
                    } else if (auto mc = pop_discard_timeouts()) {
                        return *mc;
                    }
                }
            } else {
                // The terminal has been closed.
                return char_event_type_t::eof;
            }
        } else {
            // Check to see if we want a universal variable barrier.
            bool barrier_from_poll = notifier.poll();
            bool barrier_from_readability = false;
            if (notifier_fd > 0 && FD_ISSET(notifier_fd, &fdset)) {
                barrier_from_readability = notifier.notification_fd_became_readable(notifier_fd);
            }
            if (barrier_from_poll || barrier_from_readability) {
                if (env_universal_barrier()) {
                    // A variable change may have triggered a repaint, etc.
                    if (auto mc = pop_discard_timeouts()) {
                        return *mc;
                    }
                }
            }

            if (FD_ISSET(in_, &fdset)) {
                unsigned char arr[1];
                if (read_blocked(in_, arr, 1) != 1) {
                    // The teminal has been closed.
                    return char_event_type_t::eof;
                }

                // We read from stdin, so don't loop.
                return arr[0];
            }

            // Check for iothread completions only if there is no data to be read from the stdin.
            // This gives priority to the foreground.
            if (ioport > 0 && FD_ISSET(ioport, &fdset)) {
                iothread_service_completion();
                if (auto mc = pop_discard_timeouts()) {
                    return *mc;
                }
            }
        }
    }
}

// Update the wait_on_escape_ms value in response to the fish_escape_delay_ms user variable being
// set.
void update_wait_on_escape_ms(const environment_t& vars) {
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
        wait_on_escape_ms = static_cast<int>(tmp);
    }
}

char_event_t input_event_queue_t::pop() {
    auto result = queue_.front();
    queue_.pop_front();
    return result;
}

maybe_t<char_event_t> input_event_queue_t::pop_discard_timeouts() {
    while (has_lookahead()) {
        auto evt = pop();
        if (!evt.is_timeout()) {
            return evt;
        }
    }
    return none();
}

char_event_t input_event_queue_t::readch() {
    ASSERT_IS_MAIN_THREAD();
    if (auto mc = pop_discard_timeouts()) {
        return *mc;
    }
    wchar_t res;
    mbstate_t state = {};
    while (true) {
        auto evt = readb();
        if (!evt.is_char()) {
            return evt;
        }

        wint_t b = evt.get_char();
        if (MB_CUR_MAX == 1) {
            return b;  // single-byte locale, all values are legal
        }

        char bb = b;
        size_t sz = std::mbrtowc(&res, &bb, 1, &state);

        switch (sz) {
            case static_cast<size_t>(-1): {
                std::memset(&state, '\0', sizeof(state));
                FLOG(reader, L"Illegal input");
                return char_event_type_t::check_exit;
            }
            case static_cast<size_t>(-2): {
                break;
            }
            case 0: {
                return 0;
            }
            default: {
                return res;
            }
        }
    }
}

char_event_t input_event_queue_t::readch_timed(bool dequeue_timeouts) {
    char_event_t result{char_event_type_t::timeout};
    if (has_lookahead()) {
        result = pop();
    } else {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(in_, &fds);
        struct timeval tm = {wait_on_escape_ms / 1000, 1000 * (wait_on_escape_ms % 1000)};
        if (select(1, &fds, nullptr, nullptr, &tm) > 0) {
            result = readch();
        }
    }
    // If we got a timeout, either through dequeuing or creating, ensure it stays on the queue.
    if (result.is_timeout()) {
        if (!dequeue_timeouts) queue_.push_front(char_event_type_t::timeout);
        return char_event_type_t::timeout;
    }
    return result;
}

void input_event_queue_t::push_back(const char_event_t& ch) { queue_.push_back(ch); }

void input_event_queue_t::push_front(const char_event_t& ch) { queue_.push_front(ch); }
