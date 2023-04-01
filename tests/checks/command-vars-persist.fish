#RUN: %ghoti -c 'set foo bar' -c 'echo $foo'
# CHECK: bar
