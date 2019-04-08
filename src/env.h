// Prototypes for functions for manipulating fish script variables.
#ifndef FISH_ENV_H
#define FISH_ENV_H

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "common.h"
#include "maybe.h"

extern size_t read_byte_limit;
extern bool curses_initialized;

/// Character for separating two array elements. We use 30, i.e. the ascii record separator since
/// that seems logical.
#define ARRAY_SEP (wchar_t)0x1e

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

/// Initialize environment variable data.
void env_init(const struct config_paths_t *paths = NULL);

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
    bool operator!=(const env_var_t &rhs) const { return ! (*this == rhs); }
};

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
    wcstring get_pwd_slash() const;
};

/// The null environment contains nothing.
class null_environment_t : public environment_t {
   public:
    null_environment_t();
    ~null_environment_t() override;

    maybe_t<env_var_t> get(const wcstring &key, env_mode_flags_t mode = ENV_DEFAULT) const override;
    wcstring_list_t get_names(int flags) const override;
};

/// Synchronizes all universal variable changes: writes everything out, reads stuff in.
void env_universal_barrier();

/// A environment stack of scopes. This is the main class that tracks fish variables.
struct var_stack_t;
class env_node_t;
class env_stack_t final : public environment_t {
    friend class parser_t;
    std::unique_ptr<var_stack_t> vars_;

    int set_internal(const wcstring &key, env_mode_flags_t var_mode, wcstring_list_t val);

    bool try_remove(std::shared_ptr<env_node_t> n, const wchar_t *key, int var_mode);
    std::shared_ptr<env_node_t> get_node(const wcstring &key);

    static env_stack_t make_principal();

    var_stack_t &vars_stack();
    const var_stack_t &vars_stack() const;

    explicit env_stack_t(std::unique_ptr<var_stack_t> vars_);
    env_stack_t();
    ~env_stack_t() override;

    env_stack_t(env_stack_t &&);

   public:
    /// Gets the variable with the specified name, or none() if it does not exist.
    maybe_t<env_var_t> get(const wcstring &key, env_mode_flags_t mode = ENV_DEFAULT) const override;

    /// Sets the variable with the specified name to the given values.
    int set(const wcstring &key, env_mode_flags_t mode, wcstring_list_t vals);

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

    /// Synchronizes all universal variable changes: writes everything out, reads stuff in.
    void universal_barrier();

    /// Returns an array containing all exported variables in a format suitable for execv
    const char *const *export_arr();

    /// Returns all variable names.
    wcstring_list_t get_names(int flags) const override;

    /// Update the termsize variable.
    void set_termsize();

    /// Update the PWD variable directory.
    bool set_pwd();

    /// Sets up argv as the given null terminated array of strings.
    void set_argv(const wchar_t *const *argv);

    /// Mark that exported variables have changed.
    void mark_changed_exported();

    // Compatibility hack; access the "environment stack" from back when there was just one.
    static env_stack_t &principal();

    // Access a variable stack that only represents globals.
    // Do not push or pop from this.
    static env_stack_t &globals();
};

class env_vars_snapshot_t : public environment_t {
    std::map<wcstring, env_var_t> vars;
    wcstring_list_t names;

   public:
    env_vars_snapshot_t() = default;
    env_vars_snapshot_t(const env_vars_snapshot_t &) = default;
    env_vars_snapshot_t &operator=(const env_vars_snapshot_t &) = default;
    env_vars_snapshot_t(const environment_t &source, const wchar_t *const *keys);
    ~env_vars_snapshot_t() override;

    maybe_t<env_var_t> get(const wcstring &key, env_mode_flags_t mode = ENV_DEFAULT) const override;

    wcstring_list_t get_names(int flags) const override;

    // Vars necessary for highlighting.
    static const wchar_t *const highlighting_keys[];

    // Vars necessary for completion.
    static const wchar_t *const completing_keys[];
};

extern int g_fork_count;
extern bool g_use_posix_spawn;

typedef std::map<wcstring, env_var_t> var_table_t;

extern bool term_has_xn;  // does the terminal have the "eat_newline_glitch"

/// Returns true if we think the terminal supports setting its title.
bool term_supports_setting_title();

/// Gets a path appropriate for runtime storage
wcstring env_get_runtime_path();

/// Replace empty path elements with "." - see #3914.
void fix_colon_delimited_var(const wcstring &var_name, env_stack_t &vars);

#endif
