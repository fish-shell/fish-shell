# SPDX-FileCopyrightText: © 2020 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish %s
echo $foo[0]
#CHECKERR: {{.*}}checks/zero_based_array.fish (line {{\d+}}): array indices start at 1, not 0.
#CHECKERR: echo $foo[0]
#CHECKERR: ^
echo $foo[ 0 ]
#CHECKERR: {{.*}}checks/zero_based_array.fish (line {{\d+}}): array indices start at 1, not 0.
#CHECKERR: echo $foo[ 0 ]
#CHECKERR: ^
echo $foo[ 00 ]
#CHECKERR: {{.*}}checks/zero_based_array.fish (line {{\d+}}): array indices start at 1, not 0.
#CHECKERR: echo $foo[ 00 ]
#CHECKERR: ^
echo $foo[+0]
#CHECKERR: {{.*}}checks/zero_based_array.fish (line {{\d+}}): array indices start at 1, not 0.
#CHECKERR: echo $foo[+0]
#CHECKERR: ^
echo $foo[-0]
#CHECKERR: {{.*}}checks/zero_based_array.fish (line {{\d+}}): array indices start at 1, not 0.
#CHECKERR: echo $foo[-0]
#CHECKERR: ^
echo $foo[0..1]
#CHECKERR: {{.*}}checks/zero_based_array.fish (line {{\d+}}): array indices start at 1, not 0.
#CHECKERR: echo $foo[0..1]
#CHECKERR:           ^
echo $foo[1..0]
#CHECKERR: {{.*}}checks/zero_based_array.fish (line {{\d+}}): array indices start at 1, not 0.
#CHECKERR: echo $foo[1..0]
#CHECKERR:              ^

# and make sure these didn't break
set -l foo one two three
echo $foo[001]
#CHECK: one
