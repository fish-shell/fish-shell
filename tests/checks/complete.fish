# RUN: %fish %s
function complete_test_alpha1
    echo $argv
end

complete -c complete_test_alpha1 --no-files -a '(commandline)'
complete -c complete_test_alpha2 --no-files -w 'complete_test_alpha1 extra1'
complete -c complete_test_alpha3 --no-files -w 'complete_test_alpha2 extra2'

complete -C'complete_test_alpha1 arg1 '
# CHECK: complete_test_alpha1 arg1 
complete -C'complete_test_alpha2 arg2 '
# CHECK: complete_test_alpha1 extra1 arg2 
complete -C'complete_test_alpha3 arg3 '
# CHECK: complete_test_alpha1 extra1 extra2 arg3 
# Works even with the argument as a separate token.
complete -C 'complete_test_alpha3 arg3 '
# CHECK: complete_test_alpha1 extra1 extra2 arg3 

alias myalias1 'complete_test_alpha1 arg1'
alias myalias2='complete_test_alpha1 arg2'

myalias1 call1
myalias2 call2
# CHECK: arg1 call1
# CHECK: arg2 call2
complete -C'myalias1 call3 '
complete -C'myalias2 call3 '
# CHECK: complete_test_alpha1 arg1 call3 
# CHECK: complete_test_alpha1 arg2 call3 

# Ensure that commands can't wrap themselves - if this did,
# the completion would be executed a bunch of times.
function t --wraps t
    echo t
end
complete -c t -fa '(t)'
complete -C't '
# CHECK: t

# Ensure file completion happens even though it was disabled above.
complete -c t -l fileoption -rF
# Only match one file because I don't want to touch this any time we add a test file.
complete -C't --fileoption ' | string match test.fish
# CHECK: test.fish

# Make sure bare `complete` is reasonable,
complete -p '/complete test/beta1' -d 'desc, desc' -sZ
complete -c 'complete test beta2' -r -d 'desc \' desc2 [' -a 'foo bar'
complete -c complete_test_beta2 -x -n false -A -o test
complete

# CHECK: complete --no-files complete_test_alpha1 -a '(commandline)'
# CHECK: complete --no-files complete_test_alpha2
# CHECK: complete --no-files complete_test_alpha3
# CHECK: complete --force-files t -l fileoption
# CHECK: complete --no-files t -a '(t)'
# CHECK: complete -p '/complete test/beta1' -s Z -d 'desc, desc'
# CHECK: complete --requires-param 'complete test beta2' -d desc\ \'\ desc2\ \[ -a 'foo bar'
# CHECK: complete --exclusive complete_test_beta2 -o test -n false
# CHECK: complete {{.*}}
# CHECK: complete {{.*}}
# CHECK: complete {{.*}}
# CHECK: complete {{.*}}

# Recursive calls to complete (see #3474)
complete -c complete_test_recurse1 -xa '(echo recursing 1>&2; complete -C"complete_test_recurse1 ")'
LANG=C complete -C'complete_test_recurse1 '
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: recursing
# CHECKERR: error: completion reached maximum recursion depth, possible cycle?

# short options
complete -c foo -f -a non-option-argument
complete -c foo -f --short-option x
complete -c foo -f --short-option y -a ARGY
complete -c foo -f --short-option z -a ARGZ -r
complete -c foo -f --old-option single-long-ending-in-z
complete -c foo -f --old-option x-single-long
complete -c foo -f --old-option y-single-long
complete -c foo -f --old-option z-single-long
complete -c foo -f --long-option x-long -a ARGLONG
# Make sure that arguments of concatenated short options are expanded (#332)
complete -C'foo -xy'
# CHECK: -xyARGY
# CHECK: -xyz
# A required parameter means we don't want more short options.
complete -C'foo -yz'
# CHECK: -yzARGZ
# Required parameter with space: complete only the parameter (no non-option arguments).
complete -C'foo -xz '
# CHECK: ARGZ
# Optional parameter with space: complete only non-option arguments.
complete -C'foo -xy '
# CHECK: non-option-argument
complete -C'foo -single-long-ending-in-z'
# CHECK: -single-long-ending-in-z
complete -C'foo -single-long-ending-in-z '
# CHECK: non-option-argument
# CHECK: -x-single-long
complete -C'foo -x' | string match -- -x-single-long
# CHECK: -y-single-long
complete -C'foo -y' | string match -- -y-single-long
# This does NOT suggest -z-single-long, but will probably not occur in practise.
# CHECK: -zARGZ
complete -C'foo -z'


