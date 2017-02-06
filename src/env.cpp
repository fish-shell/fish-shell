// Functions for setting and getting environment variables.
#include "config.h"  // IWYU pragma: keep

#include <assert.h>
#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <pwd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif

#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

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
#include "path.h"
#include "proc.h"
#include "reader.h"
#include "sanity.h"
#include "screen.h"
#include "wutil.h"  // IWYU pragma: keep

/// Value denoting a null string.
#define ENV_NULL L"\x1d"

/// Some configuration path environment variables.
#define FISH_DATADIR_VAR L"__fish_datadir"
#define FISH_SYSCONFDIR_VAR L"__fish_sysconfdir"
#define FISH_HELPDIR_VAR L"__fish_help_dir"
#define FISH_BIN_DIR L"__fish_bin_dir"

/// At init, we read all the environment variables from this array.
extern char **environ;

bool g_use_posix_spawn = false;  // will usually be set to true

/// Does the terminal have the "eat_newline_glitch".
bool term_has_xn = false;

/// List of all locale environment variable names.
static const wchar_t *const locale_variable[] = {
    L"LANG",     L"LANGUAGE",          L"LC_ALL",         L"LC_ADDRESS",   L"LC_COLLATE",
    L"LC_CTYPE", L"LC_IDENTIFICATION", L"LC_MEASUREMENT", L"LC_MESSAGES",  L"LC_MONETARY",
    L"LC_NAME",  L"LC_NUMERIC",        L"LC_PAPER",       L"LC_TELEPHONE", L"LC_TIME",
    NULL};

/// List of all curses environment variable names.
static const wchar_t *const curses_variable[] = {L"TERM", L"TERMINFO", L"TERMINFO_DIRS", NULL};

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
    /// Does this node contain any variables which are exported to subshells.
    bool exportv = false;
    /// Pointer to next level.
    std::unique_ptr<env_node_t> next;

    /// Returns a pointer to the given entry if present, or NULL.
    const var_entry_t *find_entry(const wcstring &key);
};

class variable_entry_t {
    wcstring value; /**< Value of the variable */
};

static pthread_mutex_t env_lock = PTHREAD_MUTEX_INITIALIZER;

static bool local_scope_exports(const env_node_t *n);
static void handle_locale(const wchar_t *env_var_name);

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

    // Returns the next scope to search for a given node, respecting the new_scope lag
    // Returns NULL if we're done
    env_node_t *next_scope_to_search(env_node_t *node);
    const env_node_t *next_scope_to_search(const env_node_t *node) const;

    // Returns the scope used for unspecified scopes
    // An unspecified scope is either the topmost shadowing scope, or the global scope if none
    // This implements the default behavior of 'set'
    env_node_t *resolve_unspecified_scope();
};

void var_stack_t::push(bool new_scope) {
    std::unique_ptr<env_node_t> node(new env_node_t(new_scope));
    node->next = std::move(this->top);
    this->top = std::move(node);
    if (new_scope && local_scope_exports(this->top.get())) {
        this->mark_changed_exported();
    }
}

