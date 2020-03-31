# RUN: %fish -C 'set -l fish %fish' %s
if not set -q GITHUB_WORKFLOW
    $fish -c 'if status --is-login ; echo login shell ; else ; echo not login shell ; end; if status --is-interactive ; echo interactive ; else ; echo not interactive ; end' -l -i
else
    echo login shell
    echo interactive
end
# CHECK: login shell
# CHECK: interactive
