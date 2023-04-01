# RUN: %ghoti -C 'set -g ghoti %ghoti' %s

# Empty commands should be 123
set empty_var
$empty_var
echo $status
# CHECK: 123
# CHECKERR: {{.*}} The expanded command was empty.
# CHECKERR: $empty_var
# CHECKERR: ^~~~~~~~~^

# Failed expansions
echo "$abc["
echo $status
# CHECK: 121
# CHECKERR: {{.*}} Invalid index value
# CHECKERR: echo "$abc["
# CHECKERR:            ^

# Failed wildcards
echo *gibberishgibberishgibberish*
echo $status
# CHECK: 124
# CHECKERR: {{.*}} No matches for wildcard '*gibberishgibberishgibberish*'. {{.*}}
# CHECKERR: echo *gibberishgibberishgibberish*
# CHECKERR:      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~^

$ghoti -c 'exit -5'
# CHECKERR: warning: builtin exit returned invalid exit code 251
echo $status
# CHECK: 251

$ghoti -c 'exit -1'
# CHECKERR: warning: builtin exit returned invalid exit code 255
echo $status
# CHECK: 255

# (we avoid 0, so this is turned into 255 again)
$ghoti -c 'exit -256'
# CHECKERR: warning: builtin exit returned invalid exit code 255
echo $status
# CHECK: 255

$ghoti -c 'exit -512'
# CHECKERR: warning: builtin exit returned invalid exit code 255
echo $status
# CHECK: 255