void var_stack_t::pop() {
    // Don't pop the global
    if (this->top.get() == this->global_env) {
        debug(0, _(L"Tried to pop empty environment stack."));
        sanity_lose();
        return;
    }

    const wchar_t *locale_changed = NULL;

    for (int i = 0; locale_variable[i]; i++) {
        var_table_t::iterator result = top->env.find(locale_variable[i]);
        if (result != top->env.end()) {
            locale_changed = locale_variable[i];
            break;
        }
    }

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

    var_table_t::iterator iter;
    for (const auto &entry_pair : old_top->env) {
        const var_entry_t &entry = entry_pair.second;
        if (entry.exportv) {
            this->mark_changed_exported();
            break;
        }
    }
    // TODO: Move this to something general.
    if (locale_changed) handle_locale(locale_changed);
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
struct const_string_set_comparer {
    bool operator()(const wchar_t *a, const wchar_t *b) { return wcscmp(a, b) < 0; }
};
typedef std::set<const wchar_t *, const_string_set_comparer> const_string_set_t;

/// Table of variables that may not be set using the set command.
static const_string_set_t env_read_only;

static bool is_read_only(const wcstring &key) {
    return env_read_only.find(key.c_str()) != env_read_only.end();
}

/// Table of variables whose value is dynamically calculated, such as umask, status, etc.
static const_string_set_t env_electric;

static bool is_electric(const wcstring &key) {
    return env_electric.find(key.c_str()) != env_electric.end();
}

const var_entry_t *env_node_t::find_entry(const wcstring &key) {
    const var_entry_t *result = NULL;
    var_table_t::const_iterator where = env.find(key);
    if (where != env.end()) {
        result = &where->second;
    }
    return result;
}

/// Return the current umask value.
static mode_t get_umask() {
    mode_t res;
    res = umask(0);
    umask(res);
    return res;
}

/// Check if the specified variable is a timezone variable.
static bool var_is_timezone(const wcstring &key) { return key == L"TZ"; }

/// Properly sets all timezone information.
static void handle_timezone(const wchar_t *env_var_name) {
    debug(2, L"handle_timezone() called in response to '%ls' changing", env_var_name);
    const env_var_t val = env_get_string(env_var_name, ENV_EXPORT);
    const std::string &value = wcs2string(val);
    const std::string &name = wcs2string(env_var_name);
    debug(2, L"timezone var %s='%s'", name.c_str(), value.c_str());
    if (val.empty()) {
        unsetenv(name.c_str());
    } else {
        setenv(name.c_str(), value.c_str(), 1);
    }
    tzset();
}

/// Check if the specified variable is a locale variable.
static bool var_is_locale(const wcstring &key) {
    for (size_t i = 0; locale_variable[i]; i++) {
        if (key == locale_variable[i]) {
            return true;
        }
    }
    return false;
}

/// Properly sets all locale information.
static void handle_locale(const wchar_t *env_var_name) {
    debug(2, L"handle_locale() called in response to '%ls' changing", env_var_name);
    // We have to make a copy because the subsequent setlocale() call to change the locale will
    // invalidate the pointer from the this setlocale() call.
    char *old_msg_locale = strdup(setlocale(LC_MESSAGES, NULL));
    const env_var_t val = env_get_string(env_var_name, ENV_EXPORT);
    const std::string &value = wcs2string(val);
    const std::string &name = wcs2string(env_var_name);
    debug(2, L"locale var %s='%s'", name.c_str(), value.c_str());
    if (val.empty()) {
        unsetenv(name.c_str());
    } else {
        setenv(name.c_str(), value.c_str(), 1);
    }

    char *locale = setlocale(LC_ALL, "");
    fish_setlocale();
    debug(2, L"handle_locale() setlocale(): '%s'", locale);

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

/// Check if the specified variable is a locale variable.
static bool var_is_curses(const wcstring &key) {
    for (size_t i = 0; curses_variable[i]; i++) {
        if (key == curses_variable[i]) {
            return true;
        }
    }
    return false;
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
static bool does_term_support_setting_title() {
    const env_var_t term_str = env_get_string(L"TERM");
    if (term_str.missing()) return false;

    const wchar_t *term = term_str.c_str();
    bool recognized = contains(term, L"xterm", L"screen", L"tmux", L"nxterm", L"rxvt");
    if (!recognized) recognized = !wcsncmp(term, L"xterm-", wcslen(L"xterm-"));
    if (!recognized) recognized = !wcsncmp(term, L"screen-", wcslen(L"screen-"));
    if (!recognized) recognized = !wcsncmp(term, L"tmux-", wcslen(L"tmux-"));
    if (!recognized) {
        if (contains(term, L"linux", L"dumb")) return false;

        char *n = ttyname(STDIN_FILENO);
        if (!n || strstr(n, "tty") || strstr(n, "/vc/")) return false;
    }

    return true;
}

/// Handle changes to the TERM env var that do not involves the curses subsystem.
static void handle_term() { can_set_term_title = does_term_support_setting_title(); }

/// Push all curses/terminfo env vars into the global environment where they can be found by those
/// libraries.
static void handle_curses(const wchar_t *env_var_name) {
    debug(2, L"handle_curses() called in response to '%ls' changing", env_var_name);
    const env_var_t val = env_get_string(env_var_name, ENV_EXPORT);
    const std::string &name = wcs2string(env_var_name);
    const std::string &value = wcs2string(val);
    debug(2, L"curses var %s='%s'", name.c_str(), value.c_str());
    if (val.empty()) {
        unsetenv(name.c_str());
    } else {
        setenv(name.c_str(), value.c_str(), 1);
    }
    // TODO: Modify input_init() to allow calling it when the terminfo env vars are dynamically
    // changed. At the present time it can be called just once. Also, we should really only do this
    // if the TERM var is set.
    // input_init();
    term_has_xn = tgetflag((char *)"xn") == 1;  // does terminal have the eat_newline_glitch

    // Invalidate the cached escape sequences since they may no longer be valid.
    cached_esc_sequences.clear();
}

/// React to modifying the given variable.
static void react_to_variable_change(const wcstring &key) {
    if (var_is_locale(key)) {
        handle_locale(key.c_str());
    } else if (var_is_curses(key)) {
        handle_curses(key.c_str());
        if (key == L"TERM") handle_term();
    } else if (var_is_timezone(key)) {
        handle_timezone(key.c_str());
    } else if (key == L"fish_term256" || key == L"fish_term24bit") {
        update_fish_color_support();
        reader_react_to_color_change();
    } else if (string_prefixes_string(L"fish_color_", key)) {
        reader_react_to_color_change();
    } else if (key == L"fish_escape_delay_ms") {
        update_wait_on_escape_ms();
    } else if (key == L"LINES" || key == L"COLUMNS") {
        invalidate_termsize(true);  // force fish to update its idea of the terminal size plus vars
    }
}

/// Universal variable callback function. This function makes sure the proper events are triggered
/// when an event occurs.
static void universal_callback(fish_message_type_t type, const wchar_t *name) {
    const wchar_t *str = NULL;

    switch (type) {
        case SET:
        case SET_EXPORT: {
            str = L"SET";
            break;
        }
        case ERASE: {
            str = L"ERASE";
            break;
        }
    }

    if (str) {
        vars_stack().mark_changed_exported();

        event_t ev = event_t::variable_event(name);
        ev.arguments.push_back(L"VARIABLE");
        ev.arguments.push_back(str);
        ev.arguments.push_back(name);
        event_fire(&ev);
    }

    if (name) react_to_variable_change(name);
}

/// Make sure the PATH variable contains something.
static void setup_path() {
    const env_var_t path = env_get_string(L"PATH");
    if (path.missing_or_empty()) {
        const wchar_t *value = L"/usr/bin" ARRAY_SEP_STR L"/bin";
        env_set(L"PATH", value, ENV_GLOBAL | ENV_EXPORT);
    }
}

/// Initialize the `COLUMNS` and `LINES` env vars if they don't already exist to reasonable
/// defaults. They will be updated later by the `get_current_winsize()` function if they need to be
/// adjusted.
static void env_set_termsize() {
    env_var_t cols = env_get_string(L"COLUMNS");
    if (cols.missing_or_empty()) env_set(L"COLUMNS", DFLT_TERM_COL_STR, ENV_EXPORT | ENV_GLOBAL);
    env_var_t rows = env_get_string(L"LINES");
    if (rows.missing_or_empty()) env_set(L"LINES", DFLT_TERM_ROW_STR, ENV_EXPORT | ENV_GLOBAL);
}

bool env_set_pwd() {
    wcstring res = wgetcwd();
    if (res.empty()) {
        debug(0,
              _(L"Could not determine current working directory. Is your locale set correctly?"));
        return false;
    }
    env_set(L"PWD", res.c_str(), ENV_EXPORT | ENV_GLOBAL);
    return true;
}

wcstring env_get_pwd_slash(void) {
    env_var_t pwd = env_get_string(L"PWD");
    if (pwd.missing_or_empty()) {
        return L"";
    }
    if (!string_suffixes_string(L"/", pwd)) {
        pwd.push_back(L'/');
    }
    return pwd;
}

// Here is the whitelist of variables that we colon-delimit, both incoming from the environment and
// outgoing back to it. This is deliberately very short - we don't want to add language-specific
// values like CLASSPATH.
static bool variable_is_colon_delimited_array(const wcstring &str) {
    return contains(str, L"PATH", L"MANPATH", L"CDPATH");
}

/// Set up the USER variable.
static void setup_user(bool force) {
    if (env_get_string(L"USER").missing_or_empty() || force) {
        const struct passwd *pw = getpwuid(getuid());
        if (pw && pw->pw_name) {
            const wcstring uname = str2wcstring(pw->pw_name);
            env_set(L"USER", uname.c_str(), ENV_GLOBAL | ENV_EXPORT);
        }
    }
}

void env_init(const struct config_paths_t *paths /* or NULL */) {
    // These variables can not be altered directly by the user.
    const wchar_t *const ro_keys[] = {
        L"status", L"history", L"_", L"PWD", L"FISH_VERSION",
        // L"SHLVL" is readonly but will be inserted below after we increment it.
    };
    for (size_t i = 0; i < sizeof ro_keys / sizeof *ro_keys; i++) {
        env_read_only.insert(ro_keys[i]);
    }

    // Names of all dynamically calculated variables.
    env_electric.insert(L"history");
    env_electric.insert(L"status");
    env_electric.insert(L"umask");

    // Now the environment variable handling is set up, the next step is to insert valid data.

    // Import environment variables. Walk backwards so that the first one out of any duplicates wins
    // (#2784).
    wcstring key, val;
    const char *const *envp = environ;
    size_t i = 0;
    while (envp && envp[i]) {
        i++;
    }
    while (i--) {
        const wcstring key_and_val = str2wcstring(envp[i]);  // like foo=bar
        size_t eql = key_and_val.find(L'=');
        if (eql == wcstring::npos) {
            // No equals found.
            if (is_read_only(key_and_val) || is_electric(key_and_val)) continue;
            env_set(key_and_val, L"", ENV_EXPORT | ENV_GLOBAL);
        } else {
            key.assign(key_and_val, 0, eql);
            if (is_read_only(key) || is_electric(key)) continue;
            val.assign(key_and_val, eql + 1, wcstring::npos);
            if (variable_is_colon_delimited_array(key)) {
                std::replace(val.begin(), val.end(), L':', ARRAY_SEP);
            }

            env_set(key, val.c_str(), ENV_EXPORT | ENV_GLOBAL);
        }
    }

    // Set the given paths in the environment, if we have any.
    if (paths != NULL) {
        env_set(FISH_DATADIR_VAR, paths->data.c_str(), ENV_GLOBAL);
        env_set(FISH_SYSCONFDIR_VAR, paths->sysconf.c_str(), ENV_GLOBAL);
        env_set(FISH_HELPDIR_VAR, paths->doc.c_str(), ENV_GLOBAL);
        env_set(FISH_BIN_DIR, paths->bin.c_str(), ENV_GLOBAL);
    }

    // Set up the USER and PATH variables
    setup_path();
    setup_user(false);

    // Set up the version variable.
    wcstring version = str2wcstring(get_fish_version());
    env_set(L"FISH_VERSION", version.c_str(), ENV_GLOBAL);

    // Set up SHLVL variable.
    const env_var_t shlvl_str = env_get_string(L"SHLVL");
    wcstring nshlvl_str = L"1";
    if (!shlvl_str.missing()) {
        const wchar_t *end;
        // TODO: Figure out how to handle invalid numbers better. Shouldn't we issue a diagnostic?
        long shlvl_i = fish_wcstol(shlvl_str.c_str(), &end);
        if (!errno && shlvl_i >= 0) {
            nshlvl_str = to_string<long>(shlvl_i + 1);
        }
    }
    env_set(L"SHLVL", nshlvl_str.c_str(), ENV_GLOBAL | ENV_EXPORT);
    env_read_only.insert(L"SHLVL");

    // Set up the HOME variable.
    if (env_get_string(L"HOME").missing_or_empty()) {
        const env_var_t unam = env_get_string(L"USER");
        char *unam_narrow = wcs2str(unam.c_str());
        struct passwd *pw = getpwnam(unam_narrow);
        if (pw == NULL) {
            // Maybe USER is set but it's bogus. Reset USER from the db and try again.
            setup_user(true);
            const env_var_t unam = env_get_string(L"USER");
            unam_narrow = wcs2str(unam.c_str());
            pw = getpwnam(unam_narrow);
        }
        if (pw && pw->pw_dir) {
            const wcstring dir = str2wcstring(pw->pw_dir);
            env_set(L"HOME", dir.c_str(), ENV_GLOBAL | ENV_EXPORT);
        }
        free(unam_narrow);
    }

    env_set_pwd();       // initialize the PWD variable
    env_set_termsize();  // initialize the terminal size variables

    // Set up universal variables. The empty string means to use the deafult path.
    assert(s_universal_variables == NULL);
    s_universal_variables = new env_universal_t(L"");
    s_universal_variables->load();

    // Set g_use_posix_spawn. Default to true.
    env_var_t use_posix_spawn = env_get_string(L"fish_use_posix_spawn");
    g_use_posix_spawn =
        (use_posix_spawn.missing_or_empty() ? true : from_string<bool>(use_posix_spawn));

    // Set fish_bind_mode to "default".
    env_set(FISH_BIND_MODE_VAR, DEFAULT_BIND_MODE, ENV_GLOBAL);

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
        if (env->find_entry(key) != NULL) {
            break;
        }

        env = vars_stack().next_scope_to_search(env);
    }
    return env;
}

/// Set the value of the environment variable whose name matches key to val.
///
/// Memory policy: All keys and values are copied, the parameters can and should be freed by the
/// caller afterwards
///
/// \param key The key
/// \param val The value
/// \param var_mode The type of the variable. Can be any combination of ENV_GLOBAL, ENV_LOCAL,
/// ENV_EXPORT and ENV_USER. If mode is zero, the current variable space is searched and the current
/// mode is used. If no current variable with the same name is found, ENV_LOCAL is assumed.
///
/// Returns:
///
/// * ENV_OK on success.
/// * ENV_PERM, can only be returned when setting as a user, e.g. ENV_USER is set. This means that
/// the user tried to change a read-only variable.
/// * ENV_SCOPE, the variable cannot be set in the given scope. This applies to readonly/electric
/// variables set from the local or universal scopes, or set as exported.
/// * ENV_INVALID, the variable value was invalid. This applies only to special variables.
int env_set(const wcstring &key, const wchar_t *val, env_mode_flags_t var_mode) {
    ASSERT_IS_MAIN_THREAD();
    bool has_changed_old = vars_stack().has_changed_exported;
    int done = 0;

    if (val && contains(key, L"PWD", L"HOME")) {
        // Canonicalize our path; if it changes, recurse and try again.
        wcstring val_canonical = val;
        path_make_canonical(val_canonical);
        if (val != val_canonical) {
            return env_set(key, val_canonical.c_str(), var_mode);
        }
    }

    if ((var_mode & (ENV_LOCAL | ENV_UNIVERSAL)) && (is_read_only(key) || is_electric(key))) {
        return ENV_SCOPE;
    }
    if ((var_mode & ENV_EXPORT) && is_electric(key)) {
        return ENV_SCOPE;
    }
    if ((var_mode & ENV_USER) && is_read_only(key)) {
        return ENV_PERM;
    }

    if (key == L"umask") {
        // Set the new umask.
        if (val && wcslen(val)) {
            long mask = fish_wcstol(val, NULL, 8);
            if (!errno && mask <= 0777 && mask >= 0) {
                umask(mask);
                // Do not actually create a umask variable, on env_get, it will be calculated
                // dynamically.
                return ENV_OK;
            }
        }

        return ENV_INVALID;
    }

    // Zero element arrays are internaly not coded as null but as this placeholder string.
    if (!val) {
        val = ENV_NULL;  //!OCLINT(parameter reassignment)
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
            const var_entry_t &entry = result->second;
            if (entry.exportv) {
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
                // use existing entry's exportv
                var_mode =  //!OCLINT(parameter reassignment)
                    preexisting_entry_exportv ? ENV_EXPORT : 0;
            }
        } else {
            if (!get_proc_had_barrier()) {
                set_proc_had_barrier(true);
                env_universal_barrier();
            }

            if (uvars() && !uvars()->get(key).missing()) {
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
            var_entry_t &entry = node->env[key];
            if (entry.exportv) {
                // This variable already existed, and was exported.
                has_changed_new = true;
            }
            entry.val = val;
            if (var_mode & ENV_EXPORT) {
                // The new variable is exported.
                entry.exportv = true;
                node->exportv = true;
                has_changed_new = true;
            } else {
                entry.exportv = false;
            }

            if (has_changed_old || has_changed_new) vars_stack().mark_changed_exported();
        }
    }

    event_t ev = event_t::variable_event(key);
    ev.arguments.reserve(3);
    ev.arguments.push_back(L"VARIABLE");
    ev.arguments.push_back(L"SET");
    ev.arguments.push_back(key);

    // debug( 1, L"env_set: fire events on variable %ls", key );
    event_fire(&ev);
    // debug( 1, L"env_set: return from event firing" );

    react_to_variable_change(key);
    return ENV_OK;
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

    react_to_variable_change(key);

    return !erased;
}

const wchar_t *env_var_t::c_str(void) const {
    assert(!is_missing);  //!OCLINT(multiple unary operator)
    return wcstring::c_str();
}

env_var_t env_get_string(const wcstring &key, env_mode_flags_t mode) {
    const bool has_scope = mode & (ENV_LOCAL | ENV_GLOBAL | ENV_UNIVERSAL);
    const bool search_local = !has_scope || (mode & ENV_LOCAL);
    const bool search_global = !has_scope || (mode & ENV_GLOBAL);
    const bool search_universal = !has_scope || (mode & ENV_UNIVERSAL);

    const bool search_exported = (mode & ENV_EXPORT) || !(mode & ENV_UNEXPORT);
    const bool search_unexported = (mode & ENV_UNEXPORT) || !(mode & ENV_EXPORT);

    // Make the assumption that electric keys can't be shadowed elsewhere, since we currently block
    // that in env_set().
    if (is_electric(key)) {
        if (!search_global) return env_var_t::missing_var();
        if (key == L"history") {
            // Big hack. We only allow getting the history on the main thread. Note that history_t
            // may ask for an environment variable, so don't take the lock here (we don't need it).
            if (!is_main_thread()) {
                return env_var_t::missing_var();
            }
            env_var_t result;

            history_t *history = reader_get_history();
            if (!history) {
                history = &history_t::history_with_name(L"fish");
            }
            if (history) history->get_string_representation(&result, ARRAY_SEP_STR);
            return result;
        } else if (key == L"status") {
            return to_string(proc_get_last_status());
        } else if (key == L"umask") {
            return format_string(L"0%0.3o", get_umask());
        }
        // We should never get here unless the electric var list is out of sync with the above code.
        DIE("unerecognized electric var name");
    }

    if (search_local || search_global) {
        /* Lock around a local region */
        scoped_lock locker(env_lock);

        env_node_t *env = search_local ? vars_stack().top.get() : vars_stack().global_env;

        while (env != NULL) {
            const var_entry_t *entry = env->find_entry(key);
            if (entry != NULL && (entry->exportv ? search_exported : search_unexported)) {
                if (entry->val == ENV_NULL) {
                    return env_var_t::missing_var();
                }
                return entry->val;
            }

            if (has_scope) {
                if (!search_global || env == vars_stack().global_env) break;
                env = vars_stack().global_env;
            } else {
                env = vars_stack().next_scope_to_search(env);
            }
        }
    }

    if (!search_universal) return env_var_t::missing_var();

    // Another hack. Only do a universal barrier on the main thread (since it can change variable
    // values). Make sure we do this outside the env_lock because it may itself call env_get_string.
    if (is_main_thread() && !get_proc_had_barrier()) {
        set_proc_had_barrier(true);
        env_universal_barrier();
    }

    if (uvars()) {
        env_var_t env_var = uvars()->get(key);
        if (env_var == ENV_NULL ||
            !(uvars()->get_export(key) ? search_exported : search_unexported)) {
            env_var = env_var_t::missing_var();
        }
        return env_var;
    }
    return env_var_t::missing_var();
}

bool env_exist(const wchar_t *key, env_mode_flags_t mode) {
    CHECK(key, false);

    const bool has_scope = mode & (ENV_LOCAL | ENV_GLOBAL | ENV_UNIVERSAL);
    const bool test_local = !has_scope || (mode & ENV_LOCAL);
    const bool test_global = !has_scope || (mode & ENV_GLOBAL);
    const bool test_universal = !has_scope || (mode & ENV_UNIVERSAL);

    const bool test_exported = (mode & ENV_EXPORT) || !(mode & ENV_UNEXPORT);
    const bool test_unexported = (mode & ENV_UNEXPORT) || !(mode & ENV_EXPORT);

    if (is_electric(key)) {
        // Electric variables all exist, and they are all global. A local or universal version can
        // not exist. They are also never exported.
        if (test_global && test_unexported) {
            return true;
        }
        return false;
    }

    if (test_local || test_global) {
        const env_node_t *env = test_local ? vars_stack().top.get() : vars_stack().global_env;
        while (env != NULL) {
            if (env == vars_stack().global_env && !test_global) {
                break;
            }

            var_table_t::const_iterator result = env->env.find(key);
            if (result != env->env.end()) {
                const var_entry_t &res = result->second;
                return res.exportv ? test_exported : test_unexported;
            }
            env = vars_stack().next_scope_to_search(env);
        }
    }

    if (test_universal) {
        if (!get_proc_had_barrier()) {
            set_proc_had_barrier(true);
            env_universal_barrier();
        }

        if (uvars() && !uvars()->get(key).missing()) {
            return uvars()->get_export(key) ? test_exported : test_unexported;
        }
    }

    return 0;
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
        const var_entry_t &e = iter->second;

        if ((e.exportv && show_exported) || (!e.exportv && show_unexported)) {
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
static void get_exported(const env_node_t *n, std::map<wcstring, wcstring> *h) {
    if (!n) return;

    if (n->new_scope)
        get_exported(vars_stack().global_env, h);
    else
        get_exported(n->next.get(), h);

    var_table_t::const_iterator iter;
    for (iter = n->env.begin(); iter != n->env.end(); ++iter) {
        const wcstring &key = iter->first;
        const var_entry_t &val_entry = iter->second;

        if (val_entry.exportv && val_entry.val != ENV_NULL) {
            // Export the variable. Don't use std::map::insert here, since we need to overwrite
            // existing values from previous scopes.
            (*h)[key] = val_entry.val;
        } else {
            // We need to erase from the map if we are not exporting, since a lower scope may have
            // exported. See #2132.
            h->erase(key);
        }
    }
}

// Given a map from key to value, add values to out of the form key=value.
static void export_func(const std::map<wcstring, wcstring> &envs, std::vector<std::string> &out) {
    out.reserve(out.size() + envs.size());
    std::map<wcstring, wcstring>::const_iterator iter;
    for (iter = envs.begin(); iter != envs.end(); ++iter) {
        const wcstring &key = iter->first;
        const std::string &ks = wcs2string(key);
        std::string vs = wcs2string(iter->second);

        // Arrays in the value are ASCII record separator (0x1e) delimited. But some variables
        // should have colons. Add those.
        if (variable_is_colon_delimited_array(key)) {
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
    std::map<wcstring, wcstring> vals;

    debug(4, L"env_export_arr() recalc");

    get_exported(this->top.get(), &vals);

    if (uvars()) {
        const wcstring_list_t uni = uvars()->get_names(true, false);
        for (size_t i = 0; i < uni.size(); i++) {
            const wcstring &key = uni.at(i);
            const env_var_t val = uvars()->get(key);

            if (!val.missing() && val != ENV_NULL) {
                // Note that std::map::insert does NOT overwrite a value already in the map,
                // which we depend on here.
                vals.insert(std::pair<wcstring, wcstring>(key, val));
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
    if (*argv) {
        const wchar_t *const *arg;
        wcstring sb;

        for (arg = argv; *arg; arg++) {
            if (arg != argv) {
                sb.append(ARRAY_SEP_STR);
            }
            sb.append(*arg);
        }

        env_set(L"argv", sb.c_str(), ENV_LOCAL);
    } else {
        env_set(L"argv", 0, ENV_LOCAL);
    }
}

env_vars_snapshot_t::env_vars_snapshot_t(const wchar_t *const *keys) {
    ASSERT_IS_MAIN_THREAD();
    wcstring key;
    for (size_t i = 0; keys[i]; i++) {
        key.assign(keys[i]);
        const env_var_t val = env_get_string(key);
        if (!val.missing()) {
            vars[key] = val;
        }
    }
}

void env_universal_barrier() {
    ASSERT_IS_MAIN_THREAD();
    if (uvars()) {
        callback_data_list_t changes;
        bool changed = uvars()->sync(&changes);
        if (changed) {
            universal_notifier_t::default_notifier().post_notification();
        }

        // Post callbacks.
        for (size_t i = 0; i < changes.size(); i++) {
            const callback_data_t &data = changes.at(i);
            universal_callback(data.type, data.key.c_str());
        }
    }
}

env_vars_snapshot_t::env_vars_snapshot_t() {}

// The "current" variables are not a snapshot at all, but instead trampoline to env_get_string, etc.
// We identify the current snapshot based on pointer values.
static const env_vars_snapshot_t sCurrentSnapshot;
const env_vars_snapshot_t &env_vars_snapshot_t::current() { return sCurrentSnapshot; }

bool env_vars_snapshot_t::is_current() const { return this == &sCurrentSnapshot; }

env_var_t env_vars_snapshot_t::get(const wcstring &key) const {
    // If we represent the current state, bounce to env_get_string.
    if (this->is_current()) {
        return env_get_string(key);
    }
    std::map<wcstring, wcstring>::const_iterator iter = vars.find(key);
    return iter == vars.end() ? env_var_t::missing_var() : env_var_t(iter->second);
}

const wchar_t *const env_vars_snapshot_t::highlighting_keys[] = {L"PATH", L"CDPATH",
                                                                 L"fish_function_path", NULL};

const wchar_t *const env_vars_snapshot_t::completing_keys[] = {L"PATH", L"CDPATH", NULL};
