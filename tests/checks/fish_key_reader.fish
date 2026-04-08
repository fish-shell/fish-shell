#RUN: fish=%fish %fish %s

# See `tests/pexpects/fkr.py` for non-error tests

fish_key_reader --continuous=invalid
# CHECKERR: fish_key_reader: --continuous=invalid: option does not take an argument

fish_key_reader --invalid-opt
# CHECKERR: fish_key_reader: --invalid-opt: unknown option

fish_key_reader some-unexpected-args
# CHECKERR: Expected no arguments, got 1

echo | builtin fish_key_reader
# CHECKERR: Stdin must be attached to a tty.

set -l dir (dirname $fish)
echo | command $dir/fish_key_reader
# CHECKERR: Stdin must be attached to a tty.
