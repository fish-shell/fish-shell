#RUN: %fish %s
set -l max 9223372036854775807
set -l close_max 9223372036854775806
set -l min -9223372036854775807
set -l close_min -9223372036854775806
set -l diff_max 18446744073709551614

# check failure cases
random a
#CHECKERR: random: a: invalid integer
random $diff_max
#CHECKERR: random: 18446744073709551614: invalid integer
random -- 1 2 3 4
#CHECKERR: random: too many arguments
random -- 10 $diff_max
#CHECKERR: random: 18446744073709551614: invalid integer
random -- 1 1d
random -- 1 1c 10
#CHECKERR: random: 1d: invalid integer
#CHECKERR: random: 1c: invalid integer
random -- 1 - 10
#CHECKERR: random: -: invalid integer
random -- 1 -1 10
#CHECKERR: random: -1: invalid integer
random -- 1 $min 10
#CHECKERR: random: -9223372036854775807: invalid integer
random -- 1 0 10
#CHECKERR: random: STEP must be a positive integer
random choice
#CHECKERR: random: nothing to choose from
random choic a b c
#CHECKERR: random: too many arguments

function check_boundaries
    if not test "$argv[1]" -ge "$argv[2]" -a "$argv[1]" -le "$argv[3]"
        printf "Unexpected: %s <= %s <= %s not verified\n" $argv[2] $argv[1] $argv[3] >&2
        status print-stack-trace
        return 1
    end
end

function test_range
    return (check_boundaries (random -- $argv) $argv)
end

function check_contains
    if not contains -- $argv[1] $argv[2..-1]
        printf "Unexpected: %s not among possibilities" $argv[1] >&2
        printf " %s" $argv[2..-1] >&2
        printf "\n" >&2
        status print-stack-trace
        return 1
    end
end

function test_step
    return (check_contains (random -- $argv) (seq -- $argv))
end

function test_choice
    return (check_contains (random choice $argv) $argv)
end

for i in (seq 10)
    check_boundaries (random) 0 32767

    test_range 0 10
    test_range -10 -1
    test_range -10 10
    test_range 5 5

    test_range 0 $max
    test_range $min -1
    test_range $min $max

    test_range $close_max $max
    test_range $min $close_min
    test_range $close_min $close_max

    #OSX's `seq` uses scientific notation for large numbers, hence not usable here
    check_contains (random -- 0 $max $max) 0 $max
    check_contains (random -- 0 $close_max $max) 0 $close_max
    check_contains (random -- $min $max 0) $min 0
    check_contains (random -- $min $close_max 0) $min -1
    check_contains (random -- $min $max $max) $min 0 $max
    check_contains (random -- $min $diff_max $max) $min $max
    check_contains (random -- 5 0) 0 1 2 3 4 5

    test_step 0 $i 10
    test_step -5 $i 5
    test_step -10 $i 0

    test_choice a
    test_choice foo bar
    test_choice bass trout salmon zander perch carp
end


#check seeding
set -l seed (random)
random $seed
set -l run1 (random) (random) (random) (random) (random)
random $seed
set -l run2 (random) (random) (random) (random) (random)
if not test "$run1" = "$run2"
    printf "Unexpected different sequences after seeding with %s\n" $seed
    printf "%s " $run1
    printf "\n"
    printf "%s " $run2
    printf "\n"
end
