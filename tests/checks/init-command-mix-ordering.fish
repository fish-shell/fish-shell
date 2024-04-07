#RUN: %fish -c 'echo command' -C 'echo init-command' | %filter-ctrlseqs
# CHECK: init-command
# CHECK: command
