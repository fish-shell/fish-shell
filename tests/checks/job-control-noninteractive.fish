#RUN: env fth=%fish_test_helper %fish %s

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
