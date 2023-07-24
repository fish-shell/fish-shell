// Prototypes for functions for storing and retrieving function information. These functions also
// take care of autoloading functions in the $fish_function_path. Actual function evaluation is
// taken care of by the parser and to some degree the builtin handling library.
#ifndef FISH_FUNCTION_H
#define FISH_FUNCTION_H

#include "cxx.h"
#include "maybe.h"

struct function_properties_t;
class parser_t;

#if INCLUDE_RUST_HEADERS
#include "function.rs.h"
#endif

maybe_t<rust::Box<function_properties_t>> function_get_props(const wcstring &name);
maybe_t<rust::Box<function_properties_t>> function_get_props_autoload(const wcstring &name,
                                                                      parser_t &parser);

#endif
