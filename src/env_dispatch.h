// Prototypes for functions that react to environment variable changes
#ifndef FISH_ENV_DISPATCH_H
#define FISH_ENV_DISPATCH_H

#include "config.h"  // IWYU pragma: keep

#include "common.h"
#include "env.h"

class env_stack_t;

/// Initialize variable dispatch.
void env_dispatch_init(const environment_t &vars);

/// React to changes in variables like LANG which require running some code.
void env_dispatch_var_change(const wcstring &key, env_stack_t &vars);

/// FFI wrapper which always uses the principal stack.
/// TODO: pass in the variables directly.
void env_dispatch_var_change_ffi(const wcstring &key /*, env_stack_t &vars */);

#endif
