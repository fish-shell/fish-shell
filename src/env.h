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
struct EnvironmentRef;
enum class env_stack_set_result_t : uint8_t;
class Statuses;
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

/// env_var_t is an immutable value-type data structure representing the value of an environment
/// variable. This wraps the EnvVar type from Rust.
using env_var_t = EnvVar;
// class env_var_t {
//    public:
//     using env_var_flags_t = uint8_t;
//     enum {
//         flag_export = 1 << 0,     // whether the variable is exported
//         flag_read_only = 1 << 1,  // whether the variable is read only
//         flag_pathvar = 1 << 2,    // whether the variable is a path variable
//     };
//     env_var_t() : env_var_t(wcstring_list_ffi_t{}, 0) {}
//     env_var_t(const wcstring_list_ffi_t &vals, uint8_t flags);
//     env_var_t(const env_var_t &);
//     env_var_t(env_var_t &&) = default;

//     env_var_t(wcstring val, env_var_flags_t flags)
//         : env_var_t{std::vector<wcstring>{std::move(val)}, flags} {}

//     // Construct from FFI. This transfers ownership of the EnvVar, which should originate
//     // in Box::into_raw().
//     static env_var_t new_ffi(EnvVar *ptr);

//     // Get the underlying EnvVar pointer.
//     // Note you may need to mem::transmute this, since autocxx gets confused when going from Rust
//     ->
//     // C++ -> Rust.
//     const EnvVar *ffi_ptr() const { return &*this->impl_; }

//     bool empty() const;
//     bool exports() const;
//     bool is_read_only() const;
//     bool is_pathvar() const;
//     env_var_flags_t get_flags() const;

//     wcstring as_string() const;
//     void to_list(std::vector<wcstring> &out) const;
//     std::vector<wcstring> as_list() const;
//     wcstring_list_ffi_t as_list_ffi() const { return as_list(); }

//     /// \return the character used when delimiting quoted expansion.
//     wchar_t get_delimiter() const;

//     static env_var_flags_t flags_for(const wchar_t *name);

//     env_var_t &operator=(const env_var_t &);
//     env_var_t &operator=(env_var_t &&) = default;

//     bool operator==(const env_var_t &rhs) const;
//     bool operator!=(const env_var_t &rhs) const { return !(*this == rhs); }

//    private:
//     env_var_t(rust::Box<EnvVar> &&impl) : impl_(std::move(impl)) {}

//     rust::Box<EnvVar> impl_;
// };
// typedef std::unordered_map<wcstring, env_var_t> var_table_t;

/// An environment is read-only access to variable values.
using environment_t = EnvStackRef;
// class environment_t {
//    protected:
//     environment_t() = default;

//    public:
//     virtual maybe_t<env_var_t> get(const wcstring &key,
//                                    env_mode_flags_t mode = ENV_DEFAULT) const = 0;
//     virtual std::vector<wcstring> get_names(env_mode_flags_t flags) const = 0;
//     virtual ~environment_t();

//     maybe_t<env_var_t> get_unless_empty(const wcstring &key,
//                                         env_mode_flags_t mode = ENV_DEFAULT) const;
//     /// \return a environment variable as a unique pointer, or nullptr if none.
//     std::unique_ptr<env_var_t> get_or_null(const wcstring &key,
//                                            env_mode_flags_t mode = ENV_DEFAULT) const;

//     /// Returns the PWD with a terminating slash.
//     virtual wcstring get_pwd_slash() const;
// };

// /// The null environment contains nothing.
// class null_environment_t : public environment_t {
//    public:
//     null_environment_t();
//     ~null_environment_t();

//     maybe_t<env_var_t> get(const wcstring &key, env_mode_flags_t mode = ENV_DEFAULT) const
//     override; std::vector<wcstring> get_names(env_mode_flags_t flags) const override;

//    private:
//     rust::Box<EnvNull> impl_;
// };

/// A mutable environment which allows scopes to be pushed and popped.
using env_stack_t = EnvStackRef;

#if INCLUDE_RUST_HEADERS
struct EnvDyn;
#endif

/// Gets a path appropriate for runtime storage
wcstring env_get_runtime_path();

/// A wrapper around setenv() and unsetenv() which use a lock.
/// In general setenv() and getenv() are highly incompatible with threads. This makes it only
/// slightly safer.
extern "C" {
void setenv_lock(const char *name, const char *value, int overwrite);
void unsetenv_lock(const char *name);
}

/// Returns the originally inherited variables and their values.
/// This is a simple key->value map and not e.g. cut into paths.
const std::map<wcstring, wcstring> &env_get_inherited();

/// Populate the values in the "$history" variable.
/// fish_history_val is the value of the "$fish_history" variable, or "fish" if not set.
wcstring_list_ffi_t get_history_variable_text_ffi(const wcstring &fish_history_val);

void set_inheriteds_ffi();

#endif
