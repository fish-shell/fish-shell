# RUN: env fth=%fish_test_helper fish=%fish %fish %s
#REQUIRES: command -v %fish_test_helper

status job-control full

# Ensure that lots of nested jobs all end up in the same pgroup.

function save_pgroup -a var_name
    $fth print_pgrp | read -g $var_name
end

# Here everything should live in the pgroup of the first fish_test_helper.
$fth print_pgrp | read -g global_group | save_pgroup g1 | begin
    save_pgroup g2
end | begin
    echo (save_pgroup g3) >/dev/null
end

[ "$global_group" -eq "$g1" ] && [ "$g1" -eq "$g2" ] && [ "$g2" -eq "$g3" ]
and echo "All pgroups agreed"
or echo "Pgroups disagreed. Should be in $global_group but found $g1, $g2, $g3"
# CHECK: All pgroups agreed

# Here everything should live in fish's pgroup.
# Unfortunately we don't know what fish's pgroup is (it may not be fish's pid).
# So run it twice and verify that everything agrees; this implies that it could
# not have used any of the pids of the child procs.
function nothing
end
nothing | $fth print_pgrp | read -g a0 | save_pgroup a1 | begin
    save_pgroup a2
end
nothing | $fth print_pgrp | read -g b0 | save_pgroup b1 | begin
    save_pgroup b2
end

[ "$a0" -eq "$a1" ] && [ "$a1" -eq "$a2" ] \
    && [ "$b0" -eq "$b1" ] && [ "$b1" -eq "$b2" ] \
    && [ "$a0" -eq "$b0" ]
and echo "All pgroups agreed"
or echo "Pgroups disagreed. Found $a0 $a1 $a2, and $b0 $b1 $b2"
# CHECK: All pgroups agreed

# Ensure that eval retains pgroups - #6806.
set -l tmpfile1 (mktemp)
set -l tmpfile2 (mktemp)
$fth print_pgrp > $tmpfile1 | eval '$fth print_pgrp > $tmpfile2'
read -l pgrp1 < $tmpfile1
read -l pgrp2 < $tmpfile2
[ "$pgrp1" -eq "$pgrp2" ]
and echo "eval pgroups agreed"
or echo "eval pgroups disagreed, meaning eval does not retain pgroups: $pgrp1 $pgrp2"
# CHECK: eval pgroups agreed
echo -n > $tmpfile1
echo -n > $tmpfile2

# Ensure that if a background job launches another background job, that they have different pgroups.
# Our regex will capture the first pgroup and use a negative lookahead on the second.
status job-control full
$fth print_pgrp > $tmpfile1 | begin
    $fth print_pgrp > $tmpfile2 &
    wait
end &
wait
read -l pgrp1 < $tmpfile1
read -l pgrp2 < $tmpfile2
[ "$pgrp1" -ne "$pgrp2" ]
and echo "background job correctly got new pgroup"
or echo "background job did not get new pgroup: $pgrp1 $pgrp2"
# CHECK: background job correctly got new pgroup
rm $tmpfile1 $tmpfile2
