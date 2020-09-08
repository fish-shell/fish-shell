// Prototypes for functions that react to environment variable changes
#ifndef FISH_ENV_DISPATCH_H
#define FISH_ENV_DISPATCH_H

#include "config.h"  // IWYU pragma: keep

#include <memory>

#include "common.h"
#include "env_universal_common.h"

/// Initialize variable dispatch.
class environment_t;
void env_dispatch_init(const environment_t &vars);

class env_stack_t;
void env_dispatch_var_change(const wcstring &key, env_stack_t &vars);

void env_universal_callbacks(env_stack_t *stack, const callback_data_list_t &callbacks);

#endif
