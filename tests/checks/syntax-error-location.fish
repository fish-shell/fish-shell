#RUN: fish=%fish %fish %s

# A $status used as a command should not impact the location of other errors.
$fish -c 'echo foo | exec grep # this exec is not allowed!

$status

 # The error might be found here!'

# CHECKERR: fish: The 'exec' command can not be used in a pipeline
# CHECKERR: {{^}}echo foo | exec grep # this exec is not allowed!
# CHECKERR: {{^}}           ^~~~~~~~^

$fish -c 'true | time false'
# CHECKERR: fish: The 'time' command may only be at the beginning of a pipeline
# CHECKERR: {{^}}true | time false
# CHECKERR: {{^}}       ^~~~~~~~~^


$fish -c '

FOO=BAR (true one)
(true two)

# more things
'
# CHECKERR: fish: command substitutions not allowed in command position. Try var=(your-cmd) $var ...
# CHECKERR: {{^}}FOO=BAR (true one)
# CHECKERR: {{^}}        ^~~~~~~~~^

$fish -c 'echo "unfinished "(subshell'
# CHECKERR: fish: Unexpected end of string, expecting ')'
# CHECKERR: {{^}}echo "unfinished "(subshell
# CHECKERR: {{^}}                  ^

$fish -c 'echo "unfinished "$(subshell'
# CHECKERR: fish: Unexpected end of string, expecting ')'
# CHECKERR: {{^}}echo "unfinished "$(subshell
# CHECKERR: {{^}}                   ^

$fish -c 'echo "ok $(echo still ok)syntax error: \x"'
# CHECKERR: fish: Invalid token '"ok $(echo still ok)syntax error: \x"'
# CHECKERR: {{^}}echo "ok $(echo still ok)syntax error: \x"
# CHECKERR: {{^}}                         ^~~~~~~~~~~~~~~~^

echo "function this_should_be_an_error" >$TMPDIR/this_should_be_an_error.fish
$fish -c "set -g fish_function_path $(string escape $TMPDIR); this_should_be_an_error" 2>| string replace -r "^ {$(string length "$TMPDIR")}" "      " >&2
# CHECKERR: ~/temp/this_should_be_an_error.fish (line 1): Missing end to balance this function definition
# CHECKERR: {{^}}function this_should_be_an_error
# CHECKERR: {{^}}^~~~~~~^
# CHECKERR: from sourcing file ~/temp/this_should_be_an_error.fish
# CHECKERR: source: Error while reading file '{{.*}}/this_should_be_an_error.fish'
# CHECKERR: fish: Unknown command: this_should_be_an_error
# CHECKERR: fish:
# CHECKERR: {{^}}set -g fish_function_path {{.*}}; this_should_be_an_error
# CHECKERR: {{^}}                                  ^~~~~~~~~~~~~~~~~~~~~~^

$fish -c 'echo {$}'
# CHECKERR: fish: Expected a variable name after this $.
# CHECKERR: {{^}}echo {$}
# CHECKERR: {{^}}       ^

$fish -c 'echo {$,}'
# CHECKERR: fish: Expected a variable name after this $.
# CHECKERR: {{^}}echo {$,}
# CHECKERR: {{^}}       ^

echo "bind -M" | $fish
# CHECKERR: bind: -M: option requires an argument
# CHECKERR: Standard input (line 1):
# CHECKERR: {{^}}bind -M
# CHECKERR: {{^}}^
# CHECKERR: (Type 'help bind' for related documentation)

$fish -c 'if -e; end'
# CHECKERR: fish: Unknown command: -e
# CHECKERR: fish:
# CHECKERR: {{^}}if -e; end
# CHECKERR: {{^}}   ^^

$fish -c 'begin --notanoption; end'
# CHECKERR: fish: Unknown command: --notanoption
# CHECKERR: fish:
# CHECKERR: {{^}}begin --notanoption; end
# CHECKERR: {{^}}      ^~~~~~~~~~~~^

$fish -c 'begin --help'
# CHECKERR: Documentation for begin

$fish -c 'echo (for status in foo; end)'
# CHECKERR: fish: for: status: cannot overwrite read-only variable
# CHECKERR: {{^}}for status in foo; end
# CHECKERR: {{^}}    ^~~~~^
# CHECKERR: in command substitution
# CHECKERR: fish: Invalid arguments
# CHECKERR: {{^}}echo (for status in foo; end)
# CHECKERR: {{^}}     ^~~~~~~~~~~~~~~~~~~~~~~^

