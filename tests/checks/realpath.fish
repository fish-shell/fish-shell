# RUN: %fish %s
# $XDG_DATA_HOME can itself be a relative path. So force it to an absolute
# path so we can remove it from any resolved paths below. This is needed
# because the contents of the builtin realpath.out file can't include any $PWD
# data since $PWD isn't under our control.
set -l data_home_realpath (builtin realpath $XDG_DATA_HOME)

# A bogus absolute path is handled correctly and sets a failure status.
if not builtin realpath /this/better/be/an/invalid/path
    echo first invalid path handled okay
    # CHECK: first invalid path handled okay
    # CHECKERR: builtin realpath: /this/better/be/an/invalid/path: No such file or directory
end

# A non-existent file relative to $PWD succeeds.
set -l real_path (builtin realpath nonexistent-file)
if test "$real_path" = (realpath $PWD)"/nonexistent-file"
    echo nonexistent-file in PWD correctly converted
    # CHECK: nonexistent-file in PWD correctly converted
end

# The simplest absolute path should undergo no transformation.
builtin realpath /
# CHECK: /

# The second simplest absolute path should undergo no transformation.
builtin realpath /this-better-not-exist
# CHECK: /this-better-not-exist

# Check that a pathological case is handled correctly (i.e., there is only one
# leading slash).
builtin realpath /../../x
# CHECK: /x

# Another pathological corner case. GNU realpath first strips trailing slashes
# so that "/a//" is converted to "/a" before performing the real path
# conversion. So, despite appearances, it considers "a" to be the last
# component in that case.
builtin realpath /abc/
# CHECK: /abc
builtin realpath /def///
# CHECK: /def

# Verify `realpath .` when cwd is a deleted directory gives a no such file or dir error.
set -l tmpdir (mktemp -d)
pushd $tmpdir
# Solaris rmdir tries to protect against deleting $PWD.
# But that's what we want to test, so we weasel around it.
sh -c "cd ..; rmdir $tmpdir"
builtin realpath .
# CHECKERR: builtin realpath: .: No such file or directory
popd

# A single symlink to a directory is correctly resolved.
ln -fs fish $XDG_DATA_HOME/fish-symlink
set -l real_path (builtin realpath $XDG_DATA_HOME/fish-symlink)
set -l expected_real_path "$data_home_realpath/fish"
if test "$real_path" = "$expected_real_path"
    echo "fish-symlink handled correctly"
    # CHECK: fish-symlink handled correctly
else
    echo "fish-symlink not handled correctly: $real_path != $expected_real_path" >&2
end

# With "-s" the symlink is not resolved.
set -l real_path (builtin realpath -s $data_home_realpath/fish-symlink)
set -l expected_real_path "$data_home_realpath/fish-symlink"
if test "$real_path" = "$expected_real_path"
    echo "fish-symlink handled correctly"
    # CHECK: fish-symlink handled correctly
else
    echo "fish-symlink not handled correctly: $real_path != $expected_real_path" >&2
end

# But the $PWD is still resolved
set -l oldpwd $PWD
cd $XDG_DATA_HOME/fish-symlink
set -l real_path (builtin realpath -s $data_home_realpath/fish-symlink)
set -l expected_real_path "$data_home_realpath/fish-symlink"
if test "$real_path" = "$expected_real_path"
    echo "fish-symlink handled correctly"
    # CHECK: fish-symlink handled correctly
else
    echo "fish-symlink not handled correctly: $real_path != $expected_real_path" >&2
end
cd $oldpwd

set -l real_path (builtin realpath -s .)
set -l expected_real_path (pwd -P) # Physical working directory.
if test "$real_path" = "$expected_real_path"
    echo "relative path correctly handled"
    # CHECK: relative path correctly handled
else
    echo "relative path not handled correctly: $real_path != $expected_real_path" >&2
end

test (builtin realpath -s /usr/bin/../) = /usr
or echo builtin realpath -s does not resolve .. or resolves symlink wrong

# A nonexistent file relative to a valid symlink to a directory gets converted.
# This depends on the symlink created by the previous test.
set -l real_path (builtin realpath $XDG_DATA_HOME/fish-symlink/nonexistent-file-relative-to-a-symlink)
set -l expected_real_path "$data_home_realpath/fish/nonexistent-file-relative-to-a-symlink"
if test "$real_path" = "$expected_real_path"
    echo "fish-symlink/nonexistent-file-relative-to-a-symlink correctly converted"
    # CHECK: fish-symlink/nonexistent-file-relative-to-a-symlink correctly converted
else
    echo "failure nonexistent-file-relative-to-a-symlink: $real_path != $expected_real_path" >&2
end

# We remove leading slashes even with "-s".
# This is how GNU realpath -s behaves, and also e.g.
# how bash normalizes its $PWD.
builtin realpath -s ///bin
# CHECK: /bin

builtin realpath -s //bin
# CHECK: /bin

# A path with two symlinks, first to a directory, second to a file, is correctly resolved.
ln -fs fish $XDG_DATA_HOME/fish-symlink2
touch $XDG_DATA_HOME/fish/real_file
ln -fs real_file $XDG_DATA_HOME/fish/symlink_file
set -l real_path (builtin realpath $XDG_DATA_HOME/fish-symlink/symlink_file)
set -l expected_real_path "$data_home_realpath/fish/real_file"
if test "$real_path" = "$expected_real_path"
    echo "fish-symlink/symlink_file handled correctly"
    # CHECK: fish-symlink/symlink_file handled correctly
else
    echo "fish-symlink/symlink_file not handled correctly: $real_path != expected_real_path" >&2
end

builtin realpath / /
# CHECK: /
# CHECK: /

exit 0
