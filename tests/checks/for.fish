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
