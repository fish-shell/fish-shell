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
complete -C't --fileoption ' | string match bind.expect
# CHECK: bind.expect

# Make sure bare `complete` is reasonable,
complete -p '/complete test/beta1' -d 'desc, desc' -sZ
complete -c 'complete test beta2' -r -d 'desc \' desc2 [' -a 'foo bar'
complete -c 'complete_test_beta2' -x -n 'false' -A -o test
complete

# CHECK: complete --no-files -c complete_test_alpha1 -a '(commandline)'
# CHECK: complete --no-files -c complete_test_alpha2
# CHECK: complete --no-files -c complete_test_alpha3
# CHECK: complete --force-files -c t -l fileoption
# CHECK: complete --no-files -c t -a '(t)'
# CHECK: complete -p '/complete test/beta1' -s Z -d 'desc, desc'
# CHECK: complete --requires-param -c 'complete test beta2' -d desc\ \'\ desc2\ \[ -a 'foo bar'
# CHECK: complete --exclusive -c complete_test_beta2 -o test -n false
# CHECK: complete {{.*}}
# CHECK: complete {{.*}}
# CHECK: complete {{.*}}
# CHECK: complete {{.*}}
# CHECK: complete {{.*}}

# Recursive calls to complete (see #3474)
complete -c complete_test_recurse1 -xa '(echo recursing 1>&2; complete -C"complete_test_recurse1 ")'
complete -C'complete_test_recurse1 '
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
# CHECKERR: complete: maximum recursion depth reached

# short options
complete -c foo -f -a non-option-argument
complete -c foo -f --short-option x
complete -c foo -f --short-option y -a 'ARGY'
complete -c foo -f --short-option z -a 'ARGZ' -r
complete -c foo -f --old-option single-long-ending-in-z
complete -c foo -f --old-option x-single-long
complete -c foo -f --old-option y-single-long
complete -c foo -f --old-option z-single-long
complete -c foo -f --long-option x-long -a 'ARGLONG'
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
complete -c complete_test_subcommand -n 'test (commandline -op)[1] = complete_test_subcommand' -xa 'ok'
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
