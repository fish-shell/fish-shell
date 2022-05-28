# RUN: %fish %s

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

# Send the signal and immediately define the function; it should not excute.
kill -USR1 $fish_pid
function handle1 --on-signal SIGUSR1
    gotsigusr1
end
echo "Hope it did not run"
#CHECK: Hope it did not run

kill -USR1 $fish_pid
sleep .1
#CHECK: Got USR1: 3
