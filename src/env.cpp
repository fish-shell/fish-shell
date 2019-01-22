// Functions for setting and getting environment variables.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <pthread.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

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

/// At init, we read all the environment variables from this array.
extern char **environ;

// Limit `read` to 10 MiB (bytes not wide chars) by default. This can be overridden by the
// fish_read_limit variable.
#define READ_BYTE_LIMIT 10 * 1024 * 1024
size_t read_byte_limit = READ_BYTE_LIMIT;

/// The character used to delimit path and non-path variables in exporting and in string expansion.
static const wchar_t PATH_ARRAY_SEP = L':';
static const wchar_t NONPATH_ARRAY_SEP = L' ';

bool g_use_posix_spawn = false;  // will usually be set to true
bool curses_initialized = false;

/// Does the terminal have the "eat_newline_glitch".
bool term_has_xn = false;

/// This is used to ensure that we don't perform any callbacks from `react_to_variable_change()`
/// when we're importing environment variables in `env_init()`. That's because we don't have any
/// control over the order in which the vars are imported and some of them work in combination.
/// For example, `TERMINFO_DIRS` and `TERM`. If the user has set `TERM` to a custom value that is
/// found in `TERMINFO_DIRS` we don't to call `handle_curses()` before we've imported the latter.
static bool env_initialized = false;

typedef std::unordered_map<wcstring, void (*)(const wcstring &, const wcstring &, env_stack_t &)>
    var_dispatch_table_t;
static var_dispatch_table_t var_dispatch_table;

/// List of all locale environment variable names that might trigger (re)initializing the locale
/// subsystem.
static const wcstring_list_t locale_variables({L"LANG", L"LANGUAGE", L"LC_ALL", L"LC_ADDRESS",
                                               L"LC_COLLATE", L"LC_CTYPE", L"LC_IDENTIFICATION",
                                               L"LC_MEASUREMENT", L"LC_MESSAGES", L"LC_MONETARY",
                                               L"LC_NAME", L"LC_NUMERIC", L"LC_PAPER",
                                               L"LC_TELEPHONE", L"LC_TIME"});

/// List of all curses environment variable names that might trigger (re)initializing the curses
/// subsystem.
static const wcstring_list_t curses_variables({L"TERM", L"TERMINFO", L"TERMINFO_DIRS"});

// Some forward declarations to make it easy to logically group the code.
static void init_locale(const environment_t &vars);
static void init_curses(const environment_t &vars);

// Struct representing one level in the function variable stack.
// Only our variable stack should create and destroy these
class env_node_t {
   public:
    /// Variable table.
    var_table_t env;
    /// Does this node imply a new variable scope? If yes, all non-global variables below this one
    /// in the stack are invisible. If new_scope is set for the global variable node, the universe
    /// will explode.
    bool new_scope;
    /// Does this node contain any variables which are exported to subshells
    /// or does it redefine any variables to not be exported?
    bool exportv = false;
    /// Pointer to next level.
    std::shared_ptr<env_node_t> next;

    env_node_t(bool is_new_scope) : new_scope(is_new_scope) {}

    maybe_t<env_var_t> find_entry(const wcstring &key);

    bool contains_any_of(const wcstring_list_t &vars) const;
};

using env_node_ref_t = std::shared_ptr<env_node_t>;

static std::mutex env_lock;

// A class wrapping up a variable stack
// Currently there is only one variable stack in fish,
// but we can imagine having separate (linked) stacks
// if we introduce multiple threads of execution
struct var_stack_t {
    var_stack_t(var_stack_t &&) = default;

    // Top node on the function stack.
    env_node_ref_t top;

    // Bottom node on the function stack.
    env_node_ref_t global_env;

    // Exported variable array used by execv.
    maybe_t<null_terminated_array_t<char>> export_array;

    /// Flag for checking if we need to regenerate the exported variable array.
    void mark_changed_exported() { export_array.reset(); }

    bool has_changed_exported() const { return !export_array; }

    void update_export_array_if_necessary();

    var_stack_t() : top(globals()), global_env(globals()) {
        // Add a toplevel local scope on top of the global scope. This local scope will persist
        // throughout the lifetime of the fish process, and it will ensure that `set -l` commands
        // run at the command-line don't affect the global scope.
        push(false);
    }

    // Pushes a new node onto our stack
    // Optionally creates a new scope for the node
    void push(bool new_scope);

    // Pops the top node if it's not global
    void pop();

    // Returns the next scope to search for a given node, respecting the new_scope flag.
    // Returns an empty pointer if we're done.
    env_node_ref_t next_scope_to_search(const env_node_ref_t &node) const;

    // Returns the scope used for unspecified scopes. An unspecified scope is either the topmost
    // shadowing scope, or the global scope if none. This implements the default behavior of `set`.
    env_node_ref_t resolve_unspecified_scope();

    /// Copy this vars_stack.
    var_stack_t clone() const {
        return var_stack_t(*this);
    }

   private:
    /// Copy constructor. This does not copy the export array; it just allows it to be regenerated.
    var_stack_t(const var_stack_t &rhs) : top(rhs.top), global_env(rhs.global_env) {}

    bool local_scope_exports(const env_node_ref_t &n) const;
    void get_exported(const env_node_t *n, var_table_t &h) const;

    /// Returns the global variable set.
    static env_node_ref_t globals();
};

env_node_ref_t var_stack_t::globals() {
    static env_node_ref_t s_globals{std::make_shared<env_node_t>(false)};
    return s_globals;
}

void var_stack_t::push(bool new_scope) {
    auto node = std::make_shared<env_node_t>(new_scope);

    // Copy local-exported variables.
    auto top_node = top;
    // Only if we introduce a new shadowing scope; i.e. not if it's just `begin; end` or
    // "--no-scope-shadowing".
    if (new_scope && top_node != this->global_env) {
        for (const auto &var : top_node->env) {
            if (var.second.exports()) node->env.insert(var);
        }
    }

    node->next = this->top;
    this->top = node;
    if (new_scope && local_scope_exports(this->top)) {
        this->mark_changed_exported();
    }
}

/// Return true if if the node contains any of the entries in the vars list.
bool env_node_t::contains_any_of(const wcstring_list_t &vars) const {
    for (const auto &v : vars) {
        if (env.count(v)) return true;
    }
    return false;
}

void var_stack_t::pop() {
    // Don't pop the top-most, global, level.
    if (top == this->global_env) {
        debug(0, _(L"Tried to pop empty environment stack."));
        sanity_lose();
        return;
    }

    bool locale_changed = top->contains_any_of(locale_variables);
    bool curses_changed = top->contains_any_of(curses_variables);

    if (top->new_scope) {  //!OCLINT(collapsible if statements)
        if (top->exportv || local_scope_exports(top->next)) {
            this->mark_changed_exported();
        }
    }

    // Actually do the pop!
    env_node_ref_t old_top = this->top;
    this->top = old_top->next;
    for (const auto &entry_pair : old_top->env) {
        const env_var_t &var = entry_pair.second;
        if (var.exports()) {
            this->mark_changed_exported();
            break;
        }
    }

    // TODO: instantize this locale and curses
    const auto &vars = env_stack_t::principal();
    if (locale_changed) init_locale(vars);
    if (curses_changed) init_curses(vars);
}

env_node_ref_t var_stack_t::next_scope_to_search(const env_node_ref_t &node) const {
    assert(node != NULL);
    if (node == this->global_env) {
        return nullptr;
    }
    return node->new_scope ? this->global_env : node->next;
}

