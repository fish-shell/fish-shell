#RUN: %fish %s
# Validate some things about command wrapping.

# This tests that we do not trigger a combinatorial explosion - see #5638.
# Ensure it completes successfully.
complete -c testcommand --wraps "testcommand x "
complete -c testcommand --wraps "testcommand y "
complete -c testcommand --no-files -a normal
complete -C'testcommand '
# CHECK: normal

# We get the same completion twice. TODO: fix this.
# CHECK: normal


# This tests that a call to complete from within a completion doesn't trigger
# wrap chain explosion - #5638 again.
function testcommand2_complete
    set -l tokens (commandline -opc) (commandline -ct)
    set -e tokens[1]
    echo $tokens 1>&2
end

complete -c testcommand2 -x -a "(testcommand2_complete)"
complete -c testcommand2 --wraps "testcommand2 from_wraps "
complete -C'testcommand2 explicit '
# CHECKERR: explicit
# CHECKERR: from_wraps explicit
