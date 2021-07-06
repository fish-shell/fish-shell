#RUN: %fish -C 'set -l fish %fish' %s
# Some tests of the "return" builtin.

$fish -c 'return 5'
echo $status
# CHECK: 5

$fish -c 'exit 5'
echo $status
# CHECK: 5

$fish -c 'echo foo; return 2; echo bar'
# CHECK: foo
# but not bar
echo $status
# CHECK: 2

$fish -c 'echo interactive-foo; return 69; echo interactive-bar'
# CHECK: interactive-foo
# but not bar
echo $status
# CHECK: 69
