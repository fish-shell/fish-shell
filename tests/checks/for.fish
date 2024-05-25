# RUN: %fish %s

# A for-loop-variable is a local variable in the enclosing scope.
set -g i global
# implicit set -l i $i
for i in local
end
set -ql i && echo $i
# CHECK: local

# The loop variable is initialized with any previous value.
set -g j global
for j in
end
set -ql j && echo $j
# CHECK: global

# Loop variables exist only locally in the enclosing local scope.
# They do not modify other local/global/universal variables.
set -g k global
begin
    for k in local1
        echo $k
        # CHECK: local1
        for k in local2
        end
        echo $k
        # CHECK: local2
    end
    echo $k
    # CHECK: local1
end
echo $k
# CHECK: global

function foo --on-variable foo
    echo foo set
end

for foo in 1 2 3
    true
end
# CHECK: foo set
# CHECK: foo set
# CHECK: foo set

for x in 1 2 3
    test $x -eq 2 && set -l foo bar
    echo foo value is $foo
end
# We keep the old value from outside the loop
# CHECK: foo value is 3
# CHECK: foo set
# CHECK: foo value is bar
# CHECK: foo value is 3
