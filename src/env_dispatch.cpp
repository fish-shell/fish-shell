// Support for dispatching on environment changes.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stddef.h>
#include <stdlib.h>
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

#include <assert.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "common.h"
#include "complete.h"
#include "env.h"
#include "env_dispatch.h"
#include "env_universal_common.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "flog.h"
#include "function.h"
#include "global_safety.h"
#include "history.h"
#include "input_common.h"
#include "maybe.h"
#include "output.h"
#include "parser.h"
#include "proc.h"
#include "reader.h"
#include "screen.h"
#include "termsize.h"
#include "wutil.h"  // IWYU pragma: keep

#define DEFAULT_TERM1 "ansi"
#define DEFAULT_TERM2 "dumb"

/// List of all locale environment variable names that might trigger (re)initializing the locale
/// subsystem.
static const wcstring_list_t locale_variables({L"LANG", L"LANGUAGE", L"LC_ALL", L"LC_ADDRESS",
                                               L"LC_COLLATE", L"LC_CTYPE", L"LC_IDENTIFICATION",
                                               L"LC_MEASUREMENT", L"LC_MESSAGES", L"LC_MONETARY",
                                               L"LC_NAME", L"LC_NUMERIC", L"LC_PAPER",
                                               L"LC_TELEPHONE", L"LC_TIME", L"LOCPATH"});

/// List of all curses environment variable names that might trigger (re)initializing the curses
/// subsystem.
static const wcstring_list_t curses_variables({L"TERM", L"TERMINFO", L"TERMINFO_DIRS"});

class var_dispatch_table_t {
    using named_callback_t = std::function<void(const wcstring &, env_stack_t &)>;
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

    void dispatch(const wcstring &key, env_stack_t &vars) const {
        auto named = named_table_.find(key);
        if (named != named_table_.end()) {
            named->second(key, vars);
        }
        auto anon = anon_table_.find(key);
        if (anon != anon_table_.end()) {
            anon->second(vars);
        }
    }
};

// Forward declarations.
static void init_curses(const environment_t &vars);
static void init_locale(const environment_t &vars);
static void update_fish_color_support(const environment_t &vars);

/// True if we think we can set the terminal title.
static relaxed_atomic_bool_t can_set_term_title{false};

// Run those dispatch functions which want to be run at startup.
static void run_inits(const environment_t &vars);

// return a new-ly allocated dispatch table, running those dispatch functions which should be
// initialized.
static std::unique_ptr<const var_dispatch_table_t> create_dispatch_table();

// A pointer to the variable dispatch table. This is allocated with new() and deliberately leaked to
// avoid shutdown destructors. This is set during startup and should not be modified after.
static latch_t<const var_dispatch_table_t> s_var_dispatch_table;

void env_dispatch_init(const environment_t &vars) {
    run_inits(vars);
    // Note this deliberately leaks; the dispatch table is immortal.
    // Via this construct we can avoid invoking destructors at shutdown.
    s_var_dispatch_table = create_dispatch_table();
}

/// Properly sets all timezone information.
static void handle_timezone(const wchar_t *env_var_name, const environment_t &vars) {
    const auto var = vars.get(env_var_name, ENV_DEFAULT);
    FLOGF(env_dispatch, L"handle_timezone() current timezone var: |%ls| => |%ls|", env_var_name,
          !var ? L"MISSING" : var->as_string().c_str());
    const std::string &name = wcs2string(env_var_name);
    if (var.missing_or_empty()) {
        unsetenv_lock(name.c_str());
    } else {
        const std::string value = wcs2string(var->as_string());
        setenv_lock(name.c_str(), value.c_str(), 1);
    }
    tzset();
}

