#RUN: %fish %s
echo $foo[0]
echo $foo[ 0 ]
echo $foo[ 00 ]
#CHECKERR: {{.*}}checks/zero_based_array.fish (line {{\d+}}): array indices start at 1, not 0.
#CHECKERR: echo $foo[0]
#CHECKERR: ^
#CHECKERR: {{.*}}checks/zero_based_array.fish (line {{\d+}}): array indices start at 1, not 0.
#CHECKERR: echo $foo[ 0 ]
#CHECKERR: ^
#CHECKERR: {{.*}}checks/zero_based_array.fish (line {{\d+}}): array indices start at 1, not 0.
#CHECKERR: echo $foo[ 00 ]
#CHECKERR: ^

# and make sure these didn't break
set -l foo one two three
echo $foo[001]
#CHECK: one
