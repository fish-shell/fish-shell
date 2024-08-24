#RUN: %fish %s

complete -e cat

complete -C"cat -" | wc -l
# CHECK: 0
