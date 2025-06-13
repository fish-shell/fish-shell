#RUN: %fish %s

set -g fish (status fish-path)

commandline --input "echo foo | bar" --is-valid
and echo Valid
# CHECK: Valid

commandline --input "echo foo | " --is-valid
or echo Invalid $status
# CHECK: Invalid 2

# TODO: This seems a bit awkward?
# The empty commandline is an error, not incomplete?
commandline --input '' --is-valid
or echo Invalid $status
# CHECK: Invalid 1

commandline --input 'echo $$' --is-valid
or echo Invalid $status
# CHECK: Invalid 1

commandline --help &>/dev/null
echo Help $status
# CHECK: Help 0

commandline -pC 0 --input "test | test"
echo $status
# CHECK: 0

commandline --insert-smart '$ echo 123' --current-token
# CHECKERR: commandline: --insert-smart --current-token: options cannot be used together
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --insert-smart '$ echo 123' --current-token
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)

commandline --input "echo {arg1,arg2} <in >out" --tokens-expanded
# CHECK: echo
# CHECK: arg1
# CHECK: arg2

commandline --input "echo <" --tokens-expanded
# CHECK: echo
commandline --input "echo >" --tokens-expanded
# CHECK: echo
commandline --input "echo > > arg" --tokens-expanded
# CHECK: echo
commandline --input "echo > {a,b}" --tokens-expanded
# CHECK: echo

commandline --input "echo {arg1,arg2} <in >out" --tokens-raw
# CHECK: echo
# CHECK: {arg1,arg2}

$fish -ic '
    commandline hello
    commandline
    commandline -i world
    commandline
    commandline --cursor 5
    commandline -i " "
    commandline
'
# CHECK: hello
# CHECK: helloworld
# CHECK: hello world
