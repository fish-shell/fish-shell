// Functions for setting and getting environment variables.
#include "config.h"  // IWYU pragma: keep

#include "env.h"

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
#include "env_dispatch.h"
#include "env_universal_common.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fish_version.h"
#include "flog.h"
#include "global_safety.h"
#include "history.h"
#include "input.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
#include "termsize.h"
#include "wcstringutil.h"
#include "wutil.h"  // IWYU pragma: keep

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
static constexpr wchar_t PATH_ARRAY_SEP = L':';
static constexpr wchar_t NONPATH_ARRAY_SEP = L' ';

bool curses_initialized = false;

/// Does the terminal have the "eat_newline_glitch".
bool term_has_xn = false;

/// Universal variables global instance. Initialized in env_init.
static latch_t<env_universal_t> s_universal_variables;

/// Getter for universal variables.
static env_universal_t *uvars() { return s_universal_variables; }

bool env_universal_barrier() { return env_stack_t::principal().universal_barrier(); }

struct electric_var_t {
    enum {
        freadonly = 1 << 0,  // May not be modified by the user.
        fcomputed = 1 << 1,  // Value is dynamically computed.
        fexports = 1 << 2,   // Exported to child processes.
    };
    const wchar_t *name;
    uint32_t flags;

    bool readonly() const { return flags & freadonly; }

    bool computed() const { return flags & fcomputed; }

    bool exports() const { return flags & fexports; }

    static const electric_var_t *for_name(const wcstring &name);
};

// Keep sorted alphabetically
static const std::vector<electric_var_t> electric_variables{
    {L"FISH_VERSION", electric_var_t::freadonly},
    {L"PWD", electric_var_t::freadonly | electric_var_t::fcomputed | electric_var_t::fexports},
    {L"SHLVL", electric_var_t::freadonly | electric_var_t::fexports},
    {L"_", electric_var_t::freadonly},
    {L"fish_kill_signal", electric_var_t::freadonly | electric_var_t::fcomputed},
    {L"fish_pid", electric_var_t::freadonly},
    {L"fish_private_mode", electric_var_t::freadonly},
    {L"history", electric_var_t::freadonly | electric_var_t::fcomputed},
    {L"hostname", electric_var_t::freadonly},
    {L"pipestatus", electric_var_t::freadonly | electric_var_t::fcomputed},
    {L"status", electric_var_t::freadonly | electric_var_t::fcomputed},
    {L"status_generation", electric_var_t::freadonly | electric_var_t::fcomputed},
    {L"umask", electric_var_t::fcomputed},
    {L"version", electric_var_t::freadonly},
};

const electric_var_t *electric_var_t::for_name(const wcstring &name) {
    static auto first = electric_variables.begin();
    static auto last = electric_variables.end();
    electric_var_t search{name.c_str(), 0};
    auto binsearch = std::lower_bound(first, last, search,
                                      [&](const electric_var_t &v1, const electric_var_t &v2) {
                                          return wcscmp(v1.name, v2.name) < 0;
                                      });
    if (binsearch != last && wcscmp(name.c_str(), binsearch->name) == 0) {
        return &*binsearch;
    }
    return nullptr;
}

/// Check if a variable may not be set using the set command.
static bool is_read_only(const wcstring &key) {
    if (auto ev = electric_var_t::for_name(key)) {
        return ev->readonly();
    }
    // Hack.
    return in_private_mode() && key == L"fish_history";
}

/// Return true if a variable should become a path variable by default. See #436.
static bool variable_should_auto_pathvar(const wcstring &name) {
    return string_suffixes_string(L"PATH", name);
}
// This is a big dorky lock we take around everything that might read from or modify an env_node_t.
// Fine grained locking is annoying here because env_nodes may be shared between env_stacks, so each
// node would need its own lock.
static std::mutex env_lock;

