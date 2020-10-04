# RUN: env XDG_CONFIG_HOME="$(mktemp -d)" FISH=%fish %fish %s
# Environment variable tests

# Test if variables can be properly set

set smurf blue

if test $smurf = blue
    echo Test 1 pass
else
    echo Test 1 fail
end
# CHECK: Test 1 pass

# Test if variables can be erased

set -e smurf
if set -q smurf
    echo Test 2 fail
else
    echo Test 2 pass
end
# CHECK: Test 2 pass

# Test if local variables go out of scope

set -e t3
if true
    set -l t3 bar
end

if set -q t3
    echo Test 3 fail
else
    echo Test 3 pass
end
# CHECK: Test 3 pass

# Test if globals can be set in block scope

if true
    set -g baz qux
end

if test $baz = qux
    echo Test 4 pass
else
    echo Test 4 fail
end
# CHECK: Test 4 pass


#Test that scope is preserved when setting a new value

set t5 a

if true
    set t5 b
end

if test $t5 = b
    echo Test 5 pass
else
    echo Test 5 fail
end
# CHECK: Test 5 pass

# Test that scope is preserved in double blocks

for i in 1
    set t6 $i
    for j in a
        if test $t6$j = 1a
            echo Test 6 pass
        else
            echo Test 6 fail
        end
    end
end
# CHECK: Test 6 pass


# Test if variables in for loop blocks do not go out of scope on new laps

set res fail

set -e t7
for i in 1 2
    if test $i = 1
        set t7 lala
    else
        if test $t7
            set res pass
        end
    end
end

echo Test 7 $res
# CHECK: Test 7 pass

# Test if variables are properly exported

set -e t8
if true
    set -lx t8 foo
    if test ($FISH -c "echo $t8") = foo
        echo Test 8 pass
    else
        echo Test 8 fail
    end
end
# CHECK: Test 8 pass

# Test if exported variables go out of scope

if $FISH -c "set -q t8; and exit 0; or exit 1"
    echo Test 9 fail
else
    echo Test 9 pass
end
# CHECK: Test 9 pass

# Test erasing variables in specific scope

set -eU __fish_test_universal_variables_variable_foo
set -g __fish_test_universal_variables_variable_foo bar
begin
    set -l __fish_test_universal_variables_variable_foo baz
    set -eg __fish_test_universal_variables_variable_foo
end

if set -q __fish_test_universal_variables_variable_foo
    echo Test 10 fail
else
    echo Test 10 pass
end
# CHECK: Test 10 pass

set __fish_test_universal_variables_variable_foo abc def
set -e __fish_test_universal_variables_variable_foo[1]
if test $__fish_test_universal_variables_variable_foo '=' def
    echo Test 11 pass
else
    echo Test 11 fail
end
# CHECK: Test 11 pass

# Test combinations of export and scope

set -ge __fish_test_universal_variables_variable_foo
set -Ue __fish_test_universal_variables_variable_foo
set -Ux __fish_test_universal_variables_variable_foo bar
set __fish_test_universal_variables_variable_foo baz
if test (/bin/sh -c 'echo $__fish_test_universal_variables_variable_foo') = baz -a ($FISH -c 'echo $__fish_test_universal_variables_variable_foo') = baz
    echo Test 12 pass
else
    echo Test 12 fail
end
# CHECK: Test 12 pass

set -Ue __fish_test_universal_variables_variable_foo

# Should no longer be in environment (#2046)
env | string match '__fish_test_universal_variables_variable_foo=*'

set -Ux __fish_test_universal_variables_variable_foo bar
set -U __fish_test_universal_variables_variable_foo baz
if test (/bin/sh -c 'echo $__fish_test_universal_variables_variable_foo') = baz -a ($FISH -c 'echo $__fish_test_universal_variables_variable_foo') = baz
    echo Test 13 pass
else
    echo Test 13 fail
end
# CHECK: Test 13 pass

set -Ux __fish_test_universal_variables_variable_foo bar
set -u __fish_test_universal_variables_variable_foo bar
if test (/bin/sh -c 'echo $__fish_test_universal_variables_variable_foo') = '' -a ($FISH -c 'echo $__fish_test_universal_variables_variable_foo') = bar
    echo Test 14 pass
else
    echo Test 14 fail
end
# CHECK: Test 14 pass

