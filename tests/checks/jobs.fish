#RUN: %fish %s
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
