# NAME
#   expect - assert a list of expected values match an actual value
#
# SYNOPSIS
#   expect <expected> <condition> <actual>

# OPTIONS
#   <expected>...
#   <condition>
#     --to-equal             <actual> value equals <expected> value
#     --to-not-equal         <actual> value does not equals <expected> value
#
#     --to-contain-all       all <actual> values exist in <expected> list
#     --to-not-contain-all   all <actual> values does not exist in <expected> list
#     --to-be-true           exit status should be truthy
#     --to-be-false          exit status should be falsy
#   <actual>...
#
# EXAMPLE
#   import plugins/fish-spec
#
#   function describe_my_test
#     function it_does_my_task
#       set -l result (my_task $data)
#       expect $result --to-equal 0
#     end
#   end
#   spec.run
#
# AUTHORS
#   Bruno Pinto <@pfbruno>
#   Jorge Bucaran <@bucaran>
#/
function expect
  set -l result 0
  # Parse expected / actual lists
  set -l actual    ""
  set -l expected  ""
  set -l condition ""

  for index in (seq (count $argv))
    switch $argv[$index]
      # --condition found, split expected/actual lists
      case --\*
        set expected  $argv[1..(math $index-1)]
        set condition $argv[$index]
        # No comparison required e.g. --to-be-true
        if [ $index -lt (count $argv) ]
          set actual $argv[(math $index+1)..-1]
        end
        break
    end
  end

  # Test conditions
  switch $condition
    case --to-be-false
      eval "$expected"
      test $status -ne 0
    case --to-be-true
      eval "$expected"
      test $status -eq 0
    case --to-contain-all
      set result 0
      for item in $actual
        contains -- "$item" $expected
          or set result $status
      end
      test $result -eq 0
    case --to-not-contain-all
      set result 0
      for item in $actual
        contains -- "$item" $expected
          or set result $status
      end
      test $result -ne 0
    case --to-equal
      test "$expected" = "$actual"
    case --to-not-equal
      test "$expected" != "$actual"
    case \*
      test true = false
  end

  set result $status
  if [ $result -eq 0 ]
    # Return a non-empty string to indicate success.
    set -g _spec_current_test_ouput (printf "$result")
  else
    # Return error information separated by \t and tested condition.
    set -g _spec_current_test_ouput (printf "%s\n" $expected \t $condition $actual)
  end
  return $result
end
