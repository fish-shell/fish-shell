#RUN: %fish -C 'set -l fish %fish' %s
function complete_test_alpha1
    echo $argv
end

complete -c complete_test_alpha1 --no-files -a '(commandline)'
complete -c complete_test_alpha2 --no-files -w 'complete_test_alpha1 extra1'
complete -c complete_test_alpha3 --no-files -w 'complete_test_alpha2 extra2'

complete -C'complete_test_alpha1 arg1 '
# CHECK: complete_test_alpha1 arg1
complete --escape -C'complete_test_alpha1 arg1 '
# CHECK: 'complete_test_alpha1 arg1 '
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

# See that an empty command gets files
complete -C'"" t' | string match test.fish
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
# CHECK: complete --require-parameter 'complete test beta2' -d "desc ' desc2 [" -a 'foo bar'
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
complete -c complete_test_subcommand -n 'test (commandline -xp)[1] = complete_test_subcommand' -xa ok
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
    # The "incorrect implicit cd from PATH" fails if mktemp returns an absolute path and
    # `realpath --relative-to` is not available on macOS.
    # set dir (realpath --relative-to="$PWD" (mktemp -d XXXXXXXX))
    set dir (basename (mktemp -d XXXXXXXX))
    mkdir -p $dir
    if complete -C$dir | string match -r "^$dir/.*dir" >/dev/null
        echo "implicit cd complete works"
    else
        echo "no implicit cd complete"
    end
    #CHECK: implicit cd complete works
    if complete -C"command $dir" | string match -r "^$dir/.*dir" >/dev/null
        echo "implicit cd complete after 'command'"
    else
        echo "no implicit cd complete after 'command'"
    end
    #CHECK: implicit cd complete after 'command'
    popd
    if begin
            set -l PATH $PWD/test6.tmp.dir $PATH 2>/dev/null
            complete -C$dir | string match -r "^$dir/.*dir" >/dev/null
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
#CHECKERR: complete: too many arguments
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

cd $dir
touch yummyinmytummy
complete -c fudge -f
complete -c fudge -n '__fish_seen_subcommand_from eat' -F
complete -C'fudge eat yummyin'
# CHECK: yummyinmytummy
complete -C"echo no commpletion inside comment # "
cd -

rm -r $dir

set -l dir (mktemp -d)
cd $dir

: >command-not-in-path
chmod +x command-not-in-path
complete -p $PWD/command-not-in-path -xa relative-path
complete -C './command-not-in-path '
# CHECK: relative-path

# Expand variables and tildes in command.
complete -C '$PWD/command-not-in-path '
# CHECK: relative-path
HOME=$PWD complete -C '~/command-not-in-path '
# CHECK: relative-path

# Non-canonical command path
mkdir -p subdir
: >subdir/command-in-subdir
chmod +x subdir/command-in-subdir
complete -p "$PWD/subdir/command-in-subdir" -xa custom-completions
complete -C './subdir/../subdir/command-in-subdir '
# CHECK: custom-completions

# Relative $PATH
begin
    set -lx PATH subdir $PATH
    complete -C 'command-in-subdir '
    # CHECK: custom-completions
end

cd -
rm -r $dir

# Expand variables and tildes in command.
complete cat -xa +pet
set -l path_to_cat (command -v cat)
complete -C '$path_to_cat '
# CHECK: +pet
HOME=$path_to_cat/.. complete -C '~/cat '
# CHECK: +pet

# Do not expand command substitutions.
complete -C '(echo cat) ' | string match +pet
# Give up if we expand to multiple arguments (we'd need to handle the arguments).
complete -C '{cat,arg1,arg2} ' | string match +pet
# Don't expand wildcards though we could.
complete -C '$path_to_cat* ' | string match +pet

# Also expand wrap targets.
function crookshanks --wraps '$path_to_cat'
end
complete -C 'crookshanks '
# CHECK: +pet

# Custom completion works with variable overrides.
complete cmd_with_fancy_completion -xa '(commandline -xpc | count)'
complete -C"a=1 b=2 cmd_with_fancy_completion "
# CHECK: 1
complete -C"a=1 b=2 cmd_with_fancy_completion 1 "
# CHECK: 2

complete -c thing -x -F
# CHECKERR: complete: invalid option combination, '--exclusive' and '--force-files'
# Multiple conditions
complete -f -c shot
complete -fc shot -n 'test (count (commandline -xpc) -eq 1' -n 'test (commandline -xpc)[-1] = shot' -a 'through'
# CHECKERR: complete: -n 'test (count (commandline -xpc) -eq 1': Unexpected end of string, expecting ')'
# CHECKERR: test (count (commandline -xpc) -eq 1
# CHECKERR: ^
complete -fc shot -n 'test (count (commandline -xpc)) -eq 1' -n 'test (commandline -xpc)[-1] = shot' -a 'through'
complete -fc shot -n 'test (count (commandline -xpc)) -eq 2' -n 'test (commandline -xpc)[-1] = through' -a 'the'
complete -fc shot -n 'test (count (commandline -xpc)) -eq 3' -n 'test (commandline -xpc)[-1] = the' -a 'heart'
complete -fc shot -n 'test (count (commandline -xpc)) -eq 4' -n 'test (commandline -xpc)[-1] = heart' -a 'and'
complete -fc shot -n 'test (count (commandline -xpc)) -eq 5' -n 'test (commandline -xpc)[-1] = and' -a "you\'re"
complete -fc shot -n 'test (count (commandline -xpc)) -eq 6' -n 'test (commandline -xpc)[-1] = "you\'re"' -a 'to'
complete -fc shot -n 'test (count (commandline -xpc)) -eq 7' -n 'test (commandline -xpc)[-1] = to' -a 'blame'

