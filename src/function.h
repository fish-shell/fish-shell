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

    /// Description of the function.
    wcstring description;

    /// Mapping of all variables that were inherited from the function definition scope to their
    /// values.
    std::map<wcstring, wcstring_list_t> inherit_vars;

    /// Set to true if invoking this function shadows the variables of the underlying function.
    bool shadow_scope{true};

    /// Whether the function was autoloaded.
    bool is_autoload{false};

    /// The file from which the function was created (intern'd string), or nullptr if not from a
    /// file.
    const wchar_t *definition_file{};

    /// \return the description, localized via _.
    const wchar_t *localized_description() const;

    /// \return the line number where the definition of the specified function started.
    int definition_lineno() const;

    /// \return a definition of the function, annotated with properties like event handlers and wrap
    /// targets. This is to support the 'functions' builtin.
    /// Note callers must provide the function name, since the function does not know its own name.
    wcstring annotated_definition(const wcstring &name) const;
};

using function_properties_ref_t = std::shared_ptr<const function_properties_t>;

/// Add a function. This may mutate \p props to set is_autoload.
void function_add(wcstring name, std::shared_ptr<function_properties_t> props);

/// Remove the function with the specified name.
void function_remove(const wcstring &name);

/// \return the properties for a function, or nullptr if none. This does not trigger autoloading.
function_properties_ref_t function_get_props(const wcstring &name);

/// \return the properties for a function, or nullptr if none, perhaps triggering autoloading.
function_properties_ref_t function_get_props_autoload(const wcstring &name, parser_t &parser);

/// Try autoloading a function.
/// \return true if something new was autoloaded, false if it was already loaded or did not exist.
bool function_load(const wcstring &name, parser_t &parser);

/// Sets the description of the function with the name \c name.
/// This triggers autoloading.
void function_set_desc(const wcstring &name, const wcstring &desc, parser_t &parser);

/// Returns true if the function named \p cmd exists.
/// This may autoload.
bool function_exists(const wcstring &cmd, parser_t &parser);

/// Returns true if the function \p cmd either is loaded, or exists on disk in an autoload
/// directory.
bool function_exists_no_autoload(const wcstring &cmd);

/// Returns all function names.
///
/// \param get_hidden whether to include hidden functions, i.e. ones starting with an underscore.
wcstring_list_t function_get_names(bool get_hidden);

/// Creates a new function using the same definition as the specified function. Returns true if copy
/// is successful.
bool function_copy(const wcstring &name, const wcstring &new_name);

/// Observes that fish_function_path has changed.
void function_invalidate_path();

#endif
