// Functions for handling event triggers.
#include "config.h"  // IWYU pragma: keep

#include <signal.h>
#include <stddef.h>
#include <unistd.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>

#include "common.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "input_common.h"
#include "io.h"
#include "parser.h"
#include "proc.h"
#include "signal.h"
#include "wutil.h"  // IWYU pragma: keep

/// Number of signals that can be queued before an overflow occurs.
#define SIG_UNHANDLED_MAX 64

/// This struct contains a list of generated signals waiting to be dispatched.
typedef struct {
    /// Number of delivered signals.
    volatile int count;
    /// Whether signals have been skipped.
    volatile int overflow;
    /// Array of signal events.
    volatile int signal[SIG_UNHANDLED_MAX];
} signal_list_t;

/// The signal event list. Actually two separate lists. One which is active, which is the one that
/// new events is written to. The inactive one contains the events that are currently beeing
/// performed.
static signal_list_t sig_list[2] = {{}, {}};

/// The index of sig_list that is the list of signals currently written to.
static volatile int active_list = 0;

typedef std::vector<shared_ptr<event_t>> event_list_t;

/// List of event handlers.
static event_list_t s_event_handlers;

/// List of events that have been sent but have not yet been delivered because they are blocked.
static event_list_t blocked;

/// Variables (one per signal) set when a signal is observed. This is inspected by a signal handler.
static volatile bool s_observed_signals[NSIG] = {};
static void set_signal_observed(int sig, bool val) {
    ASSERT_IS_MAIN_THREAD();
    if (sig >= 0 && (size_t)sig < sizeof s_observed_signals / sizeof *s_observed_signals) {
        s_observed_signals[sig] = val;
    }
}

/// Tests if one event instance matches the definition of a event class. If both the class and the
/// instance name a function, they must name the same function.
static int event_match(const event_t &classv, const event_t &instance) {
    // If the function names are both non-empty and different, then it's not a match.
    if (!classv.function_name.empty() && !instance.function_name.empty() &&
        classv.function_name != instance.function_name) {
        return 0;
    }

    if (classv.type == EVENT_ANY) return 1;
    if (classv.type != instance.type) return 0;

    switch (classv.type) {
        case EVENT_SIGNAL: {
            if (classv.param1.signal == EVENT_ANY_SIGNAL) return 1;
            return classv.param1.signal == instance.param1.signal;
        }
        case EVENT_VARIABLE: {
            return instance.str_param1 == classv.str_param1;
        }
        case EVENT_EXIT: {
            if (classv.param1.pid == EVENT_ANY_PID) return 1;
            return classv.param1.pid == instance.param1.pid;
        }
        case EVENT_JOB_ID: {
            return classv.param1.job_id == instance.param1.job_id;
        }
        case EVENT_GENERIC: {
            return instance.str_param1 == classv.str_param1;
        }
        default: {
            DIE("unexpected classv.type");
            break;
        }
    }

    // This should never be reached.
    debug(0, "Warning: Unreachable code reached in event_match in event.cpp\n");
    return 0;
}

/// Test if specified event is blocked.
static int event_is_blocked(const event_t &e) {
    const block_t *block;
    parser_t &parser = parser_t::principal_parser();

    size_t idx = 0;
    while ((block = parser.block_at_index(idx++))) {
        if (event_block_list_blocks_type(block->event_blocks, e.type)) return true;
    }
    return event_block_list_blocks_type(parser.global_event_blocks, e.type);
}

wcstring event_get_desc(const event_t &e) {
    wcstring result;
    switch (e.type) {
        case EVENT_SIGNAL: {
            result = format_string(_(L"signal handler for %ls (%ls)"), sig2wcs(e.param1.signal),
                                   signal_get_desc(e.param1.signal));
            break;
        }
        case EVENT_VARIABLE: {
            result = format_string(_(L"handler for variable '%ls'"), e.str_param1.c_str());
            break;
        }
        case EVENT_EXIT: {
            if (e.param1.pid > 0) {
                result = format_string(_(L"exit handler for process %d"), e.param1.pid);
            } else {
                // In events, PGIDs are stored as negative PIDs
                job_t *j = job_t::from_pid(-e.param1.pid);
                if (j)
                    result = format_string(_(L"exit handler for job %d, '%ls'"), j->job_id,
                                           j->command_wcstr());
                else
                    result = format_string(_(L"exit handler for job with process group %d"),
                                           -e.param1.pid);
            }
            break;
        }
        case EVENT_JOB_ID: {
            job_t *j = job_t::from_job_id(e.param1.job_id);
            if (j) {
                result = format_string(_(L"exit handler for job %d, '%ls'"), j->job_id,
                                       j->command_wcstr());
            } else {
                result = format_string(_(L"exit handler for job with job id %d"), e.param1.job_id);
            }
            break;
        }
        case EVENT_GENERIC: {
            result = format_string(_(L"handler for generic event '%ls'"), e.str_param1.c_str());
            break;
        }
        default: {
            result = format_string(_(L"Unknown event type '0x%x'"), e.type);
            break;
        }
    }

    return result;
}

