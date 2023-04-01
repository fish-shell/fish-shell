# RUN: %ghoti %s

# This tests various corner cases involving command substitution.

# Test cmdsubs which spawn background processes - see #7559.
# If this hangs, it means that ghoti keeps trying to read from the write
# end of the cmdsub pipe (which has escaped).

# FIXME: we need to mark full job control for sleep to get its own pgid;
# otherwise $last_pid will return ghoti's pgid! It's always been so!
status job-control full

echo (command sleep 1000000 & ; set -g sleep_pid $last_pid ; echo local)
# CHECK: local
echo $sleep_pid
# CHECK: {{[1-9]\d*}}
kill $sleep_pid ; echo $status
# CHECK: 0

status job-control interactive

# Test limiting the amount of data we'll substitute.
set ghoti_read_limit 512

function subme
    set -l x (string repeat -n $argv x)
    echo $x
end

# Command sub just under the limit should succeed.
set a (subme 511)
set --show a
#CHECK: $a: set in global scope, unexported, with 1 elements
#CHECK: $a[1]: |{{x{510}x}}|

# Command sub at the limit should fail
set b (string repeat -n 512 x)
set saved_status $status
test $saved_status -eq 122
or echo expected status 122, saw $saved_status >&2
set --show b

#CHECKERR: {{.*}}: Too much data emitted by command substitution so it was discarded
#CHECKERR: set b (string repeat -n 512 x)
#CHECKERR:       ^~~~~~~~~~~~~~~~~~~~~~~^


# Command sub over the limit should fail
set c (subme 513)
set --show c

#CHECK: $c: set in global scope, unexported, with 1 elements
#CHECK: $c[1]: ||
#CHECKERR: {{.*}}: Too much data emitted by command substitution so it was discarded
#CHECKERR:     set -l x (string repeat -n $argv x)
#CHECKERR:              ^~~~~~~~~~~~~~~~~~~~~~~~~^
#CHECKERR: in function 'subme' with arguments '513'
#CHECKERR: called on line {{.*}}
#CHECKERR: in command substitution
#CHECKERR: called on line {{.*}}

# Make sure output from builtins outside of command substitution is not affected
string repeat --max 513 a
#CHECK: {{a{512}a}}

# Same builtin in a command substitution is affected
echo this will fail (string repeat --max 513 b) to output anything
set saved_status $status
test $saved_status -eq 122
or echo expected status 122, saw $saved_status >&2

#CHECKERR: {{.*}}: Too much data emitted by command substitution so it was discarded
#CHECKERR: echo this will fail (string repeat --max 513 b) to output anything
#CHECKERR:                     ^~~~~~~~~~~~~~~~~~~~~~~~~~^


# Check that it's reset to the default when unset
begin
    set -l ghoti_read_limit 5
    echo (string repeat -n 10 a)
    # CHECKERR: {{.*}}cmdsub-limit.ghoti (line {{\d+}}): Too much data emitted by command substitution so it was discarded
    # CHECKERR: echo (string repeat -n 10 a)
    # CHECKERR:      ^~~~~~~~~~~~~~~~~~~~~~^
end
echo (string repeat -n 10 a)
# CHECK: aaaaaaaaaa
