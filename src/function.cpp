/** \file function.c

    Prototypes for functions for storing and retrieving function
  information. These functions also take care of autoloading
  functions in the $fish_function_path. Actual function evaluation
  is taken care of by the parser and to some degree the builtin
  handling library.
*/
// IWYU pragma: no_include <type_traits>
#include <wchar.h>
#include <pthread.h>
#include <map>
#include <set>
#include <dirent.h>
#include <stddef.h>
#include <string>
#include <utility>
#include <memory>

#include "wutil.h"  // IWYU pragma: keep
#include "fallback.h" // IWYU pragma: keep
#include "autoload.h"
#include "function.h"
#include "common.h"
#include "intern.h"
#include "event.h"
#include "reader.h"
#include "parser_keywords.h"
#include "env.h"

/**
   Table containing all functions
*/
typedef std::map<wcstring, function_info_t> function_map_t;
static function_map_t loaded_functions;

/**
   Functions that shouldn't be autoloaded (anymore).
*/
static std::set<wcstring> function_tombstones;

/* Lock for functions */
static pthread_mutex_t functions_lock;

/* Autoloader for functions */
class function_autoload_t : public autoload_t
{
public:
    function_autoload_t();
    virtual void command_removed(const wcstring &cmd);
};

static function_autoload_t function_autoloader;

/** Constructor */
function_autoload_t::function_autoload_t() : autoload_t(L"fish_function_path", NULL, 0)
{
}

static bool function_remove_ignore_autoload(const wcstring &name, bool tombstone = true);

/** Callback when an autoloaded function is removed */
void function_autoload_t::command_removed(const wcstring &cmd)
{
    function_remove_ignore_autoload(cmd, false);
}

/**
   Kludgy flag set by the load function in order to tell function_add
   that the function being defined is autoloaded. There should be a
   better way to do this...
*/
static bool is_autoload = false;

/**
   Make sure that if the specified function is a dynamically loaded
   function, it has been fully loaded.
*/
static int load(const wcstring &name)
{
    ASSERT_IS_MAIN_THREAD();
    scoped_lock lock(functions_lock);
    bool was_autoload = is_autoload;
    int res;

    bool no_more_autoload = function_tombstones.count(name) > 0;
    if (no_more_autoload)
        return 0;

    function_map_t::iterator iter = loaded_functions.find(name);
    if (iter !=  loaded_functions.end() && !iter->second.is_autoload)
    {
        /* We have a non-autoload version already */
        return 0;
    }

    is_autoload = true;
    res = function_autoloader.load(name, true);
    is_autoload = was_autoload;
    return res;
}

/**
   Insert a list of all dynamically loaded functions into the
   specified list.
*/
static void autoload_names(std::set<wcstring> &names, int get_hidden)
{
    size_t i;

    const env_var_t path_var_wstr =  env_get_string(L"fish_function_path");
    if (path_var_wstr.missing())
        return;
    const wchar_t *path_var = path_var_wstr.c_str();

    wcstring_list_t path_list;

    tokenize_variable_array(path_var, path_list);
    for (i=0; i<path_list.size(); i++)
    {
        const wcstring &ndir_str = path_list.at(i);
        const wchar_t *ndir = (wchar_t *)ndir_str.c_str();
        DIR *dir = wopendir(ndir);
        if (!dir)
            continue;

        wcstring name;
        while (wreaddir(dir, name))
        {
            const wchar_t *fn = name.c_str();
            const wchar_t *suffix;
            if (!get_hidden && fn[0] == L'_')
                continue;

            suffix = wcsrchr(fn, L'.');
            if (suffix && (wcscmp(suffix, L".fish") == 0))
            {
                wcstring name(fn, suffix - fn);
                names.insert(name);
            }
        }
        closedir(dir);
    }
}

void function_init()
{
    /* PCA: This recursive lock was introduced early in my work. I would like to make this a non-recursive lock but I haven't fully investigated all the call paths (for autoloading functions, etc.) */
    pthread_mutexattr_t a;
    VOMIT_ON_FAILURE(pthread_mutexattr_init(&a));
    VOMIT_ON_FAILURE(pthread_mutexattr_settype(&a,PTHREAD_MUTEX_RECURSIVE));
    VOMIT_ON_FAILURE(pthread_mutex_init(&functions_lock, &a));
    VOMIT_ON_FAILURE(pthread_mutexattr_destroy(&a));
}

