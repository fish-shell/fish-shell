# RUN: %fish %s

# Empty commands should be 123
set empty_var
$empty_var
echo $status
# CHECK: 123
# CHECKERR: {{.*}} The expanded command was empty.
# CHECKERR: $empty_var
# CHECKERR: ^

# Failed expansions
echo "$abc["
echo $status
# CHECK: 121
# CHECKERR: {{.*}} Invalid index value
# CHECKERR: echo "$abc["
# CHECKERR:             ^

# Failed wildcards
echo *gibberishgibberishgibberish*
echo $status
# CHECK: 124
# CHECKERR: {{.*}} No matches for wildcard '*gibberishgibberishgibberish*'. {{.*}}
# CHECKERR: echo *gibberishgibberishgibberish*
# CHECKERR:      ^
