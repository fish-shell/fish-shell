#RUN: %fish -C "set helper %fish_test_helper" %s

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
