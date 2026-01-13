#RUN: fish=%fish %fish %s
# Test that using variables as command names work correctly.

$EMPTY_VARIABLE
#CHECKERR: {{.*}}checks/vars_as_commands.fish (line {{\d+}}): The expanded command was empty.
#CHECKERR: $EMPTY_VARIABLE
#CHECKERR: ^~~~~~~~~~~~~~^
"$EMPTY_VARIABLE"
#CHECKERR: {{.*}}checks/vars_as_commands.fish (line {{\d+}}): The expanded command was empty.
#CHECKERR: "$EMPTY_VARIABLE"
#CHECKERR: ^~~~~~~~~~~~~~~~^

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

# $status specifically is not valid, to avoid a common error
# with `if $status`
echo 'if $status; echo foo; end' | $fish --no-config
#CHECKERR: fish: $status is not valid as a command. See `help language#conditions`
#CHECKERR: if $status; echo foo; end
#CHECKERR:    ^~~~~~^
echo 'not $status' | $fish --no-config
#CHECKERR: fish: $status is not valid as a command. See `help language#conditions`
#CHECKERR: not $status
#CHECKERR:     ^~~~~~^

# Script doesn't run at all.
echo 'echo foo; and $status' | $fish --no-config
#CHECKERR: fish: $status is not valid as a command. See `help language#conditions`
#CHECKERR: echo foo; and $status
#CHECKERR:               ^~~~~~^

echo 'set -l status_cmd true; if $status_cmd; echo Heck yes this is true; end' | $fish --no-config
#CHECK: Heck yes this is true

foo=bar $NONEXISTENT -c 'set foo 1 2 3; set --show foo'
#CHECKERR: {{.*}}checks/vars_as_commands.fish (line {{\d+}}): The expanded command was empty.
#CHECKERR: foo=bar $NONEXISTENT -c 'set foo 1 2 3; set --show foo'
#CHECKERR:         ^~~~~~~~~~~^

exit 0
