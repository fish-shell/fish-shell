#RUN: %fish %s

set -xl LANG C # uniform quotes

eval 'true | and'
# CHECKERR: {{.*}}: The 'and' command can not be used in a pipeline

eval 'true | or'
# CHECKERR: {{.*}}: The 'or' command can not be used in a pipeline

# Verify and/or behavior with if and while
if false; or true
    echo success1
end
# CHECK: success1
if false; and false
    echo failure1
end
while false; and false; or true
    echo success2
    break
end
# CHECK: success2
while false; or begin
        false; or true
    end
    echo success3
    break
end
# CHECK: success3
if false
else if false; and true
else if false; and false
else if false; or true
    echo success4
end
# CHECK: success4
if false
else if false; and true
else if false; or false
else if false
    echo "failure 4"
end
if false; or true | false
    echo failure5
end
