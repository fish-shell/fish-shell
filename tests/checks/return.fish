#RUN: %ghoti -C 'set -l ghoti %ghoti' %s
# Some tests of the "return" builtin.

$ghoti -c 'return 5'
echo $status
# CHECK: 5

$ghoti -c 'exit 5'
echo $status
# CHECK: 5

$ghoti -c 'echo foo; return 2; echo bar'
# CHECK: foo
# but not bar
echo $status
# CHECK: 2

$ghoti -ic 'echo interactive-foo; return 69; echo interactive-bar'
# CHECK: interactive-foo
# but not bar
echo $status
# CHECK: 69

# Verify negative return values don't cause UB and never map to 0
function empty_return
    return $argv[1]
end

for i in (seq -- -550 -1)
    empty_return $i
    if test $status -eq 0
        echo returning $i from a ghoti script maps to a $status of 0!
    end
end
# CHECK:
