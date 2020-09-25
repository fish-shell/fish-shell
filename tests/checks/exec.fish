#RUN: %fish %s
exec cat <nosuchfile
#CHECKERR: warning: An error occurred while redirecting file 'nosuchfile'
#CHECKERR: open: No such file or directory
echo "failed: $status"
#CHECK: failed: 1
not exec cat <nosuchfile
#CHECKERR: warning: An error occurred while redirecting file 'nosuchfile'
#CHECKERR: open: No such file or directory
echo "neg failed: $status"
#CHECK: neg failed: 0

set -l f (mktemp)
echo "#!/bin/sh"\r\n"echo foo" > $f
chmod +x $f
$f
#CHECKERR: Failed to execute process '{{.*}}'. Reason:
#CHECKERR: The file uses windows line endings (\r\n). Run dos2unix or similar to fix it.

# This needs to be last, because it actually runs exec.
exec cat </dev/null
echo "not reached"

