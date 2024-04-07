#RUN: %fish --features=remove-percent-self %s | %filter-ctrlseqs

echo %self
# CHECK: %self
