#RUN: %fish %s
# Test that using variables as command names work correctly.

$EMPTY_VARIABLE
#CHECKERR: {{.*}}checks/vars_as_commands.fish (line {{\d+}}): The expanded command was empty.
#CHECKERR: $EMPTY_VARIABLE
#CHECKERR: ^
"$EMPTY_VARIABLE"
#CHECKERR: {{.*}}checks/vars_as_commands.fish (line {{\d+}}): The expanded command was empty.
#CHECKERR: "$EMPTY_VARIABLE"
#CHECKERR: ^

set CMD1 echo basic command as variable
$CMD1
#CHECK: basic command as variable

set CMD2 echo '(' not expanded again
$CMD2
#CHECK: ( not expanded again

# Test using variables with the builtin decorator
builtin $CMD1
#CHECK: basic command as variable

# Test implicit cd
set CMD3 /usr/bin
$CMD3 && echo $PWD
#CHECK: /usr/bin

exit 0
