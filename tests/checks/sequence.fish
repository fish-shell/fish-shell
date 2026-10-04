#RUN: %fish %s
# Test the behaviour of the `sequence` builtin.

# `sequence N` is the same as `sequence --to N`.
sequence 5
# CHECK: 1
# CHECK: 2
# CHECK: 3
# CHECK: 4
# CHECK: 5
# CHECK:

sequence --to 5
# CHECK: 1
# CHECK: 2
# CHECK: 3
# CHECK: 4
# CHECK: 5
# CHECK:

# Negatives count down from -1.
sequence --to -5
# CHECK: -1
# CHECK: -2
# CHECK: -3
# CHECK: -4
# CHECK: -5
# CHECK:

# No output.
sequence 0

sequence --from 2 --to 4
# CHECK: 2
# CHECK: 3
# CHECK: 4
# CHECK:

# Infer negative step if to < from.
sequence --from 8 --to 5
# CHECK: 8
# CHECK: 7
# CHECK: 6
# CHECK: 5
# CHECK:

# Infer negative step if to < from.
sequence --from -1 --to -3
# CHECK: -1
# CHECK: -2
# CHECK: -3
# CHECK:

sequence --from -10 --to -7
# CHECK: -10
# CHECK: -9
# CHECK: -8
# CHECK: -7
# CHECK:

# Sequence that crosses 0.
sequence --from -2 --to 2
# CHECK: -2
# CHECK: -1
# CHECK: 0
# CHECK: 1
# CHECK: 2
# CHECK:

# Only one number printed if from = to.
sequence --from 5 --to 5
# CHECK: 5
# CHECK:

sequence --step 2
# CHECKERR: sequence: --to is required

sequence --to 5 --step 2
# CHECKERR: sequence: --to and --from must be specified if --step is used

sequence --from 5 --step 2
# CHECKERR: sequence: --to is required

sequence --from 3 --to 7 --step 2
# CHECK: 3
# CHECK: 5
# CHECK: 7
# CHECK:

sequence --from 10 --to 17 --step 3
# CHECK: 10
# CHECK: 13
# CHECK: 16
# CHECK:

# step values that count in the opposite direction
# of from to to are not allowed.

sequence --from 0 --to 10 --step -1
# CHECKERR: sequence: -1: step argument counts in opposite direction

sequence --from 10 --to 0 --step 1
# CHECKERR: sequence: 1: step argument counts in opposite direction

# step value of 0 is not allowed if from != to.
sequence --from 10 --to 12 --step 0
# CHECKERR: sequence: --step argument must not be 0

# if from = to, any step value will do and,
# and only the one value will be printed.

sequence --from 3 --to 3 --step 0
# CHECK: 3
# CHECK:

sequence --from 5 --to 5 --step 5
# CHECK: 5
# CHECK:
