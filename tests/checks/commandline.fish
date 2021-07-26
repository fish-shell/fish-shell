#RUN: %fish %s

commandline --input "echo foo | bar" --is-valid
and echo Valid
# CHECK: Valid

commandline --input "echo foo | " --is-valid
or echo Invalid
# CHECK: Invalid

commandline --input '' --is-valid
or echo Invalid
# CHECK: Invalid
