#RUN: %fish -c "echo 1.2.3.4." -c "echo 5.6.7.8." | %filter-ctrlseqs
# CHECK: 1.2.3.4.
# CHECK: 5.6.7.8.
