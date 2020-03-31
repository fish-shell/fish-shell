# RUN: %fish -C 'set -l fish %fish' %s
if not set -q GITHUB_WORKFLOW
    $fish -c 'if status --is-login ; echo login shell ; else ; echo not login shell ; end; if status --is-interactive ; echo interactive ; else ; echo not interactive ; end' -i
else
    # FAKE
    echo not login shell
    echo interactive
end
# CHECK: not login shell
# CHECK: interactive
