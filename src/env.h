// Prototypes for functions for manipulating fish script variables.
#ifndef FISH_ENV_H
#define FISH_ENV_H

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common.h"
#include "cxx.h"
#include "maybe.h"
#include "wutil.h"

#if INCLUDE_RUST_HEADERS
#include "env/env_ffi.rs.h"
#else
struct EnvVar;
#endif

struct owning_null_terminated_array_t;

extern size_t read_byte_limit;
extern bool curses_initialized;

struct Event;

// Flags that may be passed as the 'mode' in env_stack_t::set() / environment_t::get().
enum : uint16_t {
    /// Default mode. Used with `env_stack_t::get()` to indicate the caller doesn't care what scope
    /// the var is in or whether it is exported or unexported.
    ENV_DEFAULT = 0,
    /// Flag for local (to the current block) variable.
    ENV_LOCAL = 1 << 0,
    ENV_FUNCTION = 1 << 1,
    /// Flag for global variable.
    ENV_GLOBAL = 1 << 2,
    /// Flag for universal variable.
    ENV_UNIVERSAL = 1 << 3,
    /// Flag for exported (to commands) variable.
    ENV_EXPORT = 1 << 4,
    /// Flag for unexported variable.
    ENV_UNEXPORT = 1 << 5,
    /// Flag to mark a variable as a path variable.
    ENV_PATHVAR = 1 << 6,
    /// Flag to unmark a variable as a path variable.
    ENV_UNPATHVAR = 1 << 7,
    /// Flag for variable update request from the user. All variable changes that are made directly
    /// by the user, such as those from the `read` and `set` builtin must have this flag set. It
    /// serves one purpose: to indicate that an error should be returned if the user is attempting
    /// to modify a var that should not be modified by direct user action; e.g., a read-only var.
    ENV_USER = 1 << 8,
};
using env_mode_flags_t = uint16_t;

/// Return values for `env_stack_t::set()`.
enum { ENV_OK, ENV_PERM, ENV_SCOPE, ENV_INVALID, ENV_NOT_FOUND };

/// A struct of configuration directories, determined in main() that fish will optionally pass to
/// env_init.
struct config_paths_t {
    wcstring data;     // e.g., /usr/local/share
    wcstring sysconf;  // e.g., /usr/local/etc
    wcstring doc;      // e.g., /usr/local/share/doc/fish
    wcstring bin;      // e.g., /usr/local/bin
};

/// A collection of status and pipestatus.
struct statuses_t {
    /// Status of the last job to exit.
    int status{0};

    /// Signal from the most recent process in the last job that was terminated by a signal.
    /// 0 if all processes exited normally.
    int kill_signal{0};

    /// Pipestatus value.
    std::vector<int> pipestatus{};

    /// Return a statuses for a single process status.
    static statuses_t just(int s) {
        statuses_t result{};
        result.status = s;
        result.pipestatus.push_back(s);
        return result;
    }
};

/// Initialize environment variable data.
void env_init(const struct config_paths_t *paths = nullptr, bool do_uvars = true,
              bool default_paths = false);

/// Various things we need to initialize at run-time that don't really fit any of the other init
/// routines.
void misc_init();

/// env_var_t is an immutable value-type data structure representing the value of an environment
/// variable.
class env_var_t {
   public:
    using env_var_flags_t = uint8_t;

   public:
    enum {
        flag_export = 1 << 0,     // whether the variable is exported
        flag_read_only = 1 << 1,  // whether the variable is read only
        flag_pathvar = 1 << 2,    // whether the variable is a path variable
    };

    // Constructors.
    env_var_t() : env_var_t{std::vector<wcstring>{}, 0} {}
    env_var_t(const env_var_t &);
    env_var_t(env_var_t &&) = default;

    env_var_t(std::vector<wcstring> vals, env_var_flags_t flags);
    env_var_t(wcstring val, env_var_flags_t flags)
        : env_var_t{std::vector<wcstring>{std::move(val)}, flags} {}

    // Constructors that infer the flags from a name.
    env_var_t(const wchar_t *name, std::vector<wcstring> vals);
    env_var_t(const wchar_t *name, wcstring val)
        : env_var_t{name, std::vector<wcstring>{std::move(val)}} {}

