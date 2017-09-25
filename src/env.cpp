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

#if HAVE_NCURSES_H
#include <ncurses.h>
#elif HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif
#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif

#include <algorithm>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "builtin_bind.h"
#include "common.h"
#include "env.h"
#include "env_universal_common.h"
#include "event.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fish_version.h"
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
#define FISH_DATADIR_VAR L"__fish_datadir"
#define FISH_SYSCONFDIR_VAR L"__fish_sysconfdir"
#define FISH_HELPDIR_VAR L"__fish_help_dir"
#define FISH_BIN_DIR L"__fish_bin_dir"

/// At init, we read all the environment variables from this array.
extern char **environ;

// Limit `read` to 10 MiB (bytes not wide chars) by default. This can be overridden by the
// FISH_READ_BYTE_LIMIT variable.
#define READ_BYTE_LIMIT 10 * 1024 * 1024
size_t read_byte_limit = READ_BYTE_LIMIT;

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

typedef std::unordered_map<const wcstring, void (*)(const wcstring &, const wcstring &)> var_dispatch_table_t;
var_dispatch_table_t var_dispatch_table;

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

/// List of "path" like variable names that need special handling. This includes automatic
/// splitting and joining on import/export. As well as replacing empty elements, which implicitly
/// refer to the CWD, with an explicit '.' in the case of PATH and CDPATH.
static const wcstring_list_t colon_delimited_variable({L"PATH", L"MANPATH", L"CDPATH"});

// Some forward declarations to make it easy to logically group the code.
static void init_locale();
static void init_curses();

/// This is used to convert a serialized env_var_t back into a list. It is used when reading legacy
/// (fish 2.x) encoded vars stored in the universal variable file and the environment.
static void tokenize_variable_array(const wcstring &val, wcstring_list_t &out) {
    out.clear();  // ensure the output var is empty -- this will normally be a no-op

    // Zero element arrays are externally encoded as this placeholder string.
    if (val == ENV_NULL) return;

    size_t pos = 0, end = val.size();
    while (pos <= end) {
        size_t next_pos = val.find(ARRAY_SEP, pos);
        if (next_pos == wcstring::npos) {
            next_pos = end;
        }
        out.resize(out.size() + 1);
        out.back().assign(val, pos, next_pos - pos);
        pos = next_pos + 1;  // skip the separator, or skip past the end
    }
}

/// This is used to convert a serialized env_var_t back into a list.
wcstring_list_t decode_serialized(const wcstring &s) {
    wcstring_list_t values;
    tokenize_variable_array(s, values);
    return values;
}

// Struct representing one level in the function variable stack.
// Only our variable stack should create and destroy these
class env_node_t {
    friend struct var_stack_t;
    env_node_t(bool is_new_scope) : new_scope(is_new_scope) {}

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
    std::unique_ptr<env_node_t> next;

    maybe_t<env_var_t> find_entry(const wcstring &key);
};

class variable_entry_t {
    wcstring value; /**< Value of the variable */
};

static std::mutex env_lock;

static bool local_scope_exports(const env_node_t *n);

// A class wrapping up a variable stack
// Currently there is only one variable stack in fish,
// but we can imagine having separate (linked) stacks
// if we introduce multiple threads of execution
struct var_stack_t {
    // Top node on the function stack.
    std::unique_ptr<env_node_t> top = NULL;

    // Bottom node on the function stack
    // This is an observer pointer
    env_node_t *global_env = NULL;

    // Exported variable array used by execv.
    null_terminated_array_t<char> export_array;

    /// Flag for checking if we need to regenerate the exported variable array.
    bool has_changed_exported = true;
    void mark_changed_exported() { has_changed_exported = true; }
    void update_export_array_if_necessary();

    var_stack_t() : top(new env_node_t(false)) { this->global_env = this->top.get(); }

    // Pushes a new node onto our stack
    // Optionally creates a new scope for the node
    void push(bool new_scope);

    // Pops the top node if it's not global
    void pop();

    bool var_changed(const wcstring_list_t &vars);

    // Returns the next scope to search for a given node, respecting the new_scope lag
    // Returns NULL if we're done
    env_node_t *next_scope_to_search(env_node_t *node);
    const env_node_t *next_scope_to_search(const env_node_t *node) const;

    // Returns the scope used for unspecified scopes. An unspecified scope is either the topmost
    // shadowing scope, or the global scope if none. This implements the default behavior of `set`.
    env_node_t *resolve_unspecified_scope();
};

void var_stack_t::push(bool new_scope) {
    std::unique_ptr<env_node_t> node(new env_node_t(new_scope));

    // Copy local-exported variables.
    auto top_node = top.get();
    // Only if we introduce a new shadowing scope; i.e. not if it's just `begin; end` or
    // "--no-scope-shadowing".
    if (new_scope && top_node != this->global_env) {
        for (auto &var : top_node->env) {
            if (var.second.exportv) node->env.insert(var);
        }
    }

    node->next = std::move(this->top);
    this->top = std::move(node);
    if (new_scope && local_scope_exports(this->top.get())) {
        this->mark_changed_exported();
    }
}

/// Return true if one of the vars in the passed list was changed in the current var scope.
bool var_stack_t::var_changed(const wcstring_list_t &vars) {
    for (auto v : vars) {
        if (top->env.find(v) != top->env.end()) return true;
    }
    return false;
}

