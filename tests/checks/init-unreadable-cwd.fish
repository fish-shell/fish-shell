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

# Verify fish reports the unreadable cwd on stderr instead of silently leaving cwd_fd unset.
{
    $fish -c 'true' 2>&1 1>/dev/null | string match -q '*Unable to open the current working directory*'
    or status build-info | grep -q '^Target.*-cygwin$'
}
and echo "cwd error reported"
#CHECK: cwd error reported

# Careful here, Solaris' rm tests if the directory is in $PWD, so we need to cd back
cd $oldpwd
rmdir $tmpdir
