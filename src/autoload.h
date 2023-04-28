#if 0

// The classes responsible for autoloading functions and completions.
#ifndef FISH_AUTOLOAD_H
#define FISH_AUTOLOAD_H

#include "config.h"  // IWYU pragma: keep

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "common.h"
#include "env.h"
#include "maybe.h"
#include "parser.h"
#include "wutil.h"

class autoload_file_cache_t;

/// autoload_t is a class that knows how to autoload .fish files from a list of directories. This
/// is used by autoloading functions and completions. It maintains a file cache, which is
/// responsible for potentially cached accesses of files, and then a list of files that have
/// actually been autoloaded. A client may request a file to autoload given a command name, and may
/// be returned a path which it is expected to source.
/// autoload_t does not have any internal locks; it is the responsibility of the caller to lock
/// it.
class autoload_t {
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

    /// Invalidate any underlying cache.
    /// This is exposed for testing.
    void invalidate_cache();

    /// Like resolve_autoload(), but accepts the paths directly.
    /// This is exposed for testing.
    maybe_t<wcstring> resolve_command(const wcstring &cmd, const std::vector<wcstring> &paths);

   public:
    /// Construct an autoloader that loads from the paths given by \p env_var_name.
    explicit autoload_t(wcstring env_var_name);

    autoload_t(autoload_t &&);
    ~autoload_t();

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
    static void perform_autoload(const wcstring &path, parser_t &parser);

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

    /// \return whether autoloading has been attempted for a command.
    bool has_attempted_autoload(const wcstring &cmd);

    /// \return the names of all commands that have been autoloaded. Note this includes "in-flight"
    /// commands.
    std::vector<wcstring> get_autoloaded_commands() const;

    /// Mark that all autoloaded files have been forgotten.
    /// Future calls to path_to_autoload() will return previously-returned paths.
    void clear() {
        // Note there is no reason to invalidate the cache here.
        autoloaded_files_.clear();
    }
};

#endif
#endif
