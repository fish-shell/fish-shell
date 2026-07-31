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

set -l list 1 2 3
set list[..2] $list[2..1]
echo $list # CHECK: 2 1 3

set -l list 1 2 3
set list[2..] $list[-1..2]
echo $list # CHECK: 1 3 2

# nested slices
set -l foo (seq 2 200)
echo $foo[$foo[$foo[$foo[1 5]]]]
#CHECK: 5 9
echo $foo[1 (echo $foo[2 3]) 4]
#CHECK: 2 4 5 5
echo $foo[1(echo $foo[2 3])4]
#CHECK: 2 4 5 5
echo $foo[1 $foo[2 3] 4]
#CHECK: 2 4 5 2 5 5
echo $foo[1$foo[2 3]4]
#CHECK: 2 35 2 45

# escaped slice operator
set foo abc def ghi
echo $foo\1331\135
#CHECK: abc[1] def[1] ghi[1]
echo $foo[1]
#CHECK: abc
set foo\1331\135 bar
echo $foo\1331\135
#CHECK: bar[1] def[1] ghi[1]
echo $foo[1]
#CHECK: bar

# embded `]`
set foo abc def
set bar "1]2"
echo $foo[$bar]
#CHECKERR: {{.*}}/slices.fish (line {{\d+}}): Invalid index value
#CHECKERR: echo $foo[$bar]
#CHECKERR:             ^
