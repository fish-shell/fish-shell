// The classes responsible for autoloading functions and completions.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "autoload.h"
#include "common.h"
#include "env.h"
#include "exec.h"
#include "parser.h"
#include "wutil.h"  // IWYU pragma: keep

/// The time before we'll recheck an autoloaded file.
static const int kAutoloadStalenessInterval = 15;

/// Represents a file that we might want to autoload.
struct autoloadable_file_t {
    /// The path to the file.
    wcstring path;

    /// The metadata for the file.
    file_id_t file_id;
};

/// Class representing a cache of files that may be autoloaded.
/// This is responsible for performing cached accesses to a set of paths.
class autoload_file_cache_t {
    /// A timestamp is a monotonic point in time.
    using timestamp_t = std::chrono::time_point<std::chrono::steady_clock>;

    /// The directories from which to load.
    const wcstring_list_t dirs_;

    /// Our LRU cache of checks that were misses.
    /// The key is the command, the  value is the time of the check.
    struct misses_lru_cache_t : public lru_cache_t<misses_lru_cache_t, timestamp_t> {};
    misses_lru_cache_t misses_cache_;

    /// The set of files that we have returned to the caller, along with the time of the check.
    /// The key is the command (not the path).
    struct known_file_t {
        autoloadable_file_t file;
        timestamp_t last_checked;
    };
    std::unordered_map<wcstring, known_file_t> known_files_;

    /// \return the current timestamp.
    static timestamp_t current_timestamp() { return std::chrono::steady_clock::now(); }

    /// \return whether a timestamp is fresh enough to use.
    static bool is_fresh(timestamp_t then, timestamp_t now);

    /// Attempt to find an autoloadable file by searching our path list for a given comand.
    /// \return the file, or none() if none.
    maybe_t<autoloadable_file_t> locate_file(const wcstring &cmd) const;

   public:
    /// Initialize with a set of directories.
    explicit autoload_file_cache_t(wcstring_list_t dirs) : dirs_(std::move(dirs)) {}

    /// Initialize with empty directories.
    autoload_file_cache_t() = default;

    /// \return the directories.
    const wcstring_list_t &dirs() const { return dirs_; }

    /// Check if a command \p cmd can be loaded.
    /// If \p allow_stale is true, allow stale entries; otherwise discard them.
    /// This returns an autoloadable file, or none() if there is no such file.
    maybe_t<autoloadable_file_t> check(const wcstring &cmd, bool allow_stale = false);
};

maybe_t<autoloadable_file_t> autoload_file_cache_t::locate_file(const wcstring &cmd) const {
    // Re-use the storage for path.
    wcstring path;
    for (const wcstring &dir : dirs()) {
        // Construct the path as dir/cmd.fish
        path = dir;
        path += L"/";
        path += cmd;
        path += L".fish";

        file_id_t file_id = file_id_for_path(path);
        if (file_id != kInvalidFileID) {
            // Found it.
            autoloadable_file_t result;
            result.path = std::move(path);
            result.file_id = file_id;
            return result;
        }
    }
    return none();
}

bool autoload_file_cache_t::is_fresh(timestamp_t then, timestamp_t now) {
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now - then);
    return seconds.count() < kAutoloadStalenessInterval;
}

maybe_t<autoloadable_file_t> autoload_file_cache_t::check(const wcstring &cmd, bool allow_stale) {
    const auto now = current_timestamp();

    // Check hits.
    auto iter = known_files_.find(cmd);
    if (iter != known_files_.end()) {
        if (allow_stale || is_fresh(iter->second.last_checked, now)) {
            // Re-use this cached hit.
            return iter->second.file;
        }
        // The file is stale, remove it.
        known_files_.erase(iter);
    }

    // Check misses.
    if (timestamp_t *miss = misses_cache_.get(cmd)) {
        if (allow_stale || is_fresh(*miss, now)) {
            // Re-use this cached miss.
            return none();
        }
        // The miss is stale, remove it.
        misses_cache_.evict_node(cmd);
    }

    // We couldn't satisfy this request from the cache. Hit the disk.
    // Don't re-use 'now', the disk access could have taken a long time.
    maybe_t<autoloadable_file_t> file = locate_file(cmd);
    if (file.has_value()) {
        auto ins = known_files_.emplace(cmd, known_file_t{*file, current_timestamp()});
        assert(ins.second && "Known files cache should not have contained this cmd");
        (void)ins;
    } else {
        bool ins = misses_cache_.insert(cmd, current_timestamp());
        assert(ins && "Misses cache should not have contained this cmd");
        (void)ins;
    }
    return file;
}

