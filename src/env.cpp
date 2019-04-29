// Functions for setting and getting environment variables.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <iterator>
#include <mutex>
#include <set>
#include <utility>
#include <vector>

#include "builtin_bind.h"
#include "common.h"
#include "env.h"
#include "env_dispatch.h"
#include "env_universal_common.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fish_version.h"
#include "history.h"
#include "input.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
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

/// The character used to delimit path and non-path variables in exporting and in string expansion.
static const wchar_t PATH_ARRAY_SEP = L':';
static const wchar_t NONPATH_ARRAY_SEP = L' ';

bool curses_initialized = false;

/// Does the terminal have the "eat_newline_glitch".
bool term_has_xn = false;

/// Universal variables global instance. Initialized in env_init.
static env_universal_t *s_universal_variables = NULL;

/// Getter for universal variables.
static env_universal_t *uvars() { return s_universal_variables; }

void env_universal_barrier() { env_stack_t::principal().universal_barrier(); }

// A typedef for a set of constant strings. Note our sets are typically on the order of 6 elements,
// so we don't bother to sort them.
using string_set_t = const wchar_t *const[];

/// Check if a variable may not be set using the set command.
static bool is_read_only(const wcstring &key) {
    static const string_set_t env_read_only = {
        L"PWD",          L"SHLVL",    L"history",  L"pipestatus", L"status",           L"version",
        L"FISH_VERSION", L"fish_pid", L"hostname", L"_",          L"fish_private_mode"};
    return contains(env_read_only, key) || (in_private_mode() && key == L"fish_history");
}

/// Return true if a variable should become a path variable by default. See #436.
static bool variable_should_auto_pathvar(const wcstring &name) {
    return string_suffixes_string(L"PATH", name);
}

/// Table of variables whose value is dynamically calculated, such as umask, status, etc.
static const string_set_t env_electric = {L"history", L"pipestatus", L"status", L"umask"};

static bool is_electric(const wcstring &key) { return contains(env_electric, key); }

/// \return a the value of a variable for \p key, which must be electric (computed).
static maybe_t<env_var_t> get_electric(const wcstring &key, const environment_t &vars) {
    if (key == L"history") {
        // Big hack. We only allow getting the history on the main thread. Note that history_t
        // may ask for an environment variable, so don't take the lock here (we don't need it).
        if (!is_main_thread()) {
            return none();
        }

        history_t *history = reader_get_history();
        if (!history) {
            history = &history_t::history_with_name(history_session_id(vars));
        }
        wcstring_list_t result;
        if (history) history->get_history(result);
        return env_var_t(L"history", result);
    } else if (key == L"pipestatus") {
        const auto js = proc_get_last_statuses();
        wcstring_list_t result;
        result.reserve(js.pipestatus.size());
        for (int i : js.pipestatus) {
            result.push_back(to_string(i));
        }
        return env_var_t(L"pipestatus", std::move(result));
    } else if (key == L"status") {
        return env_var_t(L"status", to_string(proc_get_last_status()));
    } else if (key == L"umask") {
        // note umask() is an absurd API: you call it to set the value and it returns the old value.
        // Thus we have to call it twice, to reset the value.
        // We need to lock since otherwise this races.
        // Guess what the umask is; if we guess right we don't need to reset it.
        static std::mutex umask_lock;
        scoped_lock locker(umask_lock);
        int guess = 022;
        mode_t res = umask(guess);
        if (res != guess) umask(res);
        return env_var_t(L"umask", format_string(L"0%0.3o", res));
    }
    // We should never get here unless the electric var list is out of sync with the above code.
    DIE("unrecognized electric var name");
}

