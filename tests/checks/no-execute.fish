#RUN: fish=%fish %fish %s

# Test that fish -n doesn't check for command existence - function autoloading throws a wrench in that.
echo "type foo" | $fish -n
echo $status
#CHECK: 0

# Test that it doesn't time non-execution.
echo "time echo foo" | $fish -n
echo $status
#CHECK: 0

# Test that it doesn't check globs.
echo "echo /asfjidhfiusnlkxcnvklxcvlkmcxlv*" | $fish -n
echo $status
#CHECK: 0

# Test that it does print syntax errors.
echo "begin; echo oops" | $fish -n
#CHECKERR: fish: Missing end to balance this begin
#CHECKERR: begin; echo oops
#CHECKERR: ^~~~^
echo $status
#CHECK: 127

echo "begin" > broken
$fish -n $PWD/broken
#CHECKERR: /{{.*}}broken (line 1): Missing end to balance this begin
#CHECKERR: begin
#CHECKERR: ^~~~^
#CHECKERR: warning: Error while reading file /{{.*}}broken

rm broken

# Littlecheck assumes a status of 127 means the shebang was invalid.
exit 0
