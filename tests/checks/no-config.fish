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

set -q fish_function_path[2]
and echo fish_function_path has two elements

# --no-config should not enable private mode (#12711).
# Private mode is only enabled by --private.
echo "private:$fish_private_mode"
# CHECK: private:

# History operations should work under --no-config; they fail in private mode.
set -g fish_history fish
builtin history merge
echo "merge-status:$status"
# CHECK: merge-status:0