/// Some env vars contain a list of paths where an empty path element is equivalent to ".".
/// Unfortunately that convention causes problems for fish scripts. So this function replaces the
/// empty path element with an explicit ".". See issue #3914.
void fix_colon_delimited_var(const wcstring &var_name, env_stack_t &vars) {
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

const wcstring_list_t &env_var_t::as_list() const { return *vals_; }

wchar_t env_var_t::get_delimiter() const {
    return is_pathvar() ? PATH_ARRAY_SEP : NONPATH_ARRAY_SEP;
}

/// Return a string representation of the var.
wcstring env_var_t::as_string() const { return join_strings(*vals_, get_delimiter()); }

void env_var_t::to_list(wcstring_list_t &out) const { out = *vals_; }

env_var_t::env_var_flags_t env_var_t::flags_for(const wchar_t *name) {
    env_var_flags_t result = 0;
    if (is_read_only(name)) result |= flag_read_only;
    return result;
}

/// \return a singleton empty list, to avoid unnecessary allocations in env_var_t.
std::shared_ptr<const wcstring_list_t> env_var_t::empty_list() {
    static const auto result = std::make_shared<const wcstring_list_t>();
    return result;
}

environment_t::~environment_t() = default;

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

// This is a big dorky lock we take around everything that might read from or modify an env_node_t.
// Fine grained locking is annoying here because env_nodes may be shared between env_stacks, so each
// node would need its own lock.
static std::mutex env_lock;

// Struct representing one level in the function variable stack.
// Only our variable stack should create and destroy these
class env_node_t {
   public:
    /// Variable table.
    var_table_t env;
    /// Does this node imply a new variable scope? If yes, all non-global variables below this one
    /// in the stack are invisible. If new_scope is set for the global variable node, the universe
    /// will explode.
    const bool new_scope;
    /// Does this node contain any variables which are exported to subshells
    /// or does it redefine any variables to not be exported?
    bool exportv = false;
    /// Pointer to next level.
    const std::shared_ptr<env_node_t> next;

    env_node_t(bool is_new_scope, std::shared_ptr<env_node_t> next_scope)
        : new_scope(is_new_scope), next(std::move(next_scope)) {}

    maybe_t<env_var_t> find_entry(const wcstring &key) {
        auto it = env.find(key);
        if (it != env.end()) return it->second;
        return none();
    }
};
using env_node_ref_t = std::shared_ptr<env_node_t>;

// A class wrapping up a variable stack.
// This forms a linekd list of env_node_t, and also maintains the export array for exported
// environment variables.
struct var_stack_t {
    var_stack_t(var_stack_t &&) = default;

    // Top node on the function stack.
    env_node_ref_t top;

    // Bottom node on the function stack.
    const env_node_ref_t global_env;

    // Exported variable array used by execv.
    maybe_t<null_terminated_array_t<char>> export_array;

    /// Flag for checking if we need to regenerate the exported variable array.
    void mark_changed_exported() { export_array.reset(); }

    bool has_changed_exported() const { return !export_array; }

    void update_export_array_if_necessary();

    var_stack_t(env_node_ref_t top, env_node_ref_t global_env)
        : top(std::move(top)), global_env(std::move(global_env)) {}

    // Pushes a new node onto our stack
    // Optionally creates a new scope for the node
    void push(bool new_scope) {
        auto node = std::make_shared<env_node_t>(new_scope, this->top);

        // Copy local-exported variables.
        auto top_node = top;
        // Only if we introduce a new shadowing scope; i.e. not if it's just `begin; end` or
        // "--no-scope-shadowing".
        if (new_scope && top_node != this->global_env) {
            for (const auto &var : top_node->env) {
                if (var.second.exports()) node->env.insert(var);
            }
        }

        this->top = std::move(node);
        if (new_scope && local_scope_exports(this->top)) {
            this->mark_changed_exported();
        }
    }

    // Pops the top node, asserting it's not global.
    // \return the popped node.
    env_node_ref_t pop() {
        assert(top != this->global_env && "Cannot pop global node");
        env_node_ref_t old_top = this->top;
        this->top = old_top->next;
        return old_top;
    }

    // Returns the next scope to search for a given node, respecting the new_scope flag.
    // Returns an empty pointer if we're done.
    env_node_ref_t next_scope_to_search(const env_node_ref_t &node) const {
        assert(node != NULL);
        if (node == this->global_env) {
            return nullptr;
        }
        return node->new_scope ? this->global_env : node->next;
    }

    // Returns the scope used for unspecified scopes. An unspecified scope is either the topmost
    // shadowing scope, or the global scope if none. This implements the default behavior of `set`.
    env_node_ref_t resolve_unspecified_scope() {
        env_node_ref_t node = this->top;
        while (node && !node->new_scope) {
            node = node->next;
        }
        return node ? node : this->global_env;
    }

    /// Copy this vars_stack.
    var_stack_t clone() const {
        return var_stack_t(*this);
    }

    /// Snapshot this vars_stack. That is, return a new vars_stack that has copies of all local
    /// variables. Note that this drops all shadowed nodes: only the currently executing function is
    /// copied. Global variables are referenced, not copied; this is both because there are a lot of
    /// global varibables so copying would be expensive, and some (electrics) are computed so cannot
    /// be effectively copied.
    std::unique_ptr<var_stack_t> snapshot() const {
        return make_unique<var_stack_t>(snapshot_node(top), global_env);
    }

    /// \return true if the topomst local scope exports a variable.
    bool local_scope_exports(const env_node_ref_t &n) const;

    /// \return a new stack with a single top level local scope.
    // This local scope will persist throughout the lifetime of the fish process, and it will ensure
    // that `set -l` commands run at the command-line don't affect the global scope.
    static std::unique_ptr<var_stack_t> create() {
        auto locals = std::make_shared<env_node_t>(false, globals());
        return make_unique<var_stack_t>(std::move(locals), globals());
    }

   private:
    /// Copy constructor. This does not copy the export array; it just allows it to be regenerated.
    var_stack_t(const var_stack_t &rhs) : top(rhs.top), global_env(rhs.global_env) {}

    void get_exported(const env_node_t *n, var_table_t &h) const;

    /// Recursive helper for snapshot(). Snapshot a node and its unshadows parents, returning it.
    env_node_ref_t snapshot_node(const env_node_ref_t &node) const {
        assert(node && "null node in snapshot_node");
        // If we are global, re-use the global node. If we reach a new scope, jump to globals; we
        // don't snapshot shadowed scopes, because the snapshot is intended to be read-only and so
        // there would be no way to access them.
        if (node == global_env) {
            return node;
        }
        auto next = snapshot_node(node->new_scope ? global_env : node->next);
        auto result = std::make_shared<env_node_t>(node->new_scope, next);
        // Copy over variables.
        // Note assigning env is a potentially big copy.
        result->exportv = node->exportv;
        result->env = node->env;
        return result;
    }

    /// \return the global variable set.
    /// Note that this is the only place where we talk about a single global variable set; each
    /// var_stack_t has its own reference to globals and could potentially have a different global
    /// set.
    static env_node_ref_t globals() {
        static env_node_ref_t s_globals{std::make_shared<env_node_t>(false, nullptr)};
        return s_globals;
    }
};

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

/// Returns true if the specified scope or any non-shadowed non-global subscopes contain an exported
/// variable.
bool var_stack_t::local_scope_exports(const env_node_ref_t &n) const {
    assert(n != nullptr);
    if (n == global_env) return false;

    if (n->exportv) return true;

    if (n->new_scope) return false;

    return local_scope_exports(n->next);
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

    // Construct the export list: a list of strings of the form key=value.
    std::vector<std::string> export_list;
    export_list.reserve(vals.size());
    for (const auto &kv : vals) {
        std::string str = wcs2string(kv.first);
        str.push_back('=');
        str.append(wcs2string(kv.second.as_string()));
        export_list.push_back(std::move(str));
    }
    export_array.emplace(export_list);
}

// Get the variable stack
var_stack_t &env_scoped_t::vars_stack() { return *vars_; }
const var_stack_t &env_scoped_t::vars_stack() const { return *vars_; }

// Read a variable respecting the given mode.
maybe_t<env_var_t> env_scoped_t::get(const wcstring &key, env_mode_flags_t mode) const {
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
        return get_electric(key, *this);
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
    // Okay, we couldn't find a local or global var given the requirements. If there is a matching
    // universal var return that.
    if (search_universal && uvars()) {
        auto var = uvars()->get(key);
        if (var && (var->exports() ? search_exported : search_unexported)) {
            return var;
        }
    }

    return none();
}

std::shared_ptr<environment_t> env_scoped_t::snapshot() const {
    return std::shared_ptr<env_scoped_t>(new env_scoped_t(vars_->snapshot()));
}

env_scoped_t::env_scoped_t(std::unique_ptr<var_stack_t> vars) : vars_(std::move(vars)) {}
env_scoped_t::env_scoped_t(env_scoped_t &&) = default;
env_scoped_t::~env_scoped_t() = default;

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

void env_stack_t::mark_changed_exported() { vars_stack().mark_changed_exported(); }

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
    // Check for /proc/self/stat to see if we are running with Linux-style procfs
    if (access("/proc/self/stat", R_OK) == 0) {
        have_proc_stat = true;
    }
}

