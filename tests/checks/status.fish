#RUN: %fish %s
status -b
and echo '"status -b" unexpectedly returned true at top level'

begin
    status -b
    or echo '"status -b" unexpectedly returned false inside a begin block'
end

status -l
and echo '"status -l" unexpectedly returned true for a non-login shell'

status -i
and echo '"status -i" unexpectedly returned true for a non-interactive shell'

status is-login
and echo '"status is-login" unexpectedly returned true for a non-login shell'

status is-interactive
and echo '"status is-interactive" unexpectedly returned true for a non-interactive shell'

# We should get an error message about an invalid combination of flags.
status --is-interactive --is-login
#CHECKERR: status: is-interactive is-login: options cannot be used together

# We should get an error message about an unexpected arg for `status
# is-block`.
status -b is-interactive
#CHECKERR: status: is-block is-interactive: options cannot be used together
# XXX this would be better if it referred to -b rather than what it is

# Try to set the job control to an invalid mode.
status job-control full1
#CHECKERR: status: Invalid job control mode 'full1'
status --job-control=1none
#CHECKERR: status: Invalid job control mode '1none'

# Now set it to a valid mode.
status job-control none

status current-command | sed s/^/^/
# CHECK: ^fish

# Check status -u outside functions
status current-function
#CHECK: Not a function

function test_function
    status current-function
end

test_function
#CHECK: test_function
eval test_function
#CHECK: test_function

# Future Feature Flags
status features | head -n3
#CHECK: stderr-nocaret          on  3.0 ^ no longer redirects stderr (historical, can no longer be changed)
#CHECK: qmark-noglob            on  3.0 ? no longer globs
#CHECK: regex-easyesc           on  3.1 string replace -r needs fewer \'s
status test-feature stderr-nocaret
echo $status
#CHECK: 0
status test-feature not-a-feature
echo $status
#CHECK: 2

# Ensure $status isn't reset before a function is executed
function echo_last
    echo $status
end

false
echo_last
echo $status #1
#CHECK: 1
#CHECK: 0

# Verify that if swallows failure - see #1061
if false
end
echo $status
#CHECK: 0

# Verify errors from writes - see #7857.
if test -e /dev/full
    # Failed writes to stdout produce 1.
    echo foo > /dev/full
    if test $status -ne 1
        echo "Wrong status when writing to /dev/full"
    end

   # Here the builtin should fail with status 2,
   # and also the write should fail with status 1.
   # The builtin has precedence.
   builtin string --not-a-valid-option 2> /dev/full
   if test $status -ne 2
       echo "Wrong status for failing builtin"
   end
   echo "Failed write tests finished"
else
    echo "Failed write tests skipped"
    echo "write: skipped" 1>&2
    echo "write: skipped" 1>&2
end
# CHECK: Failed write tests {{finished|skipped}}
# CHECKERR: write: {{.*}}
# CHECKERR: write: {{.*}}

function test-stack-trace-main
    status stack-trace
end

function test-stack-trace-other
    test-stack-trace-main
end

printf "%s\n" (test-stack-trace-other | string replace \t '<TAB>')[1..4]
# CHECK: in function 'test-stack-trace-main'
# CHECK: <TAB>called on line {{\d+}} of file {{.*}}/status.fish
# CHECK: in function 'test-stack-trace-other'
# CHECK: <TAB>called on line {{\d+}} of file {{.*}}/status.fish

functions -c test-stack-trace-other test-stack-trace-copy
printf "%s\n" (test-stack-trace-copy | string replace \t '<TAB>')[1..4]
# CHECK: in function 'test-stack-trace-main'
# CHECK: <TAB>called on line {{\d+}} of file {{.*}}/status.fish
# CHECK: in function 'test-stack-trace-copy'
# CHECK: <TAB>called on line {{\d+}} of file {{.*}}/status.fish

status test-terminal-feature
and should have failed on missing arg
# CHECKERR: status: test-terminal-feature: expected 1 arguments; got 0
status test-terminal-feature 1 2
and should have failed on too many args
# CHECKERR: status: test-terminal-feature: expected 1 arguments; got 2
status test-terminal-feature unrecognized-feature
and should have failed on unrecognized feature
# CHECKERR: status test-terminal-feature: unrecognized feature 'unrecognized-feature'
status test-terminal-feature scroll-content-up
and should have failed when running without a TTY
