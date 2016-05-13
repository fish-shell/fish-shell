// Prototypes for functions for storing and retrieving function information. These functions also
// take care of autoloading functions in the $fish_function_path. Actual function evaluation is
// taken care of by the parser and to some degree the builtin handling library.
#ifndef FISH_FUNCTION_H
#define FISH_FUNCTION_H

#include <stdbool.h>
#include <map>
#include <vector>

#include "common.h"
#include "env.h"
#include "event.h"

class parser_t;

/// Structure describing a function. This is used by the parser to store data on a function while
/// parsing it. It is not used internally to store functions, the function_internal_data_t structure
/// is used for that purpose. Parhaps these two should be merged.
struct function_data_t {
    /// Name of function.
    wcstring name;
    /// Description of function.
    wcstring description;
    /// Function definition.
    const wchar_t *definition;
    /// List of all event handlers for this function.
    std::vector<event_t> events;
    /// List of all named arguments for this function.
    wcstring_list_t named_arguments;
    /// List of all variables that are inherited from the function definition scope. The variable
    /// values are snapshotted when function_add() is called.
    wcstring_list_t inherit_vars;
    /// Set to true if invoking this function shadows the variables of the underlying function.
    bool shadow_scope;
    /// Set to true if this function shadows a builtin.
    bool shadow_builtin;
};

class function_info_t {
   public:
    /// Function definition.
    const wcstring definition;
    /// Function description. Only the description may be changed after the function is created.
    wcstring description;
    /// File where this function was defined (intern'd string).
    const wchar_t *const definition_file;
    /// Line where definition started.
    const int definition_offset;
    /// List of all named arguments for this function.
    const wcstring_list_t named_arguments;
    /// Mapping of all variables that were inherited from the function definition scope to their
    /// values.
    const std::map<wcstring, env_var_t> inherit_vars;
    /// Flag for specifying that this function was automatically loaded.
    const bool is_autoload;
    /// Set to true if this function shadows a builtin.
    const bool shadow_builtin;
    /// Set to true if invoking this function shadows the variables of the underlying function.
    const bool shadow_scope;

    /// Constructs relevant information from the function_data.
    function_info_t(const function_data_t &data, const wchar_t *filename, int def_offset,
                    bool autoload);

    /// Used by function_copy.
    function_info_t(const function_info_t &data, const wchar_t *filename, int def_offset,
                    bool autoload);
};

/// Initialize function data.
void function_init();

/// Add a function. definition_line_offset is the line number of the function's definition within
/// its source file.
void function_add(const function_data_t &data, const parser_t &parser,
                  int definition_line_offset = 0);

/// Remove the function with the specified name.
void function_remove(const wcstring &name);

/// Returns by reference the definition of the function with the name \c name. Returns true if
/// successful, false if no function with the given name exists.
bool function_get_definition(const wcstring &name, wcstring *out_definition);

/// Returns by reference the description of the function with the name \c name. Returns true if the
/// function exists and has a nonempty description, false if it does not.
bool function_get_desc(const wcstring &name, wcstring *out_desc);

/// Sets the description of the function with the name \c name.
void function_set_desc(const wcstring &name, const wcstring &desc);

/// Returns true if the function with the name name exists.
int function_exists(const wcstring &name);

/// Attempts to load a function if not yet loaded. This is used by the completion machinery.
void function_load(const wcstring &name);

/// Returns true if the function with the name name exists, without triggering autoload.
int function_exists_no_autoload(const wcstring &name, const env_vars_snapshot_t &vars);

/// Returns all function names.
///
/// \param get_hidden whether to include hidden functions, i.e. ones starting with an underscore.
wcstring_list_t function_get_names(int get_hidden);

/// Returns tha absolute path of the file where the specified function was defined. Returns 0 if the
/// file was defined on the commandline.
///
/// This function does not autoload functions, it will only work on functions that have already been
/// defined.
///
/// This returns an intern'd string.
const wchar_t *function_get_definition_file(const wcstring &name);

/// Returns the linenumber where the definition of the specified function started.
///
/// This function does not autoload functions, it will only work on functions that have already been
/// defined.
int function_get_definition_offset(const wcstring &name);

/// Returns a list of all named arguments of the specified function.
wcstring_list_t function_get_named_arguments(const wcstring &name);

/// Returns a mapping of all variables of the specified function that were inherited from the scope
/// of the function definition to their values.
std::map<wcstring, env_var_t> function_get_inherit_vars(const wcstring &name);

/// Creates a new function using the same definition as the specified function. Returns true if copy
/// is successful.
bool function_copy(const wcstring &name, const wcstring &new_name);

/// Returns whether this function shadows a builtin of the same name.
int function_get_shadow_builtin(const wcstring &name);

/// Returns whether this function shadows variables of the underlying function.
int function_get_shadow_scope(const wcstring &name);

/// Prepares the environment for executing a function.
void function_prepare_environment(const wcstring &name, const wchar_t *const *argv,
                                  const std::map<wcstring, env_var_t> &inherited_vars);

#endif