void var_stack_t::pop() {
    // Don't pop the top-most, global, level.
    if (top.get() == this->global_env) {
        debug(0, _(L"Tried to pop empty environment stack."));
        sanity_lose();
        return;
    }

    bool locale_changed = this->var_changed(locale_variables);
    bool curses_changed = this->var_changed(curses_variables);

    if (top->new_scope) {  //!OCLINT(collapsible if statements)
        if (top->exportv || local_scope_exports(top->next.get())) {
            this->mark_changed_exported();
        }
    }

    // Actually do the pop! Move the top pointer into a local variable, then replace the top pointer
    // with the next pointer afterwards we should have a node with no next pointer, and our top
    // should be non-null.
    std::unique_ptr<env_node_t> old_top = std::move(this->top);
    this->top = std::move(old_top->next);
    old_top->next.reset();
    assert(this->top && old_top && !old_top->next);
    assert(this->top != NULL);

    for (const auto &entry_pair : old_top->env) {
        const env_var_t &var = entry_pair.second;
        if (var.exportv) {
            this->mark_changed_exported();
            break;
        }
    }

    if (locale_changed) init_locale();
    if (curses_changed) init_curses();
}

const env_node_t *var_stack_t::next_scope_to_search(const env_node_t *node) const {
    assert(node != NULL);
    if (node == this->global_env) {
        return NULL;
    }
    return node->new_scope ? this->global_env : node->next.get();
}

env_node_t *var_stack_t::next_scope_to_search(env_node_t *node) {
    assert(node != NULL);
    if (node == this->global_env) {
        return NULL;
    }
    return node->new_scope ? this->global_env : node->next.get();
}

env_node_t *var_stack_t::resolve_unspecified_scope() {
    env_node_t *node = this->top.get();
    while (node && !node->new_scope) {
        node = node->next.get();
    }
    return node ? node : this->global_env;
}

// Get the global variable stack
static var_stack_t &vars_stack() {
    static var_stack_t global_stack;
    return global_stack;
}

/// Universal variables global instance. Initialized in env_init.
static env_universal_t *s_universal_variables = NULL;

/// Getter for universal variables.
static env_universal_t *uvars() { return s_universal_variables; }

// Helper class for storing constant strings, without needing to wrap them in a wcstring.

// Comparer for const string set.
typedef std::unordered_set<wcstring> const_string_set_t;

/// Table of variables that may not be set using the set command.
static const_string_set_t env_read_only;

static bool is_read_only(const wcstring &key) {
    return env_read_only.find(key) != env_read_only.end();
}

bool env_var_t::read_only() const { return is_read_only(name); }

/// Table of variables whose value is dynamically calculated, such as umask, status, etc.
static const_string_set_t env_electric;