autoloader_t::autoloader_t(wcstring env_var_name)
    : env_var_name_(std::move(env_var_name)), cache_(make_unique<autoload_file_cache_t>()) {}

autoloader_t::~autoloader_t() = default;

bool autoloader_t::can_autoload(const wcstring &cmd) {
    return cache_->check(cmd, true /* allow stale */).has_value();
}

maybe_t<wcstring> autoloader_t::resolve_command(const wcstring &cmd, const environment_t &env) {
    // Are we currently in the process of autoloading this?
    if (current_autoloading_.count(cmd) > 0) return none();

    // Check to see if our paths have changed. If so, replace our cache.
    // Note we don't have to modify autoloadable_files_. We'll naturally detect if those have
    // changed when we query the cache.
    maybe_t<env_var_t> mvar = env.get(env_var_name_);
    const wcstring_list_t empty;
    const wcstring_list_t &paths = mvar ? mvar->as_list() : empty;
    if (paths != cache_->dirs()) {
        cache_ = make_unique<autoload_file_cache_t>(paths);
    }

    // Do we have an entry to load?
    auto mfile = cache_->check(cmd);
    if (!mfile) return none();

    // Is this file the same as what we previously autoloaded?
    auto iter = autoloaded_files_.find(cmd);
    if (iter != autoloaded_files_.end() && iter->second == mfile->file_id) {
        // The file has been autoloaded and is unchanged.
        return none();
    }

    // We're going to (tell our caller to) autoload this command.
    current_autoloading_.insert(cmd);
    autoloaded_files_[cmd] = mfile->file_id;
    return std::move(mfile->path);
}

file_access_attempt_t access_file(const wcstring &path, int mode) {
    file_access_attempt_t result = {};
    file_id_t file_id = file_id_for_path(path);
    if (file_id != kInvalidFileID && 0 == waccess(path, mode)) {
        result.file_id = file_id;
    }
    result.last_checked = time(NULL);
    return result;
}

void autoloader_t::perform_autoload(const wcstring &path) {
    wcstring script_source = L"source " + escape_string(path, ESCAPE_ALL);
    exec_subshell(script_source, parser_t::principal_parser(),
                  false /* do not apply exit status */);
}

autoload_t::autoload_t(wcstring env_var_name_var) : env_var_name(std::move(env_var_name_var)) {}

void autoload_t::entry_was_evicted(wcstring key, autoload_function_t node) {
    // This should only ever happen on the main thread.
    ASSERT_IS_MAIN_THREAD();
}

int autoload_t::unload(const wcstring &cmd) { return this->evict_node(cmd); }

int autoload_t::load(const wcstring &cmd, bool reload) {
    int res;
    ASSERT_IS_MAIN_THREAD();

    // TODO: Justify this principal_parser.
    auto &parser = parser_t::principal_parser();
    auto &vars = parser.vars();

    if (!this->paths) {
        auto path_var = vars.get(env_var_name);
        if (path_var.missing_or_empty()) return 0;
        this->paths = path_var->as_list();
    }

    // Mark that we're loading this. Hang onto the iterator for fast erasing later. Note that
    // std::set has guarantees about not invalidating iterators, so this is safe to do across the
    // callouts below.
    auto insert_result = is_loading_set.insert(cmd);
    auto where = insert_result.first;
    bool inserted = insert_result.second;

    // Warn and fail on infinite recursion. It's OK to do this because this function is only called
    // on the main thread.
    if (!inserted) {
        // We failed to insert.
        const wchar_t *fmt =
            _(L"Could not autoload item '%ls', it is already being autoloaded. "
              L"This is a circular dependency in the autoloading scripts, please remove it.");
        debug(0, fmt, cmd.c_str());
        return 1;
    }
    // Try loading it.
    assert(paths && "Should have paths");
    res = this->locate_file_and_maybe_load_it(cmd, true, reload, *this->paths);
    // Clean up.
    is_loading_set.erase(where);
    return res;
}

bool autoload_t::can_load(const wcstring &cmd, const environment_t &vars) {
    auto path_var = vars.get(env_var_name);
    if (path_var.missing_or_empty()) return false;

    std::vector<wcstring> path_list;
    path_var->to_list(path_list);
    return this->locate_file_and_maybe_load_it(cmd, false, false, path_list);
}

void autoload_t::invalidate() {
    ASSERT_IS_MAIN_THREAD();
    scoped_lock locker(lock);
    paths.reset();
    this->evict_all_nodes();
}

/// Check whether the given command is loaded.
bool autoload_t::has_tried_loading(const wcstring &cmd) {
    scoped_lock locker(lock);
    autoload_function_t *func = this->get(cmd);
    return func != NULL;
}

