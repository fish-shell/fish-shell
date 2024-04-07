#RUN: %fish %s | %filter-ctrlseqs

not true
echo $status
# CHECK: 1

# Two "not" prefixes on a single job cancel each other out.
not not sh -c 'exit 34'
echo $status
# CHECK: 34
