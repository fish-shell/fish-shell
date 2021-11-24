#RUN: %fish %s
exec cat <nosuchfile
#CHECKERR: warning: An error occurred while redirecting file 'nosuchfile'
#CHECKERR: warning: Path 'nosuchfile' does not exist
echo "failed: $status"
#CHECK: failed: 1
not exec cat <nosuchfile
#CHECKERR: warning: An error occurred while redirecting file 'nosuchfile'
#CHECKERR: warning: Path 'nosuchfile' does not exist
echo "neg failed: $status"
#CHECK: neg failed: 0

# This needs to be last, because it actually runs exec.
exec cat </dev/null
echo "not reached"

