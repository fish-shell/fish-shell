# RUN: %fish %s

# This tests various corner cases involving command substitution. Most
# importantly the limits on the amount of data we'll substitute.

set fish_read_limit 512

function subme
    set -l x (string repeat -n $argv x)
    echo $x
end

# Command sub just under the limit should succeed
set a (subme 511)
set --show a
#CHECK: $a: not set in local scope
#CHECK: $a: set in global scope, unexported, with 1 elements
#CHECK: $a[1]: length=511 value=|{{x{510}x}}|
#CHECK: $a: not set in universal scope

# Command sub at the limit should fail
set b (string repeat -n 512 x)
set saved_status $status
test $saved_status -eq 122
or echo expected status 122, saw $saved_status >&2
set --show b

#CHECK: $b: not set in local scope
#CHECK: $b: not set in global scope
#CHECK: $b: not set in universal scope
#CHECKERR: {{.*}}: Too much data emitted by command substitution so it was discarded
#CHECKERR: 
#CHECKERR: set b (string repeat -n 512 x)
#CHECKERR:       ^


# Command sub over the limit should fail
set c (subme 513)
set --show c

#CHECK: $c: not set in local scope
#CHECK: $c: set in global scope, unexported, with 1 elements
#CHECK: $c[1]: length=0 value=||
#CHECK: $c: not set in universal scope
#CHECKERR: {{.*}}: Too much data emitted by command substitution so it was discarded
#CHECKERR: 
#CHECKERR:     set -l x (string repeat -n $argv x)
#CHECKERR:              ^
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
#CHECKERR: 
#CHECKERR: echo this will fail (string repeat --max 513 b) to output anything
#CHECKERR:                     ^
