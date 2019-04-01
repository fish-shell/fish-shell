// Prototypes for functions that react to environment variable changes
#ifndef FISH_ENV_DISPATCH_H
#define FISH_ENV_DISPATCH_H

#include "config.h"  // IWYU pragma: keep

#include "common.h"
#include "env_universal_common.h"

#include <memory>

/// Initialize variable dispatch.
class environment_t;
void env_dispatch_init(const environment_t &vars);

class env_stack_t;
void env_dispatch_var_change(const wchar_t *op, const wcstring &key, env_stack_t &vars);
void guess_emoji_width();

void env_universal_callbacks(env_stack_t *stack, const callback_data_list_t &callbacks);

extern const wcstring_list_t locale_variables;
extern const wcstring_list_t curses_variables;

#endif
