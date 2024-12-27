#RUN: fish=%fish helper=%fish_test_helper %fish %s
#REQUIRES: command -v %fish_test_helper

# Check that nohup is propagated.
set output_path (mktemp)
nohup $fish -c "$helper print_ignored_signals" 2>&1 > $output_path
cat $output_path
# CHECK: Hangup: 1
rm $output_path

# Block some signals if job control is off (#6828).
status job-control none
for fish_use_posix_spawn in 0 1
	$helper print_blocked_signals &
	wait
end
# CHECKERR: Interrupt: 2
# CHECKERR: Quit: 3
# CHECKERR: Interrupt: 2
# CHECKERR: Quit: 3

# Ensure we can break from a while loop.

echo About to sigint
$helper sigint_parent &
while true
end
echo I should not be printed because I got sigint

#CHECK: About to sigint
#CHECKERR: Sent SIGINT to {{\d*}}
