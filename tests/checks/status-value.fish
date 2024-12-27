#RUN: fish=%fish %fish %s

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

$fish -c 'exit -5'
# CHECKERR: warning: builtin exit returned invalid exit code -5
echo $status
# CHECK: 251

$fish -c 'exit -1'
# CHECKERR: warning: builtin exit returned invalid exit code -1
echo $status
# CHECK: 255

# (we avoid 0, so this is turned into 255 again)
$fish -c 'exit -256'
# CHECKERR: warning: builtin exit returned invalid exit code -256
echo $status
# CHECK: 255

$fish -c 'exit -512'
# CHECKERR: warning: builtin exit returned invalid exit code -512
echo $status
# CHECK: 255
