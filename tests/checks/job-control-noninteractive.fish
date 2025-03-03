#RUN: env fth=%fish_test_helper fish=%fish %fish %s
#REQUIRES: command -v %fish_test_helper

# Ensure job control works in non-interactive environments.

status job-control full
/bin/echo hello
#CHECK: hello

$fth print_pgrp | read first
$fth print_pgrp | read second
test $first -ne $second
and echo "pgroups differed, meaning job control worked"
or echo "pgroups were the same, job control did not work"
#CHECK: pgroups differed, meaning job control worked

# fish ignores SIGTTIN and so may transfer the tty even if it
# doesn't own the tty. Ensure that doesn't happen.
$fish -c 'status job-control full ; $fth report_foreground' &
wait
#CHECKERR: background
