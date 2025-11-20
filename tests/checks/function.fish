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
#CHECK: function name1 --argument-names arg1 arg2
string join \n -- $name1a[1..2]
#CHECK: # Defined in {{.*}}, copied in {{.*}}
#CHECK: function name1a --argument-names arg1 arg2
string join \n -- $name3[1..2]
#CHECK: # Defined in {{(?:(?!, copied).)*}}
#CHECK: function name3 --argument-names arg1 arg2
string join \n -- $name3a[1..2]
#CHECK: # Defined in {{.*}}, copied in {{.*}}
#CHECK: function name3a --argument-names arg1 arg2

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
    functions | grep '^$\|directory\|^actual_function$'
end
# CHECK: actual_function

rm -r $tmpdir

functions -e foo

function foo -p bar; end
# CHECKERR: {{.*}}function.fish (line {{\d+}}): function: 'bar' is not a valid process ID
# CHECKERR: function foo -p bar; end
# CHECKERR: ^~~~~~~~~~~~~~~~~~~^

function foo --argument-names "banana pajama"; end
# CHECKERR: {{.*}}function.fish (line {{\d+}}): function: banana pajama: invalid variable name. See `help language#shell-variable-and-function-names`
# CHECKERR: function foo --argument-names "banana pajama"; end
# CHECKERR: ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~^


function foo --argument-names status; end
# CHECKERR: {{.*}}function.fish (line {{\d+}}): function: variable 'status' is read-only
# CHECKERR: function foo --argument-names status; end
# CHECKERR: ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~^

echo status $status
# CHECK: status 2

function foo --argument-names foo status; end
# CHECKERR: {{.*}}function.fish (line {{\d+}}): function: variable 'status' is read-only
# CHECKERR: function foo --argument-names foo status; end
# CHECKERR: ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~^

functions -q foo
echo exists $status
# CHECK: exists 1

# If a function is changed as part of its own arguments, then we see the change.
# This codifies historic behavior.
function foo; echo before; end
foo (function foo; echo after; end)
# CHECK: after

# If a function is deleted as part of its own arguments, then we see the change.
# This codifies historic behavior.
function foo; echo before; end
foo (functions --erase foo)
# CHECKERR: error: Unknown function 'foo'

function ()
end
# CHECKERR: {{.*}}/tests/checks/function.fish (line {{\d+}}): function: function name required
# CHECKERR: function ()
# CHECKERR: ^

# Tests the --argument-names and --inherit-variable can overwrite argv
function t --argument-names a argv c
    echo $argv
end
t 1 2 3
#CHECK: 2

function t -a argv
    echo $argv
end
t 1 2 3
#CHECK: 1

function outer
    function inner -v argv -V argv
        echo $argv
    end
    set -gx argv 4 5 6
end
outer 1 2 3
#CHECK: 1 2 3

for flag in --on-process-exit --on-job-exit
    for invalid_pid in (math 2 ^ 31) -1 -(math 2 ^ 31)
        function invalid $flag=$invalid_pid
        end
    end
    # CHECKERR: {{.*}}/function.fish (line {{\d+}}): function: '2147483648' is not a valid process ID
    # CHECKERR:     function invalid $flag=$invalid_pid
    # CHECKERR:     ^
    # CHECKERR: {{.*}}/function.fish (line {{\d+}}): function: '-1' is not a valid process ID
    # CHECKERR:     function invalid $flag=$invalid_pid
    # CHECKERR:     ^
    # CHECKERR: {{.*}}/function.fish (line {{\d+}}): function: '-2147483648' is not a valid process ID
    # CHECKERR:     function invalid $flag=$invalid_pid
    # CHECKERR:     ^
    # CHECKERR: {{.*}}/function.fish (line {{\d+}}): function: '2147483648' is not a valid process ID
    # CHECKERR:     function invalid $flag=$invalid_pid
    # CHECKERR:     ^
    # CHECKERR: {{.*}}/function.fish (line {{\d+}}): function: '-1' is not a valid process ID
    # CHECKERR:     function invalid $flag=$invalid_pid
    # CHECKERR:     ^
    # CHECKERR: {{.*}}/function.fish (line {{\d+}}): function: '-2147483648' is not a valid process ID
    # CHECKERR:     function invalid $flag=$invalid_pid
    # CHECKERR:     ^
end

exit 0
