// Functions for storing and retrieving function information. These functions also take care of
// autoloading functions in the $fish_function_path. Actual function evaluation is taken care of by
// the parser and to some degree the builtin handling library.
//
#include "config.h"  // IWYU pragma: keep

// IWYU pragma: no_include <type_traits>
#include <dirent.h>
#include <pthread.h>
#include <stddef.h>
#include <wchar.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "autoload.h"
#include "common.h"
#include "env.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "function.h"
#include "intern.h"
#include "parser.h"
#include "parser_keywords.h"
#include "reader.h"
#include "wutil.h"  // IWYU pragma: keep

class function_info_t {
public:
 /// Immutable properties of the function.
 std::shared_ptr<const function_properties_t> props;
 /// Function description. This may be changed after the function is created.
 wcstring description;
 /// File where this function was defined (intern'd string).
 const wchar_t *const definition_file;
 /// Mapping of all variables that were inherited from the function definition scope to their
 /// values.
 const std::map<wcstring, env_var_t> inherit_vars;
 /// Flag for specifying that this function was automatically loaded.
 const bool is_autoload;

 /// Constructs relevant information from the function_data.
 function_info_t(function_data_t data, const environment_t &vars, const wchar_t *filename,
                 bool autoload);

 /// Used by function_copy.
 function_info_t(const function_info_t &data, const wchar_t *filename, bool autoload);
};

/// Table containing all functions.
typedef std::unordered_map<wcstring, function_info_t> function_map_t;
static function_map_t loaded_functions;

/// Functions that shouldn't be autoloaded (anymore).
static std::unordered_set<wcstring> function_tombstones;

/// Lock for functions.
static std::recursive_mutex functions_lock;

static bool function_remove_ignore_autoload(const wcstring &name, bool tombstone = true);

/// Callback when an autoloaded function is removed.
void autoloaded_function_removed(const wcstring &cmd) {
    function_remove_ignore_autoload(cmd, false);
}

// Function autoloader
static autoload_t function_autoloader(L"fish_function_path", autoloaded_function_removed);

/// Kludgy flag set by the load function in order to tell function_add that the function being
/// defined is autoloaded. There should be a better way to do this...
static bool is_autoload = false;

/// Make sure that if the specified function is a dynamically loaded function, it has been fully
/// loaded.
static int load(const wcstring &name) {
    ASSERT_IS_MAIN_THREAD();
    scoped_rlock locker(functions_lock);
    bool was_autoload = is_autoload;
    int res;

    bool no_more_autoload = function_tombstones.count(name) > 0;
    if (no_more_autoload) return 0;

    function_map_t::iterator iter = loaded_functions.find(name);
    if (iter != loaded_functions.end() && !iter->second.is_autoload) {
        // We have a non-autoload version already.
        return 0;
    }

    is_autoload = true;
    res = function_autoloader.load(name, true);
    is_autoload = was_autoload;
    return res;
}

/// Insert a list of all dynamically loaded functions into the specified list.
static void autoload_names(std::unordered_set<wcstring> &names, int get_hidden) {
    size_t i;

    // TODO: justfy this.
    auto &vars = env_stack_t::principal();
    const auto path_var = vars.get(L"fish_function_path");
    if (path_var.missing_or_empty()) return;

    wcstring_list_t path_list;
    path_var->to_list(path_list);

    for (i = 0; i < path_list.size(); i++) {
        const wcstring &ndir_str = path_list.at(i);
        dir_t dir(ndir_str);
        if (!dir.valid()) continue;

        wcstring name;
        while (dir.read(name)) {
            const wchar_t *fn = name.c_str();
            const wchar_t *suffix;
            if (!get_hidden && fn[0] == L'_') continue;

            suffix = wcsrchr(fn, L'.');
            if (suffix && (wcscmp(suffix, L".fish") == 0)) {
                wcstring name(fn, suffix - fn);
                names.insert(name);
            }
        }
    }
}

static std::map<wcstring, env_var_t> snapshot_vars(const wcstring_list_t &vars,
                                                   const environment_t &src) {
    std::map<wcstring, env_var_t> result;
    for (const wcstring &name : vars) {
        auto var = src.get(name);
        if (var) result[name] = std::move(*var);
    }
    return result;
}