/// @return Whether this function is stale.
static bool is_stale(const autoload_function_t *func) {
    return time(NULL) - func->access.last_checked > kAutoloadStalenessInterval;
}

autoload_function_t *autoload_t::get_autoloaded_function_with_creation(const wcstring &cmd,
                                                                       bool allow_eviction) {
    ASSERT_IS_LOCKED(lock);
    autoload_function_t *func = this->get(cmd);
    if (!func) {
        if (allow_eviction) {
            this->insert(cmd, autoload_function_t(false));
        } else {
            this->insert_no_eviction(cmd, autoload_function_t(false));
        }
        func = this->get(cmd);
        assert(func);
    }
    return func;
}

static bool use_cached(autoload_function_t *func, bool really_load, bool allow_stale_functions) {
    if (!func) {
        return false;  // can't use a function that doesn't exist
    }
    if (really_load && !func->is_placeholder && !func->is_loaded) {
        return false;  // can't use an unloaded function
    }
    if (!allow_stale_functions && is_stale(func)) {
        return false;  // can't use a stale function
    }
    return true;  // I guess we can use it
}

/// This internal helper function does all the real work. By using two functions, the internal
/// function can return on various places in the code, and the caller can take care of various
/// cleanup work.
/// @param cmd the command name ('grep')
/// @param really_load Whether to actually parse it as a function, or just check it it exists
/// @param reload Whether to reload it if it's already loaded
/// @param path_list The set of paths to check
/// @return If really_load is true, returns whether the function was loaded. Otherwise returns
///         whether the function existed.
bool autoload_t::locate_file_and_maybe_load_it(const wcstring &cmd, bool really_load, bool reload,
                                               const wcstring_list_t &path_list) {
    // Note that we are NOT locked in this function!
    bool reloaded = false;

    // Try using a cached function. If we really want the function to be loaded, require that it be
    // really loaded. If we're not reloading, allow stale functions.
    {
        bool allow_stale_functions = !reload;
        scoped_lock locker(lock);
        autoload_function_t *func = this->get(cmd);  // get the function

        // If we can use this function, return whether we were able to access it.
        if (use_cached(func, really_load, allow_stale_functions)) {
            return func->access.accessible();
        }
    }

    // The source of the script will end up here.
    wcstring script_source;

    // Whether we found an accessible file.
    bool found_file = false;

    // Iterate over path searching for suitable completion files.
    for (size_t i = 0; i < path_list.size() && !found_file; i++) {
        wcstring next = path_list.at(i);
        wcstring path = next + L"/" + cmd + L".fish";

        const file_access_attempt_t access = access_file(path, R_OK);
        if (!access.accessible()) {
            continue;
        }

        // Now we're actually going to take the lock.
        scoped_lock locker(lock);
        autoload_function_t *func = this->get(cmd);

        // Generate the source if we need to load it.
        bool need_to_load_function =
            really_load &&
            (func == NULL || func->access.file_id != access.file_id || !func->is_loaded);
        if (need_to_load_function) {
            // Generate the script source.
            script_source = L"source " + escape_string(path, ESCAPE_ALL);

            // Mark that our function is no longer a placeholder, because we will load it.
            if (func) {
                func->is_placeholder = false;
            }

            // Mark that we're reloading it.
            reloaded = true;
        }

        // Create the function if we haven't yet. This does not load it. Do not trigger
        // eviction unless we are actually loading, because we don't want to evict off of
        // the main thread.
        if (!func) func = get_autoloaded_function_with_creation(cmd, really_load);

        // It's a fiction to say the script is loaded at this point, but we're definitely
        // going to load it down below.
        if (need_to_load_function) func->is_loaded = true;

        // Unconditionally record our access time.
        func->access = access;
        found_file = true;
    }

    // If no file or builtin script was found we insert a placeholder function. Later we only
    // research if the current time is at least five seconds later. This way, the files won't be
    // searched over and over again.
    if (!found_file && script_source.empty()) {
        scoped_lock locker(lock);
        // Generate a placeholder.
        autoload_function_t *func = this->get(cmd);
        if (!func) {
            if (really_load) {
                this->insert(cmd, autoload_function_t(true));
            } else {
                this->insert_no_eviction(cmd, autoload_function_t(true));
            }
            func = this->get(cmd);
            assert(func);
        }
        func->access.last_checked = time(NULL);
    }

    // If we have a script, either built-in or a file source, then run it.
    if (really_load && !script_source.empty()) {
        // Do nothing on failure.
        // TODO: rationalize this use of principal_parser, or inject the loading from outside.
        exec_subshell(script_source, parser_t::principal_parser(),
                      false /* do not apply exit status */);
    }

    if (really_load) {
        return reloaded;
    }

    return found_file || !script_source.empty();
}
