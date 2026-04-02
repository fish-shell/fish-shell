# RUN: fish=%fish %fish %s

breakpoint foo
# CHECKERR: breakpoint: expected 0 arguments; got 1

# no breakpoint in non-interactive shell
breakpoint
echo $status
# CHECK: 1