# Builtins (with subcommands; #2705)
complete -c complete_test_subcommand -n 'test (commandline -op)[1] = complete_test_subcommand' -xa ok
complete -C'not complete_test_subcommand '
# CHECK: ok
complete -C'echo; and complete_test_subcommand '
# CHECK: ok
complete -C'or complete_test_subcommand '
# CHECK: ok
complete -C'echo && command complete_test_subcommand '
# CHECK: ok
complete -C'echo || exec complete_test_subcommand '
# CHECK: ok
complete -C'echo | builtin complete_test_subcommand '
# CHECK: ok
complete -C'echo & complete_test_subcommand '
# CHECK: ok
complete -C'if while begin begin complete_test_subcommand '
# CHECK: ok

complete -C'for _ in ' | string collect >&- && echo completed some files
# CHECK: completed some files

# function; #5415
complete -C'function : --arg'
# CHECK: --argument-names	{{.*}}

complete -C'echo > /' | string length -q && echo ok
# CHECK: ok

function some_function
    echo line1
    echo line2
end
complete -c complete_test_function_desc -xa '(complete -Csome_function)'
complete -C'complete_test_function_desc ' | count
# CHECK: 1

complete -c prev-arg-variable -f
complete -C'prev-arg-variable $HOME '

# Regression test for issue #3129. In previous versions these statements would
# cause an `assert()` to fire thus killing the shell.
complete -c pkill -o ''
#CHECKERR: complete: -o requires a non-empty string
complete -c pkill -l ''
#CHECKERR: complete: -l requires a non-empty string
complete -c pkill -s ''
#CHECKERR: complete: -s requires a non-empty string

# Test that conditions that add or remove completions don't deadlock, etc.
# We actually encountered some case that was effectively like this (Issue 2 in github)

complete --command AAAA -l abcd --condition 'complete -c AAAA -l efgh'
echo "AAAA:"
complete -C'AAAA -' | sort
#CHECK: AAAA:
#CHECK: --abcd
echo "AAAA:"
complete -C'AAAA -' | sort
#CHECK: AAAA:
#CHECK: --abcd
#CHECK: --efgh

complete --command BBBB -l abcd --condition 'complete -e --command BBBB -l abcd'
echo "BBBB:"
complete -C'BBBB -'
#CHECK: BBBB:
#CHECK: --abcd
echo "BBBB:"
complete -C'BBBB -'
#CHECK: BBBB:
#CHECK: 

# Test that erasing completions works correctly
echo

function sort
    # GNU sort is really stupid, a non-C locale seems to make it assume --dictionary-order
    # If I wanted --dictionary-order, I would have specified --dictionary-order!
    env LC_ALL=C sort $argv
end

complete -c CCCC -l bar
complete -c CCCC -l baz
complete -c CCCC -o bar
complete -c CCCC -o foo
complete -c CCCC -s a
complete -c CCCC -s b
echo "CCCC:"
complete -C'CCCC -' | sort
complete -c CCCC -l bar -e
#CHECK: CCCC:
#CHECK: --bar
#CHECK: --baz
#CHECK: -a
#CHECK: -b
#CHECK: -bar
#CHECK: -foo
echo "CCCC:"
complete -C'CCCC -' | sort
complete -c CCCC -o foo -e
#CHECK: CCCC:
#CHECK: --baz
#CHECK: -a
#CHECK: -b
#CHECK: -bar
#CHECK: -foo
echo "CCCC:"
complete -C'CCCC -' | sort
complete -c CCCC -s a -e
#CHECK: CCCC:
#CHECK: --baz
#CHECK: -a
#CHECK: -b
#CHECK: -bar
echo "CCCC:"
complete -C'CCCC -' | sort
complete -c CCCC -e
#CHECK: CCCC:
#CHECK: --baz
#CHECK: -b
#CHECK: -bar
echo "CCCC:"
complete -C'CCCC -' | sort
#CHECK: CCCC:

echo "Test that -- suppresses option completions"
complete -c TestDoubleDash -l TestDoubleDashOption
complete -C'TestDoubleDash -' | sort
#CHECK: Test that -- suppresses option completions
#CHECK: --TestDoubleDashOption
echo "Expect no output:" (complete -C'TestDoubleDash -- -' | sort)
#CHECK: Expect no output:

