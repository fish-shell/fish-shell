// Implementation file for the low level input library.
#include "config.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <algorithm>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <cwchar>
#include <deque>
#include <utility>

#include "common.h"
#include "env.h"
#include "env_universal_common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fds.h"
#include "flog.h"
#include "input_common.h"
#include "iothread.h"
#include "wutil.h"

/// Time in milliseconds to wait for another byte to be available for reading
/// after \x1B is read before assuming that escape key was pressed, and not an
/// escape sequence.
#define WAIT_ON_ESCAPE_DEFAULT 30
static int wait_on_escape_ms = WAIT_ON_ESCAPE_DEFAULT;

input_event_queue_t::input_event_queue_t(int in) : in_(in) {}

/// Internal function used by readch to read one byte.
/// This calls select() on three fds: input (e.g. stdin), the ioport notifier fd (for main thread
/// requests), and the uvar notifier. This returns either the byte which was read, or one of the
/// special values below.
enum {
    // The in fd has been closed.
    readb_eof = -1,

    // select() was interrupted by a signal.
    readb_interrupted = -2,

    // Our uvar notifier reported a change (either through poll() or its fd).
    readb_uvar_notified = -3,

    // Our ioport reported a change, so service main thread requests.
    readb_ioport_notified = -4,
};
using readb_result_t = int;

static readb_result_t readb(int in_fd) {
    assert(in_fd >= 0 && "Invalid in fd");
    universal_notifier_t& notifier = universal_notifier_t::default_notifier();
    fd_readable_set_t fdset;
    for (;;) {
        fdset.clear();
        fdset.add(in_fd);

        // Add the completion ioport.
        int ioport_fd = iothread_port();
        fdset.add(ioport_fd);

        // Get the uvar notifier fd (possibly none).
        int notifier_fd = notifier.notification_fd();
        fdset.add(notifier_fd);

        // Get its suggested delay (possibly none).
        // Note a 0 here means do not poll.
        uint64_t timeout = fd_readable_set_t::kNoTimeout;
        if (uint64_t usecs_delay = notifier.usec_delay_between_polls()) {
            timeout = usecs_delay;
        }

        // Here's where we call select().
        int select_res = fdset.check_readable(timeout);
        if (select_res < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                // A signal.
                return readb_interrupted;
            } else {
                // Some fd was invalid, so probably the tty has been closed.
                return readb_eof;
            }
        }

        // select() did not return an error, so we may have a readable fd.
        // The priority order is: uvars, stdin, ioport.
        // Check to see if we want a universal variable barrier.
        // This may come about through readability, or through a call to poll().
        if ((fdset.test(notifier_fd) && notifier.notification_fd_became_readable(notifier_fd)) ||
            notifier.poll()) {
            return readb_uvar_notified;
        }

        // Check stdin.
        if (fdset.test(in_fd)) {
            unsigned char arr[1];
            if (read_blocked(in_fd, arr, 1) != 1) {
                // The terminal has been closed.
                return readb_eof;
            }
            // The common path is to return a (non-negative) char.
            return static_cast<int>(arr[0]);
        }

        // Check for iothread completions only if there is no data to be read from the stdin.
        // This gives priority to the foreground.
        if (fdset.test(ioport_fd)) {
            return readb_ioport_notified;
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
    wchar_t res{};
    mbstate_t state = {};
    for (;;) {
        // Do we have something enqueued already?
        // Note this may be initially true, or it may become true through calls to
        // iothread_service_main() or env_universal_barrier() below.
        if (auto mevt = try_pop()) {
            return mevt.acquire();
        }

        // We are going to block; but first allow any override to inject events.
        this->prepare_to_select();
        if (auto mevt = try_pop()) {
            return mevt.acquire();
        }

        readb_result_t rr = readb(in_);
        switch (rr) {
            case readb_eof:
                return char_event_type_t::eof;

            case readb_interrupted:
                // FIXME: here signals may break multibyte sequences.
                this->select_interrupted();
                break;

            case readb_uvar_notified:
                this->uvar_change_notified();
                break;

            case readb_ioport_notified:
                iothread_service_main();
                break;

            default: {
                assert(rr >= 0 && rr <= UCHAR_MAX &&
                       "Read byte out of bounds - missing error case?");
                char read_byte = static_cast<char>(static_cast<unsigned char>(rr));
                if (MB_CUR_MAX == 1) {
                    // single-byte locale, all values are legal
                    res = read_byte;
                    return res;
                }
                size_t sz = std::mbrtowc(&res, &read_byte, 1, &state);
                switch (sz) {
                    case static_cast<size_t>(-1):
                        std::memset(&state, '\0', sizeof(state));
                        FLOG(reader, L"Illegal input");
                        return char_event_type_t::check_exit;

                    case static_cast<size_t>(-2):
                        // Sequence not yet complete.
                        break;

                    case 0:
                        // Actual nul char.
                        return 0;

                    default:
                        // Sequence complete.
                        return res;
                }
                break;
            }
        }
    }
}

maybe_t<char_event_t> input_event_queue_t::readch_timed() {
    if (auto evt = try_pop()) {
        return evt;
    }
    // We are not prepared to handle a signal immediately; we only want to know if we get input on
    // our fd before the timeout. Use pselect to block all signals; we will handle signals
    // before the next call to getch().
    sigset_t sigs;
    sigfillset(&sigs);

    // pselect expects timeouts in nanoseconds.
    const uint64_t nsec_per_msec = 1000 * 1000;
    const uint64_t nsec_per_sec = nsec_per_msec * 1000;
    const uint64_t wait_nsec = wait_on_escape_ms * nsec_per_msec;
    struct timespec timeout;
    timeout.tv_sec = (wait_nsec) / nsec_per_sec;
    timeout.tv_nsec = (wait_nsec) % nsec_per_sec;

    // We have one fd of interest.
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(in_, &fdset);

    int res = pselect(in_ + 1, &fdset, nullptr, nullptr, &timeout, &sigs);
    if (res > 0) {
        return readch();
    }
    return none();
}

void input_event_queue_t::push_back(const char_event_t& ch) { queue_.push_back(ch); }

void input_event_queue_t::push_front(const char_event_t& ch) { queue_.push_front(ch); }

void input_event_queue_t::promote_interruptions_to_front() {
    // Find the first sequence of non-char events.
    // EOF is considered a char: we don't want to pull EOF in front of real chars.
    auto is_char = [](const char_event_t& ch) { return ch.is_char() || ch.is_eof(); };
    auto first = std::find_if_not(queue_.begin(), queue_.end(), is_char);
    auto last = std::find_if(first, queue_.end(), is_char);
    std::rotate(queue_.begin(), first, last);
}

void input_event_queue_t::prepare_to_select() {}
void input_event_queue_t::select_interrupted() {}
void input_event_queue_t::uvar_change_notified() {}
input_event_queue_t::~input_event_queue_t() = default;
