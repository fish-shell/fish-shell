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
struct EnvNull;
struct EnvStack;
struct EnvStackRef;
struct EnvDyn;
enum class env_stack_set_result_t : uint8_t;
struct Statuses;
#endif

struct event_list_ffi_t;
struct function_properties_t;

using statuses_t = Statuses;

/// FFI helper for events.
struct Event;
struct event_list_ffi_t {
    event_list_ffi_t(const event_list_ffi_t &) = delete;
    event_list_ffi_t &operator=(const event_list_ffi_t &) = delete;
    event_list_ffi_t();
#if INCLUDE_RUST_HEADERS
    std::vector<rust::Box<Event>> events{};
#endif

    // Append an Event pointer, which came from Box::into_raw().
    void push(void *event);
};

struct owning_null_terminated_array_t;

#if INCLUDE_RUST_HEADERS
#include "env/env_ffi.rs.h"
#else
struct EnvVar;
struct EnvNull;
struct EnvStackRef;
#endif

struct owning_null_terminated_array_t;

extern "C" {
extern bool CURSES_INITIALIZED;

/// Does the terminal have the "eat_newline_glitch".
extern bool TERM_HAS_XN;

extern size_t READ_BYTE_LIMIT;
}

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

/// env_var_t is an immutable value-type data structure representing the value of an environment
/// variable. This wraps the EnvVar type from Rust.
class env_var_t {
   public:
    using env_var_flags_t = uint8_t;
    enum {
        flag_export = 1 << 0,     // whether the variable is exported
        flag_read_only = 1 << 1,  // whether the variable is read only
        flag_pathvar = 1 << 2,    // whether the variable is a path variable
    };
    env_var_t() : env_var_t(wcstring_list_ffi_t{}, 0) {}
    env_var_t(const wcstring_list_ffi_t &vals, uint8_t flags);
    env_var_t(const env_var_t &);
    env_var_t(env_var_t &&) = default;

    env_var_t(wcstring val, env_var_flags_t flags)
        : env_var_t{std::vector<wcstring>{std::move(val)}, flags} {}

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

    static env_var_flags_t flags_for(const wchar_t *name);

    env_var_t &operator=(const env_var_t &);
    env_var_t &operator=(env_var_t &&) = default;

    bool operator==(const env_var_t &rhs) const;
    bool operator!=(const env_var_t &rhs) const { return !(*this == rhs); }

   private:
    env_var_t(rust::Box<EnvVar> &&impl) : impl_(std::move(impl)) {}

    rust::Box<EnvVar> impl_;
};

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
    null_environment_t();
    ~null_environment_t();

    maybe_t<env_var_t> get(const wcstring &key, env_mode_flags_t mode = ENV_DEFAULT) const override;
    std::vector<wcstring> get_names(env_mode_flags_t flags) const override;

   private:
    rust::Box<EnvNull> impl_;
};

/// A mutable environment which allows scopes to be pushed and popped.
class env_stack_t final : public environment_t {
    friend struct Parser;

    /// \return whether we are the principal stack.
    bool is_principal() const;

   public:
    ~env_stack_t() override;
    env_stack_t(env_stack_t &&);
    /* implicit */ env_stack_t(rust::Box<EnvStackRef> imp);
    /* implicit */ env_stack_t(uint8_t *imp);

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

    /// Snapshot this environment. This means returning a read-only copy. Local variables are copied
    /// but globals are shared (i.e. changes to global will be visible to this snapshot). This
    /// returns a shared_ptr for convenience, since the most common reason to snapshot is because
    /// you want to read from another thread.
    std::shared_ptr<environment_t> snapshot() const;

    /// Slightly optimized implementation.
    wcstring get_pwd_slash() const override;

    /// "Override" of get_or_null, as autocxx doesn't understand inheritance.
    std::unique_ptr<env_var_t> get_or_null(const wcstring &key,
                                           env_mode_flags_t mode = ENV_DEFAULT) const {
        return environment_t::get_or_null(key, mode);
    }

    // Compatibility hack; access the "environment stack" from back when there was just one.
    static const std::shared_ptr<env_stack_t> &principal_ref();
    static env_stack_t &principal() { return *principal_ref(); }

    // Access a variable stack that only represents globals.
    // Do not push or pop from this.
    static env_stack_t &globals();

    /// Access the underlying Rust implementation.
    /// This returns a const rust::Box<EnvStackRef> *, or in Rust terms, a *const Box<EnvStackRef>.
    const EnvStackRef &get_impl_ffi() const;

   private:
    /// The implementation. Do not access this directly.
    rust::Box<EnvStackRef> impl_;
};

#if INCLUDE_RUST_HEADERS
struct EnvDyn;
/// Wrapper around rust's `&dyn Environment` deriving from `environment_t`.
class env_dyn_t final : public environment_t {
   public:
    env_dyn_t(rust::Box<EnvDyn> impl) : impl_(std::move(impl)) {}
    maybe_t<env_var_t> get(const wcstring &key, env_mode_flags_t mode) const;

    std::vector<wcstring> get_names(env_mode_flags_t flags) const;

   private:
    rust::Box<EnvDyn> impl_;
};
#endif

/// A struct of configuration directories, determined in main() that fish will optionally pass to
/// env_init.
struct config_paths_t {
    wcstring data;     // e.g., /usr/local/share
    wcstring sysconf;  // e.g., /usr/local/etc
    wcstring doc;      // e.g., /usr/local/share/doc/fish
    wcstring bin;      // e.g., /usr/local/bin
};

/// Initialize environment variable data.
void env_init(const struct config_paths_t *paths = nullptr, bool do_uvars = true,
              bool default_paths = false);

using env_var_flags_t = uint8_t;
enum {
    env_var_flag_export = 1 << 0,     // whether the variable is exported
    env_var_flag_read_only = 1 << 1,  // whether the variable is read only
    env_var_flag_pathvar = 1 << 2,    // whether the variable is a path variable
};

/// A mutable environment which allows scopes to be pushed and popped.

#if INCLUDE_RUST_HEADERS
struct EnvDyn;
#endif

/// A wrapper around setenv() and unsetenv() which use a lock.
/// In general setenv() and getenv() are highly incompatible with threads. This makes it only
/// slightly safer.
extern "C" {
void setenv_lock(const char *name, const char *value, int overwrite);
void unsetenv_lock(const char *name);
}

void set_inheriteds_ffi();

#endif
