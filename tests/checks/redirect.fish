#RUN: %fish %s

function outnerr
    command echo out $argv
    command echo err $argv 1>&2
end

outnerr 0 &| count
#CHECK: 2

set -l tmpdir (mktemp -d)
outnerr overwrite &> $tmpdir/file.txt
cat $tmpdir/file.txt
#CHECK: out overwrite
#CHECK: err overwrite

outnerr append &>> $tmpdir/file.txt
cat $tmpdir/file.txt
#CHECK: out overwrite
#CHECK: err overwrite
#CHECK: out append
#CHECK: err append

echo noclobber &>>? $tmpdir/file.txt
#CHECKERR: {{.*}} The file {{.*}} already exists

eval "echo foo |& false"
#CHECKERR: {{.*}} |& is not valid. In fish, use &| to pipe both stdout and stderr.
#CHECKERR: echo foo |& false
#CHECKERR:          ^

rm -Rf $tmpdir
