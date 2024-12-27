#RUN: %fish %s

ulimit --core-size
#CHECK: {{unlimited|\d+}}
ulimit --core-size 0
ulimit --core-size
#CHECK: 0

ulimit 4352353252352352334
#CHECKERR: ulimit: Invalid limit '4352353252352352334'
#CHECKERR: 
#CHECKERR: {{.*}}checks/ulimit.fish (line {{\d+}}):
#CHECKERR: ulimit 4352353252352352334
#CHECKERR: ^
#CHECKERR: (Type 'help ulimit' for related documentation)