static bool is_electric(const wcstring &key) {
    return env_electric.find(key) != env_electric.end();
}

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
static void handle_timezone(const wchar_t *env_var_name) {
    // const env_var_t var = env_get(env_var_name, ENV_EXPORT);
    const auto var = env_get(env_var_name, ENV_DEFAULT);
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
static void fix_colon_delimited_var(const wcstring &var_name) {
    // While we auto split/join MANPATH we do not want to replace empty elements with "." (#4158).
    if (var_name == L"MANPATH") return;

    const auto paths = env_get(var_name);
    if (paths.missing_or_empty()) return;

    bool modified = false;
    wcstring_list_t pathsv;
    wcstring_list_t new_pathsv;
    paths->to_list(pathsv);
    for (auto next_path : pathsv) {
        if (next_path.empty()) {
            next_path = L".";
            modified = true;
        }
        new_pathsv.push_back(next_path);
    }

    if (!modified) {
        return;
    }

    int retval = env_set(var_name, ENV_DEFAULT | ENV_USER, new_pathsv);
    if (retval != ENV_OK) {
        debug(0, L"fix_colon_delimited_var failed unexpectedly with retval %d", retval);
    }
}

/// Initialize the locale subsystem.
static void init_locale() {
    // We have to make a copy because the subsequent setlocale() call to change the locale will
    // invalidate the pointer from the this setlocale() call.
    char *old_msg_locale = strdup(setlocale(LC_MESSAGES, NULL));

    for (auto var_name : locale_variables) {
        const auto var = env_get(var_name, ENV_EXPORT);
        const std::string &name = wcs2string(var_name);
        if (var.missing_or_empty()) {
            debug(2, L"locale var %s missing or empty", name.c_str());
            unsetenv(name.c_str());
        } else {
            const std::string value = wcs2string(var->as_string());
            debug(2, L"locale var %s='%s'", name.c_str(), value.c_str());
            setenv(name.c_str(), value.c_str(), 1);
        }
    }

    char *locale = setlocale(LC_ALL, "");
    fish_setlocale();
    debug(2, L"init_locale() setlocale(): '%s'", locale);

    const char *new_msg_locale = setlocale(LC_MESSAGES, NULL);
    debug(3, L"old LC_MESSAGES locale: '%s'", old_msg_locale);
    debug(3, L"new LC_MESSAGES locale: '%s'", new_msg_locale);
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
static const wcstring_list_t title_terms({L"xterm", L"screen", L"tmux", L"nxterm", L"rxvt"});
static bool does_term_support_setting_title() {
    const auto term_var = env_get(L"TERM");
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

        char buf[PATH_MAX];
        int retval = ttyname_r(STDIN_FILENO, buf, PATH_MAX);
        if (retval != 0 || strstr(buf, "tty") || strstr(buf, "/vc/")) return false;
    }

    return true;
}

/// Updates our idea of whether we support term256 and term24bit (see issue #10222).
static void update_fish_color_support() {
    // Detect or infer term256 support. If fish_term256 is set, we respect it;
    // otherwise infer it from the TERM variable or use terminfo.
    auto fish_term256 = env_get(L"fish_term256");
    auto term_var = env_get(L"TERM");
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
        const auto prog_var = env_get(L"TERM_PROGRAM");
        const auto progver_var = env_get(L"TERM_PROGRAM_VERSION");
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

    auto fish_term24bit = env_get(L"fish_term24bit");
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
    auto term_var = env_get(L"TERM");
    if (term_var.missing_or_empty()) return false;

    const char *term_env = wcs2str(term_var->as_string());
    if (!strcmp(term_env, DEFAULT_TERM1) || !strcmp(term_env, DEFAULT_TERM2)) return false;

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
    for (auto var_name : colon_delimited_variable) {
        fix_colon_delimited_var(var_name);
    }
}

/// Initialize the curses subsystem.
static void init_curses() {
    for (auto var_name : curses_variables) {
        const auto var = env_get(var_name, ENV_EXPORT);
        const std::string &name = wcs2string(var_name);
        if (var.missing_or_empty()) {
            debug(2, L"curses var %s missing or empty", name.c_str());
            unsetenv(name.c_str());
        } else {
            const std::string &value = wcs2string(var->as_string());
            debug(2, L"curses var %s='%s'", name.c_str(), value.c_str());
            setenv(name.c_str(), value.c_str(), 1);
        }
    }

    int err_ret;
    if (setupterm(NULL, STDOUT_FILENO, &err_ret) == ERR) {
        auto term = env_get(L"TERM");
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

    can_set_term_title = does_term_support_setting_title();
    term_has_xn = tigetflag((char *)"xenl") == 1;  // does terminal have the eat_newline_glitch
    update_fish_color_support();
    // Invalidate the cached escape sequences since they may no longer be valid.
    cached_esc_sequences.clear();
    curses_initialized = true;
}

// Here is the whitelist of variables that we colon-delimit, both incoming from the environment and
// outgoing back to it. This is deliberately very short - we don't want to add language-specific
// values like CLASSPATH.
static bool variable_is_colon_delimited_var(const wcstring &str) {
    return contains(colon_delimited_variable, str);
}

/// React to modifying the given variable.
static void react_to_variable_change(const wchar_t *op, const wcstring &key) {
    // Don't do any of this until `env_init()` has run. We only want to do this in response to
    // variables set by the user; e.g., in a script like *config.fish* or interactively or as part
    // of loading the universal variables for the first time. Variables we import from the
    // environment or that are otherwise set by fish before this gets called have to explicitly
    // call the appropriate functions to put the value of the var into effect.
    if (!env_initialized) return;

    auto dispatch = var_dispatch_table.find(key);
    if (dispatch != var_dispatch_table.end()) {
        (*dispatch->second)(op, key);
    } else if (string_prefixes_string(L"_fish_abbr_", key)) {
        update_abbr_cache(op, key);
    } else if (string_prefixes_string(L"fish_color_", key)) {
        reader_react_to_color_change();
    }
}

/// Universal variable callback function. This function makes sure the proper events are triggered
/// when an event occurs.
static void universal_callback(fish_message_type_t type, const wchar_t *name) {
    const wchar_t *op;

    switch (type) {
        case SET:
        case SET_EXPORT: {
            op = L"SET";
            break;
        }
        case ERASE: {
            op = L"ERASE";
            break;
        }
    }

    react_to_variable_change(op, name);
    vars_stack().mark_changed_exported();

    event_t ev = event_t::variable_event(name);
    ev.arguments.push_back(L"VARIABLE");
    ev.arguments.push_back(op);
    ev.arguments.push_back(name);
    event_fire(&ev);
}

/// Make sure the PATH variable contains something.
static void setup_path() {
    const auto path = env_get(L"PATH");
    if (path.missing_or_empty()) {
        wcstring_list_t value({L"/usr/bin", L"/bin"});
        env_set(L"PATH", ENV_GLOBAL | ENV_EXPORT, value);
    }
}

/// If they don't already exist initialize the `COLUMNS` and `LINES` env vars to reasonable
/// defaults. They will be updated later by the `get_current_winsize()` function if they need to be
/// adjusted.
static void env_set_termsize() {
    auto cols = env_get(L"COLUMNS");
    if (cols.missing_or_empty()) env_set_one(L"COLUMNS", ENV_GLOBAL, DFLT_TERM_COL_STR);

    auto rows = env_get(L"LINES");
    if (rows.missing_or_empty()) env_set_one(L"LINES", ENV_GLOBAL, DFLT_TERM_ROW_STR);
}

bool env_set_pwd() {
    wcstring cwd = wgetcwd();
    if (cwd.empty()) {
        debug(0,
              _(L"Could not determine current working directory. Is your locale set correctly?"));
        return false;
    }
    env_set_one(L"PWD", ENV_EXPORT | ENV_GLOBAL, cwd);
    return true;
}

/// Allow the user to override the limit on how much data the `read` command will process.
/// This is primarily for testing but could be used by users in special situations.
void env_set_read_limit() {
    auto read_byte_limit_var = env_get(L"FISH_READ_BYTE_LIMIT");
    if (!read_byte_limit_var.missing_or_empty()) {
        size_t limit = fish_wcstoull(read_byte_limit_var->as_string().c_str());
        if (errno) {
            debug(1, "Ignoring FISH_READ_BYTE_LIMIT since it is not valid");
        } else {
            read_byte_limit = limit;
        }
    }
}

wcstring env_get_pwd_slash(void) {
    auto pwd_var = env_get(L"PWD");
    if (pwd_var.missing_or_empty()) {
        return L"";
    }
    wcstring pwd = pwd_var->as_string();
    if (!string_suffixes_string(L"/", pwd)) {
        pwd.push_back(L'/');
    }
    return pwd;
}

/// Set up the USER variable.
static void setup_user(bool force) {
    if (force || env_get(L"USER").missing_or_empty()) {
        struct passwd userinfo;
        struct passwd *result;
        char buf[8192];
        int retval = getpwuid_r(getuid(), &userinfo, buf, sizeof(buf), &result);
        if (!retval && result) {
            const wcstring uname = str2wcstring(userinfo.pw_name);
            env_set_one(L"USER", ENV_GLOBAL | ENV_EXPORT, uname);
        }
    }
}

/// Various things we need to initialize at run-time that don't really fit any of the other init
/// routines.
void misc_init() {
    env_set_read_limit();

    // If stdout is open on a tty ensure stdio is unbuffered. That's because those functions might
    // be intermixed with `write()` calls and we need to ensure the writes are not reordered. See
    // issue #3748.
    if (isatty(STDOUT_FILENO)) {
        fflush(stdout);
        setvbuf(stdout, NULL, _IONBF, 0);
    }

#ifdef OS_IS_CYGWIN
    // MS Windows tty devices do not currently have either a read or write timestamp. Those
    // respective fields of `struct stat` are always the current time. Which means we can't
    // use them. So we assume no external program has written to the terminal behind our
    // back. This makes multiline promptusable. See issue #2859 and
    // https://github.com/Microsoft/BashOnWindows/issues/545
    has_working_tty_timestamps = false;
#else
    // This covers preview builds of Windows Subsystem for Linux (WSL).
    FILE *procsyskosrel;
    if ((procsyskosrel = wfopen(L"/proc/sys/kernel/osrelease", "r"))) {
        wcstring osrelease;
        fgetws2(&osrelease, procsyskosrel);
        if (osrelease.find(L"3.4.0-Microsoft") != wcstring::npos) {
            has_working_tty_timestamps = false;
        }
    }
    if (procsyskosrel) {
        fclose(procsyskosrel);
    }
#endif  // OS_IS_MS_WINDOWS
}

static void env_universal_callbacks(callback_data_list_t &callbacks) {
    for (size_t i = 0; i < callbacks.size(); i++) {
        const callback_data_t &data = callbacks.at(i);
        universal_callback(data.type, data.key.c_str());
    }
}

void env_universal_barrier() {
    ASSERT_IS_MAIN_THREAD();
    if (!uvars()) return;

    callback_data_list_t callbacks;
    bool changed = uvars()->sync(callbacks);
    if (changed) {
        universal_notifier_t::default_notifier().post_notification();
    }

    env_universal_callbacks(callbacks);
}

static void handle_fish_term_change(const wcstring &op, const wcstring &var_name) {
    UNUSED(op);
    UNUSED(var_name);
    update_fish_color_support();
    reader_react_to_color_change();
}

static void handle_escape_delay_change(const wcstring &op, const wcstring &var_name) {
    UNUSED(op);
    UNUSED(var_name);
    update_wait_on_escape_ms();
}

static void handle_term_size_change(const wcstring &op, const wcstring &var_name) {
    UNUSED(op);
    UNUSED(var_name);
    invalidate_termsize(true);  // force fish to update its idea of the terminal size plus vars
}

static void handle_read_limit_change(const wcstring &op, const wcstring &var_name) {
    UNUSED(op);
    UNUSED(var_name);
    env_set_read_limit();
}

static void handle_fish_history_change(const wcstring &op, const wcstring &var_name) {
    UNUSED(op);
    UNUSED(var_name);
    reader_change_history(history_session_id().c_str());
}

static void handle_tz_change(const wcstring &op, const wcstring &var_name) {
    UNUSED(op);
    handle_timezone(var_name.c_str());
}

static void handle_magic_colon_var_change(const wcstring &op, const wcstring &var_name) {
    UNUSED(op);
    fix_colon_delimited_var(var_name);
}

static void handle_locale_change(const wcstring &op, const wcstring &var_name) {
    UNUSED(op);
    UNUSED(var_name);
    init_locale();
}

static void handle_curses_change(const wcstring &op, const wcstring &var_name) {
    UNUSED(op);
    UNUSED(var_name);
    init_curses();
}

/// Populate the dispatch table used by `react_to_variable_change()` to efficiently call the
/// appropriate function to handle a change to a variable.
static void setup_var_dispatch_table() {
    for (auto var_name : locale_variables) {
        var_dispatch_table.emplace(var_name, handle_locale_change);
    }

    for (auto var_name : curses_variables) {
        var_dispatch_table.emplace(var_name, handle_curses_change);
    }

    for (auto var_name : colon_delimited_variable) {
        var_dispatch_table.emplace(var_name, handle_magic_colon_var_change);
    }

    var_dispatch_table.emplace(L"fish_term256", handle_fish_term_change);
    var_dispatch_table.emplace(L"fish_term24bit", handle_fish_term_change);
    var_dispatch_table.emplace(L"fish_escape_delay_ms", handle_escape_delay_change);
    var_dispatch_table.emplace(L"LINES", handle_term_size_change);
    var_dispatch_table.emplace(L"COLUMNS", handle_term_size_change);
    var_dispatch_table.emplace(L"FISH_READ_BYTE_LIMIT", handle_read_limit_change);
    var_dispatch_table.emplace(L"fish_history", handle_fish_history_change);
    var_dispatch_table.emplace(L"TZ", handle_tz_change);
}

void env_init(const struct config_paths_t *paths /* or NULL */) {
    setup_var_dispatch_table();

    // These variables can not be altered directly by the user.
    for (auto &k : {
             wcstring(L"status"),
             wcstring(L"history"),
             wcstring(L"_"),
             wcstring(L"PWD"),
             wcstring(L"FISH_VERSION") }) {
        env_read_only.emplace(std::move(k));
        // L"SHLVL" is readonly but will be inserted below after we increment it.
    };

    // Names of all dynamically calculated variables.
    env_electric.emplace(L"history");
    env_electric.emplace(L"status");
    env_electric.emplace(L"umask");

    // Now the environment variable handling is set up, the next step is to insert valid data.

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
            env_set_empty(key_and_val, ENV_EXPORT | ENV_GLOBAL);
        } else {
            key.assign(key_and_val, 0, eql);
            if (is_read_only(key) || is_electric(key)) continue;
            val.assign(key_and_val, eql + 1, wcstring::npos);
            if (variable_is_colon_delimited_var(key)) {
                std::replace(val.begin(), val.end(), L':', ARRAY_SEP);
                wcstring_list_t values = decode_serialized(val);
                env_set(key, ENV_EXPORT | ENV_GLOBAL, values);
            } else {
                wcstring_list_t values = decode_serialized(val);
                env_set(key, ENV_EXPORT | ENV_GLOBAL, values);
            }
        }
    }

    // Set the given paths in the environment, if we have any.
    if (paths != NULL) {
        env_set_one(FISH_DATADIR_VAR, ENV_GLOBAL, paths->data);
        env_set_one(FISH_SYSCONFDIR_VAR, ENV_GLOBAL, paths->sysconf);
        env_set_one(FISH_HELPDIR_VAR, ENV_GLOBAL, paths->doc);
        env_set_one(FISH_BIN_DIR, ENV_GLOBAL, paths->bin);
    }

    init_locale();
    init_curses();
    init_input();
    init_path_vars();

    // Set up the USER and PATH variables
    setup_path();

    // Some `su`s keep $USER when changing to root.
    // This leads to issues later on (and e.g. in prompts),
    // so we work around it by resetting $USER.
    // TODO: Figure out if that su actually checks if username == "root"(as the man page says) or
    // UID == 0.
    uid_t uid = getuid();
    setup_user(uid == 0);

    // Set up the version variable.
    wcstring version = str2wcstring(get_fish_version());
    env_set_one(L"FISH_VERSION", ENV_GLOBAL, version);

    // Set up SHLVL variable.
    const auto shlvl_var = env_get(L"SHLVL");
    wcstring nshlvl_str = L"1";
    if (!shlvl_var.missing_or_empty()) {
        const wchar_t *end;
        // TODO: Figure out how to handle invalid numbers better. Shouldn't we issue a diagnostic?
        long shlvl_i = fish_wcstol(shlvl_var->as_string().c_str(), &end);
        if (!errno && shlvl_i >= 0) {
            nshlvl_str = to_string<long>(shlvl_i + 1);
        }
    }
    env_set_one(L"SHLVL", ENV_GLOBAL | ENV_EXPORT, nshlvl_str);
    env_read_only.emplace(L"SHLVL");

    // Set up the HOME variable.
    // Unlike $USER, it doesn't seem that `su`s pass this along
    // if the target user is root, unless "--preserve-environment" is used.
    // Since that is an explicit choice, we should allow it to enable e.g.
    //     env HOME=(mktemp -d) su --preserve-environment fish
    if (env_get(L"HOME").missing_or_empty()) {
        auto user_var = env_get(L"USER");
        if (!user_var.missing_or_empty()) {
            char *unam_narrow = wcs2str(user_var->as_string());
            struct passwd userinfo;
            struct passwd *result;
            char buf[8192];
            int retval = getpwnam_r(unam_narrow, &userinfo, buf, sizeof(buf), &result);
            if (retval || !result) {
                // Maybe USER is set but it's bogus. Reset USER from the db and try again.
                setup_user(true);
                user_var = env_get(L"USER");
                if (!user_var.missing_or_empty()) {
                    unam_narrow = wcs2str(user_var->as_string());
                    retval = getpwnam_r(unam_narrow, &userinfo, buf, sizeof(buf), &result);
                }
            }
            if (!retval && result && userinfo.pw_dir) {
                const wcstring dir = str2wcstring(userinfo.pw_dir);
                env_set_one(L"HOME", ENV_GLOBAL | ENV_EXPORT, dir);
            } else {
                // We cannot get $HOME. This triggers warnings for history and config.fish already,
                // so it isn't necessary to warn here as well.
                env_set_empty(L"HOME", ENV_GLOBAL | ENV_EXPORT);
            }
            free(unam_narrow);
        } else {
            // If $USER is empty as well (which we tried to set above), we can't get $HOME.
            env_set_empty(L"HOME", ENV_GLOBAL | ENV_EXPORT);
        }
    }

    env_set_pwd();         // initialize the PWD variable
    env_set_termsize();    // initialize the terminal size variables
    env_set_read_limit();  // initialize the read_byte_limit

    // Set g_use_posix_spawn. Default to true.
    auto use_posix_spawn = env_get(L"fish_use_posix_spawn");
    g_use_posix_spawn =
        use_posix_spawn.missing_or_empty() ? true : from_string<bool>(use_posix_spawn->as_string());

    // Set fish_bind_mode to "default".
    env_set_one(FISH_BIND_MODE_VAR, ENV_GLOBAL, DEFAULT_BIND_MODE);

    // This is somewhat subtle. At this point we consider our environment to be sufficiently
    // initialized that we can react to changes to variables. Prior to doing this we expect that the
    // code for setting vars that might have side-effects will do whatever
    // `react_to_variable_change()` would do for that var.
    env_initialized = true;

    // Set up universal variables. The empty string means to use the default path.
    assert(s_universal_variables == NULL);
    s_universal_variables = new env_universal_t(L"");
    callback_data_list_t callbacks;
    s_universal_variables->load(callbacks);
    env_universal_callbacks(callbacks);

    // Now that the global scope is fully initialized, add a toplevel local scope. This same local
    // scope will persist throughout the lifetime of the fish process, and it will ensure that `set
    // -l` commands run at the command-line don't affect the global scope.
    env_push(false);
}

