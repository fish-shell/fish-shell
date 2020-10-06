// Functions for handling event triggers.
#include "config.h"  // IWYU pragma: keep

#include "event.h"

#include <signal.h>
#include <stddef.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "input_common.h"
#include "io.h"
#include "parser.h"
#include "proc.h"
#include "signal.h"
#include "wutil.h"  // IWYU pragma: keep

class pending_signals_t {
    static constexpr size_t SIGNAL_COUNT = NSIG;

    /// A counter that is incremented each time a pending signal is received.
    std::atomic<uint32_t> counter_{0};

    /// List of pending signals.
    std::array<std::atomic<bool>, SIGNAL_COUNT> received_{};

    /// The last counter visible in acquire_pending().
    /// This is not accessed from a signal handler.
    owning_lock<uint32_t> last_counter_{0};

   public:
    pending_signals_t() = default;

    /// No copying.
    pending_signals_t(const pending_signals_t &);
    void operator=(const pending_signals_t &);

    /// Mark a signal as pending. This may be called from a signal handler.
    /// We expect only one signal handler to execute at once.
    /// Also note that these may be coalesced.
    void mark(int which) {
        if (which >= 0 && static_cast<size_t>(which) < received_.size()) {
            // Must mark our received first, then pending.
            received_[which].store(true, std::memory_order_relaxed);
            uint32_t count = counter_.load(std::memory_order_relaxed);
            counter_.store(1 + count, std::memory_order_release);
        }
    }

    /// \return the list of signals that were set, clearing them.
    std::bitset<SIGNAL_COUNT> acquire_pending() {
        auto current = last_counter_.acquire();

        // Check the counter first. If it hasn't changed, no signals have been received.
        uint32_t count = counter_.load(std::memory_order_acquire);
        if (count == *current) {
            return {};
        }

        // The signal count has changed. Store the new counter and fetch all the signals that are
        // set.
        *current = count;
        std::bitset<SIGNAL_COUNT> result{};
        uint32_t bit = 0;
        for (auto &signal : received_) {
            bool val = signal.load(std::memory_order_relaxed);
            if (val) {
                result.set(bit);
                signal.store(false, std::memory_order_relaxed);
            }
            bit++;
        }
        return result;
    }
};

static pending_signals_t s_pending_signals;

/// List of event handlers.
static owning_lock<event_handler_list_t> s_event_handlers;

/// Variables (one per signal) set when a signal is observed. This is inspected by a signal handler.
static volatile sig_atomic_t s_observed_signals[NSIG] = {};

static void set_signal_observed(int sig, bool val) {
    if (sig >= 0 &&
        static_cast<size_t>(sig) < sizeof s_observed_signals / sizeof *s_observed_signals) {
        s_observed_signals[sig] = val;
    }
}

/// Tests if one event instance matches the definition of a event class.
static bool handler_matches(const event_handler_t &classv, const event_t &instance) {
    if (classv.desc.type == event_type_t::any) return true;
    if (classv.desc.type != instance.desc.type) return false;

    switch (classv.desc.type) {
        case event_type_t::signal: {
            return classv.desc.param1.signal == instance.desc.param1.signal;
        }
        case event_type_t::variable: {
            return instance.desc.str_param1 == classv.desc.str_param1;
        }
        case event_type_t::exit: {
            if (classv.desc.param1.pid == EVENT_ANY_PID) return true;
            return classv.desc.param1.pid == instance.desc.param1.pid;
        }
        case event_type_t::caller_exit: {
            return classv.desc.param1.caller_id == instance.desc.param1.caller_id;
        }
        case event_type_t::generic: {
            return classv.desc.str_param1 == instance.desc.str_param1;
        }
        case event_type_t::any:
        default: {
            DIE("unexpected classv.type");
            return false;
        }
    }
}

/// Test if specified event is blocked.
static int event_is_blocked(parser_t &parser, const event_t &e) {
    (void)e;
    const block_t *block;
    size_t idx = 0;
    while ((block = parser.block_at_index(idx++))) {
        if (event_block_list_blocks_type(block->event_blocks)) return true;
    }
    return event_block_list_blocks_type(parser.global_event_blocks);
}

wcstring event_get_desc(const parser_t &parser, const event_t &evt) {
    const event_description_t &ed = evt.desc;
    switch (ed.type) {
        case event_type_t::signal: {
            return format_string(_(L"signal handler for %ls (%ls)"), sig2wcs(ed.param1.signal),
                                 signal_get_desc(ed.param1.signal));
        }

        case event_type_t::variable: {
            return format_string(_(L"handler for variable '%ls'"), ed.str_param1.c_str());
        }

        case event_type_t::exit: {
            if (ed.param1.pid > 0) {
                return format_string(_(L"exit handler for process %d"), ed.param1.pid);
            } else {
                // In events, PGIDs are stored as negative PIDs
                job_t *j = parser.job_get_from_pid(-ed.param1.pid);
                if (j) {
                    return format_string(_(L"exit handler for job %d, '%ls'"), j->job_id(),
                                         j->command_wcstr());
                } else {
                    return format_string(_(L"exit handler for job with process group %d"),
                                         -ed.param1.pid);
                }
            }
            DIE("Unreachable");
        }

        case event_type_t::caller_exit: {
            return _(L"exit handler for command substitution caller");
        }

        case event_type_t::generic: {
            return format_string(_(L"handler for generic event '%ls'"), ed.str_param1.c_str());
        }
        case event_type_t::any: {
            DIE("Unreachable");
        }
        default:
            DIE("Unknown event type");
    }
}