set -Ux __fish_test_universal_variables_variable_foo bar
set -Uu __fish_test_universal_variables_variable_foo baz
if test (/bin/sh -c 'echo $__fish_test_universal_variables_variable_foo') = '' -a ($FISH -c 'echo $__fish_test_universal_variables_variable_foo') = baz
    echo Test 15 pass
else
    echo Test 15 fail
end
# CHECK: Test 15 pass

set -eU __fish_test_universal_variables_variable_foo
function watch_foo --on-variable __fish_test_universal_variables_variable_foo
    echo Foo change detected
end

set -U __fish_test_universal_variables_variable_foo 1234
# CHECK: Foo change detected
set -eU __fish_test_universal_variables_variable_foo
# CHECK: Foo change detected
# WTF set -eg __fish_test_universal_variables_variable_foo

functions -e watch_foo

# test erasing variables without a specified scope

set -g test16res
set -U __fish_test_universal_variables_variable_foo universal
set -g __fish_test_universal_variables_variable_foo global

begin
    set -l __fish_test_universal_variables_variable_foo blocklocal

    function test16
        set -l __fish_test_universal_variables_variable_foo function
        begin
            set -l __fish_test_universal_variables_variable_foo functionblock
            set test16res $test16res $__fish_test_universal_variables_variable_foo

            # This sequence seems pointless but it's really verifying that we
            # successfully expose higher scopes as we erase the closest scope.
            set -e __fish_test_universal_variables_variable_foo
            set test16res $test16res $__fish_test_universal_variables_variable_foo

            set -e __fish_test_universal_variables_variable_foo
            set test16res $test16res $__fish_test_universal_variables_variable_foo

            set -e __fish_test_universal_variables_variable_foo
            set test16res $test16res $__fish_test_universal_variables_variable_foo

            set -e __fish_test_universal_variables_variable_foo
            set -q __fish_test_universal_variables_variable_foo
            and set test16res $test16res $__fish_test_universal_variables_variable_foo
        end

        set -q __fish_test_universal_variables_variable_foo
        and echo __fish_test_universal_variables_variable_foo should set after test16 inner begin..end
    end

    test16
    set test16res $test16res $__fish_test_universal_variables_variable_foo
end
# CHECK: count:5 content:[functionblock function global universal blocklocal]

set -q __fish_test_universal_variables_variable_foo
and echo __fish_test_universal_variables_variable_foo should set after test16 outer begin..end

echo count:(count $test16res) "content:[$test16res]"
if test (count $test16res) = 5 -a "$test16res" = "functionblock function global universal blocklocal"
    echo Test 16 pass
else
    echo Test 16 fail
end
# CHECK: Test 16 pass

# Test that shadowing with a non-exported variable works
set -gx __fish_test_env17 UNSHADOWED
env | string match '__fish_test_env17=*'
# CHECK: __fish_test_env17=UNSHADOWED

function __fish_test_shadow
    set -l __fish_test_env17
    env | string match -q '__fish_test_env17=*'; or echo SHADOWED
end
__fish_test_shadow
# CHECK: SHADOWED

# Test that the variable is still exported (#2611)
env | string match '__fish_test_env17=*'
# CHECK: __fish_test_env17=UNSHADOWED

# Test that set var (command substitution) works with if/while.

if set fish_test_18 (false)
    echo Test 18 fail
else
    echo Test 18 pass
end
# CHECK: Test 18 pass

if not set fish_test_18 (true)
    echo Test 18 fail
else
    echo Test 18 pass
end
# CHECK: Test 18 pass

set __fish_test_18_status pass
while set fish_test_18 (false); or not set fish_test_18 (true)
    set __fish_test_18_status fail
    break
end
echo Test 18 $__fish_test_18_status
# CHECK: Test 18 pass

# Test that local exported variables are copied to functions (#1091)
function __fish_test_local_export
    echo $var
    set var boo
    echo $var
end
set -lx var wuwuwu
__fish_test_local_export
# CHECK: wuwuwu
# CHECK: boo
echo $var
# CHECK: wuwuwu

# Test that we don't copy local-exports to blocks.
set -lx var foo
begin
    echo $var
    # CHECK: foo
    set var bar
    echo $var
    # CHECK: bar
end
echo $var # should be "bar"
# CHECK: bar

# clear for other shells
set -eU __fish_test_universal_variables_variable_foo

