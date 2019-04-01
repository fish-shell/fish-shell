// Support for dispatching on environment changes.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <pthread.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <cstring>
#include <cwchar>

#if HAVE_CURSES_H
#include <curses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#elif HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#endif
#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif

#include <algorithm>
#include <iterator>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "builtin_bind.h"
#include "common.h"
#include "complete.h"
#include "env.h"
#include "env_universal_common.h"
#include "event.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fish_version.h"
#include "function.h"
#include "history.h"
#include "input.h"
#include "input_common.h"
#include "output.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
#include "sanity.h"
#include "screen.h"
#include "wutil.h"  // IWYU pragma: keep

#define DEFAULT_TERM1 "ansi"
#define DEFAULT_TERM2 "dumb"

/// Some configuration path environment variables.
#define FISH_DATADIR_VAR L"__fish_data_dir"
#define FISH_SYSCONFDIR_VAR L"__fish_sysconf_dir"
#define FISH_HELPDIR_VAR L"__fish_help_dir"
#define FISH_BIN_DIR L"__fish_bin_dir"
#define FISH_CONFIG_DIR L"__fish_config_dir"
#define FISH_USER_DATA_DIR L"__fish_user_data_dir"

/// List of all locale environment variable names that might trigger (re)initializing the locale
/// subsystem.
extern const wcstring_list_t locale_variables({L"LANG", L"LANGUAGE", L"LC_ALL", L"LC_ADDRESS",
                                               L"LC_COLLATE", L"LC_CTYPE", L"LC_IDENTIFICATION",
                                               L"LC_MEASUREMENT", L"LC_MESSAGES", L"LC_MONETARY",
                                               L"LC_NAME", L"LC_NUMERIC", L"LC_PAPER",
                                               L"LC_TELEPHONE", L"LC_TIME"});

/// List of all curses environment variable names that might trigger (re)initializing the curses
/// subsystem.
extern const wcstring_list_t curses_variables({L"TERM", L"TERMINFO", L"TERMINFO_DIRS"});

class var_dispatch_table_t {
    using named_callback_t = std::function<void(const wcstring &, const wcstring &, env_stack_t &)>;
    std::unordered_map<wcstring, named_callback_t> named_table_;

    using anon_callback_t = std::function<void(env_stack_t &)>;
    std::unordered_map<wcstring, anon_callback_t> anon_table_;

    bool observes_var(const wcstring &name) {
        return named_table_.count(name) || anon_table_.count(name);
    }

   public:
    /// Add a callback for the given variable, which expects the name.
    /// We must not already be observing this variable.
    void add(wcstring name, named_callback_t cb) {
        assert(!observes_var(name) && "Already observing that variable");
        named_table_.emplace(std::move(name), std::move(cb));
    }

    /// Add a callback for the given variable, which ignores the name.
    /// We must not already be observing this variable.
    void add(wcstring name, anon_callback_t cb) {
        assert(!observes_var(name) && "Already observing that variable");
        anon_table_.emplace(std::move(name), std::move(cb));
    }

    void dispatch(const wchar_t *op, const wcstring &key, env_stack_t &vars) const {
        auto named = named_table_.find(key);
        if (named != named_table_.end()) {
            named->second(op, key, vars);
        }
        auto anon = anon_table_.find(key);
        if (anon != anon_table_.end()) {
            anon->second(vars);
        }
    }
};

// Run those dispatch functions which want to be run at startup.
static void run_inits(const environment_t &vars);

// return a new-ly allocated dispatch table, running those dispatch functions which should be
// initialized.
static std::unique_ptr<const var_dispatch_table_t> create_dispatch_table();

// A pointer to the variable dispatch table. This is allocated with new() and deliberately leaked to
// avoid shutdown destructors. This is set during startup and should not be modified after.
static const var_dispatch_table_t *s_var_dispatch_table;

void env_dispatch_init(const environment_t &vars) {
    run_inits(vars);
    // Note this deliberately leaks; the dispatch table is immortal.
    // Via this construct we can avoid invoking destructors at shutdown.
    s_var_dispatch_table = create_dispatch_table().release();
}

