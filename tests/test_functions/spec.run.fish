# NAME
#   spec.run - run a suite of tests
#
# SYNOPSIS
#   spec.run [OPTIONS]
#
# OPTIONS
#   -v --verbose
#     Print full test output.
#
# DESCRIPTION
#   In order to run tests create the test file, import plugins/spec at
#   before adding any of the functions described above and call spec.run.
#
# FUNCTIONS
#   function before_all     Run before any tests are run.
#   function before_each    Run before each test.
#   function describe_*     Use to organize different specs.
#   function it_*           Use to test your library/plugin.
#   function after_each     Run after each test.
#   function after_all      Run after all tests are finished.
#
# NOTES
#   After each test is evaluated, the function is erased from the scope by
#   spec.eval to guarantee that subsequent describe blocks will not end up
#   calling the previous describe's batch of tests.
#
#   The fish-spec library is no different from other Oh-My-Fish plugins.
#   Use `import plugins/fish-spec` at the top of your spec file and call
#
#     spec.run $argv
#
#   After your suite of tests.
#
# EXAMPLES
#     import plugins/fish-spec
#     function describe_erase
#       function before_each
#         set -g nums 1 2 3
#       end
#       function it_erases_one_item -d "It should erase an item from a list."
#         erase 1 --from nums
#         expect $nums --to-not-contain 1
#       end
#     end
#     spec.run --verbose
#
# AUTHORS
#   Bruno Pinto <@pfbruno>
#   Jorge Bucaran <@bucaran>
#/
function spec.run
  set -l result  0
  set -l tests  ""
  set -l describes (spec.functions describe_)

  # Load this suite unique set of tests.
  for describe in $describes
    spec.eval $describe --header $argv
    spec.eval before_all $argv

    set tests (spec.functions it_)
    set -l failed 0

    for test in $tests
      spec.eval before_each $argv

      if not spec.eval $test --tab 1 $argv
        set result 1 # Flunk!
        set failed (math 1+$failed)
      end

      spec.eval after_each $argv
    end
    spec.eval after_all $argv

    if contains -- $argv[1] -v --verbose
      spec.view --report (count $tests) $failed
    end

    # Clean up methods after all tests are finished.
    spec.functions -e it_ before_ after_
  end

  if [ -z "$describes" -o -z "$tests" ]
    echo "No tests found."
    return 1
  end

  spec.functions -e describe_

  return $result
end
