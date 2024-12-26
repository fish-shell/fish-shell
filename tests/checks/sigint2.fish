#RUN: %fish -C "set helper %fish_test_helper" %s
# This hangs on OpenBSD
#REQUIRES: test "$(uname)" != OpenBSD
#REQUIRES: command -v %fish_test_helper

# Command subs run in same pgroup as fish, even if job control is 'all'.
# Verify that they get the same pgroup across runs (presumably fish's).
status job-control full
set g1 ($helper print_pgrp)
for i in (seq 10)
    if test $g1 -ne ($helper print_pgrp)
        echo "Unexpected pgroup"
    end
end
status job-control interactive

echo "Finished testing pgroups"
#CHECK: Finished testing pgroups


# Ensure that if child processes SIGINT, we exit our loops
# Test for #3780

echo About to sigint
#CHECK: About to sigint

while true
    sh -c 'echo Here we go; sleep .25; kill -s INT $$'
end
#CHECK: Here we go

echo I should not be printed because of the SIGINT.
