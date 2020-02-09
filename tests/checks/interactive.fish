#RUN: %fish -c 'if status --is-login ; echo login shell ; else ; echo not login shell ; end; if status --is-interactive ; echo interactive ; else ; echo not interactive ; end' -i
# CHECK: not login shell
# CHECK: interactive
