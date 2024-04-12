#RUN: %fish %s
# Validate some things about command wrapping.

set -g LANG C # For predictable error messages.

# This tests that we do not trigger a combinatorial explosion - see #5638.
# Ensure it completes successfully.
complete -c testcommand --wraps "testcommand x "
complete -c testcommand --wraps "testcommand y "
complete -c testcommand --no-files -a normal
complete -C'testcommand '
# CHECK: normal

# Test double wraps.
complete -c testcommand0 -x -a crosswalk
complete -c testcommand1 -x --wraps testcommand0
complete -c testcommand2 -x --wraps testcommand1
complete -C 'testcommand 0'
# CHECK: crosswalk

# This tests that a call to complete from within a completion doesn't trigger
# wrap chain explosion - #5638 again.
function testcommand2_complete
    set -l tokens (commandline -xpc) (commandline -ct)
    set -e tokens[1]
    echo $tokens 1>&2
end

complete -c testcommand2 -x -a "(testcommand2_complete)"
complete -c testcommand2 --wraps "testcommand2 from_wraps "
complete -C'testcommand2 explicit '
# CHECKERR: explicit
# CHECKERR: from_wraps explicit

# Test that prefixing with a variable assignment works - see #7344.
complete -c recvar --exclusive -a recvar_comp
complete -c recvar --wraps 'A=B recvar'
complete -C 'recvar '
# CHECK: recvar_comp

# Test that completions do not perform subcommands.
# That is, `FOO=(launch_missiles) command<tab>` does not launch any missiles.
set -g missile_count 0
function launch_missiles
    set -g missile_count (math "$missile_count + 1")
end

# Ensure missile launching work.
launch_missiles
echo $missile_count
# CHECK: 1

set -g GOOD global_good
set -g BAD global_bad

function do_print_good_bad
    echo "GOOD:$GOOD"
    echo "BAD:$BAD"
end
complete -c print_good_bad -x -a '(do_print_good_bad)'
complete -C 'print_good_bad '
# CHECK: BAD:global_bad
# CHECK: GOOD:global_good

# Key check is completions should expand GOOD but not BAD,
# because GOOD is just a string but BAD contains a cmdsub
# which may do arbitrary things.
complete -C 'GOOD=local_good BAD=(launch_missiles) print_good_bad '
# CHECK: BAD:
# CHECK: GOOD:local_good

# Completion should not have launched any missiles.
echo $missile_count
# CHECK: 1

# Torture test with a bunch of variable assignments.
# Note this tests the existing behavior when there's conflicts
# (innermost wrapped command wins) but it's not clear that this is desirable.
function show_vars
    echo "AAA:$AAA"
    echo "BBB:$BBB"
    echo "CCC:$CCC"
end
complete -c show_vars_cmd0 -x -a '(show_vars)'
complete -c show_vars_cmd1 -x --wraps 'AAA=aaa show_vars_cmd0'
complete -c show_vars_cmd2 -x --wraps 'AAA=nope BBB=bbb show_vars_cmd1'
complete -c show_vars_cmd3 -x --wraps 'BBB=nope show_vars_cmd2'
complete -C 'CCC=ccc show_vars_cmd3 '
# CHECK: AAA:aaa
# CHECK: BBB:bbb
# CHECK: CCC:ccc
