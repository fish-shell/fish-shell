# RUN: env fth=%fish_test_helper %fish %s
#REQUIRES: command -v %fish_test_helper

set -g SIGUSR1_COUNT 0

function gotsigusr1
    set -g SIGUSR1_COUNT (math $SIGUSR1_COUNT + 1)
    echo "Got USR1: $SIGUSR1_COUNT"

end

function handle1 --on-signal SIGUSR1
    gotsigusr1
end

function handle_us42 --on-signal SIGUSR2
end

kill -USR1 $fish_pid
sleep .1
#CHECK: Got USR1: 1

kill -USR1 $fish_pid
sleep .1
#CHECK: Got USR1: 2

kill -USR2 $fish_pid
sleep .1
kill -USR2 $fish_pid
sleep .1

# Previous signals do not re-trigger handlers.
functions --erase handle1
kill -USR1 $fish_pid
sleep .1
kill -USR1 $fish_pid
sleep .1

kill -USR2 $fish_pid
sleep .1

# Send the signal and immediately define the function; it should not execute.
kill -USR1 $fish_pid
function handle1 --on-signal SIGUSR1
    gotsigusr1
end
echo "Hope it did not run"
#CHECK: Hope it did not run

kill -USR1 $fish_pid
sleep .1
#CHECK: Got USR1: 3

# We can trap SIGINT.
# Trapping it prevents exiting.
function handle_int --on-signal SIGINT
    echo Got SIGINT
end
kill -INT $fish_pid
#CHECK: Got SIGINT

# In non-interactive mode, we have historically treated
# a child process which dies with SIGINT as if we got SIGINT.
# However in this case we don't execute handlers; we just don't exit.
$fth sigint_self
echo "Should not have exited"
#CHECK: Should not have exited

# If a signal is received while handling a signal, that is deferred until the handler is done.
set -g INTCOUNT 0
function handle_int --on-signal SIGINT
    test $INTCOUNT -gt 2 && return
    set -g INTCOUNT (math $INTCOUNT + 1)
    echo "start handle_int $INTCOUNT"
    sleep .1
    kill -INT $fish_pid
    echo "end handle_int $INTCOUNT"
end
kill -INT $fish_pid
# CHECK: start handle_int 1
# CHECK: end handle_int 1
# CHECK: start handle_int 2
# CHECK: end handle_int 2
# CHECK: start handle_int 3
# CHECK: end handle_int 3

# Remove our handler and SIGINT ourselves. Now we should exit.
functions --erase handle_int
kill -INT $fish_pid
echo "I should not run"
