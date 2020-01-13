# RUN: %fish %s
echo 'a | b' | read -lt a b c

set -l
# CHECK: a a
# CHECK: b '|'
# CHECK: c b

echo 'a"foo bar"b' | read -lt a b c

set -l
# CHECK: a 'afoo barb'
# CHECK: b
# CHECK: c

echo "a  b b" | read a b
string escape $a $b
# CHECK: a
# CHECK: 'b b'

echo 'a<><>b<>b' | read -d '<>' a b
printf %s\n $a $b
# CHECK: a
# CHECK: <>b<>b
