// Prototypes for functions for setting and getting environment variables.
#ifndef FISH_ENV_H
#define FISH_ENV_H

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "common.h"

extern size_t read_byte_limit;
extern bool curses_initialized;

// Flags that may be passed as the 'mode' in env_set / env_get_string.
enum {
    /// Default mode.
    ENV_DEFAULT = 0,

    /// Flag for local (to the current block) variable.
    ENV_LOCAL = 1,

    /// Flag for exported (to commands) variable.
    ENV_EXPORT = 2,

    /// Flag for unexported variable.
    ENV_UNEXPORT = 16,

    /// Flag for global variable.
    ENV_GLOBAL = 4,

    /// Flag for variable update request from the user. All variable changes that are made directly
    /// by the user, such as those from the 'set' builtin must have this flag set.
    ENV_USER = 8,

    /// Flag for universal variable.
    ENV_UNIVERSAL = 32
};
typedef uint32_t env_mode_flags_t;

/// Return values for `env_set()`.
enum { ENV_OK, ENV_PERM, ENV_SCOPE, ENV_INVALID };

/// A struct of configuration directories, determined in main() that fish will optionally pass to
/// env_init.
struct config_paths_t {
    wcstring data;     // e.g. /usr/local/share
    wcstring sysconf;  // e.g. /usr/local/etc
    wcstring doc;      // e.g. /usr/local/share/doc/fish
    wcstring bin;      // e.g. /usr/local/bin
};

/// Initialize environment variable data.
void env_init(const struct config_paths_t *paths = NULL);

/// Various things we need to initialize at run-time that don't really fit any of the other init
/// routines.
void misc_init();

int env_set(const wcstring &key, const wchar_t *val, env_mode_flags_t mode);

class env_var_t : public wcstring {
   private:
    bool is_missing;

   public:
    static env_var_t missing_var() {
        env_var_t result((wcstring()));
        result.is_missing = true;
        return result;
    }

    env_var_t(const env_var_t &x) : wcstring(x), is_missing(x.is_missing) {}
    env_var_t(const wcstring &x) : wcstring(x), is_missing(false) {}
    env_var_t(const wchar_t *x) : wcstring(x), is_missing(false) {}
    env_var_t() : wcstring(L""), is_missing(false) {}

    bool missing(void) const { return is_missing; }

    bool missing_or_empty(void) const { return missing() || empty(); }

    const wchar_t *c_str(void) const;

    env_var_t &operator=(const env_var_t &s) {
        is_missing = s.is_missing;
        wcstring::operator=(s);
        return *this;
    }

    bool operator==(const env_var_t &s) const {
        return is_missing == s.is_missing &&
               static_cast<const wcstring &>(*this) == static_cast<const wcstring &>(s);
    }

    bool operator==(const wcstring &s) const {
        return !is_missing && static_cast<const wcstring &>(*this) == s;
    }

    bool operator!=(const env_var_t &s) const { return !(*this == s); }

    bool operator!=(const wcstring &s) const { return !(*this == s); }

    bool operator==(const wchar_t *s) const {
        return !is_missing && static_cast<const wcstring &>(*this) == s;
    }

    bool operator!=(const wchar_t *s) const { return !(*this == s); }
};

/// Gets the variable with the specified name, or env_var_t::missing_var if it does not exist or is
/// an empty array.
///
/// \param key The name of the variable to get
/// \param mode An optional scope to search in. All scopes are searched if unset
env_var_t env_get_string(const wcstring &key, env_mode_flags_t mode = ENV_DEFAULT);

/// Returns true if the specified key exists. This can't be reliably done using env_get, since
/// env_get returns null for 0-element arrays.
///
/// \param key The name of the variable to remove
/// \param mode the scope to search in. All scopes are searched if set to default
bool env_exist(const wchar_t *key, env_mode_flags_t mode);

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
    std::map<wcstring, wcstring> vars;
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

/// A variable entry. Stores the value of a variable and whether it should be exported.
struct var_entry_t {
    wcstring val;  // the value of the variable
    bool exportv;  // whether the variable should be exported

    var_entry_t() : exportv(false) {}
};

typedef std::map<wcstring, var_entry_t> var_table_t;

extern bool term_has_xn;  // does the terminal have the "eat_newline_glitch"

/// Returns true if we think the terminal supports setting its title.
bool term_supports_setting_title();
#endif
