#RUN: env fth=%fish_test_helper fish=%fish %fish %s

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

$fish -c '
    function __fish_print_help
        if set -q argv[2]
            echo Error-message: $argv[2] >&2
        end
        echo Documentation for $argv[1] >&2
        return 1
    end
    sleep .2 & bg %1
'
#CHECKERR: Error-message: bg: Can't put job 1, 'sleep .2 &' to background because it is not under job control
#CHECKERR: Documentation for bg
