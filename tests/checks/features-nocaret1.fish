#RUN: %fish --features 'no-stderr-nocaret' -c 'status test-feature stderr-nocaret; echo nocaret: $status' | %filter-ctrlseqs
# CHECK: nocaret: 0
