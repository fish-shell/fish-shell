#RUN: %fish --features 'no-stderr-nocaret' -c 'echo -n careton:; echo ^/dev/null' | %filter-ctrlseqs
# CHECK: careton:^/dev/null
