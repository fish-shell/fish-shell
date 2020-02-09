#RUN: %fish %s
# Tests for the "builtin" builtin/keyword.
builtin -q string; and echo String exists
#CHECK: String exists
builtin -q; and echo None exists
builtin -q string echo banana; and echo Some of these exist
#CHECK: Some of these exist
builtin -nq string
#CHECKERR: builtin: Invalid combination of options,
#CHECKERR: --query and --names are mutually exclusive
echo $status
#CHECK: 2
exit 0
