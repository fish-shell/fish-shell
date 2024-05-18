#RUN: %fish %s

function outnerr
    command echo out $argv
    command echo err $argv 1>&2
end

outnerr 0 &| count
#CHECK: 2


outnerr appendfd 2>>&1
#CHECK: out appendfd
#CHECK: err appendfd

set -l tmpdir (mktemp -d)
outnerr overwrite &>$tmpdir/file.txt
cat $tmpdir/file.txt
#CHECK: out overwrite
#CHECK: err overwrite

outnerr append &>>$tmpdir/file.txt
cat $tmpdir/file.txt
#CHECK: out overwrite
#CHECK: err overwrite
#CHECK: out append
#CHECK: err append

echo noclobber &>>?$tmpdir/file.txt
#CHECKERR: {{.*}} The file {{.*}} already exists

eval "echo foo |& false"
#CHECKERR: {{.*}} |& is not valid. In fish, use &| to pipe both stdout and stderr.
#CHECKERR: echo foo |& false
#CHECKERR:          ^^

# Ensure that redirection empty data still creates the file.
rm -f $tmpdir/file.txt
test -f $tmpdir/file.txt && echo "File exists" || echo "File does not exist"
#CHECK: File does not exist

echo -n >$tmpdir/file.txt
test -f $tmpdir/file.txt && echo "File exists" || echo "File does not exist"
#CHECK: File exists

rm $tmpdir/file.txt
echo -n 2>$tmpdir/file.txt
test -f $tmpdir/file.txt && echo "File exists" || echo "File does not exist"
#CHECK: File exists

rm $tmpdir/file.txt

function redir_to_argv1
    if set -q argv[1]
        redir_to_argv1 >$argv[1]
    end
    echo foo
end

redir_to_argv1 $tmpdir/bar
# CHECK: foo
cat $tmpdir/bar
# CHECK: foo
rm $tmpdir/bar

# Verify that we can turn stderr into stdout and then pipe it
# Note that the order here has historically been unspecified - 'errput' could conceivably appear before 'output'.
begin
    echo output
    echo errput 1>&2
end 2>&1 | sort | tee $tmpdir/tee_test.txt
cat $tmpdir/tee_test.txt

rm $tmpdir/tee_test.txt
rmdir $tmpdir

#CHECK: errput
#CHECK: output
#CHECK: errput
#CHECK: output

# Test that trailing ^ doesn't trigger redirection, see #1873
echo caret_no_redirect 12345^
#CHECK: caret_no_redirect 12345^

# Verify that we can pipe something other than stdout
# The first line should be printed, since we output to stdout but pipe stderr to /dev/null
# The second line should not be printed, since we output to stderr and pipe it to /dev/null
begin
    echo is_stdout
end 2>| cat >/dev/null
begin
    echo is_stderr 1>&2
end 2>| cat >/dev/null
#CHECK: is_stdout

# Verify builtin behavior with closed stdin.
# count silently ignores closed stdin, others may print an error.
true <&-
echo $status
#CHECK: 0
test -t 0 <&-
echo $status
#CHECK: 1
read abc <&-
#CHECKERR: read: stdin is closed

# This one should output nothing.
echo derp >&-

# Redirection to 0, 1, 2 should always work. We don't test 0 since writing to stdin is weird and unpredictable.
echo hooray1 >&1
echo hooray2 >&2
#CHECK: hooray1
#CHECKERR: hooray2

# "Verify that pipes don't conflict with fd redirections"
# This code is very similar to eval. We go over a bunch of fads
# to make it likely that we will nominally conflict with a pipe
# fish is supposed to detect this case and dup the pipe to something else
echo "/bin/echo pipe 3 <&3 3<&-" | source 3<&0
echo "/bin/echo pipe 4 <&4 4<&-" | source 4<&0
echo "/bin/echo pipe 5 <&5 5<&-" | source 5<&0
echo "/bin/echo pipe 6 <&6 6<&-" | source 6<&0
echo "/bin/echo pipe 7 <&7 7<&-" | source 7<&0
echo "/bin/echo pipe 8 <&8 8<&-" | source 8<&0
echo "/bin/echo pipe 9 <&9 9<&-" | source 9<&0
echo "/bin/echo pipe 10 <&10 10<&-" | source 10<&0
echo "/bin/echo pipe 11 <&11 11<&-" | source 11<&0
echo "/bin/echo pipe 12 <&12 12<&-" | source 12<&0
#CHECK: pipe 3
#CHECK: pipe 4
#CHECK: pipe 5
#CHECK: pipe 6
#CHECK: pipe 7
#CHECK: pipe 8
#CHECK: pipe 9
#CHECK: pipe 10
#CHECK: pipe 11
#CHECK: pipe 12

echo foo >/bin/echo/file
#CHECKERR: warning: An error occurred while redirecting file '/bin/echo/file'
#CHECKERR: warning: Path '/bin/echo' is not a directory

echo foo <?nonexistent
#CHECK: foo
echo $status
#CHECK: 0

read -l foo <?nonexistent
echo $status
#CHECK: 1
set -S foo
#CHECK: $foo: set in local scope, unexported, with 0 elements

set -l fish (status fish-path)
$fish --no-config -c 'true <&?fail'
#CHECKERR: fish: Requested redirection to '?fail', which is not a valid file descriptor
#CHECKERR: true <&?fail
#CHECKERR: ^~~~~~^

$fish --no-config -c 'true <?&fail'
#CHECKERR: fish: Expected a string, but found a '&'
#CHECKERR: true <?&fail
#CHECKERR: ^
