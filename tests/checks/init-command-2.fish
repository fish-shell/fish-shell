#RUN: %ghoti -C 'echo init-command' -C 'echo 2nd init-command'
# CHECK: init-command
# CHECK: 2nd init-command
