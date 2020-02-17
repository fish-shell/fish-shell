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
#CHECKERR:          ^

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

rm -Rf $tmpdir
