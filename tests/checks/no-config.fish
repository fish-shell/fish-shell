# SPDX-FileCopyrightText: © 2021 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish --no-config %s

functions | string match help
# CHECK: help

# Universal variables are disabled, we fall back to setting them as global
set -U
# CHECK:
set -U foo bar
set -S foo
# CHECK: $foo: set in global scope, unexported, with 1 elements
# CHECK: $foo[1]: |bar|

set -S fish_function_path fish_complete_path
# CHECK: $fish_function_path: set in global scope, unexported, with 1 elements
# CHECK: $fish_function_path[1]: |{{.*}}|
