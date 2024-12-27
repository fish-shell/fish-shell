# RUN: fish=%fish %fish %s
# Test that fish doesn't crash if cwd is unreadable at the start (#6597)

set -l oldpwd $PWD
set -l tmpdir (mktemp -d)

# $fish might be a relative path (e.g. "../test/root/bin/fish")
set -l fish (builtin realpath $fish)
cd $tmpdir
chmod 000 .
# There's an error, but we don't really care about the specific one.
$fish -c 'echo Look Ma! No crashing!' 2>/dev/null
#CHECK: Look Ma! No crashing!

# Careful here, Solaris' rm tests if the directory is in $PWD, so we need to cd back
cd $oldpwd
rmdir $tmpdir
