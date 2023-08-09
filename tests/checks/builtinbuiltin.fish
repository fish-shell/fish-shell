#RUN: %fish %s
# Tests for the "builtin" builtin/keyword.
builtin -q string; and echo String exists
#CHECK: String exists
builtin -q; and echo None exists
builtin -q string echo banana; and echo Some of these exist
#CHECK: Some of these exist
builtin -nq string
#CHECKERR: builtin: invalid option combination, --query and --names are mutually exclusive
echo $status
#CHECK: 2

builtin -- -q &| grep -q "builtin - run a builtin command\|fish: builtin: missing man page"
echo $status
#CHECK: 0

exit 0
