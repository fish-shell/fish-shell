#RUN: %fish -c 'set foo bar' -c 'echo $foo' | %filter-ctrlseqs
# CHECK: bar
