#RUN: %fish --features '   stderr-nocaret' -c 'echo -n "caretoff: "; echo ^/dev/null' | %filter-ctrlseqs
# CHECK: caretoff: ^/dev/null