/// Update the value of g_guessed_fish_emoji_width
static void guess_emoji_width(const environment_t &vars) {
    if (auto width_str = vars.get(L"fish_emoji_width")) {
        int new_width = fish_wcstol(width_str->as_string().c_str());
        g_fish_emoji_width = std::max(0, new_width);
        FLOGF(term_support, "'fish_emoji_width' preference: %d, overwriting default",
              g_fish_emoji_width);
        return;
    }

    wcstring term;
    if (auto term_var = vars.get(L"TERM_PROGRAM")) {
        term = term_var->as_string();
    }

    double version = 0;
    if (auto version_var = vars.get(L"TERM_PROGRAM_VERSION")) {
        std::string narrow_version = wcs2string(version_var->as_string());
        version = strtod(narrow_version.c_str(), nullptr);
    }

    if (term == L"Apple_Terminal" && version >= 400) {
        // Apple Terminal on High Sierra
        g_guessed_fish_emoji_width = 2;
        FLOGF(term_support, "default emoji width: 2 for %ls", term.c_str());
    } else if (term == L"iTerm.app") {
        // iTerm2 defaults to Unicode 8 sizes.
        // See https://gitlab.com/gnachman/iterm2/wikis/unicodeversionswitching
        g_guessed_fish_emoji_width = 1;
        FLOGF(term_support, "default emoji width: 1");
    } else {
        // Default to whatever system wcwidth says to U+1F603,
        // but only if it's at least 1.
        int w = wcwidth(L'😃');
        g_guessed_fish_emoji_width = w > 0 ? w : 1;
        FLOGF(term_support, "default emoji width: %d", g_guessed_fish_emoji_width);
    }
}

/// React to modifying the given variable.
void env_dispatch_var_change(const wcstring &key, env_stack_t &vars) {
    ASSERT_IS_MAIN_THREAD();
    // Do nothing if not yet fully initialized.
    if (!s_var_dispatch_table) return;

    s_var_dispatch_table->dispatch(key, vars);

    // Eww.
    if (string_prefixes_string(L"fish_color_", key)) {
        reader_schedule_prompt_repaint();
    }
}

/// Universal variable callback function. This function makes sure the proper events are triggered
/// when an event occurs.
static void universal_callback(env_stack_t *stack, const callback_data_t &cb) {
    const wchar_t *op = cb.is_erase() ? L"ERASE" : L"SET";

    env_dispatch_var_change(cb.key, *stack);

    // TODO: eliminate this principal_parser. Need to rationalize how multiple threads work here.
    event_fire(parser_t::principal_parser(), event_t::variable(cb.key, {L"VARIABLE", op, cb.key}));
}

void env_universal_callbacks(env_stack_t *stack, const callback_data_list_t &callbacks) {
    for (const callback_data_t &cb : callbacks) {
        universal_callback(stack, cb);
    }
}

static void handle_fish_term_change(const env_stack_t &vars) {
    update_fish_color_support(vars);
    reader_schedule_prompt_repaint();
}

static void handle_change_ambiguous_width(const env_stack_t &vars) {
    int new_width = 1;
    if (auto width_str = vars.get(L"fish_ambiguous_width")) {
        new_width = fish_wcstol(width_str->as_string().c_str());
    }
    g_fish_ambiguous_width = std::max(0, new_width);
}

static void handle_term_size_change(const env_stack_t &vars) {
    termsize_container_t::shared().handle_columns_lines_var_change(vars);
}

static void handle_fish_history_change(const env_stack_t &vars) {
    reader_change_history(history_session_id(vars));
}

static void handle_function_path_change(const env_stack_t &vars) {
    UNUSED(vars);
    function_invalidate_path();
}

static void handle_complete_path_change(const env_stack_t &vars) {
    UNUSED(vars);
    complete_invalidate_path();
}

