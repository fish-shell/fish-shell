// The classes responsible for autoloading functions and completions.
#include "config.h"  // IWYU pragma: keep

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "autoload.h"
#include "common.h"
#include "env.h"
#include "exec.h"
#include "wutil.h"  // IWYU pragma: keep

// The time before we'll recheck an autoloaded file.
static const int kAutoloadStalenessInterval = 15;

file_access_attempt_t access_file(const wcstring &path, int mode) {
    // printf("Touch %ls\n", path.c_str());
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

autoload_t::autoload_t(const wcstring &env_var_name_var, const builtin_script_t *const scripts,
                       size_t script_count)
    : lock(),
      env_var_name(env_var_name_var),
      builtin_scripts(scripts),
      builtin_script_count(script_count) {
    pthread_mutex_init(&lock, NULL);
}

autoload_t::~autoload_t() { pthread_mutex_destroy(&lock); }

void autoload_t::node_was_evicted(autoload_function_t *node) {
    // This should only ever happen on the main thread.
    ASSERT_IS_MAIN_THREAD();

    // Tell ourselves that the command was removed if it was loaded.
    if (node->is_loaded) this->command_removed(node->key);
    delete node;
}

int autoload_t::unload(const wcstring &cmd) { return this->evict_node(cmd); }

int autoload_t::load(const wcstring &cmd, bool reload) {
    int res;
    CHECK_BLOCK(0);
    ASSERT_IS_MAIN_THREAD();

    env_var_t path_var = env_get_string(env_var_name);

    // Do we know where to look?
    if (path_var.empty()) return 0;

    // Check if the lookup path has changed. If so, drop all loaded files. path_var may only be
    // inspected on the main thread.
    if (path_var != this->last_path) {
        this->last_path = path_var;
        this->last_path_tokenized.clear();
        tokenize_variable_array(this->last_path, this->last_path_tokenized);

        scoped_lock locker(lock);
        this->evict_all_nodes();
    }

    // Mark that we're loading this. Hang onto the iterator for fast erasing later. Note that
    // std::set has guarantees about not invalidating iterators, so this is safe to do across the
    // callouts below.
    typedef std::set<wcstring>::iterator set_iterator_t;
    std::pair<set_iterator_t, bool> insert_result = is_loading_set.insert(cmd);
    set_iterator_t where = insert_result.first;
    bool inserted = insert_result.second;

    // Warn and fail on infinite recursion. It's OK to do this because this function is only called
    // on the main thread.
    if (!inserted) {
        // We failed to insert.
        debug(0, _(L"Could not autoload item '%ls', it is already being autoloaded. "
                   L"This is a circular dependency in the autoloading scripts, please remove it."),
              cmd.c_str());
        return 1;
    }
    // Try loading it.
    res = this->locate_file_and_maybe_load_it(cmd, true, reload, this->last_path_tokenized);
    // Clean up.
    is_loading_set.erase(where);
    return res;
}

bool autoload_t::can_load(const wcstring &cmd, const env_vars_snapshot_t &vars) {
    const env_var_t path_var = vars.get(env_var_name);
    if (path_var.missing_or_empty()) return false;

    std::vector<wcstring> path_list;
    tokenize_variable_array(path_var, path_list);
    return this->locate_file_and_maybe_load_it(cmd, false, false, path_list);
}

static bool script_name_precedes_script_name(const builtin_script_t &script1,
                                             const builtin_script_t &script2) {
    return wcscmp(script1.name, script2.name) < 0;
}

/// Check whether the given command is loaded.
bool autoload_t::has_tried_loading(const wcstring &cmd) {
    scoped_lock locker(lock);
    autoload_function_t *func = this->get_node(cmd);
    return func != NULL;
}

static bool is_stale(const autoload_function_t *func) {
    // Return whether this function is stale. Internalized functions can never be stale.
    return !func->is_internalized &&
           time(NULL) - func->access.last_checked > kAutoloadStalenessInterval;
}

autoload_function_t *autoload_t::get_autoloaded_function_with_creation(const wcstring &cmd,
                                                                       bool allow_eviction) {
    ASSERT_IS_LOCKED(lock);
    autoload_function_t *func = this->get_node(cmd);
    if (!func) {
        func = new autoload_function_t(cmd);
        if (allow_eviction) {
            this->add_node(func);
        } else {
            this->add_node_without_eviction(func);
        }
    }
    return func;
}

/// This internal helper function does all the real work. By using two functions, the internal
/// function can return on various places in the code, and the caller can take care of various
/// cleanup work.
///
///   cmd: the command name ('grep')
///   really_load: whether to actually parse it as a function, or just check it it exists
///   reload: whether to reload it if it's already loaded
///   path_list: the set of paths to check
///
/// Result: if really_load is true, returns whether the function was loaded. Otherwise returns
/// whether the function existed.
bool autoload_t::locate_file_and_maybe_load_it(const wcstring &cmd, bool really_load, bool reload,
                                               const wcstring_list_t &path_list) {
    // Note that we are NOT locked in this function!
    bool reloaded = 0;

    // Try using a cached function. If we really want the function to be loaded, require that it be
    // really loaded. If we're not reloading, allow stale functions.
    {
        bool allow_stale_functions = !reload;

        scoped_lock locker(lock);
        autoload_function_t *func = this->get_node(cmd);  // get the function

        // Determine if we can use this cached function.
        bool use_cached;
        if (!func) {
            // Can't use a function that doesn't exist.
            use_cached = false;
        } else if (really_load && !func->is_placeholder && !func->is_loaded) {
            use_cached = false;  // can't use an unloaded function
        } else if (!allow_stale_functions && is_stale(func)) {
            use_cached = false;  // can't use a stale function
        } else {
            use_cached = true;  // I guess we can use it
        }

        // If we can use this function, return whether we were able to access it.
        if (use_cached) {
            assert(func != NULL);
            return func->is_internalized || func->access.accessible;
        }
    }
    // The source of the script will end up here.
    wcstring script_source;
    bool has_script_source = false;

    // Whether we found an accessible file.
    bool found_file = false;

    // Look for built-in scripts via a binary search.
    const builtin_script_t *matching_builtin_script = NULL;
    if (builtin_script_count > 0) {
        const builtin_script_t test_script = {cmd.c_str(), NULL};
        const builtin_script_t *array_end = builtin_scripts + builtin_script_count;
        const builtin_script_t *found = std::lower_bound(builtin_scripts, array_end, test_script,
                                                         script_name_precedes_script_name);
        if (found != array_end && !wcscmp(found->name, test_script.name)) {
            matching_builtin_script = found;
        }
    }
    if (matching_builtin_script) {
        has_script_source = true;
        script_source = str2wcstring(matching_builtin_script->def);

        // Make a node representing this function.
        scoped_lock locker(lock);
        autoload_function_t *func = this->get_autoloaded_function_with_creation(cmd, really_load);

        // This function is internalized.
        func->is_internalized = true;

        // It's a fiction to say the script is loaded at this point, but we're definitely going to
        // load it down below.
        if (really_load) func->is_loaded = true;
    }

    if (!has_script_source) {
        // Iterate over path searching for suitable completion files.
        for (size_t i = 0; i < path_list.size(); i++) {
            wcstring next = path_list.at(i);
            wcstring path = next + L"/" + cmd + L".fish";

            const file_access_attempt_t access = access_file(path, R_OK);
            if (access.accessible) {
                found_file = true;

                // Now we're actually going to take the lock.
                scoped_lock locker(lock);
                autoload_function_t *func = this->get_node(cmd);

                // Generate the source if we need to load it.
                bool need_to_load_function =
                    really_load &&
                    (func == NULL || func->access.mod_time != access.mod_time || !func->is_loaded);
                if (need_to_load_function) {
                    // Generate the script source.
                    wcstring esc = escape_string(path, 1);
                    script_source = L"source " + esc;
                    has_script_source = true;

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
                if (!func) {
                    func = get_autoloaded_function_with_creation(cmd, really_load);
                }

                // It's a fiction to say the script is loaded at this point, but we're definitely
                // going to load it down below.
                if (need_to_load_function) func->is_loaded = true;

                // Unconditionally record our access time.
                func->access = access;

                break;
            }
        }

        // If no file or builtin script was found we insert a placeholder function. Later we only
        // research if the current time is at least five seconds later. This way, the files won't be
        // searched over and over again.
        if (!found_file && !has_script_source) {
            scoped_lock locker(lock);
            // Generate a placeholder.
            autoload_function_t *func = this->get_node(cmd);
            if (!func) {
                func = new autoload_function_t(cmd);
                func->is_placeholder = true;
                if (really_load) {
                    this->add_node(func);
                } else {
                    this->add_node_without_eviction(func);
                }
            }
            func->access.last_checked = time(NULL);
        }
    }

    // If we have a script, either built-in or a file source, then run it.
    if (really_load && has_script_source) {
        // Do nothing on failure.
        exec_subshell(script_source, false /* do not apply exit status */);
    }

    if (really_load) {
        return reloaded;
    } else {
        return found_file || has_script_source;
    }
}
