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

test 1 =
# CHECKERR: test: Missing argument at index 3
# CHECKERR: 1 =
# CHECKERR: ^
# CHECKERR: {{.*}}test.fish (line {{\d+}}):
# CHECKERR: test 1 =
# CHECKERR: ^

test 1 = 2 and echo true or echo false
# CHECKERR: test: Expected a combining operator like '-a' at index 4
# CHECKERR: 1 = 2 and echo true or echo false
# CHECKERR: ^
# CHECKERR: {{.*}}test.fish (line {{\d+}}):
# CHECKERR: test 1 = 2 and echo true or echo false
# CHECKERR: ^

function t
    test $argv[1] -eq 5
end

t foo
# CHECKERR: Invalid argument: 'foo'
# CHECKERR: {{.*}}test.fish (line {{\d+}}):
# CHECKERR: test $argv[1] -eq 5
# CHECKERR: ^
# CHECKERR: in function 't' with arguments 'foo'
# CHECKERR: called on line {{\d+}} of file {{.*}}test.fish

t 5,2
# CHECKERR: Integer 5 in '5,2' followed by non-digit
# CHECKERR: {{.*}}test.fish (line {{\d+}}):
# CHECKERR: test $argv[1] -eq 5
# CHECKERR: ^
# CHECKERR: in function 't' with arguments '5,2'
# CHECKERR: called on line {{\d+}} of file {{.*}}test.fish