/// Search all visible scopes in order for the specified key. Return the first scope in which it was
/// found.
static env_node_t *env_get_node(const wcstring &key) {
    env_node_t *env = vars_stack().top.get();
    while (env != NULL) {
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
    // Do not actually create a umask variable. On env_get() it will be calculated.
    umask(mask);
    return ENV_OK;
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
static int env_set_internal(const wcstring &key, env_mode_flags_t var_mode, wcstring_list_t val) {
    ASSERT_IS_MAIN_THREAD();
    bool has_changed_old = vars_stack().has_changed_exported;
    int done = 0;

    if (val.size() == 1 && (key == L"PWD" || key == L"HOME")) {
        // Canonicalize our path; if it changes, recurse and try again.
        wcstring val_canonical = val.front();
        path_make_canonical(val_canonical);
        if (val.front() != val_canonical) {
            return env_set_internal(key, var_mode, {val_canonical});
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
        const bool old_export = uvars() && uvars()->get_export(key);
        bool new_export;
        if (var_mode & ENV_EXPORT) {
            // Export the var.
            new_export = true;
        } else if (var_mode & ENV_UNEXPORT) {
            // Unexport the var.
            new_export = false;
        } else {
            // Not changing the export status of the var.
            new_export = old_export;
        }
        if (uvars()) {
            uvars()->set(key, val, new_export);
            env_universal_barrier();
            if (old_export || new_export) {
                vars_stack().mark_changed_exported();
            }
        }
    } else {
        // Determine the node.
        bool has_changed_new = false;
        env_node_t *preexisting_node = env_get_node(key);
        bool preexisting_entry_exportv = false;
        if (preexisting_node != NULL) {
            var_table_t::const_iterator result = preexisting_node->env.find(key);
            assert(result != preexisting_node->env.end());
            const env_var_t &var = result->second;
            if (var.exportv) {
                preexisting_entry_exportv = true;
                has_changed_new = true;
            }
        }

        env_node_t *node = NULL;
        if (var_mode & ENV_GLOBAL) {
            node = vars_stack().global_env;
        } else if (var_mode & ENV_LOCAL) {
            node = vars_stack().top.get();
        } else if (preexisting_node != NULL) {
            node = preexisting_node;
            if ((var_mode & (ENV_EXPORT | ENV_UNEXPORT)) == 0) {
                // Use existing entry's exportv status.
                var_mode =  //!OCLINT(parameter reassignment)
                    preexisting_entry_exportv ? ENV_EXPORT : 0;
            }
        } else {
            if (!get_proc_had_barrier()) {
                set_proc_had_barrier(true);
                env_universal_barrier();
            }

            if (uvars() && uvars()->get(key)) {
                bool exportv;
                if (var_mode & ENV_EXPORT) {
                    exportv = true;
                } else if (var_mode & ENV_UNEXPORT) {
                    exportv = false;
                } else {
                    exportv = uvars()->get_export(key);
                }
                uvars()->set(key, val, exportv);
                env_universal_barrier();
                done = 1;

            } else {
                // New variable with unspecified scope
                node = vars_stack().resolve_unspecified_scope();
            }
        }

        if (!done) {
            // Set the entry in the node. Note that operator[] accesses the existing entry, or
            // creates a new one.
            env_var_t &var = node->env[key];
            if (var.exportv) {
                // This variable already existed, and was exported.
                has_changed_new = true;
            }

            var.set_vals(std::move(val));

            if (var_mode & ENV_EXPORT) {
                // The new variable is exported.
                var.exportv = true;
                node->exportv = true;
                has_changed_new = true;
            } else {
                var.exportv = false;
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

    // debug(1, L"env_set: fire events on variable |%ls|", key);
    event_fire(&ev);
    // debug(1, L"env_set: return from event firing");

    react_to_variable_change(L"SET", key);
    return ENV_OK;
}

/// Sets the variable with the specified name to the given values.
int env_set(const wcstring &key, env_mode_flags_t mode, wcstring_list_t vals) {
    return env_set_internal(key, mode, std::move(vals));
}

/// Sets the variable with the specified name to a single value.
int env_set_one(const wcstring &key, env_mode_flags_t mode, wcstring val) {
    wcstring_list_t vals;
    vals.push_back(std::move(val));
    return env_set_internal(key, mode, std::move(vals));
}

/// Sets the variable with the specified name without any (i.e., zero) values.
int env_set_empty(const wcstring &key, env_mode_flags_t mode) {
    return env_set_internal(key, mode, {});
}

/// Attempt to remove/free the specified key/value pair from the specified map.
///
/// \return zero if the variable was not found, non-zero otherwise
static bool try_remove(env_node_t *n, const wchar_t *key, int var_mode) {
    if (n == NULL) {
        return false;
    }

    var_table_t::iterator result = n->env.find(key);
    if (result != n->env.end()) {
        if (result->second.exportv) {
            vars_stack().mark_changed_exported();
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
    return try_remove(n->next.get(), key, var_mode);
}

int env_remove(const wcstring &key, int var_mode) {
    ASSERT_IS_MAIN_THREAD();
    env_node_t *first_node;
    int erased = 0;

    if ((var_mode & ENV_USER) && is_read_only(key)) {
        return 2;
    }

    first_node = vars_stack().top.get();

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
        bool is_exported = uvars()->get_export(key);
        erased = uvars() && uvars()->remove(key);
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

    react_to_variable_change(L"ERASE", key);

    return !erased;
}

const wcstring_list_t &env_var_t::as_list() const { return vals; }

/// Return a string representation of the var. At the present time this uses the legacy 2.x
/// encoding.
wcstring env_var_t::as_string(void) const {
    if (this->vals.empty()) return wcstring(ENV_NULL);

    wchar_t sep = variable_is_colon_delimited_var(this->name) ? L':' : ARRAY_SEP;
    auto it = this->vals.cbegin();
    wcstring result(*it);
    while (++it != vals.end()) {
        result.push_back(sep);
        result.append(*it);
    }
    return result;
}

void env_var_t::to_list(wcstring_list_t &out) const {
    out = vals;
}

maybe_t<env_var_t> env_get(const wcstring &key, env_mode_flags_t mode) {
    const bool has_scope = mode & (ENV_LOCAL | ENV_GLOBAL | ENV_UNIVERSAL);
    const bool search_local = !has_scope || (mode & ENV_LOCAL);
    const bool search_global = !has_scope || (mode & ENV_GLOBAL);
    const bool search_universal = !has_scope || (mode & ENV_UNIVERSAL);

    const bool search_exported = (mode & ENV_EXPORT) || !(mode & ENV_UNEXPORT);
    const bool search_unexported = (mode & ENV_UNEXPORT) || !(mode & ENV_EXPORT);

    // Make the assumption that electric keys can't be shadowed elsewhere, since we currently block
    // that in env_set().
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
                history = &history_t::history_with_name(history_session_id());
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
        env_node_t *env = search_local ? vars_stack().top.get() : vars_stack().global_env;

        while (env != NULL) {
            if (env == vars_stack().global_env && !search_global) {
                break;
            }

            var_table_t::const_iterator result = env->env.find(key);
            if (result != env->env.end()) {
                const env_var_t var = result->second;
                if (var.exportv ? search_exported : search_unexported) {
                    return var;
                }
            }
            env = vars_stack().next_scope_to_search(env);
        }
    }

    if (!search_universal) return none();

    // Another hack. Only do a universal barrier on the main thread (since it can change variable
    // values). Make sure we do this outside the env_lock because it may itself call `env_get()`.
    if (is_main_thread() && !get_proc_had_barrier()) {
        set_proc_had_barrier(true);
        env_universal_barrier();
    }

    // Okay, we couldn't find a local or global var given the requirements. If there is a matching
    // universal var return that.
    if (uvars()) {
        auto var = uvars()->get(key);
        if (var && (uvars()->get_export(key) ? search_exported : search_unexported)) {
            return var;
        }
    }

    return none();
}

/// Returns true if the specified scope or any non-shadowed non-global subscopes contain an exported
/// variable.
static bool local_scope_exports(const env_node_t *n) {
    assert(n != NULL);
    if (n == vars_stack().global_env) return false;

    if (n->exportv) return true;

    if (n->new_scope) return false;

    return local_scope_exports(n->next.get());
}

void env_push(bool new_scope) { vars_stack().push(new_scope); }

void env_pop() { vars_stack().pop(); }

/// Function used with to insert keys of one table into a set::set<wcstring>.
static void add_key_to_string_set(const var_table_t &envs, std::set<wcstring> *str_set,
                                  bool show_exported, bool show_unexported) {
    var_table_t::const_iterator iter;
    for (iter = envs.begin(); iter != envs.end(); ++iter) {
        const env_var_t &var = iter->second;

        if ((var.exportv && show_exported) || (!var.exportv && show_unexported)) {
            // Insert this key.
            str_set->insert(iter->first);
        }
    }
}

wcstring_list_t env_get_names(int flags) {
    scoped_lock locker(env_lock);

    wcstring_list_t result;
    std::set<wcstring> names;
    int show_local = flags & ENV_LOCAL;
    int show_global = flags & ENV_GLOBAL;
    int show_universal = flags & ENV_UNIVERSAL;

    const env_node_t *n = vars_stack().top.get();
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
                n = n->next.get();
        }
    }

    if (show_global) {
        add_key_to_string_set(vars_stack().global_env->env, &names, show_exported, show_unexported);
        if (show_unexported) {
            result.insert(result.end(), env_electric.begin(), env_electric.end());
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
static void get_exported(const env_node_t *n, var_table_t &h) {
    if (!n) return;

    if (n->new_scope) {
        get_exported(vars_stack().global_env, h);
    } else {
        get_exported(n->next.get(), h);
    }

    var_table_t::const_iterator iter;
    for (iter = n->env.begin(); iter != n->env.end(); ++iter) {
        const wcstring &key = iter->first;
        const env_var_t var = iter->second;

        if (var.exportv) {
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

// Given a map from key to value, add values to out of the form key=value.
static void export_func(const var_table_t &envs, std::vector<std::string> &out) {
    out.reserve(out.size() + envs.size());
    for (auto iter = envs.begin(); iter != envs.end(); ++iter) {
        const wcstring &key = iter->first;
        const std::string &ks = wcs2string(key);
        std::string vs = wcs2string(iter->second.as_string());

        // Arrays in the value are ASCII record separator (0x1e) delimited. But some variables
        // should have colons. Add those.
        if (variable_is_colon_delimited_var(key)) {
            // Replace ARRAY_SEP with colon.
            std::replace(vs.begin(), vs.end(), (char)ARRAY_SEP, ':');
        }

        // Put a string on the vector.
        out.push_back(std::string());
        std::string &str = out.back();
        str.reserve(ks.size() + 1 + vs.size());

        // Append our environment variable data to it.
        str.append(ks);
        str.append("=");
        str.append(vs);
    }
}

void var_stack_t::update_export_array_if_necessary() {
    if (!this->has_changed_exported) {
        return;
    }

    debug(4, L"env_export_arr() recalc");
    var_table_t vals;
    get_exported(this->top.get(), vals);

    if (uvars()) {
        const wcstring_list_t uni = uvars()->get_names(true, false);
        for (size_t i = 0; i < uni.size(); i++) {
            const wcstring &key = uni.at(i);
            auto var = uvars()->get(key);

            if (!var.missing_or_empty()) {
                // Note that std::map::insert does NOT overwrite a value already in the map,
                // which we depend on here.
                vals.insert(std::pair<wcstring, env_var_t>(key, *var));
            }
        }
    }

    std::vector<std::string> local_export_buffer;
    export_func(vals, local_export_buffer);
    export_array.set(local_export_buffer);
    has_changed_exported = false;
}

const char *const *env_export_arr() {
    ASSERT_IS_MAIN_THREAD();
    ASSERT_IS_NOT_FORKED_CHILD();
    vars_stack().update_export_array_if_necessary();
    return vars_stack().export_array.get();
}

void env_set_argv(const wchar_t *const *argv) {
    if (argv && *argv) {
        wcstring_list_t list;
        for (auto arg = argv; *arg; arg++) {
            list.push_back(*arg);
        }

        env_set(L"argv", ENV_LOCAL, list);
    } else {
        env_set_empty(L"argv", ENV_LOCAL);
    }
}

env_vars_snapshot_t::env_vars_snapshot_t(const wchar_t *const *keys) {
    ASSERT_IS_MAIN_THREAD();
    wcstring key;
    for (size_t i = 0; keys[i]; i++) {
        key.assign(keys[i]);
        const auto var = env_get(key);
        if (var) {
            vars[key] = *var;
        }
    }
}

env_vars_snapshot_t::env_vars_snapshot_t() {}

// The "current" variables are not a snapshot at all, but instead trampoline to env_get, etc.
// We identify the current snapshot based on pointer values.
static const env_vars_snapshot_t sCurrentSnapshot;
const env_vars_snapshot_t &env_vars_snapshot_t::current() { return sCurrentSnapshot; }

bool env_vars_snapshot_t::is_current() const { return this == &sCurrentSnapshot; }

maybe_t<env_var_t> env_vars_snapshot_t::get(const wcstring &key) const {
    // If we represent the current state, bounce to env_get.
    if (this->is_current()) {
        return env_get(key);
    }
    auto iter = vars.find(key);
    if (iter == vars.end()) return none();
    return env_var_t(iter->second);
}

const wchar_t *const env_vars_snapshot_t::highlighting_keys[] = {L"PATH", L"CDPATH",
                                                                 L"fish_function_path", NULL};

const wchar_t *const env_vars_snapshot_t::completing_keys[] = {L"PATH", L"CDPATH", NULL};
