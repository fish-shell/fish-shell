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
