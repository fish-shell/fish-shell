// Prototypes for functions for storing and retrieving function information. These functions also
// take care of autoloading functions in the $fish_function_path. Actual function evaluation is
// taken care of by the parser and to some degree the builtin handling library.
#ifndef FISH_FUNCTION_H
#define FISH_FUNCTION_H

#include <map>
#include <vector>

#include "common.h"
#include "env.h"
#include "event.h"
#include "parse_tree.h"
#include "tnode.h"

class parser_t;

/// A function's constant properties. These do not change once initialized.
struct function_properties_t {
    /// Parsed source containing the function.
    parsed_source_ref_t parsed_source;

    /// Node containing the function body, pointing into parsed_source.
    tnode_t<grammar::job_list> body_node;

    /// List of all named arguments for this function.
    wcstring_list_t named_arguments;

    /// Set to true if invoking this function shadows the variables of the underlying function.
    bool shadow_scope;
};

/// Structure describing a function. This is used by the parser to store data on a function while
/// parsing it. It is not used internally to store functions, the function_info_t structure
/// is used for that purpose. Parhaps this should be merged with function_info_t.
struct function_data_t {
    /// Name of function.
    wcstring name;
    /// Description of function.
    wcstring description;
    /// List of all variables that are inherited from the function definition scope. The variable
    /// values are snapshotted when function_add() is called.
    wcstring_list_t inherit_vars;
    /// Function's metadata fields
    function_properties_t props;
    /// List of all event handlers for this function.
    std::vector<event_t> events;
};

/// Add a function.
void function_add(const function_data_t &data, const parser_t &parser);

/// Remove the function with the specified name.
void function_remove(const wcstring &name);

/// Returns the properties for a function, or nullptr if none. This does not trigger autoloading.
std::shared_ptr<const function_properties_t> function_get_properties(const wcstring &name);

/// Returns by reference the definition of the function with the name \c name. Returns true if
/// successful, false if no function with the given name exists.
/// This does not trigger autoloading.
bool function_get_definition(const wcstring &name, wcstring &out_definition);

/// Returns by reference the description of the function with the name \c name. Returns true if the
/// function exists and has a nonempty description, false if it does not.
/// This does not trigger autoloading.
bool function_get_desc(const wcstring &name, wcstring &out_desc);

/// Sets the description of the function with the name \c name.
void function_set_desc(const wcstring &name, const wcstring &desc);

/// Returns true if the function with the name name exists.
/// This may autoload.
int function_exists(const wcstring &name);

/// Attempts to load a function if not yet loaded. This is used by the completion machinery.
void function_load(const wcstring &name);

/// Returns true if the function with the name name exists, without triggering autoload.
int function_exists_no_autoload(const wcstring &name, const environment_t &vars);

/// Returns all function names.
///
/// \param get_hidden whether to include hidden functions, i.e. ones starting with an underscore.
wcstring_list_t function_get_names(int get_hidden);

/// Returns true if the function was autoloaded.
bool function_is_autoloaded(const wcstring &name);

/// Returns tha absolute path of the file where the specified function was defined. Returns 0 if the
/// file was defined on the commandline.
///
/// This function does not autoload functions, it will only work on functions that have already been
/// defined.
///
/// This returns an intern'd string.
const wchar_t *function_get_definition_file(const wcstring &name);

/// Returns the linenumber where the definition of the specified function started.
/// This does not trigger autoloading.
int function_get_definition_lineno(const wcstring &name);

/// Returns a mapping of all variables of the specified function that were inherited from the scope
/// of the function definition to their values.
std::map<wcstring, env_var_t> function_get_inherit_vars(const wcstring &name);

/// Creates a new function using the same definition as the specified function. Returns true if copy
/// is successful.
bool function_copy(const wcstring &name, const wcstring &new_name);

/// Prepares the environment for executing a function.
void function_prepare_environment(env_stack_t &vars, const wcstring &name,
                                  const wchar_t *const *argv,
                                  const std::map<wcstring, env_var_t> &inherited_vars);

/// Observes that fish_function_path has changed.
void function_invalidate_path();

#endif
