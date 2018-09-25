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

file_access_attempt_t access_file(const wcstring &path, int mode) {
    // fwprintf(stderr, L"Touch %ls\n", path.c_str());
    file_access_attempt_t result = {};
    struct stat statbuf;
    if (wstat(path, &statbuf)) {
        result.error = errno;
    } else {
        result.mod_time = statbuf.st_mtime;
        if (waccess(path, mode)) {
            result.error = errno;
        } else {
            result.accessible = true;
        }
    }

    // Note that we record the last checked time after the call, on the assumption that in a slow
    // filesystem, the lag comes before the kernel check, not after.
    result.stale = false;
    result.last_checked = time(NULL);
    return result;
}

autoload_t::autoload_t(wcstring env_var_name_var,
                       command_removed_function_t cmd_removed_callback)
    : env_var_name(std::move(env_var_name_var)), command_removed(cmd_removed_callback) {}

void autoload_t::entry_was_evicted(wcstring key, autoload_function_t node) {
    // This should only ever happen on the main thread.
    ASSERT_IS_MAIN_THREAD();

    // Tell ourselves that the command was removed if it was loaded.
    if (node.is_loaded) this->command_removed(std::move(key));
}

int autoload_t::unload(const wcstring &cmd) { return this->evict_node(cmd); }

int autoload_t::load(const wcstring &cmd, bool reload) {
    int res;
    CHECK_BLOCK(0);
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
        bool added;
        if (allow_eviction) {
            added = this->insert(cmd, autoload_function_t(false));
        } else {
            added = this->insert_no_eviction(cmd, autoload_function_t(false));
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
            return func->access.accessible;
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
        if (!access.accessible) {
            continue;
        }

        // Now we're actually going to take the lock.
        scoped_lock locker(lock);
        autoload_function_t *func = this->get(cmd);

        // Generate the source if we need to load it.
        bool need_to_load_function =
            really_load &&
            (func == NULL || func->access.mod_time != access.mod_time || !func->is_loaded);
        if (need_to_load_function) {
            // Generate the script source.
            script_source = L"source " + escape_string(path, ESCAPE_ALL);

            // Remove any loaded command because we are going to reload it. Note that this
            // will deadlock if command_removed calls back into us.
            if (func && func->is_loaded) {
                command_removed(cmd);
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
