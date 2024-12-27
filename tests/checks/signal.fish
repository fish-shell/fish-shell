# RUN: env fish_test_helper=%fish_test_helper fish=%fish %fish %s
#REQUIRES: command -v %fish_test_helper

$fish -c 'function main; exit 4; true; end; main'
echo $status
#CHECK: 4

$fish -c 'begin; exit 5; true; end'
echo $status
#CHECK: 5

$fish -c 'kill -SIGHUP $fish_pid'
echo $status
#CHECK: 129

$fish -c 'function main; kill -SIGTERM $fish_pid; true; end; main'
echo $status
#CHECK: 143

function alarm --on-signal ALRM
    echo ALRM received
end

kill -s ALRM $fish_pid
# CHECK: ALRM received

function anychild --on-process-exit 0
    # Type and exit status
    echo $argv[1] $argv[3]
end

function anyjob --on-job-exit 0
    # Type and exit status
    echo $argv[1] $argv[3]
end

echo "command false:"
command false
# CHECK: command false:
# (Solaris' false exits with 255, not 1)
# CHECK: PROCESS_EXIT {{1|255}}
# CHECK: JOB_EXIT 0

echo "command true:"
command true
# CHECK: command true:
# CHECK: PROCESS_EXIT 0
# CHECK: JOB_EXIT 0

echo "command false | true:"
command false | command true
# CHECK: command false | true:
# CHECK: PROCESS_EXIT {{1|255}}
# CHECK: PROCESS_EXIT 0
# CHECK: JOB_EXIT 0

# Signals are reported correctly.
# SIGKILL $status is 128 + 9 = 137
$fish_test_helper sigkill_self
# CHECK: PROCESS_EXIT 137
# CHECK: JOB_EXIT 0

function test_blocks
    block -l
    command echo "This is the process whose exit event should be blocked"
    echo "This should come before the event handler"
end
test_blocks
# CHECK: This is the process whose exit event should be blocked
# CHECK: This should come before the event handler

echo "Now event handler should have run"
# CHECK: PROCESS_EXIT 0
# CHECK: JOB_EXIT 0
# CHECK: Now event handler should have run
# CHECK: PROCESS_EXIT 0