env_node_ref_t var_stack_t::resolve_unspecified_scope() {
    env_node_ref_t node = this->top;
    while (node && !node->new_scope) {
        node = node->next;
    }
    return node ? node : this->global_env;
}

env_stack_t::env_stack_t() : vars_(make_unique<var_stack_t>()) {}
env_stack_t::env_stack_t(std::unique_ptr<var_stack_t> vars) : vars_(std::move(vars)) {}

// Get the variable stack
var_stack_t &env_stack_t::vars_stack() { return *vars_; }

const var_stack_t &env_stack_t::vars_stack() const { return *vars_; }

/// Universal variables global instance. Initialized in env_init.
static env_universal_t *s_universal_variables = NULL;

/// Getter for universal variables.
static env_universal_t *uvars() { return s_universal_variables; }

// A typedef for a set of constant strings. Note our sets are typically on the order of 6 elements,
// so we don't bother to sort them.
using string_set_t = const wchar_t *const[];

template <typename T>
bool string_set_contains(const T &set, const wchar_t *val) {
    for (const wchar_t *entry : set) {
        if (!wcscmp(val, entry)) return true;
    }
    return false;
}

/// Check if a variable may not be set using the set command.
static bool is_read_only(const wchar_t *val) {
    const string_set_t env_read_only = {
        L"PWD",          L"SHLVL",    L"history",  L"status", L"version",
        L"FISH_VERSION", L"fish_pid", L"hostname", L"_",      L"fish_private_mode"};
    return string_set_contains(env_read_only, val) ||
        (in_private_mode() && wcscmp(L"fish_history", val) == 0);
}

static bool is_read_only(const wcstring &val) { return is_read_only(val.c_str()); }

/// Return true if a variable should become a path variable by default. See #436.
static bool variable_should_auto_pathvar(const wcstring &name) {
    return string_suffixes_string(L"PATH", name);
}

/// Table of variables whose value is dynamically calculated, such as umask, status, etc.
static const string_set_t env_electric = {L"history", L"status", L"umask"};

static bool is_electric(const wcstring &key) { return contains(env_electric, key); }

maybe_t<env_var_t> env_node_t::find_entry(const wcstring &key) {
    var_table_t::const_iterator entry = env.find(key);
    if (entry != env.end()) return entry->second;
    return none();
}

