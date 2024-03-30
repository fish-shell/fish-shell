#RUN: %fish -c "echo 1.2.3.4." | %filter-ctrlseqs
# CHECK: 1.2.3.4.