    // Construct from FFI. This transfers ownership of the EnvVar, which should originate
    // in Box::into_raw().
    static env_var_t new_ffi(EnvVar *ptr);

    // Get the underlying EnvVar pointer.
    // Note you may need to mem::transmute this, since autocxx gets confused when going from Rust ->
    // C++ -> Rust.
    const EnvVar *ffi_ptr() const { return &*this->impl_; }

    bool empty() const;
    bool exports() const;
    bool is_read_only() const;
    bool is_pathvar() const;
    env_var_flags_t get_flags() const;

    wcstring as_string() const;
    void to_list(std::vector<wcstring> &out) const;
    std::vector<wcstring> as_list() const;
    wcstring_list_ffi_t as_list_ffi() const { return as_list(); }

    /// \return the character used when delimiting quoted expansion.
    wchar_t get_delimiter() const;

    /// \return a copy of this variable with new values.
    env_var_t setting_vals(std::vector<wcstring> vals) const {
        return env_var_t{std::move(vals), get_flags()};
    }

    env_var_t setting_exports(bool exportv) const {
        env_var_flags_t flags = get_flags();
        if (exportv) {
            flags |= flag_export;
        } else {
            flags &= ~flag_export;
        }
        return env_var_t{as_list(), flags};
    }

    env_var_t setting_pathvar(bool pathvar) const {
        env_var_flags_t flags = get_flags();
        if (pathvar) {
            flags |= flag_pathvar;
        } else {
            flags &= ~flag_pathvar;
        }
        return env_var_t{as_list(), flags};
    }

    static env_var_flags_t flags_for(const wchar_t *name);
    static std::shared_ptr<const std::vector<wcstring>> empty_list();

    env_var_t &operator=(const env_var_t &);
    env_var_t &operator=(env_var_t &&) = default;

    bool operator==(const env_var_t &rhs) const;
    bool operator!=(const env_var_t &rhs) const { return !(*this == rhs); }

   private:
    env_var_t(rust::Box<EnvVar> &&impl) : impl_(std::move(impl)) {}

    rust::Box<EnvVar> impl_;
};
typedef std::unordered_map<wcstring, env_var_t> var_table_t;

/// An environment is read-only access to variable values.
class environment_t {
   protected:
    environment_t() = default;

   public:
    virtual maybe_t<env_var_t> get(const wcstring &key,
                                   env_mode_flags_t mode = ENV_DEFAULT) const = 0;
    virtual std::vector<wcstring> get_names(env_mode_flags_t flags) const = 0;
    virtual ~environment_t();

    maybe_t<env_var_t> get_unless_empty(const wcstring &key,
                                        env_mode_flags_t mode = ENV_DEFAULT) const;
    /// \return a environment variable as a unique pointer, or nullptr if none.
    std::unique_ptr<env_var_t> get_or_null(const wcstring &key,
                                           env_mode_flags_t mode = ENV_DEFAULT) const;

    /// Returns the PWD with a terminating slash.
    virtual wcstring get_pwd_slash() const;
};

/// The null environment contains nothing.
class null_environment_t : public environment_t {
   public:
    null_environment_t() = default;
    ~null_environment_t() override;

    maybe_t<env_var_t> get(const wcstring &key, env_mode_flags_t mode = ENV_DEFAULT) const override;
    std::vector<wcstring> get_names(env_mode_flags_t flags) const override;
};

/// A mutable environment which allows scopes to be pushed and popped.
class env_stack_impl_t;
class env_stack_t final : public environment_t {
    friend class parser_t;

    /// The implementation. Do not access this directly.
    std::unique_ptr<env_stack_impl_t> impl_;

    /// All environment stacks are guarded by a global lock.
    acquired_lock<env_stack_impl_t> acquire_impl();
    acquired_lock<const env_stack_impl_t> acquire_impl() const;

    explicit env_stack_t(std::unique_ptr<env_stack_impl_t> impl);

    /// \return whether we are the principal stack.
    bool is_principal() const { return this == principal_ref().get(); }

