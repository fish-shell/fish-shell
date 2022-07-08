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
# CHECKERR: Argument is not a number: 'foo'
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

test -x /usr/bin/go /usr/local/bin/go
# CHECKERR: test: unexpected argument at index 3: '/usr/local/bin/go'
# CHECKERR: -x /usr/bin/go /usr/local/bin/go
# CHECKERR: {{               \^}}
# CHECKERR: {{.*}}test.fish (line {{\d+}}):
# CHECKERR: test -x /usr/bin/go /usr/local/bin/go
# CHECKERR: ^

touch -m -t 197001010000.00 epoch
touch -m -t 190212112045.40 old
touch -m -t 190212112045.39 oldest
touch -m -t 203801080314.07 newest
test oldest -ot old || echo bad ot
test newest -nt old || echo bad nt
test old -ot oldest && echo bad ot
test epoch -nt newest && echo bad nt

for file in epoch old oldest newest
    test $file -nt nonexist && echo good nt || echo bad nt;
end
#CHECK: good nt
#CHECK: good nt
#CHECK: good nt
#CHECK: good nt

for file in epoch old oldest newest
    test nonexist -ot $file && echo good ot || echo bad ot;
end
#CHECK: good ot
#CHECK: good ot
#CHECK: good ot
#CHECK: good ot

ln -sf epoch epochlink
test epoch -ef epochlink && echo good ef || echo bad ef
#CHECK: good ef

test epoch -ef old && echo bad ef || echo good ef
#CHECK: good ef

rm -f epoch old oldest newest epochlink
