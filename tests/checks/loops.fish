#RUN: fish=%fish %fish %s

function never_runs
    while false
    end
end

function early_return
    while true
        return 2
    end
end

function runs_once
    set -l i 1
    while test $i -ne 0 && set i (math $i - 1)
    end
end

# this should return 0
never_runs
echo "Empty Loop in Function: $status"
#CHECK: Empty Loop in Function: 0

# this should return 0
runs_once
echo "Runs Once: $status"
#CHECK: Runs Once: 0

# this should return 2
early_return
echo "Early Return: $status"
#CHECK: Early Return: 2

function set_status
    return $argv[1]
end

# The previous status is visible in the loop condition.
# This includes both the incoming status, and the last command in the
# loop body.
set_status 36
while begin
        set -l saved $status
        echo "Condition Status: $status"
        set_status $saved
    end
    true
end
#CHECK: Condition Status: 36

# The condition status IS visible in the loop body.
set_status 55
while true
    echo "Body Status: $status"
    break
end
#CHECK: Body Status: 0

# The status of the last command is visible in the loop condition
set_status 13
while begin
        set -l saved $status
        echo "Condition 2 Status: $saved"
        test $saved -ne 5
    end
    set_status 5
end
#CHECK: Condition 2 Status: 13
#CHECK: Condition 2 Status: 5

# The status of the last command is visible outside the loop
set rem 5 7 11
while [ (count $rem) -gt 0 ]
    set_status $rem[1]
    set rem $rem[2..-1]
end
echo "Loop Exit Status: $status"
#CHECK: Loop Exit Status: 11

# Empty loops succeed.
false
while false
end
echo "Empty Loop Status: $status"
#CHECK: Empty Loop Status: 0

# Loop control in conditions, should have no output.
for i in 1 2 3
    while break
    end
    echo $i
end
for i in 1 2 3
    while continue
    end
    echo $i
end

if false; or --help
end

# Make sure while loops don't run forever with no-exec (#1543)
echo "while true; end" | $fish --no-execute

# For loops with read-only vars is an error (#4342)
for status in a b c
    echo $status
end
#CHECKERR: {{.*}}loops.fish (line {{\d+}}): for: status: cannot overwrite read-only variable
#CHECKERR: for status in a b c
#CHECKERR:     ^~~~~^

# "That goes for non-electric ones as well (#5548)"
for hostname in a b c
    echo $hostname
end
#CHECKERR: {{.*}}loops.fish (line {{\d+}}): for: hostname: cannot overwrite read-only variable
#CHECKERR: for hostname in a b c
#CHECKERR:     ^~~~~~~^

# For loop control vars available outside the for block
begin
    set -l loop_var initial-value
    for loop_var in a b c
        # do nothing
    end
    set --show loop_var
end
#CHECK: $loop_var: set in local scope, unexported, with 1 elements
#CHECK: $loop_var[1]: |c|

set -g loop_var global_val
function loop_test
    for loop_var in a b c
        if test $loop_var = b
            break
        end
    end
    set --show loop_var
end
loop_test
set --show loop_var
#CHECK: $loop_var: set in local scope, unexported, with 1 elements
#CHECK: $loop_var[1]: |b|

begin
    set -l loop_var
    for loop_var in aa bb cc
    end
    set --show loop_var
end
set --show loop_var
#CHECK: $loop_var: set in global scope, unexported, with 1 elements
#CHECK: $loop_var[1]: |global_val|
#CHECK: $loop_var: set in global scope, unexported, with 1 elements
#CHECK: $loop_var[1]: |global_val|
#CHECK: $loop_var: set in local scope, unexported, with 1 elements
#CHECK: $loop_var[1]: |cc|
#CHECK: $loop_var: set in global scope, unexported, with 1 elements
#CHECK: $loop_var[1]: |global_val|
#CHECK: $loop_var: set in global scope, unexported, with 1 elements
#CHECK: $loop_var[1]: |global_val|
