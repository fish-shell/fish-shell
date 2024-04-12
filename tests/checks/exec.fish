#RUN: %fish -C 'set -l fish %fish' %s

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

# See that variable overrides are applied to exec'd processes
$fish --no-config -c 'foo=bar exec env' | grep foo=bar
# CHECK: foo=bar

# This needs to be last, because it actually runs exec.
exec cat </dev/null
echo "not reached"
