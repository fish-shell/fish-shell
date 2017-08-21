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

extern size_t read_byte_limit;
extern bool curses_initialized;

/// Character for separating two array elements. We use 30, i.e. the ascii record separator since
/// that seems logical.
#define ARRAY_SEP (wchar_t)0x1e

/// String containing the character for separating two array elements.
#define ARRAY_SEP_STR L"\x1e"

/// Value denoting a null string.
#define ENV_NULL L"\x1d"

// Flags that may be passed as the 'mode' in env_set / env_get.
enum {
    /// Default mode. Used with `env_get()` to indicate the caller doesn't care what scope the var
    /// is in or whether it is exported or unexported.
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
    /// Flag for variable update request from the user. All variable changes that are made directly
    /// by the user, such as those from the `read` and `set` builtin must have this flag set. It
    /// serves one purpose: to indicate that an error should be returned if the user is attempting
    /// to modify a var that should not be modified by direct user action; e.g., a read-only var.
    ENV_USER = 1 << 5,
};
typedef uint32_t env_mode_flags_t;

/// Return values for `env_set()`.
enum { ENV_OK, ENV_PERM, ENV_SCOPE, ENV_INVALID };

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

class env_var_t {
   private:
    wcstring name;         // name of the var
    wcstring_list_t vals;  // list of values assigned to the var
    bool is_missing;       // true if the var is not set in the requested scope

   public:
    bool exportv;  // whether the variable should be exported

    void set_missing(void) { is_missing = true; }

    // Constructors.
    env_var_t(const env_var_t &v)
        : name(v.name), vals(v.vals), is_missing(v.is_missing), exportv(v.exportv) {}
    env_var_t(const wcstring &our_name, const wcstring_list_t &l)
        : name(our_name), vals(l), is_missing(false), exportv(false) {}
    env_var_t(const wcstring &our_name, const wcstring &s)
        : name(our_name),
          vals({
              s,
          }),
          is_missing(false),
          exportv(false) {}
    env_var_t(const wcstring &our_name, const wchar_t *s)
        : name(our_name),
          vals({
              wcstring(s),
          }),
          is_missing(false),
          exportv(false) {}
    env_var_t() : name(), vals(), is_missing(false), exportv(false) {}

    bool empty(void) const { return vals.empty() || (vals.size() == 1 && vals[0].empty()); };
    bool missing(void) const { return is_missing; }
    bool missing_or_empty(void) const { return missing() || empty(); }
    bool read_only(void) const;

    bool matches_string(const wcstring &str) {
        if (is_missing) return false;
        if (vals.size() > 1) return false;
        return vals[0] == str;
    }

    wcstring as_string() const;
    void to_list(wcstring_list_t &out) const;
    wcstring_list_t &as_list();
    const wcstring_list_t &as_const_list() const;

    const wcstring get_name() const { return name; }

    void set_vals(wcstring_list_t &l) { vals = l; }
    void swap_vals(wcstring_list_t &l) { vals.swap(l); }
    void set_val(const wcstring &s) {
        vals = {
            s,
        };
    }

    env_var_t &operator=(const env_var_t &var) {
        this->vals = var.vals;
        this->exportv = var.exportv;
        this->is_missing = var.is_missing;
        return *this;
    }

    /// Compare a simple string to the var. Returns true iff the var is not missing, has a single
    /// value and that value matches the string being compared to.
    bool operator==(const wcstring &str) const {
        if (is_missing) return false;
        if (vals.size() > 1) return false;
        return vals[0] == str;
    }

    bool operator==(const env_var_t &var) const {
        return this->is_missing == var.is_missing && vals == var.vals;
    }

    bool operator==(const wcstring_list_t &values) const { return !is_missing && vals == values; }

    bool operator!=(const env_var_t &var) const { return vals != var.vals; }
};

env_var_t create_missing_var();
extern env_var_t missing_var;

/// This is used to convert a serialized env_var_t back into a list.
wcstring_list_t decode_serialized(const wcstring &s);

/// Gets the variable with the specified name, or env_var_t::missing_var if it does not exist.
env_var_t env_get(const wcstring &key, env_mode_flags_t mode = ENV_DEFAULT);

/// Sets the variable with the specified name to the given values. The `vals` will be set to an
/// empty list on return since its content will be swapped into the `env_var_t` that is created.
int env_set(const wcstring &key, env_mode_flags_t mode, wcstring_list_t &vals,
            bool swap_vals = true);

/// Sets the variable with the specified name to a single value.
int env_set_one(const wcstring &key, env_mode_flags_t mode, const wcstring &val);

/// Sets the variable with the specified name to no values.
int env_set_empty(const wcstring &key, env_mode_flags_t mode);

/// Remove environment variable.
///
/// \param key The name of the variable to remove
/// \param mode should be ENV_USER if this is a remove request from the user, 0 otherwise. If this
/// is a user request, read-only variables can not be removed. The mode may also specify the scope
/// of the variable that should be erased.
///
/// \return zero if the variable existed, and non-zero if the variable did not exist
int env_remove(const wcstring &key, int mode);

/// Push the variable stack. Used for implementing local variables for functions and for-loops.
void env_push(bool new_scope);

/// Pop the variable stack. Used for implementing local variables for functions and for-loops.
void env_pop();

/// Synchronizes all universal variable changes: writes everything out, reads stuff in.
void env_universal_barrier();

/// Returns an array containing all exported variables in a format suitable for execv
const char *const *env_export_arr();

/// Sets up argv as the given null terminated array of strings.
void env_set_argv(const wchar_t *const *argv);

/// Returns all variable names.
wcstring_list_t env_get_names(int flags);

/// Update the PWD variable directory.
bool env_set_pwd();

/// Returns the PWD with a terminating slash.
wcstring env_get_pwd_slash();

/// Update the read_byte_limit variable.
void env_set_read_limit();

class env_vars_snapshot_t {
    std::map<wcstring, env_var_t> vars;
    bool is_current() const;

   public:
    env_vars_snapshot_t(const env_vars_snapshot_t &) = default;
    env_vars_snapshot_t &operator=(const env_vars_snapshot_t &) = default;

    env_vars_snapshot_t(const wchar_t *const *keys);
    env_vars_snapshot_t();

    env_var_t get(const wcstring &key) const;

    // Returns the fake snapshot representing the live variables array.
    static const env_vars_snapshot_t &current();

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
#endif
