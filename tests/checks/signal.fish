# RUN: %fish -C 'set -l fish %fish; set -lx fth %fish_test_helper' %s

$fish -c 'function main; exit 4; true; end; main'
echo $status
#CHECK: 4

$fish -c 'begin; exit 5; true; end'
echo $status
#CHECK: 5

$fish -c 'kill -SIGHUP %self'
echo $status
#CHECK: 129

$fish -c 'function main; kill -SIGTERM %self; true; end; main'
echo $status
#CHECK: 143


status job-control full

# Block no signal by default.
$fth print_blocked_signals

# Block some job control signals in subshells.
true ($fth print_blocked_signals)
# TODO why is this 18 on macOS?
# CHECKERR: {{Suspended 18|Stopped: 20}}
# CHECKERR: Stopped (tty input): 21
# CHECKERR: Stopped (tty output): 22

# Also block the signals in event handlers.
function event_handler --on-event event
    $fth print_blocked_signals
end
emit event
# CHECKERR: {{Suspended 18|Stopped: 20}}
# CHECKERR: Stopped (tty input): 21
# CHECKERR: Stopped (tty output): 22

status job-control interactive


function alarm --on-signal ALRM
    echo ALRM received
end

kill -s ALRM $fish_pid
# CHECK: ALRM received

function anychild --on-process-exit 0
    # Type and exit status
    echo $argv[1] $argv[3]
end

echo "command false:"
command false
# CHECK: command false:
# CHECK: PROCESS_EXIT 1
# CHECK: JOB_EXIT 0

echo "command true:"
command true
# CHECK: command true:
# CHECK: PROCESS_EXIT 0
# CHECK: JOB_EXIT 0

echo "command false | true:"
command false | command true
# CHECK: command false | true:
# CHECK: PROCESS_EXIT 1
# CHECK: PROCESS_EXIT 0
# CHECK: JOB_EXIT 0

function test_blocks
    block -l
    command echo "This is the process whose exit event shuld be blocked"
    echo "This should come before the event handler"
end
test_blocks
# CHECK: This is the process whose exit event shuld be blocked
# CHECK: This should come before the event handler

echo "Now event handler should have run"
# CHECK: PROCESS_EXIT 0
# CHECK: JOB_EXIT 0
# CHECK: Now event handler should have run
# CHECK: PROCESS_EXIT 0
