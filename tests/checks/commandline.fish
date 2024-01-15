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

commandline --input 'echo $$' --current-command
# CHECK: echo

commandline --input 'foo=bar ' --current-command
# CHECK:

commandline --input 'boo; and foo=bar git' --current-command
# CHECK: git

commandline --input 'cat boo | foo=bar less' --current-command
# CHECK: less
