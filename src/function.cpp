// Functions for storing and retrieving function information. These functions also take care of
// autoloading functions in the $fish_function_path. Actual function evaluation is taken care of by
// the parser and to some degree the builtin handling library.
//
#include "config.h"  // IWYU pragma: keep

#include "function.h"

#include "common.h"

maybe_t<rust::Box<function_properties_t>> function_get_props(const wcstring &name) {
    if (auto *ptr = function_get_props_raw(name)) {
        return rust::Box<function_properties_t>::from_raw(ptr);
    }
    return none();
}

maybe_t<rust::Box<function_properties_t>> function_get_props_autoload(const wcstring &name,
                                                                      parser_t &parser) {
    if (auto *ptr = function_get_props_autoload_raw(name, parser)) {
        return rust::Box<function_properties_t>::from_raw(ptr);
    }
    return none();
}
