# RUN: %fish %s
function complete_test_alpha1; echo $argv; end

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
function t --wraps t; echo t; end
complete -c t -fa '(t)'
complete -C't '
# CHECK: t

# Ensure file completion happens even though it was disabled above.
complete -c t -l fileoption -rF
# Only match one file because I don't want to touch this any time we add a test file.
complete -C't --fileoption ' | string match bind.expect
# CHECK: bind.expect
