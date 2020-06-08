#RUN: %fish %s
set n 10
set test (seq $n)
echo $test[1..$n] # normal range
#CHECK: 1 2 3 4 5 6 7 8 9 10
echo $test[1 .. 2] # spaces are allowed
#CHECK: 1 2
echo $test[$n..1] # inverted range
#CHECK: 10 9 8 7 6 5 4 3 2 1
echo $test[2..5 8..6] # several ranges
#CHECK: 2 3 4 5 8 7 6
echo $test[-1..-2] # range with negative limits
#CHECK: 10 9
echo $test[-1..1] # range with mixed limits
#CHECK: 10 9 8 7 6 5 4 3 2 1

set test1 $test
set test1[-1..1] $test
echo $test1
#CHECK: 10 9 8 7 6 5 4 3 2 1
set test1[1..$n] $test
echo $test1
#CHECK: 1 2 3 4 5 6 7 8 9 10
set test1[$n..1] $test
echo $test1
#CHECK: 10 9 8 7 6 5 4 3 2 1
set test1[2..4 -2..-4] $test1[4..2 -4..-2]
echo $test1
#CHECK: 10 7 8 9 6 5 2 3 4 1

echo (seq 5)[-1..1]
#CHECK: 5 4 3 2 1
echo (seq $n)[3..5 -2..2]
#CHECK: 3 4 5 9 8 7 6 5 4 3 2

echo $test[(count $test)..1]
#CHECK: 10 9 8 7 6 5 4 3 2 1
echo $test[1..(count $test)]
#CHECK: 1 2 3 4 5 6 7 8 9 10

echo $test[ .. ]
#CHECK: 1 2 3 4 5 6 7 8 9 10
echo $test[ ..3]
#CHECK: 1 2 3
echo $test[8.. ]
#CHECK: 8 9 10
echo $test[..2 5]
# CHECK: 1 2 5
echo $test[2 9..]
# CHECK: 2 9 10

# missing start, cannot use implied range
echo $test[1..2..]
#CHECKERR: {{.*}}: Invalid index value
#CHECKERR: echo $test[1..2..]
#CHECKERR:                ^
echo $test[..1..2]
#CHECKERR: {{.*}}: Invalid index value
#CHECKERR: echo $test[..1..2]
#CHECKERR:               ^

set -l empty
echo $test[ $empty..]
#CHECK:
echo $test[.."$empty"]
#CHECK: 1 2 3 4 5 6 7 8 9 10
echo $test["$empty"..]
#CHECK: 1 2 3 4 5 6 7 8 9 10
echo $test[ (true)..3]
#CHECK:
echo $test[ (string join \n 1 2 3)..3 ]
#CHECK: 1 2 3 2 3 3
