# RUN: %fish -C 'set -g fish %fish' %s
set -g PATH
$fish -c "nonexistent-command-1234 banana rama"
#CHECKERR: fish: Unknown command: nonexistent-command-1234
#CHECKERR: fish: 
#CHECKERR: nonexistent-command-1234 banana rama
#CHECKERR: ^
$fish -C 'function fish_command_not_found; echo cmd-not-found; end' -ic "nonexistent-command-1234 1 2 3 4"
#CHECKERR: cmd-not-found
#CHECKERR: fish: 
#CHECKERR: nonexistent-command-1234 1 2 3 4
#CHECKERR: ^
$fish -C 'function fish_command_not_found; echo command-not-found $argv; end' -c "nonexistent-command-abcd foo bar baz"
#CHECKERR: command-not-found nonexistent-command-abcd foo bar baz
#CHECKERR: fish: 
#CHECKERR: nonexistent-command-abcd foo bar baz
#CHECKERR: ^

$fish -C 'functions --erase fish_command_not_found' -c 'nonexistent-command apple friday'
#CHECKERR: fish: Unknown command: nonexistent-command
#CHECKERR: nonexistent-command apple friday
#CHECKERR: ^

command -v nonexistent-command-1234
echo $status
#CHECK: 127


{ echo; echo }
# CHECKERR: {{.*}}: Unknown command: '{ echo; echo }'
# CHECKERR: {{.*}}: '{ ... }' is not supported for grouping commands. Please use 'begin; ...; end'
# CHECKERR: { echo; echo }
# CHECKERR: ^

set -g PATH .
echo banana > foobar
foobar --banana
# CHECKERR: fish: Unknown command: foobar
# CHECKERR: fish: ./foobar exists but isn't executable
# CHECKERR: checks/command-not-found.fish (line 37):
# CHECKERR: foobar --banana
# CHECKERR: ^


exit 0