#if 0
static void show_all_handlers(void) {
    fwprintf(stdout, L"event handlers:\n");
    for (event_list_t::const_iterator iter = events.begin(); iter != events.end(); ++iter) {
        const event_t *foo = *iter;
        wcstring tmp = event_get_desc(foo);
        fwprintf(stdout, L"    handler now %ls\n", tmp.c_str());
    }
}
#endif

/// Give a more condensed description of \c event compared to \c event_get_desc. It includes what
/// function will fire if the \c event is an event handler.
static wcstring event_desc_compact(const event_t &event) {
    wcstring res;
    wchar_t const *temp;
    int sig;
    switch (event.type) {
        case EVENT_ANY: {
            res = L"EVENT_ANY";
            break;
        }
        case EVENT_VARIABLE: {
            if (event.str_param1.c_str()) {
                res = format_string(L"EVENT_VARIABLE($%ls)", event.str_param1.c_str());
            } else {
                res = L"EVENT_VARIABLE([any])";
            }
            break;
        }
        case EVENT_SIGNAL: {
            sig = event.param1.signal;
            if (sig == EVENT_ANY_SIGNAL) {
                temp = L"[all signals]";
            } else if (sig == 0) {
                temp = L"not set";
            } else {
                temp = sig2wcs(sig);
            }
            res = format_string(L"EVENT_SIGNAL(%ls)", temp);
            break;
        }
        case EVENT_EXIT: {
            if (event.param1.pid == EVENT_ANY_PID) {
                res = wcstring(L"EVENT_EXIT([all child processes])");
            } else if (event.param1.pid > 0) {
                res = format_string(L"EVENT_EXIT(pid %d)", event.param1.pid);
            } else {
                // In events, PGIDs are stored as negative PIDs
                job_t *j = job_t::from_pid(-event.param1.pid);
                if (j)
                    res = format_string(L"EVENT_EXIT(jobid %d: \"%ls\")", j->job_id,
                                        j->command_wcstr());
                else
                    res = format_string(L"EVENT_EXIT(pgid %d)", -event.param1.pid);
            }
            break;
        }
        case EVENT_JOB_ID: {
            job_t *j = job_t::from_job_id(event.param1.job_id);
            if (j)
                res =
                    format_string(L"EVENT_JOB_ID(job %d: \"%ls\")", j->job_id, j->command_wcstr());
            else
                res = format_string(L"EVENT_JOB_ID(jobid %d)", event.param1.job_id);
            break;
        }
        case EVENT_GENERIC: {
            res = format_string(L"EVENT_GENERIC(%ls)", event.str_param1.c_str());
            break;
        }
        default: {
            res = format_string(L"unknown/illegal event(%x)", event.type);
            break;
        }
    }
    if (event.function_name.size()) {
        return format_string(L"%ls: \"%ls\"", res.c_str(), event.function_name.c_str());
    }
    return res;
}

void event_add_handler(const event_t &event) {
    if (debug_level >= 3) {
        wcstring desc = event_desc_compact(event);
        debug(3, "register: %ls", desc.c_str());
    }

    shared_ptr<event_t> e = std::make_shared<event_t>(event);
    if (e->type == EVENT_SIGNAL) {
        signal_handle(e->param1.signal, 1);
        set_signal_observed(e->param1.signal, true);
    }

    s_event_handlers.push_back(std::move(e));
}

