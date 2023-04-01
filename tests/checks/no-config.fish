#RUN: %ghoti --no-config %s

functions | string match help
# CHECK: help

# Universal variables are disabled, we fall back to setting them as global
set -U
# CHECK:
set -U foo bar
set -S foo
# CHECK: $foo: set in global scope, unexported, with 1 elements
# CHECK: $foo[1]: |bar|

set -S ghoti_function_path ghoti_complete_path
# CHECK: $ghoti_function_path: set in global scope, unexported, with 1 elements
# CHECK: $ghoti_function_path[1]: |{{.*}}|