# fish seems to have always handled "exclusive" options strangely
# It seems to treat them the same as "old-style" (single-dash) long options
echo "Testing exclusive options"
#CHECK: Testing exclusive options
complete -c TestExclusive -x -s Q
complete -c TestExclusive -x -s W
complete -c TestExclusive -s A
echo "Expect -A -Q -W:" (complete -C'TestExclusive -' | sort | string join ' ')
#CHECK: Expect -A -Q -W: -A -Q -W
echo "Expect -AQ -AW:" (complete -C'TestExclusive -A' | sort | string join ' ')
#CHECK: Expect -AQ -AW: -AQ -AW
echo "Expect no output 1:" (complete -C'TestExclusive -Q')
#CHECK: Expect no output 1:
echo "Expect no output 2:" (complete -C'TestExclusive -W')
#CHECK: Expect no output 2:

# Test for optional arguments, like cp's --backup
complete -c TestOptionalArgument -l backup -f -a 'none all simple'
echo "Expect --backup --backup=:" (complete -C'TestOptionalArgument -' | sort | string join ' ')
#CHECK: Expect --backup --backup=: --backup --backup=
echo "Expect --backup=all  --backup=none  --backup=simple:" (complete -C'TestOptionalArgument --backup=' | sort | string join ' ')
#CHECK: Expect --backup=all  --backup=none  --backup=simple: --backup=all --backup=none --backup=simple

# Test that directory completions work correctly
if begin
        rm -rf test6.tmp.dir; and mkdir test6.tmp.dir
    end
    pushd test6.tmp.dir
    set -l dir (mktemp -d XXXXXXXX)
    if complete -C$dir | grep "^$dir/.*Directory" >/dev/null
        echo "implicit cd complete works"
    else
        echo "no implicit cd complete"
    end
    #CHECK: implicit cd complete works
    if complete -C"command $dir" | grep "^$dir/.*Directory" >/dev/null
        echo "implicit cd complete after 'command'"
    else
        echo "no implicit cd complete after 'command'"
    end
    #CHECK: implicit cd complete after 'command'
    popd
    if begin
            set -l PATH $PWD/test6.tmp.dir $PATH 2>/dev/null
            complete -C$dir | grep "^$dir/.*Directory" >/dev/null
        end
        echo "incorrect implicit cd from PATH"
    else
        echo "PATH does not cause incorrect implicit cd"
    end
    #CHECK: PATH does not cause incorrect implicit cd
    rm -rf test6.tmp.dir
else
    echo "error: could not create temp environment" >&2
end

# Test command expansion with parened PATHs (#952)
begin
    set -l parened_path $PWD/'test6.tmp2.(paren).dir'
    set -l parened_subpath $parened_path/subdir
    if not begin
            rm -rf $parened_path
            and mkdir $parened_path
            and mkdir $parened_subpath
            and ln -s /bin/ls $parened_path/'__test6_(paren)_command'
            and ln -s /bin/ls $parened_subpath/'__test6_subdir_(paren)_command'
        end
        echo "error: could not create command expansion temp environment" >&2
    end

    # Verify that we can expand commands when PATH has parens
    set -l PATH $parened_path $PATH
    set -l completed (complete -C__test6_ | cut -f 1 -d \t)
    if test "$completed" = '__test6_(paren)_command'
        echo "Command completion with parened PATHs test passed"
    else
        echo "Command completion with parened PATHs test failed. Expected __test6_(paren)_command, got $completed" >&2
    end
    #CHECK: Command completion with parened PATHs test passed

    # Verify that commands with intermediate slashes do NOT expand with respect to PATH
    set -l completed (complete -Csubdir/__test6_subdir)
    if test -z "$completed"
        echo "Command completion with intermediate slashes passed"
    else
        echo "Command completion with intermediate slashes: should output nothing, instead got $completed" >&2
    end
    #CHECK: Command completion with intermediate slashes passed

    rm -rf $parened_path
end

# This should only list the completions for `banana`
complete -c banana -a '1 2 3'
complete -c banana
#CHECK: complete banana -a '1 2 3'

# "-c" is optional
complete banana -a bar
complete banana
#CHECK: complete banana -a bar
#CHECK: complete banana -a '1 2 3'

# "-a" ain't
complete banana bar
#CHECKERR: complete: Too many arguments
#CHECKERR:
#CHECKERR: {{.*}}checks/complete.fish (line {{\d+}}):
#CHECKERR: complete banana bar
#CHECKERR: ^
#CHECKERR:
#CHECKERR: (Type 'help complete' for related documentation)

# Multiple commands can be specified, in that case "-c" (or "-p") is mandatory.
complete -c kapstachelbeere -c physalis -a arg
complete -c kapstachelbeere -c physalis
# CHECK: complete kapstachelbeere -a arg
# CHECK: complete physalis -a arg

set -l dir (mktemp -d)
echo >$dir/target
complete -C ': $dir/'
# CHECK: $dir/target
rm $dir/target
rmdir $dir
