#RUN: %ghoti -c 'echo command' -C 'echo init-command'
# CHECK: init-command
# CHECK: command
