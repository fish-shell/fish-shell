#RUN: %fish -c '%fish -c false; echo RC: $status' | %filter-ctrlseqs
# CHECK: RC: 1