static void handle_tz_change(const wcstring &var_name, const env_stack_t &vars) {
    handle_timezone(var_name.c_str(), vars);
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

static void handle_fish_use_posix_spawn_change(const environment_t &vars) {
    // note this defaults to true
    auto use_posix_spawn = vars.get(L"fish_use_posix_spawn");
    g_use_posix_spawn =
        use_posix_spawn.missing_or_empty() ? true : bool_from_string(use_posix_spawn->as_string());
}

/// Allow the user to override the limit on how much data the `read` command will process.
/// This is primarily for testing but could be used by users in special situations.
static void handle_read_limit_change(const environment_t &vars) {
    auto read_byte_limit_var = vars.get(L"fish_read_limit");
    if (!read_byte_limit_var.missing_or_empty()) {
        size_t limit = fish_wcstoull(read_byte_limit_var->as_string().c_str());
        if (errno) {
            FLOGF(warning, "Ignoring fish_read_limit since it is not valid");
        } else {
            read_byte_limit = limit;
        }
    }
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
    var_dispatch_table->add(L"fish_use_posix_spawn", handle_fish_use_posix_spawn_change);

    // This std::move is required to avoid a build error on old versions of libc++ (#5801)
    return std::move(var_dispatch_table);
}

static void run_inits(const environment_t &vars) {
    // This is the subset of those dispatch functions which want to be run at startup.
    init_locale(vars);
    init_curses(vars);
    guess_emoji_width(vars);
    update_wait_on_escape_ms(vars);
    handle_read_limit_change(vars);
    handle_fish_use_posix_spawn_change(vars);
}

/// Updates our idea of whether we support term256 and term24bit (see issue #10222).
static void update_fish_color_support(const environment_t &vars) {
    // Detect or infer term256 support. If fish_term256 is set, we respect it;
    // otherwise infer it from the TERM variable or use terminfo.
    wcstring term;
    bool support_term256 = false;
    bool support_term24bit = false;

    if (auto term_var = vars.get(L"TERM")) term = term_var->as_string();

    if (auto fish_term256 = vars.get(L"fish_term256")) {
        // $fish_term256
        support_term256 = bool_from_string(fish_term256->as_string());
        FLOGF(term_support, L"256 color support determined by '$fish_term256'");
    } else if (term.find(L"256color") != wcstring::npos) {
        // TERM is *256color*: 256 colors explicitly supported
        support_term256 = true;
        FLOGF(term_support, L"256 color support enabled for TERM=%ls", term.c_str());
    } else if (term.find(L"xterm") != wcstring::npos) {
        // Assume that all 'xterm's can handle 256, except for Terminal.app from Snow Leopard
        wcstring term_program;
        if (auto tp = vars.get(L"TERM_PROGRAM")) term_program = tp->as_string();
        if (term_program == L"Apple_Terminal") {
            auto tpv = vars.get(L"TERM_PROGRAM_VERSION");
            if (tpv && fish_wcstod(tpv->as_string().c_str(), nullptr) > 299) {
                // OS X Lion is version 299+, it has 256 color support (see github Wiki)
                support_term256 = true;
                FLOGF(term_support, L"256 color support enabled for TERM=%ls on Terminal.app",
                      term.c_str());
            }
        } else {
            support_term256 = true;
            FLOGF(term_support, L"256 color support enabled for TERM=%ls", term.c_str());
        }
    } else if (cur_term != nullptr) {
        // See if terminfo happens to identify 256 colors
        support_term256 = (max_colors >= 256);
        FLOGF(term_support, L"256 color support: %d colors per terminfo entry for %ls", max_colors,
              term.c_str());
    }

    // Handle $fish_term24bit
    if (auto fish_term24bit = vars.get(L"fish_term24bit")) {
        support_term24bit = bool_from_string(fish_term24bit->as_string());
        FLOGF(term_support, L"'fish_term24bit' preference: 24-bit color %ls",
              support_term24bit ? L"enabled" : L"disabled");
    } else {
        if (vars.get(L"STY") || string_prefixes_string(L"eterm", term)) {
            // Screen and emacs' ansi-term swallow truecolor sequences,
            // so we ignore them unless force-enabled.
            FLOGF(term_support, L"Truecolor support: disabling for eterm/screen");
            support_term24bit = false;
        } else if (cur_term != nullptr && max_colors >= 32767) {
            // $TERM wins, xterm-direct reports 32767 colors, we assume that's the minimum
            // as xterm is weird when it comes to color.
            FLOGF(term_support, L"Truecolor support: Enabling per terminfo for %ls with %d colors",
                  term.c_str(), max_colors);
            support_term24bit = true;
        } else {
            if (auto ct = vars.get(L"COLORTERM")) {
                // If someone set $COLORTERM, that's the sort of color they want.
                if (ct->as_string() == L"truecolor" || ct->as_string() == L"24bit") {
                    FLOGF(term_support, L"Truecolor support: Enabling per $COLORTERM='%ls'",
                          ct->as_string().c_str());
                    support_term24bit = true;
                }
            } else if (vars.get(L"KONSOLE_VERSION") || vars.get(L"KONSOLE_PROFILE_NAME")) {
                // All konsole versions that use $KONSOLE_VERSION are new enough to support this,
                // so no check is necessary.
                FLOGF(term_support, L"Truecolor support: Enabling for Konsole");
                support_term24bit = true;
            } else if (auto it = vars.get(L"ITERM_SESSION_ID")) {
                // Supporting versions of iTerm include a colon here.
                // We assume that if this is iTerm, it can't also be st, so having this check
                // inside is okay.
                if (it->as_string().find(L':') != wcstring::npos) {
                    FLOGF(term_support, L"Truecolor support: Enabling for ITERM");
                    support_term24bit = true;
                }
            } else if (string_prefixes_string(L"st-", term)) {
                FLOGF(term_support, L"Truecolor support: Enabling for st");
                support_term24bit = true;
            } else if (auto vte = vars.get(L"VTE_VERSION")) {
                if (fish_wcstod(vte->as_string().c_str(), nullptr) > 3600) {
                    FLOGF(term_support, L"Truecolor support: Enabling for VTE version %ls",
                          vte->as_string().c_str());
                    support_term24bit = true;
                }
            }
        }
    }
    color_support_t support = (support_term256 ? color_support_term256 : 0) |
                              (support_term24bit ? color_support_term24bit : 0);
    output_set_color_support(support);
}

// Try to initialize the terminfo/curses subsystem using our fallback terminal name. Do not set
// `TERM` to our fallback. We're only doing this in the hope of getting a minimally functional
// shell. If we launch an external command that uses TERM it should get the same value we were
// given, if any.
static bool initialize_curses_using_fallback(const char *term) {
    // If $TERM is already set to the fallback name we're about to use there isn't any point in
    // seeing if the fallback name can be used.
    auto &vars = env_stack_t::globals();
    auto term_var = vars.get(L"TERM");
    if (term_var.missing_or_empty()) return false;

    auto term_env = wcs2string(term_var->as_string());
    if (term_env == DEFAULT_TERM1 || term_env == DEFAULT_TERM2) return false;

    if (session_interactivity() != session_interactivity_t::not_interactive)
        FLOGF(warning, _(L"Using fallback terminal type '%s'."), term);

    int err_ret;
    if (setupterm(const_cast<char *>(term), STDOUT_FILENO, &err_ret) == OK) return true;
    if (session_interactivity() != session_interactivity_t::not_interactive) {
        FLOGF(warning, _(L"Could not set up terminal using the fallback terminal type '%s'."),
              term);
    }
    return false;
}

/// This is a pretty lame heuristic for detecting terminals that do not support setting the
/// title. If we recognise the terminal name as that of a virtual terminal, we assume it supports
/// setting the title. If we recognise it as that of a console, we assume it does not support
/// setting the title. Otherwise we check the ttyname and see if we believe it is a virtual
/// terminal.
///
/// One situation in which this breaks down is with screen, since screen supports setting the
/// terminal title if the underlying terminal does so, but will print garbage on terminals that
/// don't. Since we can't see the underlying terminal below screen there is no way to fix this.
static const wchar_t *const title_terms[] = {L"xterm",  L"screen", L"tmux",
                                             L"nxterm", L"rxvt",   L"alacritty"};
static bool does_term_support_setting_title(const environment_t &vars) {
    const auto term_var = vars.get(L"TERM");
    if (term_var.missing_or_empty()) return false;

    const wcstring term_str = term_var->as_string();
    const wchar_t *term = term_str.c_str();
    bool recognized = contains(title_terms, term_var->as_string());
    if (!recognized) recognized = !std::wcsncmp(term, L"xterm-", std::wcslen(L"xterm-"));
    if (!recognized) recognized = !std::wcsncmp(term, L"screen-", std::wcslen(L"screen-"));
    if (!recognized) recognized = !std::wcsncmp(term, L"tmux-", std::wcslen(L"tmux-"));
    if (!recognized) {
        if (std::wcscmp(term, L"linux") == 0) return false;
        if (std::wcscmp(term, L"dumb") == 0) return false;
        // NetBSD
        if (std::wcscmp(term, L"vt100") == 0) return false;
        if (std::wcscmp(term, L"wsvt25") == 0) return false;

        char buf[PATH_MAX];
        int retval = ttyname_r(STDIN_FILENO, buf, PATH_MAX);
        if (retval != 0 || std::strstr(buf, "tty") || std::strstr(buf, "/vc/")) return false;
    }

    return true;
}

/// Initialize the curses subsystem.
static void init_curses(const environment_t &vars) {
    for (const auto &var_name : curses_variables) {
        std::string name = wcs2string(var_name);
        const auto var = vars.get(var_name, ENV_EXPORT);
        if (var.missing_or_empty()) {
            FLOGF(term_support, L"curses var %s missing or empty", name.c_str());
            unsetenv_lock(name.c_str());
        } else {
            std::string value = wcs2string(var->as_string());
            FLOGF(term_support, L"curses var %s='%s'", name.c_str(), value.c_str());
            setenv_lock(name.c_str(), value.c_str(), 1);
        }
    }

    int err_ret;
    if (setupterm(nullptr, STDOUT_FILENO, &err_ret) == ERR) {
        auto term = vars.get(L"TERM");
        if (session_interactivity() != session_interactivity_t::not_interactive) {
            FLOGF(warning, _(L"Could not set up terminal."));
            if (term.missing_or_empty()) {
                FLOGF(warning, _(L"TERM environment variable not set."));
            } else {
                FLOGF(warning, _(L"TERM environment variable set to '%ls'."),
                      term->as_string().c_str());
                FLOGF(warning, _(L"Check that this terminal type is supported on this system."));
            }
        }

        if (!initialize_curses_using_fallback(DEFAULT_TERM1)) {
            initialize_curses_using_fallback(DEFAULT_TERM2);
        }
    }

    can_set_term_title = does_term_support_setting_title(vars);
    term_has_xn =
        tigetflag(const_cast<char *>("xenl")) == 1;  // does terminal have the eat_newline_glitch
    update_fish_color_support(vars);
    // Invalidate the cached escape sequences since they may no longer be valid.
    layout_cache_t::shared.clear();
    curses_initialized = true;
}

/// Initialize the locale subsystem.
static void init_locale(const environment_t &vars) {
    // We have to make a copy because the subsequent setlocale() call to change the locale will
    // invalidate the pointer from the this setlocale() call.
    char *old_msg_locale = strdup(setlocale(LC_MESSAGES, nullptr));

    for (const auto &var_name : locale_variables) {
        const auto var = vars.get(var_name, ENV_EXPORT);
        const std::string &name = wcs2string(var_name);
        if (var.missing_or_empty()) {
            FLOGF(env_locale, L"locale var %s missing or empty", name.c_str());
            unsetenv_lock(name.c_str());
        } else {
            const std::string value = wcs2string(var->as_string());
            FLOGF(env_locale, L"locale var %s='%s'", name.c_str(), value.c_str());
            setenv_lock(name.c_str(), value.c_str(), 1);
        }
    }

    char *locale = setlocale(LC_ALL, "");
    fish_setlocale();
    FLOGF(env_locale, L"init_locale() setlocale(): '%s'", locale);

    const char *new_msg_locale = setlocale(LC_MESSAGES, nullptr);
    FLOGF(env_locale, L"old LC_MESSAGES locale: '%s'", old_msg_locale);
    FLOGF(env_locale, L"new LC_MESSAGES locale: '%s'", new_msg_locale);
#ifdef HAVE__NL_MSG_CAT_CNTR
    if (std::strcmp(old_msg_locale, new_msg_locale) != 0) {
        // Make change known to GNU gettext.
        extern int _nl_msg_cat_cntr;
        _nl_msg_cat_cntr++;
    }
#endif
    free(old_msg_locale);
}

/// Returns true if we think the terminal supports setting its title.
bool term_supports_setting_title() { return can_set_term_title; }

/// Miscellaneous variables.
bool g_use_posix_spawn = false;

// Limit `read` to 100 MiB (bytes not wide chars) by default. This can be overridden by the
// fish_read_limit variable.
size_t read_byte_limit = 100 * 1024 * 1024;
