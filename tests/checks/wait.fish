# RUN: %fish %s

# Ensure that we can wait for stuff.
status job-control full

set pids

for i in (seq 16)
    command true &
    set -a pids $last_pid
    command false &
    set -a pids $last_pid
end

# Note fish does not (yet) report the exit status of waited-on commands.
for pid in $pids
    wait $pid
end

for i in (seq 16)
    command true &
    command false &
end
wait true false
jobs
# CHECK: jobs: There are no jobs

# Ensure on-process-exit works for exited jobs.
command false &
set pid $last_pid

# Ensure it gets reaped
sleep .1

function waiter --on-process-exit $pid
    echo exited $argv
end
# (Solaris' false exits with 255, not 1)
# CHECK: exited PROCESS_EXIT {{\d+}} {{1|255}}

# Regression test for #9002
sleep 1 &
set p1 $last_pid

sleep 2 &
set p2 $last_pid

function p1_cb --on-process-exit $p1
    echo "P1 over"
end

function p2_cb --on-process-exit $p2
    echo "P2 over"
end

wait
# CHECK: P1 over
# CHECK: P2 over

# Events for background jobs from event handlers (#9096)
function __test_background_job_exit_event --on-variable trigger_var
    sleep .1 &
    function callback --on-process-exit (jobs --last --pid)
        echo -n "Callback called"
    end
end
set trigger_var 123
sleep .5
# CHECK: Callback called

wait (math 2 ^ 32)
# CHECKERR: wait: '4294967296' is not a valid process ID
wait 0
# CHECKERR: wait: '0' is not a valid process ID
wait -- -1
# CHECKERR: wait: Could not find child processes with the name '-1'
wait -- -(math 2 ^ 31)
# CHECKERR: wait: Could not find child processes with the name '-2147483648'
