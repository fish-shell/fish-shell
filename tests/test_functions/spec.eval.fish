# NAME
#   spec.eval - eval a test function
#
# SYNOPSIS
#   spec.eval <function> [OPTIONS...]
#
# OPTIONS
#   See spec.report
#
# AUTHORS
#   Bruno Pinto <@pfbruno>
#   Jorge Bucaran <@bucaran>
#/
function spec.eval
  set -l result $status
  set -l test $argv[1]
  set -e argv[1]

  if functions -q $test
    # Erase previous test output
    set -e _spec_current_test_ouput

    # Run the test yielding control to the user defined spec.
    eval $test
    set result $status

    # Display test results.
    spec.view $test $result $argv -- $_spec_current_test_ouput
  end

  return $result
end