static std::map<wcstring,env_var_t> snapshot_vars(const wcstring_list_t &vars)
{
    std::map<wcstring,env_var_t> result;
    for (wcstring_list_t::const_iterator it = vars.begin(), end = vars.end(); it != end; ++it)
    {
        result.insert(std::make_pair(*it, env_get_string(*it)));
    }
    return result;
}

function_info_t::function_info_t(const function_data_t &data, const wchar_t *filename, int def_offset, bool autoload) :
    definition(data.definition),
    description(data.description),
    definition_file(intern(filename)),
    definition_offset(def_offset),
    named_arguments(data.named_arguments),
    inherit_vars(snapshot_vars(data.inherit_vars)),
    is_autoload(autoload),
    shadows(data.shadows)
{
}

function_info_t::function_info_t(const function_info_t &data, const wchar_t *filename, int def_offset, bool autoload) :
    definition(data.definition),
    description(data.description),
    definition_file(intern(filename)),
    definition_offset(def_offset),
    named_arguments(data.named_arguments),
    inherit_vars(data.inherit_vars),
    is_autoload(autoload),
    shadows(data.shadows)
{
}

void function_add(const function_data_t &data, const parser_t &parser, int definition_line_offset)
{
    ASSERT_IS_MAIN_THREAD();

    CHECK(! data.name.empty(),);
    CHECK(data.definition,);
    scoped_lock lock(functions_lock);

    /* Remove the old function */
    function_remove(data.name);

    /* Create and store a new function */
    const wchar_t *filename = reader_current_filename();

    const function_map_t::value_type new_pair(data.name, function_info_t(data, filename, definition_line_offset, is_autoload));
    loaded_functions.insert(new_pair);

    /* Add event handlers */
    for (std::vector<event_t>::const_iterator iter = data.events.begin(); iter != data.events.end(); ++iter)
    {
        event_add_handler(*iter);
    }
}

int function_exists(const wcstring &cmd)
{
    if (parser_keywords_is_reserved(cmd))
        return 0;
    scoped_lock lock(functions_lock);
    load(cmd);
    return loaded_functions.find(cmd) != loaded_functions.end();
}

void function_load(const wcstring &cmd)
{
    if (! parser_keywords_is_reserved(cmd))
    {
        scoped_lock lock(functions_lock);
        load(cmd);
    }
}

int function_exists_no_autoload(const wcstring &cmd, const env_vars_snapshot_t &vars)
{
    if (parser_keywords_is_reserved(cmd))
        return 0;
    scoped_lock lock(functions_lock);
    return loaded_functions.find(cmd) != loaded_functions.end() || function_autoloader.can_load(cmd, vars);
}

static bool function_remove_ignore_autoload(const wcstring &name, bool tombstone)
{
    // Note: the lock may be held at this point, but is recursive
    scoped_lock lock(functions_lock);

    function_map_t::iterator iter = loaded_functions.find(name);

    // not found.  not erasing.
    if (iter == loaded_functions.end())
        return false;

    // removing an auto-loaded function.  prevent it from being
    // auto-reloaded.
    if (iter->second.is_autoload && tombstone)
        function_tombstones.insert(name);

    loaded_functions.erase(iter);

    event_t ev(EVENT_ANY);
    ev.function_name=name;
    event_remove(ev);

    return true;
}

void function_remove(const wcstring &name)
{
    if (function_remove_ignore_autoload(name))
        function_autoloader.unload(name);
}

static const function_info_t *function_get(const wcstring &name)
{
    // The caller must lock the functions_lock before calling this; however our mutex is currently recursive, so trylock will never fail
    // We need a way to correctly check if a lock is locked (or better yet, make our lock non-recursive)
    //ASSERT_IS_LOCKED(functions_lock);
    function_map_t::iterator iter = loaded_functions.find(name);
    if (iter == loaded_functions.end())
    {
        return NULL;
    }
    else
    {
        return &iter->second;
    }
}

bool function_get_definition(const wcstring &name, wcstring *out_definition)
{
    scoped_lock lock(functions_lock);
    const function_info_t *func = function_get(name);
    if (func && out_definition)
    {
        out_definition->assign(func->definition);
    }
    return func != NULL;
}

wcstring_list_t function_get_named_arguments(const wcstring &name)
{
    scoped_lock lock(functions_lock);
    const function_info_t *func = function_get(name);
    return func ? func->named_arguments : wcstring_list_t();
}

std::map<wcstring,env_var_t> function_get_inherit_vars(const wcstring &name)
{
    scoped_lock lock(functions_lock);
    const function_info_t *func = function_get(name);
    return func ? func->inherit_vars : std::map<wcstring,env_var_t>();
}

int function_get_shadows(const wcstring &name)
{
    scoped_lock lock(functions_lock);
    const function_info_t *func = function_get(name);
    return func ? func->shadows : false;
}


bool function_get_desc(const wcstring &name, wcstring *out_desc)
{
    /* Empty length string goes to NULL */
    scoped_lock lock(functions_lock);
    const function_info_t *func = function_get(name);
    if (out_desc && func && ! func->description.empty())
    {
        out_desc->assign(_(func->description.c_str()));
        return true;
    }
    else
    {
        return false;
    }
}

void function_set_desc(const wcstring &name, const wcstring &desc)
{
    load(name);
    scoped_lock lock(functions_lock);
    function_map_t::iterator iter = loaded_functions.find(name);
    if (iter != loaded_functions.end())
    {
        iter->second.description = desc;
    }
}

bool function_copy(const wcstring &name, const wcstring &new_name)
{
    bool result = false;
    scoped_lock lock(functions_lock);
    function_map_t::const_iterator iter = loaded_functions.find(name);
    if (iter != loaded_functions.end())
    {
        // This new instance of the function shouldn't be tied to the definition file of the original, so pass NULL filename, etc.
        const function_map_t::value_type new_pair(new_name, function_info_t(iter->second, NULL, 0, false));
        loaded_functions.insert(new_pair);
        result = true;
    }
    return result;
}

wcstring_list_t function_get_names(int get_hidden)
{
    std::set<wcstring> names;
    scoped_lock lock(functions_lock);
    autoload_names(names, get_hidden);

    function_map_t::const_iterator iter;
    for (iter = loaded_functions.begin(); iter != loaded_functions.end(); ++iter)
    {
        const wcstring &name = iter->first;

        /* Maybe skip hidden */
        if (! get_hidden)
        {
            if (name.empty() || name.at(0) == L'_') continue;
        }
        names.insert(name);
    }
    return wcstring_list_t(names.begin(), names.end());
}

const wchar_t *function_get_definition_file(const wcstring &name)
{
    scoped_lock lock(functions_lock);
    const function_info_t *func = function_get(name);
    return func ? func->definition_file : NULL;
}


int function_get_definition_offset(const wcstring &name)
{
    scoped_lock lock(functions_lock);
    const function_info_t *func = function_get(name);
    return func ? func->definition_offset : -1;
}

void function_prepare_environment(const wcstring &name, const wchar_t * const * argv, const std::map<wcstring, env_var_t> &inherited_vars)
{
    /* Three components of the environment:
     1. argv
     2. named arguments
     3. inherited variables
    */
    env_set_argv(argv);
 
    const wcstring_list_t named_arguments = function_get_named_arguments(name);
    if (! named_arguments.empty())
    {
        const wchar_t * const *arg;
        size_t i;
        for (i=0, arg=argv; i < named_arguments.size(); i++)
        {
            env_set(named_arguments.at(i).c_str(), *arg, ENV_LOCAL | ENV_USER);
            
            if (*arg)
                arg++;
        }
    }
    
    for (std::map<wcstring,env_var_t>::const_iterator it = inherited_vars.begin(), end = inherited_vars.end(); it != end; ++it)
    {
        env_set(it->first, it->second.missing() ? NULL : it->second.c_str(), ENV_LOCAL | ENV_USER);
    }
}
