#RUN: fish=%fish %fish %s

# A $status used as a command should not impact the location of other errors.
echo 'echo foo | exec grep # this exec is not allowed!

$status

 # The error might be found here!' | $fish 2>| string replace -r '(.*)' '<$1>'

# CHECK: <fish: The 'exec' command can not be used in a pipeline>
# CHECK: <echo foo | exec grep # this exec is not allowed!>
# CHECK: <           ^~~~~~~~^>

echo 'true | time false' | $fish 2>| string replace -r '(.*)' '<$1>'
# CHECK: <fish: The 'time' command may only be at the beginning of a pipeline>
# CHECK: <true | time false>
# CHECK: <       ^~~~~~~~~^>


echo '

FOO=BAR (true one)
(true two)

# more things
' | $fish 2>| string replace -r '(.*)' '<$1>'

# CHECK: <fish: command substitutions not allowed in command position. Try var=(your-cmd) $var ...>
# CHECK: <FOO=BAR (true one)>
# CHECK: <        ^~~~~~~~~^>

$fish -c 'echo "unfinished "(subshell' 2>| string replace -r '.*' '<$0>'
# CHECK: <fish: Unexpected end of string, expecting ')'>
# CHECK: <echo "unfinished "(subshell>
# CHECK: <                  ^>

$fish -c 'echo "unfinished "$(subshell' 2>| string replace -r '.*' '<$0>'
# CHECK: <fish: Unexpected end of string, expecting ')'>
# CHECK: <echo "unfinished "$(subshell>
# CHECK: <                   ^>

$fish -c 'echo "ok $(echo still ok)syntax error: \x"' 2>| string replace -r '.*' '<$0>'
# CHECK: <fish: Invalid token '"ok $(echo still ok)syntax error: \x"'>
# CHECK: <echo "ok $(echo still ok)syntax error: \x">
# CHECK: <                         ^~~~~~~~~~~~~~~~^>

echo "function this_should_be_an_error" >$TMPDIR/this_should_be_an_error.fish
$fish -c "set -g fish_function_path $(string escape $TMPDIR); this_should_be_an_error"
# CHECKERR: ~/temp/this_should_be_an_error.fish (line 1): Missing end to balance this function definition
# CHECKERR: function this_should_be_an_error
# CHECKERR: ^~~~~~~^
# CHECKERR: from sourcing file ~/temp/this_should_be_an_error.fish
# CHECKERR: source: Error while reading file '{{.*}}/this_should_be_an_error.fish'
# CHECKERR: fish: Unknown command: this_should_be_an_error
# CHECKERR: fish:
# CHECKERR: set -g fish_function_path {{.*}}; this_should_be_an_error
# CHECKERR:                                   ^~~~~~~~~~~~~~~~~~~~~~^

$fish -c 'echo {$}'
# CHECKERR: fish: Expected a variable name after this $.
# CHECKERR: echo {$}
# CHECKERR: ^

$fish -c 'echo {$,}'
# CHECKERR: fish: Expected a variable name after this $.
# CHECKERR: echo {$,}
# CHECKERR: ^

echo "bind -M" | $fish
# CHECKERR: bind: -M: option requires an argument
# CHECKERR: Standard input (line 1):
# CHECKERR: bind -M
# CHECKERR: ^
# CHECKERR: (Type 'help bind' for related documentation)

$fish -c 'if -e; end'
# CHECKERR: fish: Unknown command: -e
# CHECKERR: fish:
# CHECKERR: if -e; end
# CHECKERR:    ^^

$fish -c 'begin --notanoption; end'
# CHECKERR: fish: Unknown command: --notanoption
# CHECKERR: fish:
# CHECKERR: begin --notanoption; end
# CHECKERR:       ^~~~~~~~~~~~^

$fish -c 'begin --help'
# CHECKERR: fish: begin: missing man page
# CHECKERR: Documentation may not be installed.
# CHECKERR: `help begin` will show an online version

$fish -c 'echo (for status in foo; end)'
# CHECKERR: fish: for: status: cannot overwrite read-only variable
# CHECKERR: for status in foo; end
# CHECKERR: ^~~~~^
# CHECKERR: in command substitution
# CHECKERR: fish: Invalid arguments
# CHECKERR: echo (for status in foo; end)
# CHECKERR: ^~~~~~~~~~~~~~~~~~~~~~~^

$fish -c 'echo (echo <&foo)'
# CHECKERR: fish: Requested redirection to 'foo', which is not a valid file descriptor
# CHECKERR: echo <&foo
# CHECKERR: ^~~~^
# CHECKERR: in command substitution
# CHECKERR: fish: Invalid arguments
# CHECKERR: echo (echo <&foo)
# CHECKERR: ^~~~~~~~~~~^


$fish -c 'echo (time echo foo &)'
# CHECKERR: fish: 'time' is not supported for background jobs. Consider using 'command time'.
# CHECKERR: time echo foo &
# CHECKERR: ^~~~~~~~~~~~~~^
# CHECKERR: in command substitution
# CHECKERR: fish: Invalid arguments
# CHECKERR: echo (time echo foo &)
# CHECKERR: ^~~~~~~~~~~~~~~~^

$fish -c 'time begin; end &'
# CHECKERR: fish: 'time' is not supported for background jobs. Consider using 'command time'.
# CHECKERR: time begin; end &
# CHECKERR: ^~~~~~~~~~~~~~~~^

$fish -c 'echo (set -l foo 1 2 3; for $foo in foo; end)'
# CHECKERR: fish: Unable to expand variable name ''
# CHECKERR: set -l foo 1 2 3; for $foo in foo; end
# CHECKERR: ^~~^
# CHECKERR: in command substitution
# CHECKERR: fish: Expansion error
# CHECKERR: echo (set -l foo 1 2 3; for $foo in foo; end)
# CHECKERR: ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~^

$fish -c 'echo (echo *nosuchname*)'
# CHECKERR: fish: No matches for wildcard '*nosuchname*'. See `help wildcards-globbing`.
# CHECKERR: echo *nosuchname*
# CHECKERR: ^~~~~~~~~~~^
# CHECKERR: in command substitution
# CHECKERR: fish: Unmatched wildcard
# CHECKERR: echo (echo *nosuchname*)
# CHECKERR: ^~~~~~~~~~~~~~~~~~^