void event_remove(const event_t &criterion) {
    if (debug_level >= 3) {
        wcstring desc = event_desc_compact(criterion);
        debug(3, "unregister: %ls", desc.c_str());
    }

    event_list_t::iterator iter = s_event_handlers.begin();
    while (iter != s_event_handlers.end()) {
        const event_t *n = iter->get();
        if (!event_match(criterion, *n)) {
            ++iter;
            continue;
        }

        // If this event was a signal handler and no other handler handles the specified signal
        // type, do not handle that type of signal any more.
        if (n->type == EVENT_SIGNAL) {
            event_t e = event_t::signal_event(n->param1.signal);
            if (event_get(e, NULL) == 1) {
                signal_handle(e.param1.signal, 0);
                set_signal_observed(e.param1.signal, 0);
            }
        }
        iter = s_event_handlers.erase(iter);
    }
}

int event_get(const event_t &criterion, event_list_t *out) {
    ASSERT_IS_MAIN_THREAD();
    int found = 0;
    for (const shared_ptr<event_t> &n : s_event_handlers) {
        if (event_match(criterion, *n)) {
            found++;
            if (out) out->push_back(n);
        }
    }
    return found;
}

bool event_is_signal_observed(int sig) {
    // We are in a signal handler! Don't allocate memory, etc.
    bool result = false;
    if (sig >= 0 && (unsigned long)sig < sizeof(s_observed_signals) / sizeof(*s_observed_signals)) {
        result = s_observed_signals[sig];
    }
    return result;
}

/// Perform the specified event. Since almost all event firings will not be matched by even a single
/// event handler, we make sure to optimize the 'no matches' path. This means that nothing is
/// allocated/initialized unless needed.
static void event_fire_internal(const event_t &event) {
    // Iterate over all events, adding events that should be fired to a second list. We need
    // to do this in a separate step since an event handler might call event_remove or
    // event_add_handler, which will change the contents of the \c events list.
    event_list_t fire;
    for (shared_ptr<event_t> &criterion : s_event_handlers) {
        // Check if this event is a match.
        if (event_match(*criterion, event)) {
            fire.push_back(criterion);
        }
    }

    // No matches. Time to return.
    if (fire.empty()) return;

    if (signal_is_blocked()) {
        // Fix for https://github.com/fish-shell/fish-shell/issues/608. Don't run event handlers
        // while signals are blocked.
        input_common_add_callback([event]() {
            ASSERT_IS_MAIN_THREAD();
            event_fire(&event);
        });
        return;
    }

    // Iterate over our list of matching events.
    for (shared_ptr<event_t> &criterion : fire) {
        // Only fire if this event is still present
        if (!contains(s_event_handlers, criterion)) {
            continue;
        }

        // Fire event.
        wcstring buffer = criterion->function_name;

        for (size_t j = 0; j < event.arguments.size(); j++) {
            wcstring arg_esc = escape_string(event.arguments.at(j), 1);
            buffer += L" ";
            buffer += arg_esc;
        }

        // debug( 1, L"Event handler fires command '%ls'", buffer.c_str() );

        // Event handlers are not part of the main flow of code, so they are marked as
        // non-interactive.
        proc_push_interactive(0);
        int prev_status = proc_get_last_status();
        parser_t &parser = parser_t::principal_parser();

        event_block_t *b = parser.push_block<event_block_t>(event);
        parser.eval(buffer, io_chain_t(), TOP);
        parser.pop_block(b);
        proc_pop_interactive();
        proc_set_last_status(prev_status);
    }
}

/// Handle all pending signal events.
static void event_fire_delayed() {
    // If is_event is one, we are running the event-handler non-recursively.
    //
    // When the event handler has called a piece of code that triggers another event, we do not want
    // to fire delayed events because of concurrency problems.
    if (!blocked.empty() && is_event == 1) {
        event_list_t local_blocked;
        local_blocked.swap(blocked);
        for (const shared_ptr<event_t> &e : local_blocked) {
            if (event_is_blocked(*e)) {
                blocked.push_back(e);
            } else {
                event_fire_internal(*e);
            }
        }
    }

    int al = active_list;

    while (sig_list[al].count > 0) {
        signal_list_t *lst;

        // Switch signal lists.
        sig_list[1 - al].count = 0;
        sig_list[1 - al].overflow = 0;
        al = 1 - al;
        active_list = al;

        // Set up.
        lst = &sig_list[1 - al];
        if (lst->overflow) {
            debug(0, _(L"Signal list overflow. Signals have been ignored."));
        }

        // Send all signals in our private list.
        for (int i = 0; i < lst->count; i++) {
            shared_ptr<event_t> e = std::make_shared<event_t>(EVENT_SIGNAL);
            int signal = lst->signal[i];
            e->param1.signal = signal;
            e->arguments.push_back(sig2wcs(signal));
            if (event_is_blocked(*e)) {
                blocked.push_back(e);
            } else {
                event_fire_internal(*e);
            }
        }
    }
}

