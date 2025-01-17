#RUN: fish=%fish %fish %s

$fish -c "echo 1.2.3.4."
# CHECK: 1.2.3.4.

PATH= $fish -c "command a" 2>/dev/null
echo $status
# CHECK: 127

PATH= $fish -c "echo (command a)" 2>/dev/null
echo $status
# CHECK: 127

if not set -q GITHUB_WORKFLOW
    $fish -c 'if status --is-login ; echo login shell ; else ; echo not login shell ; end; if status --is-interactive ; echo interactive ; else ; echo not interactive ; end' -i
    # CHECK: not login shell
    # CHECK: interactive
    $fish -c 'if status --is-login ; echo login shell ; else ; echo not login shell ; end; if status --is-interactive ; echo interactive ; else ; echo not interactive ; end' -l -i
    # CHECK: login shell
    # CHECK: interactive
else
    # GitHub Action doesn't start this in a terminal, so fish would complain.
    # Instead, we just fake the result, since we have no way to indicate a skipped test.
    echo not login shell
    echo interactive
    echo login shell
    echo interactive
end

$fish -c 'if status --is-login ; echo login shell ; else ; echo not login shell ; end; if status --is-interactive ; echo interactive ; else ; echo not interactive ; end' -l
# CHECK: login shell
# CHECK: not interactive

$fish -c 'if status --is-login ; echo login shell ; else ; echo not login shell ; end; if status --is-interactive ; echo interactive ; else ; echo not interactive ; end'
# CHECK: not login shell
# CHECK: not interactive

$fish -c 'if status --is-login ; echo login shell ; else ; echo not login shell ; end; if status --is-interactive ; echo interactive ; else ; echo not interactive ; end' -l
# CHECK: login shell
# CHECK: not interactive

# Arguments for -c
$fish -c 'string escape $argv' 1 2 3
# CHECK: 1
# CHECK: 2
# CHECK: 3

# Two -cs
$fish -c 'string escape y$argv' -c 'string escape x$argv' 1 2 3
# CHECK: y1
# CHECK: y2
# CHECK: y3
# CHECK: x1
# CHECK: x2
# CHECK: x3

# Should just do nothing.
$fish --no-execute

set -l tmp (mktemp -d)
$fish --profile $tmp/normal.prof --profile-startup $tmp/startup.prof -ic exit

# This should be the full file - just the one command we gave explicitly!
cat $tmp/normal.prof
# CHECK: Time{{\s+}}Sum{{\s+}}Command
# CHECK: {{\d+\s+\d+\s+>}} exit

string match -rq "builtin source " < $tmp/startup.prof
and echo matched
# CHECK: matched

# See that sending both profiles to the same file works.
$fish --profile $tmp/full.prof --profile-startup $tmp/full.prof -c 'echo thisshouldneverbeintheconfig'
# CHECK: thisshouldneverbeintheconfig
string match -rq "builtin source " < $tmp/full.prof
and echo matched
# CHECK: matched
string match -rq "echo thisshouldneverbeintheconfig" < $tmp/full.prof
and echo matched
# CHECK: matched

# See that profiling without startup actually gives us just the command
$fish --no-config --profile $tmp/nostartup.prof -c 'echo foo'
# CHECK: foo
count < $tmp/nostartup.prof
# CHECK: 2

$fish --no-config -c 'echo notprinted; echo foo | exec true; echo banana'
# CHECKERR: fish: The 'exec' command can not be used in a pipeline
# CHECKERR: echo notprinted; echo foo | exec true; echo banana
# CHECKERR:                             ^~~~~~~~^

# Running multiple command lists continues even if one has a syntax error.
$fish --no-config -c 'echo $$ oh no syntax error' -c 'echo this works'
# CHECK: this works
# CHECKERR: fish: $$ is not the pid. In fish, please use $fish_pid.
# CHECKERR: echo $$ oh no syntax error
# CHECKERR: ^

$fish --no-config .
# CHECKERR: error: Unable to read input file: Is a directory
# CHECKERR: warning: Error while reading file .

$fish --no-config -c 'echo notprinted; echo foo; a=b'
# CHECKERR: fish: Unsupported use of '='. In fish, please use 'set a b'.
# CHECKERR: echo notprinted; echo foo; a=b
# CHECKERR:                            ^~^

$fish --no-config -c 'echo notprinted | and true'
# CHECKERR: fish: The 'and' command can not be used in a pipeline
# CHECKERR: echo notprinted | and true
# CHECKERR:                   ^~^

$fish --no-config --features
# CHECKERR: fish: --features: option requires an argument

# Regression test for a hang.
echo "set -L" | $fish > /dev/null
