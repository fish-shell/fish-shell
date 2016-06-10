// Functions for handling event triggers
//
// Because most of these functions can be called by signal handler, it is important to make it well
// defined when these functions produce output or perform memory allocations, since such functions
// may not be safely called by signal handlers.
#ifndef FISH_EVENT_H
#define FISH_EVENT_H

#include <stdbool.h>
#include <unistd.h>
#include <vector>

#include "common.h"

/// The signal number that is used to match any signal.
#define EVENT_ANY_SIGNAL -1

/// The process id that is used to match any process id.
#define EVENT_ANY_PID 0

/// Enumeration of event types.
enum {
    /// Matches any event type (Not always any event, as the function name may limit the choice as
    /// well.
    EVENT_ANY,
    /// An event triggered by a signal.
    EVENT_SIGNAL,
    /// An event triggered by a variable update.
    EVENT_VARIABLE,
    /// An event triggered by a job or process exit.
    EVENT_EXIT,
    /// An event triggered by a job exit.
    EVENT_JOB_ID,
    /// A generic event.
    EVENT_GENERIC,
};

/// The structure which represents an event. The event_t struct has several event-related use-cases:
///
/// - When used as a parameter to event_add, it represents a class of events, and function_name is
/// the name of the function which will be called whenever an event matching the specified class
/// occurs. This is also how events are stored internally.
///
/// - When used as a parameter to event_get, event_remove and event_fire, it represents a class of
/// events, and if the function_name field is non-zero, only events which call the specified
/// function will be returned.
struct event_t {
   public:
    /// Type of event.
    int type;

    /// The type-specific parameter. The int types are one of the following:
    ///
    /// signal: Signal number for signal-type events.Use EVENT_ANY_SIGNAL to match any signal
    /// pid: Process id for process-type events. Use EVENT_ANY_PID to match any pid.
    /// job_id: Job id for EVENT_JOB_ID type events
    union {
        int signal;
        int job_id;
        pid_t pid;
    } param1;

    /// The string types are one of the following:
    ///
    /// variable: Variable name for variable-type events.
    /// param: The parameter describing this generic event.
    wcstring str_param1;

    /// The name of the event handler function.
    wcstring function_name;

    /// The argument list. Only used when sending a new event using event_fire. In all other
    /// situations, the value of this variable is ignored.
    wcstring_list_t arguments;

    explicit event_t(int t);
    ~event_t();

    static event_t signal_event(int sig);
    static event_t variable_event(const wcstring &str);
    static event_t generic_event(const wcstring &str);
};

/// Add an event handler.
///
/// May not be called by a signal handler, since it may allocate new memory.
void event_add_handler(const event_t &event);

/// Remove all events matching the specified criterion.
///
/// May not be called by a signal handler, since it may free allocated memory.
void event_remove(const event_t &event);

/// Return all events which match the specified event class
///
/// This function is safe to call from a signal handler _ONLY_ if the out parameter is null.
///
/// \param criterion Is the class of events to return. If the criterion has a non-null
/// function_name, only events which trigger the specified function will return.
/// \param out the list to add events to. May be 0, in which case no events will be added, but the
/// result count will still be valid
///
/// \return the number of found matches
int event_get(const event_t &criterion, std::vector<event_t *> *out);

/// Returns whether an event listener is registered for the given signal. This is safe to call from
/// a signal handler.
bool event_is_signal_observed(int signal);

/// Fire the specified event. The function_name field of the event must be set to 0. If the event is
/// of type EVENT_SIGNAL, no the event is queued, and will be dispatched the next time event_fire is
/// called. If event is a null-pointer, all pending events are dispatched.
///
/// This function is safe to call from a signal handler _ONLY_ if the event parameter is for a
/// signal. Signal events not be fired, by the call to event_fire, instead they will be fired the
/// next time event_fire is called with anull argument. This is needed to make sure that no code
/// evaluation is ever performed by a signal handler.
///
/// \param event the specific event whose handlers should fire. If null, then all delayed events
/// will be fired.
void event_fire(const event_t *event);

/// Like event_fire, but takes a signal directly.
void event_fire_signal(int signal);

/// Initialize the event-handling library.
void event_init();

/// Destroy the event-handling library.
void event_destroy();

/// Free all memory used by the specified event.
void event_free(event_t *e);

/// Returns a string describing the specified event.
wcstring event_get_desc(const event_t &e);

/// Fire a generic event with the specified name.
void event_fire_generic(const wchar_t *name, wcstring_list_t *args = NULL);

#endif
