#RUN: %fish -C 'set -l fish %fish' %s

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