/// Ensure the content of the magic path env vars is reasonable. Specifically, that empty path
/// elements are converted to explicit "." to make the vars easier to use in fish scripts.
static void init_path_vars() {
    // Do not replace empties in MATHPATH - see #4158.
    fix_colon_delimited_var(L"PATH", env_stack_t::globals());
    fix_colon_delimited_var(L"CDPATH", env_stack_t::globals());
}

/// Make sure the PATH variable contains something.
static void setup_path() {
    auto &vars = env_stack_t::globals();
    const auto path = vars.get(L"PATH");
    if (path.missing_or_empty()) {
#if defined(_CS_PATH)
        // _CS_PATH: colon-separated paths to find POSIX utilities
        std::string cspath;
        cspath.resize(confstr(_CS_PATH, nullptr, 0));
        confstr(_CS_PATH, &cspath[0], cspath.length());
#else
        std::string cspath = "/usr/bin:/bin";  // I doubt this is even necessary
#endif
        vars.set_one(L"PATH", ENV_GLOBAL | ENV_EXPORT, str2wcstring(cspath));
    }
}

void env_init(const struct config_paths_t *paths /* or NULL */) {
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

    // Set up $IFS - this used to be in share/config.fish, but really breaks if it isn't done.
    vars.set_one(L"IFS", ENV_GLOBAL, L"\n \t");

    // Set up the version variable.
    wcstring version = str2wcstring(get_fish_version());
    vars.set_one(L"version", ENV_GLOBAL, version);
    vars.set_one(L"FISH_VERSION", ENV_GLOBAL, version);

    // Set the $fish_pid variable.
    vars.set_one(L"fish_pid", ENV_GLOBAL, to_string(getpid()));

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
            nshlvl_str = to_string(shlvl_i + 1);
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
    // Note we may inherit a virtual PWD that doesn't match what getcwd would return; respect that
    // if and only if it matches getcwd (#5647). Note we treat PWD as read-only so it was not set in
    // vars.
    const char *incoming_pwd_cstr = getenv("PWD");
    wcstring incoming_pwd = incoming_pwd_cstr ? str2wcstring(incoming_pwd_cstr) : wcstring{};
    if (!incoming_pwd.empty() && paths_are_same_file(incoming_pwd, L".")) {
        vars.set_one(L"PWD", ENV_EXPORT | ENV_GLOBAL, incoming_pwd);
    } else {
        vars.set_pwd_from_getcwd();
    }
    vars.set_termsize();    // initialize the terminal size variables

    // Set fish_bind_mode to "default".
    vars.set_one(FISH_BIND_MODE_VAR, ENV_GLOBAL, DEFAULT_BIND_MODE);

    // Allow changes to variables to produce events.
    env_dispatch_init(vars);

    init_input();

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
        scoped_lock locker(env_lock);
        // Determine the node.
        bool has_changed_new = false;
        env_node_ref_t preexisting_node = get_node(key);
        maybe_t<env_var_t::env_var_flags_t> preexisting_flags{};
        if (preexisting_node != nullptr) {
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
        } else if (preexisting_node != nullptr) {
            node = std::move(preexisting_node);
            if ((var_mode & (ENV_EXPORT | ENV_UNEXPORT)) == 0) {
                // Use existing entry's exportv status.
                if (preexisting_flags && (*preexisting_flags & env_var_t::flag_export)) {
                    var_mode |= ENV_EXPORT;
                }
            }
        } else {
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

            var = var.setting_vals(std::move(val))
                      .setting_exports(var_mode & ENV_EXPORT)
                      .setting_pathvar(var_mode & ENV_PATHVAR)
                      .setting_read_only(is_read_only(key));

            if (var_mode & ENV_EXPORT) {
                // The new variable is exported.
                node->exportv = true;
                has_changed_new = true;
            } else {
                // Set the node's exported when it changes something about exports
                // (also when it redefines a variable to not be exported).
                node->exportv = has_changed_old != has_changed_new;
            }

            if (has_changed_old || has_changed_new) vars_stack().mark_changed_exported();
        }
    }

    event_fire(event_t::variable(key, {L"VARIABLE", L"SET", key}));
    env_dispatch_var_change(key, *this);
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
            event_fire(event_t::variable(key, {L"VARIABLE", L"ERASE", key}));
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
            event_fire(event_t::variable(key, {L"VARIABLE", L"ERASE", key}));
        }

        if (is_exported) vars_stack().mark_changed_exported();
    }

    env_dispatch_var_change(key, *this);

    return erased ? ENV_OK : ENV_NOT_FOUND;
}

