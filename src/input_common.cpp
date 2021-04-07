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

input_event_queue_t::input_event_queue_t(int in, interrupt_handler_t handler)
    : in_(in), interrupt_handler_(std::move(handler)) {}

/// Internal function used by input_common_readch to read one byte from fd 0. This function should
/// only be called by input_common_readch().
char_event_t input_event_queue_t::readb() {
    select_wrapper_t fdset;
    for (;;) {
        fdset.clear();
        fdset.add(in_);

        int ioport = iothread_port();
        if (ioport > 0) {
            fdset.add(ioport);
        }

        // Get our uvar notifier.
        universal_notifier_t& notifier = universal_notifier_t::default_notifier();
        int notifier_fd = notifier.notification_fd();
        if (notifier_fd > 0) {
            fdset.add(notifier_fd);
        }

        // Get its suggested delay (possibly none).
        uint64_t timeout_usec = select_wrapper_t::kNoTimeout;
        if (auto notifier_usec_delay = notifier.usec_delay_between_polls()) {
            timeout_usec = notifier_usec_delay;
        }
        int res = fdset.select(timeout_usec);
        if (res < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                // Some uvar notifiers rely on signals - see #7671.
                if (notifier.poll()) {
                    env_universal_barrier();
                }
                if (interrupt_handler_) {
                    if (auto interrupt_evt = interrupt_handler_()) {
                        return *interrupt_evt;
                    } else if (auto mc = try_pop()) {
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
            if (notifier_fd > 0 && fdset.test(notifier_fd)) {
                barrier_from_readability = notifier.notification_fd_became_readable(notifier_fd);
            }
            if (barrier_from_poll || barrier_from_readability) {
                if (env_universal_barrier()) {
                    // A variable change may have triggered a repaint, etc.
                    if (auto mc = try_pop()) {
                        return *mc;
                    }
                }
            }

            if (fdset.test(in_)) {
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
            if (ioport > 0 && fdset.test(ioport)) {
                iothread_service_main();
                if (auto mc = try_pop()) {
                    return mc.acquire();
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

maybe_t<char_event_t> input_event_queue_t::try_pop() {
    if (queue_.empty()) {
        return none();
    }
    auto result = std::move(queue_.front());
    queue_.pop_front();
    return result;
}

char_event_t input_event_queue_t::readch() {
    ASSERT_IS_MAIN_THREAD();
    if (auto mc = try_pop()) {
        return mc.acquire();
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

maybe_t<char_event_t> input_event_queue_t::readch_timed() {
    if (auto evt = try_pop()) {
        return evt;
    }
    const uint64_t usec_per_msec = 1000;
    uint64_t timeout_usec = static_cast<uint64_t>(wait_on_escape_ms) * usec_per_msec;
    if (select_wrapper_t::is_fd_readable(in_, timeout_usec)) {
        return readch();
    }
    return none();
}

void input_event_queue_t::push_back(const char_event_t& ch) { queue_.push_back(ch); }

void input_event_queue_t::push_front(const char_event_t& ch) { queue_.push_front(ch); }
