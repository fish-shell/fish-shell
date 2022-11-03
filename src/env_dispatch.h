// SPDX-FileCopyrightText: © 2019 fish-shell contributors
//
// SPDX-License-Identifier: GPL-2.0-only

// Prototypes for functions that react to environment variable changes
#ifndef FISH_ENV_DISPATCH_H
#define FISH_ENV_DISPATCH_H

#include "config.h"  // IWYU pragma: keep

#include "common.h"

class environment_t;
class env_stack_t;

/// Initialize variable dispatch.
void env_dispatch_init(const environment_t &vars);

/// React to changes in variables like LANG which require running some code.
void env_dispatch_var_change(const wcstring &key, env_stack_t &vars);

#endif
