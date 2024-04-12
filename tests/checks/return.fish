#RUN: %fish -C 'set -l fish %fish; set -l filter_ctrls %filter-control-sequences' %s
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

begin
    $fish -ic 'echo interactive-foo; return 69; echo interactive-bar'
    # CHECK: interactive-foo
    # but not bar
    echo $status
    # CHECK: 69
end | $filter_ctrls

# Verify negative return values don't cause UB and never map to 0
function empty_return
    return $argv[1]
end

for i in (seq -- -550 -1)
    empty_return $i
    if test $status -eq 0
        echo returning $i from a fish script maps to a $status of 0!
    end
end
# CHECK:
