/// The flogger: debug logging support for fish.
#ifndef FISH_FLOG_H
#define FISH_FLOG_H

#include "config.h"  // IWYU pragma: keep

#include <stdio.h>

#include <string>
#include <type_traits>
#include <utility>

#include "global_safety.h"
#include "wcstringutil.h"

using wcstring = std::wstring;
using wcstring_list_t = std::vector<wcstring>;

template <typename T>
class owning_lock;

namespace flog_details {

class category_list_t;
class category_t {
    friend category_list_t;
    category_t(const wchar_t *name, const wchar_t *desc, bool enabled = false);

   public:
    /// The name of this category.
    const wchar_t *const name;

    /// A (non-localized) description of the category.
    const wchar_t *const description;

    /// Whether the category is enabled.
    relaxed_atomic_bool_t enabled;
};

class category_list_t {
    category_list_t() = default;

   public:
    /// The singleton global category list instance.
    static category_list_t *const g_instance;

    /// What follows are the actual logging categories.
    /// To add a new category simply define a new field, following the pattern.

    category_t error{L"error", L"Serious unexpected errors (on by default)", true};

    category_t debug{L"debug", L"Debugging aid (on by default)", true};

    category_t warning{L"warning", L"Warnings (on by default)", true};

    category_t config{L"config", L"Finding and reading configuration"};

    category_t exec_job_status{L"exec-job-status", L"Jobs changing status"};

    category_t exec_job_exec{L"exec-job-exec", L"Jobs being executed"};

    category_t exec_fork{L"exec-fork", L"Calls to fork()"};

    category_t parse_productions{L"parse-productions", L"Resolving tokens"};
    category_t parse_productions_chatty{L"parse-productions-chatty", L"Resolving tokens (chatty messages)"};

    category_t proc_job_run{L"proc-job-run", L"Jobs getting started or continued"};

    category_t proc_termowner{L"proc-termowner", L"Terminal ownership events"};

    category_t proc_internal_proc{L"proc-internal-proc", L"Internal (non-forked) process events"};

    category_t proc_reap_internal{L"proc-reap-internal",
                                  L"Reaping internal (non-forked) processes"};

    category_t proc_reap_external{L"proc-reap-external", L"Reaping external (forked) processes"};
    category_t proc_pgroup{L"proc-pgroup", L"Process groups"};

    category_t env_locale{L"env-locale", L"Changes to locale variables"};

    category_t env_export{L"env-export", L"Changes to exported variables"};

    category_t env_dispatch{L"env-dispatch", L"Reacting to variables"};

    category_t uvar_file{L"uvar-file", L"Writing/reading the universal variable store"};

    category_t topic_monitor{L"topic-monitor", L"Internal details of the topic monitor"};
    category_t char_encoding{L"char-encoding", L"Character encoding issues"};

    category_t history{L"history", L"Command history events"};
    category_t history_file{L"history-file", L"Reading/Writing the history file"};

    category_t profile_history{L"profile-history", L"History performance measurements"};

    category_t iothread{L"iothread", L"Background IO thread events"};

    category_t term_support{L"term-support", L"Terminal feature detection"};

    category_t reader{L"reader", L"The interactive reader/input system"};
    category_t complete{L"complete", L"The completion system"};
    category_t path{L"path", L"Searching/using paths"};
};

/// The class responsible for logging.
/// This is protected by a lock.
class logger_t {
    FILE *file_;

    void log1(const wchar_t *);
    void log1(const char *);
    void log1(wchar_t);
    void log1(char);

    void log1(const wcstring &s) { log1(s.c_str()); }
    void log1(const std::string &s) { log1(s.c_str()); }

    template <typename T,
              typename Enabler = typename std::enable_if<std::is_integral<T>::value>::type>
    void log1(const T &v) {
        log1(to_string(v));
    }

    template <typename T>
    void log_args_impl(const T &arg) {
        log1(arg);
    }

    template <typename T, typename... Ts>
    void log_args_impl(const T &arg, const Ts &... rest) {
        log1(arg);
        log1(' ');
        log_args_impl<Ts...>(rest...);
    }

   public:
    void set_file(FILE *f) { file_ = f; }

    logger_t();

    template <typename... Args>
    void log_args(const category_t &cat, const Args &... args) {
        log1(cat.name);
        log1(": ");
        log_args_impl(args...);
        log1('\n');
    }

    void log_fmt(const category_t &cat, const wchar_t *fmt, ...);
    void log_fmt(const category_t &cat, const char *fmt, ...);

    // Log outside of the usual flog usage.
    void log_extra(const wchar_t *s) { log1(s); }
};

extern owning_lock<logger_t> g_logger;

}  // namespace flog_details

/// Set the active flog categories according to the given wildcard \p wc.
void activate_flog_categories_by_pattern(const wcstring &wc);

/// Set the file that flog should output to.
/// flog does not close this file.
void set_flog_output_file(FILE *f);

/// \return a list of all categories, sorted by name.
std::vector<const flog_details::category_t *> get_flog_categories();

/// Print some extra stuff to the flog file (stderr by default).
/// This is used by the tracing machinery.
void log_extra_to_flog_file(const wcstring &s);

/// Output to the fish log a sequence of arguments, separated by spaces, and ending with a newline.
#define FLOG(wht, ...)                                                        \
    do {                                                                      \
        if (flog_details::category_list_t::g_instance->wht.enabled) {         \
            flog_details::g_logger.acquire()->log_args(                       \
                flog_details::category_list_t::g_instance->wht, __VA_ARGS__); \
        }                                                                     \
    } while (0)

/// Output to the fish log a printf-style formatted string.
#define FLOGF(wht, ...)                                                       \
    do {                                                                      \
        if (flog_details::category_list_t::g_instance->wht.enabled) {         \
            flog_details::g_logger.acquire()->log_fmt(                        \
                flog_details::category_list_t::g_instance->wht, __VA_ARGS__); \
        }                                                                     \
    } while (0)

#endif

#define should_flog(wht) \
    (flog_details::category_list_t::g_instance->wht.enabled)