void event_add_handler(std::shared_ptr<event_handler_t> eh) {
    if (eh->desc.type == event_type_t::signal) {
        signal_handle(eh->desc.param1.signal);
        set_signal_observed(eh->desc.param1.signal, true);
    }

    s_event_handlers.acquire()->push_back(std::move(eh));
}

void event_remove_function_handlers(const wcstring &name) {
    auto handlers = s_event_handlers.acquire();
    auto begin = handlers->begin(), end = handlers->end();
    handlers->erase(std::remove_if(begin, end,
                                   [&](const shared_ptr<event_handler_t> &eh) {
                                       return eh->function_name == name;
                                   }),
                    end);
}

event_handler_list_t event_get_function_handlers(const wcstring &name) {
    auto handlers = s_event_handlers.acquire();
    event_handler_list_t result;
    for (const shared_ptr<event_handler_t> &eh : *handlers) {
        if (eh->function_name == name) {
            result.push_back(eh);
        }
    }
    return result;
}

bool event_is_signal_observed(int sig) {
    // We are in a signal handler! Don't allocate memory, etc.
    bool result = false;
    if (sig >= 0 && static_cast<unsigned long>(sig) <
                        sizeof(s_observed_signals) / sizeof(*s_observed_signals)) {
        result = s_observed_signals[sig];
    }
    return result;
}

/// Perform the specified event. Since almost all event firings will not be matched by even a single
/// event handler, we make sure to optimize the 'no matches' path. This means that nothing is
/// allocated/initialized unless needed.
static void event_fire_internal(parser_t &parser, const event_t &event) {
    auto &ld = parser.libdata();
    assert(ld.is_event >= 0 && "is_event should not be negative");
    scoped_push<decltype(ld.is_event)> inc_event{&ld.is_event, ld.is_event + 1};

    // Suppress fish_trace during events.
    scoped_push<bool> suppress_trace{&ld.suppress_fish_trace, true};

    // Capture the event handlers that match this event.
    event_handler_list_t fire;
    for (const auto &handler : *s_event_handlers.acquire()) {
        // Check if this event is a match.
        if (handler_matches(*handler, event)) {
            fire.push_back(handler);
        }
    }

    // Iterate over our list of matching events. Fire the ones that are still present.
    for (const shared_ptr<event_handler_t> &handler : fire) {
        // Only fire if this event is still present.
        // TODO: this is kind of crazy. We want to support removing (and thereby suppressing) an
        // event handler from another, but we also don't want to hold the lock across callouts. How
        // can we make this less silly?
        if (!contains(*s_event_handlers.acquire(), handler)) {
            continue;
        }

        // Construct a buffer to evaluate, starting with the function name and then all the
        // arguments.
        wcstring buffer = handler->function_name;
        for (const wcstring &arg : event.arguments) {
            buffer.push_back(L' ');
            buffer.append(escape_string(arg, ESCAPE_ALL));
        }

        // Event handlers are not part of the main flow of code, so they are marked as
        // non-interactive.
        scoped_push<bool> interactive{&ld.is_interactive, false};
        auto prev_statuses = parser.get_last_statuses();

        FLOGF(event, L"Firing event '%ls'", event.desc.str_param1.c_str());
        block_t *b = parser.push_block(block_t::event_block(event));
        parser.eval(buffer, io_chain_t());
        parser.pop_block(b);
        parser.set_last_statuses(std::move(prev_statuses));
    }
}

/// Handle all pending signal events.
void event_fire_delayed(parser_t &parser) {
    auto &ld = parser.libdata();
    // Do not invoke new event handlers from within event handlers.
    if (ld.is_event) return;
    // Do not invoke new event handlers if we are unwinding (#6649).
    if (signal_check_cancel()) return;

    std::vector<shared_ptr<const event_t>> to_send;
    to_send.swap(ld.blocked_events);
    assert(ld.blocked_events.empty());

    // Append all signal events to to_send.
    auto signals = s_pending_signals.acquire_pending();
    if (signals.any()) {
        for (uint32_t sig = 0; sig < signals.size(); sig++) {
            if (signals.test(sig)) {
                auto e = std::make_shared<event_t>(event_type_t::signal);
                e->desc.param1.signal = sig;
                e->arguments.push_back(sig2wcs(sig));
                to_send.push_back(std::move(e));
            }
        }
    }

    // Fire or re-block all events.
    for (const auto &evt : to_send) {
        if (event_is_blocked(parser, *evt)) {
            ld.blocked_events.push_back(evt);
        } else {
            event_fire_internal(parser, *evt);
        }
    }
}