/// Properly sets all timezone information.
static void handle_timezone(const wchar_t *env_var_name, const environment_t &vars) {
    const auto var = vars.get(env_var_name, ENV_DEFAULT);
    debug(2, L"handle_timezone() current timezone var: |%ls| => |%ls|", env_var_name,
          !var ? L"MISSING" : var->as_string().c_str());
    const std::string &name = wcs2string(env_var_name);
    if (var.missing_or_empty()) {
        unsetenv(name.c_str());
    } else {
        const std::string value = wcs2string(var->as_string());
        setenv(name.c_str(), value.c_str(), 1);
    }
    tzset();
}

/// Update the value of g_guessed_fish_emoji_width
static void guess_emoji_width(const environment_t &vars) {
    if (auto width_str = vars.get(L"fish_emoji_width")) {
        int new_width = fish_wcstol(width_str->as_string().c_str());
        g_fish_emoji_width = std::max(0, new_width);
        debug(2, "'fish_emoji_width' preference: %d, overwriting default", g_fish_emoji_width);
        return;
    }

    wcstring term;
    if (auto term_var = vars.get(L"TERM_PROGRAM")) {
        term = term_var->as_string();
    }

    double version = 0;
    if (auto version_var = vars.get(L"TERM_PROGRAM_VERSION")) {
        std::string narrow_version = wcs2string(version_var->as_string());
        version = strtod(narrow_version.c_str(), NULL);
    }

    if (term == L"Apple_Terminal" && version >= 400) {
        // Apple Terminal on High Sierra
        g_guessed_fish_emoji_width = 2;
        debug(2, "default emoji width: 2 for %ls", term.c_str());
    } else if (term == L"iTerm.app") {
        // iTerm2 defaults to Unicode 8 sizes.
        // See https://gitlab.com/gnachman/iterm2/wikis/unicodeversionswitching
        g_guessed_fish_emoji_width = 1;
        debug(2, "default emoji width: 1");
    } else {
        // Default to whatever system wcwidth says to U+1F603,
        // but only if it's at least 1.
        int w = wcwidth(L'😃');
        g_guessed_fish_emoji_width = w > 0 ? w : 1;
        debug(2, "default emoji width: %d", g_guessed_fish_emoji_width);
    }
}

/// React to modifying the given variable.
void env_dispatch_var_change(const wchar_t *op, const wcstring &key, env_stack_t &vars) {
    ASSERT_IS_MAIN_THREAD();
    // Do nothing if not yet fully initialized.
    if (!s_var_dispatch_table) return;

    s_var_dispatch_table->dispatch(op, key, vars);

    // Eww.
    if (string_prefixes_string(L"fish_color_", key)) {
        reader_react_to_color_change();
    }
}

/// Universal variable callback function. This function makes sure the proper events are triggered
/// when an event occurs.
static void universal_callback(env_stack_t *stack, const callback_data_t &cb) {
    const wchar_t *op = cb.is_erase() ? L"ERASE" : L"SET";

    env_dispatch_var_change(op, cb.key, *stack);
    stack->mark_changed_exported();
    event_fire(event_t::variable(cb.key, {L"VARIABLE", op, cb.key}));
}

void env_universal_callbacks(env_stack_t *stack, const callback_data_list_t &callbacks) {
    for (const callback_data_t &cb : callbacks) {
        universal_callback(stack, cb);
    }
}

static void handle_fish_term_change(const wcstring &op, const wcstring &var_name,
                                    env_stack_t &vars) {
    UNUSED(op);
    UNUSED(var_name);
    update_fish_color_support(vars);
    reader_react_to_color_change();
}

static void handle_change_ambiguous_width(const wcstring &op, const wcstring &var_name,
                                          env_stack_t &vars) {
    (void)op;
    (void)var_name;
    int new_width = 1;
    if (auto width_str = vars.get(L"fish_ambiguous_width")) {
        new_width = fish_wcstol(width_str->as_string().c_str());
    }
    g_fish_ambiguous_width = std::max(0, new_width);
}

