#RUN: %fish %s

# Verify zombies are not left by disown (#7183, #5342)
# Do this first to avoid colliding with the other disowned processes below, which may
# still be running at the end of the script
sleep 0.2 &
disown
sleep 0.2
echo Trigger process reaping
sleep 0.1
#CHECK: Trigger process reaping
# The initial approach here was to kill the PID of the sleep process, which should
# be gone by the time we get here. Unfortunately, kill from procps on pre-2016 distributions
# does not print an error for non-existent PIDs, so instead look for zombies in this session.
# There could already be zombies from previous tests run in this session or a test could be run
# simultaneously that causes a zombie to spawn, so limit the output only to processes started by
# this fish instance.
if not contains (uname) SunOS
    ps -o ppid,stat
else
    ps -o ppid,s
end | string match -e $fish_pid | string match '*Z*'

# Verify disown can be used with last_pid, even if it is separate from the pgroup.
# This should silently succeed.
command true | sleep 0.5 &
disown $last_pid

jobs -q
echo $status
#CHECK: 1
sleep 5 &
sleep 5 &
jobs -c
#CHECK: Command
#CHECK: sleep
#CHECK: sleep
jobs -q
echo $status
#CHECK: 0
bg -23 1 2>/dev/null
or echo bg: invalid option -23 >&2
#CHECKERR: bg: invalid option -23
fg 3
#CHECKERR: fg: No suitable job: 3
bg 3
#CHECKERR: bg: Could not find job '3'
sleep 1 &
disown
jobs -c
#CHECK: Command
#CHECK: sleep
#CHECK: sleep
jobs 1
echo $status
#CHECK: 1
#CHECKERR: jobs: No suitable job: 1
jobs foo
echo $status
#CHECK: 2
#CHECKERR: jobs: 'foo' is not a valid process id
jobs -q 1
echo $status
#CHECK: 1
jobs -q foo
echo $status
#CHECK: 2
#CHECKERR: jobs: 'foo' is not a valid process id
disown foo
#CHECKERR: disown: 'foo' is not a valid job specifier
disown (jobs -p)
or exit 0

# Verify `jobs` output within a function lists background jobs
# https://github.com/fish-shell/fish-shell/issues/5824
function foo
    sleep 0.2 &
    jobs -c
end
foo

# Verify we observe job exit events
sleep 1 &
set sleep_job $last_pid
function sleep_done_$sleep_job --on-job-exit $sleep_job
    /bin/echo "sleep is done"
    functions --erase sleep_done_$sleep_job
end
sleep 2

# Verify `jobs -l` works and returns the right status codes
# https://github.com/fish-shell/fish-shell/issues/6104
jobs --last --command
echo $status
#CHECK: Command
#CHECK: sleep
#CHECK: sleep is done
#CHECK: 1
sleep 0.2 &
jobs -lc
echo $status
#CHECK: Command
#CHECK: sleep
#CHECK: 0

function foo
    function caller --on-job-exit caller
        echo caller
    end
    echo foo
end

function bar --on-event bar
    echo (foo)
end

emit bar
#CHECK: foo
#CHECK: caller

# We can't rely on a *specific* pgid being assigned,
# but we can rely on it not being fish's.
command true &
set -l truepid $last_pid
test $truepid != $fish_pid || echo true has same pid as fish

# Job exit events work even after the job has exited!
sleep .5
function thud --on-job-exit $truepid
    echo "thud called"
end
# CHECK: thud called

disown 252
# CHECKERR: disown: Could not find job '252'
