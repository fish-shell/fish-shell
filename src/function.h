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

class parser_t;

namespace ast {
struct block_statement_t;
}

/// A function's constant properties. These do not change once initialized.
struct function_properties_t {
    /// Parsed source containing the function.
    parsed_source_ref_t parsed_source;

    /// Node containing the function statement, pointing into parsed_source.
    /// We store block_statement, not job_list, so that comments attached to the header are
    /// preserved.
    const ast::block_statement_t *func_node;

    /// List of all named arguments for this function.
    wcstring_list_t named_arguments;

    /// Mapping of all variables that were inherited from the function definition scope to their
    /// values.
    std::map<wcstring, wcstring_list_t> inherit_vars;

    /// Set to true if invoking this function shadows the variables of the underlying function.
    bool shadow_scope{true};
};

using function_properties_ref_t = std::shared_ptr<const function_properties_t>;

/// Add a function.
void function_add(wcstring name, wcstring description, function_properties_ref_t props,
                  const wchar_t *filename);

/// Remove the function with the specified name.
void function_remove(const wcstring &name);

/// Returns the properties for a function, or nullptr if none. This does not trigger autoloading.
function_properties_ref_t function_get_properties(const wcstring &name);

/// Returns by reference the definition of the function with the name \c name. Returns true if
/// successful, false if no function with the given name exists.
/// This does not trigger autoloading.
bool function_get_definition(const wcstring &name, wcstring &out_definition);

/// Returns by reference the description of the function with the name \c name. Returns true if the
/// function exists and has a nonempty description, false if it does not.
/// This does not trigger autoloading.
bool function_get_desc(const wcstring &name, wcstring &out_desc);

/// Sets the description of the function with the name \c name.
void function_set_desc(const wcstring &name, const wcstring &desc, parser_t &parser);

/// Returns true if the function with the name name exists.
/// This may autoload.
int function_exists(const wcstring &cmd, parser_t &parser);

/// Attempts to load a function if not yet loaded. This is used by the completion machinery.
void function_load(const wcstring &cmd, parser_t &parser);

/// Returns true if the function with the name name exists, without triggering autoload.
int function_exists_no_autoload(const wcstring &cmd);

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

/// Creates a new function using the same definition as the specified function. Returns true if copy
/// is successful.
bool function_copy(const wcstring &name, const wcstring &new_name);

/// Observes that fish_function_path has changed.
void function_invalidate_path();

wcstring functions_def(const wcstring &name);
#endif
