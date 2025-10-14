# RUN: %fish %s

fish_delta | grep -A1 '\bfunction __fish_print_help\b'
# CHECK: -function __fish_print_help --description "Print help for the specified fish function or builtin"
# CHECK: -    set -l item $argv[1]
# CHECK: --
# CHECK: +function __fish_print_help
# CHECK: +    echo Documentation for $argv >&2
