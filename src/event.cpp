// Functions for handling event triggers.
#include "config.h"  // IWYU pragma: keep

#include "event.h"

#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <memory>
#include <string>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "flog.h"
#include "io.h"
#include "maybe.h"
#include "parser.h"
#include "proc.h"
#include "signal.h"
#include "termsize.h"
#include "wcstringutil.h"
#include "wutil.h"  // IWYU pragma: keep

namespace {
class pending_signals_t {
    static constexpr size_t SIGNAL_COUNT = NSIG;

    /// A counter that is incremented each time a pending signal is received.
    std::atomic<uint32_t> counter_{0};

    /// List of pending signals.
    std::array<relaxed_atomic_bool_t, SIGNAL_COUNT> received_{};

    /// The last counter visible in acquire_pending().
    /// This is not accessed from a signal handler.
    owning_lock<uint32_t> last_counter_{0};

   public:
    pending_signals_t() = default;

    /// No copying.
    pending_signals_t(const pending_signals_t &) = delete;
    pending_signals_t &operator=(const pending_signals_t &) = delete;

    /// Mark a signal as pending. This may be called from a signal handler.
    /// We expect only one signal handler to execute at once.
    /// Also note that these may be coalesced.
    void mark(int which) {
        if (which >= 0 && static_cast<size_t>(which) < received_.size()) {
            // Must mark our received first, then pending.
            received_[which] = true;
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

        // The signal count has changed. Store the new counter and fetch all set signals.
        *current = count;
        std::bitset<SIGNAL_COUNT> result{};
        for (size_t i = 0; i < NSIG; i++) {
            if (received_[i]) {
                result.set(i);
                received_[i] = false;
            }
        }
        return result;
    }
};
}  // namespace

static pending_signals_t s_pending_signals;

/// List of event handlers.
static owning_lock<event_handler_list_t> s_event_handlers;

/// Tracks the number of registered event handlers for each signal.
/// This is inspected by a signal handler. We assume no values in here overflow.
static std::array<relaxed_atomic_t<uint32_t>, NSIG> s_observed_signals;

static inline void inc_signal_observed(int sig) {
    if (0 <= sig && sig < NSIG) {
        s_observed_signals[sig]++;
    }
}

static inline void dec_signal_observed(int sig) {
    if (0 <= sig && sig < NSIG) {
        s_observed_signals[sig]--;
    }
}

bool event_is_signal_observed(int sig) {
    // We are in a signal handler!
    uint32_t count = 0;
    if (0 <= sig && sig < NSIG) {
        count = s_observed_signals[sig];
    }
    return count > 0;
}

/// \return true if a handler is "one shot": it fires at most once.
static bool handler_is_one_shot(const event_handler_t &handler) {
    switch (handler.desc.type) {
        case event_type_t::process_exit:
            return handler.desc.param1.pid != EVENT_ANY_PID;
        case event_type_t::job_exit:
            return handler.desc.param1.jobspec.pid != EVENT_ANY_PID;
        case event_type_t::caller_exit:
            return true;
        case event_type_t::signal:
        case event_type_t::variable:
        case event_type_t::generic:
        case event_type_t::any:
            return false;
    }
    DIE("Unreachable");
}

/// Tests if one event instance matches the definition of an event class.
/// In case of a match, \p only_once indicates that the event cannot match again by nature.
static bool handler_matches(const event_handler_t &handler, const event_t &instance) {
    if (handler.desc.type == event_type_t::any) return true;
    if (handler.desc.type != instance.desc.type) return false;

    switch (handler.desc.type) {
        case event_type_t::signal: {
            return handler.desc.param1.signal == instance.desc.param1.signal;
        }
        case event_type_t::variable: {
            return instance.desc.str_param1 == handler.desc.str_param1;
        }
        case event_type_t::process_exit: {
            if (handler.desc.param1.pid == EVENT_ANY_PID) return true;
            return handler.desc.param1.pid == instance.desc.param1.pid;
        }
        case event_type_t::job_exit: {
            const auto &jobspec = handler.desc.param1.jobspec;
            if (jobspec.pid == EVENT_ANY_PID) return true;
            return jobspec.internal_job_id == instance.desc.param1.jobspec.internal_job_id;
        }
        case event_type_t::caller_exit: {
            return handler.desc.param1.caller_id == instance.desc.param1.caller_id;
        }
        case event_type_t::generic: {
            return handler.desc.str_param1 == instance.desc.str_param1;
        }
        case event_type_t::any:
        default: {
            DIE("unexpected classv.type");
            return false;
        }
    }
}

/// Test if specified event is blocked.
static bool event_is_blocked(parser_t &parser, const event_t &e) {
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

        case event_type_t::process_exit: {
            return format_string(_(L"exit handler for process %d"), ed.param1.pid);
        }

        case event_type_t::job_exit: {
            const auto &jobspec = ed.param1.jobspec;
            if (const job_t *j = parser.job_get_from_pid(jobspec.pid)) {
                return format_string(_(L"exit handler for job %d, '%ls'"), j->job_id(),
                                     j->command_wcstr());
            } else {
                return format_string(_(L"exit handler for job with pid %d"), jobspec.pid);
            }
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
        inc_signal_observed(eh->desc.param1.signal);
    }

    s_event_handlers.acquire()->push_back(std::move(eh));
}

// \remove handlers for which \p func returns true.
// Simultaneously update our signal_observed array.
template <typename T>
static void remove_handlers_if(const T &func) {
    auto handlers = s_event_handlers.acquire();
    auto iter = handlers->begin();
    while (iter != handlers->end()) {
        event_handler_t *handler = iter->get();
        if (func(*handler)) {
            handler->removed = true;
            if (handler->desc.type == event_type_t::signal) {
                dec_signal_observed(handler->desc.param1.signal);
            }
            iter = handlers->erase(iter);
        } else {
            ++iter;
        }
    }
}

void event_remove_function_handlers(const wcstring &name) {
    remove_handlers_if(
        [&](const event_handler_t &handler) { return handler.function_name == name; });
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
    std::vector<std::shared_ptr<event_handler_t>> fire;
    {
        auto event_handlers = s_event_handlers.acquire();
        for (const auto &handler : *event_handlers) {
            if (handler_matches(*handler, event)) {
                fire.push_back(handler);
            }
        }
    }

    // Iterate over our list of matching events. Fire the ones that are still present.
    bool fired_one_shot = false;
    for (const auto &handler : fire) {
        // A previous handlers may have erased this one.
        if (handler->removed) continue;

        // Construct a buffer to evaluate, starting with the function name and then all the
        // arguments.
        wcstring buffer = handler->function_name;
        for (const wcstring &arg : event.arguments) {
            buffer.push_back(L' ');
            buffer.append(escape_string(arg));
        }

        // Event handlers are not part of the main flow of code, so they are marked as
        // non-interactive.
        scoped_push<bool> interactive{&ld.is_interactive, false};
        auto prev_statuses = parser.get_last_statuses();

        FLOGF(event, L"Firing event '%ls' to handler '%ls'", event.desc.str_param1.c_str(),
              handler->function_name.c_str());
        block_t *b = parser.push_block(block_t::event_block(event));
        parser.eval(buffer, io_chain_t());
        parser.pop_block(b);
        parser.set_last_statuses(std::move(prev_statuses));

        handler->fired = true;
        fired_one_shot |= handler_is_one_shot(*handler);
    }

    // Remove any fired one-shot handlers.
    if (fired_one_shot) {
        remove_handlers_if([](const event_handler_t &handler) {
            return handler.fired && handler_is_one_shot(handler);
        });
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
                // HACK: The only variables we change in response to a *signal*
                // are $COLUMNS and $LINES.
                // Do that now.
                if (sig == SIGWINCH) {
                    (void)termsize_container_t::shared().updating(parser);
                }
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

static const wchar_t *event_name_for_type(event_type_t type) {
    switch (type) {
        case event_type_t::any:
            return L"any";
        case event_type_t::signal:
            return L"signal";
        case event_type_t::variable:
            return L"variable";
        case event_type_t::process_exit:
            return L"process-exit";
        case event_type_t::job_exit:
            return L"job-exit";
        case event_type_t::caller_exit:
            return L"caller-exit";
        case event_type_t::generic:
            return L"generic";
    }
    return L"";
}

const wchar_t *const event_filter_names[] = {L"signal",       L"variable", L"exit",
                                             L"process-exit", L"job-exit", L"caller-exit",
                                             L"generic",      nullptr};

static bool filter_matches_event(const wcstring &filter, event_type_t type) {
    if (filter.empty()) return true;
    switch (type) {
        case event_type_t::any:
            return false;
        case event_type_t::signal:
            return filter == L"signal";
        case event_type_t::variable:
            return filter == L"variable";
        case event_type_t::process_exit:
            return filter == L"process-exit" || filter == L"exit";
        case event_type_t::job_exit:
            return filter == L"job-exit" || filter == L"exit";
        case event_type_t::caller_exit:
            return filter == L"process-exit" || filter == L"exit";
        case event_type_t::generic:
            return filter == L"generic";
    }
    DIE("Unreachable");
}

void event_print(io_streams_t &streams, const wcstring &type_filter) {
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
                          return d1.param1.signal < d2.param1.signal;
                      case event_type_t::process_exit:
                          return d1.param1.pid < d2.param1.pid;
                      case event_type_t::job_exit:
                          return d1.param1.jobspec.pid < d2.param1.jobspec.pid;
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
        if (!filter_matches_event(type_filter, evt->desc.type)) {
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
            case event_type_t::process_exit:
            case event_type_t::job_exit:
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

void event_fire_generic(parser_t &parser, wcstring name, wcstring_list_t args) {
    event_t ev(event_type_t::generic);
    ev.desc.str_param1 = std::move(name);
    ev.arguments = std::move(args);
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

// static
event_t event_t::variable_erase(wcstring name) {
    event_t evt{event_type_t::variable};
    evt.arguments = {L"VARIABLE", L"ERASE", name};
    evt.desc.str_param1 = std::move(name);
    return evt;
}

// static
event_t event_t::variable_set(wcstring name) {
    event_t evt{event_type_t::variable};
    evt.arguments = {L"VARIABLE", L"SET", name};
    evt.desc.str_param1 = std::move(name);
    return evt;
}

// static
event_t event_t::process_exit(pid_t pid, int status) {
    event_t evt{event_type_t::process_exit};
    evt.desc.param1.pid = pid;
    evt.arguments.reserve(3);
    evt.arguments.push_back(L"PROCESS_EXIT");
    evt.arguments.push_back(to_string(pid));
    evt.arguments.push_back(to_string(status));
    return evt;
}

// static
event_t event_t::job_exit(pid_t pgid, internal_job_id_t jid) {
    event_t evt{event_type_t::job_exit};
    evt.desc.param1.jobspec = {pgid, jid};
    evt.arguments.reserve(3);
    evt.arguments.push_back(L"JOB_EXIT");
    evt.arguments.push_back(to_string(pgid));
    evt.arguments.push_back(L"0");  // historical
    return evt;
}

// static
event_t event_t::caller_exit(uint64_t internal_job_id, int job_id) {
    event_t evt{event_type_t::caller_exit};
    evt.desc.param1.caller_id = internal_job_id;
    evt.arguments.reserve(3);
    evt.arguments.push_back(L"JOB_EXIT");
    evt.arguments.push_back(to_string(job_id));
    evt.arguments.push_back(L"0");  // historical
    return evt;
}
