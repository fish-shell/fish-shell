#RUN: %fish -C 'echo init-command' -c 'echo command' | %filter-ctrlseqs
# CHECK: init-command
# CHECK: command
