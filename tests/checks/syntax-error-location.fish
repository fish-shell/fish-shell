#RUN: %ghoti -C 'set -g ghoti %ghoti' %s

# A $status used as a command should not impact the location of other errors.
echo 'echo foo | exec grep # this exec is not allowed!

$status

 # The error might be found here!' | $ghoti 2>| string replace -r '(.*)' '<$1>'

# CHECK: <ghoti: The 'exec' command can not be used in a pipeline>
# CHECK: <echo foo | exec grep # this exec is not allowed!>
# CHECK: <           ^~~~~~~~^>

echo 'true | time false' | $ghoti 2>| string replace -r '(.*)' '<$1>'
# CHECK: <ghoti: The 'time' command may only be at the beginning of a pipeline>
# CHECK: <true | time false>
# CHECK: <       ^~~~~~~~~^>


echo '

FOO=BAR (true one)
(true two)

# more things
' | $ghoti 2>| string replace -r '(.*)' '<$1>'

# CHECK: <ghoti: command substitutions not allowed here>
# CHECK: <FOO=BAR (true one)>
# CHECK: <        ^~~~~~~~~^>

$ghoti -c 'echo "unfinished "(subshell' 2>| string replace -r '.*' '<$0>'
# CHECK: <ghoti: Unexpected end of string, expecting ')'>
# CHECK: <echo "unfinished "(subshell>
# CHECK: <                  ^>

$ghoti -c 'echo "unfinished "$(subshell' 2>| string replace -r '.*' '<$0>'
# CHECK: <ghoti: Unexpected end of string, expecting ')'>
# CHECK: <echo "unfinished "$(subshell>
# CHECK: <                   ^>

$ghoti -c 'echo "ok $(echo still ok)syntax error: \x"' 2>| string replace -r '.*' '<$0>'
# CHECK: <ghoti: Invalid token '"ok $(echo still ok)syntax error: \x"'>
# CHECK: <echo "ok $(echo still ok)syntax error: \x">
# CHECK: <                         ^~~~~~~~~~~~~~~~^>

echo "function this_should_be_an_error" >$TMPDIR/this_should_be_an_error.ghoti
$ghoti -c "set -g ghoti_function_path $(string escape $TMPDIR); this_should_be_an_error"
# CHECKERR: ~/temp/this_should_be_an_error.ghoti (line 1): Missing end to balance this function definition
# CHECKERR: function this_should_be_an_error
# CHECKERR: ^~~~~~~^
# CHECKERR: from sourcing file ~/temp/this_should_be_an_error.ghoti
# CHECKERR: source: Error while reading file '{{.*}}/this_should_be_an_error.ghoti'
# CHECKERR: ghoti: Unknown command: this_should_be_an_error
# CHECKERR: ghoti:
# CHECKERR: set -g ghoti_function_path {{.*}}; this_should_be_an_error
# CHECKERR:                                   ^~~~~~~~~~~~~~~~~~~~~~^

$ghoti -c 'echo {$}'
# CHECKERR: ghoti: Expected a variable name after this $.
# CHECKERR: echo {$}
# CHECKERR: ^

$ghoti -c 'echo {$,}'
# CHECKERR: ghoti: Expected a variable name after this $.
# CHECKERR: echo {$,}
# CHECKERR: ^

echo "bind -M" | $ghoti
# CHECKERR: bind: -M: option requires an argument
# CHECKERR: Standard input (line 1): 
# CHECKERR: bind -M
# CHECKERR: ^
# CHECKERR: (Type 'help bind' for related documentation)