/// Return the current umask value.
static mode_t get_umask() {
    mode_t res;
    res = umask(0);
    umask(res);
    return res;
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

/// Some env vars contain a list of paths where an empty path element is equivalent to ".".
/// Unfortunately that convention causes problems for fish scripts. So this function replaces the
/// empty path element with an explicit ".". See issue #3914.
static void fix_colon_delimited_var(const wcstring &var_name, env_stack_t &vars) {
    const auto paths = vars.get(var_name);
    if (paths.missing_or_empty()) return;

    // See if there's any empties.
    const wcstring empty = wcstring();
    const wcstring_list_t &strs = paths->as_list();
    if (contains(strs, empty)) {
        // Copy the list and replace empties with L"."
        wcstring_list_t newstrs = strs;
        std::replace(newstrs.begin(), newstrs.end(), empty, wcstring(L"."));
        int retval = vars.set(var_name, ENV_DEFAULT | ENV_USER, std::move(newstrs));
        if (retval != ENV_OK) {
            debug(0, L"fix_colon_delimited_var failed unexpectedly with retval %d", retval);
        }
    }
}

/// Initialize the locale subsystem.
static void init_locale(const environment_t &vars) {
    // We have to make a copy because the subsequent setlocale() call to change the locale will
    // invalidate the pointer from the this setlocale() call.
    char *old_msg_locale = strdup(setlocale(LC_MESSAGES, NULL));

    for (const auto &var_name : locale_variables) {
        const auto var = vars.get(var_name, ENV_EXPORT);
        const std::string &name = wcs2string(var_name);
        if (var.missing_or_empty()) {
            debug(5, L"locale var %s missing or empty", name.c_str());
            unsetenv(name.c_str());
        } else {
            const std::string value = wcs2string(var->as_string());
            debug(5, L"locale var %s='%s'", name.c_str(), value.c_str());
            setenv(name.c_str(), value.c_str(), 1);
        }
    }

    char *locale = setlocale(LC_ALL, "");
    fish_setlocale();
    debug(5, L"init_locale() setlocale(): '%s'", locale);

    const char *new_msg_locale = setlocale(LC_MESSAGES, NULL);
    debug(5, L"old LC_MESSAGES locale: '%s'", old_msg_locale);
    debug(5, L"new LC_MESSAGES locale: '%s'", new_msg_locale);
#ifdef HAVE__NL_MSG_CAT_CNTR
    if (strcmp(old_msg_locale, new_msg_locale)) {
        // Make change known to GNU gettext.
        extern int _nl_msg_cat_cntr;
        _nl_msg_cat_cntr++;
    }
#endif
    free(old_msg_locale);
}

/// True if we think we can set the terminal title else false.
static bool can_set_term_title = false;

/// Returns true if we think the terminal supports setting its title.
bool term_supports_setting_title() { return can_set_term_title; }

/// This is a pretty lame heuristic for detecting terminals that do not support setting the
/// title. If we recognise the terminal name as that of a virtual terminal, we assume it supports
/// setting the title. If we recognise it as that of a console, we assume it does not support
/// setting the title. Otherwise we check the ttyname and see if we believe it is a virtual
/// terminal.
///
/// One situation in which this breaks down is with screen, since screen supports setting the
/// terminal title if the underlying terminal does so, but will print garbage on terminals that
/// don't. Since we can't see the underlying terminal below screen there is no way to fix this.
static const wchar_t *const title_terms[] = {L"xterm", L"screen", L"tmux", L"nxterm", L"rxvt"};
static bool does_term_support_setting_title(const environment_t &vars) {
    const auto term_var = vars.get(L"TERM");
    if (term_var.missing_or_empty()) return false;

    const wcstring term_str = term_var->as_string();
    const wchar_t *term = term_str.c_str();
    bool recognized = contains(title_terms, term_var->as_string());
    if (!recognized) recognized = !wcsncmp(term, L"xterm-", wcslen(L"xterm-"));
    if (!recognized) recognized = !wcsncmp(term, L"screen-", wcslen(L"screen-"));
    if (!recognized) recognized = !wcsncmp(term, L"tmux-", wcslen(L"tmux-"));
    if (!recognized) {
        if (wcscmp(term, L"linux") == 0) return false;
        if (wcscmp(term, L"dumb") == 0) return false;
        // NetBSD
        if (wcscmp(term, L"vt100") == 0) return false;
        if (wcscmp(term, L"wsvt25") == 0) return false;

        char buf[PATH_MAX];
        int retval = ttyname_r(STDIN_FILENO, buf, PATH_MAX);
        if (retval != 0 || strstr(buf, "tty") || strstr(buf, "/vc/")) return false;
    }

    return true;
}

/// Updates our idea of whether we support term256 and term24bit (see issue #10222).
static void update_fish_color_support(const environment_t &vars) {
    // Detect or infer term256 support. If fish_term256 is set, we respect it;
    // otherwise infer it from the TERM variable or use terminfo.
    auto fish_term256 = vars.get(L"fish_term256");
    auto term_var = vars.get(L"TERM");
    wcstring term = term_var.missing_or_empty() ? L"" : term_var->as_string();
    bool support_term256 = false;  // default to no support
    if (!fish_term256.missing_or_empty()) {
        support_term256 = from_string<bool>(fish_term256->as_string());
        debug(2, L"256 color support determined by 'fish_term256'");
    } else if (term.find(L"256color") != wcstring::npos) {
        // TERM=*256color*: Explicitly supported.
        support_term256 = true;
        debug(2, L"256 color support enabled for '256color' in TERM");
    } else if (term.find(L"xterm") != wcstring::npos) {
        // Assume that all xterms are 256, except for OS X SnowLeopard
        const auto prog_var = vars.get(L"TERM_PROGRAM");
        const auto progver_var = vars.get(L"TERM_PROGRAM_VERSION");
        wcstring term_program = prog_var.missing_or_empty() ? L"" : prog_var->as_string();
        if (term_program == L"Apple_Terminal" && !progver_var.missing_or_empty()) {
            // OS X Lion is version 300+, it has 256 color support
            if (strtod(wcs2str(progver_var->as_string()), NULL) > 300) {
                support_term256 = true;
                debug(2, L"256 color support enabled for TERM=xterm + modern Terminal.app");
            }
        } else {
            support_term256 = true;
            debug(2, L"256 color support enabled for TERM=xterm");
        }
    } else if (cur_term != NULL) {
        // See if terminfo happens to identify 256 colors
        support_term256 = (max_colors >= 256);
        debug(2, L"256 color support: using %d colors per terminfo", max_colors);
    } else {
        debug(2, L"256 color support not enabled (yet)");
    }

    auto fish_term24bit = vars.get(L"fish_term24bit");
    bool support_term24bit;
    if (!fish_term24bit.missing_or_empty()) {
        support_term24bit = from_string<bool>(fish_term24bit->as_string());
        debug(2, L"'fish_term24bit' preference: 24-bit color %s",
              support_term24bit ? L"enabled" : L"disabled");
    } else {
        // We don't attempt to infer term24 bit support yet.
        support_term24bit = false;
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

    if (is_interactive_session) debug(1, _(L"Using fallback terminal type '%s'."), term);

    int err_ret;
    if (setupterm((char *)term, STDOUT_FILENO, &err_ret) == OK) return true;
    if (is_interactive_session) {
        debug(1, _(L"Could not set up terminal using the fallback terminal type '%s'."), term);
    }
    return false;
}

/// Ensure the content of the magic path env vars is reasonable. Specifically, that empty path
/// elements are converted to explicit "." to make the vars easier to use in fish scripts.
static void init_path_vars() {
    // Do not replace empties in MATHPATH - see #4158.
    fix_colon_delimited_var(L"PATH", env_stack_t::globals());
    fix_colon_delimited_var(L"CDPATH", env_stack_t::globals());
}

/// Update the value of g_guessed_fish_emoji_width
static void guess_emoji_width() {
    wcstring term;
    auto &vars = env_stack_t::globals();
    if (auto term_var = vars.get(L"TERM_PROGRAM")) {
        term = term_var->as_string();
    }

    double version = 0;
    if (auto version_var = vars.get(L"TERM_PROGRAM_VERSION")) {
        std::string narrow_version = wcs2string(version_var->as_string());
        version = strtod(narrow_version.c_str(), NULL);
    }

    // iTerm2 defaults to Unicode 8 sizes.
    // See https://gitlab.com/gnachman/iterm2/wikis/unicodeversionswitching

    if (term == L"Apple_Terminal" && version >= 400) {
        // Apple Terminal on High Sierra
        g_guessed_fish_emoji_width = 2;
    } else {
        g_guessed_fish_emoji_width = 1;
    }
}

/// Initialize the curses subsystem.
static void init_curses(const environment_t &vars) {
    for (const auto &var_name : curses_variables) {
        std::string name = wcs2string(var_name);
        const auto var = vars.get(var_name, ENV_EXPORT);
        if (var.missing_or_empty()) {
            debug(2, L"curses var %s missing or empty", name.c_str());
            unsetenv(name.c_str());
        } else {
            std::string value = wcs2string(var->as_string());
            debug(2, L"curses var %s='%s'", name.c_str(), value.c_str());
            setenv(name.c_str(), value.c_str(), 1);
        }
    }

    int err_ret;
    if (setupterm(NULL, STDOUT_FILENO, &err_ret) == ERR) {
        auto term = vars.get(L"TERM");
        if (is_interactive_session) {
            debug(1, _(L"Could not set up terminal."));
            if (term.missing_or_empty()) {
                debug(1, _(L"TERM environment variable not set."));
            } else {
                debug(1, _(L"TERM environment variable set to '%ls'."), term->as_string().c_str());
                debug(1, _(L"Check that this terminal type is supported on this system."));
            }
        }

        if (!initialize_curses_using_fallback(DEFAULT_TERM1)) {
            initialize_curses_using_fallback(DEFAULT_TERM2);
        }
    }

    can_set_term_title = does_term_support_setting_title(vars);
    term_has_xn = tigetflag((char *)"xenl") == 1;  // does terminal have the eat_newline_glitch
    update_fish_color_support(vars);
    // Invalidate the cached escape sequences since they may no longer be valid.
    cached_layouts.clear();
    curses_initialized = true;
}

/// React to modifying the given variable.
static void react_to_variable_change(const wchar_t *op, const wcstring &key, env_stack_t &vars) {
    // Don't do any of this until `env_init()` has run. We only want to do this in response to
    // variables set by the user; e.g., in a script like *config.fish* or interactively or as part
    // of loading the universal variables for the first time. Variables we import from the
    // environment or that are otherwise set by fish before this gets called have to explicitly
    // call the appropriate functions to put the value of the var into effect.
    if (!env_initialized) return;

    auto dispatch = var_dispatch_table.find(key);
    if (dispatch != var_dispatch_table.end()) {
        (*dispatch->second)(op, key, vars);
    } else if (string_prefixes_string(L"fish_color_", key)) {
        reader_react_to_color_change();
    }
}

/// Universal variable callback function. This function makes sure the proper events are triggered
/// when an event occurs.
static void universal_callback(env_stack_t *stack, const callback_data_t &cb) {
    const wchar_t *op = cb.is_erase() ? L"ERASE" : L"SET";

    react_to_variable_change(op, cb.key, *stack);
    stack->mark_changed_exported();

    event_t ev = event_t::variable_event(cb.key);
    ev.arguments.push_back(L"VARIABLE");
    ev.arguments.push_back(op);
    ev.arguments.push_back(cb.key);
    event_fire(&ev);
}

/// Make sure the PATH variable contains something.
static void setup_path() {
    auto &vars = env_stack_t::globals();
    const auto path = vars.get(L"PATH");
    if (path.missing_or_empty()) {
        wcstring_list_t value({L"/usr/bin", L"/bin"});
        vars.set(L"PATH", ENV_GLOBAL | ENV_EXPORT, value);
    }
}

/// If they don't already exist initialize the `COLUMNS` and `LINES` env vars to reasonable
/// defaults. They will be updated later by the `get_current_winsize()` function if they need to be
/// adjusted.
void env_stack_t::set_termsize() {
    auto &vars = env_stack_t::globals();
    auto cols = get(L"COLUMNS");
    if (cols.missing_or_empty()) vars.set_one(L"COLUMNS", ENV_GLOBAL, DFLT_TERM_COL_STR);

    auto rows = get(L"LINES");
    if (rows.missing_or_empty()) vars.set_one(L"LINES", ENV_GLOBAL, DFLT_TERM_ROW_STR);
}

/// Update the PWD variable directory from the result of getcwd().
void env_stack_t::set_pwd_from_getcwd() {
    wcstring cwd = wgetcwd();
    if (cwd.empty()) {
        debug(0,
              _(L"Could not determine current working directory. Is your locale set correctly?"));
        return;
    }
    set_one(L"PWD", ENV_EXPORT | ENV_GLOBAL, cwd);
}

/// Allow the user to override the limit on how much data the `read` command will process.
/// This is primarily for testing but could be used by users in special situations.
void env_stack_t::set_read_limit() {
    auto read_byte_limit_var = this->get(L"fish_read_limit");
    if (!read_byte_limit_var.missing_or_empty()) {
        size_t limit = fish_wcstoull(read_byte_limit_var->as_string().c_str());
        if (errno) {
            debug(1, "Ignoring fish_read_limit since it is not valid");
        } else {
            read_byte_limit = limit;
        }
    }
}

void env_stack_t::mark_changed_exported() { vars_stack().mark_changed_exported(); }

wcstring environment_t::get_pwd_slash() const {
    // Return "/" if PWD is missing.
    // See https://github.com/fish-shell/fish-shell/issues/5080
    auto pwd_var = get(L"PWD");
    wcstring pwd;
    if (!pwd_var.missing_or_empty()) {
        pwd = pwd_var->as_string();
    }
    if (!string_suffixes_string(L"/", pwd)) {
        pwd.push_back(L'/');
    }
    return pwd;
}

/// Set up the USER variable.
static void setup_user(bool force) {
    auto &vars = env_stack_t::globals();
    if (force || vars.get(L"USER").missing_or_empty()) {
        struct passwd userinfo;
        struct passwd *result;
        char buf[8192];
        int retval = getpwuid_r(getuid(), &userinfo, buf, sizeof(buf), &result);
        if (!retval && result) {
            const wcstring uname = str2wcstring(userinfo.pw_name);
            vars.set_one(L"USER", ENV_GLOBAL | ENV_EXPORT, uname);
        }
    }
}

/// Various things we need to initialize at run-time that don't really fit any of the other init
/// routines.
void misc_init() {
    // If stdout is open on a tty ensure stdio is unbuffered. That's because those functions might
    // be intermixed with `write()` calls and we need to ensure the writes are not reordered. See
    // issue #3748.
    if (isatty(STDOUT_FILENO)) {
        fflush(stdout);
        setvbuf(stdout, NULL, _IONBF, 0);
    }
}

static void env_universal_callbacks(env_stack_t *stack, const callback_data_list_t &callbacks) {
    for (const callback_data_t &cb : callbacks) {
        universal_callback(stack, cb);
    }
}

void env_stack_t::universal_barrier() {
    ASSERT_IS_MAIN_THREAD();
    if (!uvars()) return;

    callback_data_list_t callbacks;
    bool changed = uvars()->sync(callbacks);
    if (changed) {
        universal_notifier_t::default_notifier().post_notification();
    }

    env_universal_callbacks(this, callbacks);
}

static void handle_fish_term_change(const wcstring &op, const wcstring &var_name,
                                    env_stack_t &vars) {
    UNUSED(op);
    UNUSED(var_name);
    update_fish_color_support(vars);
    reader_react_to_color_change();
}

static void handle_escape_delay_change(const wcstring &op, const wcstring &var_name,
                                       env_stack_t &vars) {
    UNUSED(op);
    UNUSED(var_name);
    update_wait_on_escape_ms(vars);
}

static void handle_change_emoji_width(const wcstring &op, const wcstring &var_name,
                                      env_stack_t &vars) {
    (void)op;
    (void)var_name;
    int new_width = 0;
    if (auto width_str = vars.get(L"fish_emoji_width")) {
        new_width = fish_wcstol(width_str->as_string().c_str());
    }
    g_fish_emoji_width = std::max(0, new_width);
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
    reader_change_history(history_session_id(vars).c_str());
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

static void handle_locale_change(const wcstring &op, const wcstring &var_name, env_stack_t &vars) {
    UNUSED(op);
    UNUSED(var_name);
    init_locale(vars);
}

static void handle_curses_change(const wcstring &op, const wcstring &var_name, env_stack_t &vars) {
    UNUSED(op);
    UNUSED(var_name);
    guess_emoji_width();
    init_curses(vars);
}

/// Populate the dispatch table used by `react_to_variable_change()` to efficiently call the
/// appropriate function to handle a change to a variable.
static void setup_var_dispatch_table() {
    for (const auto &var_name : locale_variables) {
        var_dispatch_table.emplace(var_name, handle_locale_change);
    }

    for (const auto &var_name : curses_variables) {
        var_dispatch_table.emplace(var_name, handle_curses_change);
    }

    var_dispatch_table.emplace(L"PATH", handle_magic_colon_var_change);
    var_dispatch_table.emplace(L"CDPATH", handle_magic_colon_var_change);
    var_dispatch_table.emplace(L"fish_term256", handle_fish_term_change);
    var_dispatch_table.emplace(L"fish_term24bit", handle_fish_term_change);
    var_dispatch_table.emplace(L"fish_escape_delay_ms", handle_escape_delay_change);
    var_dispatch_table.emplace(L"fish_emoji_width", handle_change_emoji_width);
    var_dispatch_table.emplace(L"fish_ambiguous_width", handle_change_ambiguous_width);
    var_dispatch_table.emplace(L"LINES", handle_term_size_change);
    var_dispatch_table.emplace(L"COLUMNS", handle_term_size_change);
    var_dispatch_table.emplace(L"fish_complete_path", handle_complete_path_change);
    var_dispatch_table.emplace(L"fish_function_path", handle_function_path_change);
    var_dispatch_table.emplace(L"fish_read_limit", handle_read_limit_change);
    var_dispatch_table.emplace(L"fish_history", handle_fish_history_change);
    var_dispatch_table.emplace(L"TZ", handle_tz_change);
}

void env_init(const struct config_paths_t *paths /* or NULL */) {
    setup_var_dispatch_table();

    env_stack_t &vars = env_stack_t::globals();
    // Import environment variables. Walk backwards so that the first one out of any duplicates wins
    // (See issue #2784).
    wcstring key, val;
    const char *const *envp = environ;
    size_t i = 0;
    while (envp && envp[i]) i++;
    while (i--) {
        const wcstring key_and_val = str2wcstring(envp[i]);  // like foo=bar
        size_t eql = key_and_val.find(L'=');
        if (eql == wcstring::npos) {
            // No equal-sign found so treat it as a defined var that has no value(s).
            if (is_read_only(key_and_val) || is_electric(key_and_val)) continue;
            vars.set_empty(key_and_val, ENV_EXPORT | ENV_GLOBAL);
        } else {
            key.assign(key_and_val, 0, eql);
            val.assign(key_and_val, eql+1, wcstring::npos);
            if (is_read_only(key) || is_electric(key)) continue;
            vars.set(key, ENV_EXPORT | ENV_GLOBAL, {val});
        }
    }

    // Set the given paths in the environment, if we have any.
    if (paths != NULL) {
        vars.set_one(FISH_DATADIR_VAR, ENV_GLOBAL, paths->data);
        vars.set_one(FISH_SYSCONFDIR_VAR, ENV_GLOBAL, paths->sysconf);
        vars.set_one(FISH_HELPDIR_VAR, ENV_GLOBAL, paths->doc);
        vars.set_one(FISH_BIN_DIR, ENV_GLOBAL, paths->bin);
    }

    wcstring user_config_dir;
    path_get_config(user_config_dir);
    vars.set_one(FISH_CONFIG_DIR, ENV_GLOBAL, user_config_dir);

    wcstring user_data_dir;
    path_get_data(user_data_dir);
    vars.set_one(FISH_USER_DATA_DIR, ENV_GLOBAL, user_data_dir);

    init_locale(vars);
    init_curses(vars);
    init_input();
    init_path_vars();
    guess_emoji_width();

    // Set up the USER and PATH variables
    setup_path();

    // Some `su`s keep $USER when changing to root.
    // This leads to issues later on (and e.g. in prompts),
    // so we work around it by resetting $USER.
    // TODO: Figure out if that su actually checks if username == "root"(as the man page says) or
    // UID == 0.
    uid_t uid = getuid();
    setup_user(uid == 0);

    // Set up $IFS - this used to be in share/config.fish, but really breaks if it isn't done.
    vars.set_one(L"IFS", ENV_GLOBAL, L"\n \t");

    // Set up the version variable.
    wcstring version = str2wcstring(get_fish_version());
    vars.set_one(L"version", ENV_GLOBAL, version);
    vars.set_one(L"FISH_VERSION", ENV_GLOBAL, version);

    // Set the $fish_pid variable.
    vars.set_one(L"fish_pid", ENV_GLOBAL, to_string<long>(getpid()));

    // Set the $hostname variable
    wcstring hostname = L"fish";
    get_hostname_identifier(hostname);
    vars.set_one(L"hostname", ENV_GLOBAL, hostname);

    // Set up SHLVL variable. Not we can't use vars.get() because SHLVL is read-only, and therefore
    // was not inherited from the environment.
    wcstring nshlvl_str = L"1";
    if (const char *shlvl_var = getenv("SHLVL")) {
        const wchar_t *end;
        // TODO: Figure out how to handle invalid numbers better. Shouldn't we issue a diagnostic?
        long shlvl_i = fish_wcstol(str2wcstring(shlvl_var).c_str(), &end);
        if (!errno && shlvl_i >= 0) {
            nshlvl_str = to_string<long>(shlvl_i + 1);
        }
    }
    vars.set_one(L"SHLVL", ENV_GLOBAL | ENV_EXPORT, nshlvl_str);

    // Set up the HOME variable.
    // Unlike $USER, it doesn't seem that `su`s pass this along
    // if the target user is root, unless "--preserve-environment" is used.
    // Since that is an explicit choice, we should allow it to enable e.g.
    //     env HOME=(mktemp -d) su --preserve-environment fish
    if (vars.get(L"HOME").missing_or_empty()) {
        auto user_var = vars.get(L"USER");
        if (!user_var.missing_or_empty()) {
            char *unam_narrow = wcs2str(user_var->as_string());
            struct passwd userinfo;
            struct passwd *result;
            char buf[8192];
            int retval = getpwnam_r(unam_narrow, &userinfo, buf, sizeof(buf), &result);
            if (retval || !result) {
                // Maybe USER is set but it's bogus. Reset USER from the db and try again.
                setup_user(true);
                user_var = vars.get(L"USER");
                if (!user_var.missing_or_empty()) {
                    unam_narrow = wcs2str(user_var->as_string());
                    retval = getpwnam_r(unam_narrow, &userinfo, buf, sizeof(buf), &result);
                }
            }
            if (!retval && result && userinfo.pw_dir) {
                const wcstring dir = str2wcstring(userinfo.pw_dir);
                vars.set_one(L"HOME", ENV_GLOBAL | ENV_EXPORT, dir);
            } else {
                // We cannot get $HOME. This triggers warnings for history and config.fish already,
                // so it isn't necessary to warn here as well.
                vars.set_empty(L"HOME", ENV_GLOBAL | ENV_EXPORT);
            }
            free(unam_narrow);
        } else {
            // If $USER is empty as well (which we tried to set above), we can't get $HOME.
            vars.set_empty(L"HOME", ENV_GLOBAL | ENV_EXPORT);
        }
    }

    // initialize the PWD variable if necessary
    // Note we may inherit a virtual PWD that doesn't match what getcwd would return; respect that.
    if (vars.get(L"PWD").missing_or_empty()) {
        vars.set_pwd_from_getcwd();
    }
    vars.set_termsize();    // initialize the terminal size variables
    vars.set_read_limit();  // initialize the read_byte_limit

    // Set g_use_posix_spawn. Default to true.
    auto use_posix_spawn = vars.get(L"fish_use_posix_spawn");
    g_use_posix_spawn =
        use_posix_spawn.missing_or_empty() ? true : from_string<bool>(use_posix_spawn->as_string());

    // Set fish_bind_mode to "default".
    vars.set_one(FISH_BIND_MODE_VAR, ENV_GLOBAL, DEFAULT_BIND_MODE);

    // This is somewhat subtle. At this point we consider our environment to be sufficiently
    // initialized that we can react to changes to variables. Prior to doing this we expect that the
    // code for setting vars that might have side-effects will do whatever
    // `react_to_variable_change()` would do for that var.
    env_initialized = true;

    // Set up universal variables. The empty string means to use the default path.
    assert(s_universal_variables == NULL);
    s_universal_variables = new env_universal_t(L"");
    callback_data_list_t callbacks;
    s_universal_variables->initialize(callbacks);
    env_universal_callbacks(&env_stack_t::principal(), callbacks);
}

/// Search all visible scopes in order for the specified key. Return the first scope in which it was
/// found.
env_node_ref_t env_stack_t::get_node(const wcstring &key) {
    env_node_ref_t env = vars_stack().top;
    while (env) {
        if (env->find_entry(key)) break;
        env = vars_stack().next_scope_to_search(env);
    }
    return env;
}

static int set_umask(const wcstring_list_t &list_val) {
    long mask = -1;
    if (list_val.size() == 1 && !list_val.front().empty()) {
        mask = fish_wcstol(list_val.front().c_str(), NULL, 8);
    }

    if (errno || mask > 0777 || mask < 0) return ENV_INVALID;
    // Do not actually create a umask variable. On env_stack_t::get() it will be calculated.
    umask(mask);
    return ENV_OK;
}

/// Set a universal variable, inheriting as applicable from the given old variable.
static void env_set_internal_universal(const wcstring &key, wcstring_list_t val,
                                       env_mode_flags_t input_var_mode, env_stack_t *stack) {
    ASSERT_IS_MAIN_THREAD();
    if (!uvars()) return;
    env_mode_flags_t var_mode = input_var_mode;
    auto oldvar = uvars()->get(key);
    // Resolve whether or not to export.
    if ((var_mode & (ENV_EXPORT | ENV_UNEXPORT)) == 0) {
        bool doexport = oldvar ? oldvar->exports() : false;
        var_mode |= (doexport ? ENV_EXPORT : ENV_UNEXPORT);
    }
    // Resolve whether to be a path variable.
    // Here we fall back to the auto-pathvar behavior.
    if ((var_mode & (ENV_PATHVAR | ENV_UNPATHVAR)) == 0) {
        bool dopathvar = oldvar ? oldvar->is_pathvar() : variable_should_auto_pathvar(key);
        var_mode |= (dopathvar ? ENV_PATHVAR : ENV_UNPATHVAR);
    }

    // Split about ':' if it's a path variable.
    if (var_mode & ENV_PATHVAR) {
        wcstring_list_t split_val;
        for (const wcstring &str : val) {
            vec_append(split_val, split_string(str, PATH_ARRAY_SEP));
        }
        val = std::move(split_val);
    }

    // Construct and set the new variable.
    env_var_t::env_var_flags_t varflags = 0;
    if (var_mode & ENV_EXPORT) varflags |= env_var_t::flag_export;
    if (var_mode & ENV_PATHVAR) varflags |= env_var_t::flag_pathvar;
    env_var_t new_var{val, varflags};

    uvars()->set(key, new_var);
    env_universal_barrier();
    if (new_var.exports() || (oldvar && oldvar->exports())) {
        stack->mark_changed_exported();
    }
}

/// Set the value of the environment variable whose name matches key to val.
///
/// \param key The key
/// \param val The value to set.
/// \param var_mode The type of the variable. Can be any combination of ENV_GLOBAL, ENV_LOCAL,
/// ENV_EXPORT and ENV_USER. If mode is ENV_DEFAULT, the current variable space is searched and the
/// current mode is used. If no current variable with the same name is found, ENV_LOCAL is assumed.
///
/// Returns:
///
/// * ENV_OK on success.
/// * ENV_PERM, can only be returned when setting as a user, e.g. ENV_USER is set. This means that
/// the user tried to change a read-only variable.
/// * ENV_SCOPE, the variable cannot be set in the given scope. This applies to readonly/electric
/// variables set from the local or universal scopes, or set as exported.
/// * ENV_INVALID, the variable value was invalid. This applies only to special variables.
int env_stack_t::set_internal(const wcstring &key, env_mode_flags_t input_var_mode, wcstring_list_t val) {
    ASSERT_IS_MAIN_THREAD();
    env_mode_flags_t var_mode = input_var_mode;
    bool has_changed_old = vars_stack().has_changed_exported();
    int done = 0;

    if (val.size() == 1 && (key == L"PWD" || key == L"HOME")) {
        // Canonicalize our path; if it changes, recurse and try again.
        wcstring val_canonical = val.front();
        path_make_canonical(val_canonical);
        if (val.front() != val_canonical) {
            return set_internal(key, var_mode, {val_canonical});
        }
    }

    if ((var_mode & ENV_LOCAL || var_mode & ENV_UNIVERSAL) &&
        (is_read_only(key) || is_electric(key))) {
        return ENV_SCOPE;
    }
    if ((var_mode & ENV_EXPORT) && is_electric(key)) {
        return ENV_SCOPE;
    }
    if ((var_mode & ENV_USER) && is_read_only(key)) {
        return ENV_PERM;
    }

    if (key == L"umask") {  // set new umask
        return set_umask(val);
    }

    if (var_mode & ENV_UNIVERSAL) {
        if (uvars()) {
            env_set_internal_universal(key, std::move(val), var_mode, this);
        }
    } else {
        // Determine the node.
        bool has_changed_new = false;
        env_node_ref_t preexisting_node = get_node(key);
        maybe_t<env_var_t::env_var_flags_t> preexisting_flags{};
        if (preexisting_node != NULL) {
            var_table_t::const_iterator result = preexisting_node->env.find(key);
            assert(result != preexisting_node->env.end());
            preexisting_flags = result->second.get_flags();
            if (*preexisting_flags & env_var_t::flag_export) {
                has_changed_new = true;
            }
        }

        env_node_ref_t node = nullptr;
        if (var_mode & ENV_GLOBAL) {
            node = vars_stack().global_env;
        } else if (var_mode & ENV_LOCAL) {
            node = vars_stack().top;
        } else if (preexisting_node != NULL) {
            node = preexisting_node;
            if ((var_mode & (ENV_EXPORT | ENV_UNEXPORT)) == 0) {
                // Use existing entry's exportv status.
                if (preexisting_flags && (*preexisting_flags & env_var_t::flag_export)) {
                    var_mode |= ENV_EXPORT;
                }
            }
        } else {
            if (!get_proc_had_barrier()) {
                set_proc_had_barrier(true);
                env_universal_barrier();
            }
            if (uvars() && uvars()->get(key)) {
                // Modifying an existing universal variable.
                env_set_internal_universal(key, std::move(val), var_mode, this);
                done = true;
            } else {
                // New variable with unspecified scope
                node = vars_stack().resolve_unspecified_scope();
            }
        }

        if (!done) {
            // Resolve if we should mark ourselves as a path variable or not.
            // If there's an existing variable, use its path flag; otherwise infer it.
            if ((var_mode & (ENV_PATHVAR | ENV_UNPATHVAR)) == 0) {
                bool should_pathvar = false;
                if (preexisting_flags) {
                    should_pathvar = *preexisting_flags & env_var_t::flag_pathvar;
                } else {
                    should_pathvar = variable_should_auto_pathvar(key);
                }
                var_mode |= should_pathvar ? ENV_PATHVAR : ENV_UNPATHVAR;
            }

            // Split about ':' if it's a path variable.
            if (var_mode & ENV_PATHVAR) {
                wcstring_list_t split_val;
                for (const wcstring &str : val) {
                    vec_append(split_val, split_string(str, PATH_ARRAY_SEP));
                }
                val = std::move(split_val);
            }

            // Set the entry in the node. Note that operator[] accesses the existing entry, or
            // creates a new one.
            env_var_t &var = node->env[key];
            if (var.exports()) {
                // This variable already existed, and was exported.
                has_changed_new = true;
            }

            var.set_vals(std::move(val));
            var.set_pathvar(var_mode & ENV_PATHVAR);
            var.set_read_only(is_read_only(key));

            if (var_mode & ENV_EXPORT) {
                // The new variable is exported.
                var.set_exports(true);
                node->exportv = true;
                has_changed_new = true;
            } else {
                var.set_exports(false);
                // Set the node's exported when it changes something about exports
                // (also when it redefines a variable to not be exported).
                node->exportv = has_changed_old != has_changed_new;
            }

            if (has_changed_old || has_changed_new) vars_stack().mark_changed_exported();
        }
    }

    event_t ev = event_t::variable_event(key);
    ev.arguments.reserve(3);
    ev.arguments.push_back(L"VARIABLE");
    ev.arguments.push_back(L"SET");
    ev.arguments.push_back(key);
    event_fire(&ev);
    react_to_variable_change(L"SET", key, *this);
    return ENV_OK;
}

/// Sets the variable with the specified name to the given values.
int env_stack_t::set(const wcstring &key, env_mode_flags_t mode, wcstring_list_t vals) {
    return set_internal(key, mode, std::move(vals));
}

/// Sets the variable with the specified name to a single value.
int env_stack_t::set_one(const wcstring &key, env_mode_flags_t mode, wcstring val) {
    wcstring_list_t vals;
    vals.push_back(std::move(val));
    return set_internal(key, mode, std::move(vals));
}

/// Sets the variable with the specified name without any (i.e., zero) values.
int env_stack_t::set_empty(const wcstring &key, env_mode_flags_t mode) {
    return set_internal(key, mode, {});
}

/// Attempt to remove/free the specified key/value pair from the specified map.
///
/// \return zero if the variable was not found, non-zero otherwise
bool env_stack_t::try_remove(env_node_ref_t n, const wchar_t *key, int var_mode) {
    if (n == nullptr) {
        return false;
    }

    var_table_t::iterator result = n->env.find(key);
    if (result != n->env.end()) {
        if (result->second.exports()) {
            mark_changed_exported();
        }
        n->env.erase(result);
        return true;
    }

    if (var_mode & ENV_LOCAL) {
        return false;
    }

    if (n->new_scope) {
        return try_remove(vars_stack().global_env, key, var_mode);
    }
    return try_remove(n->next, key, var_mode);
}

int env_stack_t::remove(const wcstring &key, int var_mode) {
    ASSERT_IS_MAIN_THREAD();
    env_node_ref_t first_node{};
    int erased = 0;

    if ((var_mode & ENV_USER) && is_read_only(key)) {
        return ENV_SCOPE;
    }

    first_node = vars_stack().top;

    if (!(var_mode & ENV_UNIVERSAL)) {
        if (var_mode & ENV_GLOBAL) {
            first_node = vars_stack().global_env;
        }

        if (try_remove(first_node, key.c_str(), var_mode)) {
            event_t ev = event_t::variable_event(key);
            ev.arguments.push_back(L"VARIABLE");
            ev.arguments.push_back(L"ERASE");
            ev.arguments.push_back(key);
            event_fire(&ev);

            erased = 1;
        }
    }

    if (!erased && !(var_mode & ENV_GLOBAL) && !(var_mode & ENV_LOCAL)) {
        bool is_exported = false;
        if (uvars()) {
            if (auto old_flags = uvars()->get_flags(key)) {
                is_exported = *old_flags & env_var_t::flag_export;
            }
            erased = uvars()->remove(key);
        }
        if (erased) {
            env_universal_barrier();
            event_t ev = event_t::variable_event(key);
            ev.arguments.push_back(L"VARIABLE");
            ev.arguments.push_back(L"ERASE");
            ev.arguments.push_back(key);
            event_fire(&ev);
        }

        if (is_exported) vars_stack().mark_changed_exported();
    }

    react_to_variable_change(L"ERASE", key, *this);

    return erased ? ENV_OK : ENV_NOT_FOUND;
}

const wcstring_list_t &env_var_t::as_list() const { return vals; }

wchar_t env_var_t::get_delimiter() const {
    return is_pathvar() ? PATH_ARRAY_SEP : NONPATH_ARRAY_SEP;
}

/// Return a string representation of the var.
wcstring env_var_t::as_string() const {
    return join_strings(vals, get_delimiter());
}

void env_var_t::to_list(wcstring_list_t &out) const {
    out = vals;
}

env_var_t::env_var_flags_t env_var_t::flags_for(const wchar_t *name) {
    env_var_flags_t result = 0;
    if (is_read_only(name)) result |= flag_read_only;
    return result;
}

maybe_t<env_var_t> env_stack_t::get(const wcstring &key, env_mode_flags_t mode) const {
    const bool has_scope = mode & (ENV_LOCAL | ENV_GLOBAL | ENV_UNIVERSAL);
    const bool search_local = !has_scope || (mode & ENV_LOCAL);
    const bool search_global = !has_scope || (mode & ENV_GLOBAL);
    const bool search_universal = !has_scope || (mode & ENV_UNIVERSAL);

    const bool search_exported = (mode & ENV_EXPORT) || !(mode & ENV_UNEXPORT);
    const bool search_unexported = (mode & ENV_UNEXPORT) || !(mode & ENV_EXPORT);

    // Make the assumption that electric keys can't be shadowed elsewhere, since we currently block
    // that in env_stack_t::set().
    if (is_electric(key)) {
        if (!search_global) return none();
        if (key == L"history") {
            // Big hack. We only allow getting the history on the main thread. Note that history_t
            // may ask for an environment variable, so don't take the lock here (we don't need it).
            if (!is_main_thread()) {
                return none();
            }

            history_t *history = reader_get_history();
            if (!history) {
                history = &history_t::history_with_name(history_session_id(*this));
            }
            wcstring_list_t result;
            if (history) history->get_history(result);
            return env_var_t(L"history", result);
        } else if (key == L"status") {
            return env_var_t(L"status", to_string(proc_get_last_status()));
        } else if (key == L"umask") {
            return env_var_t(L"umask", format_string(L"0%0.3o", get_umask()));
        }
        // We should never get here unless the electric var list is out of sync with the above code.
        DIE("unerecognized electric var name");
    }

    if (search_local || search_global) {
        scoped_lock locker(env_lock);  // lock around a local region
        env_node_ref_t env = search_local ? vars_stack().top : vars_stack().global_env;

        while (env != NULL) {
            if (env == vars_stack().global_env && !search_global) {
                break;
            }

            var_table_t::const_iterator result = env->env.find(key);
            if (result != env->env.end()) {
                const env_var_t &var = result->second;
                if (var.exports() ? search_exported : search_unexported) {
                    return var;
                }
            }
            env = vars_stack().next_scope_to_search(env);
        }
    }

    if (!search_universal) return none();

    // Another hack. Only do a universal barrier on the main thread (since it can change variable
    // values). Make sure we do this outside the env_lock because it may itself call `get()`.
    if (is_main_thread() && !get_proc_had_barrier()) {
        set_proc_had_barrier(true);
        env_universal_barrier();
    }

    // Okay, we couldn't find a local or global var given the requirements. If there is a matching
    // universal var return that.
    if (uvars()) {
        auto var = uvars()->get(key);
        if (var && (var->exports() ? search_exported : search_unexported)) {
            return var;
        }
    }

    return none();
}

void env_universal_barrier() { env_stack_t::principal().universal_barrier(); }

/// Returns true if the specified scope or any non-shadowed non-global subscopes contain an exported
/// variable.
bool var_stack_t::local_scope_exports(const env_node_ref_t &n) const {
    assert(n != nullptr);
    if (n == global_env) return false;

    if (n->exportv) return true;

    if (n->new_scope) return false;

    return local_scope_exports(n->next);
}

void env_stack_t::push(bool new_scope) { vars_stack().push(new_scope); }

void env_stack_t::pop() { vars_stack().pop(); }

/// Function used with to insert keys of one table into a set::set<wcstring>.
static void add_key_to_string_set(const var_table_t &envs, std::set<wcstring> *str_set,
                                  bool show_exported, bool show_unexported) {
    var_table_t::const_iterator iter;
    for (iter = envs.begin(); iter != envs.end(); ++iter) {
        const env_var_t &var = iter->second;

        if ((var.exports() && show_exported) || (!var.exports() && show_unexported)) {
            // Insert this key.
            str_set->insert(iter->first);
        }
    }
}

wcstring_list_t env_stack_t::get_names(int flags) const {
    scoped_lock locker(env_lock);

    wcstring_list_t result;
    std::set<wcstring> names;
    int show_local = flags & ENV_LOCAL;
    int show_global = flags & ENV_GLOBAL;
    int show_universal = flags & ENV_UNIVERSAL;

    env_node_ref_t n = vars_stack().top;
    const bool show_exported = (flags & ENV_EXPORT) || !(flags & ENV_UNEXPORT);
    const bool show_unexported = (flags & ENV_UNEXPORT) || !(flags & ENV_EXPORT);

    if (!show_local && !show_global && !show_universal) {
        show_local = show_universal = show_global = 1;
    }

    if (show_local) {
        while (n) {
            if (n == vars_stack().global_env) break;

            add_key_to_string_set(n->env, &names, show_exported, show_unexported);
            if (n->new_scope)
                break;
            else
                n = n->next;
        }
    }

    if (show_global) {
        add_key_to_string_set(vars_stack().global_env->env, &names, show_exported, show_unexported);
        if (show_unexported) {
            result.insert(result.end(), std::begin(env_electric), std::end(env_electric));
        }
    }

    if (show_universal && uvars()) {
        const wcstring_list_t uni_list = uvars()->get_names(show_exported, show_unexported);
        names.insert(uni_list.begin(), uni_list.end());
    }

    result.insert(result.end(), names.begin(), names.end());
    return result;
}

/// Get list of all exported variables.
void var_stack_t::get_exported(const env_node_t *n, var_table_t &h) const {
    if (!n) return;

    if (n->new_scope) {
        get_exported(global_env.get(), h);
    } else {
        get_exported(n->next.get(), h);
    }

    var_table_t::const_iterator iter;
    for (iter = n->env.begin(); iter != n->env.end(); ++iter) {
        const wcstring &key = iter->first;
        const env_var_t var = iter->second;

        if (var.exports()) {
            // Export the variable. Don't use std::map::insert here, since we need to overwrite
            // existing values from previous scopes.
            h[key] = var;
        } else {
            // We need to erase from the map if we are not exporting, since a lower scope may have
            // exported. See #2132.
            h.erase(key);
        }
    }
}

// Given a map from key to value, return a vector of strings of the form key=value
static std::vector<std::string> get_export_list(const var_table_t &envs) {
    std::vector<std::string> result;
    result.reserve(envs.size());
    for (const auto &kv : envs) {
        std::string ks = wcs2string(kv.first);
        std::string vs = wcs2string(kv.second.as_string());
        // Create and append a string of the form ks=vs
        std::string str;
        str.reserve(ks.size() + 1 + vs.size());
        str.append(ks);
        str.append("=");
        str.append(vs);
        result.push_back(std::move(str));
    }
    return result;
}

void var_stack_t::update_export_array_if_necessary() {
    if (!this->has_changed_exported()) {
        return;
    }

    debug(4, L"export_arr() recalc");
    var_table_t vals;
    get_exported(this->top.get(), vals);

    if (uvars()) {
        const wcstring_list_t uni = uvars()->get_names(true, false);
        for (const wcstring &key : uni) {
            auto var = uvars()->get(key);

            if (!var.missing_or_empty()) {
                // Note that std::map::insert does NOT overwrite a value already in the map,
                // which we depend on here.
                vals.insert(std::pair<wcstring, env_var_t>(key, *var));
            }
        }
    }

    export_array.emplace(get_export_list(vals));
}

const char *const *env_stack_t::export_arr() {
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_NOT_FORKED_CHILD();
    vars_stack().update_export_array_if_necessary();
    assert(vars_stack().export_array && "Should have export array");
    return vars_stack().export_array->get();
}

void env_stack_t::set_argv(const wchar_t *const *argv) {
    if (argv && *argv) {
        wcstring_list_t list;
        for (auto arg = argv; *arg; arg++) {
            list.emplace_back(*arg);
        }
        set(L"argv", ENV_LOCAL, std::move(list));
    } else {
        set_empty(L"argv", ENV_LOCAL);
    }
}

environment_t::~environment_t() = default;
env_stack_t::~env_stack_t() = default;
env_stack_t::env_stack_t(env_stack_t &&) = default;

null_environment_t::null_environment_t() = default;
null_environment_t::~null_environment_t() = default;
maybe_t<env_var_t> null_environment_t::get(const wcstring &key, env_mode_flags_t mode) const {
    UNUSED(key);
    UNUSED(mode);
    return none();
}
wcstring_list_t null_environment_t::get_names(int flags) const {
    UNUSED(flags);
    return {};
}

env_stack_t env_stack_t::make_principal() {
    const env_stack_t &gl = env_stack_t::globals();
    std::unique_ptr<var_stack_t> dup_stack = make_unique<var_stack_t>(gl.vars_stack().clone());
    return env_stack_t{std::move(dup_stack)};
}

env_stack_t &env_stack_t::principal() {
    static env_stack_t s_principal = make_principal();
    return s_principal;
}

env_stack_t &env_stack_t::globals() {
    static env_stack_t s_global;
    return s_global;
}

env_vars_snapshot_t::env_vars_snapshot_t(const environment_t &source, const wchar_t *const *keys) {
    ASSERT_IS_MAIN_THREAD();
    wcstring key;
    for (size_t i = 0; keys[i]; i++) {
        key.assign(keys[i]);
        const auto var = source.get(key);
        if (var) {
            vars[key] = std::move(*var);
        }
    }
    names = source.get_names(0);
}

env_vars_snapshot_t::~env_vars_snapshot_t() = default;

maybe_t<env_var_t> env_vars_snapshot_t::get(const wcstring &key, env_mode_flags_t mode) const {
    UNUSED(mode);
    auto iter = vars.find(key);
    if (iter == vars.end()) return none();
    return iter->second;
}

wcstring_list_t env_vars_snapshot_t::get_names(int flags) const {
    UNUSED(flags);
    return names;
}

const wchar_t *const env_vars_snapshot_t::highlighting_keys[] = {
    L"PATH", L"CDPATH", L"fish_function_path", L"PWD", L"HOME", NULL};

const wchar_t *const env_vars_snapshot_t::completing_keys[] = {
    L"PATH", L"CDPATH", L"fish_function_path", L"PWD", L"HOME", NULL};

#if defined(__APPLE__) || defined(__CYGWIN__)
static int check_runtime_path(const char *path) {
    UNUSED(path);
    return 0;
}
#else
/// Check, and create if necessary, a secure runtime path. Derived from tmux.c in tmux
/// (http://tmux.sourceforge.net/).
static int check_runtime_path(const char *path) {
    // Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
    //
    // Permission to use, copy, modify, and distribute this software for any
    // purpose with or without fee is hereby granted, provided that the above
    // copyright notice and this permission notice appear in all copies.
    //
    // THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    // WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    // MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    // ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    // WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
    // IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
    // OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
    struct stat statpath;
    uid_t uid = geteuid();

    if (mkdir(path, S_IRWXU) != 0 && errno != EEXIST) return errno;
    if (lstat(path, &statpath) != 0) return errno;
    if (!S_ISDIR(statpath.st_mode) || statpath.st_uid != uid ||
        (statpath.st_mode & (S_IRWXG | S_IRWXO)) != 0)
        return EACCES;
    return 0;
}
#endif

/// Return the path of an appropriate runtime data directory.
wcstring env_get_runtime_path() {
    wcstring result;
    const char *dir = getenv("XDG_RUNTIME_DIR");

    // Check that the path is actually usable. Technically this is guaranteed by the fdo spec but in
    // practice it is not always the case: see #1828 and #2222.
    int mode = R_OK | W_OK | X_OK;
    if (dir != NULL && access(dir, mode) == 0 && check_runtime_path(dir) == 0) {
        result = str2wcstring(dir);
    } else {
        // Don't rely on $USER being set, as setup_user() has not yet been called.
        // See https://github.com/fish-shell/fish-shell/issues/5180
        // getpeuid() can't fail, but getpwuid sure can.
        auto pwuid = getpwuid(geteuid());
        const char *uname = pwuid ? pwuid->pw_name : NULL;
        // /tmp/fish.user
        std::string tmpdir = "/tmp/fish.";
        if (uname) {
            tmpdir.append(uname);
        }

        if (!uname || check_runtime_path(tmpdir.c_str()) != 0) {
            debug(0, L"Runtime path not available.");
            debug(0, L"Try deleting the directory %s and restarting fish.", tmpdir.c_str());
            return result;
        }

        result = str2wcstring(tmpdir);
    }
    return result;
}