void event_enqueue_signal(int signal) {
    // Beware, we are in a signal handler
    s_pending_signals.mark(signal);
}

void event_fire(parser_t &parser, const event_t &event) {
    // Fire events triggered by signals.
    event_fire_delayed(parser);

    if (event_is_blocked(parser, event)) {
        parser.libdata().blocked_events.push_back(std::make_shared<event_t>(event));
    } else {
        event_fire_internal(parser, event);
    }
}

/// Mapping between event type to name.
/// Note we don't bother to sort this.
struct event_type_name_t {
    event_type_t type;
    const wchar_t *name;
};

static const event_type_name_t events_mapping[] = {{event_type_t::signal, L"signal"},
                                                   {event_type_t::variable, L"variable"},
                                                   {event_type_t::exit, L"exit"},
                                                   {event_type_t::caller_exit, L"caller-exit"},
                                                   {event_type_t::generic, L"generic"}};

maybe_t<event_type_t> event_type_for_name(const wcstring &name) {
    for (const auto &em : events_mapping) {
        if (name == em.name) {
            return em.type;
        }
    }
    return none();
}

static const wchar_t *event_name_for_type(event_type_t type) {
    for (const auto &em : events_mapping) {
        if (type == em.type) {
            return em.name;
        }
    }
    return L"";
}

void event_print(io_streams_t &streams, maybe_t<event_type_t> type_filter) {
    event_handler_list_t tmp = *s_event_handlers.acquire();
    std::sort(tmp.begin(), tmp.end(),
              [](const shared_ptr<event_handler_t> &e1, const shared_ptr<event_handler_t> &e2) {
                  const event_description_t &d1 = e1->desc;
                  const event_description_t &d2 = e2->desc;
                  if (d1.type != d2.type) {
                      return d1.type < d2.type;
                  }
                  switch (d1.type) {
                      case event_type_t::signal:
                          return d1.signal < d2.signal;
                      case event_type_t::exit:
                          return d1.param1.pid < d2.param1.pid;
                      case event_type_t::caller_exit:
                          return d1.param1.caller_id < d2.param1.caller_id;
                      case event_type_t::variable:
                      case event_type_t::any:
                      case event_type_t::generic:
                          return d1.str_param1 < d2.str_param1;
                  }
                  DIE("Unreachable");
              });

    maybe_t<event_type_t> last_type{};
    for (const shared_ptr<event_handler_t> &evt : tmp) {
        // If we have a filter, skip events that don't match.
        if (type_filter && *type_filter != evt->desc.type) {
            continue;
        }

        if (!last_type || *last_type != evt->desc.type) {
            if (last_type) streams.out.append(L"\n");
            last_type = evt->desc.type;
            streams.out.append_format(L"Event %ls\n", event_name_for_type(*last_type));
        }
        switch (evt->desc.type) {
            case event_type_t::signal:
                streams.out.append_format(L"%ls %ls\n", sig2wcs(evt->desc.param1.signal),
                                          evt->function_name.c_str());
                break;
            case event_type_t::exit:
                break;
            case event_type_t::caller_exit:
                streams.out.append_format(L"caller-exit %ls\n", evt->function_name.c_str());
                break;
            case event_type_t::variable:
            case event_type_t::generic:
                streams.out.append_format(L"%ls %ls\n", evt->desc.str_param1.c_str(),
                                          evt->function_name.c_str());
                break;
            case event_type_t::any:
                DIE("Unreachable");
            default:
                streams.out.append_format(L"%ls\n", evt->function_name.c_str());
                break;
        }
    }
}

void event_fire_generic(parser_t &parser, const wchar_t *name, const wcstring_list_t *args) {
    assert(name && "Null name");

    event_t ev(event_type_t::generic);
    ev.desc.str_param1 = name;
    if (args) ev.arguments = *args;
    event_fire(parser, ev);
}

event_description_t event_description_t::signal(int sig) {
    event_description_t event(event_type_t::signal);
    event.param1.signal = sig;
    return event;
}

event_description_t event_description_t::variable(wcstring str) {
    event_description_t event(event_type_t::variable);
    event.str_param1 = std::move(str);
    return event;
}

event_description_t event_description_t::generic(wcstring str) {
    event_description_t event(event_type_t::generic);
    event.str_param1 = std::move(str);
    return event;
}

event_t event_t::variable(wcstring name, wcstring_list_t args) {
    event_t evt{event_type_t::variable};
    evt.desc.str_param1 = std::move(name);
    evt.arguments = std::move(args);
    return evt;
}
