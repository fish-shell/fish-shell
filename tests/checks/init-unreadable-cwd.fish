#RUN: %ghoti -C 'set -g ghoti %ghoti' %s
# Test that ghoti doesn't crash if cwd is unreadable at the start (#6597)

set -l oldpwd $PWD
set -l tmpdir (mktemp -d)

# $ghoti might be a relative path (e.g. "../test/root/bin/ghoti")
set -l ghoti (builtin realpath $ghoti)
cd $tmpdir
chmod 000 .
# There's an error, but we don't really care about the specific one.
$ghoti -c 'echo Look Ma! No crashing!' 2>/dev/null
#CHECK: Look Ma! No crashing!

# Careful here, Solaris' rm tests if the directory is in $PWD, so we need to cd back
cd $oldpwd
rmdir $tmpdir