static void handle_term_size_change(const wcstring &op, const wcstring &var_name,
                                    env_stack_t &vars) {
    UNUSED(op);
    UNUSED(var_name);
    UNUSED(vars);
    invalidate_termsize(true);  // force fish to update its idea of the terminal size plus vars
}

static void handle_read_limit_change(const wcstring &op, const wcstring &var_name,
                                     env_stack_t &vars) {
    UNUSED(op);
    UNUSED(var_name);
    vars.set_read_limit();
}

static void handle_fish_history_change(const wcstring &op, const wcstring &var_name,
                                       env_stack_t &vars) {
    UNUSED(op);
    UNUSED(var_name);
    reader_change_history(history_session_id(vars));
}

static void handle_function_path_change(const wcstring &op, const wcstring &var_name,
                                        env_stack_t &vars) {
    UNUSED(op);
    UNUSED(var_name);
    UNUSED(vars);
    function_invalidate_path();
}

static void handle_complete_path_change(const wcstring &op, const wcstring &var_name,
                                        env_stack_t &vars) {
    UNUSED(op);
    UNUSED(var_name);
    UNUSED(vars);
    complete_invalidate_path();
}

static void handle_tz_change(const wcstring &op, const wcstring &var_name, env_stack_t &vars) {
    UNUSED(op);
    handle_timezone(var_name.c_str(), vars);
}

static void handle_magic_colon_var_change(const wcstring &op, const wcstring &var_name,
                                          env_stack_t &vars) {
    UNUSED(op);
    fix_colon_delimited_var(var_name, vars);
}

static void handle_locale_change(const environment_t &vars) {
    init_locale(vars);
    // We need to re-guess emoji width because the locale might have changed to a multibyte one.
    guess_emoji_width(vars);
}

static void handle_curses_change(const environment_t &vars) {
    guess_emoji_width(vars);
    init_curses(vars);
}

/// Populate the dispatch table used by `env_dispatch_var_change()` to efficiently call the
/// appropriate function to handle a change to a variable.
/// Note this returns a new-allocated value that we expect to leak.
static std::unique_ptr<const var_dispatch_table_t> create_dispatch_table() {
    auto var_dispatch_table = make_unique<var_dispatch_table_t>();
    for (const auto &var_name : locale_variables) {
        var_dispatch_table->add(var_name, handle_locale_change);
    }

    for (const auto &var_name : curses_variables) {
        var_dispatch_table->add(var_name, handle_curses_change);
    }

    var_dispatch_table->add(L"PATH", handle_magic_colon_var_change);
    var_dispatch_table->add(L"CDPATH", handle_magic_colon_var_change);
    var_dispatch_table->add(L"fish_term256", handle_fish_term_change);
    var_dispatch_table->add(L"fish_term24bit", handle_fish_term_change);
    var_dispatch_table->add(L"fish_escape_delay_ms", update_wait_on_escape_ms);
    var_dispatch_table->add(L"fish_emoji_width", guess_emoji_width);
    var_dispatch_table->add(L"fish_ambiguous_width", handle_change_ambiguous_width);
    var_dispatch_table->add(L"LINES", handle_term_size_change);
    var_dispatch_table->add(L"COLUMNS", handle_term_size_change);
    var_dispatch_table->add(L"fish_complete_path", handle_complete_path_change);
    var_dispatch_table->add(L"fish_function_path", handle_function_path_change);
    var_dispatch_table->add(L"fish_read_limit", handle_read_limit_change);
    var_dispatch_table->add(L"fish_history", handle_fish_history_change);
    var_dispatch_table->add(L"TZ", handle_tz_change);
    return var_dispatch_table;
}

static void run_inits(const environment_t &vars) {
    // This is the subset of those dispatch functions which want to be run at startup.
    handle_locale_change(vars);
    handle_curses_change(vars);
    update_wait_on_escape_ms(vars);
}
