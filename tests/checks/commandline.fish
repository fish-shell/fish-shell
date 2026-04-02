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

$fish -c 'commandline foo'
# CHECKERR: commandline: Can not set commandline in non-interactive mode
# CHECKERR: Standard input (line 1):
# CHECKERR: commandline foo
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)

commandline --tokens-expanded --tokens-raw
# CHECKERR: commandline: invalid option combination, --tokens options are mutually exclusive
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --tokens-expanded --tokens-raw
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)

commandline --tokens-expanded --tokenize
# CHECKERR: commandline: invalid option combination, --tokens options are mutually exclusive
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --tokens-expanded --tokenize
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)

commandline --function --current-buffer
# CHECKERR: commandline: invalid option combination
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --function --current-buffer
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)

commandline --function foo
# CHECKERR: commandline: Unknown input function 'foo'
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --function foo
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)

commandline --selection-start foo
# CHECKERR: commandline: too many arguments
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --selection-start foo
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)
commandline --selection-end foo
# CHECKERR: commandline: too many arguments
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --selection-end foo
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)

commandline --line 1 2
# CHECKERR: commandline: too many arguments
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --line 1 2
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)

commandline --cut-at-cursor --cursor
# CHECKERR: commandline: invalid option combination
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --cut-at-cursor --cursor
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)

commandline --cut-at-cursor --tokens-expanded abc
# CHECKERR: commandline: invalid option combination, --cut-at-cursor and token options can not be used when setting the commandline
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --cut-at-cursor --tokens-expanded abc
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)

commandline --search-field --tokens-expanded
# CHECKERR: commandline: invalid option combination
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --search-field --tokens-expanded
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)

commandline --insert-smart 0 --search-field
# CHECKERR: commandline: --insert-smart --search-field: options cannot be used together
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --insert-smart 0 --search-field
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)

commandline --insert-smart 0 --current-token
# CHECKERR: commandline: --insert-smart --current-token: options cannot be used together
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --insert-smart 0 --current-token
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)

commandline --line abc
# CHECKERR: commandline: abc: invalid integer
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --line abc
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)
commandline --line 0
# CHECKERR: commandline: line/column index starts at 1
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --line 0
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)
commandline --line 1
# OK
commandline --line 2
# CHECKERR: commandline: there is no line 2
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --line 2
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)
commandline --line 3
# CHECKERR: commandline: there is no line 3
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --line 3
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)

commandline --column abc
# CHECKERR: commandline: abc: invalid integer
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --column abc
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)
commandline --column 0
# CHECKERR: commandline: line/column index starts at 1
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --column 0
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)
commandline --column 1
# OK
commandline --column 2
# CHECKERR: commandline: column 2 exceeds line length
# CHECKERR: {{.*}}/commandline.fish (line {{\d+}}):
# CHECKERR: commandline --column 2
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)

$fish -ic "commandline --cursor abc"
# CHECKERR: commandline: abc: invalid integer
# CHECKERR: Standard input (line 1):
# CHECKERR: commandline --cursor abc
# CHECKERR: ^
# CHECKERR: (Type 'help commandline' for related documentation)
