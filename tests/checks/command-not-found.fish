# RUN: %ghoti -C 'set -g ghoti %ghoti' %s
set -g PATH
$ghoti -c "nonexistent-command-1234 banana rama"
#CHECKERR: ghoti: Unknown command: nonexistent-command-1234
#CHECKERR: ghoti: 
#CHECKERR: nonexistent-command-1234 banana rama
#CHECKERR: ^~~~~~~~~~~~~~~~~~~~~~~^
$ghoti -C 'function ghoti_command_not_found; echo cmd-not-found; end' -ic "nonexistent-command-1234 1 2 3 4"
#CHECKERR: cmd-not-found
#CHECKERR: ghoti: 
#CHECKERR: nonexistent-command-1234 1 2 3 4
#CHECKERR: ^~~~~~~~~~~~~~~~~~~~~~~^
$ghoti -C 'function ghoti_command_not_found; echo command-not-found $argv; end' -c "nonexistent-command-abcd foo bar baz"
#CHECKERR: command-not-found nonexistent-command-abcd foo bar baz
#CHECKERR: ghoti: 
#CHECKERR: nonexistent-command-abcd foo bar baz
#CHECKERR: ^~~~~~~~~~~~~~~~~~~~~~~^

$ghoti -C 'functions --erase ghoti_command_not_found' -c 'nonexistent-command apple friday'
#CHECKERR: ghoti: Unknown command: nonexistent-command
#CHECKERR: nonexistent-command apple friday
#CHECKERR: ^~~~~~~~~~~~~~~~~~^

command -v nonexistent-command-1234
echo $status
#CHECK: 127


{ echo; echo }
# CHECKERR: {{.*}}: Unknown command: '{ echo; echo }'
# CHECKERR: {{.*}}: '{ ... }' is not supported for grouping commands. Please use 'begin; ...; end'
# CHECKERR: { echo; echo }
# CHECKERR: ^~~~~~~~~~~~~^

set -g PATH .
echo banana > foobar
foobar --banana
# CHECKERR: checks/command-not-found.ghoti (line {{\d+}}): Unknown command. './foobar' exists but is not an executable file.
# CHECKERR: foobar --banana
# CHECKERR: ^~~~~^


exit 0