void event_fire_signal(int signal) {
    // This means we are in a signal handler. We must be very careful not do do anything that could
    // cause a memory allocation or something else that might be bad when in a signal handler.
    if (sig_list[active_list].count < SIG_UNHANDLED_MAX)
        sig_list[active_list].signal[sig_list[active_list].count++] = signal;
    else
        sig_list[active_list].overflow = 1;
}

void event_fire(const event_t *event) {
    if (event && event->type == EVENT_SIGNAL) {
        event_fire_signal(event->param1.signal);
    } else {
        is_event++;

        // Fire events triggered by signals.
        event_fire_delayed();

        if (event) {
            if (event_is_blocked(*event)) {
                blocked.push_back(std::make_shared<event_t>(*event));
            } else {
                event_fire_internal(*event);
            }
        }
        is_event--;
        assert(is_event >= 0);
    }
}

/// Mapping between event type to name.
/// Note we don't bother to sort this.
struct event_type_name_t {
    event_type_t type;
    const wchar_t *name;
};

static const event_type_name_t events_mapping[] = {
    {EVENT_SIGNAL, L"signal"},
    {EVENT_VARIABLE, L"variable"},
    {EVENT_EXIT, L"exit"},
    {EVENT_JOB_ID, L"job-id"},
    {EVENT_GENERIC, L"generic"}
};

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
    std::vector<shared_ptr<event_t>> tmp = s_event_handlers;
    std::sort(tmp.begin(), tmp.end(),
            [](const shared_ptr<event_t> &e1, const shared_ptr<event_t> &e2) {
                if (e1->type == e2->type) {
                    switch (e1->type) {
                        case EVENT_SIGNAL:
                            return e1->param1.signal < e2->param1.signal;
                        case EVENT_JOB_ID:
                            return e1->param1.job_id < e2->param1.job_id;
                        case EVENT_VARIABLE:
                        case EVENT_ANY:
                        case EVENT_GENERIC:
                            return e1->str_param1 < e2->str_param1;
                    }
                }
                return e1->type < e2->type;
            });

    maybe_t<event_type_t> last_type{};
    for (const shared_ptr<event_t> &evt : tmp) {
        // If we have a filter, skip events that don't match.
        if (type_filter && *type_filter != evt->type) {
            continue;
        }

        if (!last_type || *last_type != evt->type) {
            if (last_type)
                streams.out.append(L"\n");
            last_type = static_cast<event_type_t>(evt->type);
            streams.out.append_format(L"Event %ls\n", event_name_for_type(*last_type));
        }
        switch (evt->type) {
            case EVENT_SIGNAL:
                streams.out.append_format(L"%ls %ls\n", sig2wcs(evt->param1.signal),
                        evt->function_name.c_str());
                break;
            case EVENT_JOB_ID:
                streams.out.append_format(L"%d %ls\n", evt->param1,
                        evt->function_name.c_str());
                break;
            case EVENT_VARIABLE:
            case EVENT_GENERIC:
                streams.out.append_format(L"%ls %ls\n", evt->str_param1.c_str(),
                        evt->function_name.c_str());
                break;
            default:
                streams.out.append_format(L"%ls\n", evt->function_name.c_str());
                break;
        }
    }
}

void event_fire_generic(const wchar_t *name, wcstring_list_t *args) {
    CHECK(name, );

    event_t ev(EVENT_GENERIC);
    ev.str_param1 = name;
    if (args) ev.arguments = *args;
    event_fire(&ev);
}

event_t::event_t(int t) : type(t), param1(), str_param1(), function_name(), arguments() {}

event_t::~event_t() = default;

event_t event_t::signal_event(int sig) {
    event_t event(EVENT_SIGNAL);
    event.param1.signal = sig;
    return event;
}

event_t event_t::variable_event(const wcstring &str) {
    event_t event(EVENT_VARIABLE);
    event.str_param1 = str;
    return event;
}

event_t event_t::generic_event(const wcstring &str) {
    event_t event(EVENT_GENERIC);
    event.str_param1 = str;
    return event;
}
