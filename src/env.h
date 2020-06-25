// Prototypes for functions for manipulating fish script variables.
#ifndef FISH_ENV_H
#define FISH_ENV_H

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "common.h"
#include "maybe.h"
#include "null_terminated_array.h"

extern size_t read_byte_limit;
extern bool curses_initialized;

struct event_t;

// Flags that may be passed as the 'mode' in env_stack_t::set() / environment_t::get().
enum {
    /// Default mode. Used with `env_stack_t::get()` to indicate the caller doesn't care what scope
    /// the var is in or whether it is exported or unexported.
    ENV_DEFAULT = 0,
    /// Flag for local (to the current block) variable.
    ENV_LOCAL = 1 << 0,
    /// Flag for global variable.
    ENV_GLOBAL = 1 << 1,
    /// Flag for universal variable.
    ENV_UNIVERSAL = 1 << 2,
    /// Flag for exported (to commands) variable.
    ENV_EXPORT = 1 << 3,
    /// Flag for unexported variable.
    ENV_UNEXPORT = 1 << 4,
    /// Flag to mark a variable as a path variable.
    ENV_PATHVAR = 1 << 5,
    /// Flag to unmark a variable as a path variable.
    ENV_UNPATHVAR = 1 << 6,
    /// Flag for variable update request from the user. All variable changes that are made directly
    /// by the user, such as those from the `read` and `set` builtin must have this flag set. It
    /// serves one purpose: to indicate that an error should be returned if the user is attempting
    /// to modify a var that should not be modified by direct user action; e.g., a read-only var.
    ENV_USER = 1 << 7,
};
typedef uint32_t env_mode_flags_t;

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
void env_init(const struct config_paths_t *paths = nullptr);

/// Various things we need to initialize at run-time that don't really fit any of the other init
/// routines.
void misc_init();

/// env_var_t is an immutable value-type data structure representing the value of an environment
/// variable.
class env_var_t {
   public:
    using env_var_flags_t = uint8_t;

   private:
    env_var_t(std::shared_ptr<const wcstring_list_t> vals, env_var_flags_t flags)
        : vals_(std::move(vals)), flags_(flags) {}

    /// The list of values in this variable.
    /// shared_ptr allows for cheap copying.
    std::shared_ptr<const wcstring_list_t> vals_{empty_list()};

    /// Flag in this variable.
    env_var_flags_t flags_{};

   public:
    enum {
        flag_export = 1 << 0,     // whether the variable is exported
        flag_read_only = 1 << 1,  // whether the variable is read only
        flag_pathvar = 1 << 2,    // whether the variable is a path variable
    };

    // Constructors.
    env_var_t() = default;
    env_var_t(const env_var_t &) = default;
    env_var_t(env_var_t &&) = default;

    env_var_t(wcstring_list_t vals, env_var_flags_t flags)
        : env_var_t(std::make_shared<wcstring_list_t>(std::move(vals)), flags) {}

    env_var_t(wcstring val, env_var_flags_t flags)
        : env_var_t(wcstring_list_t{std::move(val)}, flags) {}

    // Constructors that infer the flags from a name.
    env_var_t(const wchar_t *name, wcstring_list_t vals)
        : env_var_t(std::move(vals), flags_for(name)) {}

    env_var_t(const wchar_t *name, wcstring val) : env_var_t(std::move(val), flags_for(name)) {}

    bool empty() const { return vals_->empty() || (vals_->size() == 1 && vals_->front().empty()); }
    bool read_only() const { return flags_ & flag_read_only; }
    bool exports() const { return flags_ & flag_export; }
    bool is_pathvar() const { return flags_ & flag_pathvar; }
    env_var_flags_t get_flags() const { return flags_; }

    wcstring as_string() const;
    void to_list(wcstring_list_t &out) const;
    const wcstring_list_t &as_list() const;

    /// \return the character used when delimiting quoted expansion.
    wchar_t get_delimiter() const;

    /// \return a copy of this variable with new values.
    env_var_t setting_vals(wcstring_list_t vals) const {
        return env_var_t{std::move(vals), flags_};
    }

    env_var_t setting_exports(bool exportv) const {
        env_var_flags_t flags = flags_;
        if (exportv) {
            flags |= flag_export;
        } else {
            flags &= ~flag_export;
        }
        return env_var_t{vals_, flags};
    }

    env_var_t setting_pathvar(bool pathvar) const {
        env_var_flags_t flags = flags_;
        if (pathvar) {
            flags |= flag_pathvar;
        } else {
            flags &= ~flag_pathvar;
        }
        return env_var_t{vals_, flags};
    }

