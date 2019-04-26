// The classes responsible for autoloading functions and completions.
#ifndef FISH_AUTOLOAD_H
#define FISH_AUTOLOAD_H

#include <pthread.h>
#include <time.h>

#include <set>
#include <unordered_set>

#include "common.h"
#include "env.h"
#include "lru.h"
#include "wutil.h"

class autoload_file_cache_t;
class environment_t;

/// autoloader_t is a class that knows how to autoload .fish files from a list of directories. This
/// is used by autoloading functions and completions. It maintains a file cache, which is
/// responsible for potentially cached accesses of files, and then a list of files that have
/// actually been autoloaded. A client may request a file to autoload given a command name, and may
/// be returned a path which it is expected to source.
/// autoloader_t does not have any internal locks; it is the responsibility of the caller to lock
/// it.
class autoloader_t {
    /// The environment variable whose paths we observe.
    const wcstring env_var_name_;

    /// A map from command to the files we have autoloaded.
    std::unordered_map<wcstring, file_id_t> autoloaded_files_;

    /// The list of commands that we are currently autoloading.
    std::unordered_set<wcstring> current_autoloading_;

    /// The autoload cache.
    /// This is a unique_ptr because want to change it if the value of our environment variable
    /// changes. This is never null (but it may be a cache with no paths).
    std::unique_ptr<autoload_file_cache_t> cache_;

   public:
    /// Construct an autoloader that loads from the paths given by \p env_var_name.
    explicit autoloader_t(wcstring env_var_name);

    ~autoloader_t();

    /// Given a command, get a path to autoload.
    /// For example, if the environment variable is 'fish_function_path' and the command is 'foo',
    /// this will look for a file 'foo.fish' in one of the directories given by fish_function_path.
    /// If there is no such file, OR if the file has been previously resolved and is now unchanged,
    /// this will return none. But if the file is either new or changed, this will return the path.
    /// After returning a path, the command is marked in-progress until the caller calls
    /// mark_autoload_finished() with the same command. Note this does not actually execute any
    /// code; it is the caller's responsibility to load the file.
    maybe_t<wcstring> resolve_command(const wcstring &cmd, const environment_t &env);

    /// Helper to actually perform an autoload.
    /// This is a static function because it executes fish script, and so must be called without
    /// holding any particular locks.
    static void perform_autoload(const wcstring &path);

    /// Mark that a command previously returned from path_to_autoload is finished autoloading.
    void mark_autoload_finished(const wcstring &cmd) {
        size_t amt = current_autoloading_.erase(cmd);
        assert(amt > 0 && "cmd was not being autoloaded");
        (void)amt;
    }

    /// \return whether a command is currently being autoloaded.
    bool autoload_in_progress(const wcstring &cmd) const {
        return current_autoloading_.count(cmd) > 0;
    }

    /// \return whether a command could potentially be autoloaded.
    /// This does not actually mark the command as being autoloaded.
    bool can_autoload(const wcstring &cmd);

    /// Mark that all autoloaded files have been forgotten.
    /// Future calls to path_to_autoload() will return previously-returned paths.
    void clear() {
        // Note there is no reason to invalidate the cache here.
        autoloaded_files_.clear();
    }
};

/// Record of an attempt to access a file.
struct file_access_attempt_t {
    /// If filled, the file ID of the checked file.
    /// Otherwise the file did not exist or was otherwise inaccessible.
    /// Note that this will never contain kInvalidFileID.
    maybe_t<file_id_t> file_id;

    /// When we last checked the file.
    time_t last_checked;

    /// Whether or not we believe we can access this file.
    bool accessible() const { return file_id.has_value(); }
};
file_access_attempt_t access_file(const wcstring &path, int mode);

struct autoload_function_t {
    explicit autoload_function_t(bool placeholder)
        : access(), is_loaded(false), is_placeholder(placeholder) {}

    /// The last access attempt recorded
    file_access_attempt_t access;
    /// Have we actually loaded this function?
    bool is_loaded;
    /// Whether we are a placeholder that stands in for "no such function". If this is true, then
    /// is_loaded must be false.
    bool is_placeholder;
};

/// Class representing a path from which we can autoload and the autoloaded contents.
class autoload_t : public lru_cache_t<autoload_t, autoload_function_t> {
   private:
    /// Lock for thread safety.
    std::mutex lock;
    /// The environment variable name.
    const wcstring env_var_name;
    /// The paths from which to autoload, or missing if none.
    maybe_t<wcstring_list_t> paths;
    /// A table containing all the files that are currently being loaded.
    /// This is here to help prevent recursion.
    std::unordered_set<wcstring> is_loading_set;

    void remove_all_functions() { this->evict_all_nodes(); }

    bool locate_file_and_maybe_load_it(const wcstring &cmd, bool really_load, bool reload,
                                       const wcstring_list_t &path_list);

    autoload_function_t *get_autoloaded_function_with_creation(const wcstring &cmd,
                                                               bool allow_eviction);

   public:
    // CRTP override
    void entry_was_evicted(wcstring key, autoload_function_t node);

    // Create an autoload_t for the given environment variable name.
    explicit autoload_t(wcstring env_var_name_var);

    /// Autoload the specified file, if it exists in the specified path. Do not load it multiple
    /// times unless its timestamp changes or parse_util_unload is called.
    /// @param cmd the filename to search for. The suffix '.fish' is always added to this name
    /// @param reload wheter to recheck file timestamps on already loaded files
    int load(const wcstring &cmd, bool reload);

    /// Check whether we have tried loading the given command. Does not do any I/O.
    bool has_tried_loading(const wcstring &cmd);

    /// Tell the autoloader that the specified file, in the specified path, is no longer loaded.
    /// Returns non-zero if the file was removed, zero if the file had not yet been loaded
    int unload(const wcstring &cmd);

    /// Check whether the given command could be loaded, but do not load it.
    bool can_load(const wcstring &cmd, const environment_t &vars);

    /// Invalidates all entries. Uesd when the underlying path variable changes.
    void invalidate();
};
#endif
