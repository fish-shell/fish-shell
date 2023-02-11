#RUN: %fish %s
function t --argument-names a b c
    echo t
end

set -g foo 'global foo'
set -l foo 'local foo'
set bar one 'two    2' \t '' 3
set baz
function frob -V foo -V bar -V baz
    set --show foo bar baz
end

frob
#CHECK: $foo: set in local scope, unexported, with 1 elements
#CHECK: $foo[1]: |local foo|
#CHECK: $foo: set in global scope, unexported, with 1 elements
#CHECK: $foo[1]: |global foo|
#CHECK: $bar: set in local scope, unexported, with 5 elements
#CHECK: $bar[1]: |one|
#CHECK: $bar[2]: |two    2|
#CHECK: $bar[3]: |\t|
#CHECK: $bar[4]: ||
#CHECK: $bar[5]: |3|
#CHECK: $bar: set in global scope, unexported, with 5 elements
#CHECK: $bar[1]: |one|
#CHECK: $bar[2]: |two    2|
#CHECK: $bar[3]: |\t|
#CHECK: $bar[4]: ||
#CHECK: $bar[5]: |3|
#CHECK: $baz: set in local scope, unexported, with 0 elements
#CHECK: $baz: set in global scope, unexported, with 0 elements

set foo 'bad foo'
set bar 'bad bar'
set baz 'bad baz'
frob
#CHECK: $foo: set in local scope, unexported, with 1 elements
#CHECK: $foo[1]: |local foo|
#CHECK: $foo: set in global scope, unexported, with 1 elements
#CHECK: $foo[1]: |global foo|
#CHECK: $bar: set in local scope, unexported, with 5 elements
#CHECK: $bar[1]: |one|
#CHECK: $bar[2]: |two    2|
#CHECK: $bar[3]: |\t|
#CHECK: $bar[4]: ||
#CHECK: $bar[5]: |3|
#CHECK: $bar: set in global scope, unexported, with 1 elements
#CHECK: $bar[1]: |bad bar|
#CHECK: $baz: set in local scope, unexported, with 0 elements
#CHECK: $baz: set in global scope, unexported, with 1 elements
#CHECK: $baz[1]: |bad baz|

# This sequence of tests originally verified that functions `name2` and
# `name4` were created. See issue #2068. That behavior is not what we want.
# The function name must always be the first argument of the `function`
# command. See issue #2827.
function name1 -a arg1 arg2
    echo hello
end
function -a arg1 arg2 name2
end
#CHECKERR: {{.*}}checks/function.fish (line {{\d+}}): function: -a: invalid function name
#CHECKERR: function -a arg1 arg2 name2
#CHECKERR: ^
function name3 --argument-names arg1 arg2
    echo hello
    echo goodbye
end
function --argument-names arg1 arg2 name4
end
#CHECKERR: {{.*}}checks/function.fish (line {{\d+}}): function: --argument-names: invalid function name
#CHECKERR: function --argument-names arg1 arg2 name4
#CHECKERR: ^
function name5 abc --argument-names def
end
#CHECKERR: {{.*}}checks/function.fish (line {{\d+}}): function: abc: unexpected positional argument
#CHECKERR: function name5 abc --argument-names def
#CHECKERR: ^
functions -q name1; and echo "Function name1 found"
functions -q name2; or echo "Function name2 not found as expected"
functions -q name3; and echo "Function name3 found"
functions -q name4; or echo "Function name4 not found as expected"
#CHECK: Function name1 found
#CHECK: Function name2 not found as expected
#CHECK: Function name3 found
#CHECK: Function name4 not found as expected

functions -c name1 name1a
functions --copy name3 name3a
functions -q name1a
or echo "Function name1a not found as expected"
functions -q name3a
or echo "Function name3a not found as expected"

# Poor man's diff because on some systems diff defaults to unified output, but that prints filenames.
#
set -l name1 (functions name1)
set -l name1a (functions name1a)
set -l name3 (functions name3)
set -l name3a (functions name3a)
# First two lines for the copied and non-copied functions are different. Skip it for now.
test "$name1[3..-1]" = "$name1a[3..-1]"; and echo "1 = 1a"
#CHECK: 1 = 1a
test "$name3[3..-1]" = "$name3a[3..-1]"; and echo "3 = 3a"
#CHECK: 3 = 3a

# Test the first two lines.
string join \n -- $name1[1..2]
#CHECK: # Defined in {{(?:(?!, copied).)*}}
#CHECK: function name1 --argument arg1 arg2
string join \n -- $name1a[1..2]
#CHECK: # Defined in {{.*}}, copied in {{.*}}
#CHECK: function name1a --argument arg1 arg2
string join \n -- $name3[1..2]
#CHECK: # Defined in {{(?:(?!, copied).)*}}
#CHECK: function name3 --argument arg1 arg2
string join \n -- $name3a[1..2]
#CHECK: # Defined in {{.*}}, copied in {{.*}}
#CHECK: function name3a --argument arg1 arg2

function test
    echo banana
end
#CHECKERR: {{.*}}checks/function.fish (line {{\d+}}): function: test: cannot use reserved keyword as function name
#CHECKERR: function test
#CHECKERR: ^

functions -q; or echo False
#CHECK: False

# See that we don't count a file with an empty function name,
# or directories
set -l tmpdir (mktemp -d)
touch $tmpdir/.fish
mkdir $tmpdir/directory.fish
touch $tmpdir/actual_function.fish

begin
    set -l fish_function_path $tmpdir
    functions
end
# CHECK: actual_function

# these are functions defined either in this file,
# or eagerly in share/config.fish.
# I don't know of a way to ignore just them.
#
# CHECK: bg
# CHECK: disown
# CHECK: fg
# CHECK: fish_command_not_found
# CHECK: fish_sigtrap_handler
# CHECK: frob
# CHECK: kill
# CHECK: name1
# CHECK: name1a
# CHECK: name3
# CHECK: name3a
# CHECK: t
# CHECK: wait

rm -r $tmpdir

exit 0
