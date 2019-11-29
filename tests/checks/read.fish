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