   public:
    ~env_stack_t() override;
    env_stack_t(env_stack_t &&);

    /// Implementation of environment_t.
    maybe_t<env_var_t> get(const wcstring &key, env_mode_flags_t mode = ENV_DEFAULT) const override;

    /// Implementation of environment_t.
    std::vector<wcstring> get_names(env_mode_flags_t flags) const override;

    /// Sets the variable with the specified name to the given values.
    int set(const wcstring &key, env_mode_flags_t mode, std::vector<wcstring> vals);

    /// Sets the variable with the specified name to the given values.
    /// The values should have type const wchar_t *const * (but autocxx doesn't support that).
    int set_ffi(const wcstring &key, env_mode_flags_t mode, const void *vals, size_t count);

    /// Sets the variable with the specified name to a single value.
    int set_one(const wcstring &key, env_mode_flags_t mode, wcstring val);

    /// Sets the variable with the specified name to no values.
    int set_empty(const wcstring &key, env_mode_flags_t mode);

    /// Update the PWD variable based on the result of getcwd.
    void set_pwd_from_getcwd();

    /// Remove environment variable.
    ///
    /// \param key The name of the variable to remove
    /// \param mode should be ENV_USER if this is a remove request from the user, 0 otherwise. If
    /// this is a user request, read-only variables can not be removed. The mode may also specify
    /// the scope of the variable that should be erased.
    ///
    /// \return zero if the variable existed, and non-zero if the variable did not exist
    int remove(const wcstring &key, int mode);

    /// Push the variable stack. Used for implementing local variables for functions and for-loops.
    void push(bool new_scope);

    /// Pop the variable stack. Used for implementing local variables for functions and for-loops.
    void pop();

    /// Returns an array containing all exported variables in a format suitable for execv.
    std::shared_ptr<owning_null_terminated_array_t> export_arr();

    /// Snapshot this environment. This means returning a read-only copy. Local variables are copied
    /// but globals are shared (i.e. changes to global will be visible to this snapshot). This
    /// returns a shared_ptr for convenience, since the most common reason to snapshot is because
    /// you want to read from another thread.
    std::shared_ptr<environment_t> snapshot() const;

    /// Helpers to get and set the proc statuses.
    /// These correspond to $status and $pipestatus.
    statuses_t get_last_statuses() const;
    int get_last_status() const;
    void set_last_statuses(statuses_t s);

    /// Sets up argv as the given list of strings.
    void set_argv(std::vector<wcstring> argv);

    /// Slightly optimized implementation.
    wcstring get_pwd_slash() const override;

    /// "Override" of get_or_null, as autocxx doesn't understand inheritance.
    std::unique_ptr<env_var_t> get_or_null(const wcstring &key,
                                           env_mode_flags_t mode = ENV_DEFAULT) const {
        return environment_t::get_or_null(key, mode);
    }

    /// Synchronizes universal variable changes.
    /// If \p always is set, perform synchronization even if there's no pending changes from this
    /// instance (that is, look for changes from other fish instances).
    /// \return a list of events for changed variables.
    std::vector<rust::Box<Event>> universal_sync(bool always);

    // Compatibility hack; access the "environment stack" from back when there was just one.
    static const std::shared_ptr<env_stack_t> &principal_ref();
    static env_stack_t &principal() { return *principal_ref(); }

    // Access a variable stack that only represents globals.
    // Do not push or pop from this.
    static env_stack_t &globals();
};

bool get_use_posix_spawn();

extern bool term_has_xn;  // does the terminal have the "eat_newline_glitch"

/// Returns true if we think the terminal supports setting its title.
bool term_supports_setting_title();

/// Gets a path appropriate for runtime storage
wcstring env_get_runtime_path();

/// A wrapper around setenv() and unsetenv() which use a lock.
/// In general setenv() and getenv() are highly incompatible with threads. This makes it only
/// slightly safer.
void setenv_lock(const char *name, const char *value, int overwrite);
void unsetenv_lock(const char *name);

/// Returns the originally inherited variables and their values.
/// This is a simple key->value map and not e.g. cut into paths.
const std::map<wcstring, wcstring> &env_get_inherited();

#endif
