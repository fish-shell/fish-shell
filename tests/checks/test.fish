# RUN: %fish %s
#
# Tests for the `test` builtin, aka `[`.
test inf -gt 0
# CHECKERR: Number is infinite
# CHECKERR: {{.*}}test.fish (line {{\d+}}):
# CHECKERR: test inf -gt 0
# CHECKERR: ^

test 5 -eq nan
# CHECKERR: Not a number
# CHECKERR: {{.*}}test.fish (line {{\d+}}):
# CHECKERR: test 5 -eq nan
# CHECKERR: ^

test -z nan || echo nan is fine
# CHECK: nan is fine
