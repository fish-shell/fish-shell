#RUN: fish=%fish %fish %s
# 
set -g PATH .

set x (string repeat 1000 x)
$x
#CHECKERR: {{.*}}: Command name exceeds maximum length: '{{x{1000}\b}}'
#CHECKERR: $x
#CHECKERR: ^^

$fish -c 'set x (string repeat 1000 x); $x'
#CHECKERR: fish: Command name exceeds maximum length: '{{x{1000}\b}}'
#CHECKERR: set x (string repeat 1000 x); $x
#CHECKERR:                               ^^
exit 0
