#RUN: %fish %s
# "Basic && and || support"

echo first && echo second
echo third || echo fourth
true && false
echo "true && false: $status"
true || false
echo "true || false: $status"
true && false || true
echo "true && false || true: $status"
# CHECK: first
# CHECK: second
# CHECK: third
# CHECK: true && false: 1
# CHECK: true || false: 0
# CHECK: true && false || true: 0

# "&& and || in if statements"

if true || false
    echo "if test 1 ok"
end
if true && false
else
    echo "if test 2 ok"
end
if true && false; or true
    echo "if test 3 ok"
end
if [ 0 = 1 ] || [ 5 -ge 3 ]
    echo "if test 4 ok"
end
# CHECK: if test 1 ok
# CHECK: if test 2 ok
# CHECK: if test 3 ok
# CHECK: if test 4 ok

# "&& and || in while statements"

set alpha 0
set beta 0
set gamma 0
set delta 0
while [ $alpha -lt 2 ] && [ $beta -lt 3 ]
    or [ $gamma -lt 4 ] || [ $delta -lt 5 ]
    echo $alpha $beta $gamma
    set alpha ( math $alpha + 1 )
    set beta ( math $beta + 1 )
    set gamma ( math $gamma + 1 )
    set delta ( math $delta + 1 )
end
# CHECK: 0 0 0
# CHECK: 1 1 1
# CHECK: 2 2 2
# CHECK: 3 3 3
# CHECK: 4 4 4

# "Nots"

true && ! false
echo $status
not true && ! false
echo $status
not not not true
echo $status
not ! ! not true
echo $status
not ! echo not !
echo $status
# CHECK: 0
# CHECK: 1
# CHECK: 1
# CHECK: 0
# CHECK: not !
# CHECK: 0

# "Complex scenarios"

begin
    echo 1
    false
end || begin
    echo 2 && echo 3
end

if false && true
    or not false
    echo 4
end
# CHECK: 1
# CHECK: 2
# CHECK: 3
# CHECK: 4

# Newlines
true &&
echo newline after conjunction
# CHECK: newline after conjunction
false ||
echo newline after disjunction
# CHECK: newline after disjunction

true &&

echo empty lines after conjunction
# CHECK: empty lines after conjunction

true &&
# can have comments here!
echo comment after conjunction
# CHECK: comment after conjunction

# --help works
builtin and --help >/dev/null
# CHECKERR: Documentation for and
echo $status
and --help >/dev/null
# CHECKERR: Documentation for and
echo $status
# CHECK: 0
# CHECK: 0

builtin or --help >/dev/null
# CHECKERR: Documentation for or
echo $status
or --help >/dev/null
# CHECKERR: Documentation for or
echo $status
# CHECK: 0
# CHECK: 0

builtin not --help >/dev/null
# CHECKERR: Documentation for not
echo $status
not --help >/dev/null
# CHECKERR: Documentation for not
echo $status
# CHECK: 0
# CHECK: 0
