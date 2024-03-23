# RUN: %fish %s
#
# Tests for the posix-mandated zero and one argument modes for the `test` builtin, aka `[`.

test -n
echo $status
#CHECK: 0

test -z
echo $status
#CHECK: 0

test -d
echo $status
#CHECK: 0

test "foo"
echo $status
#CHECK: 0

test ""
echo $status
#CHECK: 1

test -z "" -a foo
echo $status
#CHECK: 0

set -l fish (status fish-path)
echo 'test foo; test; test -z; test -n; test -d; echo oops' | $fish -d 'deprecated-*' >/dev/null
#CHECKERR: test: called with one argument. This will return false in future.
#CHECKERR: Standard input (line 1):
#CHECKERR: test foo; test; test -z; test -n; test -d; echo oops
#CHECKERR: ^
#CHECKERR: test: called with no arguments. This will be an error in future.
#CHECKERR: Standard input (line 1):
#CHECKERR: test foo; test; test -z; test -n; test -d; echo oops
#CHECKERR:           ^
#CHECKERR: test: called with one argument. This will return false in future.
# (yes, `test -z` is skipped because it would behave the same)
#CHECKERR: Standard input (line 1):
#CHECKERR: test foo; test; test -z; test -n; test -d; echo oops
#CHECKERR:                          ^
#CHECKERR: test: called with one argument. This will return false in future.
#CHECKERR: Standard input (line 1):
#CHECKERR: test foo; test; test -z; test -n; test -d; echo oops
#CHECKERR: ^

