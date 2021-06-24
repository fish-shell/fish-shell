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