complete -C"shot "
# CHECK: through
complete -C"shot through "
# CHECK: the

# See that conditions after a failing one aren't executed.
set -g oops 0
complete -fc oooops
complete -fc oooops -n true -n true -n true -n 'false' -n 'set -g oops 1' -a oops
complete -C'oooops '
echo $oops
# CHECK: 0

complete -fc oooops -n 'true' -n 'set -g oops 1' -a oops
complete -C'oooops '
# CHECK: oops
echo $oops
# CHECK: 1


# See that we load completions only if the command exists in $PATH,
# as a workaround for #3117.

# We need a completion script that runs the command,
# and prints something simple, and isn't already used above.
#
# Currently, tr fits the bill (it does `tr --version` to check for GNUisms)
begin
    $fish -c "complete -C'tr -'" | string match -- '-d*'
    # CHECK: -d{{\t.*}}
end

set -l tmpdir (mktemp -d)
cd $tmpdir
begin
    touch tr
    chmod +x tr
    set -l PATH
    complete -C'./tr -' | string match -- -d
    or echo No match for relative
    # CHECK: No match for relative
    complete -c tr | string length -q
    or echo Empty completions
    # CHECK: Empty completions
end

rm -$f $tmpdir/*

# Leading dots are not completed for default file completion,
# but may be for custom command (e.g. git add).
function dotty
end
function notty
end
complete -c dotty --no-files -a '(echo .a*)'
touch .abc .def
complete -C'notty '
echo "Should be nothing"
# CHECK: Should be nothing
complete -C'dotty '
# CHECK: .abc

rm -r $tmpdir

complete -C'complete --command=mktemp' | string replace -rf '=mktemp\t.*' '=mktemp'
# (one "--command=" is okay, we used to get "--command=--command="
# CHECK: --command=mktemp

## Test token expansion in commandline -x

complete complete_make -f -a '(argparse C/directory= -- (commandline -xpc)[2..];
                               echo Completing targets in directory $_flag_C)'
var=path/to complete -C'complete_make -C "$var/build-directory" '
# CHECK: Completing targets in directory path/to/build-directory
var1=path complete -C'var2=to complete_make -C "$var1/$var2/other-build-directory" '
# CHECK: Completing targets in directory path/to/other-build-directory

complete complete_existing_argument -f -a '(commandline -xpc)[2..]'
var=a_value complete -C'complete_existing_argument "1  2" $var \'quoted (foo bar)\' unquoted(baz qux) '
# CHECK: 1  2
# CHECK: a_value
# CHECK: quoted (foo bar)
# CHECK: unquoted(baz qux)

complete complete_first_argument_and_count -f -a '(set -l args (commandline -xpc)[2..]
                                        echo (count $args) arguments, first argument is $args[1])'
list=arg(seq 10) begin
    complete -C'complete_first_argument_and_count $list$list '
    # CHECK: 100 arguments, first argument is arg1arg1
    complete -C'complete_first_argument_and_count $list$list$list '
    # CHECK: 1 arguments, first argument is $list$list$list
end

## Test commandline --tokens-raw
complete complete_raw_tokens -f -ka '(commandline --tokens-raw)'
complete -C'complete_raw_tokens "foo" bar\\ baz (qux) '
# CHECK: complete_raw_tokens
# CHECK: "foo"
# CHECK: bar\ baz
# CHECK: (qux)

## Test deprecated commandline -o
complete complete_unescaped_tokens -f -ka '(commandline -o)'
complete -C'complete_unescaped_tokens "foo" bar\\ baz (qux) '
# CHECK: complete_unescaped_tokens
# CHECK: foo
# CHECK: bar baz
# CHECK: (qux)

## Fuzzy completion of options
complete complete_long_option -f -l some-long-option
complete -C'complete_long_option --slo'
# CHECK: --some-long-option
complete complete_long_option -f -o an-old-option
complete -C'complete_long_option -ao'
# CHECK: -an-old-option
# But only if the user typed a dash
complete -C'complete_long_option lo'

# Check that descriptions are correctly generated for commands.
# Override __fish_describe_command to prevent missing man pages or broken __fish_apropos on macOS
# from failing this test. (TODO: Test the latter separately.)
function __fish_describe_command
    echo -e "whereis\twhere is it"
    echo -e "whoami\twho am i"
    echo -e "which\which is it"
end
test (count (complete -C"wh" | string match -rv "\tcommand|^while")) -gt 0 && echo "found" || echo "fail"
# CHECK: found
