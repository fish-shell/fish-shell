#RUN: %fish -C 'echo init-command' -C 'echo 2nd init-command' | %filter-ctrlseqs
# CHECK: init-command
# CHECK: 2nd init-command