function_info_t::function_info_t(function_data_t data, const environment_t &vars,
                                 const wchar_t *filename, bool autoload)
    : props(std::make_shared<const function_properties_t>(std::move(data.props))),
      description(std::move(data.description)),
      definition_file(intern(filename)),
      inherit_vars(snapshot_vars(data.inherit_vars, vars)),
      is_autoload(autoload) {}

function_info_t::function_info_t(const function_info_t &data, const wchar_t *filename,
                                 bool autoload)
    : props(data.props),
      description(data.description),
      definition_file(intern(filename)),
      inherit_vars(data.inherit_vars),
      is_autoload(autoload) {}

void function_add(const function_data_t &data, const parser_t &parser) {
    UNUSED(parser);
    ASSERT_IS_MAIN_THREAD();

    CHECK(!data.name.empty(), );  //!OCLINT(multiple unary operator)
    scoped_rlock locker(functions_lock);

    // Remove the old function.
    function_remove(data.name);

    // Create and store a new function.
    const wchar_t *filename = reader_current_filename();

    const function_map_t::value_type new_pair(
        data.name, function_info_t(data, parser.vars(), filename, is_autoload));
    loaded_functions.insert(new_pair);

    // Add event handlers.
    for (const event_t &event : data.events) {
        event_add_handler(event);
    }
}

std::shared_ptr<const function_properties_t> function_get_properties(const wcstring &name) {
    if (parser_keywords_is_reserved(name)) return nullptr;
    scoped_rlock locker(functions_lock);
    auto where = loaded_functions.find(name);
    if (where != loaded_functions.end()) {
        return where->second.props;
    }
    return nullptr;
}

int function_exists(const wcstring &cmd) {
    if (parser_keywords_is_reserved(cmd)) return 0;
    scoped_rlock locker(functions_lock);
    load(cmd);
    return loaded_functions.find(cmd) != loaded_functions.end();
}

void function_load(const wcstring &cmd) {
    if (!parser_keywords_is_reserved(cmd)) {
        scoped_rlock locker(functions_lock);
        load(cmd);
    }
}

int function_exists_no_autoload(const wcstring &cmd, const environment_t &vars) {
    if (parser_keywords_is_reserved(cmd)) return 0;
    scoped_rlock locker(functions_lock);
    return loaded_functions.find(cmd) != loaded_functions.end() ||
           function_autoloader.can_load(cmd, vars);
}

static bool function_remove_ignore_autoload(const wcstring &name, bool tombstone) {
    // Note: the lock may be held at this point, but is recursive.
    scoped_rlock locker(functions_lock);

    function_map_t::iterator iter = loaded_functions.find(name);

    // Not found.  Not erasing.
    if (iter == loaded_functions.end()) return false;

    // Removing an auto-loaded function.  Prevent it from being auto-reloaded.
    if (iter->second.is_autoload && tombstone) function_tombstones.insert(name);

    loaded_functions.erase(iter);
    event_t ev(EVENT_ANY);
    ev.function_name = name;
    event_remove(ev);
    return true;
}

void function_remove(const wcstring &name) {
    if (function_remove_ignore_autoload(name)) function_autoloader.unload(name);
}

/// Returns a function by name if it has been loaded, returns false otherwise. Does not autoload.
static const function_info_t *function_get(const wcstring &name) {
    // The caller must lock the functions_lock before calling this; however our mutex is currently
    // recursive, so trylock will never fail. We need a way to correctly check if a lock is locked
    // (or better yet, make our lock non-recursive).
    // ASSERT_IS_LOCKED(functions_lock);
    function_map_t::iterator iter = loaded_functions.find(name);
    if (iter == loaded_functions.end()) {
        return NULL;
    }
    return &iter->second;
}

bool function_get_definition(const wcstring &name, wcstring &out_definition) {
    scoped_rlock locker(functions_lock);
    const function_info_t *func = function_get(name);
    if (func) {
        out_definition = func->props->body_node.get_source(func->props->parsed_source->src);
    }
    return func != NULL;
}