# Test behavior of universals on startup (#1526)
echo Testing Universal Startup
# CHECK: Testing Universal Startup
set -U testu 0
$FISH -c 'set -U testu 1'
echo $testu
# CHECK: 1
$FISH -c 'set -q testu; and echo $testu'
# CHECK: 1

$FISH -c 'set -U testu 2'
echo $testu
# CHECK: 2
$FISH -c 'set -q testu; and echo $testu'
# CHECK: 2

$FISH -c 'set -e testu'
set -q testu
or echo testu undef in top level shell
# CHECK: testu undef in top level shell
$FISH -c 'set -q testu; or echo testu undef in sub shell'
# CHECK: testu undef in sub shell

# test SHLVL
# use a subshell to ensure a clean slate
env SHLVL= $FISH -c 'echo SHLVL: $SHLVL; $FISH -c \'echo SHLVL: $SHLVL\''
# CHECK: SHLVL: 1
# CHECK: SHLVL: 2

# exec should decrement SHLVL
env SHLVL= $FISH -c 'echo SHLVL: $SHLVL; exec $FISH -c \'echo SHLVL: $SHLVL\''
# CHECK: SHLVL: 1
# CHECK: SHLVL: 1

# garbage SHLVLs should be treated as garbage
env SHLVL=3foo $FISH -c 'echo SHLVL: $SHLVL'
# CHECK: SHLVL: 1

# whitespace is allowed though (for bash compatibility)
env SHLVL="3  " $FISH -c 'echo SHLVL: $SHLVL'
env SHLVL="  3" $FISH -c 'echo SHLVL: $SHLVL'
# CHECK: SHLVL: 4
# CHECK: SHLVL: 4

# Test transformation of inherited variables
env DISPLAY="localhost:0.0" $FISH -c 'echo Elements in DISPLAY: (count $DISPLAY)'
# CHECK: Elements in DISPLAY: 1

# We can't use PATH for this because the global configuration will modify PATH
# based on /etc/paths and /etc/paths.d.
# Exported arrays are colon delimited; they are automatically split on colons if they end in PATH.
set -gx FOO one two three four
set -gx FOOPATH one two three four
$FISH -c 'echo Elements in FOO and FOOPATH: (count $FOO) (count $FOOPATH)'
# CHECK: Elements in FOO and FOOPATH: 1 4

# some must use colon separators!
set -lx MANPATH man1 man2 man3
env | grep '^MANPATH='
# CHECK: MANPATH=man1:man2:man3

# ensure we don't escape space and colon values
set -x DONT_ESCAPE_COLONS 1: 2: :3:
env | grep '^DONT_ESCAPE_COLONS='
# CHECK: DONT_ESCAPE_COLONS=1: 2: :3:

set -x DONT_ESCAPE_SPACES '1 ' '2 ' ' 3 ' 4
env | grep '^DONT_ESCAPE_SPACES='
# CHECK: DONT_ESCAPE_SPACES=1  2   3  4

set -x DONT_ESCAPE_COLONS_PATH 1: 2: :3:
env | grep '^DONT_ESCAPE_COLONS_PATH='
# CHECK: DONT_ESCAPE_COLONS_PATH=1::2:::3:


# Path universal variables
set -U __fish_test_path_not a b c
set -U __fish_test_PATH 1 2 3
echo "$__fish_test_path_not $__fish_test_PATH" $__fish_test_path_not $__fish_test_PATH
# CHECK: a b c 1:2:3 a b c 1 2 3

set --unpath __fish_test_PATH $__fish_test_PATH
echo "$__fish_test_path_not $__fish_test_PATH" $__fish_test_path_not $__fish_test_PATH
# CHECK: a b c 1 2 3 a b c 1 2 3

set --path __fish_test_path_not $__fish_test_path_not
echo "$__fish_test_path_not $__fish_test_PATH" $__fish_test_path_not $__fish_test_PATH
# CHECK: a:b:c 1 2 3 a b c 1 2 3

set --path __fish_test_PATH $__fish_test_PATH
echo "$__fish_test_path_not $__fish_test_PATH" $__fish_test_path_not $__fish_test_PATH
# CHECK: a:b:c 1:2:3 a b c 1 2 3

set -U __fish_test_PATH 1:2:3
echo "$__fish_test_PATH" $__fish_test_PATH
# CHECK: 1:2:3 1 2 3

