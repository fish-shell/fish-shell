#RUN: %fish %s

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
