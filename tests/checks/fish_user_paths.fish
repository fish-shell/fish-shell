# RUN: %fish %s
#
# This deals with $PATH manipulation. We need to be careful not to step on anything.

set -l tmpdir (mktemp -d)
mkdir $tmpdir/bin
mkdir $tmpdir/sbin
mkdir $tmpdir/etc
ln -s $tmpdir/bin $tmpdir/link

# We set fish_user_paths to an empty global to have a starting point
set -g fish_user_paths
set fish_user_paths $tmpdir/bin

# Confirm that it actually ends up in $PATH
contains -- (builtin realpath $tmpdir/bin) $PATH
and echo Have bin
# CHECK: Have bin

# Not adding duplicates
set PATH $PATH
set -l --path oldpath $PATH
set -a fish_user_paths $tmpdir/bin
test "$oldpath" = "$PATH"
or begin
    echo OH NO A DUPLICATE
    echo NEW: $PATH
    echo OLD: $oldpath
end


# Add a link to the same path.
set -a fish_user_paths $tmpdir/link
contains -- $tmpdir/link $PATH
and echo Have bin
# CHECK: Have bin