    env_var_t setting_read_only(bool read_only) const {
        env_var_flags_t flags = flags_;
        if (read_only) {
            flags |= flag_read_only;
        } else {
            flags &= ~flag_read_only;
        }
        return env_var_t{vals_, flags};
    }

    static env_var_flags_t flags_for(const wchar_t *name);
    static std::shared_ptr<const wcstring_list_t> empty_list();

    env_var_t &operator=(const env_var_t &var) = default;
    env_var_t &operator=(env_var_t &&) = default;

    bool operator==(const env_var_t &rhs) const {
        return *vals_ == *rhs.vals_ && flags_ == rhs.flags_;
    }
    bool operator!=(const env_var_t &rhs) const { return !(*this == rhs); }
};
typedef std::unordered_map<wcstring, env_var_t> var_table_t;

/// An environment is read-only access to variable values.
class environment_t {
   protected:
    environment_t() = default;

   public:
    virtual maybe_t<env_var_t> get(const wcstring &key,
                                   env_mode_flags_t mode = ENV_DEFAULT) const = 0;
    virtual wcstring_list_t get_names(int flags) const = 0;
    virtual ~environment_t();

    /// Returns the PWD with a terminating slash.
    virtual wcstring get_pwd_slash() const;
};

/// The null environment contains nothing.
class null_environment_t : public environment_t {
   public:
    null_environment_t() = default;
    ~null_environment_t() override;

    maybe_t<env_var_t> get(const wcstring &key, env_mode_flags_t mode = ENV_DEFAULT) const override;
    wcstring_list_t get_names(int flags) const override;
};

/// A mutable environment which allows scopes to be pushed and popped.
class env_stack_impl_t;
class env_stack_t final : public environment_t {
    friend class parser_t;

    /// The implementation. Do not access this directly.
    const std::unique_ptr<env_stack_impl_t> impl_;

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
    wcstring_list_t get_names(int flags) const override;

    /// Sets the variable with the specified name to the given values.
    /// If \p out_events is supplied, populate it with any events generated through setting the
    /// variable.
    int set(const wcstring &key, env_mode_flags_t mode, wcstring_list_t vals,
            std::vector<event_t> *out_events = nullptr);

    /// Sets the variable with the specified name to a single value.
    int set_one(const wcstring &key, env_mode_flags_t mode, wcstring val,
                std::vector<event_t> *out_events = nullptr);

    /// Sets the variable with the specified name to no values.
    int set_empty(const wcstring &key, env_mode_flags_t mode,
                  std::vector<event_t> *out_events = nullptr);

    /// Update the PWD variable based on the result of getcwd.
    void set_pwd_from_getcwd();

    /// Remove environment variable.
    ///
    /// \param key The name of the variable to remove
    /// \param mode should be ENV_USER if this is a remove request from the user, 0 otherwise. If
    /// this is a user request, read-only variables can not be removed. The mode may also specify
    /// the scope of the variable that should be erased.
    /// \param out_events if non-null, populate it with any events generated from removing this
    /// variable.
    ///
    /// \return zero if the variable existed, and non-zero if the variable did not exist
    int remove(const wcstring &key, int mode, std::vector<event_t> *out_events = nullptr);

    /// Push the variable stack. Used for implementing local variables for functions and for-loops.
    void push(bool new_scope);

    /// Pop the variable stack. Used for implementing local variables for functions and for-loops.
    void pop();

    /// Synchronizes all universal variable changes: writes everything out, reads stuff in.
    /// \return true if something changed, false otherwise.
    bool universal_barrier();

    /// Returns an array containing all exported variables in a format suitable for execv.
    std::shared_ptr<const null_terminated_array_t<char>> export_arr();

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
    void set_argv(wcstring_list_t argv);

    /// Slightly optimized implementation.
    wcstring get_pwd_slash() const override;

    // Compatibility hack; access the "environment stack" from back when there was just one.
    static const std::shared_ptr<env_stack_t> &principal_ref();
    static env_stack_t &principal() { return *principal_ref(); }

    // Access a variable stack that only represents globals.
    // Do not push or pop from this.
    static env_stack_t &globals();
};

extern bool g_use_posix_spawn;

extern bool term_has_xn;  // does the terminal have the "eat_newline_glitch"

/// Synchronizes all universal variable changes: writes everything out, reads stuff in.
/// \return true if any value changed.
bool env_universal_barrier();

/// Returns true if we think the terminal supports setting its title.
bool term_supports_setting_title();

/// Gets a path appropriate for runtime storage
wcstring env_get_runtime_path();

/// A wrapper around setenv() and unsetenv() which use a lock.
/// In general setenv() and getenv() are highly incompatible with threads. This makes it only
/// slightly safer.
void setenv_lock(const char *name, const char *value, int overwrite);
void unsetenv_lock(const char *name);

#endif
