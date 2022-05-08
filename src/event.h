// Functions for handling event triggers
//
// Because most of these functions can be called by signal handler, it is important to make it well
// defined when these functions produce output or perform memory allocations, since such functions
// may not be safely called by signal handlers.
#ifndef FISH_EVENT_H
#define FISH_EVENT_H

#include <unistd.h>

#include <map>
#include <memory>
#include <vector>

#include "common.h"
#include "io.h"

/// The process id that is used to match any process id.
#define EVENT_ANY_PID 0

/// Enumeration of event types.
enum class event_type_t {
    /// Matches any event type (Not always any event, as the function name may limit the choice as
    /// well.
    any,
    /// An event triggered by a signal.
    signal,
    /// An event triggered by a variable update.
    variable,
    /// An event triggered by a process exit.
    process_exit,
    /// An event triggered by a job exit.
    job_exit,
    /// An event triggered by a job exit, triggering the 'caller'-style events only.
    caller_exit,
    /// A generic event.
    generic,
};

/// Null-terminated list of valid event filter names.
/// These are what are valid to pass to 'functions --handlers-type'
extern const wchar_t *const event_filter_names[];

/// Properties of an event.
struct event_description_t {
    /// Helper type for on-job-exit events.
    struct job_spec_t {
        // pid requested by the event, or ANY_PID for all.
        pid_t pid;

        // internal_job_id of the job to match.
        // If this is 0, we match either all jobs (pid == ANY_PID) or no jobs (otherwise).
        uint64_t internal_job_id;
    };

    /// The event type.
    event_type_t type;

    /// The type-specific parameter. The int types are one of the following:
    ///
    /// signal: Signal number for signal-type events.Use EVENT_ANY_SIGNAL to match any signal
    /// pid: Process id for process-type events. Use EVENT_ANY_PID to match any pid.
    /// jobspec: Info for on-job-exit events.
    /// caller_id: Internal job id for caller_exit type events
    union {
        int signal;
        pid_t pid;
        job_spec_t jobspec;
        uint64_t caller_id;
    } param1{};

    /// The string types are one of the following:
    ///
    /// variable: Variable name for variable-type events.
    /// param: The parameter describing this generic event.
    wcstring str_param1{};

    explicit event_description_t(event_type_t t) : type(t) {}
    static event_description_t signal(int sig);
    static event_description_t variable(wcstring str);
    static event_description_t generic(wcstring str);
};

/// Represents a handler for an event.
struct event_handler_t {
    /// Properties of the event to match.
    event_description_t desc;

    /// Name of the function to invoke.
    wcstring function_name{};

    explicit event_handler_t(event_type_t t) : desc(t) {}
    event_handler_t(event_description_t d, wcstring name)
        : desc(std::move(d)), function_name(std::move(name)) {}
};
using event_handler_list_t = std::vector<std::shared_ptr<event_handler_t>>;

/// Represents a event that is fired, or capable of being fired.
struct event_t {
    /// Properties of the event.
    event_description_t desc;

    /// Arguments to any handler.
    wcstring_list_t arguments{};

    explicit event_t(event_type_t t) : desc(t) {}

    /// Create an event_type_t::variable event with the args for erasing a variable.
    static event_t variable_erase(wcstring name);
    /// Create an event_type_t::variable event with the args for setting a variable.
    static event_t variable_set(wcstring name);

    /// Create a PROCESS_EXIT event.
    static event_t process_exit(pid_t pid, int status);

    /// Create a JOB_EXIT event. The pgid should be positive.
    /// The reported status is always 0 for historical reasons.
    static event_t job_exit(pid_t pgid, internal_job_id_t jid);

    /// Create a caller_exit event.
    static event_t caller_exit(uint64_t internal_job_id, int job_id);
};

class parser_t;

/// Add an event handler.
void event_add_handler(std::shared_ptr<event_handler_t> eh);

/// Remove all events for the given function name.
void event_remove_function_handlers(const wcstring &name);

/// Return all event handlers for the given function.
event_handler_list_t event_get_function_handlers(const wcstring &name);

/// Returns whether an event listener is registered for the given signal. This is safe to call from
/// a signal handler.
bool event_is_signal_observed(int signal);

/// Fire the specified event \p event, executing it on \p parser.
void event_fire(parser_t &parser, const event_t &event);

/// Fire all delayed events attached to the given parser.
void event_fire_delayed(parser_t &parser);

/// Enqueue a signal event. Invoked from a signal handler.
void event_enqueue_signal(int signal);

/// Print all events. If type_filter is not empty, only output events with that type.
void event_print(io_streams_t &streams, const wcstring &type_filter);

/// Returns a string describing the specified event.
wcstring event_get_desc(const parser_t &parser, const event_t &e);

/// Fire a generic event with the specified name.
void event_fire_generic(parser_t &parser, wcstring name,
                        const wcstring_list_t *args = nullptr);

#endif