/// We cache our null-terminated export list. However an exported variable may change for lots of
/// reasons: popping a scope, a modified universal variable, etc. We thus have a monotone counter.
/// Every time an exported variable changes in a node, it acquires the next generation. 0 is a
/// sentinel that indicates that the node contains no exported variables.
using export_generation_t = uint64_t;
static export_generation_t next_export_generation() {
    static owning_lock<export_generation_t> s_gen;
    auto val = s_gen.acquire();
    return ++*val;
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
    static const auto s_empty_result = std::make_shared<const wcstring_list_t>();
    return s_empty_result;
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
        setvbuf(stdout, nullptr, _IONBF, 0);
    }
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
    env_stack_t &vars = env_stack_t::principal();
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
            if (!electric_var_t::for_name(key_and_val)) {
                vars.set_empty(key_and_val, ENV_EXPORT | ENV_GLOBAL);
            }
        } else {
            key.assign(key_and_val, 0, eql);
            val.assign(key_and_val, eql + 1, wcstring::npos);
            if (!electric_var_t::for_name(key)) {
                // fish_user_paths should not be exported; attempting to re-import it from
                // a value we previously (due to user error) exported will cause impossibly
                // difficult to debug PATH problems.
                if (key != L"fish_user_paths") {
                    vars.set(key, ENV_EXPORT | ENV_GLOBAL, {val});
                }
            }
        }
    }

    // Set the given paths in the environment, if we have any.
    if (paths != nullptr) {
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
            std::string unam_narrow = wcs2string(user_var->as_string());
            struct passwd userinfo;
            struct passwd *result;
            char buf[8192];
            int retval = getpwnam_r(unam_narrow.c_str(), &userinfo, buf, sizeof(buf), &result);
            if (retval || !result) {
                // Maybe USER is set but it's bogus. Reset USER from the db and try again.
                setup_user(true);
                user_var = vars.get(L"USER");
                if (!user_var.missing_or_empty()) {
                    unam_narrow = wcs2string(user_var->as_string());
                    retval = getpwnam_r(unam_narrow.c_str(), &userinfo, buf, sizeof(buf), &result);
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

    // Initialize termsize variables.
    auto termsize = termsize_container_t::shared().initialize(vars);
    if (vars.get(L"COLUMNS").missing_or_empty())
        vars.set_one(L"COLUMNS", ENV_GLOBAL, to_string(termsize.width));
    if (vars.get(L"LINES").missing_or_empty())
        vars.set_one(L"LINES", ENV_GLOBAL, to_string(termsize.height));

    // Set fish_bind_mode to "default".
    vars.set_one(FISH_BIND_MODE_VAR, ENV_GLOBAL, DEFAULT_BIND_MODE);

    // Allow changes to variables to produce events.
    env_dispatch_init(vars);

    init_input();

    // Complain about invalid config paths.
    path_emit_config_directory_errors(vars);

    // Set up universal variables. The empty string means to use the default path.
    s_universal_variables.emplace(L"");
    callback_data_list_t callbacks;
    s_universal_variables->initialize(callbacks);
    env_universal_callbacks(&vars, callbacks);

    // Do not import variables that have the same name and value as
    // an exported universal variable. See issues #5258 and #5348.
    for (const auto &kv : uvars()->get_table()) {
        const wcstring &name = kv.first;
        const env_var_t &uvar = kv.second;
        if (!uvar.exports()) continue;
        // Look for a global exported variable with the same name.
        maybe_t<env_var_t> global = vars.globals().get(name, ENV_GLOBAL | ENV_EXPORT);
        if (global && uvar.as_string() == global->as_string()) {
            vars.globals().remove(name, ENV_GLOBAL | ENV_EXPORT);
        }
    }
}

static int set_umask(const wcstring_list_t &list_val) {
    long mask = -1;
    if (list_val.size() == 1 && !list_val.front().empty()) {
        mask = fish_wcstol(list_val.front().c_str(), nullptr, 8);
    }

    if (errno || mask > 0777 || mask < 0) return ENV_INVALID;
    // Do not actually create a umask variable. On env_stack_t::get() it will be calculated.
    umask(mask);
    return ENV_OK;
}

namespace {
struct query_t {
    // Whether any scopes were specified.
    bool has_scope;

    // Whether to search local, global, universal scopes.
    bool local;
    bool global;
    bool universal;

    // Whether export or unexport was specified.
    bool has_export_unexport;

    // Whether to search exported and unexported variables.
    bool exports;
    bool unexports;

    // Whether pathvar or unpathvar was set.
    bool has_pathvar_unpathvar;
    bool pathvar;
    bool unpathvar;

    // Whether this is a "user" set.
    bool user;

    explicit query_t(env_mode_flags_t mode) {
        has_scope = mode & (ENV_LOCAL | ENV_GLOBAL | ENV_UNIVERSAL);
        local = !has_scope || (mode & ENV_LOCAL);
        global = !has_scope || (mode & ENV_GLOBAL);
        universal = !has_scope || (mode & ENV_UNIVERSAL);

        has_export_unexport = mode & (ENV_EXPORT | ENV_UNEXPORT);
        exports = !has_export_unexport || (mode & ENV_EXPORT);
        unexports = !has_export_unexport || (mode & ENV_UNEXPORT);

        // note we don't use pathvar for searches, so these don't default to true if unspecified.
        has_pathvar_unpathvar = mode & (ENV_PATHVAR | ENV_UNPATHVAR);
        pathvar = mode & ENV_PATHVAR;
        unpathvar = mode & ENV_UNPATHVAR;

        user = mode & ENV_USER;
    }

    bool export_matches(const env_var_t &var) const {
        if (has_export_unexport) {
            return var.exports() ? exports : unexports;
        } else {
            return true;
        }
    }
};

// Struct representing one level in the function variable stack.
class env_node_t {
   public:
    /// Variable table.
    var_table_t env;
    /// Does this node imply a new variable scope? If yes, all non-global variables below this one
    /// in the stack are invisible. If new_scope is set for the global variable node, the universe
    /// will explode.
    const bool new_scope;
    /// The export generation. If this is nonzero, then we contain a variable that is exported to
    /// subshells, or redefines a variable to not be exported.
    export_generation_t export_gen = 0;
    /// Pointer to next level.
    const std::shared_ptr<env_node_t> next;

    env_node_t(bool is_new_scope, std::shared_ptr<env_node_t> next_scope)
        : new_scope(is_new_scope), next(std::move(next_scope)) {}

    maybe_t<env_var_t> find_entry(const wcstring &key) {
        auto it = env.find(key);
        if (it != env.end()) return it->second;
        return none();
    }

    bool exports() const { return export_gen > 0; }

    void changed_exported() { export_gen = next_export_generation(); }
};
}  // namespace

using env_node_ref_t = std::shared_ptr<env_node_t>;
class env_scoped_impl_t : public environment_t {
    /// A struct wrapping up parser-local variables. These are conceptually variables that differ in
    /// different fish internal processes.
    struct perproc_data_t {
        wcstring pwd{};
        statuses_t statuses{statuses_t::just(0)};
    };

   public:
    env_scoped_impl_t(env_node_ref_t locals, env_node_ref_t globals)
        : locals_(std::move(locals)), globals_(std::move(globals)) {
        assert(locals_ && globals_ && "Nodes cannot be null");
    }

    maybe_t<env_var_t> get(const wcstring &key, env_mode_flags_t mode = ENV_DEFAULT) const override;
    wcstring_list_t get_names(int flags) const override;

    perproc_data_t &perproc_data() { return perproc_data_; }
    const perproc_data_t &perproc_data() const { return perproc_data_; }

    std::shared_ptr<environment_t> snapshot() const;

    ~env_scoped_impl_t() override = default;

    std::shared_ptr<const null_terminated_array_t<char>> export_array();

    env_scoped_impl_t(env_scoped_impl_t &&) = delete;
    env_scoped_impl_t(const env_scoped_impl_t &) = delete;
    void operator=(env_scoped_impl_t &&) = delete;
    void operator=(const env_scoped_impl_t &) = delete;

   protected:
    // A linked list of scopes.
    env_node_ref_t locals_{};

    // Global scopes. There is no parent here.
    env_node_ref_t globals_{};

    // Per process data.
    perproc_data_t perproc_data_{};

    // Exported variable array used by execv.
    std::shared_ptr<const null_terminated_array_t<char>> export_array_{};

    // Cached list of export generations corresponding to the above export_array_.
    // If this differs from the current export generations then we need to regenerate the array.
    std::vector<export_generation_t> export_array_generations_{};

   private:
    // These "try" methods return true on success, false on failure. On a true return, \p result is
    // populated. A maybe_t<maybe_t<...>> is a bridge too far.
    // These may populate result with none() if a variable is present which does not match the
    // query.
    maybe_t<env_var_t> try_get_computed(const wcstring &key) const;
    maybe_t<env_var_t> try_get_local(const wcstring &key) const;
    maybe_t<env_var_t> try_get_global(const wcstring &key) const;
    maybe_t<env_var_t> try_get_universal(const wcstring &key) const;

    /// Invoke a function on the current (nonzero) export generations, in order.
    template <typename Func>
    void enumerate_generations(const Func &func) const {
        // Our uvars generation count doesn't come from next_export_generation(), so always supply
        // it even if it's 0.
        func(uvars() ? uvars()->get_export_generation() : 0);
        if (globals_->exports()) func(globals_->export_gen);
        for (auto node = locals_; node; node = node->next) {
            if (node->exports()) func(node->export_gen);
        }
    }

    /// \return whether the current export array is empty or out-of-date.
    bool export_array_needs_regeneration() const;

    /// \return a newly allocated export array.
    std::shared_ptr<const null_terminated_array_t<char>> create_export_array() const;
};

/// Get the exported variables into a variable table.
static void get_exported(const env_node_ref_t &n, var_table_t &table) {
    if (!n) return;

    // Allow parent scopes to populate first, since we may want to overwrite those results.
    get_exported(n->next, table);

    for (const auto &kv : n->env) {
        const wcstring &key = kv.first;
        const env_var_t &var = kv.second;
        if (var.exports()) {
            // Export the variable. Don't use std::map::insert here, since we need to overwrite
            // existing values from previous scopes.
            table[key] = var;
        } else {
            // We need to erase from the map if we are not exporting, since a lower scope may have
            // exported. See #2132.
            table.erase(key);
        }
    }
}

bool env_scoped_impl_t::export_array_needs_regeneration() const {
    // Check if our export array is stale. If we don't have one, it's obviously stale. Otherwise,
    // compare our cached generations with the current generations. If they don't match exactly then
    // our generation list is stale.
    if (!export_array_) return true;

    bool mismatch = false;
    auto cursor = export_array_generations_.begin();
    auto end = export_array_generations_.end();
    enumerate_generations([&](export_generation_t gen) {
        if (cursor != end && *cursor == gen) {
            ++cursor;
        } else {
            mismatch = true;
        }
    });
    if (cursor != end) {
        mismatch = true;
    }
    return mismatch;
}

std::shared_ptr<const null_terminated_array_t<char>> env_scoped_impl_t::create_export_array()
    const {
    var_table_t table;

    FLOG(env_export, L"create_export_array() recalc");
    var_table_t vals;
    get_exported(this->globals_, vals);
    get_exported(this->locals_, vals);

    if (uvars()) {
        const wcstring_list_t uni = uvars()->get_names(true, false);
        for (const wcstring &key : uni) {
            auto var = uvars()->get(key);
            assert(var && "Variable should be present in uvars");
            // Note that std::map::insert does NOT overwrite a value already in the map,
            // which we depend on here.
            // Note: Using std::move around make_pair prevents the compiler from implementing
            // copy elision.
            vals.insert(std::make_pair(key, std::move(*var)));
        }
    }

    // Dorky way to add our single exported computed variable.
    vals[L"PWD"] = env_var_t(L"PWD", perproc_data().pwd);

    // Construct the export list: a list of strings of the form key=value.
    std::vector<std::string> export_list;
    export_list.reserve(vals.size());
    for (const auto &kv : vals) {
        std::string str = wcs2string(kv.first);
        str.push_back('=');
        str.append(wcs2string(kv.second.as_string()));
        export_list.push_back(std::move(str));
    }
    return std::make_shared<null_terminated_array_t<char>>(export_list);
}

std::shared_ptr<const null_terminated_array_t<char>> env_scoped_impl_t::export_array() {
    ASSERT_IS_NOT_FORKED_CHILD();
    if (export_array_needs_regeneration()) {
        export_array_ = create_export_array();

        // Update our export array generations.
        export_array_generations_.clear();
        enumerate_generations(
            [this](export_generation_t gen) { export_array_generations_.push_back(gen); });
    }
    return export_array_;
}

maybe_t<env_var_t> env_scoped_impl_t::try_get_computed(const wcstring &key) const {
    const electric_var_t *ev = electric_var_t::for_name(key);
    if (!(ev && ev->computed())) {
        return none();
    }
    if (key == L"PWD") {
        return env_var_t(perproc_data().pwd, env_var_t::flag_export);
    } else if (key == L"history") {
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
        return env_var_t(L"history", std::move(result));
    } else if (key == L"pipestatus") {
        const auto &js = perproc_data().statuses;
        wcstring_list_t result;
        result.reserve(js.pipestatus.size());
        for (int i : js.pipestatus) {
            result.push_back(to_string(i));
        }
        return env_var_t(L"pipestatus", std::move(result));
    } else if (key == L"status") {
        const auto &js = perproc_data().statuses;
        return env_var_t(L"status", to_string(js.status));
    } else if (key == L"status_generation") {
        auto status_generation = reader_status_count();
        return env_var_t(L"status_generation", to_string(status_generation));
    } else if (key == L"fish_kill_signal") {
        const auto &js = perproc_data().statuses;
        return env_var_t(L"fish_kill_signal", to_string(js.kill_signal));
    } else if (key == L"umask") {
        // note umask() is an absurd API: you call it to set the value and it returns the old
        // value. Thus we have to call it twice, to reset the value. The env_lock protects
        // against races. Guess what the umask is; if we guess right we don't need to reset it.
        mode_t guess = 022;
        mode_t res = umask(guess);
        if (res != guess) umask(res);
        return env_var_t(L"umask", format_string(L"0%0.3o", res));
    }
    // We should never get here unless the electric var list is out of sync with the above code.
    DIE("unrecognized computed var name");
}

maybe_t<env_var_t> env_scoped_impl_t::try_get_local(const wcstring &key) const {
    auto cursor = locals_;
    while (cursor) {
        auto where = cursor->env.find(key);
        if (where != cursor->env.end()) {
            return where->second;
        }
        cursor = cursor->next;
    }
    return none();
}

maybe_t<env_var_t> env_scoped_impl_t::try_get_global(const wcstring &key) const {
    auto where = globals_->env.find(key);
    if (where != globals_->env.end()) {
        return where->second;
    }
    return none();
}

maybe_t<env_var_t> env_scoped_impl_t::try_get_universal(const wcstring &key) const {
    if (!uvars()) return none();
    auto var = uvars()->get(key);
    if (var) {
        return var;
    }
    return none();
}

maybe_t<env_var_t> env_scoped_impl_t::get(const wcstring &key, env_mode_flags_t mode) const {
    const query_t query(mode);

    maybe_t<env_var_t> result;
    // Computed variables are effectively global and can't be shadowed.
    if (query.global) {
        result = try_get_computed(key);
    }

    if (!result && query.local) {
        result = try_get_local(key);
    }
    if (!result && query.global) {
        result = try_get_global(key);
    }
    if (!result && query.universal) {
        result = try_get_universal(key);
    }
    // If the user requested only exported or unexported variables, enforce that here.
    if (result && !query.export_matches(*result)) {
        result = none();
    }
    return result;
}

wcstring_list_t env_scoped_impl_t::get_names(int flags) const {
    const query_t query(flags);
    std::set<wcstring> names;

    // Helper to add the names of variables from \p envs to names, respecting show_exported and
    // show_unexported.
    auto add_keys = [&](const var_table_t &envs) {
        for (const auto &kv : envs) {
            if (query.export_matches(kv.second)) {
                names.insert(kv.first);
            }
        }
    };

    if (query.local) {
        for (auto cursor = locals_; cursor != nullptr; cursor = cursor->next) {
            add_keys(cursor->env);
        }
    }

    if (query.global) {
        add_keys(globals_->env);
        // Add electrics.
        for (const auto &ev : electric_variables) {
            if (ev.exports() ? query.exports : query.unexports) {
                names.insert(ev.name);
            }
        }
    }

    if (query.universal && uvars()) {
        const wcstring_list_t uni_list = uvars()->get_names(query.exports, query.unexports);
        names.insert(uni_list.begin(), uni_list.end());
    }

    return wcstring_list_t(names.begin(), names.end());
}

/// Recursive helper to snapshot a series of nodes.
static env_node_ref_t copy_node_chain(const env_node_ref_t &node) {
    if (node == nullptr) {
        return nullptr;
    }

    auto next = copy_node_chain(node->next);
    auto result = std::make_shared<env_node_t>(node->new_scope, next);
    // Copy over variables.
    // Note assigning env is a potentially big copy.
    result->export_gen = node->export_gen;
    result->env = node->env;
    return result;
}

std::shared_ptr<environment_t> env_scoped_impl_t::snapshot() const {
    auto ret = std::make_shared<env_scoped_impl_t>(copy_node_chain(locals_), globals_);
    ret->perproc_data_ = this->perproc_data_;
    return ret;
}

// A struct that wraps up the result of setting or removing a variable.
struct mod_result_t {
    // The publicly visible status of the set call.
    int status{ENV_OK};

    // Whether we modified the global scope.
    bool global_modified{false};

    // Whether we modified universal variables.
    bool uvar_modified{false};

    explicit mod_result_t(int status) : status(status) {}
};

/// A mutable subclass of env_scoped_impl_t.
class env_stack_impl_t final : public env_scoped_impl_t {
   public:
    using env_scoped_impl_t::env_scoped_impl_t;

    /// Set a variable under the name \p key, using the given \p mode, setting its value to \p val.
    mod_result_t set(const wcstring &key, env_mode_flags_t mode, wcstring_list_t val);

    /// Remove a variable under the name \p key.
    mod_result_t remove(const wcstring &key, int var_mode);

    /// Push a new shadowing local scope.
    void push_shadowing();

    /// Push a new non-shadowing (inner) local scope.
    void push_nonshadowing();

    /// Pop the variable stack.
    /// \return the popped node.
    env_node_ref_t pop();

    /// \return a new impl representing global variables, with a single local scope.
    static std::unique_ptr<env_stack_impl_t> create() {
        static const auto s_global_node = std::make_shared<env_node_t>(false, nullptr);
        auto local = std::make_shared<env_node_t>(false, nullptr);
        return make_unique<env_stack_impl_t>(std::move(local), s_global_node);
    }

    ~env_stack_impl_t() override = default;

   private:
    /// The scopes of caller functions, which are currently shadowed.
    std::vector<env_node_ref_t> shadowed_locals_;

    /// A restricted set of variable flags.
    struct var_flags_t {
        // if set, whether we should become a path variable; otherwise guess based on the name.
        maybe_t<bool> pathvar{};

        // if set, the new export value; otherwise inherit any existing export value.
        maybe_t<bool> exports{};

        // whether the variable is exported by some parent.
        bool parent_exports{};
    };

    /// Find the first node in the chain starting at \p node which contains the given key \p key.
    static env_node_ref_t find_in_chain(const env_node_ref_t &node, const wcstring &key) {
        for (auto cursor = node; cursor; cursor = cursor->next) {
            if (cursor->env.count(key)) {
                return cursor;
            }
        }
        return nullptr;
    }

    /// Remove a variable from the chain \p node.
    /// \return true if the variable was found and removed.
    bool remove_from_chain(const env_node_ref_t &node, const wcstring &key) const {
        for (auto cursor = node; cursor; cursor = cursor->next) {
            auto iter = cursor->env.find(key);
            if (iter != cursor->env.end()) {
                if (iter->second.exports()) {
                    node->changed_exported();
                }
                cursor->env.erase(iter);
                return true;
            }
        }
        return false;
    }

    /// Try setting\p key as an electric or readonly variable.
    /// \return an error code, or none() if not an electric or readonly variable.
    /// \p val will not be modified upon a none() return.
    maybe_t<int> try_set_electric(const wcstring &key, const query_t &query, wcstring_list_t &val);

    /// Set a universal value.
    void set_universal(const wcstring &key, wcstring_list_t val, const query_t &query);

    /// Set a variable in a given node \p node.
    void set_in_node(const env_node_ref_t &node, const wcstring &key, wcstring_list_t &&val,
                     const var_flags_t &flags);

    // Implement the default behavior of 'set' by finding the node for an unspecified scope.
    env_node_ref_t resolve_unspecified_scope() {
        for (auto cursor = locals_; cursor; cursor = cursor->next) {
            if (cursor->new_scope) return cursor;
        }
        return globals_;
    }

    /// Get a pointer to an existing variable, or nullptr.
    /// This is used for inheriting pathvar and export status.
    const env_var_t *find_variable(const wcstring &key) const {
        env_node_ref_t node = find_in_chain(locals_, key);
        if (!node) node = find_in_chain(globals_, key);
        if (node) {
            auto iter = node->env.find(key);
            assert(iter != node->env.end() && "Node should contain key");
            return &iter->second;
        }
        return nullptr;
    }
};

void env_stack_impl_t::push_nonshadowing() {
    locals_ = std::make_shared<env_node_t>(false, locals_);
}

void env_stack_impl_t::push_shadowing() {
    // Propagate local exported variables.
    auto node = std::make_shared<env_node_t>(true, nullptr);
    for (auto cursor = locals_; cursor; cursor = cursor->next) {
        for (const auto &var : cursor->env) {
            if (var.second.exports()) {
                node->env.insert(var);
                node->changed_exported();
            }
        }
    }
    this->shadowed_locals_.push_back(std::move(locals_));
    this->locals_ = std::move(node);
}

env_node_ref_t env_stack_impl_t::pop() {
    auto popped = std::move(locals_);
    if (popped->next) {
        // Pop the inner scope.
        locals_ = popped->next;
    } else {
        // Exhausted the inner scopes, put back a shadowing scope.
        assert(!shadowed_locals_.empty() && "Attempt to pop last local scope");
        locals_ = std::move(shadowed_locals_.back());
        shadowed_locals_.pop_back();
    }
    assert(locals_ && "Attempt to pop first local scope");
    return popped;
}

/// Apply the pathvar behavior, splitting about colons.
static wcstring_list_t colon_split(const wcstring_list_t &val) {
    wcstring_list_t split_val;
    split_val.reserve(val.size());
    for (const wcstring &str : val) {
        vec_append(split_val, split_string(str, PATH_ARRAY_SEP));
    }
    return split_val;
}

void env_stack_impl_t::set_in_node(const env_node_ref_t &node, const wcstring &key,
                                   wcstring_list_t &&val, const var_flags_t &flags) {
    env_var_t &var = node->env[key];

    // Use an explicit exports, or inherit from the existing variable.
    bool res_exports = flags.exports.has_value() ? *flags.exports : var.exports();

    // Pathvar is inferred from the name. If set, split our entry about colons.
    bool res_pathvar =
        flags.pathvar.has_value() ? *flags.pathvar : variable_should_auto_pathvar(key);
    if (res_pathvar) {
        val = colon_split(val);
    }

    var = var.setting_vals(std::move(val))
              .setting_exports(res_exports)
              .setting_pathvar(res_pathvar)
              .setting_read_only(is_read_only(key));

    // Perhaps mark that this node contains an exported variable, or shadows an exported variable.
    // If so regenerate the export list.
    if (res_exports || flags.parent_exports) {
        node->changed_exported();
    }
}

maybe_t<int> env_stack_impl_t::try_set_electric(const wcstring &key, const query_t &query,
                                                wcstring_list_t &val) {
    const electric_var_t *ev = electric_var_t::for_name(key);
    if (!ev) {
        return none();
    }

    // If a variable is electric, it may only be set in the global scope.
    if (query.has_scope && !query.global) {
        return ENV_SCOPE;
    }

    // If the variable is read-only, the user may not set it.
    if (query.user && ev->readonly()) {
        return ENV_PERM;
    }

    // Be picky about exporting.
    if (query.has_export_unexport) {
        if (ev->exports() ? query.unexports : query.exports) {
            return ENV_SCOPE;
        }
    }

    // Handle computed mutable electric variables.
    if (key == L"umask") {
        return set_umask(val);
    } else if (key == L"PWD") {
        assert(val.size() == 1 && "Should have exactly one element in PWD");
        wcstring &pwd = val.front();
        if (pwd != perproc_data().pwd) {
            perproc_data().pwd = std::move(pwd);
            globals_->changed_exported();
        }
        return ENV_OK;
    }

    // Decide on the mode and set it in the global scope.
    var_flags_t flags{};
    flags.exports = ev->exports();
    flags.parent_exports = ev->exports();
    flags.pathvar = false;
    set_in_node(globals_, key, std::move(val), flags);
    return ENV_OK;
}

/// Set a universal variable, inheriting as applicable from the given old variable.
void env_stack_impl_t::set_universal(const wcstring &key, wcstring_list_t val,
                                     const query_t &query) {
    ASSERT_IS_MAIN_THREAD();
    if (!uvars()) return;
    auto oldvar = uvars()->get(key);
    // Resolve whether or not to export.
    bool exports = false;
    if (query.has_export_unexport) {
        exports = query.exports;
    } else if (oldvar) {
        exports = oldvar->exports();
    }

    // Resolve whether to be a path variable.
    // Here we fall back to the auto-pathvar behavior.
    bool pathvar = false;
    if (query.has_pathvar_unpathvar) {
        pathvar = query.pathvar;
    } else if (oldvar) {
        pathvar = oldvar->is_pathvar();
    } else {
        pathvar = variable_should_auto_pathvar(key);
    }

    // Split about ':' if it's a path variable.
    if (pathvar) {
        wcstring_list_t split_val;
        for (const wcstring &str : val) {
            vec_append(split_val, split_string(str, PATH_ARRAY_SEP));
        }
        val = std::move(split_val);
    }

    // Construct and set the new variable.
    env_var_t::env_var_flags_t varflags = 0;
    if (exports) varflags |= env_var_t::flag_export;
    if (pathvar) varflags |= env_var_t::flag_pathvar;
    env_var_t new_var{val, varflags};

    uvars()->set(key, new_var);
}

mod_result_t env_stack_impl_t::set(const wcstring &key, env_mode_flags_t mode,
                                   wcstring_list_t val) {
    const query_t query(mode);
    // Handle electric and read-only variables.
    if (auto ret = try_set_electric(key, query, val)) {
        return mod_result_t{*ret};
    }

    // Resolve as much of our flags as we can. Note these contain maybes, and we may defer the final
    // decision until the set_in_node call. Also note that we only inherit pathvar, not export. For
    // example, if you have a global exported variable, a local variable with the same name will not
    // automatically be exported. But if you have a global pathvar, a local variable with the same
    // name will be a pathvar. This is historical.
    var_flags_t flags{};
    if (const env_var_t *existing = find_variable(key)) {
        flags.pathvar = existing->is_pathvar();
        flags.parent_exports = existing->exports();
    }
    if (query.has_export_unexport) {
        flags.exports = query.exports;
    }
    if (query.has_pathvar_unpathvar) {
        flags.pathvar = query.pathvar;
    }

    mod_result_t result{ENV_OK};
    if (query.has_scope) {
        // The user requested a particular scope.
        if (query.universal) {
            set_universal(key, std::move(val), query);
            result.uvar_modified = true;
        } else if (query.global) {
            set_in_node(globals_, key, std::move(val), flags);
            result.global_modified = true;
        } else if (query.local) {
            assert(locals_ != globals_ && "Locals should not be globals");
            set_in_node(locals_, key, std::move(val), flags);
        } else {
            DIE("Unknown scope");
        }
    } else if (env_node_ref_t node = find_in_chain(locals_, key)) {
        // Existing local variable.
        set_in_node(node, key, std::move(val), flags);
    } else if (env_node_ref_t node = find_in_chain(globals_, key)) {
        // Existing global variable.
        set_in_node(node, key, std::move(val), flags);
        result.global_modified = true;
    } else if (uvars() && uvars()->get(key)) {
        // Existing universal variable.
        set_universal(key, std::move(val), query);
        result.uvar_modified = true;
    } else {
        // Unspecified scope with no existing variables.
        auto node = resolve_unspecified_scope();
        assert(node && "Should always resolve some scope");
        set_in_node(node, key, std::move(val), flags);
        result.global_modified = (node == globals_);
    }
    return result;
}

mod_result_t env_stack_impl_t::remove(const wcstring &key, int mode) {
    const query_t query(mode);

    // Users can't remove read-only keys.
    if (query.user && is_read_only(key)) {
        return mod_result_t{ENV_SCOPE};
    }

    // Helper to remove from uvars.
    auto remove_from_uvars = [&] { return uvars() && uvars()->remove(key); };

    mod_result_t result{ENV_OK};
    if (query.has_scope) {
        // The user requested erasing from a particular scope.
        if (query.universal) {
            result.status = remove_from_uvars() ? ENV_OK : ENV_NOT_FOUND;
            result.uvar_modified = true;
        } else if (query.global) {
            result.status = remove_from_chain(globals_, key) ? ENV_OK : ENV_NOT_FOUND;
            result.global_modified = true;
        } else if (query.local) {
            result.status = remove_from_chain(locals_, key) ? ENV_OK : ENV_NOT_FOUND;
        } else {
            DIE("Unknown scope");
        }
    } else if (remove_from_chain(locals_, key)) {
        // pass
    } else if (remove_from_chain(globals_, key)) {
        result.global_modified = true;
    } else if (remove_from_uvars()) {
        result.uvar_modified = true;
    } else {
        result.status = ENV_NOT_FOUND;
    }
    return result;
}

bool env_stack_t::universal_barrier() {
    // Only perform universal barriers for the principal env stack.
    // This means that changes from other fish processes will only be visible when the "main thread
    // runs."
    if (!is_principal()) return false;

    ASSERT_IS_MAIN_THREAD();
    if (!uvars()) return false;

    callback_data_list_t callbacks;
    bool changed = uvars()->sync(callbacks);
    if (changed) {
        universal_notifier_t::default_notifier().post_notification();
    }

    env_universal_callbacks(this, callbacks);
    return changed || !callbacks.empty();
}

statuses_t env_stack_t::get_last_statuses() const {
    return acquire_impl()->perproc_data().statuses;
}

int env_stack_t::get_last_status() const { return acquire_impl()->perproc_data().statuses.status; }

void env_stack_t::set_last_statuses(statuses_t s) {
    acquire_impl()->perproc_data().statuses = std::move(s);
}

/// Update the PWD variable directory from the result of getcwd().
void env_stack_t::set_pwd_from_getcwd() {
    wcstring cwd = wgetcwd();
    if (cwd.empty()) {
        FLOG(error,
             _(L"Could not determine current working directory. Is your locale set correctly?"));
        return;
    }
    set_one(L"PWD", ENV_EXPORT | ENV_GLOBAL, cwd);
}

env_stack_t::env_stack_t(std::unique_ptr<env_stack_impl_t> impl) : impl_(std::move(impl)) {}

acquired_lock<env_stack_impl_t> env_stack_t::acquire_impl() {
    return acquired_lock<env_stack_impl_t>::from_global(env_lock, impl_.get());
}

acquired_lock<const env_stack_impl_t> env_stack_t::acquire_impl() const {
    return acquired_lock<const env_stack_impl_t>::from_global(env_lock, impl_.get());
}

maybe_t<env_var_t> env_stack_t::get(const wcstring &key, env_mode_flags_t mode) const {
    return acquire_impl()->get(key, mode);
}

wcstring_list_t env_stack_t::get_names(int flags) const { return acquire_impl()->get_names(flags); }

int env_stack_t::set(const wcstring &key, env_mode_flags_t mode, wcstring_list_t vals,
                     std::vector<event_t> *out_events) {
    // Historical behavior.
    if (vals.size() == 1 && (key == L"PWD" || key == L"HOME")) {
        path_make_canonical(vals.front());
    }

    // Hacky stuff around PATH and CDPATH: #3914.
    // Not MANPATH; see #4158.
    // Replace empties with dot. Note we ignore pathvar here.
    if (key == L"PATH" || key == L"CDPATH") {
        auto munged_vals = colon_split(vals);
        std::replace(munged_vals.begin(), munged_vals.end(), wcstring(L""), wcstring(L"."));
        vals = std::move(munged_vals);
    }

    mod_result_t ret = acquire_impl()->set(key, mode, std::move(vals));
    if (ret.status == ENV_OK) {
        // If we modified the global state, or we are principal, then dispatch changes.
        // Important to not hold the lock here.
        if (ret.global_modified || is_principal()) {
            env_dispatch_var_change(key, *this);
        }
        if (out_events) {
            out_events->push_back(event_t::variable(key, {L"VARIABLE", L"SET", key}));
        }
    }
    // If the principal stack modified universal variables, then post a barrier.
    if (ret.uvar_modified && is_principal()) {
        universal_barrier();
    }
    return ret.status;
}

int env_stack_t::set_one(const wcstring &key, env_mode_flags_t mode, wcstring val,
                         std::vector<event_t> *out_events) {
    wcstring_list_t vals;
    vals.push_back(std::move(val));
    return set(key, mode, std::move(vals), out_events);
}

int env_stack_t::set_empty(const wcstring &key, env_mode_flags_t mode,
                           std::vector<event_t> *out_events) {
    return set(key, mode, {}, out_events);
}

int env_stack_t::remove(const wcstring &key, int mode, std::vector<event_t> *out_events) {
    mod_result_t ret = acquire_impl()->remove(key, mode);
    if (ret.status == ENV_OK) {
        if (ret.global_modified || is_principal()) {
            // Important to not hold the lock here.
            env_dispatch_var_change(key, *this);
        }
        if (out_events) {
            out_events->push_back(event_t::variable(key, {L"VARIABLE", L"ERASE", key}));
        }
    }
    if (ret.uvar_modified && is_principal()) {
        universal_barrier();
    }
    return ret.status;
}

std::shared_ptr<const null_terminated_array_t<char>> env_stack_t::export_arr() {
    return acquire_impl()->export_array();
}

std::shared_ptr<environment_t> env_stack_t::snapshot() const { return acquire_impl()->snapshot(); }

void env_stack_t::set_argv(wcstring_list_t argv) { set(L"argv", ENV_LOCAL, std::move(argv)); }

wcstring env_stack_t::get_pwd_slash() const {
    wcstring pwd = acquire_impl()->perproc_data().pwd;
    if (!string_suffixes_string(L"/", pwd)) {
        pwd.push_back(L'/');
    }
    return pwd;
}

void env_stack_t::push(bool new_scope) {
    auto impl = acquire_impl();
    if (new_scope) {
        impl->push_shadowing();
    } else {
        impl->push_nonshadowing();
    }
}

void env_stack_t::pop() {
    auto popped = acquire_impl()->pop();
    // Only dispatch variable changes if we are the principal environment.
    if (this == principal_ref().get()) {
        // TODO: we would like to coalesce locale / curses changes, so that we only re-initialize
        // once.
        for (const auto &kv : popped->env) {
            env_dispatch_var_change(kv.first, *this);
        }
    }
}

env_stack_t &env_stack_t::globals() {
    static env_stack_t s_globals(env_stack_impl_t::create());
    return s_globals;
}

const std::shared_ptr<env_stack_t> &env_stack_t::principal_ref() {
    static const std::shared_ptr<env_stack_t> s_principal{
        new env_stack_t(env_stack_impl_t::create())};
    return s_principal;
}

env_stack_t::~env_stack_t() = default;

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
    if (dir != nullptr && check_runtime_path(dir) == 0) {
        result = str2wcstring(dir);
    } else {
        // Don't rely on $USER being set, as setup_user() has not yet been called.
        // See https://github.com/fish-shell/fish-shell/issues/5180
        // getpeuid() can't fail, but getpwuid sure can.
        auto pwuid = getpwuid(geteuid());
        const char *uname = pwuid ? pwuid->pw_name : nullptr;
        // /tmp/fish.user
        std::string tmpdir = get_path_to_tmp_dir() + "/fish.";
        if (uname) {
            tmpdir.append(uname);
        }

        if (!uname || check_runtime_path(tmpdir.c_str()) != 0) {
            FLOG(error, L"Runtime path not available.");
            FLOGF(error, L"Try deleting the directory %s and restarting fish.", tmpdir.c_str());
            return result;
        }

        result = str2wcstring(tmpdir);
    }
    return result;
}

static std::mutex s_setenv_lock{};

void setenv_lock(const char *name, const char *value, int overwrite) {
    scoped_lock locker(s_setenv_lock);
    setenv(name, value, overwrite);
}

void unsetenv_lock(const char *name) {
    scoped_lock locker(s_setenv_lock);
    unsetenv(name);
}