set -e __fish_test_PATH
set -e __fish_test_path_not

set -U --path __fish_test_path2 a:b
echo "$__fish_test_path2" $__fish_test_path2
# CHECK: a:b a b

set -e __fish_test_path2

# Test empty uvars (#5992)
set -Ux __fish_empty_uvar
set -Uq __fish_empty_uvar
echo $status
# CHECK: 0
$FISH -c 'set -Uq __fish_empty_uvar; echo $status'
# CHECK: 0
env | grep __fish_empty_uvar
# CHECK: __fish_empty_uvar=

# Variable names in other commands
# Test invalid variable names in loops (#5800)
for a,b in y 1 z 3
    echo $a,$b
end
# CHECKERR: {{.*}} for: Variable name 'a,b' is not valid. See `help identifiers`.
# CHECKERR:
# CHECKERR: for a,b in y 1 z 3
# CHECKERR:     ^


# Global vs Universal Unspecified Scopes
set -U __fish_test_global_vs_universal universal
echo "global-vs-universal 1: $__fish_test_global_vs_universal"
# CHECK: global-vs-universal 1: universal

set -g __fish_test_global_vs_universal global
echo "global-vs-universal 2: $__fish_test_global_vs_universal"
# CHECK: global-vs-universal 2: global


set __fish_test_global_vs_universal global2
echo "global-vs-universal 3: $__fish_test_global_vs_universal"
# CHECK: global-vs-universal 3: global2

set -e -g __fish_test_global_vs_universal
echo "global-vs-universal 4: $__fish_test_global_vs_universal"
# CHECK: global-vs-universal 4: universal

set -e -U __fish_test_global_vs_universal
echo "global-vs-universal 5: $__fish_test_global_vs_universal"
# CHECK: global-vs-universal 5: 

# Export local variables from all parent scopes (issue #6153).
function func
    echo $local
end
set -lx local outer
func
# CHECK: outer
begin
    func
    # CHECK: outer

    set -lx local inner
    begin
        func
    end
    # CHECK: inner
end

# Skip importing universal variables (#5258)
while set -q EDITOR
    set -e EDITOR
end
set -Ux EDITOR emacs -nw
# CHECK: $EDITOR: set in universal scope, exported, with 2 elements
$FISH -c 'set -S EDITOR' | string match -r -e 'global|universal'

# When the variable has been changed outside of fish we accept it.
# CHECK: $EDITOR: set in global scope, exported, with 1 elements
# CHECK: $EDITOR: set in universal scope, exported, with 2 elements
sh -c "EDITOR='vim -g' $FISH -c "'\'set -S EDITOR\'' | string match -r -e 'global|universal'

# Verify behavior of `set --show` given an invalid var name
set --show 'argle bargle'
#CHECKERR: $argle bargle: invalid var name

# Verify behavior of `set --show`
set semiempty ''
set --show semiempty
#CHECK: $semiempty: set in global scope, unexported, with 1 elements
#CHECK: $semiempty[1]: ||

set -U var1 hello
set --show var1
#CHECK: $var1: set in universal scope, unexported, with 1 elements
#CHECK: $var1[1]: |hello|

set -l var1
set -g var1 goodbye "and don't come back"
set --show var1
#CHECK: $var1: set in local scope, unexported, with 0 elements
#CHECK: $var1: set in global scope, unexported, with 2 elements
#CHECK: $var1[1]: |goodbye|
#CHECK: $var1[2]: |and don\'t come back|
#CHECK: $var1: set in universal scope, unexported, with 1 elements
#CHECK: $var1[1]: |hello|

set -g var2
set --show _unset_var var2
#CHECK: $var2: set in global scope, unexported, with 0 elements

# Appending works
set -g var3a a b c
set -a var3a
set -a var3a d
set -a var3a e f
set --show var3a
#CHECK: $var3a: set in global scope, unexported, with 6 elements
#CHECK: $var3a[1]: |a|
#CHECK: $var3a[2]: |b|
#CHECK: $var3a[3]: |c|
#CHECK: $var3a[4]: |d|
#CHECK: $var3a[5]: |e|
#CHECK: $var3a[6]: |f|
set -g var3b
set -a var3b
set --show var3b
#CHECK: $var3b: set in global scope, unexported, with 0 elements
set -g var3c
set -a var3c 'one string'
set --show var3c
#CHECK: $var3c: set in global scope, unexported, with 1 elements
#CHECK: $var3c[1]: |one string|