std::map<wcstring, env_var_t> function_get_inherit_vars(const wcstring &name) {
    scoped_rlock locker(functions_lock);
    const function_info_t *func = function_get(name);
    return func ? func->inherit_vars : std::map<wcstring, env_var_t>();
}

bool function_get_desc(const wcstring &name, wcstring &out_desc) {
    // Empty length string goes to NULL.
    scoped_rlock locker(functions_lock);
    const function_info_t *func = function_get(name);
    if (func && !func->description.empty()) {
        out_desc = _(func->description.c_str());
        return true;
    }

    return false;
}

void function_set_desc(const wcstring &name, const wcstring &desc) {
    load(name);
    scoped_rlock locker(functions_lock);
    function_map_t::iterator iter = loaded_functions.find(name);
    if (iter != loaded_functions.end()) {
        iter->second.description = desc;
    }
}

bool function_copy(const wcstring &name, const wcstring &new_name) {
    bool result = false;
    scoped_rlock locker(functions_lock);
    function_map_t::const_iterator iter = loaded_functions.find(name);
    if (iter != loaded_functions.end()) {
        // This new instance of the function shouldn't be tied to the definition file of the
        // original, so pass NULL filename, etc.
        const function_map_t::value_type new_pair(new_name,
                                                  function_info_t(iter->second, NULL, false));
        loaded_functions.insert(new_pair);
        result = true;
    }
    return result;
}

wcstring_list_t function_get_names(int get_hidden) {
    std::unordered_set<wcstring> names;
    scoped_rlock locker(functions_lock);
    autoload_names(names, get_hidden);

    for (const auto &func : loaded_functions) {
        const wcstring &name = func.first;

        // Maybe skip hidden.
        if (!get_hidden && (name.empty() || name.at(0) == L'_')) {
            continue;
        }
        names.insert(name);
    }
    return wcstring_list_t(names.begin(), names.end());
}

const wchar_t *function_get_definition_file(const wcstring &name) {
    scoped_rlock locker(functions_lock);
    const function_info_t *func = function_get(name);
    return func ? func->definition_file : NULL;
}

bool function_is_autoloaded(const wcstring &name) {
    scoped_rlock locker(functions_lock);
    const function_info_t *func = function_get(name);
    return func->is_autoload;
}

int function_get_definition_lineno(const wcstring &name) {
    scoped_rlock locker(functions_lock);
    const function_info_t *func = function_get(name);
    if (!func) return -1;
    // return one plus the number of newlines at offsets less than the start of our function's
    // statement (which includes the header).
    // TODO: merge with line_offset_of_character_at_offset?
    auto block_stat = func->props->body_node.try_get_parent<grammar::block_statement>();
    assert(block_stat && "Function body is not part of block statement");
    auto source_range = block_stat.source_range();
    assert(source_range && "Function has no source range");
    uint32_t func_start = source_range->start;
    const wcstring &source = func->props->parsed_source->src;
    assert(func_start <= source.size() && "function start out of bounds");
    return 1 + std::count(source.begin(), source.begin() + func_start, L'\n');
}

void function_invalidate_path() { function_autoloader.invalidate(); }

// Setup the environment for the function. There are three components of the environment:
// 1. argv
// 2. named arguments
// 3. inherited variables
void function_prepare_environment(env_stack_t &vars, const wcstring &name,
                                  const wchar_t *const *argv,
                                  const std::map<wcstring, env_var_t> &inherited_vars) {
    vars.set_argv(argv);
    auto props = function_get_properties(name);
    if (props && !props->named_arguments.empty()) {
        const wchar_t *const *arg = argv;
        for (const wcstring &named_arg : props->named_arguments) {
            if (*arg) {
                vars.set_one(named_arg, ENV_LOCAL | ENV_USER, *arg);
                arg++;
            } else {
                vars.set_empty(named_arg, ENV_LOCAL | ENV_USER);
            }
        }
    }

    for (const auto &kv : inherited_vars) {
        vars.set(kv.first, ENV_LOCAL | ENV_USER, kv.second.as_list());
    }
}
