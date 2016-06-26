// The classes responsible for autoloading functions and completions.
#ifndef FISH_AUTOLOAD_H
#define FISH_AUTOLOAD_H

#include <pthread.h>
#include <stddef.h>
#include <time.h>
#include <set>

#include "common.h"
#include "lru.h"

/// Record of an attempt to access a file.
struct file_access_attempt_t {
    /// Modification time of the file
    time_t mod_time;
    /// When we last checked the file
    time_t last_checked;
    /// Whether or not we believe we can access this file
    bool accessible;
    /// The access attempt is stale
    bool stale;
    /// If we cannot access the file, the error code encountered.
    int error;
};
file_access_attempt_t access_file(const wcstring &path, int mode);

struct autoload_function_t : public lru_node_t {
    explicit autoload_function_t(const wcstring &key)
        : lru_node_t(key),
          access(),
          is_loaded(false),
          is_placeholder(false),
          is_internalized(false) {}

    /// The last access attempt recorded
    file_access_attempt_t access;
    /// Have we actually loaded this function?
    bool is_loaded;
    /// Whether we are a placeholder that stands in for "no such function". If this is true, then
    /// is_loaded must be false.
    bool is_placeholder;
    /// Whether this function came from a builtin "internalized" script.
    bool is_internalized;
};

struct builtin_script_t {
    const wchar_t *name;
    const char *def;
};

class env_vars_snapshot_t;

/// Class representing a path from which we can autoload and the autoloaded contents.
class autoload_t : private lru_cache_t<autoload_function_t> {
   private:
    /// Lock for thread safety.
    pthread_mutex_t lock;
    /// The environment variable name.
    const wcstring env_var_name;
    /// Builtin script array.
    const struct builtin_script_t *const builtin_scripts;
    /// Builtin script count.
    const size_t builtin_script_count;
    /// The path from which we most recently autoloaded.
    wcstring last_path;
    /// the most reecently autoloaded path, tokenized (split on separators).
    wcstring_list_t last_path_tokenized;
    /// A table containing all the files that are currently being loaded.
    /// This is here to help prevent recursion.
    std::set<wcstring> is_loading_set;

    void remove_all_functions(void) { this->evict_all_nodes(); }

    bool locate_file_and_maybe_load_it(const wcstring &cmd, bool really_load, bool reload,
                                       const wcstring_list_t &path_list);

    virtual void node_was_evicted(autoload_function_t *node);

    autoload_function_t *get_autoloaded_function_with_creation(const wcstring &cmd,
                                                               bool allow_eviction);

   protected:
    /// Overridable callback for when a command is removed.
    virtual void command_removed(const wcstring &cmd) {}

   public:
    /// Create an autoload_t for the given environment variable name.
    autoload_t(const wcstring &env_var_name_var, const builtin_script_t *scripts,
               size_t script_count);

    virtual ~autoload_t();

    /// Autoload the specified file, if it exists in the specified path. Do not load it multiple
    /// times unless its timestamp changes or parse_util_unload is called.
    /// Autoloading one file may unload another.
    /// @param cmd the filename to search for. The suffix '.fish' is always added to this name
    /// @param reload wheter to recheck file timestamps on already loaded files
    int load(const wcstring &cmd, bool reload);

    /// Check whether we have tried loading the given command. Does not do any I/O.
    bool has_tried_loading(const wcstring &cmd);

    /// Tell the autoloader that the specified file, in the specified path, is no longer loaded.
    /// Returns non-zero if the file was removed, zero if the file had not yet been loaded
    int unload(const wcstring &cmd);

    /// Check whether the given command could be loaded, but do not load it.
    bool can_load(const wcstring &cmd, const env_vars_snapshot_t &vars);
};
#endif