# Prepending works
set -g var4a a b c
set -p var4a
set -p var4a d
set -p var4a e f
set --show var4a
#CHECK: $var4a: set in global scope, unexported, with 6 elements
#CHECK: $var4a[1]: |e|
#CHECK: $var4a[2]: |f|
#CHECK: $var4a[3]: |d|
#CHECK: $var4a[4]: |a|
#CHECK: $var4a[5]: |b|
#CHECK: $var4a[6]: |c|
set -g var4b
set -p var4b
set --show var4b
#CHECK: $var4b: set in global scope, unexported, with 0 elements
set -g var4c
set -p var4c 'one string'
set --show var4c
#CHECK: $var4c: set in global scope, unexported, with 1 elements
#CHECK: $var4c[1]: |one string|

# Appending and prepending at same time works
set -g var5 abc def
set -a -p var5 0 x 0
set --show var5
#CHECK: $var5: set in global scope, unexported, with 8 elements
#CHECK: $var5[1]: |0|
#CHECK: $var5[2]: |x|
#CHECK: $var5[3]: |0|
#CHECK: $var5[4]: |abc|
#CHECK: $var5[5]: |def|
#CHECK: $var5[6]: |0|
#CHECK: $var5[7]: |x|
#CHECK: $var5[8]: |0|

# Setting local scope when no local scope of the var uses the closest scope
set -g var6 ghi jkl
begin
    set -l -a var6 mno
    set --show var6
end
#CHECK: $var6: set in local scope, unexported, with 3 elements
#CHECK: $var6[1]: |ghi|
#CHECK: $var6[2]: |jkl|
#CHECK: $var6[3]: |mno|
#CHECK: $var6: set in global scope, unexported, with 2 elements
#CHECK: $var6[1]: |ghi|
#CHECK: $var6[2]: |jkl|

# Exporting works
set -x TESTVAR0
set -x TESTVAR1 a
set -x TESTVAR2 a b
env | grep TESTVAR | sort | cat -v
#CHECK: TESTVAR0=
#CHECK: TESTVAR1=a
#CHECK: TESTVAR2=a b

# if/for/while scope
function test_ifforwhile_scope
    if set -l ifvar1 (true && echo val1)
    end
    if set -l ifvar2 (echo val2 && false)
    end
    if false
    else if set -l ifvar3 (echo val3 && false)
    end
    while set -l whilevar1 (echo val3 ; false)
    end
    set --show ifvar1 ifvar2 ifvar3 whilevar1
end
test_ifforwhile_scope
#CHECK: $ifvar1: set in local scope, unexported, with 1 elements
#CHECK: $ifvar1[1]: |val1|
#CHECK: $ifvar2: set in local scope, unexported, with 1 elements
#CHECK: $ifvar2[1]: |val2|
#CHECK: $ifvar3: set in local scope, unexported, with 1 elements
#CHECK: $ifvar3[1]: |val3|
#CHECK: $whilevar1: set in local scope, unexported, with 1 elements
#CHECK: $whilevar1[1]: |val3|

# $status should always be read-only, setting it makes no sense because it's immediately overwritten.
set -g status 5
#CHECKERR: set: Tried to change the read-only variable 'status'

while set -e __fish_test_universal_exported_var
end
set -xU __fish_test_universal_exported_var 1
$FISH -c 'set __fish_test_universal_exported_var 2'
env | string match -e __fish_test_universal_exported_var
#CHECK: __fish_test_universal_exported_var=2

# Test that computed variables are global.
# If they can be set they can only be set in global scope,
# so they should only be shown in global scope.
set -S status
#CHECK: $status: set in global scope, unexported, with 1 elements
#CHECK: $status[1]: |0|

set -ql history
echo $status
#CHECK: 1

set --path newvariable foo
set -S newvariable
#CHECK: $newvariable: set in global scope, unexported, a path variable with 1 elements
#CHECK: $newvariable[1]: |foo|

set foo foo
set bar bar
set -e baz

set -e foo baz bar
echo $status
#CHECK: 4
set -S foo baz bar

set foo 1 2 3
set bar 1 2 3

set -e foo[1] bar[2]
echo $foo
#CHECK: 2 3
echo $bar
#CHECK: 1 3

true