void env_stack_t::push(bool new_scope) { vars_stack().push(new_scope); }

void env_stack_t::pop() {
    auto &vars = vars_stack();
    auto old_node = vars.pop();

    // Maybe exported variables have changed.
    if (old_node->exportv) {
        // This node exported or unexported a variable.
        vars.mark_changed_exported();
    } else if (old_node->new_scope && vars.local_scope_exports(old_node->next)) {
        // This node was a local scope, so it shadowed exports from its parent.
        vars.mark_changed_exported();
    }

    // TODO: we would like to coalesce locale / curses changes, so that we only re-initialize once.
    for (const auto &kv : old_node->env) {
        env_dispatch_var_change(kv.first, *this);
    }
}

wcstring_list_t env_scoped_t::get_names(int flags) const {
    scoped_lock locker(env_lock);

    wcstring_list_t result;
    std::set<wcstring> names;
    int show_local = flags & ENV_LOCAL;
    int show_global = flags & ENV_GLOBAL;
    int show_universal = flags & ENV_UNIVERSAL;

    env_node_ref_t n = vars_stack().top;
    const bool show_exported = (flags & ENV_EXPORT) || !(flags & ENV_UNEXPORT);
    const bool show_unexported = (flags & ENV_UNEXPORT) || !(flags & ENV_EXPORT);

    // Helper to add the names of variables from \p envs to names, respecting show_exported and
    // show_unexported.
    auto add_keys = [&](const var_table_t &envs) {
        for (const auto &kv : envs) {
            const env_var_t &var = kv.second;
            if (var.exports() ? show_exported : show_unexported) {
                names.insert(kv.first);
            }
        }
    };

    if (!show_local && !show_global && !show_universal) {
        show_local = show_universal = show_global = 1;
    }

    if (show_local) {
        while (n) {
            if (n == vars_stack().global_env) break;
            add_keys(n->env);
            if (n->new_scope)
                break;
            else
                n = n->next;
        }
    }

    if (show_global) {
        add_keys(vars_stack().global_env->env);
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

env_stack_t::~env_stack_t() = default;
env_stack_t::env_stack_t(env_stack_t &&) = default;

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
    static env_stack_t s_global(var_stack_t::create());
    return s_global;
}

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
        std::string tmpdir = get_path_to_tmp_dir() + "/fish.";
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