$fish -c 'echo (echo <&foo)'
# CHECKERR: fish: Requested redirection to 'foo', which is not a valid file descriptor
# CHECKERR: {{^}}echo <&foo
# CHECKERR: {{^}}     ^~~~^
# CHECKERR: in command substitution
# CHECKERR: fish: Invalid arguments
# CHECKERR: {{^}}echo (echo <&foo)
# CHECKERR: {{^}}     ^~~~~~~~~~~^


$fish -c 'echo (time echo foo &)'
# CHECKERR: fish: 'time' is not supported for background jobs. Consider using 'command time'.
# CHECKERR: {{^}}time echo foo &
# CHECKERR: {{^}}^~~~~~~~~~~~~~^
# CHECKERR: in command substitution
# CHECKERR: fish: Invalid arguments
# CHECKERR: {{^}}echo (time echo foo &)
# CHECKERR: {{^}}     ^~~~~~~~~~~~~~~~^

$fish -c 'time begin; end &'
# CHECKERR: fish: 'time' is not supported for background jobs. Consider using 'command time'.
# CHECKERR: {{^}}time begin; end &
# CHECKERR: {{^}}^~~~~~~~~~~~~~~~^

$fish -c 'echo (set -l foo 1 2 3; for $foo in foo; end)'
# CHECKERR: fish: Unable to expand variable name ''
# CHECKERR: {{^}}set -l foo 1 2 3; for $foo in foo; end
# CHECKERR: {{^}}                      ^~~^
# CHECKERR: in command substitution
# CHECKERR: fish: Expansion error
# CHECKERR: {{^}}echo (set -l foo 1 2 3; for $foo in foo; end)
# CHECKERR: {{^}}     ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~^

$fish -c 'echo (echo *nosuchname*)'
# CHECKERR: fish: No matches for wildcard '*nosuchname*'. See `help language#wildcards-globbing`.
# CHECKERR: {{^}}echo *nosuchname*
# CHECKERR: {{^}}     ^~~~~~~~~~~^
# CHECKERR: in command substitution
# CHECKERR: fish: Unmatched wildcard
# CHECKERR: {{^}}echo (echo *nosuchname*)
# CHECKERR: {{^}}     ^~~~~~~~~~~~~~~~~~^

$fish -c 'foo )'
# CHECKERR: fish: Unexpected ')' for unopened parenthesis
# CHECKERR: {{^}}foo )
# CHECKERR: {{^}}    ^

$fish -c 'foo (})'
# CHECKERR: fish: Unexpected '}' found, expecting ')'
# CHECKERR: {{^}}foo (})
# CHECKERR: {{^}}     ^

$fish -c 'foo {)}'
# CHECKERR: fish: Unexpected ')' found, expecting '}'
# CHECKERR: {{^}}foo {)}
# CHECKERR: {{^}}     ^

$fish -c 'foo $var[)]'
# CHECKERR: fish: Unexpected ')' found, expecting ']'
# CHECKERR: {{^}}foo $var[)]
# CHECKERR: {{^}}         ^

$fish -c 'foo $var[}]'
# CHECKERR: fish: Unexpected '}' found, expecting ']'
# CHECKERR: {{^}}foo $var[}]
# CHECKERR: {{^}}         ^

$fish -c 'echo foo"bar$(echo)$(echo)bur'
# CHECKERR: fish: Unexpected end of string, quotes are not balanced
# CHECKERR: {{^}}echo foo"bar$(echo)$(echo)bur
# CHECKERR: {{^}}        ^


# Check that the error is on the last unclosed expression
$fish -c 'echo abc"def$(ghi{jkl$mno[pqr'
# CHECKERR: fish: Unexpected end of string, square brackets do not match
# CHECKERR: {{^}}echo abc"def$(ghi{jkl$mno[pqr
# CHECKERR: {{^}}                         ^
$fish -c 'echo abc(def{ghi$jkl[mno"pqr'
# CHECKERR: fish: Unexpected end of string, quotes are not balanced
# CHECKERR: {{^}}echo abc(def{ghi$jkl[mno"pqr
# CHECKERR: {{^}}                        ^
$fish -c 'echo abc{def$ghi[jkl"mno$(pqr'
# CHECKERR: fish: Unexpected end of string, expecting ')'
# CHECKERR: {{^}}echo abc{def$ghi[jkl"mno$(pqr
# CHECKERR: {{^}}                         ^
$fish -c 'echo abc$def[ghi"jkl$(mno{pqr'
# CHECKERR: fish: Unexpected end of string, incomplete parameter expansion
# CHECKERR: {{^}}echo abc$def[ghi"jkl$(mno{pqr
# CHECKERR: {{^}}                         ^
